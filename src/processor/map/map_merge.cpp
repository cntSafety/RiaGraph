#include <unordered_set>

#include "binder/expression/node_rel_expression.h"
#include "binder/expression/property_expression.h"
#include "planner/operator/persistent/logical_merge.h"
#include "processor/operator/persistent/merge.h"
#include "processor/operator/persistent/namespace_mutation.h"
#include "processor/plan_mapper.h"
#include <processor/expression_mapper.h>

using namespace riag::planner;

namespace riag {
namespace processor {

static FactorizedTableSchema getFactorizedTableSchema(const binder::expression_vector& keys,
    uint64_t numNodeInsertExecutors, uint64_t numRelInsertExecutors) {
    auto tableSchema = FactorizedTableSchema();
    auto isUnFlat = false;
    auto groupID = 0u;
    for (auto& key : keys) {
        auto size = common::LogicalTypeUtils::getRowLayoutSize(key->dataType);
        tableSchema.appendColumn(ColumnSchema(isUnFlat, groupID, size));
    }
    auto numNodeIDFields = numNodeInsertExecutors + numRelInsertExecutors;
    for (auto i = 0u; i < numNodeIDFields; i++) {
        tableSchema.appendColumn(ColumnSchema(isUnFlat, groupID, sizeof(common::nodeID_t)));
    }
    tableSchema.appendColumn(ColumnSchema(isUnFlat, groupID, sizeof(common::hash_t)));
    return tableSchema;
}

// The columns of an insert the MERGE pattern gives a value, e.g. v of (a:A {v: 5}).
static std::vector<common::idx_t> getPatternColumns(const LogicalInsertInfo& info) {
    const auto& pattern = info.pattern->constCast<binder::NodeOrRelExpression>();
    std::vector<common::idx_t> columns;
    for (auto i = 0u; i < info.columnExprs.size(); ++i) {
        const auto& column = *info.columnExprs[i];
        if (column.expressionType == common::ExpressionType::PROPERTY &&
            pattern.hasPropertyDataExpr(
                column.constCast<binder::PropertyExpression>().getPropertyName())) {
            columns.push_back(i);
        }
    }
    return columns;
}

// Whether a SET item writes a property the MERGE pattern gives a value to one of the inserted
// nodes and rels.
static bool changesPattern(const std::vector<binder::BoundSetPropertyInfo>& setInfos,
    const std::vector<const LogicalInsertInfo*>& insertInfos) {
    for (const auto& setInfo : setInfos) {
        if (setInfo.column->expressionType != common::ExpressionType::PROPERTY) {
            continue;
        }
        const auto propertyName =
            setInfo.column->constCast<binder::PropertyExpression>().getPropertyName();
        for (const auto* insertInfo : insertInfos) {
            if (*setInfo.pattern == *insertInfo->pattern &&
                insertInfo->pattern->constCast<binder::NodeOrRelExpression>().hasPropertyDataExpr(
                    propertyName)) {
                return true;
            }
        }
    }
    return false;
}

// See MergeInfo::onCreateChangesPattern.
static void mapPatternChanges(const LogicalMerge& logicalMerge, const planner::Schema& inSchema,
    MergeInfo& mergeInfo) {
    std::vector<const LogicalInsertInfo*> insertInfos;
    for (const auto& info : logicalMerge.getInsertNodeInfos()) {
        insertInfos.push_back(&info);
    }
    for (const auto& info : logicalMerge.getInsertRelInfos()) {
        insertInfos.push_back(&info);
    }
    mergeInfo.onCreateChangesPattern =
        changesPattern(logicalMerge.getOnCreateSetNodeInfos(), insertInfos) ||
        changesPattern(logicalMerge.getOnCreateSetRelInfos(), insertInfos);
    mergeInfo.onMatchChangesPattern =
        changesPattern(logicalMerge.getOnMatchSetNodeInfos(), insertInfos) ||
        changesPattern(logicalMerge.getOnMatchSetRelInfos(), insertInfos);
    if (!mergeInfo.onCreateChangesPattern && !mergeInfo.onMatchChangesPattern) {
        return;
    }
    auto expressionMapper = ExpressionMapper(&inSchema);
    std::unordered_set<std::string> patternValues;
    for (const auto* info : insertInfos) {
        auto columns = getPatternColumns(*info);
        for (const auto column : columns) {
            mergeInfo.patternValueEvaluators.push_back(
                expressionMapper.getEvaluator(info->columnDataExprs[column]));
        }
        mergeInfo.patternColumns.push_back(std::move(columns));
        for (const auto& [_, value] :
            info->pattern->constCast<binder::NodeOrRelExpression>().getPropertyDataExprRef()) {
            patternValues.insert(value->getUniqueName());
        }
    }
    const auto& keys = logicalMerge.getKeys();
    for (auto i = 0u; i < keys.size(); ++i) {
        if (!patternValues.contains(keys[i]->getUniqueName())) {
            mergeInfo.fixedKeys.push_back(i);
        }
    }
}

std::unique_ptr<PhysicalOperator> PlanMapper::mapMerge(const LogicalOperator* logicalOperator) {
    auto& logicalMerge = logicalOperator->constCast<LogicalMerge>();
    auto outSchema = logicalMerge.getSchema();
    auto inSchema = logicalMerge.getChild(0)->getSchema();
    auto prevOperator = mapOperator(logicalOperator->getChild(0).get());
    auto existenceMarkPos = getDataPos(*logicalMerge.getExistenceMark(), *inSchema);
    std::vector<NodeInsertExecutor> nodeInsertExecutors;
    for (auto& info : logicalMerge.getInsertNodeInfos()) {
        nodeInsertExecutors.push_back(getNodeInsertExecutor(&info, *inSchema, *outSchema));
    }
    std::vector<RelInsertExecutor> relInsertExecutors;
    for (auto& info : logicalMerge.getInsertRelInfos()) {
        relInsertExecutors.push_back(getRelInsertExecutor(&info, *inSchema, *outSchema));
    }
    std::vector<std::unique_ptr<NodeSetExecutor>> onCreateNodeSetExecutors;
    for (auto& info : logicalMerge.getOnCreateSetNodeInfos()) {
        onCreateNodeSetExecutors.push_back(getNodeSetExecutor(info, *inSchema));
    }
    std::vector<std::unique_ptr<RelSetExecutor>> onCreateRelSetExecutors;
    for (auto& info : logicalMerge.getOnCreateSetRelInfos()) {
        onCreateRelSetExecutors.push_back(getRelSetExecutor(info, *inSchema));
    }
    std::vector<std::unique_ptr<NodeSetExecutor>> onMatchNodeSetExecutors;
    common::executor_info executorInfo;
    for (auto i = 0u; i < logicalMerge.getOnMatchSetNodeInfos().size(); i++) {
        auto& info = logicalMerge.getOnMatchSetNodeInfos()[i];
        for (auto j = 0u; j < logicalMerge.getInsertNodeInfos().size(); j++) {
            if (*info.pattern == *logicalMerge.getInsertNodeInfos()[j].pattern) {
                executorInfo[j].push_back(i);
            }
        }
        onMatchNodeSetExecutors.push_back(getNodeSetExecutor(info, *inSchema));
    }
    std::vector<std::unique_ptr<RelSetExecutor>> onMatchRelSetExecutors;
    for (auto i = 0u; i < logicalMerge.getOnMatchSetRelInfos().size(); i++) {
        auto& info = logicalMerge.getOnMatchSetRelInfos()[i];
        for (auto j = 0u; j < logicalMerge.getInsertRelInfos().size(); j++) {
            if (*info.pattern == *logicalMerge.getInsertRelInfos()[j].pattern) {
                executorInfo[j + logicalMerge.getInsertNodeInfos().size()].push_back(
                    i + logicalMerge.getOnMatchSetNodeInfos().size());
            }
        }
        onMatchRelSetExecutors.push_back(getRelSetExecutor(info, *inSchema));
    }
    binder::expression_vector expressions;
    for (auto& info : logicalMerge.getInsertNodeInfos()) {
        for (auto& expr : info.columnExprs) {
            expressions.push_back(expr);
        }
    }
    for (auto& info : logicalMerge.getInsertRelInfos()) {
        for (auto& expr : info.columnExprs) {
            expressions.push_back(expr);
        }
    }
    std::vector<binder::expression_pair> onCreateOperation;
    for (auto& info : logicalMerge.getOnCreateSetRelInfos()) {
        onCreateOperation.emplace_back(info.column, info.columnData);
    }
    for (auto& info : logicalMerge.getOnCreateSetNodeInfos()) {
        onCreateOperation.emplace_back(info.column, info.columnData);
    }
    std::vector<binder::expression_pair> onMatchOperation;
    for (auto& info : logicalMerge.getOnMatchSetRelInfos()) {
        onMatchOperation.emplace_back(info.column, info.columnData);
    }
    for (auto& info : logicalMerge.getOnMatchSetNodeInfos()) {
        onMatchOperation.emplace_back(info.column, info.columnData);
    }
    auto printInfo =
        std::make_unique<MergePrintInfo>(expressions, onCreateOperation, onMatchOperation);
    std::vector<std::unique_ptr<evaluator::ExpressionEvaluator>> keyEvaluators;
    auto expressionMapper = ExpressionMapper(inSchema);
    for (auto& key : logicalMerge.getKeys()) {
        keyEvaluators.push_back(expressionMapper.getEvaluator(key));
    }

    auto suppressDuplicateCreatedOutput = logicalMerge.suppressesDuplicateCreatedOutput();
    // One ID field per inserted node and rel: a later row with the same key reads the pattern's
    // stored state back through these IDs (see Merge::executeOnCreatedPattern).
    MergeInfo mergeInfo{std::move(keyEvaluators),
        getFactorizedTableSchema(logicalMerge.getKeys(), logicalMerge.getInsertNodeInfos().size(),
            logicalMerge.getInsertRelInfos().size()),
        std::move(executorInfo), suppressDuplicateCreatedOutput, existenceMarkPos};
    // The existence check scanned the matched pattern before any row of this vector was merged,
    // so an ON MATCH SET reading a property it writes reads it again (as SET does).
    mergeInfo.onMatchNodeColumnsToRefresh =
        getColumnsToRefresh(logicalMerge.getOnMatchSetNodeInfos(), *inSchema);
    mergeInfo.onMatchRelColumnsToRefresh =
        getColumnsToRefresh(logicalMerge.getOnMatchSetRelInfos(), *inSchema);
    mapPatternChanges(logicalMerge, *inSchema, mergeInfo);
    const auto mapMoves = [&](const auto& moves, auto& destination) {
        for (const auto& move : moves) {
            destination.push_back({getDataPos(*move.entity->getInternalID(), *outSchema),
                move.target, move.entity->getEntry(0)->getOwningCatalog(),
                move.entity->getDataType().getLogicalTypeID() == common::LogicalTypeID::REL});
        }
    };
    mapMoves(logicalMerge.onMatchNamespaceMoves, mergeInfo.onMatchNamespaceMoves);
    mapMoves(logicalMerge.onCreateNamespaceMoves, mergeInfo.onCreateNamespaceMoves);
    std::unique_ptr<PhysicalOperator> result =
        std::make_unique<Merge>(std::move(nodeInsertExecutors), std::move(relInsertExecutors),
            std::move(onCreateNodeSetExecutors), std::move(onCreateRelSetExecutors),
            std::move(onMatchNodeSetExecutors), std::move(onMatchRelSetExecutors),
            std::move(mergeInfo), std::move(prevOperator), getOperatorID(), std::move(printInfo));
    return result;
}

} // namespace processor
} // namespace riag
