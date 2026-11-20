#pragma once

#include <optional>

#include "parser/expression/parsed_expression.h"
#include "reading_clause.h"

namespace riag {
namespace parser {

// ASSERT <condition> [AS '<message>']. A reading clause: it passes every row on unchanged and
// fails the statement when the condition is not TRUE. See docs/assert.md.
class AssertClause final : public ReadingClause {
    static constexpr common::ClauseType clauseType_ = common::ClauseType::ASSERT;

public:
    AssertClause(std::unique_ptr<ParsedExpression> condition, std::string conditionText,
        std::optional<std::string> message)
        : ReadingClause{clauseType_}, condition{std::move(condition)},
          conditionText{std::move(conditionText)}, message{std::move(message)} {}

    const ParsedExpression* getCondition() const { return condition.get(); }
    // The condition as written, used as the failure message when AS is absent.
    const std::string& getConditionText() const { return conditionText; }
    const std::optional<std::string>& getMessage() const { return message; }

private:
    std::unique_ptr<ParsedExpression> condition;
    std::string conditionText;
    std::optional<std::string> message;
};

} // namespace parser
} // namespace riag
