#pragma once

#include <atomic>

#include "common/assert.h"

// Alternative variant of the buffer manager which doesn't rely on MADV_DONTNEED (on Unix) for
// evicting pages (which is unavailable in Webassembly runtimes)
#if BM_MALLOC
#include <memory>
#endif

namespace riag {
namespace storage {

// Keeps the state information of a page in a file.
class PageState {
    static constexpr uint64_t DIRTY_MASK = 0x0080000000000000;
    static constexpr uint64_t STATE_MASK = 0xFF00000000000000;
    static constexpr uint64_t VERSION_MASK = 0x00FFFFFFFFFFFFFF;
    static constexpr uint64_t NUM_BITS_TO_SHIFT_FOR_STATE = 56;

public:
    static constexpr uint64_t UNLOCKED = 0;
    static constexpr uint64_t LOCKED = 1;
    static constexpr uint64_t MARKED = 2;
    static constexpr uint64_t EVICTED = 3;

    PageState() {
        stateAndVersion.store(EVICTED << NUM_BITS_TO_SHIFT_FOR_STATE);
#if BM_MALLOC
        readerCount.store(0);
#endif
    }

    uint64_t getState() const { return getState(stateAndVersion.load()); }
    static uint64_t getState(uint64_t stateAndVersion) {
        return (stateAndVersion & STATE_MASK) >> NUM_BITS_TO_SHIFT_FOR_STATE;
    }
    static uint64_t getVersion(uint64_t stateAndVersion) { return stateAndVersion & VERSION_MASK; }
    static uint64_t updateStateWithSameVersion(uint64_t oldStateAndVersion, uint64_t newState) {
        return ((oldStateAndVersion << 8) >> 8) | (newState << NUM_BITS_TO_SHIFT_FOR_STATE);
    }
    static uint64_t updateStateAndIncrementVersion(uint64_t oldStateAndVersion, uint64_t newState) {
        return (((oldStateAndVersion << 8) >> 8) + 1) | (newState << NUM_BITS_TO_SHIFT_FOR_STATE);
    }
    // Takes the lock of a page that is in a frame. If another thread holds the lock, waits for it
    // to be released: unlike a compare-exchange from the state that was just read, which also
    // succeeds from LOCKED (LOCKED -> LOCKED) and would leave two threads believing they own the
    // lock. Returns false without locking if the page is not in a frame (EVICTED).
    bool spinLockUnlessEvicted() {
        while (true) {
            auto oldStateAndVersion = stateAndVersion.load();
            switch (getState(oldStateAndVersion)) {
            case EVICTED: {
                return false;
            }
            case LOCKED: {
                continue;
            }
            default: {
                if (stateAndVersion.compare_exchange_strong(oldStateAndVersion,
                        updateStateWithSameVersion(oldStateAndVersion, LOCKED))) {
                    fenceAfterLock();
                    return true;
                }
            }
            }
        }
    }
    bool tryLock(uint64_t oldStateAndVersion) {
        if (stateAndVersion.compare_exchange_strong(oldStateAndVersion,
                updateStateWithSameVersion(oldStateAndVersion, LOCKED))) {
            fenceAfterLock();
            return true;
        }
        return false;
    }
    void unlock() {
        DASSERT(getState(stateAndVersion.load()) == LOCKED);
        stateAndVersion.store(updateStateAndIncrementVersion(stateAndVersion.load(), UNLOCKED));
    }
    void unlockUnchanged() {
        DASSERT(getState(stateAndVersion.load()) == LOCKED);
        stateAndVersion.store(updateStateWithSameVersion(stateAndVersion.load(), UNLOCKED));
    }
    // Change page state from Mark to Unlocked.
    bool tryClearMark(uint64_t oldStateAndVersion) {
        DASSERT(getState(oldStateAndVersion) == MARKED);
        return stateAndVersion.compare_exchange_strong(oldStateAndVersion,
            updateStateWithSameVersion(oldStateAndVersion, UNLOCKED));
    }
    bool tryMark(uint64_t oldStateAndVersion) {
        return stateAndVersion.compare_exchange_strong(oldStateAndVersion,
            updateStateWithSameVersion(oldStateAndVersion, MARKED));
    }

    void setDirty() {
        DASSERT(getState(stateAndVersion.load()) == LOCKED);
        stateAndVersion |= DIRTY_MASK;
    }
    void clearDirty() {
        DASSERT(getState(stateAndVersion.load()) == LOCKED);
        stateAndVersion &= ~DIRTY_MASK;
    }
    // Meant to be used when flushing in a single thread.
    // Should not be used if other threads are modifying the page state
    void clearDirtyWithoutLock() { stateAndVersion &= ~DIRTY_MASK; }
    bool isDirty() const { return stateAndVersion & DIRTY_MASK; }
    uint64_t getStateAndVersion() const { return stateAndVersion.load(); }
    // The version re-check of an optimistic read (BufferManager::optimisticRead). The frame was
    // read with plain loads, and the load of the version alone does not keep them before it: on
    // a weakly ordered CPU (ARM) they may be satisfied after it, so the re-check could still see
    // the old version while the bytes came from a writer that held the lock in between. The
    // acquire fence pairs with the release fence in fenceAfterLock(): if any byte the reader
    // returned was written under a lock taken after the reader's first load, the re-check sees
    // LOCKED or a later version and the read is retried. This is the reader half of a seqlock
    // in the C++ memory model (Boehm, "Can Seqlocks Get Along With Programming Language Memory
    // Models?", 2012). On x86 the fence emits no instruction; it only stops the compiler from
    // moving the frame loads past the re-check.
    uint64_t getStateAndVersionAfterOptimisticRead() const {
        std::atomic_thread_fence(std::memory_order_acquire);
        return stateAndVersion.load();
    }

    void resetToEvicted() {
#if BM_MALLOC
        // Clear the page while still holding LOCKED: storing EVICTED first would let
        // another thread pin (EVICTED->LOCKED) and allocate a new page concurrently
        // with this reset, racing on the unique_ptr and tripping TSan.
        page.reset();
#endif
        stateAndVersion.store(updateStateAndIncrementVersion(stateAndVersion.load(), EVICTED));
    }

#if BM_MALLOC
    uint8_t* getPage() const { return page.get(); }
    uint8_t* allocatePage(uint64_t pageSize) {
        page = std::make_unique<uint8_t[]>(pageSize);
        return page.get();
    }
    uint16_t getReaderCount() const { return readerCount; }
    void addReader() { readerCount++; }
    void removeReader() { readerCount--; }
#endif

private:
    // Called right after every successful transition to LOCKED. The lock holder then writes the
    // frame with plain stores (a pinned writer, cachePageIntoFrame, the zero-fill of a released
    // frame, updateFrameIfPageIsInFrame) while optimistic readers may be reading it. A successful
    // compare-exchange orders the accesses before it, not the stores after it: without this fence
    // a reader could see a store made under the lock and still read the version from before the
    // lock. Writer half of the seqlock, see getStateAndVersionAfterOptimisticRead(). On x86 it
    // emits no instruction.
    static void fenceAfterLock() { std::atomic_thread_fence(std::memory_order_release); }

    // The top byte (bits 56..63, STATE_MASK) is the state. The low 56 bits (bits 0..55,
    // VERSION_MASK) are the version, and the dirty bit is bit 55 (DIRTY_MASK) of those: it is
    // the highest bit of the version, not of the word. The state updates keep the low 56 bits
    // as they are (that is how the dirty bit survives a state change), so they carry the dirty
    // bit along, and an increment of the version that reaches bit 55 sets the dirty bit.
    std::atomic<uint64_t> stateAndVersion;
#if BM_MALLOC
    std::unique_ptr<uint8_t[]> page;
    std::atomic<uint16_t> readerCount;
#endif
};

} // namespace storage
} // namespace riag
