#include "optimizer/limit_push_down_optimizer.h"

#include <algorithm>

#include "binder/expression/expression_util.h"
#include "planner/operator/extend/logical_recursive_extend.h"
#include "planner/operator/logical_distinct.h"
#include "planner/operator/logical_hash_join.h"
#include "planner/operator/logical_limit.h"
#include "planner/operator/logical_table_function_call.h"

using namespace riag::binder;
using namespace riag::common;
using namespace riag::planner;

namespace riag {
namespace optimizer {

// A LIMIT/SKIP cap may only be pushed through operators that never reduce the number of rows
// they emit relative to their input (i.e. 1:1 or 1:many operators). Any operator that can
// discard rows -- FILTER, DISTINCT, joins, aggregates, UNWIND of empty lists, etc. -- would
// leave the parent LIMIT with too few surviving rows if a static cap were placed below it, so
// those are hard barriers. The cap is instead applied directly to operators that support early
// termination (TABLE_FUNCTION_CALL, DISTINCT, and the recursive extend behind its 1:1
// property-probe join, see visitOperator below).
static bool isPushDownSupported(planner::LogicalOperator* op) {
    switch (op->getOperatorType()) {
    case LogicalOperatorType::MULTIPLICITY_REDUCER:
    case LogicalOperatorType::EXPLAIN:
    case LogicalOperatorType::ACCUMULATE:
    case LogicalOperatorType::PROJECTION:
        return true;
    default:
        return false;
    }
}

void LimitPushDownOptimizer::rewrite(LogicalPlan* plan) {
    visitOperator(plan->getLastOperator().get());
}

void LimitPushDownOptimizer::visitOperator(planner::LogicalOperator* op) {
    switch (op->getOperatorType()) {
    case LogicalOperatorType::LIMIT: {
        auto& limit = op->constCast<LogicalLimit>();
        // Without a known number there is no finite row demand to hand down.
        if ((limit.hasSkipNum() && !ExpressionUtil::canEvaluateAsLiteral(*limit.getSkipNum())) ||
            (limit.hasLimitNum() && !ExpressionUtil::canEvaluateAsLiteral(*limit.getLimitNum()))) {
            return;
        }
        offset_t innerSkip = 0;
        offset_t innerLimit = INVALID_LIMIT;
        if (limit.hasSkipNum()) {
            innerSkip = ExpressionUtil::evaluateAsSkipLimit(*limit.getSkipNum());
        }
        if (limit.hasLimitNum()) {
            innerLimit = ExpressionUtil::evaluateAsSkipLimit(*limit.getLimitNum());
        }
        // This operator sits below the ones already visited, so (skipNumber, limitNumber) is
        // the SKIP/LIMIT that applies to its output. Stacked SKIP/LIMITs compose: the rows kept
        // are those this operator keeps, minus the first skipNumber of them, at most limitNumber
        // of those. Overwriting either number instead loses the clauses above (two SKIPs above
        // a DISTINCT capped it at the inner skip + the outer limit).
        offset_t remaining = innerLimit;
        if (innerLimit != INVALID_LIMIT) {
            remaining = innerLimit > skipNumber ? innerLimit - skipNumber : 0;
        }
        // On overflow the saturated skip makes the operators below refuse the push down.
        skipNumber =
            innerSkip > INVALID_LIMIT - skipNumber ? INVALID_LIMIT : innerSkip + skipNumber;
        limitNumber = std::min(remaining, limitNumber);
        visitOperator(limit.getChild(0).get());
        return;
    }
    case LogicalOperatorType::TABLE_FUNCTION_CALL: {
        // SKIP without LIMIT has no finite row demand and skip + limit may overflow.
        if (limitNumber == INVALID_LIMIT || skipNumber >= INVALID_LIMIT - limitNumber) {
            return;
        }
        auto& tableFuncCall = op->cast<LogicalTableFunctionCall>();
        if (tableFuncCall.getTableFunc().supportsPushDownFunc()) {
            tableFuncCall.setLimitNum(skipNumber + limitNumber);
        }
        return;
    }
    case LogicalOperatorType::DISTINCT: {
        // The physical DISTINCT is capped at skip + limit distinct groups, after which the
        // remaining LIMIT/SKIP operator on top selects the requested rows. SKIP without LIMIT
        // must not cap DISTINCT, and skip + limit must not overflow.
        if (limitNumber == INVALID_LIMIT || skipNumber >= INVALID_LIMIT - limitNumber) {
            return;
        }
        // A capped DISTINCT stops pulling once it holds enough groups, so a write in its pipeline
        // would apply to only the rows pulled so far: MATCH (a) SET a.v = 1 RETURN DISTINCT a.v
        // LIMIT 1 set one row. Planner::planProjectionBody relies on this: it does not
        // materialize a DISTINCT's output before a LIMIT.
        if (op->getChild(0)->hasUpdateInPipeline()) {
            return;
        }
        auto& distinctOp = op->cast<LogicalDistinct>();
        distinctOp.setLimitNum(limitNumber);
        distinctOp.setSkipNum(skipNumber);
        return;
    }
    case LogicalOperatorType::HASH_JOIN: {
        if (limitNumber == INVALID_LIMIT) {
            return;
        }
        auto& hashJoin = op->cast<LogicalHashJoin>();
        // The recursive-extend join created by Planner::appendRecursiveExtend is a 1:1 INNER
        // join that reads the bound node properties back after path expansion. Only that direct
        // shape is safe: nested joins, filtering joins, and multiplicative joins must remain
        // barriers because their first N probe rows may produce fewer than N final rows.
        if (hashJoin.getJoinType() != JoinType::INNER || hashJoin.requireFlatProbeKeys()) {
            return;
        }
        if (op->getChild(0)->getOperatorType() != LogicalOperatorType::PATH_PROPERTY_PROBE ||
            op->getChild(0)->getChild(0)->getOperatorType() !=
                LogicalOperatorType::RECURSIVE_EXTEND ||
            skipNumber >= INVALID_LIMIT - limitNumber) {
            return;
        }
        auto& extend = op->getChild(0)->getChild(0)->cast<LogicalRecursiveExtend>();
        extend.setLimitNum(skipNumber + limitNumber);
        return;
    }
    case LogicalOperatorType::UNION_ALL: {
        for (auto i = 0u; i < op->getNumChildren(); ++i) {
            auto optimizer = LimitPushDownOptimizer();
            optimizer.visitOperator(op->getChild(i).get());
        }
        return;
    }
    default: {
        // Only row-count-preserving operators let the cap pass through; FILTER and every
        // other operator that can alter the number of rows is a barrier.
        if (isPushDownSupported(op)) {
            visitOperator(op->getChild(0).get());
        }
        return;
    }
    }
}

} // namespace optimizer
} // namespace riag
