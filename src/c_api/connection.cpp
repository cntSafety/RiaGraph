#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

#include "c_api/helpers.h"
#include "c_api/riag.h"
#include "common/exception/exception.h"
#include "main/riag.h"
#include "planner/operator/logical_operator.h"
#include "planner/operator/logical_plan.h"
#include "planner/operator/logical_table_function_call.h"
#include "storage/table/arrow_table_support.h"

namespace riag {
namespace common {
class Value;
}
} // namespace riag

using namespace riag::common;
using namespace riag::main;
using namespace riag::planner;

static std::mutex arrowTableIDMutex;
static std::unordered_map<Connection*, std::unordered_map<std::string, std::string>> arrowTableIDs;

static void rememberArrowTableID(Connection* connection, const char* tableName,
    std::string arrowId) {
    std::lock_guard<std::mutex> lock(arrowTableIDMutex);
    arrowTableIDs[connection][tableName] = std::move(arrowId);
}

static std::string getRememberedArrowTableID(Connection* connection, const char* tableName) {
    std::lock_guard<std::mutex> lock(arrowTableIDMutex);
    if (!arrowTableIDs.contains(connection) || !arrowTableIDs.at(connection).contains(tableName)) {
        return "";
    }
    return arrowTableIDs.at(connection).at(tableName);
}

static void forgetArrowTableID(Connection* connection, const char* tableName) {
    std::lock_guard<std::mutex> lock(arrowTableIDMutex);
    if (!arrowTableIDs.contains(connection)) {
        return;
    }
    arrowTableIDs.at(connection).erase(tableName);
    if (arrowTableIDs.at(connection).empty()) {
        arrowTableIDs.erase(connection);
    }
}

static void forgetArrowTableIDs(Connection* connection) {
    std::lock_guard<std::mutex> lock(arrowTableIDMutex);
    arrowTableIDs.erase(connection);
}

static ArrowSchemaWrapper takeArrowSchema(ArrowSchema* schema) {
    ArrowSchemaWrapper wrapper;
    static_cast<ArrowSchema&>(wrapper) = *schema;
    schema->release = nullptr;
    return wrapper;
}

static std::vector<ArrowArrayWrapper> takeArrowArrays(ArrowArray* arrays, uint64_t numArrays) {
    std::vector<ArrowArrayWrapper> wrappers;
    wrappers.reserve(numArrays);
    for (auto i = 0u; i < numArrays; ++i) {
        ArrowArrayWrapper wrapper;
        static_cast<ArrowArray&>(wrapper) = arrays[i];
        arrays[i].release = nullptr;
        wrappers.push_back(std::move(wrapper));
    }
    return wrappers;
}

static riag_state setQueryResult(std::unique_ptr<QueryResult> queryResult,
    riag_query_result* outQueryResult) {
    if (queryResult == nullptr) {
        return RiaGError;
    }
    auto queryResultPtr = queryResult.release();
    outQueryResult->_query_result = queryResultPtr;
    outQueryResult->_is_owned_by_cpp = false;
    if (!queryResultPtr->isSuccess()) {
        setLastCAPIErrorMessage(queryResultPtr->getErrorMessage());
        return RiaGError;
    }
    return RiaGSuccess;
}

riag_state riag_connection_init(riag_database* database, riag_connection* out_connection) {
    if (database == nullptr || database->_database == nullptr) {
        out_connection->_connection = nullptr;
        return RiaGError;
    }
    try {
        out_connection->_connection = new Connection(static_cast<Database*>(database->_database));
    } catch (Exception& e) {
        out_connection->_connection = nullptr;
        return RiaGError;
    }
    return RiaGSuccess;
}

void riag_connection_destroy(riag_connection* connection) {
    if (connection == nullptr) {
        return;
    }
    std::atomic_ref<void*> atomic_conn(connection->_connection);
    void* ptr = atomic_conn.exchange(nullptr);
    if (ptr != nullptr) {
        auto connectionPtr = static_cast<Connection*>(ptr);
        forgetArrowTableIDs(connectionPtr);
        delete connectionPtr;
    }
}

riag_state riag_connection_set_max_num_thread_for_exec(riag_connection* connection,
    uint64_t num_threads) {
    if (connection == nullptr || connection->_connection == nullptr) {
        return RiaGError;
    }
    try {
        static_cast<Connection*>(connection->_connection)->setMaxNumThreadForExec(num_threads);
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
}

riag_state riag_connection_get_max_num_thread_for_exec(riag_connection* connection,
    uint64_t* out_result) {
    if (connection == nullptr || connection->_connection == nullptr) {
        return RiaGError;
    }
    try {
        *out_result = static_cast<Connection*>(connection->_connection)->getMaxNumThreadForExec();
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
}

riag_state riag_connection_query(riag_connection* connection, const char* query,
    riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto query_result =
            static_cast<Connection*>(connection->_connection)->query(query).release();
        if (query_result == nullptr) {
            return RiaGError;
        }
        out_query_result->_query_result = query_result;
        out_query_result->_is_owned_by_cpp = false;
        if (!query_result->isSuccess()) {
            setLastCAPIErrorMessage(query_result->getErrorMessage());
            return RiaGError;
        }
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

riag_state riag_connection_prepare(riag_connection* connection, const char* query,
    riag_prepared_statement* out_prepared_statement) {
    if (connection == nullptr || connection->_connection == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto prepared_statement =
            static_cast<Connection*>(connection->_connection)->prepare(query).release();
        if (prepared_statement == nullptr) {
            return RiaGError;
        }
        out_prepared_statement->_prepared_statement = prepared_statement;
        out_prepared_statement->_bound_values =
            new std::unordered_map<std::string, std::unique_ptr<Value>>;
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
    return RiaGSuccess;
}

riag_state riag_connection_execute(riag_connection* connection,
    riag_prepared_statement* prepared_statement, riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr ||
        prepared_statement == nullptr || prepared_statement->_prepared_statement == nullptr ||
        prepared_statement->_bound_values == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto prepared_statement_ptr =
            static_cast<PreparedStatement*>(prepared_statement->_prepared_statement);
        auto bound_values = static_cast<std::unordered_map<std::string, std::unique_ptr<Value>>*>(
            prepared_statement->_bound_values);

        // Must copy the parameters for safety, and so that the parameters in the prepared statement
        // stay the same.
        std::unordered_map<std::string, std::unique_ptr<Value>> copied_bound_values;
        for (auto& [name, value] : *bound_values) {
            copied_bound_values.emplace(name, value->copy());
        }

        auto query_result =
            static_cast<Connection*>(connection->_connection)
                ->executeWithParams(prepared_statement_ptr, std::move(copied_bound_values))
                .release();
        if (query_result == nullptr) {
            return RiaGError;
        }
        out_query_result->_query_result = query_result;
        out_query_result->_is_owned_by_cpp = false;
        if (!query_result->isSuccess()) {
            setLastCAPIErrorMessage(query_result->getErrorMessage());
            return RiaGError;
        }
        return RiaGSuccess;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

riag_state riag_connection_create_arrow_table(riag_connection* connection, const char* table_name,
    ArrowSchema* schema, ArrowArray* arrays, uint64_t num_arrays,
    riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr || table_name == nullptr ||
        schema == nullptr || arrays == nullptr || out_query_result == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto result = riag::ArrowTableSupport::createViewFromArrowTable(
            *static_cast<Connection*>(connection->_connection), table_name, takeArrowSchema(schema),
            takeArrowArrays(arrays, num_arrays));
        auto state = setQueryResult(std::move(result.queryResult), out_query_result);
        if (state == RiaGSuccess) {
            rememberArrowTableID(static_cast<Connection*>(connection->_connection), table_name,
                std::move(result.arrowId));
        }
        return state;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

riag_state riag_connection_create_arrow_rel_table(riag_connection* connection,
    const char* table_name, const char* src_table_name, const char* dst_table_name,
    ArrowSchema* schema, ArrowArray* arrays, uint64_t num_arrays,
    riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr || table_name == nullptr ||
        src_table_name == nullptr || dst_table_name == nullptr || schema == nullptr ||
        arrays == nullptr || out_query_result == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto result = riag::ArrowTableSupport::createRelTableFromArrowTable(
            *static_cast<Connection*>(connection->_connection), table_name, src_table_name,
            dst_table_name, takeArrowSchema(schema), takeArrowArrays(arrays, num_arrays));
        auto state = setQueryResult(std::move(result.queryResult), out_query_result);
        if (state == RiaGSuccess) {
            rememberArrowTableID(static_cast<Connection*>(connection->_connection), table_name,
                std::move(result.arrowId));
        }
        return state;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

riag_state riag_connection_create_arrow_rel_table_csr(riag_connection* connection,
    const char* table_name, const char* src_table_name, const char* dst_table_name,
    ArrowSchema* indices_schema, ArrowArray* indices_arrays, uint64_t num_indices_arrays,
    ArrowSchema* indptr_schema, ArrowArray* indptr_arrays, uint64_t num_indptr_arrays,
    const char* dst_col_name, riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr || table_name == nullptr ||
        src_table_name == nullptr || dst_table_name == nullptr || indices_schema == nullptr ||
        indices_arrays == nullptr || indptr_schema == nullptr || indptr_arrays == nullptr ||
        out_query_result == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto result = riag::ArrowTableSupport::createRelTableFromArrowCSR(
            *static_cast<Connection*>(connection->_connection), table_name, src_table_name,
            dst_table_name, takeArrowSchema(indices_schema),
            takeArrowArrays(indices_arrays, num_indices_arrays), takeArrowSchema(indptr_schema),
            takeArrowArrays(indptr_arrays, num_indptr_arrays),
            dst_col_name != nullptr ? dst_col_name : "to");
        auto state = setQueryResult(std::move(result.queryResult), out_query_result);
        if (state == RiaGSuccess) {
            rememberArrowTableID(static_cast<Connection*>(connection->_connection), table_name,
                std::move(result.arrowId));
        }
        return state;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

riag_state riag_connection_drop_arrow_table(riag_connection* connection, const char* table_name,
    riag_query_result* out_query_result) {
    if (connection == nullptr || connection->_connection == nullptr || table_name == nullptr ||
        out_query_result == nullptr) {
        return RiaGError;
    }
    try {
        clearLastCAPIErrorMessage();
        auto connectionPtr = static_cast<Connection*>(connection->_connection);
        auto arrowId = getRememberedArrowTableID(connectionPtr, table_name);
        auto result = riag::ArrowTableSupport::unregisterArrowTable(*connectionPtr, table_name);
        auto state = setQueryResult(std::move(result), out_query_result);
        if (state == RiaGSuccess) {
            if (!arrowId.empty()) {
                riag::ArrowTableSupport::unregisterArrowData(arrowId);
            }
            forgetArrowTableID(connectionPtr, table_name);
        }
        return state;
    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        return RiaGError;
    }
}

void riag_connection_interrupt(riag_connection* connection) {
    static_cast<Connection*>(connection->_connection)->interrupt();
}

riag_state riag_connection_set_query_timeout(riag_connection* connection, uint64_t timeout_in_ms) {
    if (connection == nullptr || connection->_connection == nullptr) {
        return RiaGError;
    }
    try {
        static_cast<Connection*>(connection->_connection)->setQueryTimeOut(timeout_in_ms);
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
}

// Helper: walk the logical operator tree to find the first TABLE_FUNCTION_CALL
// whose table function supports push-down, and return its description (SQL).
// Returns empty string if no such operator is found.
static std::string findPushedSqlFromPlan(LogicalOperator* op) {
    if (op == nullptr) {
        return "";
    }
    if (op->getOperatorType() == LogicalOperatorType::TABLE_FUNCTION_CALL) {
        auto& tableFuncCall = op->constCast<LogicalTableFunctionCall>();
        if (tableFuncCall.getTableFunc().supportsPushDownFunc()) {
            auto desc = tableFuncCall.getBindData()->getDescription();
            if (!desc.empty()) {
                return desc;
            }
        }
    }
    // Depth-first search: check children
    for (auto i = 0u; i < op->getNumChildren(); ++i) {
        auto child = op->getChild(i).get();
        if (child != nullptr) {
            auto result = findPushedSqlFromPlan(child);
            if (!result.empty()) {
                return result;
            }
        }
    }
    return "";
}

riag_state riag_connection_get_pushed_sql(riag_connection* connection, const char* cypher_query,
    char** out_sql) {
    if (connection == nullptr || connection->_connection == nullptr || cypher_query == nullptr ||
        out_sql == nullptr) {
        return RiaGError;
    }
    // Initialize the out parameter up-front so every error path leaves it as NULL
    // rather than whatever the caller passed in.
    *out_sql = nullptr;
    try {
        clearLastCAPIErrorMessage();

        auto* connPtr = static_cast<Connection*>(connection->_connection);

        // Prepare the query (this runs the planner and optimizer)
        auto prepared = connPtr->prepare(cypher_query);
        if (prepared == nullptr || !prepared->isSuccess()) {
            std::string err = prepared ? prepared->getErrorMessage() : "prepare returned null";
            setLastCAPIErrorMessage(err);
            return RiaGError;
        }

        // Get the cached prepared statement to access the logical plan
        auto name = prepared->getName();
        auto& manager = connPtr->getClientContext()->getCachedPreparedStatementManager();
        auto* cached = manager.getCachedStatement(name);
        if (cached == nullptr || cached->logicalPlan == nullptr) {
            setLastCAPIErrorMessage("No logical plan found in cached statement");
            return RiaGError;
        }

        // Walk the logical plan to find the pushed-down SQL
        auto root = cached->logicalPlan->getLastOperator().get();
        auto sql = findPushedSqlFromPlan(root);
        if (sql.empty()) {
            *out_sql = nullptr;
            // Not an error if no pushdown SQL is found - caller can handle NULL
            return RiaGSuccess;
        }

        *out_sql = convertToOwnedCString(sql);
        return RiaGSuccess;

    } catch (Exception& e) {
        setLastCAPIErrorMessage(e.what());
        *out_sql = nullptr;
        return RiaGError;
    }
}
