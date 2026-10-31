#include "common/types/value/value.h"

#include "c_api/helpers.h"
#include "c_api/riag.h"
#include "common/constants.h"
#include "common/type_utils.h"
#include "common/types/types.h"
#include "common/types/value/nested.h"
#include "common/types/value/node.h"
#include "common/types/value/recursive_rel.h"
#include "common/types/value/rel.h"
#include "function/cast/functions/cast_from_string_functions.h"

using namespace riag::common;

static bool isEmptyNestedWithAny(const Value& value) {
    if (!value.getDataType().containsAny()) {
        return false;
    }
    switch (value.getDataType().getPhysicalType()) {
    case PhysicalTypeID::LIST:
    case PhysicalTypeID::ARRAY:
    case PhysicalTypeID::STRUCT:
        return NestedVal::getChildrenSize(const_cast<Value*>(&value)) == 0;
    default:
        return false;
    }
}

static bool tryGetMaxParameterType(const LogicalType& left, const LogicalType& right,
    LogicalType& result) {
    if (left.getLogicalTypeID() == LogicalTypeID::STRING ||
        right.getLogicalTypeID() == LogicalTypeID::STRING) {
        result = LogicalType::STRING();
        return true;
    }
    return LogicalTypeUtils::tryGetMaxLogicalType(left, right, result);
}

static bool canCopyValueForNested(const Value& value, const LogicalType& targetType) {
    if (value.getDataType() == targetType) {
        return true;
    }
    if (value.isNull()) {
        return true;
    }
    if (isEmptyNestedWithAny(value)) {
        return true;
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::STRING ||
        targetType.getLogicalTypeID() == LogicalTypeID::DOUBLE ||
        targetType.getLogicalTypeID() == LogicalTypeID::FLOAT) {
        return true;
    }
    if (LogicalTypeUtils::isIntegral(value.getDataType()) &&
        LogicalTypeUtils::isIntegral(targetType)) {
        return true;
    }
    if (value.getDataType().getLogicalTypeID() != targetType.getLogicalTypeID()) {
        return false;
    }
    const auto* nested = &value;
    if (targetType.getLogicalTypeID() == LogicalTypeID::LIST) {
        const auto& childType = ListType::getChildType(targetType);
        for (auto i = 0u; i < NestedVal::getChildrenSize(nested); ++i) {
            if (!canCopyValueForNested(*NestedVal::getChildVal(nested, i), childType)) {
                return false;
            }
        }
        return true;
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::MAP) {
        const auto& keyType = MapType::getKeyType(targetType);
        const auto& valueType = MapType::getValueType(targetType);
        for (auto i = 0u; i < NestedVal::getChildrenSize(nested); ++i) {
            auto* mapEntry = NestedVal::getChildVal(nested, i);
            if (!canCopyValueForNested(*NestedVal::getChildVal(mapEntry, 0), keyType) ||
                !canCopyValueForNested(*NestedVal::getChildVal(mapEntry, 1), valueType)) {
                return false;
            }
        }
        return true;
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::STRUCT) {
        for (auto i = 0u; i < NestedVal::getChildrenSize(nested); ++i) {
            if (!canCopyValueForNested(*NestedVal::getChildVal(nested, i),
                    StructType::getFieldType(targetType, static_cast<struct_field_idx_t>(i)))) {
                return false;
            }
        }
        return true;
    }
    return value.getDataType().containsAny();
}

static std::string incompatibleParameterTypeMessage(const LogicalType& left,
    const LogicalType& right) {
    return std::format("Runtime exception: Cannot convert Python object to RiaG value : {}  is "
                       "incompatible with {}",
        left.toString(), right.toString());
}

static riag_value* createCAPIValue(std::unique_ptr<Value> value) {
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = value.release();
    c_value->_is_owned_by_cpp = false;
    return c_value;
}

// The target comes from LogicalTypeUtils::tryGetMaxLogicalType, which joins two integer types only
// into one that holds both: implicit integer casts only widen, and a signed/unsigned pair becomes a
// wider signed type. So the cast below is exact. CreateListAllIntegerPairs pins this.
static std::unique_ptr<Value> castIntegerForNested(const Value& value,
    const LogicalType& targetType) {
    // A null of the target type keeps its exact type: SERIAL's default value is an INT64.
    auto result = std::make_unique<Value>(Value::createNullValue(targetType));
    result->setNull(false);
    TypeUtils::visit(
        value.getDataType(),
        [&]<IntegerTypes SRC>(SRC) {
            TypeUtils::visit(
                targetType,
                [&]<IntegerTypes DST>(DST) {
                    auto castValue = static_cast<DST>(value.getValue<SRC>());
                    result->copyFromRowLayout(reinterpret_cast<const uint8_t*>(&castValue));
                },
                [](auto) { UNREACHABLE_CODE; });
        },
        [](auto) { UNREACHABLE_CODE; });
    return result;
}

static std::unique_ptr<Value> copyValueForNested(const Value& value,
    const LogicalType& targetType) {
    if (value.getDataType() == targetType) {
        return value.copy();
    }
    if (value.isNull()) {
        return std::make_unique<Value>(Value::createNullValue(targetType));
    }
    if (isEmptyNestedWithAny(value)) {
        return std::make_unique<Value>(Value::createDefaultValue(targetType));
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::STRING) {
        return std::make_unique<Value>(Value::createValue<std::string>(value.toString()));
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::DOUBLE) {
        return std::make_unique<Value>(std::stod(value.toString()));
    }
    if (targetType.getLogicalTypeID() == LogicalTypeID::FLOAT) {
        return std::make_unique<Value>(static_cast<float>(std::stof(value.toString())));
    }
    if (LogicalTypeUtils::isIntegral(value.getDataType()) &&
        LogicalTypeUtils::isIntegral(targetType)) {
        return castIntegerForNested(value, targetType);
    }
    if (value.getDataType().getLogicalTypeID() == LogicalTypeID::LIST &&
        targetType.getLogicalTypeID() == LogicalTypeID::LIST) {
        std::vector<std::unique_ptr<Value>> children;
        const auto& childType = ListType::getChildType(targetType);
        for (auto i = 0u; i < NestedVal::getChildrenSize(const_cast<Value*>(&value)); ++i) {
            children.push_back(copyValueForNested(
                *NestedVal::getChildVal(const_cast<Value*>(&value), i), childType));
        }
        return std::make_unique<Value>(targetType.copy(), std::move(children));
    }
    if (value.getDataType().getLogicalTypeID() == LogicalTypeID::MAP &&
        targetType.getLogicalTypeID() == LogicalTypeID::MAP) {
        std::vector<std::unique_ptr<Value>> children;
        const auto& keyType = MapType::getKeyType(targetType);
        const auto& valueType = MapType::getValueType(targetType);
        for (auto i = 0u; i < NestedVal::getChildrenSize(const_cast<Value*>(&value)); ++i) {
            auto* mapEntry = NestedVal::getChildVal(const_cast<Value*>(&value), i);
            std::vector<StructField> fields;
            fields.emplace_back(InternalKeyword::MAP_KEY, keyType.copy());
            fields.emplace_back(InternalKeyword::MAP_VALUE, valueType.copy());
            std::vector<std::unique_ptr<Value>> structValues;
            structValues.push_back(
                copyValueForNested(*NestedVal::getChildVal(mapEntry, 0), keyType));
            structValues.push_back(
                copyValueForNested(*NestedVal::getChildVal(mapEntry, 1), valueType));
            children.push_back(std::make_unique<Value>(LogicalType::STRUCT(std::move(fields)),
                std::move(structValues)));
        }
        return std::make_unique<Value>(targetType.copy(), std::move(children));
    }
    if (value.getDataType().getLogicalTypeID() == LogicalTypeID::STRUCT &&
        targetType.getLogicalTypeID() == LogicalTypeID::STRUCT) {
        std::vector<std::unique_ptr<Value>> children;
        for (auto i = 0u; i < NestedVal::getChildrenSize(&value); ++i) {
            children.push_back(copyValueForNested(*NestedVal::getChildVal(&value, i),
                StructType::getFieldType(targetType, static_cast<struct_field_idx_t>(i))));
        }
        return std::make_unique<Value>(targetType.copy(), std::move(children));
    }
    return value.copy();
}

riag_value* riag_value_create_null() {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(Value::createNullValue());
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_null_with_data_type(riag_logical_type* data_type) {
    if (data_type == nullptr || data_type->_data_type == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value =
        new Value(Value::createNullValue(*static_cast<LogicalType*>(data_type->_data_type)));
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

bool riag_value_is_null(riag_value* value) {
    if (value == nullptr || value->_value == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<Value*>(value->_value)->isNull();
    RIAG_C_API_GUARD_END(false)
}

void riag_value_set_null(riag_value* value, bool is_null) {
    if (value == nullptr || value->_value == nullptr) {
        return;
    }
    RIAG_C_API_GUARD_BEGIN
    static_cast<Value*>(value->_value)->setNull(is_null);
    RIAG_C_API_GUARD_END_VOID
}

riag_value* riag_value_create_default(riag_logical_type* data_type) {
    if (data_type == nullptr || data_type->_data_type == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value =
        new Value(Value::createDefaultValue(*static_cast<LogicalType*>(data_type->_data_type)));
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_bool(bool val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_int8(int8_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_int16(int16_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_int32(int32_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_int64(int64_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_uint8(uint8_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_uint16(uint16_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_uint32(uint32_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_uint64(uint64_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_int128(riag_int128_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    int128_t int128(val_.low, val_.high);
    c_value->_value = new Value(int128);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_float(float val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_double(double val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_decimal(const char* val_, uint32_t precision, uint32_t scale) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto decimalType = LogicalType::DECIMAL(precision, scale);
    auto value = Value::createDefaultValue(decimalType);
    switch (decimalType.getPhysicalType()) {
    case PhysicalTypeID::INT16:
        riag::function::decimalCast(val_, strlen(val_), value.val.int16Val, decimalType);
        break;
    case PhysicalTypeID::INT32:
        riag::function::decimalCast(val_, strlen(val_), value.val.int32Val, decimalType);
        break;
    case PhysicalTypeID::INT64:
        riag::function::decimalCast(val_, strlen(val_), value.val.int64Val, decimalType);
        break;
    case PhysicalTypeID::INT128:
        riag::function::decimalCast(val_, strlen(val_), value.val.int128Val, decimalType);
        break;
    default:
        free(c_value);
        return nullptr;
    }
    c_value->_value = new Value(std::move(value));
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_internal_id(riag_internal_id_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    internalID_t id(val_.offset, val_.table_id);
    c_value->_value = new Value(id);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_date(riag_date_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto date = date_t(val_.days);
    c_value->_value = new Value(date);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_timestamp_ns(riag_timestamp_ns_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto timestamp_ns = timestamp_ns_t(val_.value);
    c_value->_value = new Value(timestamp_ns);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_timestamp_ms(riag_timestamp_ms_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto timestamp_ms = timestamp_ms_t(val_.value);
    c_value->_value = new Value(timestamp_ms);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_timestamp_sec(riag_timestamp_sec_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto timestamp_sec = timestamp_sec_t(val_.value);
    c_value->_value = new Value(timestamp_sec);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_timestamp_tz(riag_timestamp_tz_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto timestamp_tz = timestamp_tz_t(val_.value);
    c_value->_value = new Value(timestamp_tz);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_timestamp(riag_timestamp_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto timestamp = timestamp_t(val_.value);
    c_value->_value = new Value(timestamp);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_interval(riag_interval_t val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    auto interval = interval_t(val_.months, val_.days, val_.micros);
    c_value->_value = new Value(interval);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_string(const char* val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_json(const char* val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(LogicalType::JSON(), val_);
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_uuid(const char* val_) {
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value =
        new Value(riag::common::uuid{riag::common::UUID::fromCString(val_, strlen(val_))});
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_value* riag_value_create_blob(const uint8_t* data, uint64_t length) {
    RIAG_C_API_GUARD_BEGIN
    if (data == nullptr && length != 0) {
        // A null buffer with a non-zero length would read out of bounds in the length-based
        // std::string construction below, which the exception guard cannot catch.
        return nullptr;
    }
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    std::string blobStr;
    if (data != nullptr) {
        blobStr.assign(reinterpret_cast<const char*>(data), length);
    }
    c_value->_value = new Value(LogicalType::BLOB(), std::move(blobStr));
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

riag_state riag_value_create_list(uint64_t num_elements, riag_value** elements,
    riag_value** out_value) {
    RIAG_C_API_GUARD_BEGIN
    if (out_value == nullptr) {
        return RiaGError;
    }
    if (num_elements == 0) {
        *out_value = createCAPIValue(std::make_unique<Value>(LogicalType::LIST(LogicalType::ANY()),
            std::vector<std::unique_ptr<Value>>{}));
        return RiaGSuccess;
    }
    if (elements == nullptr) {
        return RiaGError;
    }
    std::vector<std::unique_ptr<Value>> children;

    auto first_element = static_cast<Value*>(elements[0]->_value);
    auto type = first_element->getDataType().copy();

    for (uint64_t i = 0; i < num_elements; ++i) {
        auto child = static_cast<Value*>(elements[i]->_value);
        LogicalType resultType;
        if (!tryGetMaxParameterType(type, child->getDataType(), resultType)) {
            setLastCAPIErrorMessage(incompatibleParameterTypeMessage(type, child->getDataType()));
            return RiaGError;
        }
        type = std::move(resultType);
    }
    for (uint64_t i = 0; i < num_elements; ++i) {
        auto child = static_cast<Value*>(elements[i]->_value);
        if (!canCopyValueForNested(*child, type)) {
            setLastCAPIErrorMessage(incompatibleParameterTypeMessage(child->getDataType(), type));
            return RiaGError;
        }
        children.push_back(copyValueForNested(*child, type));
    }
    auto* c_value = createCAPIValue(
        std::make_unique<Value>(LogicalType::LIST(type.copy()), std::move(children)));
    *out_value = c_value;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_create_struct(uint64_t num_fields, const char** field_names,
    riag_value** field_values, riag_value** out_value) {
    RIAG_C_API_GUARD_BEGIN
    if (out_value == nullptr) {
        return RiaGError;
    }
    if (num_fields == 0 || field_names == nullptr || field_values == nullptr) {
        return RiaGError;
    }
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    std::vector<std::unique_ptr<Value>> children;
    auto struct_fields = std::vector<StructField>{};
    for (uint64_t i = 0; i < num_fields; ++i) {
        auto field_name = std::string(field_names[i]);
        auto field_value = static_cast<Value*>(field_values[i]->_value);
        auto field_type = field_value->getDataType().copy();
        struct_fields.emplace_back(std::move(field_name), std::move(field_type));
        children.push_back(field_value->copy());
    }
    auto struct_type = LogicalType::STRUCT(std::move(struct_fields));
    c_value->_value = new Value(std::move(struct_type), std::move(children));
    c_value->_is_owned_by_cpp = false;
    *out_value = c_value;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_create_map(uint64_t num_fields, riag_value** keys, riag_value** values,
    riag_value** out_value) {
    RIAG_C_API_GUARD_BEGIN
    if (out_value == nullptr) {
        return RiaGError;
    }
    if (num_fields == 0) {
        *out_value = createCAPIValue(
            std::make_unique<Value>(LogicalType::MAP(LogicalType::ANY(), LogicalType::ANY()),
                std::vector<std::unique_ptr<Value>>{}));
        return RiaGSuccess;
    }
    if (keys == nullptr || values == nullptr) {
        return RiaGError;
    }
    std::vector<std::unique_ptr<Value>> children;

    auto first_key = static_cast<Value*>(keys[0]->_value);
    auto first_value = static_cast<Value*>(values[0]->_value);
    auto key_type = first_key->getDataType().copy();
    auto value_type = first_value->getDataType().copy();

    for (uint64_t i = 0; i < num_fields; ++i) {
        auto key = static_cast<Value*>(keys[i]->_value);
        auto value = static_cast<Value*>(values[i]->_value);
        LogicalType resultKeyType;
        LogicalType resultValueType;
        if (!tryGetMaxParameterType(key_type, key->getDataType(), resultKeyType)) {
            setLastCAPIErrorMessage(incompatibleParameterTypeMessage(key_type, key->getDataType()));
            return RiaGError;
        }
        if (!tryGetMaxParameterType(value_type, value->getDataType(), resultValueType)) {
            setLastCAPIErrorMessage(
                incompatibleParameterTypeMessage(value_type, value->getDataType()));
            return RiaGError;
        }
        key_type = std::move(resultKeyType);
        value_type = std::move(resultValueType);
    }
    for (uint64_t i = 0; i < num_fields; ++i) {
        auto key = static_cast<Value*>(keys[i]->_value);
        auto value = static_cast<Value*>(values[i]->_value);
        if (!canCopyValueForNested(*key, key_type)) {
            setLastCAPIErrorMessage(incompatibleParameterTypeMessage(key->getDataType(), key_type));
            return RiaGError;
        }
        if (!canCopyValueForNested(*value, value_type)) {
            setLastCAPIErrorMessage(
                incompatibleParameterTypeMessage(value->getDataType(), value_type));
            return RiaGError;
        }
        std::vector<StructField> struct_fields;
        struct_fields.emplace_back(InternalKeyword::MAP_KEY, key_type.copy());
        struct_fields.emplace_back(InternalKeyword::MAP_VALUE, value_type.copy());
        std::vector<std::unique_ptr<Value>> struct_values;
        struct_values.push_back(copyValueForNested(*key, key_type));
        struct_values.push_back(copyValueForNested(*value, value_type));
        auto struct_type = LogicalType::STRUCT(std::move(struct_fields));
        auto struct_value = new Value(std::move(struct_type), std::move(struct_values));
        children.push_back(std::unique_ptr<Value>(struct_value));
    }
    auto* c_value = createCAPIValue(std::make_unique<Value>(
        LogicalType::MAP(key_type.copy(), value_type.copy()), std::move(children)));
    *out_value = c_value;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_value* riag_value_clone(riag_value* value) {
    if (value == nullptr || value->_value == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    auto* c_value = (riag_value*)calloc(1, sizeof(riag_value));
    c_value->_value = new Value(*static_cast<Value*>(value->_value));
    return c_value;
    RIAG_C_API_GUARD_END(nullptr)
}

void riag_value_copy(riag_value* value, riag_value* other) {
    if (value == nullptr || value->_value == nullptr) {
        return;
    }
    RIAG_C_API_GUARD_BEGIN
    static_cast<Value*>(value->_value)->copyValueFrom(*static_cast<Value*>(other->_value));
    RIAG_C_API_GUARD_END_VOID
}

void riag_value_destroy(riag_value* value) {
    RIAG_C_API_GUARD_BEGIN
    if (value == nullptr) {
        return;
    }
    if (!value->_is_owned_by_cpp) {
        if (value->_value != nullptr) {
            delete static_cast<Value*>(value->_value);
        }
        free(value);
    }
    RIAG_C_API_GUARD_END_VOID
}

riag_state riag_value_get_list_size(riag_value* value, uint64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    if (static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID() !=
        LogicalTypeID::LIST) {
        return RiaGError;
    }
    *out_result = NestedVal::getChildrenSize(static_cast<Value*>(value->_value));
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_list_element(riag_value* value, uint64_t index, riag_value* out_value) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto physical_type_id = static_cast<Value*>(value->_value)->getDataType().getPhysicalType();
    if (physical_type_id != PhysicalTypeID::ARRAY && physical_type_id != PhysicalTypeID::STRUCT &&
        physical_type_id != PhysicalTypeID::LIST) {
        return RiaGError;
    }
    auto listValue = static_cast<Value*>(value->_value);
    if (index >= NestedVal::getChildrenSize(listValue)) {
        return RiaGError;
    }
    try {
        auto val = NestedVal::getChildVal(listValue, index);
        out_value->_value = val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_struct_num_fields(riag_value* value, uint64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto physical_type_id = static_cast<Value*>(value->_value)->getDataType().getPhysicalType();
    if (physical_type_id != PhysicalTypeID::STRUCT) {
        return RiaGError;
    }
    auto val = static_cast<Value*>(value->_value);
    const auto& data_type = val->getDataType();
    try {
        *out_result = StructType::getNumFields(data_type);
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_struct_field_name(riag_value* value, uint64_t index, char** out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto physical_type_id = static_cast<Value*>(value->_value)->getDataType().getPhysicalType();
    if (physical_type_id != PhysicalTypeID::STRUCT) {
        return RiaGError;
    }
    auto val = static_cast<Value*>(value->_value);
    const auto& data_type = val->getDataType();
    if (index >= StructType::getNumFields(data_type)) {
        return RiaGError;
    }
    std::string struct_field_name = StructType::getFields(data_type)[index].getName();
    if (struct_field_name.empty()) {
        return RiaGError;
    }
    *out_result = convertToOwnedCString(struct_field_name);
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_struct_field_index(riag_value* value, const char* field_name,
    uint64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto physical_type_id = static_cast<Value*>(value->_value)->getDataType().getPhysicalType();
    if (physical_type_id != PhysicalTypeID::STRUCT) {
        return RiaGError;
    }
    auto val = static_cast<Value*>(value->_value);
    const auto& data_type = val->getDataType();
    auto index = StructType::getFieldIdx(data_type, field_name);
    if (index == INVALID_STRUCT_FIELD_IDX) {
        return RiaGError;
    }
    *out_result = index;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_struct_field_value(riag_value* value, uint64_t index,
    riag_value* out_value) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    return riag_value_get_list_element(value, index, out_value);
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_map_size(riag_value* value, uint64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::MAP) {
        return RiaGError;
    }
    auto listValue = static_cast<Value*>(value->_value);
    *out_result = NestedVal::getChildrenSize(listValue);
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_map_key(riag_value* value, uint64_t index, riag_value* out_key) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    riag_value map_entry;
    if (riag_value_get_list_element(value, index, &map_entry) == RiaGError) {
        return RiaGError;
    }
    return riag_value_get_struct_field_value(&map_entry, 0, out_key);
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_map_value(riag_value* value, uint64_t index, riag_value* out_value) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    riag_value map_entry;
    if (riag_value_get_list_element(value, index, &map_entry) == RiaGError) {
        return RiaGError;
    }
    return riag_value_get_struct_field_value(&map_entry, 1, out_value);
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_recursive_rel_node_list(riag_value* value, riag_value* out_value) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::RECURSIVE_REL) {
        return RiaGError;
    }
    out_value->_is_owned_by_cpp = true;
    try {
        out_value->_value = RecursiveRelVal::getNodes(static_cast<Value*>(value->_value));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_recursive_rel_rel_list(riag_value* value, riag_value* out_value) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::RECURSIVE_REL) {
        return RiaGError;
    }
    out_value->_is_owned_by_cpp = true;
    try {
        out_value->_value = RecursiveRelVal::getRels(static_cast<Value*>(value->_value));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

void riag_value_get_data_type(riag_value* value, riag_logical_type* out_data_type) {
    if (value == nullptr || value->_value == nullptr) {
        return;
    }
    RIAG_C_API_GUARD_BEGIN
    out_data_type->_data_type =
        new LogicalType(static_cast<Value*>(value->_value)->getDataType().copy());
    RIAG_C_API_GUARD_END_VOID
}

riag_state riag_value_get_bool(riag_value* value, bool* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::BOOL) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<bool>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_int8(riag_value* value, int8_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INT8) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<int8_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_int16(riag_value* value, int16_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INT16) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<int16_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_int32(riag_value* value, int32_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INT32) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<int32_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_int64(riag_value* value, int64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INT64) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<int64_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_uint8(riag_value* value, uint8_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::UINT8) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<uint8_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_uint16(riag_value* value, uint16_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::UINT16) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<uint16_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_uint32(riag_value* value, uint32_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::UINT32) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<uint32_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_uint64(riag_value* value, uint64_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::UINT64) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<uint64_t>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_int128(riag_value* value, riag_int128_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INT128) {
        return RiaGError;
    }
    try {
        auto int128_val = static_cast<Value*>(value->_value)->getValue<int128_t>();
        out_result->low = int128_val.low;
        out_result->high = int128_val.high;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_int128_t_from_string(const char* str, riag_int128_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
    int128_t int128_val = 0;
    try {
        riag::function::CastString::operation(string_t{str, strlen(str)}, int128_val);
        out_result->low = int128_val.low;
        out_result->high = int128_val.high;
    } catch (ConversionException& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_int128_t_to_string(riag_int128_t int128_val, char** out_result) {
    RIAG_C_API_GUARD_BEGIN
    int128_t c_int128 = 0;
    c_int128.low = int128_val.low;
    c_int128.high = int128_val.high;
    try {
        *out_result = convertToOwnedCString(TypeUtils::toString(c_int128));
    } catch (ConversionException& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}
// TODO: bind all int128_t supported functions

riag_state riag_value_get_float(riag_value* value, float* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::FLOAT) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<float>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_double(riag_value* value, double* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::DOUBLE) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Value*>(value->_value)->getValue<double>();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_internal_id(riag_value* value, riag_internal_id_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INTERNAL_ID) {
        return RiaGError;
    }
    try {
        auto id = static_cast<Value*>(value->_value)->getValue<internalID_t>();
        out_result->offset = id.offset;
        out_result->table_id = id.tableID;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_date(riag_value* value, riag_date_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::DATE) {
        return RiaGError;
    }
    try {
        auto date_val = static_cast<Value*>(value->_value)->getValue<date_t>();
        out_result->days = date_val.days;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_timestamp(riag_value* value, riag_timestamp_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::TIMESTAMP) {
        return RiaGError;
    }
    try {
        auto timestamp_val = static_cast<Value*>(value->_value)->getValue<timestamp_t>();
        out_result->value = timestamp_val.value;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_timestamp_ns(riag_value* value, riag_timestamp_ns_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::TIMESTAMP_NS) {
        return RiaGError;
    }
    try {
        auto timestamp_val = static_cast<Value*>(value->_value)->getValue<timestamp_ns_t>();
        out_result->value = timestamp_val.value;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_timestamp_ms(riag_value* value, riag_timestamp_ms_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::TIMESTAMP_MS) {
        return RiaGError;
    }
    try {
        auto timestamp_val = static_cast<Value*>(value->_value)->getValue<timestamp_ms_t>();
        out_result->value = timestamp_val.value;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_timestamp_sec(riag_value* value, riag_timestamp_sec_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::TIMESTAMP_SEC) {
        return RiaGError;
    }
    try {
        auto timestamp_val = static_cast<Value*>(value->_value)->getValue<timestamp_sec_t>();
        out_result->value = timestamp_val.value;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_timestamp_tz(riag_value* value, riag_timestamp_tz_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::TIMESTAMP_TZ) {
        return RiaGError;
    }
    try {
        auto timestamp_val = static_cast<Value*>(value->_value)->getValue<timestamp_tz_t>();
        out_result->value = timestamp_val.value;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_decimal_as_string(riag_value* value, char** out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto decimal_val = static_cast<Value*>(value->_value);
    auto logical_type_id = decimal_val->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::DECIMAL) {
        return RiaGError;
    }

    *out_result = convertToOwnedCString(decimal_val->toString());
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_interval(riag_value* value, riag_interval_t* out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::INTERVAL) {
        return RiaGError;
    }
    try {
        auto interval_val = static_cast<Value*>(value->_value)->getValue<interval_t>();
        out_result->months = interval_val.months;
        out_result->days = interval_val.days;
        out_result->micros = interval_val.micros;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_string(riag_value* value, char** out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::STRING) {
        return RiaGError;
    }
    try {
        *out_result =
            convertToOwnedCString(static_cast<Value*>(value->_value)->getValue<std::string>());
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_blob(riag_value* value, uint8_t** out_result, uint64_t* out_length) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::BLOB) {
        return RiaGError;
    }
    try {
        auto blob = static_cast<Value*>(value->_value)->getValue<std::string>();
        *out_length = blob.size();
        auto* buffer = (uint8_t*)malloc(sizeof(uint8_t) * blob.size());
        memcpy(buffer, blob.data(), blob.size());
        *out_result = buffer;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_value_get_uuid(riag_value* value, char** out_result) {
    if (value == nullptr || value->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(value->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::UUID) {
        return RiaGError;
    }
    try {
        *out_result = convertToOwnedCString(static_cast<Value*>(value->_value)->toString());
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

char* riag_value_to_string(riag_value* value) {
    if (value == nullptr || value->_value == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    return convertToOwnedCString(static_cast<Value*>(value->_value)->toString());
    RIAG_C_API_GUARD_END(nullptr)
}

riag_state riag_node_val_get_id_val(riag_value* node_val, riag_value* out_value) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        auto id_val = NodeVal::getNodeIDVal(static_cast<Value*>(node_val->_value));
        out_value->_value = id_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_node_val_get_label_val(riag_value* node_val, riag_value* out_value) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        auto label_val = NodeVal::getLabelVal(static_cast<Value*>(node_val->_value));
        out_value->_value = label_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_node_val_get_property_size(riag_value* node_val, uint64_t* out_result) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        *out_result = NodeVal::getNumProperties(static_cast<Value*>(node_val->_value));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_node_val_get_property_name_at(riag_value* node_val, uint64_t index,
    char** out_result) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        std::string property_name =
            NodeVal::getPropertyName(static_cast<Value*>(node_val->_value), index);
        if (property_name.empty()) {
            return RiaGError;
        }
        *out_result = convertToOwnedCString(property_name);
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_node_val_get_property_value_at(riag_value* node_val, uint64_t index,
    riag_value* out_value) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        auto value = NodeVal::getPropertyVal(static_cast<Value*>(node_val->_value), index);
        out_value->_value = value;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_node_val_to_string(riag_value* node_val, char** out_result) {
    if (node_val == nullptr || node_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(node_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::NODE) {
        return RiaGError;
    }
    try {
        *out_result =
            convertToOwnedCString(NodeVal::toString(static_cast<Value*>(node_val->_value)));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_id_val(riag_value* rel_val, riag_value* out_value) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        auto id_val = RelVal::getIDVal(static_cast<Value*>(rel_val->_value));
        out_value->_value = id_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_src_id_val(riag_value* rel_val, riag_value* out_value) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        auto src_id_val = RelVal::getSrcNodeIDVal(static_cast<Value*>(rel_val->_value));
        out_value->_value = src_id_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_dst_id_val(riag_value* rel_val, riag_value* out_value) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        auto dst_id_val = RelVal::getDstNodeIDVal(static_cast<Value*>(rel_val->_value));
        out_value->_value = dst_id_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_label_val(riag_value* rel_val, riag_value* out_value) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        auto label_val = RelVal::getLabelVal(static_cast<Value*>(rel_val->_value));
        out_value->_value = label_val;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_property_size(riag_value* rel_val, uint64_t* out_result) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        *out_result = RelVal::getNumProperties(static_cast<Value*>(rel_val->_value));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}
riag_state riag_rel_val_get_property_name_at(riag_value* rel_val, uint64_t index,
    char** out_result) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        std::string property_name =
            RelVal::getPropertyName(static_cast<Value*>(rel_val->_value), index);
        if (property_name.empty()) {
            return RiaGError;
        }
        *out_result = convertToOwnedCString(property_name);
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_get_property_value_at(riag_value* rel_val, uint64_t index,
    riag_value* out_value) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        auto value = RelVal::getPropertyVal(static_cast<Value*>(rel_val->_value), index);
        out_value->_value = value;
        out_value->_is_owned_by_cpp = true;
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_rel_val_to_string(riag_value* rel_val, char** out_result) {
    if (rel_val == nullptr || rel_val->_value == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto logical_type_id = static_cast<Value*>(rel_val->_value)->getDataType().getLogicalTypeID();
    if (logical_type_id != LogicalTypeID::REL) {
        return RiaGError;
    }
    try {
        *out_result = convertToOwnedCString(RelVal::toString(static_cast<Value*>(rel_val->_value)));
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

void riag_destroy_string(char* str) {
    RIAG_C_API_GUARD_BEGIN
    free(str);
    RIAG_C_API_GUARD_END_VOID
}

void riag_destroy_blob(uint8_t* blob) {
    RIAG_C_API_GUARD_BEGIN
    free(blob);
    RIAG_C_API_GUARD_END_VOID
}

riag_state riag_timestamp_ns_to_tm(riag_timestamp_ns_t timestamp, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = timestamp.value / 1000000000;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_ms_to_tm(riag_timestamp_ms_t timestamp, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = timestamp.value / 1000;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_sec_to_tm(riag_timestamp_sec_t timestamp, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = timestamp.value;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_tz_to_tm(riag_timestamp_tz_t timestamp, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = timestamp.value / 1000000;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_to_tm(riag_timestamp_t timestamp, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = timestamp.value / 1000000;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_ns_from_tm(struct tm tm, riag_timestamp_ns_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->value = time * 1000000000;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_ms_from_tm(struct tm tm, riag_timestamp_ms_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->value = time * 1000;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_sec_from_tm(struct tm tm, riag_timestamp_sec_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->value = time;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_tz_from_tm(struct tm tm, riag_timestamp_tz_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->value = time * 1000000;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_timestamp_from_tm(struct tm tm, riag_timestamp_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->value = time * 1000000;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_date_to_tm(riag_date_t date, struct tm* out_result) {
    RIAG_C_API_GUARD_BEGIN
    time_t time = date.days * 86400;
#ifdef _WIN32
    if (convertTimeToTm(time, out_result) != 0) {
        return RiaGError;
    }
#else
    if (gmtime_r(&time, out_result) == nullptr) {
        return RiaGError;
    }
#endif
    out_result->tm_hour = 0;
    out_result->tm_min = 0;
    out_result->tm_sec = 0;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_date_from_tm(struct tm tm, riag_date_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
#ifdef _WIN32
    int64_t time = convertTmToTime(tm);
#else
    int64_t time = timegm(&tm);
#endif
    if (time == -1) {
        return RiaGError;
    }
    out_result->days = time / 86400;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_date_to_string(riag_date_t date, char** out_result) {
    RIAG_C_API_GUARD_BEGIN
    tm tm{};
    if (riag_date_to_tm(date, &tm) != RiaGSuccess) {
        return RiaGError;
    }
    char buffer[80];
    if (strftime(buffer, 80, "%Y-%m-%d", &tm) == 0) {
        return RiaGError;
    }
    *out_result = convertToOwnedCString(buffer);
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_date_from_string(const char* str, riag_date_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
    try {
        date_t date = Date::fromCString(str, strlen(str));
        out_result->days = date.days;
    } catch (ConversionException& e) {
        return RiaGError;
    }
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

void riag_interval_to_difftime(riag_interval_t interval, double* out_result) {
    RIAG_C_API_GUARD_BEGIN
    auto micros = interval.micros + interval.months * Interval::MICROS_PER_MONTH +
                  interval.days * Interval::MICROS_PER_DAY;
    double seconds = micros / 1000000.0;
    *out_result = seconds;
    RIAG_C_API_GUARD_END_VOID
}

void riag_interval_from_difftime(double difftime, riag_interval_t* out_result) {
    RIAG_C_API_GUARD_BEGIN
    int64_t total_micros = static_cast<int64_t>(difftime * 1000000);
    out_result->months = total_micros / Interval::MICROS_PER_MONTH;
    total_micros -= out_result->months * Interval::MICROS_PER_MONTH;
    out_result->days = total_micros / Interval::MICROS_PER_DAY;
    total_micros -= out_result->days * Interval::MICROS_PER_DAY;
    out_result->micros = total_micros;
    RIAG_C_API_GUARD_END_VOID
}
