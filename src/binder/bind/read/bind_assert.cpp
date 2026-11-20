#include <unordered_set>
#include <utility>

#include "binder/binder.h"
#include "binder/expression/expression_util.h"
#include "binder/expression/property_expression.h"
#include "binder/expression_visitor.h"
#include "binder/query/reading_clause/bound_assert_clause.h"
#include "common/exception/binder.h"
#include "parser/query/reading_clause/assert_clause.h"
#include <format>

using namespace riag::common;
using namespace riag::parser;

namespace riag {
namespace binder {

namespace {

// Finds a part of an aggregating ASSERT condition that reads the current row outside of an
// aggregate. Such a condition has no single value: ASSERT count(*) > 0 AND a.x > 0 checks all rows
// at once and each row at a time. A WITH that groups first gives it one.
class RowReferenceFinder {
public:
    explicit RowReferenceFinder(const expression_vector& scopeExprs) {
        for (auto& expr : scopeExprs) {
            scopePtrs.insert(expr.get());
            scopeNames.insert(expr->getUniqueName());
        }
    }

    const Expression* find(const std::shared_ptr<Expression>& expr) const {
        if (scopePtrs.contains(expr.get())) {
            return expr.get();
        }
        switch (expr->expressionType) {
        case ExpressionType::AGGREGATE_FUNCTION:
            // Aggregates in scope were caught above. The others are computed by this ASSERT.
            return nullptr;
        case ExpressionType::PROPERTY: {
            auto& property = expr->constCast<PropertyExpression>();
            return scopeNames.contains(property.getVariableName()) ? expr.get() : nullptr;
        }
        case ExpressionType::SUBQUERY: {
            // A subquery that shares a node with the outer query is evaluated per row.
            auto collector = DependentVarNameCollector();
            collector.visit(expr);
            for (auto& name : collector.getVarNames()) {
                if (scopeNames.contains(name)) {
                    return expr.get();
                }
            }
            return nullptr;
        }
        default:
            for (auto& child : ExpressionChildrenCollector::collectChildren(*expr)) {
                if (auto found = find(child)) {
                    return found;
                }
            }
            return nullptr;
        }
    }

private:
    std::unordered_set<const Expression*> scopePtrs;
    std::unordered_set<std::string> scopeNames;
};

} // namespace

std::unique_ptr<BoundReadingClause> Binder::bindAssertClause(const ReadingClause& readingClause) {
    auto& assertClause = readingClause.constCast<AssertClause>();
    // Bind like a projection item so that an aggregate structurally equal to one an earlier clause
    // computed gets a column of its own, e.g. count(x) in WITH x, count(x) AS c ASSERT count(x) >
    // 0.
    auto& config = expressionBinder.config;
    auto outerAggregateNames = std::exchange(config.projectionAggregateNames, {});
    auto outerBindingProjectionList = std::exchange(config.bindingProjectionList, true);
    auto condition = expressionBinder.bindExpression(*assertClause.getCondition());
    config.bindingProjectionList = outerBindingProjectionList;
    config.projectionAggregateNames = std::move(outerAggregateNames);
    condition = expressionBinder.implicitCastIfNecessary(condition, LogicalType::BOOL());
    ExpressionUtil::validateDataType(*condition, LogicalType::BOOL());
    auto aggregates = getNewAggregateExpressions(condition);
    if (!aggregates.empty()) {
        auto finder = RowReferenceFinder(scope.getExpressions());
        if (auto rowReference = finder.find(condition)) {
            throw BinderException(std::format(
                "ASSERT condition {} mixes aggregates with {}, which is not aggregated. Aggregate "
                "it as well, or compute the aggregates with WITH first.",
                assertClause.getConditionText(), rowReference->toString()));
        }
    }
    auto message = assertClause.getMessage().has_value() ? assertClause.getMessage().value() :
                                                           assertClause.getConditionText();
    return std::make_unique<BoundAssertClause>(std::move(condition), std::move(message),
        std::move(aggregates));
}

} // namespace binder
} // namespace riag
