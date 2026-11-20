#include "planner/operator/logical_operator.h"

#include "common/exception/runtime.h"

using namespace riag::common;

namespace riag {
namespace planner {

// LCOV_EXCL_START
std::string LogicalOperatorUtils::logicalOperatorTypeToString(LogicalOperatorType type) {
    switch (type) {
    case LogicalOperatorType::ACCUMULATE:
        return "ACCUMULATE";
    case LogicalOperatorType::AGGREGATE:
        return "AGGREGATE";
    case LogicalOperatorType::ALTER:
        return "ALTER";
    case LogicalOperatorType::ANALYZE:
        return "ANALYZE";
    case LogicalOperatorType::ASSERT:
        return "ASSERT";
    case LogicalOperatorType::ATTACH_DATABASE:
        return "ATTACH_DATABASE";
    case LogicalOperatorType::COPY_FROM:
        return "COPY_FROM";
    case LogicalOperatorType::COPY_TO:
        return "COPY_TO";
    case LogicalOperatorType::COUNT_ANTI_EDGE_CHAIN:
        return "COUNT_ANTI_EDGE_CHAIN";
    case LogicalOperatorType::COUNT_EXTEND_CHAIN:
        return "COUNT_EXTEND_CHAIN";
    case LogicalOperatorType::COUNT_REL_TABLE:
        return "COUNT_REL_TABLE";
    case LogicalOperatorType::CREATE_MACRO:
        return "CREATE_MACRO";
    case LogicalOperatorType::CREATE_INDEX:
        return "CREATE_INDEX";
    case LogicalOperatorType::CREATE_SEQUENCE:
        return "CREATE_SEQUENCE";
    case LogicalOperatorType::CREATE_TABLE:
        return "CREATE_TABLE";
    case LogicalOperatorType::CROSS_PRODUCT:
        return "CROSS_PRODUCT";
    case LogicalOperatorType::DELETE:
        return "DELETE_NODE";
    case LogicalOperatorType::DETACH_DATABASE:
        return "DETACH_DATABASE";
    case LogicalOperatorType::DISTINCT:
        return "DISTINCT";
    case LogicalOperatorType::DROP:
        return "DROP";
    case LogicalOperatorType::DUMMY_SCAN:
        return "DUMMY_SCAN";
    case LogicalOperatorType::DUMMY_SINK:
        return "DUMMY_SINK";
    case LogicalOperatorType::EMPTY_RESULT:
        return "EMPTY_RESULT";
    case LogicalOperatorType::EXPLAIN:
        return "EXPLAIN";
    case LogicalOperatorType::EXPRESSIONS_SCAN:
        return "EXPRESSIONS_SCAN";
    case LogicalOperatorType::EXTENSION:
        return "LOAD";
    case LogicalOperatorType::EXPORT_DATABASE:
        return "EXPORT_DATABASE";
    case LogicalOperatorType::EXTEND:
        return "EXTEND";
    case LogicalOperatorType::FILTER:
        return "FILTER";
    case LogicalOperatorType::FLATTEN:
        return "FLATTEN";
    case LogicalOperatorType::GROUPED_REACHABLE_COUNT:
        return "GROUPED_REACHABLE_COUNT";
    case LogicalOperatorType::HASH_JOIN:
        return "HASH_JOIN";
    case LogicalOperatorType::IMPORT_DATABASE:
        return "IMPORT_DATABASE";
    case LogicalOperatorType::INDEX_LOOK_UP:
        return "INDEX_LOOK_UP";
    case LogicalOperatorType::QUERY_PRIMARY_KEY_LOOKUP:
        return "QUERY_PRIMARY_KEY_LOOKUP";
    case LogicalOperatorType::INTERSECT:
        return "INTERSECT";
    case LogicalOperatorType::INSERT:
        return "INSERT";
    case LogicalOperatorType::LIMIT:
        return "LIMIT";
    case LogicalOperatorType::MERGE:
        return "MERGE";
    case LogicalOperatorType::MULTIPLICITY_REDUCER:
        return "MULTIPLICITY_REDUCER";
    case LogicalOperatorType::NODE_LABEL_FILTER:
        return "NODE_LABEL_FILTER";
    case LogicalOperatorType::NOOP:
        return "NOOP";
    case LogicalOperatorType::ORDER_BY:
        return "ORDER_BY";
    case LogicalOperatorType::PARTITIONER:
        return "PARTITIONER";
    case LogicalOperatorType::PACKED_EXTEND:
        return "PACKED_EXTEND";
    case LogicalOperatorType::PATH_PROPERTY_PROBE:
        return "PATH_PROPERTY_PROBE";
    case LogicalOperatorType::PROJECTION:
        return "PROJECTION";
    case LogicalOperatorType::RECURSIVE_EXTEND:
        return "RECURSIVE_EXTEND";
    case LogicalOperatorType::REACHABLE_COUNT:
        return "REACHABLE_COUNT";
    case LogicalOperatorType::REL_DEGREE_TABLE:
        return "REL_DEGREE_TABLE";
    case LogicalOperatorType::SCAN_NODE_TABLE:
        return "SCAN_NODE_TABLE";
    case LogicalOperatorType::SEMI_MASKER:
        return "SEMI_MASKER";
    case LogicalOperatorType::SET_PROPERTY:
        return "SET_PROPERTY";
    case LogicalOperatorType::STANDALONE_CALL:
        return "STANDALONE_CALL";
    case LogicalOperatorType::TABLE_FUNCTION_CALL:
        return "TABLE_FUNCTION_CALL";
    case LogicalOperatorType::TRANSACTION:
        return "TRANSACTION";
    case LogicalOperatorType::UNION_ALL:
        return "UNION_ALL";
    case LogicalOperatorType::UNWIND:
        return "UNWIND";
    case LogicalOperatorType::USE_DATABASE:
        return "USE_DATABASE";
    case LogicalOperatorType::CREATE_GRAPH:
        return "CREATE_GRAPH";
    case LogicalOperatorType::USE_GRAPH:
        return "USE_GRAPH";
    case LogicalOperatorType::NAMESPACE_COMMAND:
        return "NAMESPACE_COMMAND";
    case LogicalOperatorType::NAMESPACE_MUTATION:
        return "NAMESPACE_MUTATION";
    case LogicalOperatorType::CREATE_TYPE:
        return "CREATE_TYPE";
    case LogicalOperatorType::EXTENSION_CLAUSE:
        return "EXTENSION_CLAUSE";
    case LogicalOperatorType::UNWIND_DEDUPLICATE:
        return "UNWIND_DEDUPLICATE";
    }
    // No default label: -Wswitch then enforces that every LogicalOperatorType enumerator is
    // handled above, so adding a new operator fails the build here instead of failing at
    // runtime. The throw only guards against out-of-range values (e.g. deserialized data).
    throw RuntimeException("Unknown logical operator type.");
}
// LCOV_EXCL_STOP

bool LogicalOperatorUtils::isUpdate(LogicalOperatorType type) {
    switch (type) {
    // ASSERT writes nothing, but like a write it must see every row it is given: no optimizer may
    // skip, narrow or reorder the subtree below it, and joins put its side on the build side.
    case LogicalOperatorType::ASSERT:
    case LogicalOperatorType::INSERT:
    case LogicalOperatorType::DELETE:
    case LogicalOperatorType::SET_PROPERTY:
    case LogicalOperatorType::NAMESPACE_MUTATION:
    case LogicalOperatorType::MERGE:
        return true;
    default:
        return false;
    }
}

bool LogicalOperatorUtils::isAccHashJoin(const LogicalOperator& op) {
    return op.getOperatorType() == LogicalOperatorType::HASH_JOIN &&
           op.getChild(0)->getOperatorType() == LogicalOperatorType::ACCUMULATE;
}

LogicalOperator::LogicalOperator(LogicalOperatorType operatorType,
    std::shared_ptr<LogicalOperator> child, std::optional<common::cardinality_t> cardinality)
    : operatorType{operatorType},
      cardinality{cardinality.has_value() ? cardinality.value() : child->getCardinality()} {
    children.push_back(std::move(child));
}

LogicalOperator::LogicalOperator(LogicalOperatorType operatorType,
    std::shared_ptr<LogicalOperator> left, std::shared_ptr<LogicalOperator> right)
    : LogicalOperator{operatorType} {
    children.push_back(std::move(left));
    children.push_back(std::move(right));
}

LogicalOperator::LogicalOperator(LogicalOperatorType operatorType,
    const logical_op_vector_t& children)
    : LogicalOperator{operatorType} {
    for (auto& child : children) {
        this->children.push_back(child);
    }
}

bool LogicalOperator::hasUpdateRecursive() {
    if (LogicalOperatorUtils::isUpdate(operatorType)) {
        return true;
    }
    for (auto& child : children) {
        if (child->hasUpdateRecursive()) {
            return true;
        }
    }
    return false;
}

// Updates stream: SET, CREATE, MERGE and DELETE apply to exactly the rows their consumer pulls
// through them. Follow the chain of operators whose input streams into their output. A sink that
// consumes all of its input before it emits a row ends the search, and so does the build side of a
// join or cross product (child 1), which runs to completion before its probe side (child 0).
bool LogicalOperator::hasUpdateInPipeline() const {
    auto op = this;
    while (true) {
        if (LogicalOperatorUtils::isUpdate(op->getOperatorType())) {
            return true;
        }
        switch (op->getOperatorType()) {
        case LogicalOperatorType::ACCUMULATE:
        case LogicalOperatorType::AGGREGATE:
        case LogicalOperatorType::ORDER_BY:
        case LogicalOperatorType::UNION_ALL:
        // LimitPushDownOptimizer does not cap a DISTINCT that has an update in its pipeline.
        case LogicalOperatorType::DISTINCT:
            return false;
        default:
            break;
        }
        if (op->getNumChildren() == 0) {
            return false;
        }
        op = op->children[0].get();
    }
}

std::string LogicalOperator::toString(uint64_t depth) const {
    auto padding = std::string(depth * 4, ' ');
    std::string result = padding;
    result += LogicalOperatorUtils::logicalOperatorTypeToString(operatorType) + "[" +
              getExpressionsForPrinting() + "]";
    if (children.size() == 1) {
        result += "\n" + children[0]->toString(depth);
    } else {
        for (auto& child : children) {
            result += "\n" + padding + "CHILD:\n" + child->toString(depth + 1);
        }
    }
    return result;
}

logical_op_vector_t LogicalOperator::copy(const logical_op_vector_t& ops) {
    logical_op_vector_t result;
    result.reserve(ops.size());
    for (auto& op : ops) {
        result.push_back(op->copy());
    }
    return result;
}

} // namespace planner
} // namespace riag
