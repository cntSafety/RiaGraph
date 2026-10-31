#include <fstream>

#include "c_api_test/c_api_test.h"

using namespace riag::main;
using namespace riag::common;
using namespace riag::processor;
using namespace riag::testing;

class CApiQueryResultTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
};

static riag_value* copy_flat_tuple(riag_flat_tuple* tuple, uint32_t tupleLen) {
    riag_value* ret = (riag_value*)malloc(sizeof(riag_value) * tupleLen);
    for (uint32_t i = 0; i < tupleLen; i++) {
        riag_flat_tuple_get_value(tuple, i, &ret[i]);
    }
    return ret;
}

TEST_F(CApiQueryResultTest, GetNextExample) {
    auto conn = getConnection();

    riag_query_result result;
    riag_connection_query(conn, "MATCH (p:person) RETURN p.*", &result);

    uint64_t num_tuples = riag_query_result_get_num_tuples(&result);
    riag_value** tuples = (riag_value**)malloc(sizeof(riag_value*) * num_tuples);
    for (uint64_t i = 0; i < num_tuples; ++i) {
        riag_flat_tuple tuple;
        riag_query_result_get_next(&result, &tuple);
        tuples[i] = copy_flat_tuple(&tuple, riag_query_result_get_num_columns(&result));
        riag_flat_tuple_destroy(&tuple);
    }

    for (uint64_t i = 0; i < num_tuples; ++i) {
        for (uint64_t j = 0; j < riag_query_result_get_num_columns(&result); ++j) {
            ASSERT_FALSE(riag_value_is_null(&tuples[i][j]));
            riag_value_destroy(&tuples[i][j]);
        }
        free(tuples[i]);
    }

    free((void*)tuples);

    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetErrorMessage) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN COUNT(*)", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    char* errorMessage = riag_query_result_get_error_message(&result);
    riag_query_result_destroy(&result);

    state = riag_connection_query(connection, "MATCH (a:personnnn) RETURN COUNT(*)", &result);
    ASSERT_EQ(state, RiaGError);
    ASSERT_FALSE(riag_query_result_is_success(&result));
    errorMessage = riag_query_result_get_error_message(&result);
    ASSERT_EQ(std::string(errorMessage), "Binder exception: Table personnnn does not exist.");
    riag_query_result_destroy(&result);
    riag_destroy_string(errorMessage);
}

TEST_F(CApiQueryResultTest, ToString) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN COUNT(*)", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    char* str_repr = riag_query_result_to_string(&result);
    ASSERT_EQ(state, RiaGSuccess);
    riag_destroy_string(str_repr);
    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetNumColumns) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN a.fName, a.age, a.height",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_EQ(riag_query_result_get_num_columns(&result), 3);
    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetColumnName) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN a.fName, a.age, a.height",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    char* columnName;
    ASSERT_EQ(riag_query_result_get_column_name(&result, 0, &columnName), RiaGSuccess);
    ASSERT_EQ(std::string(columnName), "a.fName");
    riag_destroy_string(columnName);
    ASSERT_EQ(riag_query_result_get_column_name(&result, 1, &columnName), RiaGSuccess);
    ASSERT_EQ(std::string(columnName), "a.age");
    riag_destroy_string(columnName);
    ASSERT_EQ(riag_query_result_get_column_name(&result, 2, &columnName), RiaGSuccess);
    ASSERT_EQ(std::string(columnName), "a.height");
    riag_destroy_string(columnName);
    ASSERT_EQ(riag_query_result_get_column_name(&result, 222, &columnName), RiaGError);
    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetColumnDataType) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN a.fName, a.age, a.height",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    riag_logical_type type;
    ASSERT_EQ(riag_query_result_get_column_data_type(&result, 0, &type), RiaGSuccess);
    auto typeCpp = (LogicalType*)(type._data_type);
    ASSERT_EQ(typeCpp->getLogicalTypeID(), LogicalTypeID::STRING);
    riag_data_type_destroy(&type);
    ASSERT_EQ(riag_query_result_get_column_data_type(&result, 1, &type), RiaGSuccess);
    typeCpp = (LogicalType*)(type._data_type);
    ASSERT_EQ(typeCpp->getLogicalTypeID(), LogicalTypeID::INT64);
    riag_data_type_destroy(&type);
    ASSERT_EQ(riag_query_result_get_column_data_type(&result, 2, &type), RiaGSuccess);
    typeCpp = (LogicalType*)(type._data_type);
    ASSERT_EQ(typeCpp->getLogicalTypeID(), LogicalTypeID::FLOAT);
    riag_data_type_destroy(&type);
    ASSERT_EQ(riag_query_result_get_column_data_type(&result, 222, &type), RiaGError);
    riag_query_result_destroy(&result);
}

// TODO(Guodong): Fix this test by adding support of STRUCT in arrow table export.
// TEST_F(CApiQueryResultTest, GetArrowSchema) {
//    auto connection = getConnection();
//    auto result = riag_connection_query(
//        connection, "MATCH (p:person)-[k:knows]-(q:person) RETURN p.fName, k, q.fName");
//    ASSERT_TRUE(riag_query_result_is_success(result));
//    auto schema = riag_query_result_get_arrow_schema(result);
//    ASSERT_STREQ(schema.name, "riag_query_result");
//    ASSERT_EQ(schema.n_children, 3);
//    ASSERT_STREQ(schema.children[0]->name, "p.fName");
//    ASSERT_STREQ(schema.children[1]->name, "k");
//    ASSERT_STREQ(schema.children[2]->name, "q.fName");
//
//    schema.release(&schema);
//    riag_query_result_destroy(result);
//}

TEST_F(CApiQueryResultTest, GetQuerySummary) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "MATCH (a:person) RETURN a.fName, a.age, a.height",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    riag_query_summary summary;
    state = riag_query_result_get_query_summary(&result, &summary);
    ASSERT_EQ(state, RiaGSuccess);
    auto compilingTime = riag_query_summary_get_compiling_time(&summary);
    ASSERT_GT(compilingTime, 0);
    auto executionTime = riag_query_summary_get_execution_time(&summary);
    ASSERT_GT(executionTime, 0);
    riag_query_summary_destroy(&summary);
    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetNext) {
    riag_query_result result;
    riag_flat_tuple row;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age ORDER BY a.fName", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));

    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &row);
    ASSERT_EQ(state, RiaGSuccess);
    auto flatTupleCpp = (FlatTuple*)(row._flat_tuple);
    ASSERT_EQ(flatTupleCpp->getValue(0)->getValue<std::string>(), "Alice");
    ASSERT_EQ(flatTupleCpp->getValue(1)->getValue<int64_t>(), 35);

    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &row);
    ASSERT_EQ(state, RiaGSuccess);
    flatTupleCpp = (FlatTuple*)(row._flat_tuple);
    ASSERT_EQ(flatTupleCpp->getValue(0)->getValue<std::string>(), "Bob");
    ASSERT_EQ(flatTupleCpp->getValue(1)->getValue<int64_t>(), 30);
    riag_flat_tuple_destroy(&row);

    while (riag_query_result_has_next(&result)) {
        riag_query_result_get_next(&result, &row);
    }
    ASSERT_FALSE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &row);
    ASSERT_EQ(state, RiaGError);
    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, ResetIterator) {
    riag_query_result result;
    riag_flat_tuple row;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age ORDER BY a.fName", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));

    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &row);
    ASSERT_EQ(state, RiaGSuccess);
    auto flatTupleCpp = (FlatTuple*)(row._flat_tuple);
    ASSERT_EQ(flatTupleCpp->getValue(0)->getValue<std::string>(), "Alice");
    ASSERT_EQ(flatTupleCpp->getValue(1)->getValue<int64_t>(), 35);

    riag_query_result_reset_iterator(&result);

    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &row);
    ASSERT_EQ(state, RiaGSuccess);
    flatTupleCpp = (FlatTuple*)(row._flat_tuple);
    ASSERT_EQ(flatTupleCpp->getValue(0)->getValue<std::string>(), "Alice");
    ASSERT_EQ(flatTupleCpp->getValue(1)->getValue<int64_t>(), 35);
    riag_flat_tuple_destroy(&row);

    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, MultipleQuery) {
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, "return 1; return 2; return 3;", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));

    char* str = riag_query_result_to_string(&result);
    ASSERT_EQ(std::string(str), "1\n1\n");
    riag_destroy_string(str);

    ASSERT_TRUE(riag_query_result_has_next_query_result(&result));
    riag_query_result next_query_result;
    ASSERT_EQ(riag_query_result_get_next_query_result(&result, &next_query_result), RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&next_query_result));
    str = riag_query_result_to_string(&next_query_result);
    ASSERT_EQ(std::string(str), "2\n2\n");
    riag_destroy_string(str);
    riag_query_result_destroy(&next_query_result);

    ASSERT_EQ(riag_query_result_get_next_query_result(&result, &next_query_result), RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&next_query_result));
    str = riag_query_result_to_string(&next_query_result);
    ASSERT_EQ(std::string(str), "3\n3\n");
    riag_destroy_string(str);

    ASSERT_FALSE(riag_query_result_has_next_query_result(&result));
    ASSERT_EQ(riag_query_result_get_next_query_result(&result, &next_query_result), RiaGError);
    riag_query_result_destroy(&next_query_result);

    riag_query_result_destroy(&result);
}

TEST_F(CApiQueryResultTest, GetLastErrorArrowSchemaFailure) {
    // A query result with a JSON column should fail to produce an Arrow schema
    riag_query_result result;
    riag_state state;
    auto connection = getConnection();

    state = riag_connection_query(connection, "RETURN CAST('{}' AS JSON)", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));

    ArrowSchema schema;
    state = riag_query_result_get_arrow_schema(&result, &schema);
    ASSERT_EQ(state, RiaGError);

    // Error message should be retrievable
    char* err = riag_get_last_error();
    ASSERT_NE(err, nullptr);
    ASSERT_NE(std::string(err).find("exception"), std::string::npos) << err;
    riag_destroy_string(err);

    riag_query_result_destroy(&result);
}
