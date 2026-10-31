#include <thread>

#include "c_api_test/c_api_test.h"

using ::testing::Test;
using namespace riag::main;
using namespace riag::testing;

class CApiConnectionTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
};

TEST_F(CApiConnectionTest, CreationAndDestroy) {
    riag_connection connection;
    riag_state state;
    auto _database = getDatabase();
    state = riag_connection_init(_database, &connection);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(connection._connection, nullptr);
    auto connectionCpp = static_cast<Connection*>(connection._connection);
    ASSERT_NE(connectionCpp, nullptr);
    riag_connection_destroy(&connection);
}

TEST_F(CApiConnectionTest, CreationAndDestroyWithNullDatabase) {
    riag_connection connection;
    riag_state state;
    state = riag_connection_init(nullptr, &connection);
    ASSERT_EQ(state, RiaGError);
}

TEST_F(CApiConnectionTest, Query) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) RETURN a.fName";
    state = riag_connection_query(connection, query, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    ASSERT_NE(resultCpp, nullptr);
    ASSERT_TRUE(resultCpp->isSuccess());
    ASSERT_TRUE(resultCpp->hasNext());
    ASSERT_EQ(resultCpp->getErrorMessage(), "");
    ASSERT_EQ(resultCpp->getNumTuples(), 8);
    ASSERT_EQ(resultCpp->getNumColumns(), 1);
    ASSERT_EQ(resultCpp->getColumnNames()[0], "a.fName");
    riag_query_result_destroy(&result);
}

TEST_F(CApiConnectionTest, SetGetMaxNumThreadForExec) {
    uint64_t maxNumThreadForExec;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_set_max_num_thread_for_exec(connection, 4);
    ASSERT_EQ(state, RiaGSuccess);
    auto connectionCpp = static_cast<Connection*>(connection->_connection);
    ASSERT_EQ(connectionCpp->getMaxNumThreadForExec(), 4);
    state = riag_connection_get_max_num_thread_for_exec(connection, &maxNumThreadForExec);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(maxNumThreadForExec, 4);
    state = riag_connection_set_max_num_thread_for_exec(connection, 8);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(connectionCpp->getMaxNumThreadForExec(), 8);
    state = riag_connection_get_max_num_thread_for_exec(connection, &maxNumThreadForExec);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(maxNumThreadForExec, 8);
    riag_connection badConnection;
    ASSERT_EQ(riag_connection_init(nullptr, &badConnection), RiaGError);
    state = riag_connection_set_max_num_thread_for_exec(&badConnection, 4);
    ASSERT_EQ(state, RiaGError);
    state = riag_connection_get_max_num_thread_for_exec(&badConnection, &maxNumThreadForExec);
    ASSERT_EQ(state, RiaGError);
}

TEST_F(CApiConnectionTest, Prepare) {
    riag_prepared_statement statement;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &statement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(statement._prepared_statement, nullptr);
    ASSERT_NE(statement._bound_values, nullptr);
    auto statementCpp = static_cast<PreparedStatement*>(statement._prepared_statement);
    ASSERT_NE(statementCpp, nullptr);
    auto connectionCpp = static_cast<Connection*>(connection->_connection);
    auto result = connectionCpp->execute(statementCpp, std::make_pair(std::string("1"), true));
    ASSERT_TRUE(result->isSuccess());
    ASSERT_TRUE(result->hasNext());
    ASSERT_EQ(result->getErrorMessage(), "");
    ASSERT_EQ(result->getNumTuples(), 1);
    auto tuple = result->getNext();
    ASSERT_EQ(tuple->getValue(0)->getValue<int64_t>(), 3);
    riag_prepared_statement_destroy(&statement);
}

TEST_F(CApiConnectionTest, Execute) {
    riag_prepared_statement statement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &statement);
    ASSERT_EQ(state, RiaGSuccess);
    riag_prepared_statement_bind_bool(&statement, "1", true);
    state = riag_connection_execute(connection, &statement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    ASSERT_NE(resultCpp, nullptr);
    ASSERT_TRUE(resultCpp->isSuccess());
    ASSERT_TRUE(resultCpp->hasNext());
    ASSERT_EQ(resultCpp->getErrorMessage(), "");
    ASSERT_EQ(resultCpp->getNumTuples(), 1);
    auto tuple = resultCpp->getNext();
    ASSERT_EQ(tuple->getValue(0)->getValue<int64_t>(), 3);
    riag_prepared_statement_destroy(&statement);
    riag_query_result_destroy(&result);
}

TEST_F(CApiConnectionTest, ExecuteError) {
    riag_prepared_statement preparedStatement;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(preparedStatement._prepared_statement, nullptr);
    ASSERT_EQ(riag_prepared_statement_bind_int64(&preparedStatement, "1", 30), RiaGSuccess);
    riag_query_result result;
    ASSERT_EQ(riag_connection_execute(connection, &preparedStatement, &result), RiaGError);
    ASSERT_FALSE(riag_query_result_is_success(&result));
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiConnectionTest, QueryTimeout) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    ASSERT_EQ(riag_connection_set_query_timeout(connection, 1), RiaGSuccess);
    state = riag_connection_query(connection,
        "UNWIND RANGE(1,100000) AS x UNWIND RANGE(1, 100000) AS y RETURN COUNT(x + y);", &result);
    ASSERT_EQ(state, RiaGError);
    ASSERT_NE(result._query_result, nullptr);
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    ASSERT_NE(resultCpp, nullptr);
    ASSERT_FALSE(resultCpp->isSuccess());
    ASSERT_EQ(resultCpp->getErrorMessage(), "Interrupted.");
    riag_query_result_destroy(&result);
    riag_connection badConnection;
    ASSERT_EQ(riag_connection_init(nullptr, &badConnection), RiaGError);
    ASSERT_EQ(riag_connection_set_query_timeout(&badConnection, 1), RiaGError);
}

#ifndef __SINGLE_THREADED__
// The following test is disabled in single-threaded mode because it requires
// a separate thread to run.
TEST_F(CApiConnectionTest, Interrupt) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    std::atomic<bool> finished = false;

    // Interrupt the query after 100ms
    // This may happen too early, so try again until the query function finishes.
    std::thread t([&connection, &finished]() {
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            riag_connection_interrupt(connection);
        } while (!finished);
    });
    state = riag_connection_query(connection,
        "UNWIND RANGE(1,100000) AS x UNWIND RANGE(1, 100000) AS y RETURN COUNT(x + y);", &result);
    finished = true;
    ASSERT_EQ(state, RiaGError);
    ASSERT_NE(result._query_result, nullptr);
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    ASSERT_NE(resultCpp, nullptr);
    ASSERT_FALSE(resultCpp->isSuccess());
    ASSERT_EQ(resultCpp->getErrorMessage(), "Interrupted.");
    riag_query_result_destroy(&result);
    t.join();
}
#endif

TEST_F(CApiConnectionTest, GetPushedSqlNoPushdown) {
    // For a simple RETURN query with no foreign tables, no pushdown SQL should be found.
    // The function should succeed but return NULL in out_sql.
    char* sql = (char*)0xDEADBEEF; // sentinel to ensure it gets set
    riag_state state;
    auto connection = getConnection();

    // Test with a simple RETURN query (no pushdown possible)
    state = riag_connection_get_pushed_sql(connection, "RETURN 1", &sql);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(sql, nullptr) << "No pushdown SQL expected for RETURN 1";

    // Test with a MATCH query on native tables (no foreign pushdown)
    state = riag_connection_get_pushed_sql(connection, "MATCH (a:person) RETURN a.fName", &sql);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(sql, nullptr) << "No pushdown SQL expected for native MATCH";

    // Test error: null connection
    sql = (char*)0xDEADBEEF;
    state = riag_connection_get_pushed_sql(nullptr, "RETURN 1", &sql);
    ASSERT_EQ(state, RiaGError);
}

TEST_F(CApiConnectionTest, GetPushedSqlSyntaxError) {
    // A query with a syntax error should return an error
    char* sql = (char*)0xDEADBEEF;
    riag_state state;
    auto connection = getConnection();

    state = riag_connection_get_pushed_sql(connection, "THIS IS NOT VALID CYPHER", &sql);
    ASSERT_EQ(state, RiaGError);
    // sql should be NULL on error
    ASSERT_EQ(sql, nullptr);

    // Error message should be retrievable
    char* err = riag_get_last_error();
    ASSERT_NE(err, nullptr);
    // All riag exceptions are prefixed with "<Type> exception: " (e.g. "Parser exception: ").
    // Check for "exception" so the assertion is robust across exception types.
    ASSERT_NE(std::string(err).find("exception"), std::string::npos) << err;
    riag_destroy_string(err);
}

TEST_F(CApiConnectionTest, GetLastErrorQueryFailure) {
    // A query that returns a failed QueryResult should set the last error message
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();

    state = riag_connection_query(connection, "MATCH (a:NoSuchTable) RETURN a", &result);
    ASSERT_EQ(state, RiaGError);

    // Error message should be retrievable
    char* err = riag_get_last_error();
    ASSERT_NE(err, nullptr);
    ASSERT_NE(std::string(err).find("exception"), std::string::npos) << err;
    riag_destroy_string(err);
}

TEST_F(CApiConnectionTest, GetLastErrorExecuteFailure) {
    // A prepared statement that fails on execution should set the last error message
    riag_query_result result;
    riag_prepared_statement prepared;
    riag_state state;
    auto connection = getConnection();

    state = riag_connection_prepare(connection, "MATCH (a:NoSuchTable) RETURN a", &prepared);
    ASSERT_EQ(state, RiaGSuccess) << "Prepare should succeed (syntax is valid)";

    state = riag_connection_execute(connection, &prepared, &result);
    ASSERT_EQ(state, RiaGError);

    // Error message should be retrievable
    char* err = riag_get_last_error();
    ASSERT_NE(err, nullptr);
    ASSERT_NE(std::string(err).find("exception"), std::string::npos) << err;
    riag_destroy_string(err);

    riag_prepared_statement_destroy(&prepared);
}
