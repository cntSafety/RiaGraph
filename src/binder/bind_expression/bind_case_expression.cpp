#include "binder/binder.h"
#include "binder/expression/case_expression.h"
#include "binder/expression/expression_util.h"
#include "binder/expression_binder.h"
#include "parser/expression/parsed_case_expression.h"

using namespace riag::common;
using namespace riag::parser;

namespace riag {
namespace binder {

// A map, or a list of maps (at any depth).
static bool isMapShaped(const LogicalType& type) {
    switch (type.getLogicalTypeID()) {
    case LogicalTypeID::STRUCT:
        return true;
    case LogicalTypeID::LIST:
        return isMapShaped(ListType::getChildType(type));
    case LogicalTypeID::ARRAY:
        return isMapShaped(ArrayType::getChildType(type));
    default:
        return false;
    }
}

std::shared_ptr<Expression> ExpressionBinder::bindCaseExpression(
    const ParsedExpression& parsedExpression) {
    auto& parsedCaseExpression = parsedExpression.constCast<ParsedCaseExpression>();
    auto resultType = LogicalType::ANY();
    expression_vector boundResults;
    // Resolve result type by checking each then expression type.
    for (auto i = 0u; i < parsedCaseExpression.getNumCaseAlternative(); ++i) {
        auto alternative = parsedCaseExpression.getCaseAlternative(i);
        auto boundThen = bindExpression(*alternative->thenExpression);
        if (boundThen->getDataType().getLogicalTypeID() != LogicalTypeID::ANY) {
            resultType = boundThen->getDataType().copy();
        }
        boundResults.push_back(std::move(boundThen));
    }
    if (parsedCaseExpression.hasElseExpression()) {
        auto elseExpression = bindExpression(*parsedCaseExpression.getElseExpression());
        // Resolve result type by else expression if above resolving fails.
        if (resultType.getLogicalTypeID() == LogicalTypeID::ANY) {
            resultType = elseExpression->getDataType().copy();
        }
        boundResults.push_back(std::move(elseExpression));
    }
    // Maps in different branches may list their keys in another order or have other keys.
    // Combine them by name, like a list literal or coalesce does, so that no branch has to be
    // cast to another branch's keys (cntSafety/RiaGraph#193). Other result types keep the
    // inherited rule above.
    if (isMapShaped(resultType)) {
        LogicalType combinedType(LogicalTypeID::ANY);
        if (ExpressionUtil::tryCombineDataType(boundResults, combinedType) &&
            isMapShaped(combinedType)) {
            resultType = std::move(combinedType);
        }
    }
    auto name = binder->getUniqueExpressionName(parsedExpression.getRawName());
    // bind ELSE ...
    std::shared_ptr<Expression> elseExpression;
    if (parsedCaseExpression.hasElseExpression()) {
        elseExpression = bindExpression(*parsedCaseExpression.getElseExpression());
    } else {
        elseExpression = createNullLiteralExpression();
    }
    elseExpression = implicitCastIfNecessary(elseExpression, resultType);
    auto boundCaseExpression =
        make_shared<CaseExpression>(resultType.copy(), std::move(elseExpression), name);
    // bind WHEN ... THEN ...
    if (parsedCaseExpression.hasCaseExpression()) {
        auto boundCase = bindExpression(*parsedCaseExpression.getCaseExpression());
        for (auto i = 0u; i < parsedCaseExpression.getNumCaseAlternative(); ++i) {
            auto caseAlternative = parsedCaseExpression.getCaseAlternative(i);
            auto boundWhen = bindExpression(*caseAlternative->whenExpression);
            boundWhen = implicitCastIfNecessary(boundWhen, boundCase->dataType);
            // rewrite "CASE a.age WHEN 1" as "CASE WHEN a.age = 1"
            if (ExpressionUtil::isNullLiteral(*boundWhen)) {
                boundWhen = bindNullOperatorExpression(ExpressionType::IS_NULL,
                    expression_vector{boundWhen});
            } else {
                boundWhen = bindComparisonExpression(ExpressionType::EQUALS,
                    expression_vector{boundCase, boundWhen});
            }
            auto boundThen = bindExpression(*caseAlternative->thenExpression);
            boundThen = implicitCastIfNecessary(boundThen, resultType);
            boundCaseExpression->addCaseAlternative(boundWhen, boundThen);
        }
    } else {
        for (auto i = 0u; i < parsedCaseExpression.getNumCaseAlternative(); ++i) {
            auto caseAlternative = parsedCaseExpression.getCaseAlternative(i);
            auto boundWhen = bindExpression(*caseAlternative->whenExpression);
            boundWhen = implicitCastIfNecessary(boundWhen, LogicalType::BOOL());
            auto boundThen = bindExpression(*caseAlternative->thenExpression);
            boundThen = implicitCastIfNecessary(boundThen, resultType);
            boundCaseExpression->addCaseAlternative(boundWhen, boundThen);
        }
    }
    return boundCaseExpression;
}

} // namespace binder
} // namespace riag
