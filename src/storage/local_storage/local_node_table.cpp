#include "storage/local_storage/local_node_table.h"

#include "catalog/catalog.h"
#include "catalog/catalog_entry/node_table_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/cast.h"
#include "common/exception/message.h"
#include "common/exception/runtime.h"
#include "common/types/types.h"
#include "common/types/value/value.h"
#include "storage/index/hash_index.h"
#include "storage/storage_utils.h"
#include "storage/table/node_table.h"

using namespace riag::common;
using namespace riag::transaction;

namespace riag {
namespace storage {

std::vector<LogicalType> LocalNodeTable::getNodeTableColumnTypes(
    const catalog::TableCatalogEntry& table) {
    std::vector<LogicalType> types;
    for (auto& property : table.getProperties()) {
        types.push_back(property.getType().copy());
    }
    return types;
}

LocalNodeTable::LocalNodeTable(const catalog::TableCatalogEntry* tableEntry, Table& table,
    MemoryManager& mm)
    : LocalTable{table}, overflowFileHandle(nullptr),
      nodeGroups{mm, getNodeTableColumnTypes(*tableEntry), false /*enableCompression*/} {
    initLocalHashIndex(mm);
    if (table.cast<NodeTable>().hasLocalPrimaryKey()) {
        namespaceIndexes = std::make_unique<NamespaceIndexes>(mm);
    }
    startOffset = table.getNumTotalRows(nullptr /* transaction */);
}

void LocalNodeTable::initLocalHashIndex(MemoryManager& mm) {
    auto& nodeTable = dynamic_cast_checked<const NodeTable&>(table);
    overflowFile = std::make_unique<InMemOverflowFile>(mm);
    overflowFileHandle = overflowFile->addHandle();
    hashIndex = std::make_unique<LocalHashIndex>(mm,
        nodeTable.getColumn(nodeTable.getPKColumnID()).getDataType().getPhysicalType(),
        overflowFileHandle);
}

bool LocalNodeTable::isVisible(const Transaction* transaction, offset_t offset) const {
    if (offset < startOffset) {
        throw RuntimeException(
            "Primary-key index contains an invalid local node offset. Please drop and rebuild "
            "the _PK index.");
    }
    auto [nodeGroupIdx, offsetInGroup] =
        StorageUtils::getNodeGroupIdxAndOffsetInChunk(offset - startOffset);
    auto* nodeGroup = nodeGroups.getNodeGroup(nodeGroupIdx, true /*mayOutOfBound*/);
    if (nodeGroup == nullptr || offsetInGroup >= nodeGroup->getNumRows()) {
        throw RuntimeException(
            "Primary-key index contains an invalid local node offset. Please drop and rebuild "
            "the _PK index.");
    }
    if (nodeGroup->isDeleted(transaction, offsetInGroup)) {
        return false;
    }
    return nodeGroup->isInserted(transaction, offsetInGroup);
}

offset_t LocalNodeTable::validateUniquenessConstraint(const Transaction* transaction,
    const ValueVector& pkVector) const {
    DASSERT(pkVector.state->getSelVector().getSelSize() == 1);
    return hashIndex->lookup(pkVector,
        [&](offset_t offset_) { return isVisible(transaction, offset_); });
}

bool LocalNodeTable::insert(Transaction* transaction, TableInsertState& insertState) {
    auto& nodeInsertState = insertState.constCast<NodeTableInsertState>();
    const auto nodeOffset = startOffset + nodeGroups.getNumTotalRows();
    DASSERT(nodeInsertState.pkVector.state->getSelVector().getSelSize() == 1);
    if (table.cast<NodeTable>().hasLocalPrimaryKey()) {
        if (!transaction->isRecovery()) {
            insertNamespaceKey(transaction, nodeInsertState.pkVector, nodeOffset,
                nodeInsertState.namespaceID);
        }
    } else if (!hashIndex->insert(nodeInsertState.pkVector, nodeOffset,
                   [&](offset_t offset) { return isVisible(transaction, offset); })) {
        const auto val =
            nodeInsertState.pkVector.getAsValue(nodeInsertState.pkVector.state->getSelVector()[0]);
        throw RuntimeException(ExceptionMessage::duplicatePKException(val->toString()));
    }
    const auto nodeIDPos =
        nodeInsertState.nodeIDVector.state->getSelVector().getSelectedPositions()[0];
    nodeInsertState.nodeIDVector.setValue(nodeIDPos, internalID_t{nodeOffset, table.getTableID()});
    nodeGroups.append(&DUMMY_TRANSACTION, insertState.propertyVectors);
    return true;
}

bool LocalNodeTable::update(Transaction* transaction, TableUpdateState& updateState) {
    DASSERT(transaction->isDummy());
    const auto& nodeUpdateState = updateState.cast<NodeTableUpdateState>();
    DASSERT(nodeUpdateState.nodeIDVector.state->getSelVector().getSelSize() == 1);
    const auto pos = nodeUpdateState.nodeIDVector.state->getSelVector()[0];
    const auto offset = nodeUpdateState.nodeIDVector.readNodeOffset(pos);
    DASSERT(nodeUpdateState.columnID != table.cast<NodeTable>().getPKColumnID());
    DASSERT(offset >= startOffset);
    const auto [nodeGroupIdx, rowIdxInGroup] =
        StorageUtils::getQuotientRemainder(offset - startOffset, StorageConfig::NODE_GROUP_SIZE);
    const auto nodeGroup = nodeGroups.getNodeGroup(nodeGroupIdx);
    nodeGroup->update(transaction, rowIdxInGroup, nodeUpdateState.columnID,
        nodeUpdateState.propertyVector);
    return true;
}

bool LocalNodeTable::delete_(Transaction* transaction, TableDeleteState& deleteState) {
    DASSERT(transaction->isDummy());
    const auto& nodeDeleteState = deleteState.cast<NodeTableDeleteState>();
    DASSERT(nodeDeleteState.nodeIDVector.state->getSelVector().getSelSize() == 1);
    const auto pos = nodeDeleteState.nodeIDVector.state->getSelVector()[0];
    const auto offset = nodeDeleteState.nodeIDVector.readNodeOffset(pos);
    DASSERT(offset >= startOffset);
    if (!table.cast<NodeTable>().hasLocalPrimaryKey()) {
        hashIndex->delete_(nodeDeleteState.pkVector);
    }
    const auto [nodeGroupIdx, rowIdxInGroup] =
        StorageUtils::getQuotientRemainder(offset - startOffset, StorageConfig::NODE_GROUP_SIZE);
    const auto nodeGroup = nodeGroups.getNodeGroup(nodeGroupIdx);
    return nodeGroup->delete_(transaction, rowIdxInGroup);
}

bool LocalNodeTable::addColumn(TableAddColumnState& addColumnState) {
    nodeGroups.addColumn(addColumnState);
    return true;
}

void LocalNodeTable::clear(MemoryManager& mm) {
    if (namespaceIndexes) {
        namespaceIndexes->indexes.clear();
    }
    auto& nodeTable = dynamic_cast_checked<const NodeTable&>(table);
    hashIndex = std::make_unique<LocalHashIndex>(mm,
        nodeTable.getColumn(nodeTable.getPKColumnID()).getDataType().getPhysicalType(),
        overflowFileHandle);
    nodeGroups.clear();
}

bool LocalNodeTable::lookupPK(const Transaction* transaction, const ValueVector* keyVector,
    sel_t pos, offset_t& result) const {
    if (table.cast<NodeTable>().hasLocalPrimaryKey()) {
        std::vector<offset_t> results;
        lookupPKInNamespaces(transaction, keyVector, pos, NamespaceSet{}, results);
        if (results.size() > 1) {
            throw RuntimeException(
                "A LOCAL primary key is ambiguous without a namespace selector.");
        }
        if (results.empty()) {
            return false;
        }
        result = results.front();
        return true;
    }
    result = hashIndex->lookup(*keyVector, pos,
        [&](offset_t offset) { return isVisible(transaction, offset); });
    return result != INVALID_OFFSET;
}

LocalHashIndex& LocalNodeTable::namespaceIndex(namespace_id_t namespaceID) {
    DASSERT(namespaceIndexes);
    auto& index = namespaceIndexes->indexes[namespaceID];
    if (!index) {
        index = std::make_unique<LocalHashIndex>(namespaceIndexes->memory,
            table.cast<NodeTable>()
                .getColumn(table.cast<NodeTable>().getPKColumnID())
                .getDataType()
                .getPhysicalType(),
            overflowFileHandle);
    }
    return *index;
}

bool LocalNodeTable::lookupPKInNamespace(const Transaction* transaction,
    const ValueVector* keyVector, sel_t pos, namespace_id_t namespaceID, offset_t& result) const {
    DASSERT(namespaceIndexes);
    const auto it = namespaceIndexes->indexes.find(namespaceID);
    if (it == namespaceIndexes->indexes.end()) {
        return false;
    }
    catalog::NamespaceReadState memberships(table.getOwningCatalog()->getNamespaces(), transaction,
        table.getTableID());
    result = it->second->lookup(*keyVector, pos, [&](offset_t offset) {
        return isVisible(transaction, offset) && memberships.get(offset) == namespaceID;
    });
    return result != INVALID_OFFSET;
}

void LocalNodeTable::lookupPKInNamespaces(const Transaction* transaction,
    const ValueVector* keyVector, sel_t pos, const NamespaceSet& scope,
    std::vector<offset_t>& results) const {
    const auto lookup = [&](namespace_id_t id) {
        offset_t offset;
        if (lookupPKInNamespace(transaction, keyVector, pos, id, offset)) {
            results.push_back(offset);
        }
    };
    if (scope.isUnrestricted()) {
        for (const auto& [id, _] : namespaceIndexes->indexes) {
            lookup(id);
        }
    } else {
        for (auto id : scope.getIDs()) {
            lookup(id);
        }
    }
}

void LocalNodeTable::insertNamespaceKey(const Transaction* transaction, const ValueVector& key,
    offset_t offset, namespace_id_t namespaceID) {
    catalog::NamespaceReadState memberships(table.getOwningCatalog()->getNamespaces(), transaction,
        table.getTableID());
    if (!namespaceIndex(namespaceID).insert(key, offset, [&](offset_t existing) {
            return isVisible(transaction, existing) && memberships.get(existing) == namespaceID;
        })) {
        throw RuntimeException("Duplicate LOCAL primary key in the destination namespace.");
    }
}

} // namespace storage
} // namespace riag
