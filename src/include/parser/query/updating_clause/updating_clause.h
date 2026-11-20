#pragma once

#include <optional>

#include "common/cast.h"
#include "common/enums/clause_type.h"
#include "parser/namespace_selector.h"

namespace riag {
namespace parser {

class UpdatingClause {
public:
    explicit UpdatingClause(common::ClauseType clauseType) : clauseType{clauseType} {};
    virtual ~UpdatingClause() = default;

    common::ClauseType getClauseType() const { return clauseType; }

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
    std::optional<NamespaceSelector> namespaceSelector;
};

} // namespace parser
} // namespace riag
