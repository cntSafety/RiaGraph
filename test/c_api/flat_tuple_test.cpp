#include "c_api_test/c_api_test.h"

using namespace riag::common;
using namespace riag::main;
using namespace riag::testing;

class CApiFlatTupleTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
};

TEST_F(CApiFlatTupleTest, GetValue) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age, a.height ORDER BY a.fName LIMIT 1", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_NE(value._value, nullptr);
    auto valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(valueCpp->getValue<std::string>(), "Alice");
    riag_value_destroy(&value);
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 1, &value), RiaGSuccess);
    ASSERT_NE(value._value, nullptr);
    valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::INT64);
    ASSERT_EQ(valueCpp->getValue<int64_t>(), 35);
    riag_value_destroy(&value);
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 2, &value), RiaGSuccess);
    ASSERT_NE(value._value, nullptr);
    valueCpp = static_cast<Value*>(value._value);
    ASSERT_NE(valueCpp, nullptr);
    ASSERT_EQ(valueCpp->getDataType().getLogicalTypeID(), LogicalTypeID::FLOAT);
    ASSERT_FLOAT_EQ(valueCpp->getValue<float>(), 1.731);
    riag_value_destroy(&value);
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 222, &value), RiaGError);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiFlatTupleTest, ToString) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        "MATCH (a:person) RETURN a.fName, a.age, a.height ORDER BY a.fName LIMIT 1", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    auto columnWidths = (uint32_t*)malloc(3 * sizeof(uint32_t));
    columnWidths[0] = 10;
    columnWidths[1] = 5;
    columnWidths[2] = 10;
    char* str = riag_flat_tuple_to_string(&flatTuple);
    ASSERT_EQ(std::string(str), "Alice|35|1.731000\n");
    riag_destroy_string(str);
    free(columnWidths);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}
