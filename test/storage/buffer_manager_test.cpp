#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "common/constants.h"
#include "common/system_config.h"
#include "common/types/types.h"
#include "graph_test/private_graph_test.h"
#include "gtest/gtest.h"
#include "spdlog/spdlog.h"
#include "storage/buffer_manager/buffer_manager.h"
#include "storage/buffer_manager/memory_manager.h"
#include "storage/buffer_manager/spiller.h"
#include "storage/buffer_manager/vm_region.h"
#include "storage/enums/residency_state.h"
#include "storage/shadow_file.h"
#include "storage/shadow_utils.h"
#include "storage/storage_manager.h"
#include "storage/table/chunked_node_group.h"
#include "storage/table/column_chunk.h"
#include "storage/table/csr_node_group.h"
#include "test_runner/test_parser.h"

using namespace riag::common;
using namespace riag::storage;

namespace riag {
namespace testing {

class BufferManagerTest : public DBTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
    void reserveAll() {
        auto* bm = getBufferManager(*database);
        // Can't use UINT64_MAX since it will overflow the usedMemory
        ASSERT_FALSE(bm->reserve(UINT64_MAX / 2));
    }
    bool reserve(uint64_t size) { return getBufferManager(*database)->reserve(size); }
    void freeUsedMemory(uint64_t size) { getBufferManager(*database)->freeUsedMemory(size); }
    VMRegion& getVMRegion(PageSizeClass pageSizeClass) {
        return *getBufferManager(*database)->vmRegions[pageSizeClass];
    }
};

TEST_F(BufferManagerTest, TestBMUsageForIdenticalQueries) {
    auto bm = getBufferManager(*database);
    auto initialMemoryUsage = bm->getUsedMemory();
    auto numRuns = 10u;
    auto query = "MATCH (p:person) WHERE p.ID = 0 RETURN p.fName";
    for (auto i = 0u; i < numRuns; ++i) {
        auto result = conn->query(query);
        ASSERT_TRUE(result->isSuccess()) << result->toString();
    }
    auto memoryUsed = bm->getUsedMemory();
    for (auto i = 0u; i < numRuns; ++i) {
        auto result = conn->query(query);
        ASSERT_TRUE(result->isSuccess()) << result->toString();
    }
    ASSERT_EQ(memoryUsed, bm->getUsedMemory())
        << "Memory usage after two identical queries should be identical";
    spdlog::info("Memory used initially: {}", initialMemoryUsage);
    spdlog::info("Memory used after transactions: {}", memoryUsed);
}

class EmptyBufferManagerTest : public DBTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/empty-db/");
    }
};

TEST_F(EmptyBufferManagerTest, TestSpillToDiskMemoryUsage) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    auto bm = getBufferManager(*database);
    auto mm = getMemoryManager(*database);
    auto initialUsedMemory = bm->getUsedMemory();
    {
        auto buffer = mm->allocateBuffer(false, 1024);
        ASSERT_EQ(initialUsedMemory + 1024, bm->getUsedMemory());
        ASSERT_NE(buffer->getBuffer().data(), nullptr);
    }
    ASSERT_EQ(initialUsedMemory, bm->getUsedMemory());

    {
        std::vector<std::unique_ptr<ColumnChunkData>> chunks;
        chunks.push_back(std::make_unique<ColumnChunkData>(*mm, LogicalType(LogicalTypeID::INT64),
            1024, false, ResidencyState::IN_MEMORY, false));
        auto chunkedNodeGroup = InMemChunkedNodeGroup(std::move(chunks), 0);
        chunkedNodeGroup.setUnused(*mm);
        auto memoryWithChunks = bm->getUsedMemory();
        SpillResult memorySpilled{};
        bm->getSpillerOrSkip([&](auto& spiller) {
            spiller.addUnusedChunk(&chunkedNodeGroup);
            // Claim memory from unused chunks
            memorySpilled = spiller.claimNextGroup();
        });
        ASSERT_NE(memorySpilled.memoryFreed + memorySpilled.memoryNowEvictable, 0);
        // The chunks should be entirely on disk
        ASSERT_EQ(initialUsedMemory, bm->getUsedMemory() - memorySpilled.memoryFreed);
        chunkedNodeGroup.loadFromDisk(*mm);
        // The chunks should be back in memory, but Spiller::claimNextGroup does not update
        // the amount of used memory itself, so we end up with the spilled memory recorded twice
        ASSERT_EQ(memoryWithChunks, bm->getUsedMemory() - memorySpilled.memoryFreed);
    }

    {
        // The memory manager uses the buffer manager for allocations of size TEMP_PAGE_SIZE
        std::vector<std::unique_ptr<ColumnChunkData>> chunks;
        chunks.push_back(std::make_unique<ColumnChunkData>(*mm, LogicalType(LogicalTypeID::INT64),
            TEMP_PAGE_SIZE / sizeof(int64_t), false, ResidencyState::IN_MEMORY, false));
        auto chunkedNodeGroup = InMemChunkedNodeGroup(std::move(chunks), 0);
        chunkedNodeGroup.setUnused(*mm);
        SpillResult memorySpilled{};
        bm->getSpillerOrSkip([&](auto& spiller) {
            spiller.addUnusedChunk(&chunkedNodeGroup);
            // Claim memory from unused chunks
            memorySpilled = spiller.claimNextGroup();
        });
        ASSERT_EQ(memorySpilled.memoryFreed, 0);
        ASSERT_EQ(memorySpilled.memoryNowEvictable, TEMP_PAGE_SIZE);
    }
}

// Simulates the case where we try to evict a page during an optimistic read
TEST_F(BufferManagerTest, TestBMEvictionSlowRead) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    auto* fh = StorageManager::Get(*getClientContext(*conn))->getDataFH();

    auto* page = fh->pinPage(0, PageReadPolicy::READ_PAGE);
    *page = 112;
    fh->getPageState(0)->setDirty();
    fh->unpinPage(0);
    bool firstTry = true;
    // The mmap version will fail the first time, but will be run again since the page state was
    // changed during the read
#if BM_MALLOC
    fh->optimisticReadPage(0, [&](auto* frame) {
        ASSERT_TRUE(firstTry);
        firstTry = false;
        ASSERT_EQ(*frame, 112);
        // Should evict all evictable candidates in the eviction queue before failing
        // Should *not* evict the page we're currently reading
        reserveAll();
        // Probably will still succeed, but with ASAN (and BM_MALLOC) it should catch a read after
        // free if this frame was evicted
        ASSERT_EQ(*frame, 112);
    });
#else
    fh->optimisticReadPage(0, [&](auto* frame) {
        if (firstTry) {
            firstTry = false;
            ASSERT_EQ(*frame, 112);
            // Should evict all evictable candidates in the eviction queue before failing
            // Should evict the page we're currently reading
            reserveAll();
            // Frame was evicted and should now be zeroed
            // (not on macos, which does this lazily)
#ifndef __APPLE__
            ASSERT_EQ(*frame, 0);
#endif
        } else {
            // Second try it should be correct again
            ASSERT_EQ(*frame, 112);
        }
    });
#endif
}

#if !BM_MALLOC && !defined(__SINGLE_THREADED__)
// One VMRegion is shared by every file of a database, so a file growing (which adds a frame group)
// must not disturb concurrent claims and releases of frames in groups that already exist.
TEST_F(BufferManagerTest, ClaimAndReleaseFrameWhileRegionGrows) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    auto& region = getVMRegion(REGULAR_PAGE);
    const frame_idx_t frameIdx = region.addNewFrameGroup()
                                 << StorageConstants::PAGE_GROUP_SIZE_LOG2;
    constexpr uint64_t numFrameGroupsToAdd = 64;
    std::atomic<bool> doneGrowing{false};
    std::thread grower([&]() {
        for (auto i = 0u; i < numFrameGroupsToAdd; ++i) {
            region.addNewFrameGroup();
        }
        doneGrowing.store(true);
    });
    do {
        region.claimFrame(frameIdx);
        region.releaseFrame(frameIdx);
    } while (!doneGrowing.load());
    grower.join();
    // The frame must be back to non-resident: claiming it charges a whole discard granule again.
    ASSERT_NE(region.claimFrame(frameIdx), 0u);
    ASSERT_NE(region.releaseFrame(frameIdx), 0u);
}
#endif

// A rel scan state carries an in-memory CSR header sized for a whole node group, one per worker
// per rel scan. Its two UINT64 columns are plain data buffers: no null chunk, and the buffer pool
// is charged exactly their size.
TEST_F(EmptyBufferManagerTest, CSRScanStateHeaderIsTwoPlainUInt64Buffers) {
    // Smaller node groups can make a column exactly TEMP_PAGE_SIZE, which is served from paged
    // memory and accounted differently.
    if (StorageConfig::NODE_GROUP_SIZE_LOG2 != TestParser::STANDARD_NODE_GROUP_SIZE_LOG_2) {
        GTEST_SKIP();
    }
    auto* bm = getBufferManager(*database);
    auto* mm = getMemoryManager(*database);
    const auto initialUsedMemory = bm->getUsedMemory();
    constexpr auto headerBytes = 2 * StorageConfig::NODE_GROUP_SIZE * sizeof(uint64_t);
    {
        CSRNodeGroupScanState scanState(*mm);
        const auto& header = *scanState.header;
        ASSERT_EQ(header.offset->getCapacity(), StorageConfig::NODE_GROUP_SIZE);
        ASSERT_EQ(header.length->getCapacity(), StorageConfig::NODE_GROUP_SIZE);
        ASSERT_FALSE(header.offset->hasNullData());
        ASSERT_FALSE(header.length->hasNullData());
        ASSERT_EQ(header.offset->getNumValues(), 0u);
        ASSERT_EQ(header.length->getNumValues(), 0u);
        ASSERT_EQ(bm->getUsedMemory(), initialUsedMemory + headerBytes);
    }
    // Both buffers are REUSABLE_BUFFER_SIZE; those kept for reuse stay counted.
    const auto numKept = std::min<uint64_t>(2, mm->getMaxReusableBuffers());
    ASSERT_EQ(mm->getNumReusableBuffers(), numKept);
    const auto usedMemoryWithKeptBuffers =
        initialUsedMemory + numKept * MemoryManager::REUSABLE_BUFFER_SIZE;
    ASSERT_EQ(bm->getUsedMemory(), usedMemoryWithKeptBuffers);
    {
        CSRNodeGroupScanState lookupState(*mm, true /*randomLookup*/);
        ASSERT_EQ(lookupState.header->offset->getCapacity(), 1u);
        ASSERT_FALSE(lookupState.header->offset->hasNullData());
        ASSERT_EQ(bm->getUsedMemory(), usedMemoryWithKeptBuffers + 2 * sizeof(uint64_t));
    }
}

// Node-group-sized buffers are the only malloc'd size every rel scan allocates and frees; a freed
// one is kept for the next allocation of that size and stays counted as used memory.
TEST_F(EmptyBufferManagerTest, FreedNodeGroupSizedBufferIsReused) {
    auto* bm = getBufferManager(*database);
    auto* mm = getMemoryManager(*database);
    if (MemoryManager::REUSABLE_BUFFER_SIZE == TEMP_PAGE_SIZE || mm->getMaxReusableBuffers() == 0) {
        GTEST_SKIP();
    }
    constexpr auto size = MemoryManager::REUSABLE_BUFFER_SIZE;
    ASSERT_EQ(mm->getNumReusableBuffers(), 0u);
    const auto initialUsedMemory = bm->getUsedMemory();
    uint8_t* data = nullptr;
    {
        auto buffer = mm->allocateBuffer(false, size);
        data = buffer->getData();
        memset(data, 0xff, size);
    }
    ASSERT_EQ(mm->getNumReusableBuffers(), 1u);
    ASSERT_EQ(bm->getUsedMemory(), initialUsedMemory + size);
    {
        auto buffer = mm->allocateBuffer(false, size);
        ASSERT_EQ(buffer->getData(), data);
        ASSERT_EQ(mm->getNumReusableBuffers(), 0u);
        ASSERT_EQ(bm->getUsedMemory(), initialUsedMemory + size);
    }
    {
        auto buffer = mm->allocateBuffer(true /*initializeToZero*/, size);
        ASSERT_EQ(buffer->getData(), data);
        ASSERT_TRUE(std::all_of(data, data + size, [](uint8_t byte) { return byte == 0; }));
    }
    { auto buffer = mm->allocateBuffer(false, size / 2); }
    ASSERT_EQ(mm->getNumReusableBuffers(), 1u);
    ASSERT_EQ(bm->getUsedMemory(), initialUsedMemory + size);
}

TEST_F(EmptyBufferManagerTest, ReusableBuffersAreCapped) {
    // A larger pool than the test default, so that several buffers are kept.
    systemConfig->bufferPoolSize = 1ull << 30;
    conn.reset();
    database.reset();
    createDBAndConn();
    auto* bm = getBufferManager(*database);
    auto* mm = getMemoryManager(*database);
    if (MemoryManager::REUSABLE_BUFFER_SIZE == TEMP_PAGE_SIZE || mm->getMaxReusableBuffers() < 2) {
        GTEST_SKIP();
    }
    constexpr auto size = MemoryManager::REUSABLE_BUFFER_SIZE;
    const auto maxReusableBuffers = mm->getMaxReusableBuffers();
    const auto initialUsedMemory = bm->getUsedMemory();
    {
        std::vector<std::unique_ptr<MemoryBuffer>> buffers;
        for (auto i = 0u; i <= maxReusableBuffers; i++) {
            buffers.push_back(mm->allocateBuffer(false, size));
        }
    }
    ASSERT_EQ(mm->getNumReusableBuffers(), maxReusableBuffers);
    ASSERT_EQ(bm->getUsedMemory(), initialUsedMemory + maxReusableBuffers * size);
}

// A buffer kept for reuse must not make an allocation fail that would succeed without it, whether
// the allocation is malloc'd or a buffer manager page.
TEST_F(BufferManagerTest, ReusableBuffersAreReleasedWhenMemoryRunsOut) {
    auto* mm = getMemoryManager(*database);
    if (MemoryManager::REUSABLE_BUFFER_SIZE == TEMP_PAGE_SIZE || mm->getMaxReusableBuffers() == 0) {
        GTEST_SKIP();
    }
    for (const auto size : {uint64_t{1024}, TEMP_PAGE_SIZE}) {
        {
            auto buffer = mm->allocateBuffer(false, MemoryManager::REUSABLE_BUFFER_SIZE);
        }
        ASSERT_GE(mm->getNumReusableBuffers(), 1u);
        uint64_t reserved = 0;
        for (const auto step : {TEMP_PAGE_SIZE, uint64_t{1024}}) {
            while (reserve(step)) {
                reserved += step;
            }
        }
        {
            auto buffer = mm->allocateBuffer(false, size);
            ASSERT_NE(buffer->getData(), nullptr);
            ASSERT_EQ(mm->getNumReusableBuffers(), 0u);
        }
        freeUsedMemory(reserved);
    }
}

// Spilling frees a buffer to report the memory to the spiller, so it must not keep it.
TEST_F(EmptyBufferManagerTest, SpilledBufferIsNotKeptForReuse) {
    if (inMemMode ||
        StorageConfig::NODE_GROUP_SIZE_LOG2 != TestParser::STANDARD_NODE_GROUP_SIZE_LOG_2) {
        GTEST_SKIP();
    }
    auto* bm = getBufferManager(*database);
    auto* mm = getMemoryManager(*database);
    std::vector<std::unique_ptr<ColumnChunkData>> chunks;
    chunks.push_back(std::make_unique<ColumnChunkData>(*mm, LogicalType(LogicalTypeID::INT64),
        StorageConfig::NODE_GROUP_SIZE, false, ResidencyState::IN_MEMORY, false));
    auto chunkedNodeGroup = InMemChunkedNodeGroup(std::move(chunks), 0);
    chunkedNodeGroup.setUnused(*mm);
    SpillResult memorySpilled{};
    bm->getSpillerOrSkip([&](auto& spiller) {
        spiller.addUnusedChunk(&chunkedNodeGroup);
        memorySpilled = spiller.claimNextGroup();
    });
    ASSERT_EQ(memorySpilled.memoryFreed, MemoryManager::REUSABLE_BUFFER_SIZE);
    ASSERT_EQ(mm->getNumReusableBuffers(), 0u);
}

#if !defined(__SINGLE_THREADED__)
// The lock of a page (its LOCKED state) belongs to the thread that took it until that thread
// releases it. Functions that take the lock of a page they were not asked to pin have to wait for
// the owner instead of taking the lock over, otherwise two threads would be using the page and the
// first one to finish releases a lock the other still relies on.
class PageLatchTest : public EmptyBufferManagerTest {
public:
    static constexpr uint64_t NUM_HOLDS = 100000;
    static constexpr uint64_t CHECKS_PER_HOLD = 50;

    FileHandle* createFileWithOnePage(uint8_t fill) {
        auto* bm = getBufferManager(*database);
        auto* context = getClientContext(*conn);
        fh = bm->getFileHandle(databasePath + ".pagelatch",
            FileHandle::O_PERSISTENT_FILE_CREATE_NOT_EXISTS, getFileSystem(*database), context);
        pageIdx = fh->addNewPage();
        std::vector<uint8_t> content(RIAG_PAGE_SIZE, fill);
        fh->writePageToFile(content.data(), pageIdx);
        return fh;
    }

    // Pins the page NUM_HOLDS times. While a pin is held the page has to stay LOCKED, and the
    // frame must keep what the owner wrote into it. Anything else means that another thread took
    // the lock, and maybe the frame, away from the owner.
    void holdPageRepeatedly(std::atomic<bool>& violated) {
        for (auto i = 0u; i < NUM_HOLDS && !violated.load(); i++) {
            auto* frame = fh->pinPage(pageIdx, PageReadPolicy::READ_PAGE);
            const auto stamp = static_cast<uint8_t>(i);
            frame[0] = stamp;
            for (auto j = 0u; j < CHECKS_PER_HOLD; j++) {
                if (fh->getPageState(pageIdx)->getState() != PageState::LOCKED ||
                    frame[0] != stamp) {
                    violated.store(true);
                }
            }
            fh->unpinPage(pageIdx);
        }
    }

    FileHandle* fh = nullptr;
    page_idx_t pageIdx = INVALID_PAGE_IDX;
};

// updateFrameIfPageIsInFrame is used by a checkpoint while read transactions are running. A reader
// that loads an evicted page holds the lock of the page until it is done loading it.
TEST_F(PageLatchTest, UpdateFrameWaitsForPinnedPage) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    createFileWithOnePage(0x5A);
    auto* bm = getBufferManager(*database);
    std::vector<uint8_t> newPage(RIAG_PAGE_SIZE, 0xCD);
    std::atomic<bool> violated{false};
    std::atomic<bool> holderDone{false};
    std::thread holder([&]() {
        holdPageRepeatedly(violated);
        holderDone.store(true);
    });
    std::thread updater([&]() {
        while (!holderDone.load()) {
            bm->updateFrameIfPageIsInFrame(fh->getFileIndex(), newPage.data(), pageIdx);
        }
    });
    holder.join();
    updater.join();
    ASSERT_FALSE(violated.load()) << "The page was updated while another thread had it locked";
}

// removePageFromFrame does not expect the page to be locked, but eviction and loading threads
// lock pages that nobody asked them to leave alone.
TEST_F(PageLatchTest, RemovePageFromFrameWaitsForPinnedPage) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    createFileWithOnePage(0x5A);
    std::atomic<bool> violated{false};
    std::atomic<bool> holderDone{false};
    std::thread holder([&]() {
        holdPageRepeatedly(violated);
        holderDone.store(true);
    });
    std::thread remover([&]() {
        while (!holderDone.load()) {
            fh->removePageFromFrameIfNecessary(pageIdx);
        }
    });
    holder.join();
    remover.join();
    ASSERT_FALSE(violated.load()) << "The page was removed while another thread had it locked";
}

// The path of a reader that finds the page evicted: it pins the page, reads it and unpins it, all
// inside optimisticRead. The page has to be consistent whichever of the readers and updaters win.
TEST_F(PageLatchTest, OptimisticReadOfEvictedPageWithConcurrentUpdateAndRemove) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    createFileWithOnePage(0x5A);
    auto* bm = getBufferManager(*database);
    std::vector<uint8_t> newPage(RIAG_PAGE_SIZE, 0xCD);
    std::atomic<bool> badRead{false};
    std::atomic<bool> readerDone{false};
    std::thread reader([&]() {
        std::vector<uint8_t> copy(RIAG_PAGE_SIZE);
        for (auto i = 0u; i < NUM_HOLDS / 200 && !badRead.load(); i++) {
            fh->removePageFromFrameIfNecessary(pageIdx);
            fh->optimisticReadPage(pageIdx,
                [&](const uint8_t* frame) { memcpy(copy.data(), frame, RIAG_PAGE_SIZE); });
            // The page is either the one on disk or the one from the update.
            const auto expected = copy[0];
            if ((expected != 0x5A && expected != 0xCD) ||
                std::any_of(copy.begin(), copy.end(), [&](auto b) { return b != expected; })) {
                badRead.store(true);
            }
        }
        readerDone.store(true);
    });
    std::thread updater([&]() {
        while (!readerDone.load()) {
            bm->updateFrameIfPageIsInFrame(fh->getFileIndex(), newPage.data(), pageIdx);
            // Updates change the version of the page, which makes the reader read it again.
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });
    std::thread remover([&]() {
        // Every removal makes the next reader load the page again, which adds an entry to the
        // eviction queue, so removals are spaced out to stay far below its capacity.
        while (!readerDone.load()) {
            fh->removePageFromFrameIfNecessary(pageIdx);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    reader.join();
    updater.join();
    remover.join();
    ASSERT_FALSE(badRead.load());
}

// An optimistic read that passes its version re-check must have read one version of the page: the
// writer fills the whole page with one stamp per pin, so a validated copy holding two different
// bytes is a torn read. The re-check only catches this if the frame loads cannot be satisfied after
// it and the writer's stores cannot become visible before its lock (the fences in PageState). On
// x86 neither reordering happens in hardware, so there this test guards the protocol (the version
// re-check and its retry) rather than the fences; on a weakly ordered CPU (ARM) it can also catch a
// missing fence, though not reliably.
TEST_F(PageLatchTest, OptimisticReadNeverValidatesATornPage) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    createFileWithOnePage(0x5A);
    constexpr uint64_t numWrites = 20000;
    constexpr auto numReaders = 2u;
    std::atomic<bool> writerDone{false};
    std::atomic<uint64_t> numTornReads{0};
    std::atomic<uint64_t> numReads{0};
    std::thread writer([&]() {
        for (auto i = 0u; i < numWrites; i++) {
            auto* frame = fh->pinPage(pageIdx, PageReadPolicy::READ_PAGE);
            memset(frame, static_cast<uint8_t>(i), RIAG_PAGE_SIZE);
            fh->getPageState(pageIdx)->setDirty();
            fh->unpinPage(pageIdx);
        }
        writerDone.store(true);
    });
    std::vector<std::thread> readers;
    for (auto r = 0u; r < numReaders; r++) {
        readers.emplace_back([&]() {
            std::vector<uint8_t> copy(RIAG_PAGE_SIZE);
            while (!writerDone.load()) {
                fh->optimisticReadPage(pageIdx,
                    [&](const uint8_t* frame) { memcpy(copy.data(), frame, RIAG_PAGE_SIZE); });
                const auto stamp = copy[0];
                if (std::any_of(copy.begin(), copy.end(), [&](auto b) { return b != stamp; })) {
                    numTornReads.fetch_add(1);
                }
                numReads.fetch_add(1);
            }
        });
    }
    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }
    ASSERT_EQ(numTornReads.load(), 0u) << "of " << numReads.load() << " validated reads";
}

// A shadow page is pinned (locked) before the original page is read into it. When that read fails
// the caller never receives the pin, so it has to be released: the rollback of a failed checkpoint
// removes the shadow page from its frame and waits for the lock.
TEST_F(PageLatchTest, FailedShadowPageCreationDoesNotLeaveShadowPageLocked) {
    if (inMemMode) {
        GTEST_SKIP();
    }
    createFileWithOnePage(0x5A);
    // A page past the end of the file: reading it fails.
    const auto unreadablePage = fh->getNumPages();
    auto& shadowFile = StorageManager::Get(*getClientContext(*conn))->getShadowFile();
    const auto savepoint = shadowFile.createSavepoint();
    ASSERT_THROW(ShadowUtils::createShadowVersionIfNecessaryAndPinPage(unreadablePage,
                     false /* skipReadingOriginalPage */, *fh, shadowFile),
        Exception);
    const auto shadowPage = savepoint + 1;
    auto& shadowFH = shadowFile.getShadowingFH();
    const auto state = shadowFH.getPageState(shadowPage)->getState();
    if (state == PageState::LOCKED) {
        // Do not leave the page locked for the rollback and the teardown of the test.
        shadowFH.unpinPage(shadowPage);
    }
    ASSERT_NE(state, PageState::LOCKED);
    shadowFile.rollbackToSavepoint(savepoint);
}
#endif

} // namespace testing
} // namespace riag
