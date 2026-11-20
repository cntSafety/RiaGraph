#include <memory>
#include <string>

#include "catalog/catalog.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/catalog_entry/table_catalog_entry.h"
#include "graph_test/base_graph_test.h"
#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"
#include "storage/buffer_manager/buffer_manager.h"
#include "storage/buffer_manager/memory_manager.h"
#include "storage/storage_manager.h"
#include "test_helper/test_helper.h"
#include "transaction/transaction.h"
#include <format>

using namespace riag::common;
using namespace riag::main;

namespace riag {
namespace testing {

// A dropped table keeps its storage, and the buffer-pool memory behind it, until it is
// reclaimed: by a checkpoint on disk, and once no transaction can still see it in memory. A DROP
// writes almost no WAL, so the size-based auto-checkpoint never fired for it, and an in-memory
// database never checkpoints at all: every DROP TABLE / CREATE TABLE / reload cycle kept about
// 1 MB of the pool, and a 256 MiB pool was full after 224 cycles (RiaGraphDev#235).
class DropTableReclaimTest : public ::testing::TestWithParam<bool /* inMemory */> {
protected:
    void SetUp() override {
        if (!GetParam()) {
            databasePath = TestHelper::getTempDBPathStr(
                std::string("DropTableReclaimTest.") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
            removeParentDirectoryOfDBPath(databasePath);
        }
        SystemConfig config;
        config.bufferPoolSize = 32 * 1024 * 1024;
        config.maxNumThreads = 2;
        database = std::make_unique<Database>(databasePath, config);
        conn = std::make_unique<Connection>(database.get());
    }

    void TearDown() override {
        conn.reset();
        database.reset();
        if (!databasePath.empty()) {
            removeParentDirectoryOfDBPath(databasePath);
        }
    }

    void run(Connection& connection, const std::string& query) {
        auto result = connection.query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    }

    int64_t count(Connection& connection, const std::string& query) {
        auto result = connection.query(query);
        EXPECT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
        if (!result->isSuccess() || !result->hasNext()) {
            return -1;
        }
        return result->getNext()->getValue(0)->getValue<int64_t>();
    }

    // The runner's pattern: drop both tables, recreate them, reload with single statements.
    void dropCreateReload(int cycle) {
        run(*conn, "DROP TABLE IF EXISTS KN");
        run(*conn, "DROP TABLE IF EXISTS P");
        run(*conn, "CREATE NODE TABLE P(id INT64, name STRING, g INT64, PRIMARY KEY(id))");
        run(*conn, "CREATE REL TABLE KN(FROM P TO P, w INT64)");
        for (auto i = 0; i < 20; i++) {
            run(*conn, std::format("CREATE (:P {{id: {}, name: 'n{}', g: {}}})", i, cycle, i % 5));
        }
        for (auto i = 0; i < 20; i++) {
            run(*conn,
                std::format(
                    "MATCH (a:P {{id: {}}}), (b:P {{id: {}}}) CREATE (a)-[:KN {{w: {}}}]->(b)", i,
                    (i * 7 + cycle) % 20, i));
        }
    }

    uint64_t usedMemory() const {
        return database->getMemoryManager()->getBufferManager()->getUsedMemory();
    }

    common::table_id_t tableID(const std::string& name) const {
        return database->getCatalog()
            ->getTableCatalogEntry(&transaction::DUMMY_CHECKPOINT_TRANSACTION, name)
            ->getTableID();
    }

    // The storage of a rel group lives under the IDs of its FROM/TO pairs, not the group's.
    common::table_id_t relTableID(const std::string& name) const {
        return database->getCatalog()
            ->getTableCatalogEntry(&transaction::DUMMY_CHECKPOINT_TRANSACTION, name)
            ->constCast<catalog::RelGroupCatalogEntry>()
            .getRelEntryInfos()
            .front()
            .oid;
    }

    std::string databasePath;
    std::unique_ptr<Database> database;
    std::unique_ptr<Connection> conn;
};

TEST_P(DropTableReclaimTest, DropCreateCyclesDoNotFillTheBufferPool) {
    // Before the fix the 32 MiB pool was full at cycle 27; 60 cycles would need about 70 MB.
    constexpr auto numCycles = 60;
    uint64_t baseline = 0;
    for (auto cycle = 0; cycle < numCycles; cycle++) {
        dropCreateReload(cycle);
        if (HasFatalFailure()) {
            FAIL() << "cycle " << cycle << " failed, pool use " << usedMemory();
        }
        if (cycle == 4) {
            baseline = usedMemory();
        }
    }
    EXPECT_EQ(count(*conn, "MATCH (n:P) RETURN count(*)"), 20);
    EXPECT_EQ(count(*conn, "MATCH ()-[k:KN]->() RETURN count(*)"), 20);
    // Flat, not merely below the limit: one cycle's worth of drift at most.
    EXPECT_LE(usedMemory(), baseline + 1024 * 1024)
        << "baseline after 5 cycles " << baseline << ", after " << numCycles << " " << usedMemory();
}

// A read transaction that began before the DROP still sees the table through its snapshot, so
// its storage must outlive that transaction even though the DROP now reclaims promptly.
TEST_P(DropTableReclaimTest, OpenReaderKeepsDroppedTableUntilItEnds) {
    dropCreateReload(0);
    const auto pID = tableID("P");
    const auto knID = relTableID("KN");
    Connection reader(database.get());
    run(reader, "BEGIN TRANSACTION READ ONLY");
    EXPECT_EQ(count(reader, "MATCH (n:P) RETURN count(*)"), 20);
    run(*conn, "DROP TABLE KN");
    run(*conn, "DROP TABLE P");
    if (!GetParam()) {
        run(*conn, "CHECKPOINT");
    }
    auto* storageManager = database->getStorageManager();
    EXPECT_TRUE(storageManager->containsTable(pID));
    EXPECT_TRUE(storageManager->containsTable(knID));
    EXPECT_EQ(count(reader, "MATCH (n:P) RETURN count(*)"), 20);
    EXPECT_EQ(count(reader, "MATCH (n:P) RETURN sum(n.g)"), 40);
    EXPECT_EQ(count(reader, "MATCH ()-[k:KN]->() RETURN count(*)"), 20);
    run(reader, "COMMIT");
    if (!GetParam()) {
        run(*conn, "CHECKPOINT");
    }
    EXPECT_FALSE(storageManager->containsTable(pID));
    EXPECT_FALSE(storageManager->containsTable(knID));
    run(*conn, "CREATE NODE TABLE P(id INT64, name STRING, g INT64, PRIMARY KEY(id))");
    EXPECT_EQ(count(*conn, "MATCH (n:P) RETURN count(*)"), 0);
}

INSTANTIATE_TEST_SUITE_P(DropTableReclaim, DropTableReclaimTest, ::testing::Values(false, true),
    [](const ::testing::TestParamInfo<bool>& info) {
        return info.param ? std::string("InMemory") : std::string("OnDisk");
    });

} // namespace testing
} // namespace riag
