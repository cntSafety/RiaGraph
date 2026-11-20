#pragma once

#include "common/types/value/value.h"
#include "expression.h"

namespace riag {
namespace binder {

class RIAG_API LiteralExpression final : public Expression {
    static constexpr common::ExpressionType type_ = common::ExpressionType::LITERAL;

public:
    LiteralExpression(common::Value value, const std::string& uniqueName)
        : Expression{type_, value.getDataType().copy(), uniqueName}, value{std::move(value)} {}
    // A literal the binder puts in place of an expression it knows to be constant, printed as that
    // expression: its text names a result column and reads as the expression it replaced. Unlike
    // an alias, it does not count as the AS of a WITH item.
    LiteralExpression(common::Value value, const std::string& uniqueName, std::string text)
        : Expression{type_, value.getDataType().copy(), uniqueName}, value{std::move(value)},
          text{std::move(text)} {}

    bool isNull() const { return value.isNull(); }

    void cast(const common::LogicalType& type) override;

    common::Value getValue() const { return value; }

    std::string toStringInternal() const override { return text.empty() ? value.toString() : text; }

public:
    common::Value value;

private:
    std::string text;
};

} // namespace binder
} // namespace riag
