#pragma once

#include "binder/expression/expression.h"
#include "bound_reading_clause.h"

namespace riag {
namespace binder {

// ASSERT <condition> [AS '<message>']. Without aggregates the condition is checked for every row
// that reaches the clause. With aggregates it is checked once, over all of those rows, and the rows
// are passed on unchanged. See docs/assert.md.
class BoundAssertClause final : public BoundReadingClause {
    static constexpr common::ClauseType clauseType_ = common::ClauseType::ASSERT;

public:
    BoundAssertClause(std::shared_ptr<Expression> condition, std::string message,
        expression_vector aggregates)
        : BoundReadingClause{clauseType_}, condition{std::move(condition)},
          message{std::move(message)}, aggregates{std::move(aggregates)} {}

    std::shared_ptr<Expression> getCondition() const { return condition; }
    // The text after "Assertion failed: ": the AS message, or the condition as written.
    const std::string& getMessage() const { return message; }
    bool hasAggregates() const { return !aggregates.empty(); }
    const expression_vector& getAggregates() const { return aggregates; }

private:
    std::shared_ptr<Expression> condition;
    std::string message;
    // Aggregates the condition computes itself; aggregates computed by an earlier WITH are plain
    // values here and are not listed.
    expression_vector aggregates;
};

} // namespace binder
} // namespace riag
