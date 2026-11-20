#pragma once

#include "parser/expression/parsed_expression.h"
#include "updating_clause.h"

namespace riag {
namespace parser {

class SetClause final : public UpdatingClause {
public:
    SetClause() : UpdatingClause{common::ClauseType::SET} {};

    inline void addSetItem(parsed_expr_pair setItem) { setItems.push_back(std::move(setItem)); }
    inline const std::vector<parsed_expr_pair>& getSetItemsRef() const { return setItems; }
    void addNamespaceMove(NamespaceMove move) { namespaceMoves.push_back(std::move(move)); }
    const std::vector<NamespaceMove>& getNamespaceMoves() const { return namespaceMoves; }

private:
    std::vector<parsed_expr_pair> setItems;
    std::vector<NamespaceMove> namespaceMoves;
};

} // namespace parser
} // namespace riag
