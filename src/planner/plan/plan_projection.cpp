#include "binder/expression_visitor.h"
#include "binder/query/return_with_clause/bound_projection_body.h"
#include "planner/operator/scan/logical_dummy_scan.h"
#include "planner/planner.h"

using namespace riag::binder;

namespace riag {
namespace planner {

void Planner::planProjectionBody(const BoundProjectionBody* projectionBody, LogicalPlan& plan) {
    auto expressionsToProject = projectionBody->getProjectionExpressions();
    // Seed the plan before the empty-projection check, not after.
    //
    // A projection body binds to no expressions at all when everything it projects is constant
    // and nothing downstream reads it (`WITH 1 AS gate ...`). Returning here used to leave the
    // plan empty, and `planQueryPart` then appended this body's WHERE onto that empty plan --
    // building a LogicalFilter whose child is null, which segfaults the process as soon as the
    // planner asks the child for its schema.
    //
    // A projection body over no input is one row, so the dummy scan is the correct input for it
    // whether or not there is anything left to project.
    if (plan.isEmpty()) { // e.g. RETURN 1, COUNT(2)
        appendDummyScan(plan);
    }
    if (expressionsToProject.empty()) {
        // The binder drops constants from a WITH projection list (consumers evaluate them
        // themselves), so WITH DISTINCT 1 AS x and WITH 1 AS x LIMIT 1 can arrive here with nothing
        // to project. Their DISTINCT, SKIP and LIMIT still count rows -- skipping them let
        // MATCH (a) WITH 1 AS x LIMIT 1 ... go on with every a, and WITH 5 AS x LIMIT 0 CREATE ...
        // write a row. Project a placeholder, so that they have a column to act on. The binder
        // keeps the list as is when there is an ORDER BY or an aggregate.
        if (!projectionBody->isDistinct() && !projectionBody->hasSkipOrLimit()) {
            return;
        }
        expressionsToProject.push_back(LogicalDummyScan::getDummyExpression());
    }
    auto expressionsToAggregate = projectionBody->getAggregateExpressions();
    auto expressionsToGroupBy = projectionBody->getGroupByExpressions();
    if (!expressionsToAggregate.empty()) {
        planAggregate(expressionsToAggregate, expressionsToGroupBy, plan);
    }
    // We might order by an expression that is not in projection list, so after order by we
    // always need to append a projection.
    // If distinct is presented in projection list, we need to first append project to evaluate the
    // list, then take the distinct.
    // Order by should always be the last operator (except for skip/limit) because other operators
    // will break the order.
    if (projectionBody->isDistinct() && projectionBody->hasOrderByExpressions()) {
        appendProjection(expressionsToProject, plan);
        appendDistinct(expressionsToProject, plan);
        planOrderBy(expressionsToProject, projectionBody->getOrderByExpressions(),
            projectionBody->getSortingOrders(), plan);
        appendProjection(expressionsToProject, plan);
    } else if (projectionBody->isDistinct()) {
        appendProjection(expressionsToProject, plan);
        appendDistinct(expressionsToProject, plan);
    } else if (projectionBody->hasOrderByExpressions()) {
        planOrderBy(expressionsToProject, projectionBody->getOrderByExpressions(),
            projectionBody->getSortingOrders(), plan);
        appendProjection(expressionsToProject, plan);
    } else {
        appendProjection(expressionsToProject, plan);
    }
    if (projectionBody->hasSkipOrLimit()) {
        // A LIMIT stops pulling once it has its rows, so a write in its pipeline would apply to
        // only the first limit + 1 of them: MATCH (a) SET a.v = 1 RETURN a LIMIT 1 set two rows.
        // The write's clause completes before the LIMIT applies, so materialize its output. An
        // aggregate, DISTINCT or ORDER BY in this projection body already consumes every row.
        if (projectionBody->hasLimit() && plan.hasUpdateInPipeline()) {
            appendAccumulate(plan);
        }
        appendMultiplicityReducer(plan);
        appendLimit(projectionBody->getSkipNumber(), projectionBody->getLimitNumber(), plan);
    }
}

void Planner::planAggregate(const expression_vector& expressionsToAggregate,
    const expression_vector& expressionsToGroupBy, LogicalPlan& plan) {
    DASSERT(!expressionsToAggregate.empty());
    expression_vector expressionsToProject;
    for (auto& expressionToAggregate : expressionsToAggregate) {
        if (ExpressionChildrenCollector::collectChildren(*expressionToAggregate)
                .empty()) { // skip COUNT(*)
            continue;
        }
        expressionsToProject.push_back(expressionToAggregate->getChild(0));
    }
    for (auto& expressionToGroupBy : expressionsToGroupBy) {
        expressionsToProject.push_back(expressionToGroupBy);
    }
    appendProjection(expressionsToProject, plan);
    appendAggregate(expressionsToGroupBy, expressionsToAggregate, plan);
}

void Planner::planOrderBy(const binder::expression_vector& expressionsToProject,
    const binder::expression_vector& expressionsToOrderBy, const std::vector<bool>& isAscOrders,
    LogicalPlan& plan) {
    auto expressionsToProjectBeforeOrderBy = expressionsToProject;
    auto expressionsToProjectSet =
        expression_set{expressionsToProject.begin(), expressionsToProject.end()};
    for (auto& expression : expressionsToOrderBy) {
        if (!expressionsToProjectSet.contains(expression)) {
            expressionsToProjectBeforeOrderBy.push_back(expression);
        }
    }
    appendProjection(expressionsToProjectBeforeOrderBy, plan);
    appendOrderBy(expressionsToOrderBy, isAscOrders, plan);
}

} // namespace planner
} // namespace riag
