#include "main/query_result.h"

#include "c_api/helpers.h"
#include "c_api/riag.h"

using namespace riag::main;
using namespace riag::common;
using namespace riag::processor;

void riag_query_result_destroy(riag_query_result* query_result) {
    RIAG_C_API_GUARD_BEGIN
    if (query_result == nullptr) {
        return;
    }
    if (query_result->_query_result != nullptr) {
        if (!query_result->_is_owned_by_cpp) {
            delete static_cast<QueryResult*>(query_result->_query_result);
        }
        query_result->_query_result = nullptr;
    }
    RIAG_C_API_GUARD_END_VOID
}

bool riag_query_result_is_success(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<QueryResult*>(query_result->_query_result)->isSuccess();
    RIAG_C_API_GUARD_END(false)
}

char* riag_query_result_get_error_message(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    auto error_message = static_cast<QueryResult*>(query_result->_query_result)->getErrorMessage();
    if (error_message.empty()) {
        return nullptr;
    }
    return convertToOwnedCString(error_message);
    RIAG_C_API_GUARD_END(nullptr)
}

uint64_t riag_query_result_get_num_columns(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return 0;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<QueryResult*>(query_result->_query_result)->getNumColumns();
    RIAG_C_API_GUARD_END(0)
}

riag_state riag_query_result_get_column_name(riag_query_result* query_result, uint64_t index,
    char** out_column_name) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto column_names = static_cast<QueryResult*>(query_result->_query_result)->getColumnNames();
    if (index >= column_names.size()) {
        return RiaGError;
    }
    *out_column_name = convertToOwnedCString(column_names[index]);
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_query_result_get_column_data_type(riag_query_result* query_result, uint64_t index,
    riag_logical_type* out_column_data_type) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    auto column_data_types =
        static_cast<QueryResult*>(query_result->_query_result)->getColumnDataTypes();
    if (index >= column_data_types.size()) {
        return RiaGError;
    }
    const auto& column_data_type = column_data_types[index];
    out_column_data_type->_data_type = new LogicalType(column_data_type.copy());
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

uint64_t riag_query_result_get_num_tuples(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return 0;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<QueryResult*>(query_result->_query_result)->getNumTuples();
    RIAG_C_API_GUARD_END(0)
}

riag_state riag_query_result_get_query_summary(riag_query_result* query_result,
    riag_query_summary* out_query_summary) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    if (out_query_summary == nullptr) {
        return RiaGError;
    }
    auto query_summary = static_cast<QueryResult*>(query_result->_query_result)->getQuerySummary();
    out_query_summary->_query_summary = query_summary;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

bool riag_query_result_has_next(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<QueryResult*>(query_result->_query_result)->hasNext();
    RIAG_C_API_GUARD_END(false)
}

bool riag_query_result_has_next_query_result(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return false;
    }
    RIAG_C_API_GUARD_BEGIN
    return static_cast<QueryResult*>(query_result->_query_result)->hasNextQueryResult();
    RIAG_C_API_GUARD_END(false)
}

riag_state riag_query_result_get_next_query_result(riag_query_result* query_result,
    riag_query_result* out_query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    if (!riag_query_result_has_next_query_result(query_result)) {
        return RiaGError;
    }
    auto next_query_result =
        static_cast<QueryResult*>(query_result->_query_result)->getNextQueryResult();
    if (next_query_result == nullptr) {
        return RiaGError;
    }
    out_query_result->_query_result = next_query_result;
    out_query_result->_is_owned_by_cpp = true;
    return RiaGSuccess;
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_query_result_get_next(riag_query_result* query_result,
    riag_flat_tuple* out_flat_tuple) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        clearLastCAPIErrorMessage();
        auto flat_tuple = static_cast<QueryResult*>(query_result->_query_result)->getNext();
        out_flat_tuple->_flat_tuple = flat_tuple.get();
        out_flat_tuple->_is_owned_by_cpp = true;
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

char* riag_query_result_to_string(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return nullptr;
    }
    RIAG_C_API_GUARD_BEGIN
    std::string result_string = static_cast<QueryResult*>(query_result->_query_result)->toString();
    return convertToOwnedCString(result_string);
    RIAG_C_API_GUARD_END(nullptr)
}

void riag_query_result_reset_iterator(riag_query_result* query_result) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return;
    }
    RIAG_C_API_GUARD_BEGIN
    static_cast<QueryResult*>(query_result->_query_result)->resetIterator();
    RIAG_C_API_GUARD_END_VOID
}

riag_state riag_query_result_get_arrow_schema(riag_query_result* query_result,
    ArrowSchema* out_schema) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        *out_schema = *static_cast<QueryResult*>(query_result->_query_result)->getArrowSchema();
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}

riag_state riag_query_result_get_next_arrow_chunk(riag_query_result* query_result,
    int64_t chunk_size, ArrowArray* out_arrow_array) {
    if (query_result == nullptr || query_result->_query_result == nullptr) {
        return RiaGError;
    }
    RIAG_C_API_GUARD_BEGIN
    try {
        *out_arrow_array =
            *static_cast<QueryResult*>(query_result->_query_result)->getNextArrowChunk(chunk_size);
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
    RIAG_C_API_GUARD_END(RiaGError)
}
