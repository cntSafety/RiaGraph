#include "optimizer/remove_unnecessary_order_by_optimizer.h"

#include "binder/expression/aggregate_function_expression.h"
#include "function/aggregate/count.h"
#include "function/aggregate/count_star.h"
#include "function/aggregate_function.h"
#include "planner/operator/logical_aggregate.h"
#include "planner/operator/logical_limit.h"
#include "planner/operator/logical_order_by.h"
#include "planner/operator/logical_projection.h"

using namespace riag::common;
using namespace riag::planner;

namespace riag {
namespace optimizer {

void RemoveUnnecessaryOrderByOptimizer::rewrite(LogicalPlan* plan) {
    plan->setLastOperator(visitOperator(plan->getLastOperator()));
}

std::shared_ptr<planner::LogicalOperator> RemoveUnnecessaryOrderByOptimizer::visitOperator(
    std::shared_ptr<planner::LogicalOperator> op) {
    // bottom-up traversal: rewrite children first
    for (auto i = 0u; i < op->getNumChildren(); ++i) {
        op->setChild(i, visitOperator(op->getChild(i)));
    }
    auto result = tryRemoveOrderByFromAggregate(std::move(op));
    // This optimizer runs after FactorizationRewriter, so the plan is in
    // factorized form. Recompute the factorized schema (computeFlatSchema would
    // be incorrect here and would also throw on FLATTEN nodes).
    result->computeFactorizedSchema();
    return result;
}

std::shared_ptr<planner::LogicalOperator>
RemoveUnnecessaryOrderByOptimizer::tryRemoveOrderByFromAggregate(
    std::shared_ptr<planner::LogicalOperator> op) {
    if (op->getOperatorType() != LogicalOperatorType::AGGREGATE) {
        return op;
    }
    auto& aggregate = op->cast<LogicalAggregate>();

    // Only optimize aggregates without GROUP BY keys (e.g. COUNT(*))
    if (aggregate.hasKeys()) {
        return op;
    }
    // The order only stops mattering for aggregates that ignore it: collect() returns the rows
    // in input order, and an aggregate this code does not know may depend on it too.
    bool onlyCountStar = true;
    for (auto& expression : aggregate.getAggregates()) {
        const auto& name =
            expression->constCast<binder::AggregateFunctionExpression>().getFunction().name;
        if (name != function::CountStarFunction::name && name != function::CountFunction::name &&
            name != function::AggregateMinFunction::name &&
            name != function::AggregateMaxFunction::name &&
            name != function::AggregateSumFunction::name &&
            name != function::AggregateAvgFunction::name) {
            return op;
        }
        onlyCountStar &= name == function::CountStarFunction::name;
    }

    // Walk the child chain looking for ORDER BY to remove. If an ORDER BY is
    // found (possibly through passthrough operators), it is replaced with its
    // child, optionally wrapped in a LIMIT when the ORDER BY had absorbed one.
    bool changed = false;
    auto newChild = stripOrderByBelow(op->getChild(0), changed, onlyCountStar, false);
    if (changed) {
        op->setChild(0, std::move(newChild));
    }
    return op;
}

// A LIMIT or SKIP keeps the rows the order puts first, so below one the ORDER BY decides
// which rows the aggregate sees: `WITH x ORDER BY x DESC LIMIT 3 RETURN sum(x)` summed the
// first three rows read instead of the three largest. Only COUNT(*) is then independent of
// it, and only when no FILTER above the cut sees the chosen rows.
static bool canCutRowsWithoutOrder(bool onlyCountStar, bool sawFilter) {
    return onlyCountStar && !sawFilter;
}

std::shared_ptr<planner::LogicalOperator> RemoveUnnecessaryOrderByOptimizer::stripOrderByBelow(
    std::shared_ptr<planner::LogicalOperator> op, bool& changed, bool onlyCountStar,
    bool sawFilter) {
    switch (op->getOperatorType()) {
    case LogicalOperatorType::ORDER_BY: {
        // `op` is the ORDER BY to remove. Replace it with its child, preserving
        // any absorbed LIMIT (TOP_K) as a standalone LIMIT so the row count
        // is unaffected.
        auto& orderBy = op->cast<LogicalOrderBy>();
        if ((orderBy.hasLimitNum() || orderBy.hasSkipNum()) &&
            !canCutRowsWithoutOrder(onlyCountStar, sawFilter)) {
            return op;
        }
        auto grandChild = orderBy.getChild(0);
        std::shared_ptr<planner::LogicalOperator> replacement;
        if (orderBy.hasLimitNum()) {
            auto limitExpr = orderBy.getLimitNum();
            auto skipExpr = orderBy.hasSkipNum() ? orderBy.getSkipNum() : nullptr;
            auto limitOp = std::make_shared<LogicalLimit>(skipExpr, limitExpr, grandChild);
            // The newly created Limit has no schema yet; compute it now so that
            // the parent operator can safely copy it.
            limitOp->computeFactorizedSchema();
            replacement = std::move(limitOp);
        } else {
            replacement = grandChild;
        }
        changed = true;
        return replacement;
    }
    case LogicalOperatorType::PROJECTION:
    case LogicalOperatorType::MULTIPLICITY_REDUCER:
    case LogicalOperatorType::LIMIT:
    case LogicalOperatorType::FILTER: {
        // Passthrough operator: recurse into its single child. If a removal
        // happened deeper, rewire the child and recompute this operator's schema.
        if (op->getNumChildren() != 1) {
            return op;
        }
        if (op->getOperatorType() == LogicalOperatorType::LIMIT &&
            !canCutRowsWithoutOrder(onlyCountStar, sawFilter)) {
            return op;
        }
        bool childChanged = false;
        auto newChild = stripOrderByBelow(op->getChild(0), childChanged, onlyCountStar,
            sawFilter || op->getOperatorType() == LogicalOperatorType::FILTER);
        if (childChanged) {
            op->setChild(0, std::move(newChild));
            op->computeFactorizedSchema();
            changed = true;
        }
        return op;
    }
    default:
        // Don't traverse into other operators (joins, scans, extends, ...);
        // an ORDER BY buried below them is not safe to reach through here.
        return op;
    }
}

} // namespace optimizer
} // namespace riag
