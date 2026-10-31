#include "c_api_test/c_api_test.h"

using namespace riag::main;
using namespace riag::testing;

class CApiPreparedStatementTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
};

TEST_F(CApiPreparedStatementTest, IsSuccess) {
    riag_prepared_statement preparedStatement;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(preparedStatement._prepared_statement, nullptr);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    riag_prepared_statement_destroy(&preparedStatement);

    query = "MATCH (a:personnnn) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(preparedStatement._prepared_statement, nullptr);
    ASSERT_FALSE(riag_prepared_statement_is_success(&preparedStatement));
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, IsReadOnly) {
    riag_prepared_statement preparedStatement;
    auto connection = getConnection();

    auto state = riag_connection_prepare(connection, "RETURN 1", &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    ASSERT_TRUE(riag_prepared_statement_is_read_only(&preparedStatement));
    riag_prepared_statement_destroy(&preparedStatement);

    state = riag_connection_prepare(connection,
        "CREATE NODE TABLE Person(name STRING, PRIMARY KEY(name))", &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    ASSERT_FALSE(riag_prepared_statement_is_read_only(&preparedStatement));
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, GetErrorMessage) {
    riag_prepared_statement preparedStatement;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(preparedStatement._prepared_statement, nullptr);
    ASSERT_EQ(riag_prepared_statement_get_error_message(&preparedStatement), nullptr);
    riag_prepared_statement_destroy(&preparedStatement);

    query = "MATCH (a:personnnn) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(preparedStatement._prepared_statement, nullptr);
    char* message = riag_prepared_statement_get_error_message(&preparedStatement);
    ASSERT_EQ(std::string(message), "Binder exception: Table personnnn does not exist.");
    riag_prepared_statement_destroy(&preparedStatement);
    riag_destroy_string(message);
}

TEST_F(CApiPreparedStatementTest, BindBool) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.isStudent = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_bool(&preparedStatement, "1", true), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 3);
    riag_query_result_destroy(&result);
    // Bind a different parameter
    ASSERT_EQ(riag_prepared_statement_bind_bool(&preparedStatement, "1", false), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    resultCpp = static_cast<QueryResult*>(result._query_result);
    tuple = resultCpp->getNext();
    value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 5);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindInt64) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.age > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_int64(&preparedStatement, "1", 30), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 4);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindInt32) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:movies) WHERE a.length > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_int32(&preparedStatement, "1", 200), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindInt16) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.length > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_int16(&preparedStatement, "1", 10), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindInt8) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.level > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_int8(&preparedStatement, "1", 3), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindUInt64) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.code > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(riag_prepared_statement_bind_uint64(&preparedStatement, "1", 100), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindUInt32) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.temperature> $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_uint32(&preparedStatement, "1", 10), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindUInt16) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.ulength> $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_uint16(&preparedStatement, "1", 100), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindUInt8) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query =
        "MATCH (a:person) -[s:studyAt]-> (b:organisation) WHERE s.ulevel> $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_uint8(&preparedStatement, "1", 14), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 2);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindDouble) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.eyeSight > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_double(&preparedStatement, "1", 4.5), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 7);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindFloat) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.height < $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_float(&preparedStatement, "1", 1.0), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 1);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindString) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.fName = $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    ASSERT_EQ(riag_prepared_statement_bind_string(&preparedStatement, "1", "Alice"), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 1);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindDate) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.birthdate > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    auto date = riag_date_t{0};
    ASSERT_EQ(riag_prepared_statement_bind_date(&preparedStatement, "1", date), RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 4);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindTimestamp) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.registerTime > $1 and cast(a.registerTime, "
                 "\"timestamp_ns\") > $2 and cast(a.registerTime, \"timestamp_ms\") > "
                 "$3 and cast(a.registerTime, \"timestamp_sec\") > $4 and cast(a.registerTime, "
                 "\"timestamp_tz\") > $5 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    auto timestamp = riag_timestamp_t{0};
    auto timestamp_ns = riag_timestamp_ns_t{1};
    auto timestamp_ms = riag_timestamp_ms_t{2};
    auto timestamp_sec = riag_timestamp_sec_t{3};
    auto timestamp_tz = riag_timestamp_tz_t{4};
    ASSERT_EQ(riag_prepared_statement_bind_timestamp(&preparedStatement, "1", timestamp),
        RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_timestamp_ns(&preparedStatement, "2", timestamp_ns),
        RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_timestamp_ms(&preparedStatement, "3", timestamp_ms),
        RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_timestamp_sec(&preparedStatement, "4", timestamp_sec),
        RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_timestamp_tz(&preparedStatement, "5", timestamp_tz),
        RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 7);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindInteval) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.lastJobDuration > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    auto interval = riag_interval_t{0, 0, 0};
    ASSERT_EQ(riag_prepared_statement_bind_interval(&preparedStatement, "1", interval),
        RiaGSuccess);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 8);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindValue) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "MATCH (a:person) WHERE a.registerTime > $1 RETURN COUNT(*)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    auto timestamp = riag_timestamp_t{0};
    auto timestampValue = riag_value_create_timestamp(timestamp);
    ASSERT_EQ(riag_prepared_statement_bind_value(&preparedStatement, "1", timestampValue),
        RiaGSuccess);
    riag_value_destroy(timestampValue);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 7);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}

TEST_F(CApiPreparedStatementTest, BindBlobValue) {
    riag_prepared_statement preparedStatement;
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    auto query = "RETURN octet_length($1)";
    state = riag_connection_prepare(connection, query, &preparedStatement);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_prepared_statement_is_success(&preparedStatement));
    const uint8_t blob_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    auto blobValue = riag_value_create_blob(blob_data, sizeof(blob_data));
    ASSERT_EQ(riag_prepared_statement_bind_value(&preparedStatement, "1", blobValue), RiaGSuccess);
    riag_value_destroy(blobValue);
    state = riag_connection_execute(connection, &preparedStatement, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(result._query_result, nullptr);
    ASSERT_EQ(riag_query_result_get_num_tuples(&result), 1);
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 1);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    auto resultCpp = static_cast<QueryResult*>(result._query_result);
    auto tuple = resultCpp->getNext();
    auto value = tuple->getValue(0)->getValue<int64_t>();
    ASSERT_EQ(value, 4);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&preparedStatement);
}
