#include "processor/operator/assert_condition.h"

#include "binder/expression/expression.h" // IWYU pragma: keep
#include "common/exception/assertion.h"
#include "common/metric.h"
#include "processor/execution_context.h"

using namespace riag::common;

namespace riag {
namespace processor {

std::string AssertPrintInfo::toString() const {
    return condition->toString() + " AS '" + message + "'";
}

void Assert::initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) {
    conditionEvaluator->init(*resultSet, context->clientContext);
}

bool Assert::getNextTuplesInternal(ExecutionContext* context) {
    if (!children[0]->getNextTuple(context)) {
        return false;
    }
    conditionEvaluator->evaluate();
    auto& result = *conditionEvaluator->resultVector;
    // The planner flattens all but one of the groups the condition reads, so the result holds one
    // value per tuple of that group, or a single value when every group it reads is flat.
    auto& selVector = result.state->getSelVector();
    for (auto i = 0u; i < selVector.getSelSize(); ++i) {
        auto pos = selVector[i];
        if (result.isNull(pos) || !result.getValue<bool>(pos)) {
            throw AssertionException(message);
        }
    }
    metrics->numOutputTuple.increase(selVector.getSelSize());
    return true;
}

} // namespace processor
} // namespace riag
