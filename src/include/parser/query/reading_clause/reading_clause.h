#pragma once

#include <optional>

#include "common/cast.h"
#include "common/enums/clause_type.h"
#include "parser/expression/parsed_expression.h"
#include "parser/namespace_selector.h"

namespace riag {
namespace parser {

class ReadingClause {
public:
    explicit ReadingClause(common::ClauseType clauseType) : clauseType{clauseType} {};
    virtual ~ReadingClause() = default;

    common::ClauseType getClauseType() const { return clauseType; }

    void setWherePredicate(std::unique_ptr<ParsedExpression> expression) {
        wherePredicate = std::move(expression);
    }
    bool hasWherePredicate() const { return wherePredicate != nullptr; }
    const ParsedExpression* getWherePredicate() const { return wherePredicate.get(); }

    void setNamespaceSelector(NamespaceSelector selector) {
        namespaceSelector = std::move(selector);
    }
    const std::optional<NamespaceSelector>& getNamespaceSelector() const {
        return namespaceSelector;
    }

    template<class TARGET>
    const TARGET& constCast() const {
        return common::dynamic_cast_checked<const TARGET&>(*this);
    }

private:
    common::ClauseType clauseType;
    std::unique_ptr<ParsedExpression> wherePredicate;
    std::optional<NamespaceSelector> namespaceSelector;
};
} // namespace parser
} // namespace riag
