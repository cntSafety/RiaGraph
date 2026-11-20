#pragma once

#include "expression_evaluator/expression_evaluator.h"
#include "processor/operator/physical_operator.h"

namespace riag {
namespace processor {

struct AssertPrintInfo final : OPPrintInfo {
    std::shared_ptr<binder::Expression> condition;
    std::string message;

    AssertPrintInfo(std::shared_ptr<binder::Expression> condition, std::string message)
        : condition{std::move(condition)}, message{std::move(message)} {}

    std::string toString() const override;

    std::unique_ptr<OPPrintInfo> copy() const override {
        return std::unique_ptr<AssertPrintInfo>(new AssertPrintInfo(*this));
    }

private:
    AssertPrintInfo(const AssertPrintInfo& other)
        : OPPrintInfo{other}, condition{other.condition}, message{other.message} {}
};

// Evaluates a condition on every tuple it receives and passes the tuples on unchanged. Throws
// AssertionException("Assertion failed: <message>") when the condition is FALSE or NULL for one of
// them. The message does not depend on the failing row, so whichever thread fails first reports
// the same error.
class Assert final : public PhysicalOperator {
    static constexpr PhysicalOperatorType type_ = PhysicalOperatorType::ASSERT;

public:
    Assert(std::unique_ptr<evaluator::ExpressionEvaluator> conditionEvaluator, std::string message,
        std::unique_ptr<PhysicalOperator> child, uint32_t id,
        std::unique_ptr<OPPrintInfo> printInfo)
        : PhysicalOperator{type_, std::move(child), id, std::move(printInfo)},
          conditionEvaluator{std::move(conditionEvaluator)}, message{std::move(message)} {}

    void initLocalStateInternal(ResultSet* resultSet, ExecutionContext* context) override;

    bool getNextTuplesInternal(ExecutionContext* context) override;

    std::unique_ptr<PhysicalOperator> copy() override {
        return std::make_unique<Assert>(conditionEvaluator->copy(), message, children[0]->copy(),
            id, printInfo->copy());
    }

private:
    std::unique_ptr<evaluator::ExpressionEvaluator> conditionEvaluator;
    std::string message;
};

} // namespace processor
} // namespace riag
