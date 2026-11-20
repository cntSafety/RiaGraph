#include "planner/operator/logical_assert.h"
#include "processor/expression_mapper.h"
#include "processor/operator/assert_condition.h"
#include "processor/plan_mapper.h"

using namespace riag::planner;

namespace riag {
namespace processor {

std::unique_ptr<PhysicalOperator> PlanMapper::mapAssert(const LogicalOperator* logicalOperator) {
    auto& logicalAssert = logicalOperator->constCast<LogicalAssert>();
    auto inSchema = logicalAssert.getChild(0)->getSchema();
    auto prevOperator = mapOperator(logicalOperator->getChild(0).get());
    auto exprMapper = ExpressionMapper(inSchema);
    auto evaluator = exprMapper.getEvaluator(logicalAssert.getCondition());
    auto printInfo =
        std::make_unique<AssertPrintInfo>(logicalAssert.getCondition(), logicalAssert.getMessage());
    return std::make_unique<Assert>(std::move(evaluator), logicalAssert.getMessage(),
        std::move(prevOperator), getOperatorID(), std::move(printInfo));
}

} // namespace processor
} // namespace riag
