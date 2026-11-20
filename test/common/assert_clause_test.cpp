#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;

// The Cypher ASSERT clause (docs/assert.md) with what the e2e .test format cannot express:
// parameters, prepared statements run repeatedly, and several worker threads.
namespace {
class AssertClauseTest : public ::testing::Test {
protected:
    void SetUp() override {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 4;
        database = std::make_unique<Database>("", config);
        connection = std::make_unique<Connection>(database.get());
        run("CREATE NODE TABLE P(id INT64 PRIMARY KEY, v INT64)");
        run("UNWIND range(1, 5000) AS i CREATE (:P {id: i, v: i % 7})");
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
        return result->getNext()->getValue(0)->toString();
    }
    // Runs the prepared statement with $v bound and returns the error message, empty on success.
    std::string execute(PreparedStatement* prepared, int64_t v) {
        auto result = connection->execute(prepared, std::make_pair(std::string("v"), v));
        return result->isSuccess() ? "" : result->getErrorMessage();
    }
    std::unique_ptr<Database> database;
    std::unique_ptr<Connection> connection;
};

TEST_F(AssertClauseTest, StatementLevelParameterPrecondition) {
    auto prepared = connection->prepare("ASSERT $v > 0 AS 'Parameter v shall be greater than 0'");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    EXPECT_EQ(execute(prepared.get(), 1), "");
    EXPECT_EQ(execute(prepared.get(), 0), "Assertion failed: Parameter v shall be greater than 0");
    // The cached plan keeps working after a failed execution.
    EXPECT_EQ(execute(prepared.get(), 7), "");
    EXPECT_EQ(execute(prepared.get(), -1), "Assertion failed: Parameter v shall be greater than 0");
}

TEST_F(AssertClauseTest, ParameterInsideThePipeline) {
    auto prepared = connection->prepare("MATCH (p:P) ASSERT p.v < $v RETURN count(*)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    auto result = connection->execute(prepared.get(), std::make_pair(std::string("v"), int64_t{7}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "5000");
    EXPECT_EQ(execute(prepared.get(), 6), "Assertion failed: p.v < $v");
}

TEST_F(AssertClauseTest, ParameterInAnAggregatingAssert) {
    auto prepared =
        connection->prepare("MATCH (p:P) ASSERT count(*) >= $v AS 'too few' RETURN count(*)");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    auto result =
        connection->execute(prepared.get(), std::make_pair(std::string("v"), int64_t{5000}));
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "5000");
    EXPECT_EQ(execute(prepared.get(), 5001), "Assertion failed: too few");
}

TEST_F(AssertClauseTest, DefaultMessageIsTheConditionAsWritten) {
    auto result = connection->query("MATCH (p:P)  ASSERT  p.v  <  6 RETURN count(*)");
    ASSERT_FALSE(result->isSuccess());
    EXPECT_EQ(result->getErrorMessage(), "Assertion failed: p.v  <  6");
}

TEST_F(AssertClauseTest, NullParameterFails) {
    auto prepared = connection->prepare("ASSERT $v > 0");
    ASSERT_TRUE(prepared->isSuccess()) << prepared->getErrorMessage();
    auto result = connection->execute(prepared.get(),
        std::make_pair(std::string("v"), riag::common::Value::createNullValue()));
    ASSERT_FALSE(result->isSuccess());
    EXPECT_EQ(result->getErrorMessage(), "Assertion failed: $v > 0");
}

// Many morsels on several threads: every row is checked, whichever thread meets the failing one,
// and the error is the same each time.
TEST_F(AssertClauseTest, EveryRowIsCheckedInParallel) {
    for (auto i = 0; i < 5; ++i) {
        auto result = connection->query("MATCH (p:P) ASSERT p.id <> 4321 AS 'row 4321' RETURN p");
        ASSERT_FALSE(result->isSuccess());
        EXPECT_EQ(result->getErrorMessage(), "Assertion failed: row 4321");
    }
    EXPECT_EQ(scalar("MATCH (p:P) ASSERT p.id <> 5001 RETURN count(*)"), "5000");
}

// A LIMIT above the clause must not stop its input early: the outcome may not depend on which
// rows a thread happens to produce first.
TEST_F(AssertClauseTest, LimitDoesNotCutTheCheckShort) {
    for (auto i = 0; i < 5; ++i) {
        auto result = connection->query("MATCH (p:P) ASSERT p.id <> 4999 RETURN p.id LIMIT 1");
        ASSERT_FALSE(result->isSuccess());
        EXPECT_EQ(result->getErrorMessage(), "Assertion failed: p.id <> 4999");
    }
}

// A failed ASSERT in an explicit transaction rolls it back, like every other runtime error.
TEST_F(AssertClauseTest, FailureRollsBackTheTransaction) {
    run("BEGIN TRANSACTION");
    run("CREATE (:P {id: 6000, v: 0})");
    auto result = connection->query("MATCH (p:P {id: 6000}) ASSERT p.v > 0 AS 'post-condition'");
    ASSERT_FALSE(result->isSuccess());
    EXPECT_EQ(result->getErrorMessage(), "Assertion failed: post-condition");
    result = connection->query("COMMIT");
    ASSERT_FALSE(result->isSuccess());
    EXPECT_EQ(result->getErrorMessage(), "No active transaction for COMMIT.");
    EXPECT_EQ(scalar("MATCH (p:P) RETURN count(*)"), "5000");
}

} // namespace
