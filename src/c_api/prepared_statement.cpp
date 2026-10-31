#include "main/prepared_statement.h"

#include "c_api/helpers.h"
#include "c_api/riag.h"
#include "common/types/value/value.h"

using namespace riag::common;
using namespace riag::main;

void riag_prepared_statement_bind_cpp_value(riag_prepared_statement* prepared_statement,
    const char* param_name, std::unique_ptr<Value> value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return;
    }
    RIAG_C_API_GUARD_BEGIN
    auto* bound_values = static_cast<std::unordered_map<std::string, std::unique_ptr<Value>>*>(
        prepared_statement->_bound_values);
    bound_values->erase(param_name);
    bound_values->insert({param_name, std::move(value)});
    RIAG_C_API_GUARD_END_VOID
}

void riag_prepared_statement_destroy(riag_prepared_statement* prepared_statement) {
    RIAG_C_API_GUARD_BEGIN
    if (prepared_statement == nullptr) {
        return;
    }
    if (prepared_statement->_prepared_statement != nullptr) {
        delete static_cast<PreparedStatement*>(prepared_statement->_prepared_statement);
        prepared_statement->_prepared_statement = nullptr;
    }
    if (prepared_statement->_bound_values != nullptr) {
        delete static_cast<std::unordered_map<std::string, std::unique_ptr<Value>>*>(
            prepared_statement->_bound_values);
        prepared_statement->_bound_values = nullptr;
    }
    RIAG_C_API_GUARD_END_VOID
}

bool riag_prepared_statement_is_success(riag_prepared_statement* prepared_statement) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<PreparedStatement*>(prepared_statement->_prepared_statement)->isSuccess();
    RIAG_C_API_GUARD_END(false)
}

bool riag_prepared_statement_is_read_only(riag_prepared_statement* prepared_statement) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<PreparedStatement*>(prepared_statement->_prepared_statement)->isReadOnly();
    RIAG_C_API_GUARD_END(false)
}

char* riag_prepared_statement_get_error_message(riag_prepared_statement* prepared_statement) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    auto error_message =
        static_cast<PreparedStatement*>(prepared_statement->_prepared_statement)->getErrorMessage();
    if (error_message.empty()) {
        return nullptr;
    }
    return convertToOwnedCString(error_message);
    RIAG_C_API_GUARD_END(nullptr)
}

riag_state riag_prepared_statement_bind_bool(riag_prepared_statement* prepared_statement,
    const char* param_name, bool value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_int64(riag_prepared_statement* prepared_statement,
    const char* param_name, int64_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_int32(riag_prepared_statement* prepared_statement,
    const char* param_name, int32_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_int16(riag_prepared_statement* prepared_statement,
    const char* param_name, int16_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_int8(riag_prepared_statement* prepared_statement,
    const char* param_name, int8_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_uint64(riag_prepared_statement* prepared_statement,
    const char* param_name, uint64_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_uint32(riag_prepared_statement* prepared_statement,
    const char* param_name, uint32_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_uint16(riag_prepared_statement* prepared_statement,
    const char* param_name, uint16_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_uint8(riag_prepared_statement* prepared_statement,
    const char* param_name, uint8_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_double(riag_prepared_statement* prepared_statement,
    const char* param_name, double value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_float(riag_prepared_statement* prepared_statement,
    const char* param_name, float value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(value);
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_date(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_date_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(date_t(value.days));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_timestamp_ns(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_timestamp_ns_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(timestamp_ns_t(value.value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_timestamp_ms(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_timestamp_ms_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(timestamp_ms_t(value.value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_timestamp_sec(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_timestamp_sec_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(timestamp_sec_t(value.value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_timestamp_tz(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_timestamp_tz_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(timestamp_tz_t(value.value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_timestamp(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_timestamp_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(timestamp_t(value.value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_interval(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_interval_t value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr =
            std::make_unique<Value>(interval_t(value.months, value.days, value.micros));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_string(riag_prepared_statement* prepared_statement,
    const char* param_name, const char* value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(std::string(value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_prepared_statement_bind_value(riag_prepared_statement* prepared_statement,
    const char* param_name, riag_value* value) {
    if (prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        auto value_ptr = std::make_unique<Value>(*static_cast<Value*>(value->_value));
        riag_prepared_statement_bind_cpp_value(prepared_statement, param_name,
            std::move(value_ptr));
        return RiaGSuccess;
    } catch (Exception& e) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}
