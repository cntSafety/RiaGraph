#include "catalog/catalog_set.h"

#include <algorithm>
#include <mutex>

#include "binder/ddl/bound_alter_info.h"
#include "catalog/catalog.h"
#include "catalog/catalog_entry/dummy_catalog_entry.h"
#include "catalog/catalog_entry/namespace_catalog_entry.h"
#include "catalog/catalog_entry/rel_group_catalog_entry.h"
#include "catalog/catalog_entry/table_catalog_entry.h"
#include "common/assert.h"
#include "common/exception/catalog.h"
#include "common/serializer/deserializer.h"
#include "transaction/transaction.h"
#include <format>

using namespace riag::common;
using namespace riag::transaction;

namespace riag {
namespace catalog {

CatalogSet::CatalogSet(bool isInternal) {
    if (isInternal) {
        nextOID = INTERNAL_CATALOG_SET_START_OID;
    }
}

CatalogSet::CatalogSet(Catalog* catalog, bool isInternal) : catalog{catalog} {
    if (isInternal) {
        nextOID = INTERNAL_CATALOG_SET_START_OID;
    }
}

static bool checkWWConflict(const Transaction* transaction, const CatalogEntry* entry) {
    return (entry->getTimestamp() >= Transaction::START_TRANSACTION_ID &&
               entry->getTimestamp() != transaction->getID()) ||
           (entry->getTimestamp() < Transaction::START_TRANSACTION_ID &&
               entry->getTimestamp() > transaction->getStartTS());
}

bool CatalogSet::containsEntry(const Transaction* transaction, const std::string& name) {
    std::shared_lock lck{mtx};
    return containsEntryNoLock(transaction, name);
}

bool CatalogSet::containsEntryNoLock(const Transaction* transaction,
    const std::string& name) const {
    if (!entries.contains(name)) {
        return false;
    }
    // Check versions.
    const auto entry =
        traverseVersionChainsForTransactionNoLock(transaction, entries.at(name).get());
    return entry && !entry->isDeleted();
}

CatalogEntry* CatalogSet::getEntry(const Transaction* transaction, const std::string& name) {
    std::shared_lock lck{mtx};
    return getEntryNoLock(transaction, name);
}

CatalogEntry* CatalogSet::getEntryNoLock(const Transaction* transaction,
    const std::string& name) const {
    // LCOV_EXCL_START
    validateExistNoLock(transaction, name);
    // LCOV_EXCL_STOP
    const auto entry =
        traverseVersionChainsForTransactionNoLock(transaction, entries.at(name).get());
    DASSERT(entry != nullptr && !entry->isDeleted());
    return entry;
}

oid_t CatalogSet::createEntry(Transaction* transaction, std::unique_ptr<CatalogEntry> entry,
    bool skipLoggingToWAL, std::optional<oid_t> restoredOID) {
    CatalogEntry* entryPtr = nullptr;
    oid_t oid = INVALID_OID;
    {
        std::unique_lock lck{mtx};
        oid = restoredOID.has_value() ? *restoredOID : nextOID;
        nextOID = std::max(nextOID, oid + 1);
        entry->setOID(oid);
        entryPtr = createEntryNoLock(transaction, std::move(entry));
    }
    DASSERT(entryPtr);
    if (transaction->shouldAppendToUndoBuffer()) {
        transaction->pushCreateDropCatalogEntry(*this, *entryPtr, isInternal(), skipLoggingToWAL);
    }
    return oid;
}

CatalogEntry* CatalogSet::replaceNamespaceEntry(Transaction* transaction,
    std::unique_ptr<CatalogEntry> entry, bool skipLoggingToWAL) {
    const auto type = entry->getType();
    if (type != CatalogEntryType::NAMESPACE_POLICY_ENTRY &&
        type != CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
        throw CatalogException("Only namespace policy and membership entries may be replaced.");
    }
    CatalogEntry* oldEntry = nullptr;
    CatalogEntry* newEntry = nullptr;
    {
        std::unique_lock lck{mtx};
        validateExistNoLock(transaction, entry->getName());
        if (checkWWConflict(transaction, entries.at(entry->getName()).get())) {
            throw CatalogException("Write-write conflict updating namespace metadata.");
        }
        oldEntry = getEntryNoLock(transaction, entry->getName());
        entry->setOID(oldEntry->getOID());
        entry->setTimestamp(transaction->getID());
        newEntry = entry.get();
        emplaceNoLock(std::move(entry));
    }
    if (transaction->shouldAppendToUndoBuffer()) {
        transaction->pushCreateDropCatalogEntry(*this, *oldEntry, isInternal(), skipLoggingToWAL);
    }
    return newEntry;
}

CatalogEntry* CatalogSet::createEntryNoLock(const Transaction* transaction,
    std::unique_ptr<CatalogEntry> entry) {
    // LCOV_EXCL_START
    validateNotExistNoLock(transaction, entry->getName());
    // LCOV_EXCL_STOP
    entry->setTimestamp(transaction->getID());
    if (entries.contains(entry->getName())) {
        const auto existingEntry = entries.at(entry->getName()).get();
        if (checkWWConflict(transaction, existingEntry)) {
            throw CatalogException(std::format(
                "Write-write conflict on creating catalog entry with name {}.", entry->getName()));
        }
        if (!existingEntry->isDeleted()) {
            throw CatalogException(
                std::format("Catalog entry with name {} already exists.", entry->getName()));
        }
    }
    auto dummyEntry = createDummyEntryNoLock(entry->getName(), entry->getOID());
    entries.emplace(entry->getName(), std::move(dummyEntry));
    const auto entryPtr = entry.get();
    emplaceNoLock(std::move(entry));
    return entryPtr->getPrev();
}

void CatalogSet::emplaceNoLock(std::unique_ptr<CatalogEntry> entry) {
    entry->setOwningCatalog(catalog);
    if (entries.contains(entry->getName())) {
        entry->setPrev(std::move(entries.at(entry->getName())));
        entries.erase(entry->getName());
    }
    indexNamespaceEntryNoLock(entry.get());
    entries.emplace(entry->getName(), std::move(entry));
}

void CatalogSet::eraseNoLock(const std::string& name) {
    if (namespaceLookup) {
        const auto* entry = entries.at(name).get();
        const auto identity = namespaceLookup->identities.find(entry->getOID());
        if (identity != namespaceLookup->identities.end()) {
            const auto [tableID, groupIndex] = identity->second;
            auto& heads = namespaceLookup->heads.at(tableID);
            // A renamed definition's identity heads its new name's chain; erasing the chain
            // of an earlier name (when a rollback restores its tombstone) keeps that head.
            const auto head = heads.find(groupIndex);
            if (tableID != INVALID_TABLE_ID || (head != heads.end() && head->second == entry)) {
                heads.erase(groupIndex);
            }
        }
    }
    entries.erase(name);
}

void CatalogSet::enableNamespaceLookup() {
    std::unique_lock lock{mtx};
    namespaceLookup = std::make_unique<NamespaceLookup>();
    for (const auto& [_, entry] : entries) {
        indexNamespaceEntryNoLock(entry.get());
    }
}

void CatalogSet::indexNamespaceEntryNoLock(CatalogEntry* entry) {
    if (!namespaceLookup) {
        return;
    }
    if (entry->getType() == CatalogEntryType::NAMESPACE_ENTRY) {
        // A reserved table key also makes identity-to-name lookup independent
        // of the number of table policies and membership groups in this catalog.
        namespaceLookup->identities[entry->getOID()] = {INVALID_TABLE_ID, entry->getOID()};
    } else if (entry->getType() == CatalogEntryType::NAMESPACE_POLICY_ENTRY ||
               entry->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
        const auto& info = entry->constCast<NamespaceCatalogEntry>();
        namespaceLookup->identities[entry->getOID()] = {info.tableID,
            entry->getType() == CatalogEntryType::NAMESPACE_POLICY_ENTRY ? INVALID_ROW_IDX :
                                                                           info.groupIndex};
    }
    const auto identity = namespaceLookup->identities.find(entry->getOID());
    if (identity == namespaceLookup->identities.end()) {
        return;
    }
    const auto [tableID, groupIndex] = identity->second;
    auto& head = namespaceLookup->heads[tableID][groupIndex];
    if (tableID == INVALID_TABLE_ID && head && head->getName() != entry->getName()) {
        // A namespace definition's identity moved to another name's version chain.
        if (entry->getType() != CatalogEntryType::NAMESPACE_ENTRY) {
            // A tombstone or dummy restored by a rollback on an earlier name: the identity
            // stays on the chain that holds its newest definition.
            return;
        }
        // A rename: remember the earlier name for snapshots older than the rename.
        auto& names = namespaceLookup->previousNames[entry->getOID()];
        if (std::find(names.begin(), names.end(), head->getName()) == names.end()) {
            names.push_back(head->getName());
        }
    }
    head = entry;
}

CatalogEntry* CatalogSet::getNamespaceEntry(const Transaction* transaction, table_id_t tableID,
    uint64_t groupIndex) {
    std::shared_lock lock{mtx};
    DASSERT(namespaceLookup);
    const auto table = namespaceLookup->heads.find(tableID);
    if (table == namespaceLookup->heads.end()) {
        return nullptr;
    }
    const auto group = table->second.find(groupIndex);
    if (group == table->second.end()) {
        return nullptr;
    }
    auto* entry = traverseVersionChainsForTransactionNoLock(transaction, group->second);
    if (entry && !entry->isDeleted()) {
        return entry;
    }
    if (tableID == INVALID_TABLE_ID) {
        // A snapshot older than a rename of this namespace sees the definition under an
        // earlier name. At most one name holds a visible definition of an identity.
        const auto previous = namespaceLookup->previousNames.find(groupIndex);
        if (previous != namespaceLookup->previousNames.end()) {
            for (const auto& name : previous->second) {
                const auto chain = entries.find(name);
                if (chain == entries.end()) {
                    continue;
                }
                auto* visible =
                    traverseVersionChainsForTransactionNoLock(transaction, chain->second.get());
                if (visible && !visible->isDeleted() && visible->getOID() == groupIndex &&
                    visible->getType() == CatalogEntryType::NAMESPACE_ENTRY) {
                    return visible;
                }
            }
        }
    }
    return nullptr;
}

std::unique_ptr<CatalogEntry> CatalogSet::createDummyEntryNoLock(std::string name, oid_t oid) {
    return std::make_unique<DummyCatalogEntry>(std::move(name), oid);
}

std::vector<uint64_t> CatalogSet::getNamespaceGroupIndices(table_id_t tableID) const {
    std::shared_lock lock{mtx};
    DASSERT(namespaceLookup);
    std::vector<uint64_t> result;
    if (const auto table = namespaceLookup->heads.find(tableID);
        table != namespaceLookup->heads.end()) {
        result.reserve(table->second.size());
        for (const auto& [group, _] : table->second) {
            if (group != INVALID_ROW_IDX) {
                result.push_back(group);
            }
        }
    }
    return result;
}

void CatalogSet::pruneNamespaceMembershipVersions(table_id_t tableID, uint64_t groupIndex,
    transaction_t horizon) {
    if (horizon == INVALID_TRANSACTION) {
        return;
    }
    std::unique_lock lock{mtx};
    DASSERT(namespaceLookup);
    const auto table = namespaceLookup->heads.find(tableID);
    if (table == namespaceLookup->heads.end()) {
        return;
    }
    const auto group = table->second.find(groupIndex);
    if (group == table->second.end()) {
        return;
    }
    auto* boundary = group->second;
    while (boundary && boundary->getTimestamp() > horizon) {
        boundary = boundary->hasPrev() ? boundary->getPrev() : nullptr;
    }
    if (!boundary) {
        return;
    }
    // All active snapshots can see this boundary or a newer version. Preserve
    // the boundary itself for reader caches and writer undo records. Destroy
    // older chains iteratively: a long-running reader may have pinned many.
    auto retired = boundary->movePrev();
    while (retired) {
        auto older = retired->movePrev();
        retired = std::move(older);
    }
}

CatalogEntry* CatalogSet::traverseVersionChainsForTransactionNoLock(const Transaction* transaction,
    CatalogEntry* currentEntry) {
    while (currentEntry) {
        if (currentEntry->getTimestamp() == transaction->getID()) {
            // This entry is created by the current transaction.
            break;
        }
        if (currentEntry->getTimestamp() <= transaction->getStartTS()) {
            // This entry was committed before the current transaction starts.
            break;
        }
        currentEntry = currentEntry->hasPrev() ? currentEntry->getPrev() : nullptr;
    }
    return currentEntry;
}

CatalogEntry* CatalogSet::getCommittedEntryNoLock(CatalogEntry* entry) {
    while (entry) {
        if (entry->getTimestamp() < Transaction::START_TRANSACTION_ID) {
            break;
        }
        entry = entry->getPrev();
    }
    return entry;
}

void CatalogSet::dropEntry(Transaction* transaction, const std::string& name, oid_t oid) {
    CatalogEntry* entryPtr = nullptr;
    {
        std::unique_lock lck{mtx};
        entryPtr = dropEntryNoLock(transaction, name, oid);
    }
    DASSERT(entryPtr);
    if (transaction->shouldAppendToUndoBuffer()) {
        transaction->pushCreateDropCatalogEntry(*this, *entryPtr, isInternal());
    }
}

CatalogEntry* CatalogSet::dropEntryNoLock(const Transaction* transaction, const std::string& name,
    oid_t oid) {
    // LCOV_EXCL_START
    validateExistNoLock(transaction, name);
    // LCOV_EXCL_STOP
    auto tombstone = createDummyEntryNoLock(name, oid);
    tombstone->setTimestamp(transaction->getID());
    const auto tombstonePtr = tombstone.get();
    emplaceNoLock(std::move(tombstone));
    return tombstonePtr->getPrev();
}

void CatalogSet::renameNamespaceEntry(Transaction* transaction, const std::string& oldName,
    const std::string& newName) {
    CatalogEntry* droppedEntry = nullptr;
    CatalogEntry* createdEntry = nullptr;
    {
        std::unique_lock lck{mtx};
        const auto* entry = getEntryNoLock(transaction, oldName);
        if (entry->getType() != CatalogEntryType::NAMESPACE_ENTRY) {
            throw CatalogException("Only namespace definitions may be renamed.");
        }
        auto renamed = entry->constCast<NamespaceCatalogEntry>().copy();
        renamed->rename(newName);
        droppedEntry = dropEntryNoLock(transaction, oldName, entry->getOID());
        createdEntry = createEntryNoLock(transaction, std::move(renamed));
    }
    // As in alterTableEntry, push undo records only after releasing mtx. The drop is logged
    // before the create, so replay frees the old name before the identity takes the new one.
    if (transaction->shouldAppendToUndoBuffer()) {
        transaction->pushCreateDropCatalogEntry(*this, *droppedEntry, isInternal());
        transaction->pushCreateDropCatalogEntry(*this, *createdEntry, isInternal());
    }
}

void CatalogSet::alterTableEntry(Transaction* transaction, const binder::BoundAlterInfo& alterInfo,
    bool skipLoggingToWAL) {
    // Record the work that needs undo-buffer pushes after releasing mtx. Holding mtx while
    // acquiring the transaction's changedCatalogsMutex (inside pushAlterCatalogEntry) inverts
    // the lock order vs Transaction::publishCommit (-> graphs) vs withGraphCatalog (-> mtx)
    // and trips TSan lock-order warnings, so never push while holding mtx.
    CatalogEntry* alterOldEntry = nullptr;
    CatalogEntry* renameCreatedEntry = nullptr;
    bool isRename = false;
    auto addedRelTableOID = common::INVALID_TABLE_ID;
    {
        std::unique_lock lck{mtx};
        // LCOV_EXCL_START
        validateExistNoLock(transaction, alterInfo.tableName);
        // LCOV_EXCL_STOP
        auto entry = getEntryNoLock(transaction, alterInfo.tableName);
        DASSERT(entry->getType() == CatalogEntryType::NODE_TABLE_ENTRY ||
                entry->getType() == CatalogEntryType::REL_GROUP_ENTRY);
        const auto tableEntry = entry->ptrCast<TableCatalogEntry>();
        auto newEntry = tableEntry->alter(transaction->getID(), alterInfo, this);
        switch (alterInfo.alterType) {
        case AlterType::RENAME: {
            // We treat rename table as drop and create.
            dropEntryNoLock(transaction, alterInfo.tableName, entry->getOID());
            auto createdEntry = createEntryNoLock(transaction, std::move(newEntry));
            isRename = true;
            alterOldEntry = entry;
            renameCreatedEntry = createdEntry;
        } break;
        case AlterType::COMMENT:
        case AlterType::ADD_PROPERTY:
        case AlterType::DROP_PROPERTY:
        case AlterType::RENAME_PROPERTY:
        case AlterType::SET_SORTED_BY:
        case AlterType::ADD_FROM_TO_CONNECTION:
        case AlterType::DROP_FROM_TO_CONNECTION: {
            if (alterInfo.alterType == AlterType::ADD_FROM_TO_CONNECTION) {
                addedRelTableOID =
                    newEntry->ptrCast<RelGroupCatalogEntry>()->getRelEntryInfos().back().oid;
            }
            emplaceNoLock(std::move(newEntry));
            alterOldEntry = entry;
        } break;
        default: {
            UNREACHABLE_CODE;
        }
        }
    }
    if (transaction->shouldAppendToUndoBuffer()) {
        if (isRename) {
            transaction->pushAlterCatalogEntry(*this, *alterOldEntry, alterInfo, skipLoggingToWAL);
            transaction->pushCreateDropCatalogEntry(*this, *renameCreatedEntry, isInternal(),
                true /* skipLoggingToWAL */);
        } else {
            transaction->pushAlterCatalogEntry(*this, *alterOldEntry, alterInfo, false,
                addedRelTableOID);
        }
    }
}

CatalogEntrySet CatalogSet::getEntries(const Transaction* transaction) {
    CatalogEntrySet result;
    std::shared_lock lck{mtx};
    for (auto& [name, entry] : entries) {
        auto currentEntry = traverseVersionChainsForTransactionNoLock(transaction, entry.get());
        if (!currentEntry || currentEntry->isDeleted()) {
            continue;
        }
        result.emplace(name, currentEntry);
    }
    return result;
}

CatalogEntry* CatalogSet::getEntryOfOID(const Transaction* transaction, oid_t oid) {
    std::shared_lock lck{mtx};
    for (auto& [_, entry] : entries) {
        if (entry->getOID() != oid) {
            continue;
        }
        const auto currentEntry =
            traverseVersionChainsForTransactionNoLock(transaction, entry.get());
        if (!currentEntry || currentEntry->isDeleted()) {
            continue;
        }
        return currentEntry;
    }
    return nullptr;
}

void CatalogSet::serialize(Serializer serializer) const {
    std::vector<CatalogEntry*> entriesToSerialize;
    for (auto& [_, entry] : entries) {
        switch (entry->getType()) {
        case CatalogEntryType::SCALAR_FUNCTION_ENTRY:
        case CatalogEntryType::REWRITE_FUNCTION_ENTRY:
        case CatalogEntryType::AGGREGATE_FUNCTION_ENTRY:
        case CatalogEntryType::COPY_FUNCTION_ENTRY:
        case CatalogEntryType::TABLE_FUNCTION_ENTRY:
        case CatalogEntryType::STANDALONE_TABLE_FUNCTION_ENTRY:
        case CatalogEntryType::FOREIGN_TABLE_ENTRY:
            continue;
        default: {
            auto committedEntry = getCommittedEntryNoLock(entry.get());
            if (committedEntry && !committedEntry->isDeleted()) {
                entriesToSerialize.push_back(committedEntry);
            }
        }
        }
    }
    serializer.writeDebuggingInfo("nextOID");
    serializer.serializeValue<oid_t>(nextOID);
    serializer.writeDebuggingInfo("numEntries");
    const uint64_t numEntriesToSerialize = entriesToSerialize.size();
    serializer.serializeValue<uint64_t>(numEntriesToSerialize);
    for (const auto entry : entriesToSerialize) {
        entry->serialize(serializer);
    }
}

void CatalogSet::serializeSnapshot(Serializer serializer, const Transaction* snapshotTxn) const {
    std::shared_lock lck{mtx};
    std::vector<CatalogEntry*> entriesToSerialize;
    for (auto& [_, entry] : entries) {
        switch (entry->getType()) {
        case CatalogEntryType::SCALAR_FUNCTION_ENTRY:
        case CatalogEntryType::REWRITE_FUNCTION_ENTRY:
        case CatalogEntryType::AGGREGATE_FUNCTION_ENTRY:
        case CatalogEntryType::COPY_FUNCTION_ENTRY:
        case CatalogEntryType::TABLE_FUNCTION_ENTRY:
        case CatalogEntryType::STANDALONE_TABLE_FUNCTION_ENTRY:
        case CatalogEntryType::FOREIGN_TABLE_ENTRY:
            continue;
        default: {
            auto visibleEntry = traverseVersionChainsForTransactionNoLock(snapshotTxn, entry.get());
            if (visibleEntry && !visibleEntry->isDeleted()) {
                entriesToSerialize.push_back(visibleEntry);
            }
        }
        }
    }
    serializer.writeDebuggingInfo("nextOID");
    serializer.serializeValue<oid_t>(nextOID);
    serializer.writeDebuggingInfo("numEntries");
    const uint64_t numEntriesToSerialize = entriesToSerialize.size();
    serializer.serializeValue<uint64_t>(numEntriesToSerialize);
    for (const auto entry : entriesToSerialize) {
        entry->serialize(serializer);
    }
}

std::unique_ptr<CatalogSet> CatalogSet::deserialize(Deserializer& deserializer) {
    return deserialize(nullptr, deserializer);
}

std::unique_ptr<CatalogSet> CatalogSet::deserialize(Catalog* catalog, Deserializer& deserializer) {
    std::string debuggingInfo;
    auto catalogSet = std::make_unique<CatalogSet>();
    catalogSet->catalog = catalog;
    deserializer.validateDebuggingInfo(debuggingInfo, "nextOID");
    deserializer.deserializeValue<oid_t>(catalogSet->nextOID);
    uint64_t numEntries = 0;
    deserializer.validateDebuggingInfo(debuggingInfo, "numEntries");
    deserializer.deserializeValue<uint64_t>(numEntries);
    for (uint64_t i = 0; i < numEntries; i++) {
        auto entry = CatalogEntry::deserialize(deserializer);
        if (entry != nullptr) {
            catalogSet->emplaceNoLock(std::move(entry));
        }
    }
    return catalogSet;
}

std::string CatalogSet::getOwnerCatalogName() const {
    return catalog ? catalog->getCatalogName() : "";
}

// Ideally we should not trigger the following check. Instead, we should throw more informative
// error message at catalog level.
void CatalogSet::validateExistNoLock(const Transaction* transaction,
    const std::string& name) const {
    if (!containsEntryNoLock(transaction, name)) {
        throw CatalogException(std::format("{} does not exist in catalog.", name));
    }
}

void CatalogSet::validateNotExistNoLock(const Transaction* transaction,
    const std::string& name) const {
    if (containsEntryNoLock(transaction, name)) {
        throw CatalogException(std::format("{} already exists in catalog.", name));
    }
}

} // namespace catalog
} // namespace riag
