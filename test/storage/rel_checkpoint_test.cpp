#include "catalog/catalog.h"
#include "catalog/catalog_entry/table_catalog_entry.h"
#include "common/enums/rel_direction.h"
#include "graph_test/private_graph_test.h"
#include "main/connection.h"
#include "main/database.h"
#include "storage/storage_manager.h"
#include "storage/table/csr_node_group.h"
#include "storage/table/rel_table.h"

namespace riag {
namespace testing {

// riag-rename:skip-next upstream provenance
// Regression test for cntsafety/RiaGraph#16 (LadybugDB/ladybug#611).
//
// `CSRNodeGroup::checkpointInMemAndOnDisk` invalidates the CSR index entries of deleted
// in-memory rows while collecting per-region changes. When that collection concludes no
// region needs to be written, the function used to return before `finalizeCheckpoint`,
// leaving the in-memory chunked groups attached and the CSR index holding
// `INVALID_ROW_IDX` sentinels. A later scan or checkpoint turns such a sentinel into an
// out-of-range chunk group index via
// `StorageUtils::getQuotientRemainder(row, CHUNKED_NODE_GROUP_CAPACITY)`.
//
// The invariant asserted here: once a rel node group has been checkpointed, its in-memory
// side is empty — no chunked groups and no CSR index.
class RelCheckpointTest : public EmptyDBTest {
protected:
    void SetUp() override {
        EmptyDBTest::SetUp();
        createDBAndConn();
        runQuery("CREATE NODE TABLE person (id INT64, PRIMARY KEY (id));");
        runQuery("CREATE REL TABLE knows (FROM person TO person, val INT64, MANY_MANY);");
        for (auto id = 0; id < 6; id++) {
            runQuery("CREATE (:person {id: " + std::to_string(id) + "});");
        }
    }

    void runQuery(const std::string& query) const {
        auto result = conn->query(query);
        ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    }

    void createRel(int64_t srcID, int64_t dstID, int64_t val) const {
        runQuery("MATCH (a:person), (b:person) WHERE a.id = " + std::to_string(srcID) +
                 " AND b.id = " + std::to_string(dstID) +
                 " CREATE (a)-[:knows {val: " + std::to_string(val) + "}]->(b);");
    }

    storage::RelTable& getKnowsTable() const {
        auto* catalog = database->getCatalog();
        auto* transaction = transaction::Transaction::Get(*conn->getClientContext());
        const auto& entry = catalog->getTableCatalogEntry(transaction, "knows")
                                ->constCast<catalog::RelGroupCatalogEntry>();
        return storage::StorageManager::Get(*conn->getClientContext())
            ->getTable(entry.getRelEntryInfos()[0].oid)
            ->cast<storage::RelTable>();
    }

    // Asserts that the first node group of `direction` holds no in-memory state.
    //
    // The catalog lookup in `getKnowsTable` needs an active transaction — `Transaction::Get`
    // returns null in auto-commit mode and the entry lookup then dereferences it. A CHECKPOINT
    // cannot run inside a transaction, so rather than opening one for the whole fixture (as
    // RelDeleteTest does) one is scoped to the inspection here, and the values are read out
    // before it is committed so that a failing assertion cannot leave it open.
    void expectInMemStateCleared(common::RelDataDirection direction) const {
        runQuery("BEGIN TRANSACTION;");
        auto* nodeGroup = getKnowsTable().getDirectedTableData(direction)->getNodeGroup(0);
        const auto numChunkedGroups = nodeGroup ? nodeGroup->getNumChunkedGroups() : 0;
        const auto* csrIndex =
            nodeGroup ? nodeGroup->cast<storage::CSRNodeGroup>().getCSRIndex() : nullptr;
        runQuery("COMMIT;");

        ASSERT_NE(nullptr, nodeGroup);
        EXPECT_EQ(0u, numChunkedGroups);
        EXPECT_EQ(nullptr, csrIndex);
    }
};

// Deleting every in-memory rel of a node group and then checkpointing must drop the
// node group's in-memory side, not leave a CSR index full of invalid-row sentinels.
TEST_F(RelCheckpointTest, DeleteAllInMemRelsThenCheckpoint) {
    if (inMemMode) {
        GTEST_SKIP() << "CHECKPOINT is a no-op for an in-memory database";
    }
    // A first rel plus a checkpoint, so the node group gains a persistent chunk group and
    // later checkpoints take the `checkpointInMemAndOnDisk` path.
    createRel(0, 1, 1);
    runQuery("CHECKPOINT;");

    // These two commit into the in-memory chunked groups of the same node group.
    createRel(2, 3, 2);
    createRel(4, 5, 3);

    // Delete exactly the in-memory rels: no persistent row changes, so the checkpoint
    // below finds no region to write.
    runQuery("MATCH (a:person)-[e:knows]->(b:person) WHERE a.id = 2 OR a.id = 4 DELETE e;");
    runQuery("CHECKPOINT;");

    expectInMemStateCleared(common::RelDataDirection::FWD);
    expectInMemStateCleared(common::RelDataDirection::BWD);

    // The untouched rel must still be readable.
    auto result = conn->query("MATCH (a:person)-[e:knows]->(b:person) RETURN a.id, b.id, e.val;");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    ASSERT_EQ(1u, result->getNumTuples());
    auto tuple = result->getNext();
    EXPECT_EQ(0, tuple->getValue(0)->getValue<int64_t>());
    EXPECT_EQ(1, tuple->getValue(1)->getValue<int64_t>());
    EXPECT_EQ(1, tuple->getValue(2)->getValue<int64_t>());
}

} // namespace testing
} // namespace riag
