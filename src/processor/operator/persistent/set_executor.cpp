#include "processor/operator/persistent/set_executor.h"

#include <algorithm>

#include "common/data_chunk/data_chunk.h"
#include "storage/buffer_manager/memory_manager.h"
#include "transaction/transaction.h"

using namespace riag::common;

namespace riag {
namespace processor {

void NodeSetInfo::init(const ResultSet& resultSet, main::ClientContext* context) {
    nodeIDVector = resultSet.getValueVector(nodeIDPos).get();
    if (columnVectorPos.isValid()) {
        columnVector = resultSet.getValueVector(columnVectorPos).get();
    }
    evaluator->init(resultSet, context);
    columnDataVector = evaluator->resultVector.get();
}

void NodeSetExecutor::init(ResultSet* resultSet, ExecutionContext* context) {
    info.init(*resultSet, context->clientContext);
}

void NodeSetExecutor::setNodeID(nodeID_t nodeID) const {
    info.nodeIDVector->setValue(info.nodeIDVector->state->getSelVector()[0], nodeID);
}

void NodeSetExecutor::refreshColumn(ExecutionContext* context) {
    if (!info.columnVectorPos.isValid()) {
        return;
    }
    auto& idSelVector = info.nodeIDVector->state->getSelVector();
    DASSERT(idSelVector.getSelSize() == 1);
    if (info.nodeIDVector->isNull(idSelVector[0])) {
        return;
    }
    const auto nodeID = info.nodeIDVector->getValue<nodeID_t>(idSelVector[0]);
    const auto* tableInfo = getTableInfo(nodeID.tableID);
    if (tableInfo == nullptr) {
        return;
    }
    auto transaction = transaction::Transaction::Get(*context->clientContext);
    if (refreshState == nullptr) {
        // Look up through private vectors so the lookup cannot disturb the operator's vectors.
        auto* mm = storage::MemoryManager::Get(*context->clientContext);
        auto state = DataChunkState::getSingleValueDataChunkState();
        refreshState = std::make_unique<NodeSetRefreshState>();
        refreshState->idVector =
            std::make_unique<ValueVector>(LogicalType::INTERNAL_ID(), mm, state);
        refreshState->dataVector =
            std::make_unique<ValueVector>(info.columnVector->dataType.copy(), mm, state);
        refreshState->scanState = std::make_unique<storage::NodeTableScanState>(
            refreshState->idVector.get(), std::vector{refreshState->dataVector.get()}, state);
    }
    auto& refresh = *refreshState;
    if (refresh.tableInfo != tableInfo) {
        refresh.scanState->setToTable(transaction, tableInfo->table, {tableInfo->columnID}, {});
        refresh.tableInfo = tableInfo;
    }
    refresh.idVector->setValue<nodeID_t>(0, nodeID);
    refresh.idVector->setNull(0, false);
    refresh.dataVector->resetAuxiliaryBuffer();
    tableInfo->table->initScanState(transaction, *refresh.scanState, nodeID.tableID, nodeID.offset);
    const auto columnPos = info.columnVector->state->getSelVector()[0];
    if (!tableInfo->table->lookup(transaction, *refresh.scanState) ||
        refresh.dataVector->isNull(0)) {
        info.columnVector->setNull(columnPos, true);
        return;
    }
    info.columnVector->setNull(columnPos, false);
    info.columnVector->copyFromVectorData(columnPos, refresh.dataVector.get(), 0);
}

static void writeColumnUpdateResult(ValueVector* idVector, ValueVector* columnVector,
    ValueVector* dataVector) {
    auto& idSelVector = idVector->state->getSelVector();
    auto& columnSelVector = columnVector->state->getSelVector();
    auto& dataSelVector = dataVector->state->getSelVector();
    DASSERT(idSelVector.getSelSize() == 1);
    if (idVector->isNull(idSelVector[0])) { // No update happened.
        return;
    }
    DASSERT(dataSelVector.getSelSize() == 1);
    if (dataVector->isNull(dataSelVector[0])) { // Update to NULL
        columnVector->setNull(columnSelVector[0], true);
        return;
    }
    columnVector->setNull(columnSelVector[0], false);
    columnVector->copyFromVectorData(columnSelVector[0], dataVector, dataSelVector[0]);
}

void SingleLabelNodeSetExecutor::set(ExecutionContext* context) {
    if (tableInfo.columnID == INVALID_COLUMN_ID) {
        // Not a valid column. Set projected column to null.
        if (info.columnVectorPos.isValid()) {
            info.columnVector->setNull(info.columnVector->state->getSelVector()[0], true);
        }
        return;
    }
    info.evaluator->evaluate();
    auto updateState = std::make_unique<storage::NodeTableUpdateState>(tableInfo.columnID,
        *info.nodeIDVector, *info.columnDataVector);
    tableInfo.table->initUpdateState(context->clientContext, *updateState);
    tableInfo.table->update(transaction::Transaction::Get(*context->clientContext), *updateState);
    if (info.columnVectorPos.isValid()) {
        writeColumnUpdateResult(info.nodeIDVector, info.columnVector, info.columnDataVector);
    }
}

void MultiLabelNodeSetExecutor::set(ExecutionContext* context) {
    info.evaluator->evaluate();
    auto& nodeIDSelVector = info.nodeIDVector->state->getSelVector();
    DASSERT(nodeIDSelVector.getSelSize() == 1);
    auto nodeIDPos = nodeIDSelVector[0];
    auto& nodeID = info.nodeIDVector->getValue<internalID_t>(nodeIDPos);
    if (!tableInfos.contains(nodeID.tableID)) {
        if (info.columnVectorPos.isValid()) {
            info.columnVector->setNull(info.columnVector->state->getSelVector()[0], true);
        }
        return;
    }
    auto& tableInfo = tableInfos.at(nodeID.tableID);
    auto updateState = std::make_unique<storage::NodeTableUpdateState>(tableInfo.columnID,
        *info.nodeIDVector, *info.columnDataVector);
    tableInfo.table->initUpdateState(context->clientContext, *updateState);
    tableInfo.table->update(transaction::Transaction::Get(*context->clientContext), *updateState);
    if (info.columnVectorPos.isValid()) {
        writeColumnUpdateResult(info.nodeIDVector, info.columnVector, info.columnDataVector);
    }
}

void RelSetInfo::init(const ResultSet& resultSet, main::ClientContext* context) {
    srcNodeIDVector = resultSet.getValueVector(srcNodeIDPos).get();
    dstNodeIDVector = resultSet.getValueVector(dstNodeIDPos).get();
    relIDVector = resultSet.getValueVector(relIDPos).get();
    if (columnVectorPos.isValid()) {
        columnVector = resultSet.getValueVector(columnVectorPos).get();
    }
    evaluator->init(resultSet, context);
    columnDataVector = evaluator->resultVector.get();
}

void RelSetExecutor::init(ResultSet* resultSet, ExecutionContext* context) {
    info.init(*resultSet, context->clientContext);
}

void RelSetExecutor::setRelID(nodeID_t relID) const {
    info.relIDVector->setValue(info.relIDVector->state->getSelVector()[0], relID);
}

void RelSetExecutor::refreshColumn(ExecutionContext* context) {
    if (!info.columnVectorPos.isValid()) {
        return;
    }
    auto& idSelVector = info.relIDVector->state->getSelVector();
    DASSERT(idSelVector.getSelSize() == 1);
    if (info.relIDVector->isNull(idSelVector[0])) {
        return;
    }
    const auto relID = info.relIDVector->getValue<internalID_t>(idSelVector[0]);
    const auto* tableInfo = getTableInfo(relID.tableID);
    if (tableInfo == nullptr) {
        return;
    }
    auto* table = tableInfo->table;
    auto transaction = transaction::Transaction::Get(*context->clientContext);
    if (refreshState == nullptr) {
        // Scan from a copy of the bound node ID so the scan cannot disturb the operator's vectors.
        auto* mm = storage::MemoryManager::Get(*context->clientContext);
        refreshState = std::make_unique<RelSetRefreshState>();
        refreshState->boundNodeIDVector = std::make_unique<ValueVector>(LogicalType::INTERNAL_ID(),
            mm, DataChunkState::getSingleValueDataChunkState());
        refreshState->scanChunk = std::make_unique<DataChunk>(2);
        refreshState->scanChunk->insert(0,
            std::make_shared<ValueVector>(LogicalType::INTERNAL_ID(), mm));
        refreshState->scanChunk->insert(1,
            std::make_shared<ValueVector>(info.columnVector->dataType.copy(), mm));
        refreshState->outputVectors = {&refreshState->scanChunk->getValueVectorMutable(0),
            &refreshState->scanChunk->getValueVectorMutable(1)};
        refreshState->scanState =
            std::make_unique<storage::RelTableScanState>(*mm, refreshState->boundNodeIDVector.get(),
                refreshState->outputVectors, refreshState->scanChunk->state, true /*randomLookup*/);
    }
    auto& refresh = *refreshState;
    if (refresh.tableInfo != tableInfo) {
        // There is no lookup by rel ID, so scan the rels of the bound node for this one, as
        // RelInsertExecutor::skipInsert does. RelTable::update finds the rel the same way.
        const auto directions = table->getStorageDirections();
        refresh.direction = std::ranges::count(directions, RelDataDirection::FWD) > 0 ?
                                RelDataDirection::FWD :
                                RelDataDirection::BWD;
        refresh.scanState->setToTable(transaction, table,
            {storage::REL_ID_COLUMN_ID, tableInfo->columnID}, {}, refresh.direction);
        refresh.tableInfo = tableInfo;
    }
    const auto* boundVector =
        refresh.direction == RelDataDirection::FWD ? info.srcNodeIDVector : info.dstNodeIDVector;
    const auto boundPos = boundVector->state->getSelVector()[0];
    if (boundVector->isNull(boundPos)) {
        return;
    }
    refresh.boundNodeIDVector->copyFromVectorData(0, boundVector, boundPos);
    refresh.boundNodeIDVector->setNull(0, false);
    refresh.outputVectors[1]->resetAuxiliaryBuffer();
    table->initScanState(transaction, *refresh.scanState);
    const auto columnPos = info.columnVector->state->getSelVector()[0];
    while (table->scan(transaction, *refresh.scanState)) {
        const auto& selVector = refresh.scanChunk->state->getSelVector();
        for (auto i = 0u; i < selVector.getSelSize(); ++i) {
            const auto pos = selVector[i];
            if (refresh.outputVectors[0]->getValue<internalID_t>(pos) != relID) {
                continue;
            }
            if (refresh.outputVectors[1]->isNull(pos)) {
                info.columnVector->setNull(columnPos, true);
            } else {
                info.columnVector->setNull(columnPos, false);
                info.columnVector->copyFromVectorData(columnPos, refresh.outputVectors[1], pos);
            }
            refresh.scanState->releaseScanPin();
            return;
        }
    }
    // Only a delete earlier in the statement removes the rel.
    info.columnVector->setNull(columnPos, true);
}

void SingleLabelRelSetExecutor::set(ExecutionContext* context) {
    if (tableInfo.columnID == INVALID_COLUMN_ID) {
        if (info.columnVectorPos.isValid()) {
            info.columnVector->setNull(info.columnVector->state->getSelVector()[0], true);
        }
        return;
    }
    info.evaluator->evaluate();
    auto updateState = std::make_unique<storage::RelTableUpdateState>(tableInfo.columnID,
        *info.srcNodeIDVector, *info.dstNodeIDVector, *info.relIDVector, *info.columnDataVector);
    tableInfo.table->update(transaction::Transaction::Get(*context->clientContext), *updateState);
    if (info.columnVectorPos.isValid()) {
        writeColumnUpdateResult(info.relIDVector, info.columnVector, info.columnDataVector);
    }
}

void MultiLabelRelSetExecutor::set(ExecutionContext* context) {
    info.evaluator->evaluate();
    auto& idSelVector = info.relIDVector->state->getSelVector();
    DASSERT(idSelVector.getSelSize() == 1);
    auto relID = info.relIDVector->getValue<internalID_t>(idSelVector[0]);
    if (!tableInfos.contains(relID.tableID)) {
        if (info.columnVectorPos.isValid()) {
            info.columnVector->setNull(info.columnVector->state->getSelVector()[0], true);
        }
        return;
    }
    auto& tableInfo = tableInfos.at(relID.tableID);
    auto updateState = std::make_unique<storage::RelTableUpdateState>(tableInfo.columnID,
        *info.srcNodeIDVector, *info.dstNodeIDVector, *info.relIDVector, *info.columnDataVector);
    tableInfo.table->update(transaction::Transaction::Get(*context->clientContext), *updateState);
    if (info.columnVectorPos.isValid()) {
        writeColumnUpdateResult(info.relIDVector, info.columnVector, info.columnDataVector);
    }
}

} // namespace processor
} // namespace riag
