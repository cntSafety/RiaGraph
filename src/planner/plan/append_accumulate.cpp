#include "planner/operator/logical_accumulate.h"
#include "planner/operator/scan/logical_dummy_scan.h"
#include "planner/planner.h"

using namespace riag::binder;
using namespace riag::common;

namespace riag {
namespace planner {

void Planner::tryAppendAccumulate(LogicalPlan& plan) {
    if (plan.getLastOperator()->getOperatorType() == LogicalOperatorType::ACCUMULATE) {
        return;
    }
    appendAccumulate(plan);
}

void Planner::appendAccumulate(LogicalPlan& plan) {
    appendAccumulate(AccumulateType::REGULAR, expression_vector{}, nullptr /* mark */, plan);
}

void Planner::appendOptionalAccumulate(std::shared_ptr<Expression> mark, LogicalPlan& plan) {
    appendAccumulate(AccumulateType::OPTIONAL_, expression_vector{}, mark, plan);
}

void Planner::appendAccumulate(const expression_vector& flatExprs, LogicalPlan& plan) {
    appendAccumulate(AccumulateType::REGULAR, flatExprs, nullptr /* mark */, plan);
}

void Planner::appendAccumulate(AccumulateType accumulateType, const expression_vector& flatExprs,
    std::shared_ptr<Expression> mark, LogicalPlan& plan) {
    // A materialized table holds its rows only through their columns: an input with nothing in
    // scope is materialized as no column, so the table keeps no row and everything above the
    // accumulate runs zero times instead of once per input row. WITH 5 AS x CREATE (:T {id: x})
    // created nothing that way -- the binder drops the constant from the WITH projection, so the
    // write's input is a bare dummy scan. Give such rows a column to be counted by.
    if (plan.getSchema()->getExpressionsInScope().empty()) {
        appendProjection(expression_vector{LogicalDummyScan::getDummyExpression()}, plan);
    }
    auto op =
        make_shared<LogicalAccumulate>(accumulateType, flatExprs, mark, plan.getLastOperator());
    appendFlattens(op->getGroupPositionsToFlatten(), plan);
    op->setChild(0, plan.getLastOperator());
    op->computeFactorizedSchema();
    plan.setLastOperator(std::move(op));
}

} // namespace planner
} // namespace riag
