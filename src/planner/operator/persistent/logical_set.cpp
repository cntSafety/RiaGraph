#include "planner/operator/persistent/logical_set.h"

#include "binder/expression/expression_util.h"
#include "binder/expression/rel_expression.h"
#include "binder/expression_visitor.h"
#include "planner/operator/factorization/flatten_resolver.h"

using namespace riag::binder;
using namespace riag::common;

namespace riag {
namespace planner {

void LogicalSetProperty::computeFactorizedSchema() {
    copyChildSchema(0);
}

void LogicalSetProperty::computeFlatSchema() {
    copyChildSchema(0);
}

f_group_pos_set LogicalSetProperty::getGroupsPosToFlatten(uint32_t idx) const {
    f_group_pos_set result;
    auto childSchema = children[0]->getSchema();
    auto& info = infos[idx];
    switch (getTableType()) {
    case TableType::NODE: {
        auto node = info.pattern->constPtrCast<NodeExpression>();
        result.insert(childSchema->getGroupPos(*node->getInternalID()));
    } break;
    case TableType::REL: {
        auto rel = info.pattern->constPtrCast<RelExpression>();
        result.insert(childSchema->getGroupPos(*rel->getSrcNode()->getInternalID()));
        result.insert(childSchema->getGroupPos(*rel->getDstNode()->getInternalID()));
    } break;
    default:
        UNREACHABLE_CODE;
    }
    if (!getItemsReadBy(infos, idx).empty()) {
        // The right-hand side reads a property this operator writes, so its value depends on
        // how many rows wrote the same node or rel before. Every input row must then be a write
        // of its own: flatten every group, not only the ones the item depends on. Otherwise a
        // group of several rows left unflat (`UNWIND [1,2,3] AS k MATCH (a) SET a.v = a.v + 1
        // RETURN k`) is written once for all of them.
        for (auto groupPos : childSchema->getGroupsPosInScope()) {
            result.insert(groupPos);
        }
    }
    auto analyzer = GroupDependencyAnalyzer(false, *childSchema);
    analyzer.visit(info.columnData);
    for (auto& groupPos : analyzer.getDependentGroups()) {
        result.insert(groupPos);
    }
    return FlattenAll::getGroupsPosToFlatten(result, *childSchema);
}

std::vector<idx_t> LogicalSetProperty::getItemsReadBy(
    const std::vector<BoundSetPropertyInfo>& infos, uint32_t idx) {
    auto collector = PropertyExprCollector();
    collector.visit(infos[idx].columnData);
    std::unordered_set<std::string> readProperties;
    for (auto& expr : collector.getPropertyExprs()) {
        readProperties.insert(expr->getUniqueName());
    }
    std::vector<idx_t> result;
    for (auto j = 0u; j < infos.size(); ++j) {
        // Erasing keeps one item per property when several items write the same one.
        if (readProperties.erase(infos[j].column->getUniqueName()) > 0) {
            result.push_back(j);
        }
    }
    return result;
}

std::string LogicalSetProperty::getExpressionsForPrinting() const {
    std::string result =
        ExpressionUtil::toString(std::make_pair(infos[0].column, infos[0].columnData));
    for (auto i = 1u; i < infos.size(); ++i) {
        result += ExpressionUtil::toString(std::make_pair(infos[i].column, infos[i].columnData));
    }
    return result;
}

common::TableType LogicalSetProperty::getTableType() const {
    DASSERT(!infos.empty());
    return infos[0].tableType;
}

} // namespace planner
} // namespace riag
