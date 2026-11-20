#include "processor/operator/persistent/delete_executor.h"

#include <memory>

#include "catalog/catalog.h"
#include "catalog/namespace_catalog.h"
#include "common/assert.h"
#include "common/exception/message.h"
#include "common/vector/value_vector.h"
#include "processor/execution_context.h"
#include "storage/table/rel_table.h"

using namespace riag::common;
using namespace riag::storage;
using namespace riag::transaction;

namespace riag {
namespace processor {

void NodeDeleteInfo::init(const ResultSet& resultSet) {
    nodeIDVector = resultSet.getValueVector(nodeIDPos).get();
}

void NodeTableDeleteInfo::init(const ResultSet& resultSet) {
    pkVector = resultSet.getValueVector(pkPos).get();
}

static void throwDeleteNodeWithConnectedEdgesError(const std::string& tableName,
    offset_t nodeOffset, RelDataDirection direction) {
    throw RuntimeException(ExceptionMessage::violateDeleteNodeWithConnectedEdgesConstraint(
        tableName, std::to_string(nodeOffset), RelDirectionUtils::relDirectionToString(direction)));
}

void NodeTableDeleteInfo::deleteFromRelTable(Transaction* transaction,
    ValueVector* nodeIDVector) const {
    for (auto& relTable : fwdRelTables) {
        relTable->throwIfNodeHasRels(transaction, RelDataDirection::FWD, nodeIDVector,
            throwDeleteNodeWithConnectedEdgesError);
    }
    for (auto& relTable : bwdRelTables) {
        relTable->throwIfNodeHasRels(transaction, RelDataDirection::BWD, nodeIDVector,
            throwDeleteNodeWithConnectedEdgesError);
    }
}

void NodeTableDeleteInfo::detachDeleteFromRelTable(Transaction* transaction,
    RelTableDeleteState* detachDeleteState) const {
    for (auto& relTable : fwdRelTables) {
        detachDeleteState->detachDeleteDirection = RelDataDirection::FWD;
        relTable->detachDelete(transaction, detachDeleteState);
    }
    for (auto& relTable : bwdRelTables) {
        detachDeleteState->detachDeleteDirection = RelDataDirection::BWD;
        relTable->detachDelete(transaction, detachDeleteState);
    }
}

void NodeDeleteExecutor::init(ResultSet* resultSet, ExecutionContext*) {
    info.init(*resultSet);
    if (info.deleteType == DeleteNodeType::DETACH_DELETE) {
        const auto tempSharedState = std::make_shared<DataChunkState>();
        dstNodeIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID());
        relIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID());
        dstNodeIDVector->setState(tempSharedState);
        relIDVector->setState(tempSharedState);
        detachDeleteState = std::make_unique<RelTableDeleteState>(*info.nodeIDVector,
            *dstNodeIDVector, *relIDVector);
        batchSrcNodeIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID());
        batchDstNodeIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID());
        batchRelIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID());
    }
}

void NodeDeleteExecutor::appendToBatch(internalID_t nodeID) {
    batchNodeIDs.push_back(nodeID);
}

void NodeDeleteExecutor::validateNamespaceScope(ExecutionContext* context,
    const NodeTableDeleteInfo& tableInfo) {
    if (info.deleteType != DeleteNodeType::DETACH_DELETE || info.namespaceScope.isUnrestricted()) {
        return;
    }
    const auto position = info.nodeIDVector->state->getSelVector()[0];
    if (info.nodeIDVector->isNull(position)) {
        return;
    }
    auto* transaction = Transaction::Get(*context->clientContext);
    auto* owner = tableInfo.table->getOwningCatalog();
    auto* namespaces = owner->getNamespaces();
    catalog::NamespaceReadState targetReader(namespaces, transaction,
        tableInfo.table->getTableID());
    if (!info.namespaceScope.contains(
            targetReader.get(info.nodeIDVector->readNodeOffset(position)))) {
        throw RuntimeException("DETACH DELETE target is outside its MATCH namespace scope.");
    }
    const auto check = [&](RelTable* table, RelDataDirection direction) {
        catalog::NamespaceReadState relReader(namespaces, transaction, table->getTableID());
        const auto neighborTable = direction == RelDataDirection::FWD ? table->getToNodeTableID() :
                                                                        table->getFromNodeTableID();
        catalog::NamespaceReadState nodeReader(namespaces, transaction, neighborTable);
        RelTableScanState scan(*MemoryManager::Get(*context->clientContext), info.nodeIDVector,
            {dstNodeIDVector.get(), relIDVector.get()}, dstNodeIDVector->state, true);
        scan.setToTable(transaction, table, {NBR_ID_COLUMN_ID, REL_ID_COLUMN_ID}, {}, direction);
        table->initScanState(transaction, scan);
        while (table->scan(transaction, scan)) {
            const auto& selected = dstNodeIDVector->state->getSelVector();
            for (sel_t row = 0; row < selected.getSelSize(); ++row) {
                const auto pos = selected[row];
                if (!info.namespaceScope.contains(
                        relReader.get(relIDVector->readNodeOffset(pos))) ||
                    !info.namespaceScope.contains(
                        nodeReader.get(dstNodeIDVector->readNodeOffset(pos)))) {
                    throw RuntimeException("DETACH DELETE would remove an incident relationship or "
                                           "endpoint outside its MATCH namespace scope.");
                }
            }
        }
    };
    for (auto* table : tableInfo.fwdRelTables) {
        check(table, RelDataDirection::FWD);
    }
    for (auto* table : tableInfo.bwdRelTables) {
        check(table, RelDataDirection::BWD);
    }
}

void NodeDeleteExecutor::flushBatch(const std::vector<internalID_t>& nodeIDs,
    ValueVector& srcNodeIDVector, const std::unordered_set<RelTable*>& fwdRelTables,
    const std::unordered_set<RelTable*>& bwdRelTables, Transaction* transaction) {
    if (nodeIDs.empty()) {
        return;
    }
    const auto srcState = std::make_shared<DataChunkState>();
    const auto relState = std::make_shared<DataChunkState>();
    srcNodeIDVector.setState(srcState);
    batchDstNodeIDVector->setState(relState);
    batchRelIDVector->setState(relState);
    srcState->getSelVectorUnsafe().setSelSize(nodeIDs.size());
    for (auto i = 0u; i < nodeIDs.size(); i++) {
        srcNodeIDVector.setValue(i, nodeIDs[i]);
    }
    for (auto& relTable : fwdRelTables) {
        relTable->detachDeleteBatch(transaction, srcNodeIDVector, *batchDstNodeIDVector,
            *batchRelIDVector, RelDataDirection::FWD);
    }
    for (auto& relTable : bwdRelTables) {
        relTable->detachDeleteBatch(transaction, srcNodeIDVector, *batchDstNodeIDVector,
            *batchRelIDVector, RelDataDirection::BWD);
    }
}

void SingleLabelNodeDeleteExecutor::init(ResultSet* resultSet, ExecutionContext* context) {
    NodeDeleteExecutor::init(resultSet, context);
    tableInfo.init(*resultSet);
}

void SingleLabelNodeDeleteExecutor::finalize(ExecutionContext* context) {
    // Batch vectors are only allocated for DETACH_DELETE. Dereferencing them for a
    // plain DELETE (or when nothing was batched) is a null dereference.
    if (info.deleteType != DeleteNodeType::DETACH_DELETE || batchNodeIDs.empty()) {
        return;
    }
    auto transaction = Transaction::Get(*context->clientContext);
    flushBatch(batchNodeIDs, *batchSrcNodeIDVector, tableInfo.fwdRelTables, tableInfo.bwdRelTables,
        transaction);
    batchNodeIDs.clear();
}

void SingleLabelNodeDeleteExecutor::delete_(ExecutionContext* context) {
    validateNamespaceScope(context, tableInfo);
    DASSERT(tableInfo.pkVector->state == info.nodeIDVector->state);
    auto deleteState =
        std::make_unique<NodeTableDeleteState>(*info.nodeIDVector, *tableInfo.pkVector);
    auto transaction = Transaction::Get(*context->clientContext);
    if (!tableInfo.table->delete_(transaction, *deleteState)) {
        return;
    }
    switch (info.deleteType) {
    case DeleteNodeType::DELETE: {
        tableInfo.deleteFromRelTable(transaction, info.nodeIDVector);
    } break;
    case DeleteNodeType::DETACH_DELETE: {
        const auto& selVector = info.nodeIDVector->state->getSelVector();
        const auto pos = selVector[0];
        if (!info.nodeIDVector->isNull(pos)) {
            appendToBatch(info.nodeIDVector->getValue<internalID_t>(pos));
        }
        if (batchNodeIDs.size() >= BATCH_SIZE) {
            flushBatch(batchNodeIDs, *batchSrcNodeIDVector, tableInfo.fwdRelTables,
                tableInfo.bwdRelTables, transaction);
            batchNodeIDs.clear();
        }
    } break;
    default:
        UNREACHABLE_CODE;
    }
}

void MultiLabelNodeDeleteExecutor::init(ResultSet* resultSet, ExecutionContext* context) {
    NodeDeleteExecutor::init(resultSet, context);
    for (auto& [_, tableInfo] : tableInfos) {
        tableInfo.init(*resultSet);
    }
}

void MultiLabelNodeDeleteExecutor::finalize(ExecutionContext* context) {
    // Batch vectors are only allocated for DETACH_DELETE. See above.
    if (info.deleteType != DeleteNodeType::DETACH_DELETE || batchNodeIDs.empty()) {
        return;
    }
    auto transaction = Transaction::Get(*context->clientContext);
    table_id_map_t<std::vector<internalID_t>> tableIDToNodeIDs;
    for (const auto nodeID : batchNodeIDs) {
        tableIDToNodeIDs[nodeID.tableID].push_back(nodeID);
    }
    for (auto& [tableID, nodeIDs] : tableIDToNodeIDs) {
        const auto& tableInfo = tableInfos.at(tableID);
        flushBatch(nodeIDs, *batchSrcNodeIDVector, tableInfo.fwdRelTables, tableInfo.bwdRelTables,
            transaction);
    }
    batchNodeIDs.clear();
}

void MultiLabelNodeDeleteExecutor::delete_(ExecutionContext* context) {
    auto& nodeIDSelVector = info.nodeIDVector->state->getSelVector();
    DASSERT(nodeIDSelVector.getSelSize() == 1);
    const auto pos = nodeIDSelVector[0];
    if (info.nodeIDVector->isNull(pos)) {
        return;
    }
    const auto nodeID = info.nodeIDVector->getValue<internalID_t>(pos);
    const auto& tableInfo = tableInfos.at(nodeID.tableID);
    validateNamespaceScope(context, tableInfo);
    auto deleteState =
        std::make_unique<NodeTableDeleteState>(*info.nodeIDVector, *tableInfo.pkVector);
    auto transaction = Transaction::Get(*context->clientContext);
    if (!tableInfo.table->delete_(transaction, *deleteState)) {
        return;
    }
    switch (info.deleteType) {
    case DeleteNodeType::DELETE: {
        tableInfo.deleteFromRelTable(transaction, info.nodeIDVector);
    } break;
    case DeleteNodeType::DETACH_DELETE: {
        appendToBatch(nodeID);
        if (batchNodeIDs.size() >= BATCH_SIZE) {
            finalize(context);
        }
    } break;
    default:
        UNREACHABLE_CODE;
    }
}

void RelDeleteInfo::init(const ResultSet& resultSet) {
    srcNodeIDVector = resultSet.getValueVector(srcNodeIDPos).get();
    dstNodeIDVector = resultSet.getValueVector(dstNodeIDPos).get();
    relIDVector = resultSet.getValueVector(relIDPos).get();
}

void RelDeleteExecutor::init(ResultSet* resultSet, ExecutionContext*) {
    info.init(*resultSet);
}

void SingleLabelRelDeleteExecutor::delete_(ExecutionContext* context) {
    auto deleteState = std::make_unique<RelTableDeleteState>(*info.srcNodeIDVector,
        *info.dstNodeIDVector, *info.relIDVector);
    table->delete_(Transaction::Get(*context->clientContext), *deleteState);
}

void MultiLabelRelDeleteExecutor::delete_(ExecutionContext* context) {
    auto& idSelVector = info.relIDVector->state->getSelVector();
    DASSERT(idSelVector.getSelSize() == 1);
    const auto pos = idSelVector[0];
    const auto relID = info.relIDVector->getValue<internalID_t>(pos);
    DASSERT(tableIDToTableMap.contains(relID.tableID));
    auto table = tableIDToTableMap.at(relID.tableID);
    auto deleteState = std::make_unique<RelTableDeleteState>(*info.srcNodeIDVector,
        *info.dstNodeIDVector, *info.relIDVector);
    table->delete_(Transaction::Get(*context->clientContext), *deleteState);
}

} // namespace processor
} // namespace riag
