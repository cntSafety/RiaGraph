#include "transaction/transaction.h"

#include "catalog/catalog.h"
#include "catalog/catalog_entry/sequence_catalog_entry.h"
#include "catalog/namespace_catalog.h"
#include "common/exception/runtime.h"
#include "main/client_context.h"
#include "main/database.h"
#include "main/database_manager.h"
#include "main/db_config.h"
#include "storage/local_storage/local_node_table.h"
#include "storage/local_storage/local_storage.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/undo_buffer.h"
#include "storage/wal/local_wal.h"
#include "transaction/transaction_context.h"
#include <format>

using namespace riag::catalog;

namespace riag {
namespace transaction {

bool LocalCacheManager::put(std::unique_ptr<LocalCacheObject> object) {
    std::unique_lock lck{mtx};
    const auto key = object->getKey();
    if (cachedObjects.contains(key)) {
        return false;
    }
    cachedObjects[object->getKey()] = std::move(object);
    return true;
}

Transaction::Transaction(main::ClientContext& clientContext, TransactionType transactionType,
    common::transaction_t transactionID, common::transaction_t startTS)
    : type{transactionType}, ID{transactionID}, startTS{startTS},
      commitTS{common::INVALID_TRANSACTION}, forceCheckpoint{false} {
    this->clientContext = &clientContext;
    localStorage = std::make_unique<storage::LocalStorage>(clientContext);
    undoBuffer = std::make_unique<storage::UndoBuffer>(storage::MemoryManager::Get(clientContext));
    currentTS = common::Timestamp::getCurrentTimestamp().value;
    localWAL = std::make_unique<storage::LocalWAL>(*storage::MemoryManager::Get(clientContext),
        clientContext.getDBConfig()->enableChecksums);
}

Transaction::Transaction(TransactionType transactionType) noexcept
    : type{transactionType}, ID{DUMMY_TRANSACTION_ID}, startTS{DUMMY_START_TIMESTAMP},
      commitTS{common::INVALID_TRANSACTION}, clientContext{nullptr}, undoBuffer{nullptr},
      forceCheckpoint{false} {
    currentTS = common::Timestamp::getCurrentTimestamp().value;
}

Transaction::Transaction(TransactionType transactionType, common::transaction_t ID,
    common::transaction_t startTS) noexcept
    : type{transactionType}, ID{ID}, startTS{startTS}, commitTS{common::INVALID_TRANSACTION},
      clientContext{nullptr}, undoBuffer{nullptr}, forceCheckpoint{false} {
    currentTS = common::Timestamp::getCurrentTimestamp().value;
}

bool Transaction::shouldLogToWAL() const {
    return isWriteTransaction() && !clientContext->isInMemory();
}

bool Transaction::shouldForceCheckpoint() const {
    return !clientContext->isInMemory() && forceCheckpoint;
}

void Transaction::commit(storage::WAL* wal) {
    if (commitTS == common::INVALID_TRANSACTION) {
        throw common::RuntimeException{"Cannot commit with an invalid commit timestamp."};
    }
    uint64_t walCommitSequence = 0;
    writeCommitToWAL(wal, walCommitSequence);
    publishCommit();
}

void Transaction::writeCommitToWAL(storage::WAL* wal, uint64_t& walCommitSequence) {
    walCommitSequence = 0;
    auto* namespaces = namespaceState.load(std::memory_order_acquire);
    const auto hasNamespaceData = namespaces && !namespaces->dataCatalogs.empty();
    if (shouldLogToWAL() && (!changedCatalogs.empty() || hasNamespaceData)) {
        auto owners = changedCatalogs;
        if (hasNamespaceData) {
            owners.insert(namespaces->dataCatalogs.begin(), namespaces->dataCatalogs.end());
        }
        for (auto* owner : owners) {
            if (const auto* namespaces = owner->getNamespaces();
                namespaces && namespaces->hasAllocatedEntries()) {
                clientContext->getDatabase()->getStorageManager()->ensureNamespaceStorageVersion(
                    *clientContext);
                storage::PartitionStorageRegistry::resolveOwnerStorageManager(clientContext, owner)
                    ->ensureNamespaceStorageVersion(*clientContext);
            }
        }
    }
    if (hasNamespaceData) {
        // Finalizing local edge IDs may add destination groups to this set.
        // Visit a snapshot of this transaction's groups, never the whole catalog.
        const std::vector<catalog::CatalogEntry*> stagedGroups(namespaces->groups.begin(),
            namespaces->groups.end());
        for (const auto* entry : stagedGroups) {
            if (entry->getNext() == nullptr && !entry->isDeleted()) {
                entry->getOwningCatalog()->getNamespaces()->finalizeLocalRelationshipGroup(this,
                    entry->constCast<catalog::NamespaceCatalogEntry>());
            }
        }
    }
    if (!shouldLogToWAL()) {
        return;
    }
    DASSERT(localWAL && wal);
    if (namespaces) {
        for (auto* entry : namespaces->groups) {
            // A later DROP/replace may have superseded the staged group. Only the
            // final live version is written, after its row updates are complete.
            if (entry->getNext() == nullptr && !entry->isDeleted()) {
                localWAL->logCreateCatalogEntryRecord(entry->getOwningCatalogName(), entry, false);
            }
        }
    }
    localWAL->logCommit();
    wal->logCommittedWAL(*localWAL, clientContext, walCommitSequence);
    localWAL->clear();
}

void Transaction::publishCommit() {
    if (commitTS == common::INVALID_TRANSACTION) {
        throw common::RuntimeException{"Cannot publish commit with an invalid commit timestamp."};
    }
    localStorage->commit();
    undoBuffer->commit(clientContext, commitTS);
    if (auto* namespaces = namespaceState.load(std::memory_order_acquire);
        namespaces && !namespaces->dataCatalogs.empty()) {
        auto* mainCatalog = clientContext->getDatabase()->getCatalog();
        if (namespaces->dataCatalogs.erase(mainCatalog)) {
            mainCatalog->incrementVersion(false);
        }
        if (!namespaces->dataCatalogs.empty()) {
            main::DatabaseManager::Get(*clientContext)
                ->bumpGraphCatalogVersions(namespaces->dataCatalogs, false);
        }
        namespaces->dataCatalogs.clear();
    }
    std::unordered_set<catalog::Catalog*> graphCatalogsToBump;
    catalog::Catalog* mainCatalogToBump = nullptr;
    {
        std::lock_guard lck{changedCatalogsMutex};
        if (!changedCatalogs.empty()) {
            auto* mainCatalog = clientContext->getDatabase()->getCatalog();
            for (auto* catalog : changedCatalogs) {
                if (catalog == mainCatalog) {
                    mainCatalogToBump = mainCatalog;
                } else {
                    graphCatalogsToBump.insert(catalog);
                }
            }
            changedCatalogs.clear();
        }
    }
    // Bump versions without holding changedCatalogsMutex: holding it while acquiring the
    // database-manager graphs lock inverts the lock order vs CatalogSet::alterTableEntry
    // (-> changedCatalogsMutex) vs withGraphCatalog (-> CatalogSet) and trips TSan
    // lock-order warnings.
    if (mainCatalogToBump != nullptr) {
        mainCatalogToBump->incrementVersion();
    }
    // A graph dropped after its change was recorded - in this or a concurrent
    // transaction - destroys its catalog. Route the graph bumps through the
    // database manager: under its registry lock, only catalogs it currently
    // owns are advanced, so a catalog being dropped concurrently is either
    // still owned (bumped before removal) or already gone (never touched).
    if (!graphCatalogsToBump.empty()) {
        auto* dbManager = main::DatabaseManager::Get(*clientContext);
        if (dbManager != nullptr) {
            dbManager->bumpGraphCatalogVersions(graphCatalogsToBump);
        }
    }
    for (auto& callback : commitCallbacks) {
        callback(*this);
    }
    commitCallbacks.clear();
    rollbackCallbacks.clear();
}

void Transaction::rollback(storage::WAL*) {
    // Rolling back the local storage will free + evict all optimistically-allocated pages
    // Since the undo buffer may do some scanning (e.g. to delete inserted keys from the hash index)
    // this must be rolled back first
    undoBuffer->rollback(clientContext);
    localStorage->rollback();
    {
        std::lock_guard lck{changedCatalogsMutex};
        changedCatalogs.clear();
    }
    for (auto& callback : rollbackCallbacks) {
        callback(*this);
    }
    commitCallbacks.clear();
    rollbackCallbacks.clear();
}

void Transaction::pushCommitCallback(std::function<void(Transaction&)> callback) {
    commitCallbacks.push_back(std::move(callback));
}

void Transaction::pushRollbackCallback(std::function<void(Transaction&)> callback) {
    rollbackCallbacks.push_back(std::move(callback));
}

bool Transaction::isUnCommitted(const storage::Table& table, common::offset_t nodeOffset) const {
    return localStorage && localStorage->getLocalTable(table) &&
           nodeOffset >= getMinUncommittedNodeOffset(table);
}

bool Transaction::isUnCommitted(common::table_id_t tableID, common::offset_t nodeOffset) const {
    return localStorage && localStorage->getLocalTable(tableID) &&
           nodeOffset >= getMinUncommittedNodeOffset(tableID);
}

std::optional<common::row_idx_t> Transaction::tryGetLocalRowIdx(const storage::Table& table,
    common::offset_t nodeOffset) const {
    if (!localStorage) {
        return std::nullopt;
    }
    auto* localTable = localStorage->getLocalTable(table);
    if (!localTable) {
        return std::nullopt;
    }
    const auto startOffset = localTable->cast<storage::LocalNodeTable>().getStartOffset();
    if (nodeOffset < startOffset) {
        return std::nullopt;
    }
    return nodeOffset - startOffset;
}

void Transaction::pushCreateDropCatalogEntry(CatalogSet& catalogSet, CatalogEntry& catalogEntry,
    bool isInternal, bool skipLoggingToWAL) {
    undoBuffer->createCatalogEntry(catalogSet, catalogEntry);
    if (catalogEntry.getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY ||
        catalogEntry.getNext()->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
        std::lock_guard lock{changedCatalogsMutex};
        getOrCreateNamespaceStateNoLock().dataCatalogs.insert(catalogSet.getCatalog());
    } else {
        recordCatalogChange(catalogSet.getCatalog());
    }
    if (!shouldLogToWAL() || skipLoggingToWAL) {
        return;
    }
    DASSERT(localWAL);
    const auto newCatalogEntry = catalogEntry.getNext();
    switch (newCatalogEntry->getType()) {
    case CatalogEntryType::NAMESPACE_ENTRY:
    case CatalogEntryType::NAMESPACE_POLICY_ENTRY:
    case CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY: {
        localWAL->logCreateCatalogEntryRecord(catalogSet.getOwnerCatalogName(), newCatalogEntry,
            isInternal);
    } break;
    case CatalogEntryType::INDEX_ENTRY:
    case CatalogEntryType::NODE_TABLE_ENTRY:
    case CatalogEntryType::REL_GROUP_ENTRY: {
        if (catalogEntry.getType() == CatalogEntryType::DUMMY_ENTRY) {
            DASSERT(catalogEntry.isDeleted());
            localWAL->logCreateCatalogEntryRecord(catalogSet.getOwnerCatalogName(), newCatalogEntry,
                isInternal);
        } else {
            throw common::RuntimeException("This shouldn't happen. Alter table is not supported.");
        }
    } break;
    case CatalogEntryType::SEQUENCE_ENTRY: {
        DASSERT(
            catalogEntry.getType() == CatalogEntryType::DUMMY_ENTRY && catalogEntry.isDeleted());
        if (newCatalogEntry->hasParent()) {
            // We don't log SERIAL catalog entry creation as it is implicit
            return;
        }
        localWAL->logCreateCatalogEntryRecord(catalogSet.getOwnerCatalogName(), newCatalogEntry,
            isInternal);
    } break;
    case CatalogEntryType::SCALAR_MACRO_ENTRY:
    case CatalogEntryType::TYPE_ENTRY:
    case CatalogEntryType::GRAPH_ENTRY: {
        DASSERT(
            catalogEntry.getType() == CatalogEntryType::DUMMY_ENTRY && catalogEntry.isDeleted());
        localWAL->logCreateCatalogEntryRecord(catalogSet.getOwnerCatalogName(), newCatalogEntry,
            isInternal);
    } break;
    case CatalogEntryType::DUMMY_ENTRY: {
        DASSERT(newCatalogEntry->isDeleted());
        if (catalogEntry.hasParent()) {
            return;
        }
        switch (catalogEntry.getType()) {
        case CatalogEntryType::INDEX_ENTRY:
        case CatalogEntryType::SCALAR_MACRO_ENTRY:
        case CatalogEntryType::NODE_TABLE_ENTRY:
        case CatalogEntryType::REL_GROUP_ENTRY:
        case CatalogEntryType::SEQUENCE_ENTRY:
        case CatalogEntryType::GRAPH_ENTRY: {
            localWAL->logDropCatalogEntryRecord(catalogSet.getOwnerCatalogName(),
                catalogEntry.getOID(), catalogEntry.getType());
        } break;
        case CatalogEntryType::NAMESPACE_ENTRY:
        case CatalogEntryType::NAMESPACE_POLICY_ENTRY:
        case CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY: {
            localWAL->logDropCatalogEntryRecord(catalogSet.getOwnerCatalogName(),
                catalogEntry.getOID(), catalogEntry.getType());
        } break;
        case CatalogEntryType::SCALAR_FUNCTION_ENTRY:
        case CatalogEntryType::TABLE_FUNCTION_ENTRY:
        case CatalogEntryType::STANDALONE_TABLE_FUNCTION_ENTRY: {
            // DO NOTHING. We don't persist function entries.
        } break;
        case CatalogEntryType::TYPE_ENTRY:
        default: {
            throw common::RuntimeException(std::format("Not supported catalog entry type {} yet.",
                CatalogEntryTypeUtils::toString(catalogEntry.getType())));
        }
        }
    } break;
    case CatalogEntryType::SCALAR_FUNCTION_ENTRY:
    case CatalogEntryType::TABLE_FUNCTION_ENTRY:
    case CatalogEntryType::STANDALONE_TABLE_FUNCTION_ENTRY: {
        // DO NOTHING. We don't persist function entries.
    } break;
    default: {
        throw common::RuntimeException(std::format("Not supported catalog entry type {} yet.",
            CatalogEntryTypeUtils::toString(catalogEntry.getType())));
    }
    }
}

void Transaction::pushAlterCatalogEntry(CatalogSet& catalogSet, CatalogEntry& catalogEntry,
    const binder::BoundAlterInfo& alterInfo, bool skipLoggingToWAL,
    common::table_id_t addedRelTableOID) {
    undoBuffer->createCatalogEntry(catalogSet, catalogEntry);
    recordCatalogChange(catalogSet.getCatalog());
    if (shouldLogToWAL() && !skipLoggingToWAL) {
        DASSERT(localWAL);
        localWAL->logAlterCatalogEntryRecord(catalogSet.getOwnerCatalogName(), &alterInfo,
            addedRelTableOID);
    }
}

void Transaction::pushSequenceChange(SequenceCatalogEntry* sequenceEntry, int64_t kCount,
    const SequenceRollbackData& data) {
    undoBuffer->createSequenceChange(*sequenceEntry, data);
    recordCatalogChange(sequenceEntry->getOwningCatalog());
    if (shouldLogToWAL()) {
        DASSERT(localWAL);
        localWAL->logUpdateSequenceRecord(sequenceEntry->getOwningCatalogName(),
            sequenceEntry->getOID(), kCount, sequenceEntry->getName());
    }
}

void Transaction::retireGraphCatalog(std::unique_ptr<catalog::Catalog> catalog) {
    retiredGraphCatalogs.push_back(std::move(catalog));
}

void Transaction::pushInsertInfo(common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
    common::row_idx_t numRows, const storage::VersionRecordHandler* versionRecordHandler) const {
    undoBuffer->createInsertInfo(nodeGroupIdx, startRow, numRows, versionRecordHandler);
}

void Transaction::pushDeleteInfo(common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
    common::row_idx_t numRows, const storage::VersionRecordHandler* versionRecordHandler) const {
    undoBuffer->createDeleteInfo(nodeGroupIdx, startRow, numRows, versionRecordHandler);
}

void Transaction::pushVectorUpdateInfo(storage::UpdateInfo& updateInfo,
    const common::idx_t vectorIdx, storage::VectorUpdateInfo& vectorUpdateInfo,
    common::transaction_t version) const {
    undoBuffer->createVectorUpdateInfo(&updateInfo, vectorIdx, &vectorUpdateInfo, version);
}

Transaction::~Transaction() = default;

common::offset_t Transaction::getMinUncommittedNodeOffset(const storage::Table& table) const {
    if (localStorage && localStorage->getLocalTable(table)) {
        return localStorage->getLocalTable(table)->cast<storage::LocalNodeTable>().getStartOffset();
    }
    return 0;
}

common::offset_t Transaction::getMinUncommittedNodeOffset(common::table_id_t tableID) const {
    if (localStorage) {
        if (auto* localTable = localStorage->getLocalTable(tableID)) {
            return localTable->cast<storage::LocalNodeTable>().getStartOffset();
        }
    }
    return 0;
}

Transaction::NamespaceState& Transaction::getOrCreateNamespaceStateNoLock() {
    if (!namespaceStateOwner) {
        namespaceStateOwner = std::make_unique<NamespaceState>();
        namespaceState.store(namespaceStateOwner.get(), std::memory_order_release);
    }
    return *namespaceStateOwner;
}

void Transaction::recordNamespaceGroup(catalog::CatalogEntry* entry) {
    std::lock_guard lock{changedCatalogsMutex};
    getOrCreateNamespaceStateNoLock().groups.insert(entry);
}

void Transaction::incrementNamespaceGeneration() {
    auto* state = namespaceState.load(std::memory_order_acquire);
    if (!state) {
        std::lock_guard lock{changedCatalogsMutex};
        state = &getOrCreateNamespaceStateNoLock();
    }
    state->generation.fetch_add(1, std::memory_order_relaxed);
}

void Transaction::recordCatalogChange(catalog::Catalog* catalog) {
    if (catalog == nullptr) {
        return;
    }
    std::lock_guard lck{changedCatalogsMutex};
    changedCatalogs.insert(catalog);
}

Transaction* Transaction::Get(const main::ClientContext& context) {
    return TransactionContext::Get(context)->getActiveTransaction();
}

Transaction DUMMY_TRANSACTION = Transaction(TransactionType::DUMMY);
Transaction DUMMY_CHECKPOINT_TRANSACTION = Transaction(TransactionType::CHECKPOINT,
    Transaction::DUMMY_TRANSACTION_ID, Transaction::START_TRANSACTION_ID - 1);

} // namespace transaction
} // namespace riag
