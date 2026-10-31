#include <limits>

#include "c_api_test/c_api_test.h"

using namespace riag::main;
using namespace riag::common;
using namespace riag::testing;

class CApiValueTest : public CApiTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendRiaGRootPath("dataset/tinysnb/");
    }
};

TEST(CApiValueTestEmptyDB, CreateNull) {
    riag_value* value = riag_value_create_null();
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::ANY);
    ASSERT_EQ(cppValue->isNull(), true);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateNullWithDatatype) {
    riag_logical_type type;
    riag_data_type_create(riag_data_type_id::RIAG_INT64, nullptr, 0, &type);
    riag_value* value = riag_value_create_null_with_data_type(&type);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    riag_data_type_destroy(&type);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT64);
    ASSERT_EQ(cppValue->isNull(), true);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, IsNull) {
    riag_value* value = riag_value_create_int64(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(value));
    riag_value_destroy(value);
    value = riag_value_create_null();
    ASSERT_TRUE(riag_value_is_null(value));
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, SetNull) {
    riag_value* value = riag_value_create_int64(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(value));
    riag_value_set_null(value, true);
    ASSERT_TRUE(riag_value_is_null(value));
    riag_value_set_null(value, false);
    ASSERT_FALSE(riag_value_is_null(value));
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateDefault) {
    riag_logical_type type;
    riag_data_type_create(riag_data_type_id::RIAG_INT64, nullptr, 0, &type);
    riag_value* value = riag_value_create_default(&type);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    riag_data_type_destroy(&type);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_FALSE(riag_value_is_null(value));
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT64);
    ASSERT_EQ(cppValue->getValue<int64_t>(), 0);
    riag_value_destroy(value);

    riag_data_type_create(riag_data_type_id::RIAG_STRING, nullptr, 0, &type);
    value = riag_value_create_default(&type);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    riag_data_type_destroy(&type);
    cppValue = static_cast<Value*>(value->_value);
    ASSERT_FALSE(riag_value_is_null(value));
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(cppValue->getValue<std::string>(), "");
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateBool) {
    riag_value* value = riag_value_create_bool(true);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::BOOL);
    ASSERT_EQ(cppValue->getValue<bool>(), true);
    riag_value_destroy(value);

    value = riag_value_create_bool(false);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::BOOL);
    ASSERT_EQ(cppValue->getValue<bool>(), false);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateInt8) {
    riag_value* value = riag_value_create_int8(12);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT8);
    ASSERT_EQ(cppValue->getValue<int8_t>(), 12);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateInt16) {
    riag_value* value = riag_value_create_int16(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT16);
    ASSERT_EQ(cppValue->getValue<int16_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateInt32) {
    riag_value* value = riag_value_create_int32(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT32);
    ASSERT_EQ(cppValue->getValue<int32_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateInt64) {
    riag_value* value = riag_value_create_int64(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT64);
    ASSERT_EQ(cppValue->getValue<int64_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateUInt8) {
    riag_value* value = riag_value_create_uint8(12);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::UINT8);
    ASSERT_EQ(cppValue->getValue<uint8_t>(), 12);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateUInt16) {
    riag_value* value = riag_value_create_uint16(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::UINT16);
    ASSERT_EQ(cppValue->getValue<uint16_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateUInt32) {
    riag_value* value = riag_value_create_uint32(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::UINT32);
    ASSERT_EQ(cppValue->getValue<uint32_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateUInt64) {
    riag_value* value = riag_value_create_uint64(123);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::UINT64);
    ASSERT_EQ(cppValue->getValue<uint64_t>(), 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateINT128) {
    riag_value* value = riag_value_create_int128(riag_int128_t{211111111, 100000000});
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INT128);
    auto cppTimeStamp = cppValue->getValue<int128_t>();
    ASSERT_EQ(cppTimeStamp.high, 100000000);
    ASSERT_EQ(cppTimeStamp.low, 211111111);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateFloat) {
    riag_value* value = riag_value_create_float(123.456);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::FLOAT);
    ASSERT_FLOAT_EQ(cppValue->getValue<float>(), 123.456);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateDouble) {
    riag_value* value = riag_value_create_double(123.456);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::DOUBLE);
    ASSERT_DOUBLE_EQ(cppValue->getValue<double>(), 123.456);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateInternalID) {
    auto internalID = riag_internal_id_t{1, 123};
    riag_value* value = riag_value_create_internal_id(internalID);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INTERNAL_ID);
    auto internalIDCpp = cppValue->getValue<internalID_t>();
    ASSERT_EQ(internalIDCpp.tableID, 1);
    ASSERT_EQ(internalIDCpp.offset, 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateDate) {
    riag_value* value = riag_value_create_date(riag_date_t{123});
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::DATE);
    auto cppDate = cppValue->getValue<date_t>();
    ASSERT_EQ(cppDate.days, 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateTimeStamp) {
    riag_value* value = riag_value_create_timestamp(riag_timestamp_t{123});
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::TIMESTAMP);
    auto cppTimeStamp = cppValue->getValue<timestamp_t>();
    ASSERT_EQ(cppTimeStamp.value, 123);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateTimeStampNonStandard) {
    riag_value* value_ns = riag_value_create_timestamp_ns(riag_timestamp_ns_t{12345});
    riag_value* value_ms = riag_value_create_timestamp_ms(riag_timestamp_ms_t{123456});
    riag_value* value_sec = riag_value_create_timestamp_sec(riag_timestamp_sec_t{1234567});
    riag_value* value_tz = riag_value_create_timestamp_tz(riag_timestamp_tz_t{12345678});

    ASSERT_FALSE(value_ns->_is_owned_by_cpp);
    ASSERT_FALSE(value_ms->_is_owned_by_cpp);
    ASSERT_FALSE(value_sec->_is_owned_by_cpp);
    ASSERT_FALSE(value_tz->_is_owned_by_cpp);
    auto cppValue_ns = static_cast<Value*>(value_ns->_value);
    auto cppValue_ms = static_cast<Value*>(value_ms->_value);
    auto cppValue_sec = static_cast<Value*>(value_sec->_value);
    auto cppValue_tz = static_cast<Value*>(value_tz->_value);
    ASSERT_EQ(cppValue_ns->getDataType().getLogicalTypeID(), LogicalTypeID::TIMESTAMP_NS);
    ASSERT_EQ(cppValue_ms->getDataType().getLogicalTypeID(), LogicalTypeID::TIMESTAMP_MS);
    ASSERT_EQ(cppValue_sec->getDataType().getLogicalTypeID(), LogicalTypeID::TIMESTAMP_SEC);
    ASSERT_EQ(cppValue_tz->getDataType().getLogicalTypeID(), LogicalTypeID::TIMESTAMP_TZ);

    auto cppTimeStamp_ns = cppValue_ns->getValue<timestamp_ns_t>();
    auto cppTimeStamp_ms = cppValue_ms->getValue<timestamp_ms_t>();
    auto cppTimeStamp_sec = cppValue_sec->getValue<timestamp_sec_t>();
    auto cppTimeStamp_tz = cppValue_tz->getValue<timestamp_tz_t>();
    ASSERT_EQ(cppTimeStamp_ns.value, 12345);
    ASSERT_EQ(cppTimeStamp_ms.value, 123456);
    ASSERT_EQ(cppTimeStamp_sec.value, 1234567);
    ASSERT_EQ(cppTimeStamp_tz.value, 12345678);
    riag_value_destroy(value_ns);
    riag_value_destroy(value_ms);
    riag_value_destroy(value_sec);
    riag_value_destroy(value_tz);
}

TEST(CApiValueTestEmptyDB, CreateInterval) {
    riag_value* value = riag_value_create_interval(riag_interval_t{12, 3, 300});
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::INTERVAL);
    auto cppTimeStamp = cppValue->getValue<interval_t>();
    ASSERT_EQ(cppTimeStamp.months, 12);
    ASSERT_EQ(cppTimeStamp.days, 3);
    ASSERT_EQ(cppTimeStamp.micros, 300);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateString) {
    riag_value* value = riag_value_create_string((char*)"abcdefg");
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(cppValue->getValue<std::string>(), "abcdefg");
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateBlob) {
    const uint8_t blob_data[] = {0x01, 0x02, 0x00, 0xFF, 0xFE};
    riag_value* value = riag_value_create_blob(blob_data, sizeof(blob_data));
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::BLOB);
    ASSERT_EQ(cppValue->getValue<std::string>(),
        std::string(reinterpret_cast<const char*>(blob_data), sizeof(blob_data)));
    uint8_t* out_data = nullptr;
    uint64_t out_len = 0;
    ASSERT_EQ(riag_value_get_blob(value, &out_data, &out_len), RiaGSuccess);
    ASSERT_EQ(out_len, sizeof(blob_data));
    ASSERT_EQ(memcmp(out_data, blob_data, sizeof(blob_data)), 0);
    free(out_data);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateBlobNullData) {
    // A null buffer with a non-zero length is invalid and must fail gracefully.
    ASSERT_EQ(riag_value_create_blob(nullptr, 4), nullptr);
    // A null buffer with zero length creates an empty blob.
    riag_value* value = riag_value_create_blob(nullptr, 0);
    ASSERT_NE(value, nullptr);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::BLOB);
    ASSERT_TRUE(cppValue->getValue<std::string>().empty());
    riag_value_destroy(value);
}

TEST_F(CApiValueTest, CreateList) {
    auto connection = getConnection();
    riag_value* value1 = riag_value_create_int64(123);
    riag_value* value2 = riag_value_create_int64(456);
    riag_value* value3 = riag_value_create_int64(789);
    riag_value* value4 = riag_value_create_int64(101112);
    riag_value* value5 = riag_value_create_int64(131415);
    riag_value* elements[] = {value1, value2, value3, value4, value5};
    riag_value* value = nullptr;
    riag_state state = riag_value_create_list(5, elements, &value);
    ASSERT_EQ(state, RiaGSuccess);
    // Destroy the original values, the list should still be valid
    for (int i = 0; i < 5; ++i) {
        riag_value_destroy(elements[i]);
    }
    ASSERT_FALSE(value->_is_owned_by_cpp);
    riag_prepared_statement stmt;
    state = riag_connection_prepare(connection, (char*)"RETURN $1", &stmt);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_prepared_statement_bind_value(&stmt, "1", value);
    ASSERT_EQ(state, RiaGSuccess);
    riag_query_result result;
    state = riag_connection_execute(connection, &stmt, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    riag_flat_tuple flatTuple;
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value outValue;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &outValue), RiaGSuccess);
    ASSERT_TRUE(outValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&outValue));
    uint64_t size;
    ASSERT_EQ(riag_value_get_list_size(&outValue, &size), RiaGSuccess);
    ASSERT_EQ(size, 5);
    riag_value listElement;
    ASSERT_EQ(riag_value_get_list_element(&outValue, 0, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    int64_t int64Result;
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 123);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(&outValue, 1, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 456);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(&outValue, 2, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 789);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(&outValue, 3, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 101112);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(&outValue, 4, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 131415);
    riag_value_destroy(&listElement);
    riag_value_destroy(&outValue);
    riag_value_destroy(value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&stmt);
}

TEST(CApiValueTestEmptyDB, CreateListDifferentTypes) {
    riag_value* value1 = riag_value_create_int64(123);
    riag_value* value2 = riag_value_create_string((char*)"abcdefg");
    riag_value* elements[] = {value1, value2};
    riag_value* value = nullptr;
    riag_state state = riag_value_create_list(2, elements, &value);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value listElement;
    char* strResult = nullptr;
    ASSERT_EQ(riag_value_get_list_element(value, 0, &listElement), RiaGSuccess);
    ASSERT_EQ(riag_value_get_string(&listElement, &strResult), RiaGSuccess);
    ASSERT_STREQ(strResult, "123");
    riag_destroy_string(strResult);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(value, 1, &listElement), RiaGSuccess);
    ASSERT_EQ(riag_value_get_string(&listElement, &strResult), RiaGSuccess);
    ASSERT_STREQ(strResult, "abcdefg");
    riag_destroy_string(strResult);
    riag_value_destroy(&listElement);
    riag_value_destroy(value);
    riag_value_destroy(value1);
    riag_value_destroy(value2);
}

TEST(CApiValueTestEmptyDB, CreateListEmpty) {
    riag_value* elements[] = {nullptr}; // Must be non-empty
    riag_value* value = nullptr;
    riag_state state = riag_value_create_list(0, elements, &value);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::LIST);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::ANY);
    ASSERT_EQ(NestedVal::getChildrenSize(cppValue), 0);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListWithNull) {
    riag_value* value1 = riag_value_create_null();
    riag_value* value2 = riag_value_create_int64(123);
    riag_value* elements[] = {value1, value2};
    riag_value* value = nullptr;
    riag_state state = riag_value_create_list(2, elements, &value);
    ASSERT_EQ(state, RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::INT64);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 0)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<int64_t>(), 123);
    riag_value_destroy(value1);
    riag_value_destroy(value2);
    riag_value_destroy(value);
}

static riag_value* createTypedNull(riag_data_type_id typeID) {
    riag_logical_type type;
    riag_data_type_create(typeID, nullptr, 0, &type);
    riag_value* value = riag_value_create_null_with_data_type(&type);
    riag_data_type_destroy(&type);
    return value;
}

static void destroyValues(riag_value** values, uint64_t numValues) {
    for (uint64_t i = 0; i < numValues; ++i) {
        riag_value_destroy(values[i]);
    }
}

TEST(CApiValueTestEmptyDB, CreateListMixedIntegerWidths) {
    riag_value* elements[] = {riag_value_create_int8(1), riag_value_create_int16(1000),
        riag_value_create_uint8(200), riag_value_create_int64(-5)};
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(4, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::INT64);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 0)->getValue<int64_t>(), 1);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<int64_t>(), 1000);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 2)->getValue<int64_t>(), 200);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 3)->getValue<int64_t>(), -5);
    destroyValues(elements, 4);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListMixedSignIntegerExtremes) {
    riag_value* elements[] = {riag_value_create_int64(std::numeric_limits<int64_t>::min()),
        riag_value_create_uint64(std::numeric_limits<uint64_t>::max())};
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::INT128);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 0)->getDataType(), LogicalType::INT128());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 0)->toString(), "-9223372036854775808");
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getDataType(), LogicalType::INT128());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->toString(), "18446744073709551615");
    destroyValues(elements, 2);
    riag_value_destroy(value);
}

template<typename T>
static void addIntegerExtremes(std::vector<std::pair<riag_value*, std::string>>& extremes,
    riag_value* (*create)(T)) {
    extremes.emplace_back(create(std::numeric_limits<T>::min()),
        std::to_string(std::numeric_limits<T>::min()));
    extremes.emplace_back(create(std::numeric_limits<T>::max()),
        std::to_string(std::numeric_limits<T>::max()));
}

TEST(CApiValueTestEmptyDB, CreateListAllIntegerPairs) {
    std::vector<std::pair<riag_value*, std::string>> extremes;
    addIntegerExtremes(extremes, riag_value_create_int8);
    addIntegerExtremes(extremes, riag_value_create_int16);
    addIntegerExtremes(extremes, riag_value_create_int32);
    addIntegerExtremes(extremes, riag_value_create_int64);
    addIntegerExtremes(extremes, riag_value_create_uint8);
    addIntegerExtremes(extremes, riag_value_create_uint16);
    addIntegerExtremes(extremes, riag_value_create_uint32);
    addIntegerExtremes(extremes, riag_value_create_uint64);
    extremes.emplace_back(
        riag_value_create_int128(riag_int128_t{0, std::numeric_limits<int64_t>::min()}),
        "-170141183460469231731687303715884105728");
    extremes.emplace_back(
        riag_value_create_int128(riag_int128_t{std::numeric_limits<uint64_t>::max(),
            std::numeric_limits<int64_t>::max()}),
        "170141183460469231731687303715884105727");
    for (const auto& [left, leftString] : extremes) {
        for (const auto& [right, rightString] : extremes) {
            riag_value* elements[] = {left, right};
            riag_value* value = nullptr;
            ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess)
                << leftString << ", " << rightString;
            auto cppValue = static_cast<Value*>(value->_value);
            const auto& childType = ListType::getChildType(cppValue->getDataType());
            EXPECT_EQ(NestedVal::getChildVal(cppValue, 0)->getDataType(), childType);
            EXPECT_EQ(NestedVal::getChildVal(cppValue, 0)->toString(), leftString);
            EXPECT_EQ(NestedVal::getChildVal(cppValue, 1)->getDataType(), childType);
            EXPECT_EQ(NestedVal::getChildVal(cppValue, 1)->toString(), rightString);
            riag_value_destroy(value);
        }
    }
    for (const auto& extreme : extremes) {
        riag_value_destroy(extreme.first);
    }
}

TEST(CApiValueTestEmptyDB, CreateListWithSerialNull) {
    riag_value* elements[] = {createTypedNull(RIAG_SERIAL), riag_value_create_int8(1)};
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    const auto& childType = ListType::getChildType(cppValue->getDataType());
    ASSERT_EQ(childType.getLogicalTypeID(), LogicalTypeID::SERIAL);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 0)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getDataType(), childType);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<int64_t>(), 1);
    destroyValues(elements, 2);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListWithTypedNull) {
    riag_value* integers[] = {riag_value_create_int8(1), createTypedNull(RIAG_INT16),
        riag_value_create_int32(70000)};
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(3, integers, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::INT32);
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 0)->getValue<int32_t>(), 1);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 1)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 2)->getValue<int32_t>(), 70000);
    destroyValues(integers, 3);
    riag_value_destroy(value);

    riag_value* strings[] = {createTypedNull(RIAG_INT64), riag_value_create_string((char*)"a")};
    ASSERT_EQ(riag_value_create_list(2, strings, &value), RiaGSuccess);
    cppValue = static_cast<Value*>(value->_value);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 0)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<std::string>(), "a");
    destroyValues(strings, 2);
    riag_value_destroy(value);

    riag_value* doubles[] = {createTypedNull(RIAG_INT64), riag_value_create_double(2.5)};
    ASSERT_EQ(riag_value_create_list(2, doubles, &value), RiaGSuccess);
    cppValue = static_cast<Value*>(value->_value);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 0)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<double>(), 2.5);
    destroyValues(doubles, 2);
    riag_value_destroy(value);

    riag_value* timestamps[] = {createTypedNull(RIAG_DATE),
        riag_value_create_timestamp(riag_timestamp_t{1})};
    ASSERT_EQ(riag_value_create_list(2, timestamps, &value), RiaGSuccess);
    cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(ListType::getChildType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::TIMESTAMP);
    ASSERT_TRUE(NestedVal::getChildVal(cppValue, 0)->isNull());
    ASSERT_EQ(NestedVal::getChildVal(cppValue, 1)->getValue<timestamp_t>().value, 1);
    destroyValues(timestamps, 2);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListNestedMixedIntegerWidths) {
    riag_value* inner1[] = {riag_value_create_int8(1), createTypedNull(RIAG_INT8)};
    riag_value* inner2[] = {riag_value_create_int16(1000)};
    riag_value* elements[] = {nullptr, nullptr};
    ASSERT_EQ(riag_value_create_list(2, inner1, &elements[0]), RiaGSuccess);
    ASSERT_EQ(riag_value_create_list(1, inner2, &elements[1]), RiaGSuccess);
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType(), LogicalType::LIST(LogicalType::LIST(LogicalType::INT16())));
    ASSERT_EQ(NestedVal::getChildVal(NestedVal::getChildVal(cppValue, 0), 0)->getValue<int16_t>(),
        1);
    auto* nestedNull = NestedVal::getChildVal(NestedVal::getChildVal(cppValue, 0), 1);
    ASSERT_TRUE(nestedNull->isNull());
    ASSERT_EQ(nestedNull->getDataType().getLogicalTypeID(), LogicalTypeID::INT16);
    ASSERT_EQ(NestedVal::getChildVal(NestedVal::getChildVal(cppValue, 1), 0)->getValue<int16_t>(),
        1000);
    destroyValues(inner1, 2);
    destroyValues(inner2, 1);
    destroyValues(elements, 2);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListOfStructsMixedIntegerWidths) {
    const char* fieldNames[] = {"a"};
    riag_value* fields1[] = {riag_value_create_int8(1)};
    riag_value* fields2[] = {riag_value_create_int16(1000)};
    riag_value* elements[] = {nullptr, nullptr};
    ASSERT_EQ(riag_value_create_struct(1, fieldNames, fields1, &elements[0]), RiaGSuccess);
    ASSERT_EQ(riag_value_create_struct(1, fieldNames, fields2, &elements[1]), RiaGSuccess);
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    const auto& childType = ListType::getChildType(cppValue->getDataType());
    ASSERT_EQ(childType.getLogicalTypeID(), LogicalTypeID::STRUCT);
    ASSERT_EQ(StructType::getFieldType(childType, 0).getLogicalTypeID(), LogicalTypeID::INT16);
    ASSERT_EQ(NestedVal::getChildVal(NestedVal::getChildVal(cppValue, 0), 0)->getValue<int16_t>(),
        1);
    ASSERT_EQ(NestedVal::getChildVal(NestedVal::getChildVal(cppValue, 1), 0)->getValue<int16_t>(),
        1000);
    destroyValues(fields1, 1);
    destroyValues(fields2, 1);
    destroyValues(elements, 2);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListOfMapsMixedIntegerWidths) {
    riag_value* keys[] = {riag_value_create_int8(1)};
    riag_value* values1[] = {riag_value_create_int8(1)};
    riag_value* values2[] = {riag_value_create_int16(1000)};
    riag_value* elements[] = {nullptr, nullptr};
    ASSERT_EQ(riag_value_create_map(1, keys, values1, &elements[0]), RiaGSuccess);
    ASSERT_EQ(riag_value_create_map(1, keys, values2, &elements[1]), RiaGSuccess);
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGSuccess);
    auto cppValue = static_cast<Value*>(value->_value);
    const auto& childType = ListType::getChildType(cppValue->getDataType());
    ASSERT_EQ(MapType::getKeyType(childType).getLogicalTypeID(), LogicalTypeID::INT8);
    ASSERT_EQ(MapType::getValueType(childType).getLogicalTypeID(), LogicalTypeID::INT16);
    riag_value map;
    riag_value mapValue;
    int16_t int16Result = 0;
    ASSERT_EQ(riag_value_get_list_element(value, 0, &map), RiaGSuccess);
    ASSERT_EQ(riag_value_get_map_value(&map, 0, &mapValue), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int16(&mapValue, &int16Result), RiaGSuccess);
    ASSERT_EQ(int16Result, 1);
    riag_value_destroy(&mapValue);
    riag_value_destroy(&map);
    ASSERT_EQ(riag_value_get_list_element(value, 1, &map), RiaGSuccess);
    ASSERT_EQ(riag_value_get_map_value(&map, 0, &mapValue), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int16(&mapValue, &int16Result), RiaGSuccess);
    ASSERT_EQ(int16Result, 1000);
    riag_value_destroy(&mapValue);
    riag_value_destroy(&map);
    destroyValues(keys, 1);
    destroyValues(values1, 1);
    destroyValues(values2, 1);
    destroyValues(elements, 2);
    riag_value_destroy(value);
}

TEST(CApiValueTestEmptyDB, CreateListNestedIncompatibleTypes) {
    riag_value* dates[] = {riag_value_create_date(riag_date_t{1})};
    riag_value* timestamps[] = {riag_value_create_timestamp(riag_timestamp_t{1})};
    riag_value* elements[] = {nullptr, nullptr};
    ASSERT_EQ(riag_value_create_list(1, dates, &elements[0]), RiaGSuccess);
    ASSERT_EQ(riag_value_create_list(1, timestamps, &elements[1]), RiaGSuccess);
    riag_value* value = nullptr;
    ASSERT_EQ(riag_value_create_list(2, elements, &value), RiaGError);
    char* err = riag_get_last_error();
    ASSERT_STREQ(err, "Runtime exception: Cannot convert Python object to RiaG value : DATE[]  is "
                      "incompatible with TIMESTAMP[]");
    riag_destroy_string(err);
    destroyValues(dates, 1);
    destroyValues(timestamps, 1);
    destroyValues(elements, 2);
}

TEST_F(CApiValueTest, BindListMixedIntegerWidths) {
    auto connection = getConnection();
    riag_value* elements[] = {riag_value_create_int8(0), riag_value_create_int8(2),
        riag_value_create_int16(1000)};
    riag_value* ids = nullptr;
    ASSERT_EQ(riag_value_create_list(3, elements, &ids), RiaGSuccess);
    riag_prepared_statement stmt;
    ASSERT_EQ(riag_connection_prepare(connection,
                  (char*)"MATCH (a:person) WHERE a.ID IN $ids RETURN count(*)", &stmt),
        RiaGSuccess);
    ASSERT_EQ(riag_prepared_statement_bind_value(&stmt, "ids", ids), RiaGSuccess);
    riag_query_result result;
    ASSERT_EQ(riag_connection_execute(connection, &stmt, &result), RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    riag_flat_tuple flatTuple;
    ASSERT_EQ(riag_query_result_get_next(&result, &flatTuple), RiaGSuccess);
    riag_value count;
    int64_t countResult = 0;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &count), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int64(&count, &countResult), RiaGSuccess);
    ASSERT_EQ(countResult, 2);
    riag_value_destroy(&count);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&stmt);
    destroyValues(elements, 3);
    riag_value_destroy(ids);
}

TEST_F(CApiValueTest, CreateListNested) {
    auto connection = getConnection();
    riag_value* value1 = riag_value_create_int64(123);
    riag_value* value2 = riag_value_create_int64(456);
    riag_value* value3 = riag_value_create_int64(789);
    riag_value* value4 = riag_value_create_int64(101112);
    riag_value* value5 = riag_value_create_int64(131415);
    riag_value* elements1[] = {value1, value2, value3};
    riag_value* elements2[] = {value4, value5};
    riag_value* list1 = nullptr;
    riag_value* list2 = nullptr;
    riag_value_create_list(3, elements1, &list1);
    ASSERT_FALSE(list1->_is_owned_by_cpp);
    riag_value_create_list(2, elements2, &list2);
    ASSERT_FALSE(list2->_is_owned_by_cpp);
    riag_value* elements[] = {list1, list2};
    riag_value* nestedList = nullptr;
    riag_state state = riag_value_create_list(2, elements, &nestedList);
    ASSERT_EQ(state, RiaGSuccess);
    // Destroy the original values, the list should still be valid
    for (int i = 0; i < 3; ++i) {
        riag_value_destroy(elements1[i]);
    }
    for (int i = 0; i < 2; ++i) {
        riag_value_destroy(elements2[i]);
    }
    riag_value_destroy(list1);
    riag_value_destroy(list2);
    ASSERT_FALSE(nestedList->_is_owned_by_cpp);
    riag_prepared_statement stmt;
    state = riag_connection_prepare(connection, (char*)"RETURN $1", &stmt);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_prepared_statement_bind_value(&stmt, "1", nestedList);
    ASSERT_EQ(state, RiaGSuccess);
    riag_query_result result;
    state = riag_connection_execute(connection, &stmt, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    riag_flat_tuple flatTuple;
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value outValue;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &outValue), RiaGSuccess);
    ASSERT_TRUE(outValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&outValue));
    uint64_t size;
    ASSERT_EQ(riag_value_get_list_size(&outValue, &size), RiaGSuccess);
    ASSERT_EQ(size, 2);
    riag_value listElement;
    ASSERT_EQ(riag_value_get_list_element(&outValue, 0, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&listElement));
    ASSERT_EQ(riag_value_get_list_size(&listElement, &size), RiaGSuccess);
    ASSERT_EQ(size, 3);
    riag_value innerListElement;
    ASSERT_EQ(riag_value_get_list_element(&listElement, 0, &innerListElement), RiaGSuccess);
    ASSERT_TRUE(innerListElement._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&innerListElement));
    int64_t int64Result;
    ASSERT_EQ(riag_value_get_int64(&innerListElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 123);
    riag_value_destroy(&innerListElement);
    ASSERT_EQ(riag_value_get_list_element(&listElement, 1, &innerListElement), RiaGSuccess);
    ASSERT_TRUE(innerListElement._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&innerListElement));
    ASSERT_EQ(riag_value_get_int64(&innerListElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 456);
    riag_value_destroy(&innerListElement);
    ASSERT_EQ(riag_value_get_list_element(&listElement, 2, &innerListElement), RiaGSuccess);
    ASSERT_TRUE(innerListElement._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&innerListElement));
    ASSERT_EQ(riag_value_get_int64(&innerListElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 789);
    riag_value_destroy(&innerListElement);
    riag_value_destroy(&listElement);
    ASSERT_EQ(riag_value_get_list_element(&outValue, 1, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&listElement));
    ASSERT_EQ(riag_value_get_list_size(&listElement, &size), RiaGSuccess);
    ASSERT_EQ(size, 2);
    riag_value_destroy(&listElement);
    riag_value_destroy(&outValue);
    riag_value_destroy(nestedList);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&stmt);
}

TEST_F(CApiValueTest, CreateStruct) {
    auto connection = getConnection();
    riag_value* value1 = riag_value_create_int16(32);
    riag_value* value2 = riag_value_create_string((char*)"Wong");
    riag_value* value3 = riag_value_create_string((char*)"Kelley");
    riag_value* value4 = riag_value_create_int64(123456);
    riag_value* value5 = riag_value_create_string((char*)"CEO");
    riag_value* value6 = riag_value_create_bool(true);
    riag_value* employmentElements[] = {value5, value6};
    const char* employmentFieldNames[] = {(char*)"title", (char*)"is_current"};
    riag_value* employment = nullptr;
    riag_state state =
        riag_value_create_struct(2, employmentFieldNames, employmentElements, &employment);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(employment->_is_owned_by_cpp);
    riag_value_destroy(value5);
    riag_value_destroy(value6);
    riag_value* personElements[] = {value1, value2, value3, value4, employment};
    const char* personFieldNames[] = {(char*)"age", (char*)"first_name", (char*)"last_name",
        (char*)"id", (char*)"employment"};
    riag_value* person = nullptr;
    state = riag_value_create_struct(5, personFieldNames, personElements, &person);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value_destroy(value1);
    riag_value_destroy(value2);
    riag_value_destroy(value3);
    riag_value_destroy(value4);
    riag_value_destroy(employment);
    ASSERT_FALSE(person->_is_owned_by_cpp);
    riag_prepared_statement stmt;
    state = riag_connection_prepare(connection, (char*)"RETURN $1", &stmt);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_prepared_statement_bind_value(&stmt, "1", person);
    ASSERT_EQ(state, RiaGSuccess);
    riag_query_result result;
    state = riag_connection_execute(connection, &stmt, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    riag_flat_tuple flatTuple;
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value outValue;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &outValue), RiaGSuccess);
    ASSERT_TRUE(outValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&outValue));
    uint64_t size;
    state = riag_value_get_struct_num_fields(&outValue, &size);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(size, 5);
    char* structFieldName;
    riag_value structFieldValue;
    state = riag_value_get_struct_field_name(&outValue, 0, &structFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(structFieldName, "age");
    state = riag_value_get_struct_field_value(&outValue, 0, &structFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(structFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&structFieldValue));
    int16_t int16Result;
    state = riag_value_get_int16(&structFieldValue, &int16Result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(int16Result, 32);
    riag_value_destroy(&structFieldValue);
    riag_destroy_string(structFieldName);
    state = riag_value_get_struct_field_name(&outValue, 1, &structFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(structFieldName, "first_name");
    state = riag_value_get_struct_field_value(&outValue, 1, &structFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(structFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&structFieldValue));
    char* stringResult;
    state = riag_value_get_string(&structFieldValue, &stringResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(stringResult, "Wong");
    riag_value_destroy(&structFieldValue);
    riag_destroy_string(structFieldName);
    riag_destroy_string(stringResult);
    state = riag_value_get_struct_field_name(&outValue, 2, &structFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(structFieldName, "last_name");
    state = riag_value_get_struct_field_value(&outValue, 2, &structFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(structFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&structFieldValue));
    state = riag_value_get_string(&structFieldValue, &stringResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(stringResult, "Kelley");
    riag_value_destroy(&structFieldValue);
    riag_destroy_string(structFieldName);
    riag_destroy_string(stringResult);
    state = riag_value_get_struct_field_name(&outValue, 3, &structFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(structFieldName, "id");
    state = riag_value_get_struct_field_value(&outValue, 3, &structFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(structFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&structFieldValue));
    int64_t int64Result;
    state = riag_value_get_int64(&structFieldValue, &int64Result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(int64Result, 123456);
    riag_value_destroy(&structFieldValue);
    riag_destroy_string(structFieldName);
    state = riag_value_get_struct_field_name(&outValue, 4, &structFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(structFieldName, "employment");
    state = riag_value_get_struct_field_value(&outValue, 4, &structFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(structFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&structFieldValue));
    state = riag_value_get_struct_num_fields(&structFieldValue, &size);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(size, 2);
    char* employmentFieldName;
    riag_value employmentFieldValue;
    state = riag_value_get_struct_field_name(&structFieldValue, 0, &employmentFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(employmentFieldName, "title");
    state = riag_value_get_struct_field_value(&structFieldValue, 0, &employmentFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(employmentFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&employmentFieldValue));
    state = riag_value_get_string(&employmentFieldValue, &stringResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(stringResult, "CEO");
    riag_value_destroy(&employmentFieldValue);
    riag_destroy_string(employmentFieldName);
    riag_destroy_string(stringResult);
    state = riag_value_get_struct_field_name(&structFieldValue, 1, &employmentFieldName);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_STREQ(employmentFieldName, "is_current");
    state = riag_value_get_struct_field_value(&structFieldValue, 1, &employmentFieldValue);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(employmentFieldValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&employmentFieldValue));
    bool boolResult;
    state = riag_value_get_bool(&employmentFieldValue, &boolResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_EQ(boolResult, true);
    riag_value_destroy(&employmentFieldValue);
    riag_destroy_string(employmentFieldName);
    riag_value_destroy(&structFieldValue);
    riag_destroy_string(structFieldName);
    riag_value_destroy(&outValue);
    riag_value_destroy(person);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&stmt);
}

TEST(CApiValueTestEmptyDB, CreateStructEmpty) {
    const char* fieldNames[] = {(char*)"name"}; // Must be non-empty
    riag_value* values[] = {nullptr};           // Must be non-empty
    riag_value* value = nullptr;
    riag_state state = riag_value_create_struct(0, fieldNames, values, &value);
    ASSERT_EQ(state, RiaGError);
}

TEST_F(CApiValueTest, CreateMap) {
    auto connection = getConnection();
    riag_value* key1 = riag_value_create_int64(1);
    riag_value* value1 = riag_value_create_string((char*)"one");
    riag_value* key2 = riag_value_create_int64(2);
    riag_value* value2 = riag_value_create_string((char*)"two");
    riag_value* key3 = riag_value_create_int64(3);
    riag_value* value3 = riag_value_create_string((char*)"three");
    riag_value* keys[] = {key1, key2, key3};
    riag_value* values[] = {value1, value2, value3};
    riag_value* map = nullptr;
    riag_state state = riag_value_create_map(3, keys, values, &map);
    ASSERT_EQ(state, RiaGSuccess);
    // Destroy the original values, the map should still be valid
    for (int i = 0; i < 3; ++i) {
        riag_value_destroy(keys[i]);
        riag_value_destroy(values[i]);
    }
    ASSERT_FALSE(map->_is_owned_by_cpp);
    riag_prepared_statement stmt;
    state = riag_connection_prepare(connection, (char*)"RETURN $1", &stmt);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_prepared_statement_bind_value(&stmt, "1", map);
    ASSERT_EQ(state, RiaGSuccess);
    riag_query_result result;
    state = riag_connection_execute(connection, &stmt, &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    riag_flat_tuple flatTuple;
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value outValue;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &outValue), RiaGSuccess);
    ASSERT_TRUE(outValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&outValue));
    uint64_t size;
    ASSERT_EQ(riag_value_get_map_size(&outValue, &size), RiaGSuccess);
    ASSERT_EQ(size, 3);
    riag_value mapValue;
    ASSERT_EQ(riag_value_get_map_value(&outValue, 0, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    char* stringResult;
    ASSERT_EQ(riag_value_get_string(&mapValue, &stringResult), RiaGSuccess);
    ASSERT_STREQ(stringResult, "one");
    riag_value_destroy(&mapValue);
    riag_destroy_string(stringResult);
    ASSERT_EQ(riag_value_get_map_value(&outValue, 1, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    ASSERT_EQ(riag_value_get_string(&mapValue, &stringResult), RiaGSuccess);
    ASSERT_STREQ(stringResult, "two");
    riag_value_destroy(&mapValue);
    riag_destroy_string(stringResult);
    ASSERT_EQ(riag_value_get_map_value(&outValue, 2, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    ASSERT_EQ(riag_value_get_string(&mapValue, &stringResult), RiaGSuccess);
    ASSERT_STREQ(stringResult, "three");
    riag_value_destroy(&mapValue);
    riag_destroy_string(stringResult);
    ASSERT_EQ(riag_value_get_map_key(&outValue, 0, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    int64_t int64Result;
    ASSERT_EQ(riag_value_get_int64(&mapValue, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 1);
    riag_value_destroy(&mapValue);
    ASSERT_EQ(riag_value_get_map_key(&outValue, 1, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    ASSERT_EQ(riag_value_get_int64(&mapValue, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 2);
    riag_value_destroy(&mapValue);
    ASSERT_EQ(riag_value_get_map_key(&outValue, 2, &mapValue), RiaGSuccess);
    ASSERT_TRUE(mapValue._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&mapValue));
    ASSERT_EQ(riag_value_get_int64(&mapValue, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 3);
    riag_value_destroy(&mapValue);
    riag_value_destroy(&outValue);
    riag_value_destroy(map);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_prepared_statement_destroy(&stmt);
}

TEST(CApiValueTestEmptyDB, CreateMapEmpty) {
    riag_value* keys[] = {nullptr};   // Must be non-empty
    riag_value* values[] = {nullptr}; // Must be non-empty
    riag_value* map = nullptr;
    riag_state state = riag_value_create_map(0, keys, values, &map);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(map->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(map->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::MAP);
    ASSERT_EQ(MapType::getKeyType(cppValue->getDataType()).getLogicalTypeID(), LogicalTypeID::ANY);
    ASSERT_EQ(MapType::getValueType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::ANY);
    ASSERT_EQ(NestedVal::getChildrenSize(cppValue), 0);
    riag_value_destroy(map);
}

TEST(CApiValueTestEmptyDB, CreateMapWithNull) {
    riag_value* key1 = riag_value_create_null();
    riag_value* key2 = riag_value_create_int64(2);
    riag_value* value1 = riag_value_create_string((char*)"one");
    riag_value* value2 = riag_value_create_null();
    riag_value* keys[] = {key1, key2};
    riag_value* values[] = {value1, value2};
    riag_value* map = nullptr;
    riag_state state = riag_value_create_map(2, keys, values, &map);
    ASSERT_EQ(state, RiaGSuccess);
    auto cppValue = static_cast<Value*>(map->_value);
    ASSERT_EQ(MapType::getKeyType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::INT64);
    ASSERT_EQ(MapType::getValueType(cppValue->getDataType()).getLogicalTypeID(),
        LogicalTypeID::STRING);
    riag_value mapKey;
    riag_value mapValue;
    ASSERT_EQ(riag_value_get_map_key(map, 0, &mapKey), RiaGSuccess);
    ASSERT_EQ(riag_value_get_map_value(map, 1, &mapValue), RiaGSuccess);
    ASSERT_TRUE(riag_value_is_null(&mapKey));
    ASSERT_TRUE(riag_value_is_null(&mapValue));
    riag_value_destroy(&mapKey);
    riag_value_destroy(&mapValue);
    riag_value_destroy(key1);
    riag_value_destroy(key2);
    riag_value_destroy(value1);
    riag_value_destroy(value2);
    riag_value_destroy(map);
}

TEST(CApiValueTestEmptyDB, CreateMapMixedIntegerWidths) {
    riag_value* keys[] = {riag_value_create_int8(1), riag_value_create_int16(1000)};
    riag_value* values[] = {riag_value_create_int64(7), riag_value_create_int8(-1)};
    riag_value* map = nullptr;
    ASSERT_EQ(riag_value_create_map(2, keys, values, &map), RiaGSuccess);
    auto cppValue = static_cast<Value*>(map->_value);
    ASSERT_EQ(cppValue->getDataType(),
        LogicalType::MAP(LogicalType::INT16(), LogicalType::INT64()));
    riag_value mapKey;
    riag_value mapValue;
    int16_t keyResult = 0;
    int64_t valueResult = 0;
    ASSERT_EQ(riag_value_get_map_key(map, 0, &mapKey), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int16(&mapKey, &keyResult), RiaGSuccess);
    ASSERT_EQ(keyResult, 1);
    riag_value_destroy(&mapKey);
    ASSERT_EQ(riag_value_get_map_key(map, 1, &mapKey), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int16(&mapKey, &keyResult), RiaGSuccess);
    ASSERT_EQ(keyResult, 1000);
    riag_value_destroy(&mapKey);
    ASSERT_EQ(riag_value_get_map_value(map, 1, &mapValue), RiaGSuccess);
    ASSERT_EQ(riag_value_get_int64(&mapValue, &valueResult), RiaGSuccess);
    ASSERT_EQ(valueResult, -1);
    riag_value_destroy(&mapValue);
    destroyValues(keys, 2);
    destroyValues(values, 2);
    riag_value_destroy(map);
}

TEST(CApiValueTestEmptyDB, Clone) {
    riag_value* value = riag_value_create_string((char*)"abcdefg");
    ASSERT_FALSE(value->_is_owned_by_cpp);
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(cppValue->getValue<std::string>(), "abcdefg");

    riag_value* clone = riag_value_clone(value);
    riag_value_destroy(value);

    ASSERT_FALSE(clone->_is_owned_by_cpp);
    auto cppClone = static_cast<Value*>(clone->_value);
    ASSERT_EQ(cppClone->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(cppClone->getValue<std::string>(), "abcdefg");
    riag_value_destroy(clone);
}

TEST(CApiValueTestEmptyDB, Copy) {
    riag_value* value = riag_value_create_string((char*)"abc");

    riag_value* value2 = riag_value_create_string((char*)"abcdefg");
    riag_value_copy(value, value2);
    riag_value_destroy(value2);

    ASSERT_FALSE(riag_value_is_null(value));
    auto cppValue = static_cast<Value*>(value->_value);
    ASSERT_EQ(cppValue->getDataType().getLogicalTypeID(), LogicalTypeID::STRING);
    ASSERT_EQ(cppValue->getValue<std::string>(), "abcdefg");
    riag_value_destroy(value);
}

TEST_F(CApiValueTest, GetListSize) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.workedHours ORDER BY a.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint64_t size;
    ASSERT_EQ(riag_value_get_list_size(&value, &size), RiaGSuccess);
    ASSERT_EQ(size, 2);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_list_size(badValue, &size), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetListElement) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.workedHours ORDER BY a.ID", &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint64_t size;
    ASSERT_EQ(riag_value_get_list_size(&value, &size), RiaGSuccess);
    ASSERT_EQ(size, 2);

    riag_value listElement;
    ASSERT_EQ(riag_value_get_list_element(&value, 0, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    int64_t int64Result;
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 10);

    ASSERT_EQ(riag_value_get_list_element(&value, 1, &listElement), RiaGSuccess);
    ASSERT_TRUE(listElement._is_owned_by_cpp);
    ASSERT_EQ(riag_value_get_int64(&listElement, &int64Result), RiaGSuccess);
    ASSERT_EQ(int64Result, 5);
    riag_value_destroy(&listElement);

    ASSERT_EQ(riag_value_get_list_element(&value, 222, &listElement), RiaGError);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, GetStructNumFields) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.name=\"Roma\" RETURN m.description", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    riag_flat_tuple_get_value(&flatTuple, 0, &value);
    uint64_t numFields;
    ASSERT_EQ(riag_value_get_struct_num_fields(&value, &numFields), RiaGSuccess);
    ASSERT_EQ(numFields, 14);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_struct_num_fields(badValue, &numFields), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetStructFieldName) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.name=\"Roma\" RETURN m.description", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    char* fieldName;
    ASSERT_EQ(riag_value_get_struct_field_name(&value, 0, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "rating");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 1, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "stars");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 2, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "views");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 3, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "release");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 4, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "release_ns");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 5, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "release_ms");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 6, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "release_sec");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 7, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "release_tz");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 8, &fieldName), RiaGSuccess);
    ASSERT_STREQ(fieldName, "film");
    riag_destroy_string(fieldName);

    ASSERT_EQ(riag_value_get_struct_field_name(&value, 222, &fieldName), RiaGError);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, GetStructFieldValue) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.name=\"Roma\" RETURN m.description", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);

    riag_value fieldValue;
    ASSERT_EQ(riag_value_get_struct_field_value(&value, 0, &fieldValue), RiaGSuccess);
    riag_logical_type fieldType;
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_DOUBLE);
    double doubleValue;
    ASSERT_EQ(riag_value_get_double(&fieldValue, &doubleValue), RiaGSuccess);
    ASSERT_DOUBLE_EQ(doubleValue, 1223);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 1, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 2, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_INT64);
    int64_t int64Value;
    ASSERT_EQ(riag_value_get_int64(&fieldValue, &int64Value), RiaGSuccess);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 3, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_TIMESTAMP);
    riag_timestamp_t timestamp;
    ASSERT_EQ(riag_value_get_timestamp(&fieldValue, &timestamp), RiaGSuccess);
    ASSERT_EQ(timestamp.value, 1297442662000000);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 4, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_TIMESTAMP_NS);
    riag_timestamp_ns_t timestamp_ns;
    ASSERT_EQ(riag_value_get_timestamp_ns(&fieldValue, &timestamp_ns), RiaGSuccess);
    ASSERT_EQ(timestamp_ns.value, 1297442662123456000);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 5, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_TIMESTAMP_MS);
    riag_timestamp_ms_t timestamp_ms;
    ASSERT_EQ(riag_value_get_timestamp_ms(&fieldValue, &timestamp_ms), RiaGSuccess);
    ASSERT_EQ(timestamp_ms.value, 1297442662123);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 6, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_TIMESTAMP_SEC);
    riag_timestamp_sec_t timestamp_sec;
    ASSERT_EQ(riag_value_get_timestamp_sec(&fieldValue, &timestamp_sec), RiaGSuccess);
    ASSERT_EQ(timestamp_sec.value, 1297442662);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 7, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_TIMESTAMP_TZ);
    riag_timestamp_tz_t timestamp_tz;
    ASSERT_EQ(riag_value_get_timestamp_tz(&fieldValue, &timestamp_tz), RiaGSuccess);
    ASSERT_EQ(timestamp_tz.value, 1297442662123456);
    riag_data_type_destroy(&fieldType);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 8, &fieldValue), RiaGSuccess);
    riag_value_get_data_type(&fieldValue, &fieldType);
    ASSERT_EQ(riag_data_type_get_id(&fieldType), RIAG_DATE);
    riag_date_t date;
    ASSERT_EQ(riag_value_get_date(&fieldValue, &date), RiaGSuccess);
    ASSERT_EQ(date.days, 15758);
    riag_data_type_destroy(&fieldType);
    riag_value_destroy(&fieldValue);

    ASSERT_EQ(riag_value_get_struct_field_value(&value, 222, &fieldValue), RiaGError);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, getMapNumFields) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.length = 2544 RETURN m.audience", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(riag_query_result_has_next(&result));
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);

    uint64_t mapFields;
    ASSERT_EQ(riag_value_get_map_size(&value, &mapFields), RiaGSuccess);
    ASSERT_EQ(mapFields, 1);

    riag_query_result_destroy(&result);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
}

TEST_F(CApiValueTest, getMapKey) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.length = 2544 RETURN m.audience", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(riag_query_result_has_next(&result));
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);

    riag_value key;
    ASSERT_EQ(riag_value_get_map_key(&value, 0, &key), RiaGSuccess);
    riag_logical_type keyType;
    riag_value_get_data_type(&key, &keyType);
    ASSERT_EQ(riag_data_type_get_id(&keyType), RIAG_STRING);
    char* mapName;
    ASSERT_EQ(riag_value_get_string(&key, &mapName), RiaGSuccess);
    ASSERT_STREQ(mapName, "audience1");
    riag_destroy_string(mapName);
    riag_data_type_destroy(&keyType);
    riag_value_destroy(&key);

    ASSERT_EQ(riag_value_get_map_key(&value, 1, &key), RiaGError);
    riag_query_result_destroy(&result);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
}

TEST_F(CApiValueTest, getMapValue) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) WHERE m.length = 2544 RETURN m.audience", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_FALSE(riag_query_result_has_next(&result));
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);

    riag_value mapValue;
    ASSERT_EQ(riag_value_get_map_value(&value, 0, &mapValue), RiaGSuccess);
    riag_logical_type mapType;
    riag_value_get_data_type(&mapValue, &mapType);
    ASSERT_EQ(riag_data_type_get_id(&mapType), RIAG_INT64);
    int64_t mapIntValue;
    ASSERT_EQ(riag_value_get_int64(&mapValue, &mapIntValue), RiaGSuccess);
    ASSERT_EQ(mapIntValue, 33);

    ASSERT_EQ(riag_value_get_map_value(&value, 1, &mapValue), RiaGError);

    riag_data_type_destroy(&mapType);
    riag_query_result_destroy(&result);
    riag_value_destroy(&mapValue);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
}

TEST_F(CApiValueTest, getDecimalAsString) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"UNWIND [1] AS A UNWIND [5.7, 8.3, 8.7, 13.7] AS B WITH cast(CAST(A AS DECIMAL) "
               "* "
               "CAST(B AS DECIMAL) AS DECIMAL(18, 1)) AS PROD RETURN COLLECT(PROD) AS RES",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);

    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);

    riag_logical_type dataType;
    riag_value_get_data_type(&value, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_LIST);
    uint64_t list_size;
    ASSERT_EQ(riag_value_get_list_size(&value, &list_size), RiaGSuccess);
    ASSERT_EQ(list_size, 4);
    riag_data_type_destroy(&dataType);

    riag_value decimal_entry;
    char* decimal_value;
    std::string decimal_string_value;
    ASSERT_EQ(riag_value_get_list_element(&value, 0, &decimal_entry), RiaGSuccess);
    riag_value_get_data_type(&decimal_entry, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_DECIMAL);
    ASSERT_EQ(riag_value_get_decimal_as_string(&decimal_entry, &decimal_value), RiaGSuccess);
    decimal_string_value = std::string(decimal_value);
    ASSERT_EQ(decimal_string_value, "5.7");
    riag_destroy_string(decimal_value);
    riag_data_type_destroy(&dataType);

    ASSERT_EQ(riag_value_get_list_element(&value, 1, &decimal_entry), RiaGSuccess);
    riag_value_get_data_type(&decimal_entry, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_DECIMAL);
    ASSERT_EQ(riag_value_get_decimal_as_string(&decimal_entry, &decimal_value), RiaGSuccess);
    decimal_string_value = std::string(decimal_value);
    ASSERT_EQ(decimal_string_value, "8.3");
    riag_destroy_string(decimal_value);
    riag_data_type_destroy(&dataType);

    ASSERT_EQ(riag_value_get_list_element(&value, 2, &decimal_entry), RiaGSuccess);
    riag_value_get_data_type(&decimal_entry, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_DECIMAL);
    ASSERT_EQ(riag_value_get_decimal_as_string(&decimal_entry, &decimal_value), RiaGSuccess);
    decimal_string_value = std::string(decimal_value);
    ASSERT_EQ(decimal_string_value, "8.7");
    riag_destroy_string(decimal_value);
    riag_data_type_destroy(&dataType);

    ASSERT_EQ(riag_value_get_list_element(&value, 3, &decimal_entry), RiaGSuccess);
    riag_value_get_data_type(&decimal_entry, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_DECIMAL);
    ASSERT_EQ(riag_value_get_decimal_as_string(&decimal_entry, &decimal_value), RiaGSuccess);
    decimal_string_value = std::string(decimal_value);
    ASSERT_EQ(decimal_string_value, "13.7");
    riag_destroy_string(decimal_value);
    riag_data_type_destroy(&dataType);

    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
    riag_value_destroy(&decimal_entry);
}

TEST_F(CApiValueTest, GetDataType) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.fName, a.isStudent, a.workedHours", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    riag_logical_type dataType;
    riag_value_get_data_type(&value, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_STRING);
    riag_data_type_destroy(&dataType);

    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 1, &value), RiaGSuccess);
    riag_value_get_data_type(&value, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_BOOL);
    riag_data_type_destroy(&dataType);

    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 2, &value), RiaGSuccess);
    riag_value_get_data_type(&value, &dataType);
    ASSERT_EQ(riag_data_type_get_id(&dataType), RIAG_LIST);
    riag_data_type_destroy(&dataType);
    riag_value_destroy(&value);

    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, GetBool) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.isStudent ORDER BY a.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    bool boolValue;
    ASSERT_EQ(riag_value_get_bool(&value, &boolValue), RiaGSuccess);
    ASSERT_TRUE(boolValue);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_bool(badValue, &boolValue), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInt8) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.level ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    int8_t int8Value;
    ASSERT_EQ(riag_value_get_int8(&value, &int8Value), RiaGSuccess);
    ASSERT_EQ(int8Value, 5);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_int8(badValue, &int8Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInt16) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.length ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    int16_t int16Value;
    ASSERT_EQ(riag_value_get_int16(&value, &int16Value), RiaGSuccess);
    ASSERT_EQ(int16Value, 5);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_int16(badValue, &int16Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInt32) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (m:movies) RETURN m.length ORDER BY m.name", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    int32_t int32Value;
    ASSERT_EQ(riag_value_get_int32(&value, &int32Value), RiaGSuccess);
    ASSERT_EQ(int32Value, 298);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_int32(badValue, &int32Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInt64) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a.ID ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    int64_t int64Value;
    ASSERT_EQ(riag_value_get_int64(&value, &int64Value), RiaGSuccess);
    ASSERT_EQ(int64Value, 0);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_int64(badValue, &int64Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetUInt8) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.ulevel ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint8_t uint8Value;
    ASSERT_EQ(riag_value_get_uint8(&value, &uint8Value), RiaGSuccess);
    ASSERT_EQ(uint8Value, 250);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_uint8(badValue, &uint8Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetUInt16) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.ulength ORDER BY "
               "a.ID",
        &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint16_t uint16Value;
    ASSERT_EQ(riag_value_get_uint16(&value, &uint16Value), RiaGSuccess);
    ASSERT_EQ(uint16Value, 33768);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_uint16(badValue, &uint16Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetUInt32) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) "
               "RETURN r.temperature ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint32_t uint32Value;
    ASSERT_EQ(riag_value_get_uint32(&value, &uint32Value), RiaGSuccess);
    ASSERT_EQ(uint32Value, 32800);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_uint32(badValue, &uint32Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetUInt64) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.code ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint64_t uint64Value;
    ASSERT_EQ(riag_value_get_uint64(&value, &uint64Value), RiaGSuccess);
    ASSERT_EQ(uint64Value, 9223372036854775808ull);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_uint64(badValue, &uint64Value), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInt128) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:studyAt]-> (b:organisation) RETURN r.hugedata ORDER BY "
               "a.ID",
        &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    riag_int128_t int128;
    ASSERT_EQ(riag_value_get_int128(&value, &int128), RiaGSuccess);
    ASSERT_EQ(int128.high, 100000000);
    ASSERT_EQ(int128.low, 211111111);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_int128(badValue, &int128), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, StringToInt128Test) {
    char input[] = "1844674407370955161811111111";
    riag_int128_t int128_val;
    ASSERT_EQ(riag_int128_t_from_string(input, &int128_val), RiaGSuccess);
    ASSERT_EQ(int128_val.high, 100000000);
    ASSERT_EQ(int128_val.low, 211111111);

    char badInput[] = "this is not a int128";
    riag_int128_t int128_val2;
    ASSERT_EQ(riag_int128_t_from_string(badInput, &int128_val2), RiaGError);
}

TEST_F(CApiValueTest, Int128ToStringTest) {
    auto int128_val = riag_int128_t{211111111, 100000000};
    char* str;
    ASSERT_EQ(riag_int128_t_to_string(int128_val, &str), RiaGSuccess);
    ASSERT_STREQ(str, "1844674407370955161811111111");
    riag_destroy_string(str);
}

TEST_F(CApiValueTest, GetFloat) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.height ORDER BY a.ID", &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    float floatValue;
    ASSERT_EQ(riag_value_get_float(&value, &floatValue), RiaGSuccess);
    ASSERT_FLOAT_EQ(floatValue, 1.731);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_float(badValue, &floatValue), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetDouble) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.eyeSight ORDER BY a.ID", &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    double doubleValue;
    ASSERT_EQ(riag_value_get_double(&value, &doubleValue), RiaGSuccess);
    ASSERT_DOUBLE_EQ(doubleValue, 5.0);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_double(badValue, &doubleValue), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInternalID) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a ORDER BY a.ID",
        &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    riag_value nodeIDVal;
    ASSERT_EQ(riag_value_get_struct_field_value(&value, 0 /* internal ID field idx */, &nodeIDVal),
        RiaGSuccess);
    riag_internal_id_t internalID;
    ASSERT_EQ(riag_value_get_internal_id(&nodeIDVal, &internalID), RiaGSuccess);
    ASSERT_EQ(internalID.table_id, 0);
    ASSERT_EQ(internalID.offset, 0);
    riag_value_destroy(&nodeIDVal);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_internal_id(badValue, &internalID), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetRelVal) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[r:knows]-> (b:person) RETURN r ORDER BY a.ID, b.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value rel;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &rel), RiaGSuccess);
    ASSERT_TRUE(rel._is_owned_by_cpp);
    riag_value relIdVal;
    ASSERT_EQ(riag_rel_val_get_id_val(&rel, &relIdVal), RiaGSuccess);
    riag_internal_id_t relInternalID;
    ASSERT_EQ(riag_value_get_internal_id(&relIdVal, &relInternalID), RiaGSuccess);
    ASSERT_EQ(relInternalID.table_id, 3);
    ASSERT_EQ(relInternalID.offset, 0);
    riag_value relSrcIDVal;
    ASSERT_EQ(riag_rel_val_get_src_id_val(&rel, &relSrcIDVal), RiaGSuccess);
    riag_internal_id_t relSrcID;
    ASSERT_EQ(riag_value_get_internal_id(&relSrcIDVal, &relSrcID), RiaGSuccess);
    ASSERT_EQ(relSrcID.table_id, 0);
    ASSERT_EQ(relSrcID.offset, 0);
    riag_value relDstIDVal;
    ASSERT_EQ(riag_rel_val_get_dst_id_val(&rel, &relDstIDVal), RiaGSuccess);
    riag_internal_id_t relDstID;
    ASSERT_EQ(riag_value_get_internal_id(&relDstIDVal, &relDstID), RiaGSuccess);
    ASSERT_EQ(relDstID.table_id, 0);
    ASSERT_EQ(relDstID.offset, 1);
    riag_value relLabel;
    ASSERT_EQ(riag_rel_val_get_label_val(&rel, &relLabel), RiaGSuccess);
    char* relLabelStr;
    ASSERT_EQ(riag_value_get_string(&relLabel, &relLabelStr), RiaGSuccess);
    ASSERT_STREQ(relLabelStr, "knows");
    uint64_t propertiesSize;
    ASSERT_EQ(riag_rel_val_get_property_size(&rel, &propertiesSize), RiaGSuccess);
    ASSERT_EQ(propertiesSize, 7);
    riag_destroy_string(relLabelStr);
    riag_value_destroy(&relLabel);
    riag_value_destroy(&relIdVal);
    riag_value_destroy(&relSrcIDVal);
    riag_value_destroy(&relDstIDVal);
    riag_value_destroy(&rel);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_rel_val_get_src_id_val(badValue, &relSrcIDVal), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetDate) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.birthdate ORDER BY a.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    riag_date_t date;
    ASSERT_EQ(riag_value_get_date(&value, &date), RiaGSuccess);
    ASSERT_EQ(date.days, -25567);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_date(badValue, &date), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetTimestamp) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.registerTime ORDER BY a.ID", &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    riag_timestamp_t timestamp;
    ASSERT_EQ(riag_value_get_timestamp(&value, &timestamp), RiaGSuccess);
    ASSERT_EQ(timestamp.value, 1313839530000000);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_timestamp(badValue, &timestamp), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetInterval) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.lastJobDuration ORDER BY a.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    riag_interval_t interval;
    ASSERT_EQ(riag_value_get_interval(&value, &interval), RiaGSuccess);
    ASSERT_EQ(interval.months, 36);
    ASSERT_EQ(interval.days, 2);
    ASSERT_EQ(interval.micros, 46920000000);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_interval(badValue, &interval), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetString) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.fName ORDER BY a.ID", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    char* str;
    ASSERT_EQ(riag_value_get_string(&value, &str), RiaGSuccess);
    ASSERT_STREQ(str, "Alice");
    riag_destroy_string(str);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_int32(123);
    ASSERT_EQ(riag_value_get_string(badValue, &str), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetBlob) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state =
        riag_connection_query(connection, (char*)R"(RETURN BLOB('\xAA\xBB\xCD\x1A');)", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    uint8_t* blob;
    uint64_t length;
    ASSERT_EQ(riag_value_get_blob(&value, &blob, &length), RiaGSuccess);
    ASSERT_EQ(length, 4);
    ASSERT_EQ(blob[0], 0xAA);
    ASSERT_EQ(blob[1], 0xBB);
    ASSERT_EQ(blob[2], 0xCD);
    ASSERT_EQ(blob[3], 0x1A);
    riag_destroy_blob(blob);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_blob(badValue, &blob, &length), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetUUID) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)R"(RETURN UUID("A0EEBC99-9C0B-4EF8-BB6D-6BB9BD380A11");)", &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value value;
    riag_flat_tuple_get_value(&flatTuple, 0, &value);
    ASSERT_TRUE(value._is_owned_by_cpp);
    ASSERT_FALSE(riag_value_is_null(&value));
    char* str;
    ASSERT_EQ(riag_value_get_uuid(&value, &str), RiaGSuccess);
    ASSERT_STREQ(str, "a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11");
    riag_destroy_string(str);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_value_get_uuid(badValue, &str), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, ToSting) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) RETURN a.fName, a.isStudent, a.workedHours ORDER BY "
               "a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));

    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);

    riag_value value;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &value), RiaGSuccess);
    char* str = riag_value_to_string(&value);
    ASSERT_STREQ(str, "Alice");
    riag_destroy_string(str);

    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 1, &value), RiaGSuccess);
    str = riag_value_to_string(&value);
    ASSERT_STREQ(str, "True");
    riag_destroy_string(str);

    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 2, &value), RiaGSuccess);
    str = riag_value_to_string(&value);
    ASSERT_STREQ(str, "[10,5]");
    riag_destroy_string(str);
    riag_value_destroy(&value);

    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, NodeValGetLabelVal) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));

    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value nodeVal;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &nodeVal), RiaGSuccess);
    riag_value labelVal;
    ASSERT_EQ(riag_node_val_get_label_val(&nodeVal, &labelVal), RiaGSuccess);
    char* labelStr;
    ASSERT_EQ(riag_value_get_string(&labelVal, &labelStr), RiaGSuccess);
    ASSERT_STREQ(labelStr, "person");
    riag_destroy_string(labelStr);
    riag_value_destroy(&labelVal);
    riag_value_destroy(&nodeVal);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_node_val_get_label_val(badValue, &labelVal), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, NodeValGetID) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));

    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value nodeVal;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &nodeVal), RiaGSuccess);
    riag_value nodeIDVal;
    ASSERT_EQ(riag_node_val_get_id_val(&nodeVal, &nodeIDVal), RiaGSuccess);
    ASSERT_NE(nodeIDVal._value, nullptr);
    riag_internal_id_t internalID;
    ASSERT_EQ(riag_value_get_internal_id(&nodeIDVal, &internalID), RiaGSuccess);
    ASSERT_EQ(internalID.table_id, 0);
    ASSERT_EQ(internalID.offset, 0);
    riag_value_destroy(&nodeIDVal);
    riag_value_destroy(&nodeVal);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_node_val_get_id_val(badValue, &nodeIDVal), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, NodeValGetLabelName) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));

    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value nodeVal;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &nodeVal), RiaGSuccess);
    riag_value labelVal;
    ASSERT_EQ(riag_node_val_get_label_val(&nodeVal, &labelVal), RiaGSuccess);
    char* labelStr;
    ASSERT_EQ(riag_value_get_string(&labelVal, &labelStr), RiaGSuccess);
    ASSERT_STREQ(labelStr, "person");
    riag_destroy_string(labelStr);
    riag_value_destroy(&labelVal);
    riag_value_destroy(&nodeVal);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_node_val_get_label_val(badValue, &labelVal), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, NodeValGetProperty) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection, (char*)"MATCH (a:person) RETURN a ORDER BY a.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value node;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &node), RiaGSuccess);
    char* propertyName;
    ASSERT_EQ(riag_node_val_get_property_name_at(&node, 0, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "ID");
    riag_destroy_string(propertyName);
    ASSERT_EQ(riag_node_val_get_property_name_at(&node, 1, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "fName");
    riag_destroy_string(propertyName);
    ASSERT_EQ(riag_node_val_get_property_name_at(&node, 2, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "gender");
    riag_destroy_string(propertyName);
    ASSERT_EQ(riag_node_val_get_property_name_at(&node, 3, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "isStudent");
    riag_destroy_string(propertyName);

    riag_value propertyValue;
    ASSERT_EQ(riag_node_val_get_property_value_at(&node, 0, &propertyValue), RiaGSuccess);
    int64_t propertyValueID;
    ASSERT_EQ(riag_value_get_int64(&propertyValue, &propertyValueID), RiaGSuccess);
    ASSERT_EQ(propertyValueID, 0);
    ASSERT_EQ(riag_node_val_get_property_value_at(&node, 1, &propertyValue), RiaGSuccess);
    char* propertyValuefName;
    ASSERT_EQ(riag_value_get_string(&propertyValue, &propertyValuefName), RiaGSuccess);
    ASSERT_STREQ(propertyValuefName, "Alice");
    riag_destroy_string(propertyValuefName);
    ASSERT_EQ(riag_node_val_get_property_value_at(&node, 2, &propertyValue), RiaGSuccess);
    int64_t propertyValueGender;
    ASSERT_EQ(riag_value_get_int64(&propertyValue, &propertyValueGender), RiaGSuccess);
    ASSERT_EQ(propertyValueGender, 1);
    ASSERT_EQ(riag_node_val_get_property_value_at(&node, 3, &propertyValue), RiaGSuccess);
    bool propertyValueIsStudent;
    ASSERT_EQ(riag_value_get_bool(&propertyValue, &propertyValueIsStudent), RiaGSuccess);
    ASSERT_EQ(propertyValueIsStudent, true);
    riag_value_destroy(&propertyValue);

    riag_value_destroy(&node);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_node_val_get_property_name_at(badValue, 0, &propertyName), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, NodeValToString) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (b:organisation) RETURN b ORDER BY b.ID", &result);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value node;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &node), RiaGSuccess);
    ASSERT_TRUE(node._is_owned_by_cpp);

    char* str = riag_value_to_string(&node);
    ASSERT_STREQ(str,
        "{_ID: 1:0, _LABEL: organisation, ID: 1, name: ABFsUni, orgCode: 325, mark: 3.700000, "
        "score: -2, history: 10 years 5 months 13 hours 24 us, licenseValidInterval: 3 years "
        "5 days, rating: 1.000000, state: {revenue: 138, location: ['toronto','montr,eal'], "
        "stock: {price: [96,56], volume: 1000}}, info: 3.120000}");
    riag_destroy_string(str);

    riag_value_destroy(&node);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);
}

TEST_F(CApiValueTest, RelValGetProperty) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[e:workAt]-> (b:organisation) RETURN e ORDER BY a.ID, b.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value rel;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &rel), RiaGSuccess);
    ASSERT_TRUE(rel._is_owned_by_cpp);
    uint64_t propertiesSize;
    ASSERT_EQ(riag_rel_val_get_property_size(&rel, &propertiesSize), RiaGSuccess);
    ASSERT_EQ(propertiesSize, 3);

    char* propertyName;
    ASSERT_EQ(riag_rel_val_get_property_name_at(&rel, 0, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "year");
    riag_destroy_string(propertyName);

    ASSERT_EQ(riag_rel_val_get_property_name_at(&rel, 1, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "grading");
    riag_destroy_string(propertyName);
    ASSERT_EQ(riag_rel_val_get_property_name_at(&rel, 2, &propertyName), RiaGSuccess);
    ASSERT_STREQ(propertyName, "rating");
    riag_destroy_string(propertyName);

    riag_value propertyValue;
    ASSERT_EQ(riag_rel_val_get_property_value_at(&rel, 0, &propertyValue), RiaGSuccess);
    int64_t propertyValueYear;
    ASSERT_EQ(riag_value_get_int64(&propertyValue, &propertyValueYear), RiaGSuccess);
    ASSERT_EQ(propertyValueYear, 2015);

    ASSERT_EQ(riag_rel_val_get_property_value_at(&rel, 1, &propertyValue), RiaGSuccess);
    riag_value listValue;
    ASSERT_EQ(riag_value_get_list_element(&propertyValue, 0, &listValue), RiaGSuccess);
    double listValueGrading;
    ASSERT_EQ(riag_value_get_double(&listValue, &listValueGrading), RiaGSuccess);
    ASSERT_DOUBLE_EQ(listValueGrading, 3.8);
    ASSERT_EQ(riag_value_get_list_element(&propertyValue, 1, &listValue), RiaGSuccess);
    ASSERT_EQ(riag_value_get_double(&listValue, &listValueGrading), RiaGSuccess);
    ASSERT_DOUBLE_EQ(listValueGrading, 2.5);
    riag_value_destroy(&listValue);

    ASSERT_EQ(riag_rel_val_get_property_value_at(&rel, 2, &propertyValue), RiaGSuccess);
    float propertyValueRating;
    ASSERT_EQ(riag_value_get_float(&propertyValue, &propertyValueRating), RiaGSuccess);
    ASSERT_FLOAT_EQ(propertyValueRating, 8.2);
    riag_value_destroy(&propertyValue);

    riag_value_destroy(&rel);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_rel_val_get_property_name_at(badValue, 0, &propertyName), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, RelValToString) {
    riag_query_result result;
    riag_flat_tuple flatTuple;
    riag_state state;
    auto connection = getConnection();
    state = riag_connection_query(connection,
        (char*)"MATCH (a:person) -[e:workAt]-> (b:organisation) RETURN e ORDER BY a.ID, b.ID",
        &result);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&result));
    ASSERT_TRUE(riag_query_result_has_next(&result));
    state = riag_query_result_get_next(&result, &flatTuple);
    ASSERT_EQ(state, RiaGSuccess);
    riag_value rel;
    ASSERT_EQ(riag_flat_tuple_get_value(&flatTuple, 0, &rel), RiaGSuccess);
    ASSERT_TRUE(rel._is_owned_by_cpp);
    char* str;
    ASSERT_EQ(riag_rel_val_to_string(&rel, &str), RiaGSuccess);
    ASSERT_STREQ(str, "(0:2)-{_LABEL: workAt, _ID: 7:0, year: 2015, grading: [3.800000,2.500000], "
                      "rating: 8.200000}->(1:1)");
    riag_destroy_string(str);
    riag_value_destroy(&rel);
    riag_flat_tuple_destroy(&flatTuple);
    riag_query_result_destroy(&result);

    riag_value* badValue = riag_value_create_string((char*)"abcdefg");
    ASSERT_EQ(riag_rel_val_to_string(badValue, &str), RiaGError);
    riag_value_destroy(badValue);
}

TEST_F(CApiValueTest, GetTmFromNonStandardTimestamp) {
    riag_timestamp_ns_t timestamp_ns = riag_timestamp_ns_t{17515323532900000};
    riag_timestamp_ms_t timestamp_ms = riag_timestamp_ms_t{1012323435341};
    riag_timestamp_sec_t timestamp_sec = riag_timestamp_sec_t{1432135648};
    riag_timestamp_tz_t timestamp_tz = riag_timestamp_tz_t{771513532900000};
    struct tm tm;
    ASSERT_EQ(riag_timestamp_ns_to_tm(timestamp_ns, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 70);
    ASSERT_EQ(tm.tm_mon, 6);
    ASSERT_EQ(tm.tm_mday, 22);
    ASSERT_EQ(tm.tm_hour, 17);
    ASSERT_EQ(tm.tm_min, 22);
    ASSERT_EQ(tm.tm_sec, 3);
    ASSERT_EQ(riag_timestamp_ms_to_tm(timestamp_ms, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 102);
    ASSERT_EQ(tm.tm_mon, 0);
    ASSERT_EQ(tm.tm_mday, 29);
    ASSERT_EQ(tm.tm_hour, 16);
    ASSERT_EQ(tm.tm_min, 57);
    ASSERT_EQ(tm.tm_sec, 15);
    ASSERT_EQ(riag_timestamp_sec_to_tm(timestamp_sec, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 115);
    ASSERT_EQ(tm.tm_mon, 4);
    ASSERT_EQ(tm.tm_mday, 20);
    ASSERT_EQ(tm.tm_hour, 15);
    ASSERT_EQ(tm.tm_min, 27);
    ASSERT_EQ(tm.tm_sec, 28);
    ASSERT_EQ(riag_timestamp_tz_to_tm(timestamp_tz, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 94);
    ASSERT_EQ(tm.tm_mon, 5);
    ASSERT_EQ(tm.tm_mday, 13);
    ASSERT_EQ(tm.tm_hour, 13);
    ASSERT_EQ(tm.tm_min, 18);
    ASSERT_EQ(tm.tm_sec, 52);
}

TEST_F(CApiValueTest, GetTmFromTimestamp) {
    riag_timestamp_t timestamp = riag_timestamp_t{171513532900000};
    struct tm tm;
    ASSERT_EQ(riag_timestamp_to_tm(timestamp, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 75);
    ASSERT_EQ(tm.tm_mon, 5);
    ASSERT_EQ(tm.tm_mday, 9);
    ASSERT_EQ(tm.tm_hour, 2);
    ASSERT_EQ(tm.tm_min, 38);
    ASSERT_EQ(tm.tm_sec, 52);
}

TEST_F(CApiValueTest, GetTmFromDate) {
    riag_date_t date = riag_date_t{-255};
    struct tm tm;
    ASSERT_EQ(riag_date_to_tm(date, &tm), RiaGSuccess);
    ASSERT_EQ(tm.tm_year, 69);
    ASSERT_EQ(tm.tm_mon, 3);
    ASSERT_EQ(tm.tm_mday, 21);
    ASSERT_EQ(tm.tm_hour, 0);
    ASSERT_EQ(tm.tm_min, 0);
    ASSERT_EQ(tm.tm_sec, 0);
}

TEST_F(CApiValueTest, GetTimestampFromTm) {
    struct tm tm;
    tm.tm_year = 75;
    tm.tm_mon = 5;
    tm.tm_mday = 9;
    tm.tm_hour = 2;
    tm.tm_min = 38;
    tm.tm_sec = 52;
    riag_timestamp_t timestamp;
    ASSERT_EQ(riag_timestamp_from_tm(tm, &timestamp), RiaGSuccess);
    ASSERT_EQ(timestamp.value, 171513532000000);
}

TEST_F(CApiValueTest, GetNonStandardTimestampFromTm) {
    struct tm tm;
    tm.tm_year = 70;
    tm.tm_mon = 6;
    tm.tm_mday = 22;
    tm.tm_hour = 17;
    tm.tm_min = 22;
    tm.tm_sec = 3;
    riag_timestamp_ns_t timestamp_ns;
    ASSERT_EQ(riag_timestamp_ns_from_tm(tm, &timestamp_ns), RiaGSuccess);
    ASSERT_EQ(timestamp_ns.value, 17515323000000000);
    tm.tm_year = 102;
    tm.tm_mon = 0;
    tm.tm_mday = 29;
    tm.tm_hour = 16;
    tm.tm_min = 57;
    tm.tm_sec = 15;
    riag_timestamp_ms_t timestamp_ms;
    ASSERT_EQ(riag_timestamp_ms_from_tm(tm, &timestamp_ms), RiaGSuccess);
    ASSERT_EQ(timestamp_ms.value, 1012323435000);
    tm.tm_year = 115;
    tm.tm_mon = 4;
    tm.tm_mday = 20;
    tm.tm_hour = 15;
    tm.tm_min = 27;
    tm.tm_sec = 28;
    riag_timestamp_sec_t timestamp_sec;
    ASSERT_EQ(riag_timestamp_sec_from_tm(tm, &timestamp_sec), RiaGSuccess);
    ASSERT_EQ(timestamp_sec.value, 1432135648);
    tm.tm_year = 94;
    tm.tm_mon = 5;
    tm.tm_mday = 13;
    tm.tm_hour = 13;
    tm.tm_min = 18;
    tm.tm_sec = 52;
    riag_timestamp_tz_t timestamp_tz;
    ASSERT_EQ(riag_timestamp_tz_from_tm(tm, &timestamp_tz), RiaGSuccess);
    ASSERT_EQ(timestamp_tz.value, 771513532000000);
}

TEST_F(CApiValueTest, GetDateFromTm) {
    struct tm tm;
    tm.tm_year = 69;
    tm.tm_mon = 3;
    tm.tm_mday = 21;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    riag_date_t date;
    ASSERT_EQ(riag_date_from_tm(tm, &date), RiaGSuccess);
    ASSERT_EQ(date.days, -255);
}

TEST_F(CApiValueTest, GetDateFromString) {
    char input[] = "1969-04-21";
    riag_date_t date;
    ASSERT_EQ(riag_date_from_string(input, &date), RiaGSuccess);
    ASSERT_EQ(date.days, -255);

    char badInput[] = "this is not a date";
    ASSERT_EQ(riag_date_from_string(badInput, &date), RiaGError);
}

TEST_F(CApiValueTest, GetStringFromDate) {
    riag_date_t date = riag_date_t{-255};
    char* str;
    ASSERT_EQ(riag_date_to_string(date, &str), RiaGSuccess);
    ASSERT_STREQ(str, "1969-04-21");
    riag_destroy_string(str);
}

TEST_F(CApiValueTest, GetDifftimeFromInterval) {
    riag_interval_t interval = riag_interval_t{36, 2, 46920000000};
    double difftime;
    riag_interval_to_difftime(interval, &difftime);
    ASSERT_DOUBLE_EQ(difftime, 93531720);
}

TEST_F(CApiValueTest, GetIntervalFromDifftime) {
    double difftime = 211110160.479;
    riag_interval_t interval;
    riag_interval_from_difftime(difftime, &interval);
    ASSERT_EQ(interval.months, 81);
    ASSERT_EQ(interval.days, 13);
    ASSERT_EQ(interval.micros, 34960479000);
}

TEST(CApiValueTestEmptyDB, NullHandleReturnsErrorNotCrash) {
    // The C API exception floor: null or empty handles come back as errors, never as
    // dereferences of nullptr crossing the C boundary.
    int64_t int_out = 42;
    ASSERT_EQ(riag_value_get_int64(nullptr, &int_out), RiaGError);
    uint64_t size_out = 42;
    ASSERT_EQ(riag_value_get_list_size(nullptr, &size_out), RiaGError);
    ASSERT_EQ(riag_value_clone(nullptr), nullptr);
    ASSERT_EQ(riag_value_to_string(nullptr), nullptr);
    ASSERT_EQ(riag_value_create_null_with_data_type(nullptr), nullptr);
}
