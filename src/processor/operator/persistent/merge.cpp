#include "processor/operator/persistent/merge.h"

#include <algorithm>
#include <functional>

#include "binder/expression/expression_util.h"
#include "main/client_context.h"

using namespace riag::common;

namespace riag {
namespace processor {

std::string MergePrintInfo::toString() const {
    std::string result = "Pattern: ";
    result += binder::ExpressionUtil::toString(pattern);
    if (!onMatch.empty()) {
        result += ", ON MATCH SET: " + binder::ExpressionUtil::toString(onMatch);
    }
    if (!onCreate.empty()) {
        result += ", ON CREATE SET: " + binder::ExpressionUtil::toString(onCreate);
    }
    return result;
}

void Merge::initLocalStateInternal(ResultSet* resultSet_, ExecutionContext* context) {
    for (auto& executor : nodeInsertExecutors) {
        executor.init(resultSet, context);
    }
    for (auto& executor : relInsertExecutors) {
        executor.init(resultSet, context);
    }
    for (auto& executor : onCreateNodeSetExecutors) {
        executor->init(resultSet, context);
    }
    for (auto& executor : onCreateRelSetExecutors) {
        executor->init(resultSet, context);
    }
    for (auto& executor : onMatchNodeSetExecutors) {
        executor->init(resultSet, context);
    }
    for (auto& executor : onMatchRelSetExecutors) {
        executor->init(resultSet, context);
    }
    for (auto& evaluator : info.keyEvaluators) {
        evaluator->init(*resultSet_, context->clientContext);
    }
    for (auto& evaluator : info.patternValueEvaluators) {
        evaluator->init(*resultSet_, context->clientContext);
    }
    localState.init(*resultSet, context->clientContext, info);
    onMatchNamespaceExecutor.reset();
    onCreateNamespaceExecutor.reset();
    if (!info.onMatchNamespaceMoves.empty()) {
        onMatchNamespaceExecutor =
            std::make_unique<NamespaceMutationExecutor>(info.onMatchNamespaceMoves);
        onMatchNamespaceExecutor->init(resultSet_);
    }
    if (!info.onCreateNamespaceMoves.empty()) {
        onCreateNamespaceExecutor =
            std::make_unique<NamespaceMutationExecutor>(info.onCreateNamespaceMoves);
        onCreateNamespaceExecutor->init(resultSet_);
    }
}

void MergeLocalState::init(ResultSet& resultSet, main::ClientContext* context, MergeInfo& info) {
    std::vector<common::LogicalType> types;
    // Clear for pooled-plan reuse (fresh states start empty).
    keyVectors.clear();
    createdPatterns.clear();
    createdPatternsByKey.clear();
    pendingMatches.clear();
    for (auto& evaluator : info.keyEvaluators) {
        auto keyVector = evaluator->resultVector.get();
        types.push_back(keyVector->dataType.copy());
        keyVectors.push_back(keyVector);
    }
    // TODO: remove types
    hashTable = std::make_unique<PatternCreationInfoTable>(*storage::MemoryManager::Get(*context),
        std::move(types), std::move(info.tableSchema));
    existenceVector = resultSet.getValueVector(info.existenceMark).get();
}

bool MergeLocalState::patternExists() const {
    DASSERT(existenceVector->state->getSelVector().getSelSize() == 1);
    auto pos = existenceVector->state->getSelVector()[0];
    return existenceVector->getValue<bool>(pos);
}

void Merge::executeOnMatch(ExecutionContext* context) {
    for (auto i = 0u; i < onMatchNodeSetExecutors.size(); ++i) {
        for (const auto j : info.onMatchNodeColumnsToRefresh[i]) {
            onMatchNodeSetExecutors[j]->refreshColumn(context);
        }
        onMatchNodeSetExecutors[i]->set(context);
    }
    for (auto i = 0u; i < onMatchRelSetExecutors.size(); ++i) {
        for (const auto j : info.onMatchRelColumnsToRefresh[i]) {
            onMatchRelSetExecutors[j]->refreshColumn(context);
        }
        onMatchRelSetExecutors[i]->set(context);
    }
    if (onMatchNamespaceExecutor) {
        onMatchNamespaceExecutor->execute(context);
    }
}

// The pattern was not found by the existence check, but an earlier row of this pipeline created
// it, so this row matches it. Its ON CREATE SET, earlier ON MATCH SET and any later write may
// have changed it since, so its projected properties are read back from storage rather than
// re-evaluated from the insert's column data: ON MATCH SET right-hand sides and the rows returned
// downstream must see what is stored. Only the ON MATCH SET executors that write an inserted
// pattern are pointed at its ID; the others write a variable bound by this row.
template<typename GetID>
void Merge::executeOnCreatedPattern(const GetID& getPatternID, ExecutionContext* context) {
    for (auto i = 0u; i < nodeInsertExecutors.size(); ++i) {
        nodeInsertExecutors[i].skipInsert(getPatternID(i), context->clientContext);
    }
    for (auto i = 0u; i < relInsertExecutors.size(); ++i) {
        relInsertExecutors[i].skipInsert(getPatternID(i + nodeInsertExecutors.size()),
            context->clientContext);
    }
    for (const auto& [insertID, setters] : info.executorInfo) {
        const auto id = getPatternID(insertID);
        for (auto setter : setters) {
            if (setter < onMatchNodeSetExecutors.size()) {
                onMatchNodeSetExecutors[setter]->setNodeID(id);
            } else {
                onMatchRelSetExecutors[setter - onMatchNodeSetExecutors.size()]->setRelID(id);
            }
        }
    }
    executeOnMatch(context);
}

template<typename SetID>
void Merge::executeOnNewPattern(const SetID& setPatternID, ExecutionContext* context) {
    // do insert and on create
    for (auto i = 0u; i < nodeInsertExecutors.size(); i++) {
        auto& executor = nodeInsertExecutors[i];
        executor.setNodeIDVectorToNonNull();
        auto nodeID = executor.insert(context->clientContext);
        setPatternID(i, nodeID);
    }
    for (auto i = 0u; i < relInsertExecutors.size(); i++) {
        auto& executor = relInsertExecutors[i];
        auto relID = executor.insert(context->clientContext);
        setPatternID(i + nodeInsertExecutors.size(), relID);
    }
    for (auto& executor : onCreateNodeSetExecutors) {
        executor->set(context);
    }
    for (auto& executor : onCreateRelSetExecutors) {
        executor->set(context);
    }
    if (onCreateNamespaceExecutor) {
        onCreateNamespaceExecutor->execute(context);
    }
}

bool Merge::executeNoMatch(ExecutionContext* context) {
    for (auto& evaluator : info.keyEvaluators) {
        evaluator->evaluate();
    }
    auto patternCreationInfo = localState.getPatternCreationInfo();
    if (patternCreationInfo.hasCreated && (onMatchNamespaceExecutor || onCreateNamespaceExecutor)) {
        auto* transaction = transaction::Transaction::Get(*context->clientContext);
        for (size_t i = 0; i < nodeInsertExecutors.size(); ++i) {
            patternCreationInfo.hasCreated &= nodeInsertExecutors[i].namespaceMatches(
                patternCreationInfo.getPatternID(i), transaction);
        }
        for (size_t i = 0; i < relInsertExecutors.size(); ++i) {
            patternCreationInfo.hasCreated &= relInsertExecutors[i].namespaceMatches(
                patternCreationInfo.getPatternID(i + nodeInsertExecutors.size()), transaction);
        }
    }
    if (patternCreationInfo.hasCreated) {
        if (info.suppressDuplicateCreatedOutput) {
            return false;
        }
        executeOnCreatedPattern(
            [&](executor_id_t i) { return patternCreationInfo.getPatternID(i); }, context);
    } else {
        executeOnNewPattern(
            [&](executor_id_t i, internalID_t id) { patternCreationInfo.updateID(i, id); },
            context);
    }
    return true;
}

static Value evaluateKey(evaluator::ExpressionEvaluator& evaluator) {
    evaluator.evaluate();
    const auto& vector = *evaluator.resultVector;
    DASSERT(vector.state->getSelVector().getSelSize() == 1);
    return *vector.getAsValue(vector.state->getSelVector()[0]);
}

static bool hasNull(const std::vector<Value>& key) {
    return std::ranges::any_of(key, [](const Value& value) { return value.isNull(); });
}

// Created patterns are bucketed by this hash and told apart with Value::operator==, so it only
// has to agree for equal keys: a floating-point zero hashes the same with either sign.
static hash_t hashPatternKey(const std::vector<Value>& key) {
    std::string text;
    for (const auto& value : key) {
        const auto physicalType = value.getDataType().getPhysicalType();
        if ((physicalType == PhysicalTypeID::DOUBLE && value.getValue<double>() == 0) ||
            (physicalType == PhysicalTypeID::FLOAT && value.getValue<float>() == 0)) {
            text += "0";
        } else {
            text += value.toString();
        }
        text.push_back('\0');
    }
    return std::hash<std::string>{}(text);
}

// A key with a NULL matches no row: a pattern property compared with NULL is never true.
void Merge::addCreatedPatternKey(idx_t idx) {
    const auto& key = localState.createdPatterns[idx].key;
    if (!hasNull(key)) {
        localState.createdPatternsByKey.emplace(hashPatternKey(key), idx);
    }
}

void Merge::removeCreatedPatternKey(idx_t idx) {
    const auto& key = localState.createdPatterns[idx].key;
    if (hasNull(key)) {
        return;
    }
    auto [begin, end] = localState.createdPatternsByKey.equal_range(hashPatternKey(key));
    for (auto it = begin; it != end; ++it) {
        if (it->second == idx) {
            localState.createdPatternsByKey.erase(it);
            return;
        }
    }
}

// The pattern's fixed keys, then what is stored now in its pattern columns. A rel insert executor
// finds its rel between its current end nodes, which hold the pattern's right after it was
// created or output.
std::vector<Value> Merge::readPatternKey(const CreatedPattern& pattern,
    ExecutionContext* context) const {
    std::vector<Value> key(pattern.key.begin(), pattern.key.begin() + info.fixedKeys.size());
    for (auto i = 0u; i < nodeInsertExecutors.size(); ++i) {
        for (auto& value : nodeInsertExecutors[i].readColumns(pattern.ids[i],
                 info.patternColumns[i], context->clientContext)) {
            key.push_back(std::move(value));
        }
    }
    for (auto i = 0u; i < relInsertExecutors.size(); ++i) {
        const auto executorID = i + nodeInsertExecutors.size();
        for (auto& value : relInsertExecutors[i].readColumns(pattern.ids[executorID],
                 info.patternColumns[executorID], context->clientContext)) {
            key.push_back(std::move(value));
        }
    }
    return key;
}

// MERGE is defined row by row: a row matches the patterns earlier rows created as they are stored
// now, after their ON CREATE SET and ON MATCH SET. When those can change a pattern property, a
// created pattern can stop matching the key it was created for (MERGE (a:A {v: 5}) ON CREATE SET
// a.v = k creates a node for every row), start matching another row's key, and several created
// patterns can end up with one key. So the created patterns are kept with their stored values,
// and a row is merged into every one that matches it, or creates a new one.
void Merge::mergeCreatedPatterns(ExecutionContext* context) {
    std::vector<Value> key;
    for (const auto i : info.fixedKeys) {
        key.push_back(evaluateKey(*info.keyEvaluators[i]));
    }
    for (const auto& evaluator : info.patternValueEvaluators) {
        key.push_back(evaluateKey(*evaluator));
    }
    auto& pending = localState.pendingMatches;
    pending.clear();
    if (!hasNull(key)) {
        auto* transaction = transaction::Transaction::Get(*context->clientContext);
        auto [begin, end] = localState.createdPatternsByKey.equal_range(hashPatternKey(key));
        for (auto it = begin; it != end; ++it) {
            const auto& pattern = localState.createdPatterns[it->second];
            if (pattern.key != key) {
                continue;
            }
            auto inNamespace = true;
            if (onMatchNamespaceExecutor || onCreateNamespaceExecutor) {
                for (auto i = 0u; i < nodeInsertExecutors.size(); ++i) {
                    inNamespace &=
                        nodeInsertExecutors[i].namespaceMatches(pattern.ids[i], transaction);
                }
                for (auto i = 0u; i < relInsertExecutors.size(); ++i) {
                    inNamespace &= relInsertExecutors[i].namespaceMatches(
                        pattern.ids[i + nodeInsertExecutors.size()], transaction);
                }
            }
            if (inNamespace) {
                pending.push_back(it->second);
            }
        }
    }
    if (!pending.empty()) {
        // Output in creation order.
        std::ranges::sort(pending, std::greater{});
        outputPendingMatch(context);
        return;
    }
    const auto idx = static_cast<idx_t>(localState.createdPatterns.size());
    auto& pattern = localState.createdPatterns.emplace_back();
    pattern.ids.resize(nodeInsertExecutors.size() + relInsertExecutors.size());
    executeOnNewPattern([&](executor_id_t i, internalID_t id) { pattern.ids[i] = id; }, context);
    pattern.key = std::move(key);
    if (info.onCreateChangesPattern) {
        pattern.key = readPatternKey(pattern, context);
    }
    addCreatedPatternKey(idx);
}

bool Merge::outputPendingMatch(ExecutionContext* context) {
    auto& pending = localState.pendingMatches;
    if (pending.empty()) {
        return false;
    }
    const auto idx = pending.back();
    pending.pop_back();
    auto& pattern = localState.createdPatterns[idx];
    executeOnCreatedPattern([&](executor_id_t i) { return pattern.ids[i]; }, context);
    if (info.onMatchChangesPattern) {
        removeCreatedPatternKey(idx);
        pattern.key = readPatternKey(pattern, context);
        addCreatedPatternKey(idx);
    }
    return true;
}

bool Merge::getNextTuplesInternal(ExecutionContext* context) {
    // The rest of the created patterns the last input row matched.
    if (outputPendingMatch(context)) {
        return true;
    }
    while (children[0]->getNextTuple(context)) {
        if (localState.patternExists()) {
            executeOnMatch(context);
            return true;
        }
        if (info.onCreateChangesPattern || info.onMatchChangesPattern) {
            mergeCreatedPatterns(context);
            return true;
        }
        if (executeNoMatch(context)) {
            return true;
        }
    }
    return false;
}

} // namespace processor
} // namespace riag
