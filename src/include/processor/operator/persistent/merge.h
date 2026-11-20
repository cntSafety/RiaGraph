#pragma once

#include <unordered_map>

#include "insert_executor.h"
#include "namespace_mutation.h"
#include "processor/operator/physical_operator.h"
#include "processor/result/pattern_creation_info_table.h"
#include "set_executor.h"

namespace riag {
namespace processor {

struct MergeInfo {
    std::vector<NamespaceMutationInfo> onMatchNamespaceMoves;
    std::vector<NamespaceMutationInfo> onCreateNamespaceMoves;
    std::vector<std::unique_ptr<evaluator::ExpressionEvaluator>> keyEvaluators;
    FactorizedTableSchema tableSchema;
    common::executor_info executorInfo;
    bool suppressDuplicateCreatedOutput;
    DataPos existenceMark;
    // Per ON MATCH SET executor: the executors whose target property it reads, read again from
    // storage before it runs (see NodeSetExecutor::refreshColumn).
    std::vector<std::vector<common::idx_t>> onMatchNodeColumnsToRefresh;
    std::vector<std::vector<common::idx_t>> onMatchRelColumnsToRefresh;
    // Set when an ON CREATE SET (ON MATCH SET) can change a property the MERGE pattern gives a
    // value, e.g. MERGE (a:A {v: 5}) ON CREATE SET a.v = k. A pattern an earlier row created then
    // no longer matches the key it was created for, or matches another row's key, so the key
    // hash table cannot tell which created patterns a row matches. Merge::mergeCreatedPatterns
    // tracks them by their stored values instead.
    bool onCreateChangesPattern = false;
    bool onMatchChangesPattern = false;
    // Only used when one of the above is set. Per insert executor (nodes, then rels), the columns
    // the pattern gives a value; patternValueEvaluators evaluate those values in the same order.
    // fixedKeys are the keys a SET cannot change (bound node IDs), as indexes into keyEvaluators.
    std::vector<std::vector<common::idx_t>> patternColumns;
    std::vector<std::unique_ptr<evaluator::ExpressionEvaluator>> patternValueEvaluators;
    std::vector<common::idx_t> fixedKeys;

    MergeInfo(std::vector<std::unique_ptr<evaluator::ExpressionEvaluator>> keyEvaluators,
        FactorizedTableSchema tableSchema, common::executor_info executorInfo,
        bool suppressDuplicateCreatedOutput, DataPos existenceMark)
        : keyEvaluators{std::move(keyEvaluators)}, tableSchema{std::move(tableSchema)},
          executorInfo{std::move(executorInfo)},
          suppressDuplicateCreatedOutput{suppressDuplicateCreatedOutput},
          existenceMark{existenceMark} {}
    EXPLICIT_COPY_DEFAULT_MOVE(MergeInfo);

private:
    MergeInfo(const MergeInfo& other)
        : onMatchNamespaceMoves{other.onMatchNamespaceMoves},
          onCreateNamespaceMoves{other.onCreateNamespaceMoves},
          keyEvaluators{copyVector(other.keyEvaluators)}, tableSchema{other.tableSchema.copy()},
          executorInfo{other.executorInfo},
          suppressDuplicateCreatedOutput{other.suppressDuplicateCreatedOutput},
          existenceMark{other.existenceMark},
          onMatchNodeColumnsToRefresh{other.onMatchNodeColumnsToRefresh},
          onMatchRelColumnsToRefresh{other.onMatchRelColumnsToRefresh},
          onCreateChangesPattern{other.onCreateChangesPattern},
          onMatchChangesPattern{other.onMatchChangesPattern}, patternColumns{other.patternColumns},
          patternValueEvaluators{copyVector(other.patternValueEvaluators)},
          fixedKeys{other.fixedKeys} {}
};

struct MergePrintInfo final : OPPrintInfo {
    binder::expression_vector pattern;
    std::vector<binder::expression_pair> onCreate;
    std::vector<binder::expression_pair> onMatch;

    MergePrintInfo(binder::expression_vector pattern, std::vector<binder::expression_pair> onCreate,
        std::vector<binder::expression_pair> onMatch)
        : pattern(std::move(pattern)), onCreate(std::move(onCreate)), onMatch(std::move(onMatch)) {}

    std::string toString() const override;

    std::unique_ptr<OPPrintInfo> copy() const override {
        return std::unique_ptr<MergePrintInfo>(new MergePrintInfo(*this));
    }

private:
    MergePrintInfo(const MergePrintInfo& other)
        : OPPrintInfo(other), pattern(other.pattern), onCreate(other.onCreate),
          onMatch(other.onMatch) {}
};

// A pattern this MERGE created, when a SET can change the values its properties were created with
// (see MergeInfo::onCreateChangesPattern).
struct CreatedPattern {
    // One per insert executor.
    std::vector<common::internalID_t> ids;
    // The fixed keys it was created with, then the stored values of its pattern columns.
    std::vector<common::Value> key;
};

struct MergeLocalState {
    std::vector<common::ValueVector*> keyVectors;
    std::unique_ptr<PatternCreationInfoTable> hashTable;
    common::ValueVector* existenceVector = nullptr;
    // Only used when a SET can change a pattern property.
    std::vector<CreatedPattern> createdPatterns;
    std::unordered_multimap<common::hash_t, common::idx_t> createdPatternsByKey;
    // The created patterns the current input row matches and that are not output yet.
    std::vector<common::idx_t> pendingMatches;

    void init(ResultSet& resultSet, main::ClientContext* context, MergeInfo& info);

    bool patternExists() const;

    PatternCreationInfo getPatternCreationInfo() const {
        return hashTable->getPatternCreationInfo(keyVectors);
    }
};

class Merge final : public PhysicalOperator {
    static constexpr PhysicalOperatorType type_ = PhysicalOperatorType::MERGE;

public:
    Merge(std::vector<NodeInsertExecutor> nodeInsertExecutors,
        std::vector<RelInsertExecutor> relInsertExecutors,
        std::vector<std::unique_ptr<NodeSetExecutor>> onCreateNodeSetExecutors,
        std::vector<std::unique_ptr<RelSetExecutor>> onCreateRelSetExecutors,
        std::vector<std::unique_ptr<NodeSetExecutor>> onMatchNodeSetExecutors,
        std::vector<std::unique_ptr<RelSetExecutor>> onMatchRelSetExecutors, MergeInfo info,
        std::unique_ptr<PhysicalOperator> child, uint32_t id,
        std::unique_ptr<OPPrintInfo> printInfo)
        : PhysicalOperator{type_, std::move(child), id, std::move(printInfo)},
          nodeInsertExecutors{std::move(nodeInsertExecutors)},
          relInsertExecutors{std::move(relInsertExecutors)},
          onCreateNodeSetExecutors{std::move(onCreateNodeSetExecutors)},
          onCreateRelSetExecutors{std::move(onCreateRelSetExecutors)},
          onMatchNodeSetExecutors{std::move(onMatchNodeSetExecutors)},
          onMatchRelSetExecutors{std::move(onMatchRelSetExecutors)}, info{std::move(info)} {}

    bool isParallel() const override { return false; }

    void initLocalStateInternal(ResultSet* resultSet_, ExecutionContext* context) override;

    bool getNextTuplesInternal(ExecutionContext* context) override;

    std::unique_ptr<PhysicalOperator> copy() override {
        return std::make_unique<Merge>(copyVector(nodeInsertExecutors),
            copyVector(relInsertExecutors), copyVector(onCreateNodeSetExecutors),
            copyVector(onCreateRelSetExecutors), copyVector(onMatchNodeSetExecutors),
            copyVector(onMatchRelSetExecutors), info.copy(), children[0]->copy(), id,
            printInfo->copy());
    }

private:
    void executeOnMatch(ExecutionContext* context);

    template<typename GetID>
    void executeOnCreatedPattern(const GetID& getPatternID, ExecutionContext* context);

    template<typename SetID>
    void executeOnNewPattern(const SetID& setPatternID, ExecutionContext* context);

    bool executeNoMatch(ExecutionContext* context);

    // executeNoMatch for a MERGE whose SET can change a pattern property.
    void mergeCreatedPatterns(ExecutionContext* context);
    bool outputPendingMatch(ExecutionContext* context);
    std::vector<common::Value> readPatternKey(const CreatedPattern& pattern,
        ExecutionContext* context) const;
    void addCreatedPatternKey(common::idx_t idx);
    void removeCreatedPatternKey(common::idx_t idx);

private:
    std::vector<NodeInsertExecutor> nodeInsertExecutors;
    std::vector<RelInsertExecutor> relInsertExecutors;

    std::vector<std::unique_ptr<NodeSetExecutor>> onCreateNodeSetExecutors;
    std::vector<std::unique_ptr<RelSetExecutor>> onCreateRelSetExecutors;

    std::vector<std::unique_ptr<NodeSetExecutor>> onMatchNodeSetExecutors;
    std::vector<std::unique_ptr<RelSetExecutor>> onMatchRelSetExecutors;

    MergeInfo info;
    MergeLocalState localState;
    std::unique_ptr<NamespaceMutationExecutor> onMatchNamespaceExecutor;
    std::unique_ptr<NamespaceMutationExecutor> onCreateNamespaceExecutor;
};

} // namespace processor
} // namespace riag
