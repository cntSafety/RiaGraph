#pragma once

#include "binder/expression/expression.h"
#include "planner/operator/logical_operator.h"

namespace riag {
namespace planner {

struct LogicalAssertPrintInfo final : OPPrintInfo {
    std::shared_ptr<binder::Expression> condition;
    explicit LogicalAssertPrintInfo(std::shared_ptr<binder::Expression> condition)
        : condition{std::move(condition)} {}
    std::string toString() const override { return condition->toString(); }
};

// Checks a condition on every row and passes the row on unchanged; fails the statement with
// "Assertion failed: <message>" when the condition is FALSE or NULL. Unlike FILTER it never drops
// a row, so no optimizer may move it, merge it with a filter or skip the subtree below it: it is
// classified as a side effect (LogicalOperatorUtils::isUpdate) for that reason.
class LogicalAssert final : public LogicalOperator {
    static constexpr LogicalOperatorType type_ = LogicalOperatorType::ASSERT;

public:
    LogicalAssert(std::shared_ptr<binder::Expression> condition, std::string message,
        std::shared_ptr<LogicalOperator> child)
        : LogicalOperator{type_, std::move(child)}, condition{std::move(condition)},
          message{std::move(message)} {}

    void computeFactorizedSchema() override { copyChildSchema(0); }
    void computeFlatSchema() override { copyChildSchema(0); }

    f_group_pos_set getGroupsPosToFlatten() const;

    std::string getExpressionsForPrinting() const override { return condition->toString(); }

    std::shared_ptr<binder::Expression> getCondition() const { return condition; }
    const std::string& getMessage() const { return message; }

    std::unique_ptr<OPPrintInfo> getPrintInfo() const override {
        return std::make_unique<LogicalAssertPrintInfo>(condition);
    }

    std::unique_ptr<LogicalOperator> copy() override {
        return std::make_unique<LogicalAssert>(condition, message, children[0]->copy());
    }

private:
    std::shared_ptr<binder::Expression> condition;
    std::string message;
};

} // namespace planner
} // namespace riag
