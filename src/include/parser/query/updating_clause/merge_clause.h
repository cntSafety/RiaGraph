#pragma once

#include "parser/query/graph_pattern/pattern_element.h"
#include "updating_clause.h"

namespace riag {
namespace parser {

class MergeClause final : public UpdatingClause {
public:
    explicit MergeClause(std::vector<PatternElement> patternElements)
        : UpdatingClause{common::ClauseType::MERGE}, patternElements{std::move(patternElements)} {}

    inline const std::vector<PatternElement>& getPatternElementsRef() const {
        return patternElements;
    }
    inline void addOnMatchSetItems(parsed_expr_pair setItem) {
        onMatchSetItems.push_back(std::move(setItem));
    }
    inline bool hasOnMatchSetItems() const { return !onMatchSetItems.empty(); }
    inline const std::vector<parsed_expr_pair>& getOnMatchSetItemsRef() const {
        return onMatchSetItems;
    }

    inline void addOnCreateSetItems(parsed_expr_pair setItem) {
        onCreateSetItems.push_back(std::move(setItem));
    }
    inline bool hasOnCreateSetItems() const { return !onCreateSetItems.empty(); }
    inline const std::vector<parsed_expr_pair>& getOnCreateSetItemsRef() const {
        return onCreateSetItems;
    }

    void addOnMatchNamespaceMove(NamespaceMove move) {
        onMatchNamespaceMoves.push_back(std::move(move));
    }
    void addOnCreateNamespaceMove(NamespaceMove move) {
        onCreateNamespaceMoves.push_back(std::move(move));
    }
    const std::vector<NamespaceMove>& getOnMatchNamespaceMoves() const {
        return onMatchNamespaceMoves;
    }
    const std::vector<NamespaceMove>& getOnCreateNamespaceMoves() const {
        return onCreateNamespaceMoves;
    }

private:
    std::vector<PatternElement> patternElements;
    std::vector<parsed_expr_pair> onMatchSetItems;
    std::vector<parsed_expr_pair> onCreateSetItems;
    std::vector<NamespaceMove> onMatchNamespaceMoves;
    std::vector<NamespaceMove> onCreateNamespaceMoves;
};

} // namespace parser
} // namespace riag
