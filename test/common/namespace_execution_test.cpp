#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "catalog/catalog.h"
#include "catalog/catalog_entry/index_catalog_entry.h"
#include "catalog/catalog_entry/table_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include "main/query_result/arrow_query_result.h"
#include "processor/result/flat_tuple.h"
#include "transaction/transaction.h"
#include <latch>

using namespace riag::main;

namespace {
class NamespaceExecutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        database = std::make_unique<Database>("", config);
        connection = std::make_unique<Connection>(database.get());
        run("CREATE NAMESPACE red");
        run("CREATE NAMESPACE blue");
        run("CREATE NODE TABLE P(id INT64 PRIMARY KEY, name STRING)");
        run("CREATE REL TABLE R(FROM P TO P)");
    }
    void run(const std::string& query) {
        auto result = connection->query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    }
    std::string scalar(const std::string& query) {
        auto result = connection->query(query);
        if (!result->isSuccess()) {
            ADD_FAILURE() << query << "\n" << result->getErrorMessage();
            return "<error>";
        }
        if (!result->hasNext()) {
            ADD_FAILURE() << query << " returned no row";
            return "<empty>";
        }
        return result->getNext()->getValue(0)->toString();
    }
    void fails(const std::string& query, const std::string& text = "") {
        auto result = connection->query(query);
        EXPECT_FALSE(result->isSuccess()) << query;
        if (!text.empty()) {
            EXPECT_NE(result->getErrorMessage().find(text), std::string::npos)
                << query << "\n"
                << result->getErrorMessage();
        }
    }
    void nodes() {
        run("CREATE (:P {id:0, name:'global'}), (:P {id:1, name:'red'} IN red), (:P {id:2, "
            "name:'blue'} IN blue)");
    }
    std::unique_ptr<Database> database;
    std::unique_ptr<Connection> connection;
};

TEST_F(NamespaceExecutionTest, UnrestrictedAndInlineIntersection) {
    nodes();
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "3");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.name"), "red");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN n.id"), "0");
    run("USE NAMESPACE red");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "0");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) IN ALL NAMESPACES RETURN n.id"), "2");
    run("USE NAMESPACE ALL NAMESPACES");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "3");
}

TEST_F(NamespaceExecutionTest, IndependentRelationshipAndEndpointScopes) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a)-[:R IN GLOBAL]->(b)");
    EXPECT_EQ(scalar("MATCH (a:P IN red)-[r:R IN GLOBAL]->(b:P IN blue) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (a:P)-[r:R]->(b:P) IN red, blue RETURN count(*)"), "0");
    EXPECT_EQ(scalar("MATCH (b:P IN blue)<-[r:R IN GLOBAL]-(a:P IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (a:P IN red)-[r:R IN red]->(b:P IN blue) RETURN count(*)"), "0");
}

TEST_F(NamespaceExecutionTest, NamedRelationshipSurvivesCommitAndProjectsNamespace) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a)-[:R IN red]->(b)");
    EXPECT_EQ(scalar("MATCH ()-[r:R IN red]->() RETURN namespace(r)"), "red");
    EXPECT_EQ(scalar("MATCH ()-[r:R IN blue]->() RETURN count(*)"), "0");
    run("MATCH ()-[r:R IN red]->() SET r IN blue");
    EXPECT_EQ(scalar("MATCH ()-[r:R IN blue]->() RETURN namespace(r)"), "blue");
}

TEST_F(NamespaceExecutionTest, NamespaceProjectionAndNull) {
    nodes();
    EXPECT_EQ(scalar("MATCH (n:P {id:1}) RETURN namespace(n)"), "red");
    auto result = connection->query("MATCH (n:P {id:0}) RETURN namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    ASSERT_TRUE(result->hasNext());
    EXPECT_TRUE(result->getNext()->getValue(0)->isNull());
    result = connection->query("RETURN namespace(NULL)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_TRUE(result->getNext()->getValue(0)->isNull());
}

TEST_F(NamespaceExecutionTest, CreationProjectionAndSessionDefault) {
    EXPECT_EQ(scalar("CREATE (n:P {id:1} IN red) RETURN namespace(n)"), "red");
    run("USE NAMESPACE blue");
    EXPECT_EQ(scalar("CREATE (n:P {id:2}) RETURN namespace(n)"), "blue");
    run("USE NAMESPACE red, blue");
    fails("CREATE (:P {id:3})");
    run("CREATE (:P {id:3} IN GLOBAL)");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) IN ALL NAMESPACES RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, EarlierWritesRemainVisibleToLaterMatchInTheSameQuery) {
    EXPECT_EQ(scalar("CREATE (:P {id:1} IN red) WITH 1 AS seed MATCH (n:P IN red) RETURN count(*)"),
        "1");
    run("CREATE NODE TABLE T(id INT64 PRIMARY KEY)");
    EXPECT_EQ(scalar("CREATE (:T {id:1} IN red) WITH 1 AS seed MATCH (n:T) RETURN namespace(n)"),
        "red");
}

TEST_F(NamespaceExecutionTest, CachedPlansObserveUncommittedPromotionAndRollback) {
    run("CREATE (:P {id:0})");
    auto prepared = connection->prepare("MATCH (n:P {id:$id}) RETURN namespace(n)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id:1} IN red)");
    auto result =
        connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{1}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "red");
    run("ROLLBACK");
    result = connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{0}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_TRUE(result->getNext()->getValue(0)->isNull());
}

TEST_F(NamespaceExecutionTest, TransactionControlRemainsAvailableAfterDroppingSelectedNamespace) {
    run("USE NAMESPACE red");
    run("BEGIN TRANSACTION");
    run("DROP NAMESPACE red");
    run("COMMIT");
    run("USE NAMESPACE ALL NAMESPACES");
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, MovesPreserveIdentityAndRelationships) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a)-[:R IN red]->(b)");
    const auto id = scalar("MATCH (n:P {id:1}) RETURN id(n)");
    EXPECT_EQ(scalar("MATCH (n:P {id:1}) SET n IN blue RETURN namespace(n)"), "blue");
    EXPECT_EQ(scalar("MATCH (n:P {id:1}) RETURN id(n)"), id);
    EXPECT_EQ(scalar("MATCH (a:P IN blue)-[r:R IN red]->(b:P IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, GlobalPrimaryKeyStaysGlobal) {
    run("CREATE (:P {id:1} IN red)");
    fails("CREATE (:P {id:1} IN blue)");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, LocalPrimaryKeysUseSeparateDomains) {
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY, name STRING)");
    run("CREATE (:L {id:1, name:'r'} IN red), (:L {id:1, name:'b'} IN blue), (:L {id:1, "
        "name:'g'})");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN red) RETURN n.name"), "r");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN blue) RETURN n.name"), "b");
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "3");
    EXPECT_EQ(scalar("MATCH (n:L) RETURN count(DISTINCT n.id)"), "1");
    fails("CREATE (:L {id:1} IN red)");
    fails("MATCH (n:L IN red) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:L IN red) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, LocalMoveAndRollback) {
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:L {id:1} IN red)");
    run("BEGIN TRANSACTION");
    run("MATCH (n:L IN red) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN blue) RETURN count(*)"), "1");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN blue) RETURN count(*)"), "0");
    run("MATCH (n:L IN red) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, DefinitionAndDataRollback) {
    run("BEGIN TRANSACTION");
    run("CREATE NAMESPACE temporary");
    run("CREATE (:P {id:8} IN temporary)");
    EXPECT_EQ(scalar("MATCH (n:P IN temporary) RETURN count(*)"), "1");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "0");
    fails("USE NAMESPACE temporary");
    run("BEGIN TRANSACTION");
    fails("USE NAMESPACE red");
}

TEST_F(NamespaceExecutionTest, NumericMetadataLookupSurvivesRepeatedRollbackAndPolicyReplacement) {
    run("CREATE (:P {id:0})");
    for (auto attempt = 0; attempt < 3; ++attempt) {
        run("BEGIN TRANSACTION");
        run("MATCH (n:P) SET n IN red");
        run("ALTER TABLE P SET NAMESPACES red");
        EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "1");
        run("ROLLBACK");
        EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "1");
        EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "0");
    }
    run("MATCH (n:P) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN namespace(n)"), "blue");
    run("BEGIN TRANSACTION");
    run("ALTER TABLE P SET NAMESPACES blue");
    run("ROLLBACK");
    run("CREATE (:P {id:1} IN red)");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, TablePoliciesAreRestrictiveAndDynamicAllIsDefault) {
    run("CREATE NODE TABLE T IN red, blue (id INT64 PRIMARY KEY)");
    fails("CREATE (:T {id:0})");
    run("CREATE (:T {id:1} IN red)");
    fails("ALTER TABLE T REMOVE FROM NAMESPACE red");
    run("ALTER TABLE T ADD TO NAMESPACE GLOBAL");
    run("CREATE (:T {id:0})");
    run("MATCH (n:T IN red) DELETE n");
    run("ALTER TABLE T REMOVE FROM NAMESPACE red");
    run("CREATE NAMESPACE future");
    run("CREATE (:P {id:9} IN future)");
    fails("CREATE (:T {id:9} IN future)");
}

// ADD on an ALL policy is a no-op once its names resolve; REMOVE from ALL still fails
// (namespace.md section 4.3, RiaGraphDev#94).
TEST_F(NamespaceExecutionTest, AddingToAnAllPolicyIsANoOp) {
    run("CREATE NODE TABLE T IN ALL NAMESPACES (id INT64 GLOBAL PRIMARY KEY)");
    run("ALTER TABLE T ADD TO NAMESPACE red");
    run("ALTER TABLE T ADD TO NAMESPACE GLOBAL, blue");
    fails("ALTER TABLE T ADD TO NAMESPACE nope", "nope");
    fails("ALTER TABLE T REMOVE FROM NAMESPACE red", "SET NAMESPACES");
    run("CREATE NAMESPACE later");
    run("CREATE (:T {id:1} IN later), (:T {id:2})");
    EXPECT_EQ(scalar("MATCH (n:T) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, RestrictiveDropAndDeletionCleanup) {
    nodes();
    fails("DROP NAMESPACE red");
    run("MATCH (n:P IN red) DELETE n");
    run("DROP NAMESPACE red");
    fails("MATCH (n:P IN red) RETURN n");
    run("CREATE NAMESPACE red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "0");
    run("CREATE NODE TABLE T IN red (id INT64 PRIMARY KEY)");
    fails("DROP NAMESPACE red");
    run("DROP TABLE T");
    run("DROP NAMESPACE red");
}

TEST_F(NamespaceExecutionTest, DetachDeleteChecksHiddenIncidentElementsAndAliases) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a)-[:R IN red]->(b)");
    fails("MATCH (n:P) IN red WITH n AS x DETACH DELETE x");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "3");
    EXPECT_EQ(scalar("MATCH ()-[r:R]->() RETURN count(*)"), "1");
    run("MATCH (n:P IN red) IN ALL NAMESPACES DETACH DELETE n");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH ()-[r:R]->() RETURN count(*)"), "0");
    run("DROP NAMESPACE red");
}

TEST_F(NamespaceExecutionTest, CachedPlansRefreshOnPromotionAndSessionChanges) {
    run("CREATE (:P {id:0})");
    auto prepared = connection->prepare("MATCH (n:P IN GLOBAL) RETURN count(*)");
    ASSERT_TRUE(prepared->isSuccess());
    EXPECT_EQ(connection->execute(prepared.get())->getNext()->getValue(0)->toString(), "1");
    run("CREATE (:P {id:1} IN red)");
    EXPECT_EQ(connection->execute(prepared.get())->getNext()->getValue(0)->toString(), "1");
    auto all = connection->prepare("MATCH (n:P) RETURN count(*)");
    ASSERT_TRUE(all->isSuccess());
    run("USE NAMESPACE red");
    EXPECT_EQ(connection->execute(all.get())->getNext()->getValue(0)->toString(), "1");
    run("USE NAMESPACE ALL NAMESPACES");
    EXPECT_EQ(connection->execute(all.get())->getNext()->getValue(0)->toString(), "2");
}

TEST_F(NamespaceExecutionTest, BoundWriteNamespaceIsAssertion) {
    nodes();
    fails("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a IN blue)-[:R]->(b)");
    EXPECT_EQ(scalar("MATCH ()-[r:R]->() RETURN count(*)"), "0");
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a IN red)-[:R IN red]->(b IN blue)");
    EXPECT_EQ(scalar("MATCH ()-[r:R IN red]->() RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, RecursiveScopesIncludeIntermediateNodes) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}), (c:P {id:0}) CREATE (a)-[:R IN red]->(b)-[:R IN "
        "red]->(c)");
    EXPECT_EQ(scalar("MATCH (a:P {id:1})-[:R*2..2 IN red]->(b:P {id:0}) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (a:P {id:1})-[:R*2..2 IN red]->(b:P) IN red, GLOBAL RETURN count(*)"),
        "0");
    EXPECT_EQ(scalar("MATCH (a:P {id:1})-[r:R*0..0 IN blue]->(b:P {id:1}) IN red RETURN count(*)"),
        "1");
}

TEST_F(NamespaceExecutionTest, MergeMatchesOnlyTargetNamespace) {
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("MERGE (:L {id:1} IN red)");
    run("MERGE (:L {id:1} IN blue)");
    run("MERGE (:L {id:1} IN red)");
    EXPECT_EQ(scalar("MATCH (n:L) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, LocalKeysRollbackInsertAndMoveThenReuse) {
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:L {id:1} IN red)");
    run("BEGIN TRANSACTION");
    run("CREATE (:L {id:2} IN blue)");
    run("MATCH (n:L IN red) SET n IN blue");
    run("ROLLBACK");
    run("CREATE (:L {id:2} IN red)");
    run("CREATE (:L {id:1} IN blue)");
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (n:L {id:2} IN blue) RETURN count(*)"), "0");
    run("MATCH (n:L {id:1} IN red) DELETE n");
    run("CREATE (:L {id:1} IN red)");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN red) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, MergeMovesAndPropertyUpdates) {
    run("MERGE (n:P {id:1} IN red) ON CREATE SET n.name='created', n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN n.name"), "created");
    run("MERGE (n:P {id:1} IN blue) ON MATCH SET n.name='matched', n IN red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.name"), "matched");
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("UNWIND [1,1,1] AS i MERGE (n:L {id:i} IN red)");
    EXPECT_EQ(scalar("MATCH (n:L IN red) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, MembershipSnapshotSurvivesConcurrentMove) {
    nodes();
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    auto before = reader.query("MATCH (n:P IN red) RETURN count(*)");
    ASSERT_TRUE(before->isSuccess()) << before->getErrorMessage();
    EXPECT_EQ(before->getNext()->getValue(0)->toString(), "1");
    run("MATCH (n:P IN red) SET n IN blue");
    auto after = reader.query("MATCH (n:P IN red) RETURN count(*)");
    ASSERT_TRUE(after->isSuccess()) << after->getErrorMessage();
    EXPECT_EQ(after->getNext()->getValue(0)->toString(), "1");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "0");
}

TEST_F(NamespaceExecutionTest, OldSnapshotRetainsNamespaceIdentityAfterDropAndRecreate) {
    run("CREATE (:P {id:1} IN red)");
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    run("MATCH (n:P {id:1}) DELETE n");
    run("DROP NAMESPACE red");
    run("CREATE NAMESPACE red");
    run("CREATE (:P {id:2} IN red)");
    auto result = reader.query("MATCH (n:P IN red) RETURN n.id, namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    ASSERT_TRUE(result->hasNext());
    auto tuple = result->getNext();
    EXPECT_EQ(tuple->getValue(0)->toString(), "1");
    EXPECT_EQ(tuple->getValue(1)->toString(), "red");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.id"), "2");
}

TEST_F(NamespaceExecutionTest, NamespaceScansCrossVectorAndMembershipGroupBoundaries) {
    run("UNWIND range(0,4999) AS i CREATE (:P {id:i} IN red)");
    run("UNWIND range(5000,9999) AS i CREATE (:P {id:i} IN blue)");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "5000");
    EXPECT_EQ(scalar("MATCH (n:P IN red, blue) RETURN count(*)"), "10000");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN sum(n.id)"), "12497500");
    run("MATCH (n:P IN red) WHERE n.id%2=0 SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "2500");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "7500");
    run("MATCH (n:P IN red) DELETE n");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "0");
    run("DROP NAMESPACE red");
}

TEST_F(NamespaceExecutionTest, ScopedVectorScansIncludeUncommittedRowsAndObserveMoves) {
    nodes();
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id:3} IN red), (:P {id:4} IN blue)");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "2");
    run("MATCH (n:P {id:3}) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "3");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, WarmMembershipOffsetsObserveMovesRollbackAndOldSnapshots) {
    run("UNWIND range(0,4095) AS i CREATE (:P {id:i} IN red)");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN sum(n.id)"), "8386560");
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    auto oldCount = reader.query("MATCH (n:P IN red) RETURN count(*)");
    ASSERT_TRUE(oldCount->isSuccess()) << oldCount->getErrorMessage();
    EXPECT_EQ(oldCount->getNext()->getValue(0)->toString(), "4096");
    run("BEGIN TRANSACTION");
    run("MATCH (n:P IN red) WHERE n.id%2=0 SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "2048");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "2048");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "4096");
    run("MATCH (n:P IN red) WHERE n.id%2=0 SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN sum(n.id)"), "4194304");
    auto snapshot = reader.query("MATCH (n:P IN red) RETURN count(*)");
    ASSERT_TRUE(snapshot->isSuccess()) << snapshot->getErrorMessage();
    EXPECT_EQ(snapshot->getNext()->getValue(0)->toString(), "4096");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    run("MATCH (n:P IN blue) SET n IN red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN sum(n.id)"), "8386560");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "0");
}

TEST_F(NamespaceExecutionTest, MembershipVersionsAreReclaimedAfterSnapshotRelease) {
    run("CREATE (:P {id:1} IN red)");
    auto* catalog = riag::catalog::Catalog::Get(*connection->getClientContext());
    const auto* latest = &riag::transaction::DUMMY_CHECKPOINT_TRANSACTION;
    const auto tableID = catalog->getTableCatalogEntry(latest, "P")->getTableID();
    const auto depth = [&]() {
        uint64_t count = 1;
        const riag::catalog::CatalogEntry* entry =
            catalog->getNamespaces()->getGroup(latest, tableID, 0);
        while (entry->hasPrev()) {
            entry = entry->getPrev();
            ++count;
        }
        return count;
    };
    for (uint64_t i = 0; i < 40; ++i) {
        run(std::string("MATCH (n:P) SET n IN ") + (i % 2 ? "red" : "blue"));
        EXPECT_LE(depth(), 2);
    }
    EXPECT_EQ(catalog->getNamespaces()->getGroup(&riag::transaction::DUMMY_TRANSACTION, tableID, 0),
        nullptr);
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    ASSERT_TRUE(reader.query("MATCH (n:P IN red) RETURN count(*)")->isSuccess());
    for (uint64_t i = 0; i < 40; ++i) {
        run(std::string("MATCH (n:P) SET n IN ") + (i % 2 ? "red" : "blue"));
    }
    EXPECT_GE(depth(), 40);
    auto snapshot = reader.query("MATCH (n:P IN red) RETURN count(*)");
    ASSERT_TRUE(snapshot->isSuccess()) << snapshot->getErrorMessage();
    EXPECT_EQ(snapshot->getNext()->getValue(0)->toString(), "1");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    run("MATCH (n:P) SET n IN blue");
    EXPECT_LE(depth(), 2);
    run("BEGIN TRANSACTION");
    run("MATCH (n:P) SET n IN red");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, GlobalScanMasksIncludeGapsUnionsSnapshotsAndLocalRows) {
    run("UNWIND range(0,4999) AS i CREATE (:P {id:i})");
    run("MATCH (n:P) WHERE n.id>=1024 AND n.id<2048 SET n IN red");
    run("MATCH (n:P) WHERE n.id>=3072 AND n.id<4096 SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "2952");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN sum(n.id)"), "7255644");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL, red) RETURN count(*)"), "3976");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL, red) RETURN sum(n.id)"), "8827996");
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    run("MATCH (n:P) WHERE n.id<128 SET n IN red");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "2824");
    auto old = reader.query("MATCH (n:P IN GLOBAL) RETURN count(*)");
    ASSERT_TRUE(old->isSuccess()) << old->getErrorMessage();
    EXPECT_EQ(old->getNext()->getValue(0)->toString(), "2952");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id:5000}), (:P {id:5001} IN red)");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "2825");
    run("MATCH (n:P {id:5000}) SET n IN blue");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "2824");
    run("ROLLBACK");
    run("MATCH (n:P IN red) SET n IN GLOBAL");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "3976");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL), (m:P IN GLOBAL) "
                     "WHERE n.id=m.id AND m.id=4999 RETURN n.id"),
        "4999");
}

TEST_F(NamespaceExecutionTest, ConcurrentMembershipPublicationKeepsPinnedReaderConsistent) {
    nodes();
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> reads{0};
    std::latch ready{1};
    std::string error;
    std::thread worker([&]() {
        const auto readOnce = [&]() {
            auto result = reader.query("MATCH (n:P IN red) RETURN count(*)");
            if (!result->isSuccess()) {
                error = result->getErrorMessage();
                return false;
            }
            if (result->getNext()->getValue(0)->toString() != "1") {
                error = "Pinned membership snapshot changed during publication.";
                return false;
            }
            reads.fetch_add(1, std::memory_order_relaxed);
            return true;
        };
        const auto initial = readOnce();
        ready.count_down();
        if (!initial) {
            return;
        }
        while (!stop.load(std::memory_order_relaxed) && readOnce()) {}
    });
    ready.wait();
    for (uint64_t i = 0; i < 80; ++i) {
        run(std::string("MATCH (n:P {id:1}) SET n IN ") + (i % 2 ? "red" : "blue"));
    }
    stop.store(true, std::memory_order_relaxed);
    worker.join();
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_GT(reads.load(std::memory_order_relaxed), 0);
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, DenseGlobalMaskPreservesDeletedRowsAndFilteredInputs) {
    run("UNWIND range(0,4095) AS i CREATE (:P {id:i})");
    run("MATCH (n:P {id:4095}) SET n IN red");
    run("MATCH (n:P {id:1000}) DELETE n");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "4094");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN sum(n.id)"), "8381465");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL, red) RETURN count(*)"), "4095");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL), (m:P IN GLOBAL) "
                     "WHERE n.id=m.id AND m.id IN [0,1000,2048] RETURN sum(n.id)"),
        "2048");
}

TEST_F(NamespaceExecutionTest, SingletonLocalStringKeyPlansRebindToMultipleNamespaces) {
    run("CREATE NODE TABLE L(id STRING LOCAL PRIMARY KEY, value INT64)");
    run("CREATE (:L {id:'same',value:10} IN red), (:L {id:'same',value:20} IN blue), "
        "(:L {id:'same',value:30})");
    EXPECT_EQ(scalar("MATCH (n:L {id:'missing'} IN red) RETURN count(*)"), "0");
    EXPECT_EQ(scalar("MATCH (n:L {id:'same'} IN GLOBAL) RETURN n.value"), "30");
    run("USE NAMESPACE red");
    auto prepared = connection->prepare("MATCH (n:L {id:$id}) RETURN sum(n.value)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    const auto check = [&](const std::string& expected) {
        auto result = connection->execute(prepared.get(),
            std::make_pair(std::string("id"), std::string("same")));
        ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
        EXPECT_EQ(result->getNext()->getValue(0)->toString(), expected);
    };
    check("10");
    run("USE NAMESPACE blue");
    check("20");
    run("USE NAMESPACE red, blue");
    check("30");
    run("USE NAMESPACE ALL NAMESPACES");
    check("60");
}

TEST_F(NamespaceExecutionTest, DiagnosticsDescribeImplicitAndMaterializedMembership) {
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN membership_storage"), "IMPLICIT_GLOBAL");
    nodes();
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN membership_storage"), "MVCC_GROUPS");
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN element_counts"),
        "GLOBAL=1, `red`=1, `blue`=1");
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN primary_key_scope"), "GLOBAL");
    run("MATCH (n:P IN red) SET n IN blue");
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN element_counts"),
        "GLOBAL=1, `red`=0, `blue`=2");
    run("MATCH (n:P {id:2} IN blue) DELETE n");
    EXPECT_EQ(scalar("CALL namespace_info('P') RETURN element_counts"),
        "GLOBAL=1, `red`=0, `blue`=1");
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    EXPECT_EQ(scalar("CALL namespace_info('L') RETURN primary_key_scope"), "LOCAL");
}

TEST_F(NamespaceExecutionTest, OptionalSubqueryAndGeneralNodeValuesRespectScopes) {
    nodes();
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN namespace(CASE WHEN true THEN n ELSE n END)"),
        "red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) WITH [n] AS xs UNWIND xs AS x RETURN namespace(x)"),
        "red");
    EXPECT_EQ(scalar("MATCH (a:P IN red) OPTIONAL MATCH (a)-[r:R IN blue]->(b:P) RETURN count(r)"),
        "0");
    EXPECT_EQ(scalar("MATCH (n:P) IN red WHERE EXISTS { MATCH (n:P IN blue) } RETURN count(*)"),
        "0");
    run("USE NAMESPACE red");
    EXPECT_EQ(scalar("MATCH (n:P IN blue) IN ALL NAMESPACES RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, ShortestAndWeightedPathsFilterBeforeChoosingPaths) {
    run("CREATE REL TABLE S(FROM P TO P, cost INT64)");
    run("CREATE (:P {id:1} IN red), (:P {id:2} IN blue), (:P {id:3} IN red), (:P {id:4} IN red)");
    run("MATCH (a:P {id:1}), (b:P {id:2}), (c:P {id:3}), (d:P {id:4}) "
        "CREATE (a)-[:S {cost:1} IN blue]->(d), (a)-[:S {cost:1} IN red]->(b), "
        "(b)-[:S {cost:1} IN red]->(d), (a)-[:S {cost:3} IN red]->(c), (c)-[:S {cost:3} IN "
        "red]->(d)");
    EXPECT_EQ(
        scalar(
            "MATCH (a:P {id:1})-[r:S* SHORTEST 1..4 IN red]->(b:P {id:4}) IN red RETURN length(r)"),
        "2");
    EXPECT_EQ(scalar("MATCH (a:P {id:1})-[r:S* WSHORTEST(cost) 1..4 IN red]->(b:P {id:4}) IN red "
                     "RETURN CAST(cost(r) AS INT64)"),
        "6");
    EXPECT_EQ(scalar("MATCH (a:P {id:1})-[r:S* ALL SHORTEST 1..4 IN red]->(b:P {id:4}) IN red "
                     "RETURN count(*)"),
        "1");
}

TEST_F(NamespaceExecutionTest, LocalStringKeyRangeAndSnapshotLookups) {
    run("CREATE NODE TABLE L(key STRING LOCAL PRIMARY KEY)");
    run("CREATE (:L {key:'a'} IN red), (:L {key:'a'} IN blue), (:L {key:'b'} IN red)");
    EXPECT_EQ(scalar("MATCH (n:L) WHERE n.key >= 'a' AND n.key <= 'b' RETURN count(*)"), "3");
    EXPECT_EQ(scalar("MATCH (n:L IN red) WHERE n.key >= 'a' AND n.key <= 'b' RETURN count(*)"),
        "2");
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    run("MATCH (n:L {key:'b'} IN red) SET n IN blue");
    auto result = reader.query("MATCH (n:L {key:'b'} IN red) RETURN count(*)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "1");
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    EXPECT_EQ(scalar("MATCH (n:L {key:'b'} IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceExecutionTest, PreparedWritesUseCurrentSessionAndGraphSwitchResetsSelection) {
    auto prepared = connection->prepare("CREATE (n:P {id:$id}) RETURN namespace(n)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    run("USE NAMESPACE red");
    auto result =
        connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{1}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "red");
    run("USE NAMESPACE blue");
    result = connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{2}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "blue");
    run("CREATE GRAPH other");
    run("USE GRAPH other");
    run("CREATE NODE TABLE Q(id INT64 PRIMARY KEY)");
    EXPECT_EQ(scalar("CREATE (n:Q {id:1}) RETURN count(*)"), "1");
    run("USE GRAPH main");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, DroppingSelectedGraphResetsScopeBeforeNextStatement) {
    nodes();
    run("CREATE GRAPH alpha");
    run("USE GRAPH alpha");
    run("CREATE NAMESPACE tenant");
    run("CREATE NODE TABLE P(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:P {id:1} IN tenant)");
    run("USE NAMESPACE tenant");
    run("DROP GRAPH alpha");
    EXPECT_EQ(scalar("RETURN 1"), "1");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "3");
    run("CREATE (:P {id:3})");
    EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, CtasUsesSessionTargetAndRejectsAmbiguity) {
    nodes();
    run("USE NAMESPACE red");
    run("CREATE NODE TABLE T AS MATCH (n:P) RETURN n.id AS id, n.name AS name");
    EXPECT_EQ(scalar("MATCH (n:T) RETURN namespace(n)"), "red");
    run("USE NAMESPACE blue");
    EXPECT_EQ(scalar("MATCH (n:T) RETURN count(*)"), "0");
    run("USE NAMESPACE red, blue");
    fails("CREATE NODE TABLE U AS MATCH (n:P) RETURN n.id AS id");
}

TEST_F(NamespaceExecutionTest, ArrowAndCsrResultsEnforceScopesAndBlindProjectionRejectsThem) {
    nodes();
    run("MATCH (a:P {id:1}), (b:P {id:2}) CREATE (a)-[:R IN GLOBAL]->(b)");
    auto result = connection->queryAsArrow(
        "MATCH (a:P IN red)-[r:R IN GLOBAL]->(b:P IN blue) RETURN a.rowid, r.rowid, b.rowid", 2);
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    auto* arrow = dynamic_cast<ArrowQueryResult*>(result.get());
    ASSERT_NE(arrow, nullptr);
    if (arrow->hasCSRMetadata()) {
        EXPECT_EQ(arrow->getCSRMetadata().indices.size(), 1);
    } else {
        EXPECT_EQ(result->getNumTuples(), 1);
    }
    run("USE NAMESPACE red, blue");
    result =
        connection->queryAsArrow("MATCH (a:P)-[r:R]->(b:P) RETURN a.rowid, r.rowid, b.rowid", 2);
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNumTuples(), 0);
    fails("CALL PROJECT_GRAPH('scoped', ['P'], ['R'])", "namespace");
}

TEST_F(NamespaceExecutionTest, AddedRelationshipConnectionsInheritNamespacePolicy) {
    run("CREATE NODE TABLE Q(id INT64 PRIMARY KEY)");
    run("CREATE REL TABLE S IN red (FROM P TO P)");
    run("ALTER TABLE S ADD FROM P TO Q");
    run("CREATE (:P {id:1} IN blue), (:Q {id:2} IN blue)");
    fails("MATCH (a:P), (b:Q) CREATE (a)-[:S IN blue]->(b)");
    run("MATCH (a:P), (b:Q) CREATE (a)-[:S IN red]->(b)");
    EXPECT_EQ(scalar("MATCH ()-[r:S IN red]->() RETURN count(*)"), "1");
    run("ALTER TABLE S DROP FROM P TO Q");
    run("DROP TABLE S");
    run("DROP NAMESPACE red");
}

// ALTER NAMESPACE ... RENAME TO (namespace.md, section 4.1) changes only the definition's name.
// The ID, and everything keyed by it, stays: memberships, table policies, LOCAL key domains and
// session selections. Statement text is resolved by name again when its plan is rebound.
TEST_F(NamespaceExecutionTest, RenameKeepsIdentityMembersPoliciesAndLocalKeys) {
    run("CREATE NODE TABLE L IN GLOBAL, red, blue (id INT64 LOCAL PRIMARY KEY, name STRING)");
    run("CREATE REL TABLE E IN red (FROM L TO L)");
    run("CREATE (:L {id:1, name:'r'} IN red), (:L {id:1, name:'b'} IN blue), (:L {id:1})");
    run("MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:E IN red]->(b)");
    const auto id = scalar("CALL show_namespaces() WHERE name = 'red' RETURN id");
    run("ALTER NAMESPACE red RENAME TO crimson");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'crimson' RETURN id"), id);
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    EXPECT_EQ(scalar("MATCH (n:L {name:'r'}) RETURN namespace(n)"), "crimson");
    EXPECT_EQ(scalar("MATCH ()-[e:E]->() RETURN namespace(e)"), "crimson");
    EXPECT_EQ(scalar("MATCH (n:L IN crimson) RETURN n.name"), "r");
    EXPECT_EQ(scalar("MATCH (a:L IN crimson)-[e:E IN crimson]->(b:L IN blue) RETURN b.name"), "b");
    fails("MATCH (n:L IN red) RETURN n", "Namespace red does not exist.");
    fails("USE NAMESPACE red", "Namespace red does not exist.");
    EXPECT_EQ(scalar("MATCH (n:L) WHERE namespace(n) = 'red' RETURN count(*)"), "0");
    // The table policies name the namespace by ID, so they follow the rename.
    EXPECT_EQ(scalar("CALL namespace_info('L') RETURN allowed_namespaces"),
        "GLOBAL, `crimson`, `blue`");
    EXPECT_EQ(scalar("CALL namespace_info('E') RETURN allowed_namespaces"), "`crimson`");
    EXPECT_EQ(scalar("CALL namespace_info('L') RETURN element_counts"),
        "GLOBAL=1, `crimson`=1, `blue`=1");
    // The LOCAL key domain is the renamed namespace's: id 1 is taken there, free elsewhere.
    fails("CREATE (:L {id:1} IN crimson)");
    run("CREATE (:L {id:2, name:'r2'} IN crimson)");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN crimson) RETURN n.name"), "r");
    EXPECT_EQ(scalar("MATCH (n:L {id:2}) RETURN namespace(n)"), "crimson");
    // Dropping stays restrictive: members and the finite policy still reference the ID.
    fails("DROP NAMESPACE crimson", "is referenced by a table policy");
    run("USE NAMESPACE crimson");
    EXPECT_EQ(scalar("MATCH (n:L) RETURN count(*)"), "2");
}

TEST_F(NamespaceExecutionTest, RenameErrors) {
    fails("ALTER NAMESPACE missing RENAME TO other", "Namespace missing does not exist.");
    fails("ALTER NAMESPACE red RENAME TO blue", "Namespace blue already exists.");
    // As ALTER TABLE ... RENAME TO, the current name is taken, in any casing.
    fails("ALTER NAMESPACE red RENAME TO red", "Namespace red already exists.");
    fails("ALTER NAMESPACE red RENAME TO RED", "Namespace RED already exists.");
    for (const auto* global : {"GLOBAL", "global", "Global", "`GLOBAL`", "`global`"}) {
        fails(std::string("ALTER NAMESPACE ") + global + " RENAME TO other",
            "GLOBAL is the implicit namespace and cannot be used as a named namespace.");
        fails(std::string("ALTER NAMESPACE red RENAME TO ") + global,
            "GLOBAL is the implicit namespace and cannot be used as a named namespace.");
    }
    // ALTER TABLE ... RENAME TO has no IF EXISTS; neither has this.
    fails("ALTER NAMESPACE IF EXISTS red RENAME TO other", "Parser exception");
    fails("ALTER NAMESPACE red RENAME TO other, more", "Parser exception");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN count(*)"), "1");
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    // Identifiers follow the catalog rules: quoting keeps spaces and case as written.
    run("ALTER NAMESPACE red RENAME TO `Team Red`");
    run("CREATE (:P {id:1} IN `Team Red`)");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "Team Red");
    EXPECT_EQ(scalar("MATCH (n:P IN `team red`) RETURN count(*)"), "1");
    run("BEGIN TRANSACTION READ ONLY");
    fails("ALTER NAMESPACE `Team Red` RENAME TO red");
    connection->query("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "Team Red");
}

TEST_F(NamespaceExecutionTest, RenameRollsBackAndIsVisibleInsideItsTransaction) {
    nodes();
    run("USE NAMESPACE red");
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO crimson");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "crimson");
    EXPECT_EQ(scalar("MATCH (n:P IN crimson) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN count(*)"), "0");
    run("ROLLBACK");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.name"), "red");
    fails("MATCH (n:P IN crimson) RETURN n", "Namespace crimson does not exist.");
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    // A failing statement aborts the explicit transaction, and the rename with it.
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO crimson");
    fails("MATCH (n:P IN red) RETURN n", "Namespace red does not exist.");
    fails("COMMIT");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "red");
    // Renames compose within a transaction: the identity always ends on one name.
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO crimson");
    run("ALTER NAMESPACE crimson RENAME TO scarlet");
    run("COMMIT");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "scarlet");
    run("ALTER NAMESPACE scarlet RENAME TO red");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "red");
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    // Rolling back DDL on a name the identity held earlier leaves the identity's name alone.
    run("ALTER NAMESPACE red RENAME TO crimson");
    for (const auto* ddl : {"CREATE NAMESPACE red", "CREATE NAMESPACE scarlet",
             "ALTER NAMESPACE crimson RENAME TO red", "ALTER NAMESPACE crimson RENAME TO rose",
             "ALTER NAMESPACE blue RENAME TO red"}) {
        run("BEGIN TRANSACTION");
        run(ddl);
        run("ROLLBACK");
        EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "crimson") << ddl;
        EXPECT_EQ(scalar("MATCH (n:P IN crimson) RETURN count(*)"), "1") << ddl;
        EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3") << ddl;
    }
    run("USE NAMESPACE ALL NAMESPACES");
    EXPECT_EQ(scalar("MATCH (n:P {id:2}) RETURN namespace(n)"), "blue");
}

TEST_F(NamespaceExecutionTest, RenameRebindsPreparedStatementsByName) {
    nodes();
    auto byName = connection->prepare("MATCH (n:P) IN red RETURN count(*)");
    ASSERT_TRUE(byName->isSuccess()) << byName->getErrorMessage();
    auto projection = connection->prepare("MATCH (n:P {id:1}) RETURN namespace(n)");
    ASSERT_TRUE(projection->isSuccess()) << projection->getErrorMessage();
    auto filter = connection->prepare("MATCH (n:P) WHERE namespace(n) = 'red' RETURN count(*)");
    ASSERT_TRUE(filter->isSuccess()) << filter->getErrorMessage();
    auto value = [&](PreparedStatement* statement) -> std::string {
        auto result = connection->execute(statement);
        EXPECT_TRUE(result->isSuccess()) << result->getErrorMessage();
        return result->isSuccess() ? result->getNext()->getValue(0)->toString() : "<error>";
    };
    EXPECT_EQ(value(byName.get()), "1");
    EXPECT_EQ(value(projection.get()), "red");
    EXPECT_EQ(value(filter.get()), "1");
    run("ALTER NAMESPACE red RENAME TO crimson");
    // The old name no longer resolves when the plan is rebound; the statement does not
    // silently follow the identity it was first bound to.
    auto stale = connection->execute(byName.get());
    EXPECT_FALSE(stale->isSuccess());
    EXPECT_NE(stale->getErrorMessage().find("Namespace red does not exist."), std::string::npos)
        << stale->getErrorMessage();
    EXPECT_EQ(value(projection.get()), "crimson");
    EXPECT_EQ(value(filter.get()), "0");
    // A new namespace with the old name is a new identity, without the renamed members.
    run("CREATE NAMESPACE red");
    EXPECT_EQ(value(byName.get()), "0");
    run("CREATE (:P {id:5} IN red)");
    EXPECT_EQ(value(byName.get()), "1");
    EXPECT_EQ(value(filter.get()), "1");
    EXPECT_EQ(scalar("MATCH (n:P IN crimson) RETURN n.id"), "1");
    // An uncommitted rename in the connection's own transaction refreshes the binding too.
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO rose");
    stale = connection->execute(byName.get());
    EXPECT_FALSE(stale->isSuccess());
    connection->query("ROLLBACK");
    EXPECT_EQ(value(byName.get()), "1");
}

TEST_F(NamespaceExecutionTest, RenameKeepsSessionSelectionsAndOtherConnectionsSeeNewName) {
    nodes();
    Connection other(database.get());
    ASSERT_TRUE(other.query("USE NAMESPACE red")->isSuccess());
    run("USE NAMESPACE red");
    auto prepared = connection->prepare("MATCH (n:P) RETURN n.name, namespace(n)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    run("ALTER NAMESPACE red RENAME TO crimson");
    // A selection is an identity, not a name: it follows the renamed namespace.
    auto result = connection->execute(prepared.get());
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    auto tuple = result->getNext();
    EXPECT_EQ(tuple->getValue(0)->toString(), "red");
    EXPECT_EQ(tuple->getValue(1)->toString(), "crimson");
    EXPECT_FALSE(result->hasNext());
    result = other.query("MATCH (n:P) RETURN namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "crimson");
    EXPECT_FALSE(result->hasNext());
    // Creating with the session default still targets the selected identity.
    result = other.query("CREATE (n:P {id:7}) RETURN namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "crimson");
    // A namespace created under the old name afterwards is not the selected one.
    run("CREATE NAMESPACE red");
    result = other.query("MATCH (n:P) RETURN count(*)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "2");
}

TEST_F(NamespaceExecutionTest, OldSnapshotsKeepTheNameTheyStartedWith) {
    nodes();
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    auto read = [&](const std::string& query) -> std::string {
        auto result = reader.query(query);
        EXPECT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
        return result->isSuccess() && result->hasNext() ?
                   result->getNext()->getValue(0)->toString() :
                   "<error>";
    };
    EXPECT_EQ(read("MATCH (n:P {id:1}) RETURN namespace(n)"), "red");
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO crimson");
    // Neither the uncommitted rename nor its commit changes the reader's snapshot.
    EXPECT_EQ(read("MATCH (n:P {id:1}) RETURN namespace(n)"), "red");
    Connection autoReader(database.get());
    auto result = autoReader.query("MATCH (n:P IN red) RETURN namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "red");
    run("COMMIT");
    EXPECT_EQ(read("MATCH (n:P {id:1}) RETURN namespace(n)"), "red");
    EXPECT_EQ(read("MATCH (n:P IN red) RETURN n.name"), "red");
    EXPECT_EQ(read("CALL show_namespaces() WHERE name = 'red' RETURN count(*)"), "1");
    auto missing = reader.query("MATCH (n:P IN crimson) RETURN n");
    EXPECT_FALSE(missing->isSuccess());
    reader.query("ROLLBACK");
    EXPECT_EQ(read("MATCH (n:P {id:1}) RETURN namespace(n)"), "crimson");
    result = autoReader.query("MATCH (n:P IN crimson) RETURN namespace(n)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "crimson");
}

TEST_F(NamespaceExecutionTest, RenameReusesFreedNamesWithoutMergingIdentities) {
    nodes();
    const auto redID = scalar("CALL show_namespaces() WHERE name = 'red' RETURN id");
    run("CREATE NAMESPACE old");
    const auto oldID = scalar("CALL show_namespaces() WHERE name = 'old' RETURN id");
    run("DROP NAMESPACE old");
    // A dropped namespace's name is free: red takes it and keeps its own identity.
    run("ALTER NAMESPACE red RENAME TO old");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'old' RETURN id"), redID);
    EXPECT_NE(redID, oldID);
    EXPECT_EQ(scalar("MATCH (n:P IN old) RETURN n.name"), "red");
    // The freed name red is then a new namespace with a fresh ID and no members.
    run("CREATE NAMESPACE red");
    const auto newRedID = scalar("CALL show_namespaces() WHERE name = 'red' RETURN id");
    EXPECT_GT(std::stoull(newRedID), std::stoull(oldID));
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "0");
    run("CREATE (:P {id:9, name:'new red'} IN red)");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.name"), "new red");
    EXPECT_EQ(scalar("MATCH (n:P IN old) RETURN n.name"), "red");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "4");
    // Swapping two names, as a publication swaps generations, goes through a free name.
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE red RENAME TO swap");
    run("ALTER NAMESPACE old RENAME TO red");
    run("ALTER NAMESPACE swap RENAME TO old");
    run("COMMIT");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN n.name"), "red");
    EXPECT_EQ(scalar("MATCH (n:P IN old) RETURN n.name"), "new red");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN id"), redID);
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'old' RETURN id"), newRedID);
}

TEST_F(NamespaceExecutionTest, RenameInAGraphCatalogStaysInThatCatalog) {
    run("CREATE GRAPH alpha");
    run("USE GRAPH alpha");
    run("CREATE NAMESPACE red");
    run("CREATE NODE TABLE Q(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:Q {id:1} IN red)");
    run("ALTER NAMESPACE red RENAME TO crimson");
    EXPECT_EQ(scalar("MATCH (n:Q) RETURN namespace(n)"), "crimson");
    run("USE GRAPH main");
    // The main catalog's red is another identity and keeps its name.
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN count(*)"), "1");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'crimson' RETURN count(*)"), "0");
}

class NamespaceMultiWriteTest : public NamespaceExecutionTest {
protected:
    void SetUp() override {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        config.enableMultiWrites = true;
        database = std::make_unique<Database>("", config);
        connection = std::make_unique<Connection>(database.get());
        run("CREATE NAMESPACE red");
        run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
    }
};

TEST_F(NamespaceMultiWriteTest, NamespaceSchemaChangesConflictWithOtherWriters) {
    Connection other(database.get());
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id:1} IN red)");
    auto drop = other.query("DROP NAMESPACE red");
    EXPECT_FALSE(drop->isSuccess());
    run("COMMIT");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "1");
    run("BEGIN TRANSACTION");
    run("CREATE NAMESPACE blue");
    auto write = other.query("CREATE (:P {id:2})");
    EXPECT_FALSE(write->isSuccess());
    run("ROLLBACK");
    write = other.query("CREATE (:P {id:2})");
    EXPECT_TRUE(write->isSuccess()) << write->getErrorMessage();
}

TEST_F(NamespaceMultiWriteTest, StaleNamespaceDdlSnapshotCannotDropCommittedMembership) {
    run("BEGIN TRANSACTION");
    Connection other(database.get());
    auto write = other.query("CREATE (:P {id:1} IN red)");
    ASSERT_TRUE(write->isSuccess()) << write->getErrorMessage();
    fails("DROP NAMESPACE red");
    EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "1");
}

class NamespacePersistenceTest : public NamespaceExecutionTest {
protected:
    void SetUp() override {
        directory =
            std::filesystem::temp_directory_path() /
            ("riag_namespace_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        open();
    }
    void TearDown() override {
        connection.reset();
        database.reset();
        std::filesystem::remove_all(directory);
    }
    void open() {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        config.autoCheckpoint = false;
        config.forceCheckpointOnClose = false;
        database = std::make_unique<Database>((directory / "database").string(), config);
        connection = std::make_unique<Connection>(database.get());
    }
    void reopen() {
        connection.reset();
        database.reset();
        open();
    }
    std::filesystem::path directory;
};

TEST_F(NamespacePersistenceTest, CheckpointReopenPreservesLocalKeysAndCrossNamespaceEdges) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L IN red, blue (id INT64 LOCAL PRIMARY KEY)");
    run("CREATE REL TABLE R(FROM L TO L)");
    run("CREATE (:L {id:1} IN red), (:L {id:1} IN blue)");
    run("MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:R IN red]->(b)");
    run("CHECKPOINT");
    reopen();
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R IN red]->(b:L IN blue) RETURN count(*)"), "1");
    fails("CREATE (:L {id:1} IN red)");
    run("MATCH ()-[r:R]->() SET r IN blue");
    run("CHECKPOINT");
    reopen();
    EXPECT_EQ(scalar("MATCH ()-[r:R IN blue]->() RETURN count(*)"), "1");
}

TEST_F(NamespacePersistenceTest, SelectedGraphNamespaceCatalogAndLocalIndexesReopen) {
    run("CREATE GRAPH alpha");
    run("USE GRAPH alpha");
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE P(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:P {id:1} IN red), (:P {id:1} IN blue)");
    run("USE NAMESPACE red");
    run("CHECKPOINT");
    run("USE GRAPH main");
    reopen();
    run("USE GRAPH alpha");
    EXPECT_EQ(scalar("MATCH (n:P {id:1}) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (n:P {id:1} IN blue) RETURN namespace(n)"), "blue");
    run("USE NAMESPACE red");
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "1");
    run("USE GRAPH main");
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "1");
}

TEST_F(NamespacePersistenceTest, UniformRunAndSparseGroupsSurviveWalAndCheckpoint) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
    run("UNWIND range(0,2047) AS i CREATE (:P {id:i} IN red)");
    run("UNWIND range(2048,3071) AS i CREATE (:P {id:i})");
    run("CREATE (:P {id:3072} IN blue)");
    run("MATCH (n:P {id:1025}) SET n IN blue");
    // First group is uniform red; second uses runs; fourth is sparse blue.
    for (auto checkpoint = 0; checkpoint < 2; ++checkpoint) {
        if (checkpoint) {
            run("CHECKPOINT");
        }
        reopen();
        EXPECT_EQ(scalar("MATCH (n:P IN red) RETURN count(*)"), "2047");
        EXPECT_EQ(scalar("MATCH (n:P IN blue) RETURN count(*)"), "2");
        EXPECT_EQ(scalar("MATCH (n:P IN GLOBAL) RETURN count(*)"), "1024");
        EXPECT_EQ(scalar("MATCH (n:P {id:1025}) RETURN namespace(n)"), "blue");
    }
}

TEST_F(NamespacePersistenceTest, PreparedGlobalPlanRefreshesOnFirstNamespaceUse) {
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
    run("CREATE (:P {id:0})");
    auto prepared = connection->prepare("MATCH (n:P {id:$id}) RETURN namespace(n)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    auto result =
        connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{0}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_TRUE(result->getNext()->getValue(0)->isNull());
    run("CREATE NAMESPACE red");
    run("CREATE (:P {id:1} IN red)");
    result = connection->execute(prepared.get(), std::make_pair(std::string("id"), int64_t{1}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "red");
}

TEST_F(NamespacePersistenceTest, PlansBoundInOldSnapshotRefreshAfterSnapshotEnds) {
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
    run("CREATE (:P {id:0})");
    Connection reader(database.get());
    ASSERT_TRUE(reader.query("BEGIN TRANSACTION READ ONLY")->isSuccess());
    run("CREATE NAMESPACE red");
    run("CREATE (:P {id:1} IN red)");
    auto prepared = reader.prepare("MATCH (n:P {id:$id}) RETURN namespace(n)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    auto result = reader.execute(prepared.get(), std::make_pair(std::string("id"), int64_t{0}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_TRUE(result->getNext()->getValue(0)->isNull());
    ASSERT_TRUE(reader.query("COMMIT")->isSuccess());
    result = reader.execute(prepared.get(), std::make_pair(std::string("id"), int64_t{1}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "red");
}

TEST_F(NamespacePersistenceTest, CopyUsesNamespaceAndLocalKeyDomains) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE REL TABLE R(FROM L TO L, value INT64)");
    const auto nodesFile = directory / "nodes.csv";
    const auto edgesFile = directory / "edges.csv";
    {
        std::ofstream file(nodesFile);
        file << "id\n1\n2\n";
    }
    {
        std::ofstream file(edgesFile);
        file << "from,to,value\n1,2,7\n";
    }
    run("COPY L IN red FROM '" + nodesFile.generic_string() + "' (HEADER=true)");
    run("COPY L IN blue FROM '" + nodesFile.generic_string() + "' (HEADER=true)");
    run("COPY R IN blue FROM '" + edgesFile.generic_string() + "' (HEADER=true)");
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (a:L IN blue)-[r:R IN blue]->(b:L IN blue) RETURN r.value"), "7");
    EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R]->(b:L) RETURN count(*)"), "0");
    run("CHECKPOINT");
    reopen();
    EXPECT_EQ(scalar("MATCH (a:L IN blue)-[r:R IN blue]->(b:L IN blue) RETURN r.value"), "7");
}

TEST_F(NamespacePersistenceTest, NamedCopyRejectsExtensionIndexesBeforeWriting) {
    run("CREATE NAMESPACE red");
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
    const auto filePath = directory / "nodes.csv";
    {
        std::ofstream file(filePath);
        file << "id\n1\n";
    }
    run("BEGIN TRANSACTION");
    auto& context = *connection->getClientContext();
    auto* catalog = riag::catalog::Catalog::Get(context);
    auto* transaction = riag::transaction::Transaction::Get(context);
    const auto tableID = catalog->getTableCatalogEntry(transaction, "P")->getTableID();
    // Exercise the catalog boundary without downloading/loading an extension.
    catalog->createIndex(transaction,
        std::make_unique<riag::catalog::IndexCatalogEntry>("VECTOR", tableID, "extension_index",
            std::vector<riag::common::property_id_t>{0},
            std::make_unique<riag::catalog::BuiltinIndexAuxInfo>()));
    auto prepared =
        connection->prepare("COPY P IN red FROM '" + filePath.generic_string() + "' (HEADER=true)");
    ASSERT_FALSE(prepared->isSuccess());
    EXPECT_NE(prepared->getErrorMessage().find("Named namespace COPY is not supported"),
        std::string::npos)
        << prepared->getErrorMessage();
    // A preparation error rolls back the manual transaction and its mock metadata.
    EXPECT_EQ(scalar("MATCH (n:P) RETURN count(*)"), "0");
}

TEST_F(NamespacePersistenceTest, FailedLocalCopyRollsBackMembershipAndIndexEntries) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L(id STRING LOCAL PRIMARY KEY)");
    run("CREATE (:L {id:'existing'} IN red), (:L {id:'new'} IN blue)");
    const auto filePath = directory / "duplicate.csv";
    {
        std::ofstream file(filePath);
        file << "id\nnew\nnew\n";
    }
    fails("COPY L IN red FROM '" + filePath.generic_string() + "' (HEADER=true)");
    EXPECT_EQ(scalar("MATCH (n:L) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (n:L {id:'existing'} IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:L {id:'new'} IN blue) RETURN count(*)"), "1");
    run("CREATE (:L {id:'new'} IN red)");
    EXPECT_EQ(scalar("MATCH (n:L {id:'new'}) RETURN count(*)"), "2");
    run("CHECKPOINT");
    reopen();
    EXPECT_EQ(scalar("MATCH (n:L {id:'new'}) RETURN count(*)"), "2");
}

TEST_F(NamespacePersistenceTest, LogicalExportImportPreservesMembershipAndPolicies) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L IN red, blue (id INT64 LOCAL PRIMARY KEY, name STRING)");
    run("CREATE REL TABLE R(FROM L TO L, value INT64)");
    run("CREATE (:L {id:1, name:'r'} IN red), (:L {id:1, name:'b'} IN blue)");
    run("MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:R {value:7} IN GLOBAL]->(b)");
    for (const auto* format : {"csv", "parquet"}) {
        const auto exportPath = directory / (std::string("export_") + format);
        const auto relocatedPath = directory / (std::string("relocated export ") + format);
        run("EXPORT DATABASE '" + exportPath.generic_string() + "' (FORMAT='" + format + "')");
        std::filesystem::rename(exportPath, relocatedPath);
        connection.reset();
        database.reset();
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        database = std::make_unique<Database>(
            (directory / (std::string("imported_") + format)).string(), config);
        connection = std::make_unique<Connection>(database.get());
        run("IMPORT DATABASE '" + relocatedPath.generic_string() + "'");
        EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
        EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R IN GLOBAL]->(b:L IN blue) RETURN r.value"), "7");
        fails("CREATE (:L {id:2})");
        fails("CREATE (:L {id:1} IN red)");
    }
}

TEST_F(NamespacePersistenceTest, RelationshipCopyAndPendingCreatesKeepIndependentMemberships) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE REL TABLE R(FROM L TO L)");
    run("CREATE (:L {id:1} IN red), (:L {id:2} IN red), (:L {id:1} IN blue), (:L {id:2} IN blue)");
    const auto filePath = directory / "edges.csv";
    {
        std::ofstream file(filePath);
        file << "from,to\n1,2\n";
    }
    run("BEGIN TRANSACTION");
    run("MATCH (a:L {id:1} IN red), (b:L {id:2} IN red) CREATE (a)-[:R IN red]->(b)");
    run("COPY R IN blue FROM '" + filePath.generic_string() + "' (HEADER=true)");
    EXPECT_EQ(scalar("MATCH ()-[r:R IN red]->() RETURN count(*)"), "1");
    run("COMMIT");
    EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R IN red]->(b:L IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (a:L IN blue)-[r:R IN blue]->(b:L IN blue) RETURN count(*)"), "1");
    run("CHECKPOINT");
    reopen();
    EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R IN red]->(b:L IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (a:L IN blue)-[r:R IN blue]->(b:L IN blue) RETURN count(*)"), "1");
}

TEST_F(NamespaceMultiWriteTest, RenameRequiresTheOnlyActiveWriter) {
    Connection other(database.get());
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id:1} IN red)");
    auto rename = other.query("ALTER NAMESPACE red RENAME TO crimson");
    EXPECT_FALSE(rename->isSuccess());
    run("COMMIT");
    rename = other.query("ALTER NAMESPACE red RENAME TO crimson");
    ASSERT_TRUE(rename->isSuccess()) << rename->getErrorMessage();
    EXPECT_EQ(scalar("MATCH (n:P) RETURN namespace(n)"), "crimson");
}

TEST_F(NamespacePersistenceTest, RenameSurvivesWalReplayAndCheckpoint) {
    run("CREATE NAMESPACE red");
    run("CREATE NAMESPACE blue");
    run("CREATE NODE TABLE L IN red, blue (id INT64 LOCAL PRIMARY KEY)");
    run("CREATE REL TABLE E(FROM L TO L)");
    run("CREATE (:L {id:1} IN red), (:L {id:1} IN blue)");
    run("MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:E IN red]->(b)");
    const auto redID = scalar("CALL show_namespaces() WHERE name = 'red' RETURN id");
    run("ALTER NAMESPACE red RENAME TO crimson");
    run("CREATE NAMESPACE red");
    const auto newRedID = scalar("CALL show_namespaces() WHERE name = 'red' RETURN id");
    auto check = [&]() {
        EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'crimson' RETURN id"), redID);
        EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN id"), newRedID);
        EXPECT_EQ(scalar("MATCH (n:L IN crimson) RETURN count(*)"), "1");
        EXPECT_EQ(scalar("MATCH (n:L IN red) RETURN count(*)"), "0");
        EXPECT_EQ(scalar("MATCH (a:L)-[e:E]->(b:L) RETURN namespace(a) + '/' + namespace(e) + "
                         "'/' + namespace(b)"),
            "crimson/crimson/blue");
        EXPECT_EQ(scalar("CALL namespace_info('L') RETURN allowed_namespaces"),
            "`crimson`, `blue`");
        fails("CREATE (:L {id:1} IN crimson)");
    };
    check();
    // forceCheckpointOnClose is off: reopening replays the rename from the WAL.
    reopen();
    check();
    run("CHECKPOINT");
    reopen();
    check();
    // A rename only in the WAL after a checkpoint, and one rolled back.
    run("ALTER NAMESPACE crimson RENAME TO scarlet");
    run("BEGIN TRANSACTION");
    run("ALTER NAMESPACE blue RENAME TO navy");
    run("ROLLBACK");
    reopen();
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'scarlet' RETURN id"), redID);
    EXPECT_EQ(scalar("MATCH (a:L IN scarlet)-[e:E IN scarlet]->(b:L IN blue) RETURN count(*)"),
        "1");
    fails("MATCH (n:L IN navy) RETURN n", "Namespace navy does not exist.");
    // Identity allocation continues above every ID, renamed or not.
    run("CREATE NAMESPACE green");
    EXPECT_GT(std::stoull(scalar("CALL show_namespaces() WHERE name = 'green' RETURN id")),
        std::stoull(newRedID));
}

TEST_F(NamespacePersistenceTest, RenameInAGraphCatalogReopens) {
    run("CREATE GRAPH alpha");
    run("USE GRAPH alpha");
    run("CREATE NAMESPACE red");
    run("CREATE NODE TABLE P(id INT64 LOCAL PRIMARY KEY)");
    run("CREATE (:P {id:1} IN red), (:P {id:1})");
    run("ALTER NAMESPACE red RENAME TO crimson");
    run("USE GRAPH main");
    for (auto checkpoint = 0; checkpoint < 2; ++checkpoint) {
        if (checkpoint) {
            run("USE GRAPH alpha");
            run("CHECKPOINT");
            run("USE GRAPH main");
        }
        reopen();
        run("USE GRAPH alpha");
        EXPECT_EQ(scalar("MATCH (n:P {id:1} IN crimson) RETURN namespace(n)"), "crimson");
        fails("MATCH (n:P IN red) RETURN n", "Namespace red does not exist.");
        EXPECT_EQ(scalar("MATCH (n:P {id:1}) RETURN count(*)"), "2");
        run("USE GRAPH main");
    }
}

TEST_F(NamespacePersistenceTest, RenameIsRejectedInAReadOnlyDatabase) {
    run("CREATE NAMESPACE red");
    run("CHECKPOINT");
    connection.reset();
    database.reset();
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.readOnly = true;
    database = std::make_unique<Database>((directory / "database").string(), config);
    connection = std::make_unique<Connection>(database.get());
    fails("ALTER NAMESPACE red RENAME TO crimson");
    EXPECT_EQ(scalar("CALL show_namespaces() WHERE name = 'red' RETURN count(*)"), "1");
}

// The rename changes one catalog entry. Its WAL footprint is the same whether the namespace
// has no members or many: no membership group, table policy, row, column or index is written,
// and the data file is untouched.
TEST_F(NamespacePersistenceTest, RenameCostIsIndependentOfMemberCount) {
    constexpr auto members = 100000;
    run("CREATE NAMESPACE big");
    run("CREATE NAMESPACE nil");
    run("CREATE NAMESPACE pad");
    run("CREATE NODE TABLE L IN GLOBAL, big, nil (id INT64 LOCAL PRIMARY KEY, v INT64)");
    run("CREATE REL TABLE E IN big (FROM L TO L)");
    // Pairs of member nodes joined by a member edge, without a MATCH: members / 2 edges.
    for (auto start = 1; start <= members / 2; start += 10000) {
        const auto end = std::min(start + 9999, members / 2);
        run("UNWIND range(" + std::to_string(start) + ", " + std::to_string(end) +
            ") AS i CREATE (:L {id:i, v:i} IN big)-[:E IN big]->(:L {id:i + " +
            std::to_string(members / 2) + ", v:i + " + std::to_string(members / 2) + "} IN big)");
    }
    run("CHECKPOINT");
    const auto walPath = directory / "database.wal";
    const auto dataPath = directory / "database";
    auto size = [](const std::filesystem::path& path) -> uintmax_t {
        return std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
    };
    const auto dataSize = size(dataPath);
    auto walGrowth = [&](const std::string& query, double& milliseconds) {
        const auto before = size(walPath);
        const auto start = std::chrono::steady_clock::now();
        run(query);
        milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        return size(walPath) - before;
    };
    double emptyTime = 0;
    double membersTime = 0;
    // The first record after a checkpoint also writes the WAL header.
    walGrowth("ALTER NAMESPACE pad RENAME TO wad", emptyTime);
    // Equal name lengths, so both catalog entries serialize to the same number of bytes.
    const auto emptyGrowth = walGrowth("ALTER NAMESPACE nil RENAME TO zip", emptyTime);
    const auto membersGrowth = walGrowth("ALTER NAMESPACE big RENAME TO top", membersTime);
    EXPECT_GT(emptyGrowth, 0u);
    EXPECT_EQ(membersGrowth, emptyGrowth);
    EXPECT_LT(membersGrowth, 4096u);
    EXPECT_EQ(size(dataPath), dataSize);
    std::cout << "[ RENAME   ] WAL bytes: empty=" << emptyGrowth << " members=" << membersGrowth
              << "; ms: empty=" << emptyTime << " members=" << membersTime << std::endl;
    EXPECT_EQ(scalar("MATCH (n:L IN top) RETURN count(*)"), std::to_string(members));
    EXPECT_EQ(scalar("MATCH (:L IN top)-[e:E IN top]->(:L IN top) RETURN count(*)"),
        std::to_string(members / 2));
    EXPECT_EQ(scalar("MATCH (n:L {id:" + std::to_string(members) + "} IN top) RETURN n.v"),
        std::to_string(members));
    fails("CREATE (:L {id:1} IN top)");
    reopen();
    EXPECT_EQ(scalar("MATCH (n:L IN top) RETURN count(*)"), std::to_string(members));
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN namespace(n)"), "top");
}

#if GTEST_HAS_DEATH_TEST
TEST_F(NamespacePersistenceTest, CommittedWalRecoversWithoutCheckpoint) {
    connection.reset();
    database.reset();
    const auto path = (directory / "wal_database").string();
    EXPECT_EXIT(([&]() {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        config.autoCheckpoint = false;
        config.forceCheckpointOnClose = false;
        Database childDatabase(path, config);
        Connection child(&childDatabase);
        for (const auto* statement :
            {"CREATE NAMESPACE red", "CREATE NAMESPACE blue",
                "CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)",
                "CREATE REL TABLE R(FROM L TO L)", "CREATE (:L {id:1} IN red), (:L {id:1} IN blue)",
                "MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:R IN red]->(b)",
                "MATCH ()-[r:R]->() SET r IN blue"}) {
            auto result = child.query(statement);
            if (!result->isSuccess()) {
                std::cerr << result->getErrorMessage();
                std::_Exit(2);
            }
        }
        std::_Exit(0);
    }()),
        ::testing::ExitedWithCode(0), "");
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.throwOnWalReplayFailure = true;
    database = std::make_unique<Database>(path, config);
    connection = std::make_unique<Connection>(database.get());
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (a:L IN red)-[r:R IN blue]->(b:L IN blue) RETURN count(*)"), "1");
    fails("CREATE (:L {id:1} IN red)");
}

TEST_F(NamespacePersistenceTest, RenameRecoversFromWalAfterCrash) {
    connection.reset();
    database.reset();
    const auto path = (directory / "wal_rename_database").string();
    EXPECT_EXIT(([&]() {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        config.autoCheckpoint = false;
        config.forceCheckpointOnClose = false;
        Database childDatabase(path, config);
        Connection child(&childDatabase);
        for (const auto* statement : {"CREATE NAMESPACE red", "CREATE NAMESPACE gone",
                 "CREATE NODE TABLE L IN red (id INT64 LOCAL PRIMARY KEY)",
                 "CREATE (:L {id:1} IN red)", "CHECKPOINT", "DROP NAMESPACE gone",
                 "ALTER NAMESPACE red RENAME TO gone", "CREATE NAMESPACE red",
                 "ALTER TABLE L ADD TO NAMESPACE red", "CREATE (:L {id:1} IN red)",
                 "BEGIN TRANSACTION", "ALTER NAMESPACE red RENAME TO pending"}) {
            auto result = child.query(statement);
            if (!result->isSuccess()) {
                std::cerr << statement << ": " << result->getErrorMessage();
                std::_Exit(2);
            }
        }
        // Exit without committing the last rename, closing, or checkpointing.
        std::_Exit(0);
    }()),
        ::testing::ExitedWithCode(0), "");
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.throwOnWalReplayFailure = true;
    database = std::make_unique<Database>(path, config);
    connection = std::make_unique<Connection>(database.get());
    EXPECT_EQ(scalar("CALL show_namespaces() RETURN count(*)"), "3");
    fails("MATCH (n:L IN pending) RETURN n", "Namespace pending does not exist.");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN gone) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:L {id:1} IN red) RETURN count(*)"), "1");
    EXPECT_EQ(scalar("MATCH (n:L {id:1}) RETURN count(*)"), "2");
    // The recovered table policy keeps both identities under their current names.
    EXPECT_EQ(scalar("CALL namespace_info('L') RETURN allowed_namespaces"), "`gone`, `red`");
    fails("CREATE (:L {id:1} IN gone)");
}
#endif

#ifndef __SINGLE_THREADED__
// Namespaces are optional, so DDL on another connection must not make reads fail, whether they
// name a namespace or not. A plan bound before an ALTER commits is rebound by the engine when the
// statement starts, not handed back to the caller as a retry error. That holds for ad-hoc queries,
// which bind in the transaction they run in, and for prepared statements, whose plan is rebound in
// it: either way the rebound plan is current, however often the ALTERs land.
TEST_F(NamespaceExecutionTest, ReadsRebindWhileAnotherConnectionAltersTheTable) {
    run("CREATE (:P {id:1} IN red), (:P {id:2} IN blue), (:P {id:3})");
    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};
    std::atomic<uint64_t> reads{0};
    std::atomic<uint64_t> running{0};
    std::mutex failureMtx;
    std::string failure;
    auto reader = [&](std::string query, std::string expected, bool prepare) {
        Connection readConnection(database.get());
        auto prepared = prepare ? readConnection.prepare(query) : nullptr;
        for (auto first = true; !stop.load(); first = false) {
            auto result =
                prepare ? readConnection.execute(prepared.get()) : readConnection.query(query);
            std::string error;
            if (!result->isSuccess()) {
                error = query + ": " + result->getErrorMessage();
            } else if (auto value = result->getNext()->getValue(0)->toString(); value != expected) {
                error = query + ": read " + value;
            }
            if (!error.empty()) {
                std::lock_guard guard(failureMtx);
                failure = failure.empty() ? error : failure;
                failed = true;
                return;
            }
            reads++;
            if (first) {
                running++;
            }
        }
    };
    std::vector<std::thread> readers;
    for (auto prepare : {false, true}) {
        readers.emplace_back(reader, "MATCH (n:P) RETURN count(*)", "3", prepare);
        readers.emplace_back(reader, "MATCH (n:P IN red) RETURN count(*)", "1", prepare);
    }
    // The ALTERs start once every reader has finished a read, so they overlap the reads however
    // the threads are scheduled; on a loaded machine the writer could otherwise run all of them
    // before a reader starts.
    while (running.load() < readers.size() && !failed.load()) {
        std::this_thread::yield();
    }
    for (auto round = 0; round < 50 && !failed.load(); round++) {
        run("ALTER TABLE P ADD q" + std::to_string(round) + " INT64");
    }
    stop = true;
    for (auto& thread : readers) {
        thread.join();
    }
    EXPECT_GE(reads.load(), readers.size());
    std::lock_guard guard(failureMtx);
    EXPECT_TRUE(failure.empty()) << failure;
}
#endif
} // namespace
