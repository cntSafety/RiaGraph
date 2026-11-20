#include "binder/query/reading_clause/bound_assert_clause.h"
#include "planner/operator/factorization/flatten_resolver.h"
#include "planner/operator/logical_assert.h"
#include "planner/operator/logical_cross_product.h"
#include "planner/operator/scan/logical_dummy_scan.h"
#include "planner/planner.h"

using namespace riag::binder;
using namespace riag::common;

namespace riag {
namespace planner {

// The outer columns an aggregate's arguments read: what the check plan has to scan back.
static expression_vector getAggregateInputs(const expression_vector& aggregates,
    const Schema& schema) {
    expression_set seen;
    expression_vector inputs;
    for (auto& aggregate : aggregates) {
        for (auto& child : aggregate->getChildren()) {
            auto analyzer = GroupDependencyAnalyzer(true /* collectDependentExpr */, schema);
            analyzer.visit(child);
            for (auto& expr : analyzer.getDependentExprs()) {
                if (seen.insert(expr).second) {
                    inputs.push_back(expr);
                }
            }
        }
    }
    return inputs;
}

void Planner::planAssertClause(const BoundReadingClause& readingClause, LogicalPlan& plan) {
    auto& assertClause = readingClause.constCast<BoundAssertClause>();
    if (plan.isEmpty()) { // ASSERT $v > 0 at the start of a query is checked once, like RETURN 1.
        appendDummyScan(plan);
    }
    if (!assertClause.hasAggregates()) {
        appendAssert(assertClause.getCondition(), assertClause.getMessage(), plan);
        return;
    }
    // An aggregating ASSERT checks all rows that reach it at once, and passes them on unchanged:
    // MATCH (b) ASSERT count(b) > 0 RETURN b returns every b. The rows are materialized, a check
    // plan scans them back, aggregates and asserts, and the one row it yields (it fails or yields
    // exactly one) is crossed with the materialized rows. The check plan reads the materialized
    // table, so it is built after it: the same plumbing as a correlated subquery, with a cross
    // product instead of a hash join because a global aggregate has no join key.
    if (plan.getSchema()->getExpressionsInScope().empty()) {
        // A dummy scan's row has no column to materialize (ASSERT count(*) = 1 at the start of a
        // query); give it one.
        appendProjection(expression_vector{LogicalDummyScan::getDummyExpression()}, plan);
    }
    auto outerExprs = plan.getSchema()->getExpressionsInScope();
    auto aggregates = assertClause.getAggregates();
    auto inputs = getAggregateInputs(aggregates, *plan.getSchema());
    if (inputs.empty()) {
        // E.g. COUNT(*): any column of the materialized rows carries their number.
        inputs.push_back(outerExprs[0]);
    }
    // Flatten every group, so that one row of the table is one row of the clause's input: a
    // factorized row stands for several, which the scan back would count once.
    appendAccumulate(outerExprs, plan);
    auto checkPlan = LogicalPlan();
    appendExpressionsScan(inputs, checkPlan);
    planAggregate(aggregates, expression_vector{} /* group by */, checkPlan);
    appendAssert(assertClause.getCondition(), assertClause.getMessage(), checkPlan);
    appendCrossProduct(plan, checkPlan, plan);
    auto& sipInfo = plan.getLastOperator()->cast<LogicalCrossProduct>().getSIPInfoUnsafe();
    sipInfo.dependency = SIPDependency::BUILD_DEPENDS_ON_PROBE;
    sipInfo.direction = SIPDirection::PROBE_TO_BUILD;
    // Drop the aggregates again: the rows leave the clause as they entered it, and a later clause
    // that computes the same aggregate (ASSERT count(b) > 0 ASSERT count(b) < 9) gets its own.
    appendProjection(outerExprs, plan);
}

void Planner::appendAssert(const std::shared_ptr<Expression>& condition, std::string message,
    LogicalPlan& plan) {
    planSubqueryIfNecessary(condition, plan);
    auto op =
        std::make_shared<LogicalAssert>(condition, std::move(message), plan.getLastOperator());
    appendFlattens(op->getGroupsPosToFlatten(), plan);
    op->setChild(0, plan.getLastOperator());
    op->computeFactorizedSchema();
    op->setCardinality(plan.getLastOperator()->getCardinality());
    plan.setLastOperator(std::move(op));
}

} // namespace planner
} // namespace riag
