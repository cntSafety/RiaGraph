#include "catalog/namespace_catalog.h"

#include "common/constants.h"
#include "common/exception/catalog.h"
#include "common/mask.h"
#include "common/string_utils.h"
#include "function/comparison/simd_filter.h"
#include "main/client_context.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "storage/table/rel_table.h"
#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"

namespace riag::catalog {
namespace {

std::string namespaceKey(const std::string& name) {
    return "N/" + name;
}
std::string policyKey(common::table_id_t tableID) {
    return "T/" + std::to_string(tableID);
}
std::string groupKey(common::table_id_t tableID, uint64_t groupIndex) {
    return "M/" + std::to_string(tableID) + "/" + std::to_string(groupIndex);
}

void validateNamedNamespace(const std::string& name) {
    if (name.empty() || common::StringUtils::caseInsensitiveEquals(name, "GLOBAL")) {
        throw common::CatalogException(
            "GLOBAL is the implicit namespace and cannot be used as a named namespace.");
    }
}

void invalidateLocalNamespacePlans(transaction::Transaction* transaction) {
    if (auto* context = transaction->getClientContext(); context && !transaction->isRecovery()) {
        // Include uncommitted schema/promotion changes in this connection's binding
        // generation; the shared catalog version advances only when they commit.
        context->setNamespaceSelection(context->getNamespaceSelection(),
            context->getNamespaceSelectionOwner());
    }
}

} // namespace

NamespaceCatalog::NamespaceCatalog(Catalog* owner)
    : owner{owner}, entries{std::make_unique<CatalogSet>(owner)} {
    entries->enableNamespaceLookup();
    entries->getNextOID(); // Reserve zero for the implicit global namespace.
}

bool NamespaceCatalog::contains(const transaction::Transaction* transaction,
    const std::string& name) const {
    return entries->containsEntry(transaction, namespaceKey(name));
}

common::namespace_id_t NamespaceCatalog::resolve(const transaction::Transaction* transaction,
    const std::string& name) const {
    validateNamedNamespace(name);
    if (!contains(transaction, name)) {
        throw common::CatalogException("Namespace " + name + " does not exist.");
    }
    return entries->getEntry(transaction, namespaceKey(name))->getOID();
}

std::string NamespaceCatalog::getName(const transaction::Transaction* transaction,
    common::namespace_id_t id) const {
    auto* entry = entries->getNamespaceEntry(transaction, common::INVALID_TABLE_ID, id);
    if (entry == nullptr || entry->getType() != CatalogEntryType::NAMESPACE_ENTRY) {
        throw common::CatalogException("Namespace identity no longer exists.");
    }
    return entry->getName().substr(2);
}

common::namespace_id_t NamespaceCatalog::create(transaction::Transaction* transaction,
    const std::string& name, bool ifNotExists) {
    validateNamedNamespace(name);
    if (contains(transaction, name)) {
        if (ifNotExists) {
            return resolve(transaction, name);
        }
        throw common::CatalogException("Namespace " + name + " already exists.");
    }
    const auto id = entries->createEntry(transaction,
        std::make_unique<NamespaceCatalogEntry>(CatalogEntryType::NAMESPACE_ENTRY,
            namespaceKey(name)));
    invalidateLocalNamespacePlans(transaction);
    return id;
}

void NamespaceCatalog::drop(transaction::Transaction* transaction, const std::string& name,
    bool ifExists) {
    validateNamedNamespace(name);
    if (!contains(transaction, name)) {
        if (ifExists) {
            return;
        }
        throw common::CatalogException("Namespace " + name + " does not exist.");
    }
    const auto id = resolve(transaction, name);
    for (const auto& [_, entry] : entries->getEntries(transaction)) {
        if (entry->getType() == CatalogEntryType::NAMESPACE_POLICY_ENTRY) {
            const auto& policy = entry->constCast<NamespaceCatalogEntry>();
            if (!policy.allowed.isUnrestricted() && policy.allowed.contains(id)) {
                throw common::CatalogException(
                    "Namespace " + name + " is referenced by a table policy.");
            }
        } else if (entry->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
            const auto& group = entry->constCast<NamespaceCatalogEntry>();
            if (group.memberships && std::find(group.memberships->begin(), group.memberships->end(),
                                         id) != group.memberships->end()) {
                throw common::CatalogException(
                    "Namespace " + name + " still contains graph elements.");
            }
        }
    }
    entries->dropEntry(transaction, namespaceKey(name), id);
    invalidateLocalNamespacePlans(transaction);
}

void NamespaceCatalog::rename(transaction::Transaction* transaction, const std::string& name,
    const std::string& newName) {
    validateNamedNamespace(name);
    validateNamedNamespace(newName);
    if (!contains(transaction, name)) {
        throw common::CatalogException("Namespace " + name + " does not exist.");
    }
    if (contains(transaction, newName)) {
        throw common::CatalogException("Namespace " + newName + " already exists.");
    }
    entries->renameNamespaceEntry(transaction, namespaceKey(name), namespaceKey(newName));
    invalidateLocalNamespacePlans(transaction);
}

std::vector<NamespaceCatalogEntry*> NamespaceCatalog::getDefinitions(
    const transaction::Transaction* transaction) const {
    std::vector<NamespaceCatalogEntry*> result;
    for (const auto& [_, entry] : entries->getEntries(transaction)) {
        if (entry->getType() == CatalogEntryType::NAMESPACE_ENTRY) {
            result.push_back(entry->ptrCast<NamespaceCatalogEntry>());
        }
    }
    return result;
}

const NamespaceCatalogEntry* NamespaceCatalog::getPolicy(
    const transaction::Transaction* transaction, common::table_id_t tableID) const {
    auto* entry = entries->getNamespaceEntry(transaction, tableID);
    return entry ? entry->ptrCast<NamespaceCatalogEntry>() : nullptr;
}

void NamespaceCatalog::setPolicy(transaction::Transaction* transaction, common::table_id_t tableID,
    bool relationship, common::NamespaceSet allowed, bool localPrimaryKey, bool hasMembership) {
    auto policy = std::make_unique<NamespaceCatalogEntry>(CatalogEntryType::NAMESPACE_POLICY_ENTRY,
        policyKey(tableID));
    policy->tableID = tableID;
    policy->relationship = relationship;
    policy->allowed = std::move(allowed);
    policy->localPrimaryKey = localPrimaryKey;
    policy->hasMembership = hasMembership;
    if (getPolicy(transaction, tableID)) {
        entries->replaceNamespaceEntry(transaction, std::move(policy));
    } else {
        entries->createEntry(transaction, std::move(policy));
    }
    invalidateLocalNamespacePlans(transaction);
}

const NamespaceCatalogEntry* NamespaceCatalog::getGroup(const transaction::Transaction* transaction,
    common::table_id_t tableID, uint64_t groupIndex) const {
    auto* entry = entries->getNamespaceEntry(transaction, tableID, groupIndex);
    return entry ? entry->ptrCast<NamespaceCatalogEntry>() : nullptr;
}

NamespaceCatalogEntry* NamespaceCatalog::getWritableGroup(transaction::Transaction* transaction,
    common::table_id_t tableID, uint64_t groupIndex, bool relationship) {
    std::lock_guard lock{groupWriteMutex};
    entries->pruneNamespaceMembershipVersions(tableID, groupIndex,
        transaction::TransactionManager::Get(*transaction->getClientContext())
            ->namespaceReclaimHorizon());
    auto* policy = getPolicy(transaction, tableID);
    if (!policy || !policy->hasMembership) {
        setPolicy(transaction, tableID, relationship,
            policy ? policy->allowed : common::NamespaceSet{}, policy && policy->localPrimaryKey,
            true);
    }
    if (const auto* current = getGroup(transaction, tableID, groupIndex)) {
        if (current->getTimestamp() == transaction->getID()) {
            return const_cast<NamespaceCatalogEntry*>(current);
        }
        auto* entry = entries->replaceNamespaceEntry(transaction, current->copy(), true)
                          ->ptrCast<NamespaceCatalogEntry>();
        transaction->recordNamespaceGroup(entry);
        return entry;
    }
    auto group = std::make_unique<NamespaceCatalogEntry>(
        CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY, groupKey(tableID, groupIndex));
    group->tableID = tableID;
    group->groupIndex = groupIndex;
    group->relationship = relationship;
    group->memberships = std::make_unique<NamespaceCatalogEntry::Memberships>();
    auto* result = group.get();
    entries->createEntry(transaction, std::move(group), true);
    transaction->recordNamespaceGroup(result);
    return result;
}

void NamespaceCatalog::replayEntry(transaction::Transaction* transaction,
    std::unique_ptr<NamespaceCatalogEntry> entry) {
    if (entry->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY && entry->memberships) {
        for (auto id : *entry->memberships) {
            if (id != common::GLOBAL_NAMESPACE_ID) {
                registerGroup(entry->tableID, id, entry->groupIndex);
            }
        }
    }
    if (entry->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY && !entry->relationship &&
        entry->memberships) {
        const auto* policy = getPolicy(transaction, entry->tableID);
        if (policy && policy->localPrimaryKey) {
            auto* storage = storage::PartitionStorageRegistry::resolveOwnerStorageManager(
                transaction->getClientContext(), owner);
            auto& table = storage->getTable(entry->tableID)->cast<storage::NodeTable>();
            const auto rows = table.getNumTotalRows(transaction);
            for (uint64_t i = 0; i < NamespaceCatalogEntry::GROUP_SIZE; ++i) {
                const auto offset = entry->groupIndex * NamespaceCatalogEntry::GROUP_SIZE + i;
                if (offset < rows && table.isVisible(transaction, offset)) {
                    table.moveNamespace(transaction, offset, (*entry->memberships)[i]);
                }
            }
        }
    }
    const auto oid = entry->getOID();
    if (entry->getType() == CatalogEntryType::NAMESPACE_POLICY_ENTRY) {
        entry->rename(policyKey(entry->tableID));
    } else if (entry->getType() == CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
        entry->rename(groupKey(entry->tableID, entry->groupIndex));
    }
    if (entries->containsEntry(transaction, entry->getName())) {
        entries->replaceNamespaceEntry(transaction, std::move(entry), true);
    } else {
        entries->createEntry(transaction, std::move(entry), true, oid);
    }
}

void NamespaceCatalog::dropEntry(transaction::Transaction* transaction, common::oid_t oid) {
    auto* entry = entries->getEntryOfOID(transaction, oid);
    if (entry) {
        entries->dropEntry(transaction, entry->getName(), oid);
    }
}

void NamespaceCatalog::dropTable(transaction::Transaction* transaction,
    common::table_id_t tableID) {
    for (const auto& [_, entry] : entries->getEntries(transaction)) {
        if (entry->getType() != CatalogEntryType::NAMESPACE_ENTRY &&
            entry->constCast<NamespaceCatalogEntry>().tableID == tableID) {
            entries->dropEntry(transaction, entry->getName(), entry->getOID());
        }
    }
}

void NamespaceCatalog::validatePolicy(const transaction::Transaction* transaction,
    common::table_id_t tableID, common::row_idx_t visibleRows,
    const common::NamespaceSet& allowed) const {
    common::row_idx_t namedRows = 0;
    for (const auto& [_, entry] : entries->getEntries(transaction)) {
        if (entry->getType() != CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
            continue;
        }
        const auto& group = entry->constCast<NamespaceCatalogEntry>();
        if (group.tableID != tableID || !group.memberships) {
            continue;
        }
        for (const auto id : *group.memberships) {
            if (id == common::GLOBAL_NAMESPACE_ID) {
                continue;
            }
            ++namedRows;
            if (!allowed.contains(id)) {
                throw common::CatalogException(
                    "The new table policy excludes existing graph elements.");
            }
        }
    }
    if (!allowed.contains(common::GLOBAL_NAMESPACE_ID) && visibleRows > namedRows) {
        throw common::CatalogException("The new table policy excludes existing global elements.");
    }
}

common::offset_t NamespaceCatalog::normalizeOffset(const transaction::Transaction* transaction,
    common::table_id_t tableID, common::offset_t offset) const {
    if (offset < common::StorageConstants::MAX_NUM_ROWS_IN_TABLE) {
        return offset;
    }
    auto* storage = storage::PartitionStorageRegistry::resolveOwnerStorageManager(
        transaction->getClientContext(), owner);
    auto& table = storage->getTable(tableID)->cast<storage::RelTable>();
    return offset - common::StorageConstants::MAX_NUM_ROWS_IN_TABLE + table.reserveRelOffsets(0);
}

void NamespaceCatalog::finalizeLocalRelationshipGroup(transaction::Transaction* transaction,
    const NamespaceCatalogEntry& group) {
    const auto start = group.groupIndex * NamespaceCatalogEntry::GROUP_SIZE;
    if (!group.relationship || start < common::StorageConstants::MAX_NUM_ROWS_IN_TABLE ||
        !group.memberships) {
        return;
    }
    const auto committedStart = normalizeOffset(transaction, group.tableID, start);
    NamespaceWriteState writer(this, transaction, group.tableID, true);
    for (uint64_t i = 0; i < NamespaceCatalogEntry::GROUP_SIZE; ++i) {
        if ((*group.memberships)[i] != common::GLOBAL_NAMESPACE_ID) {
            writer.set(committedStart + i, (*group.memberships)[i]);
        }
    }
    entries->dropEntry(transaction, group.getName(), group.getOID());
}

void NamespaceCatalog::serialize(common::Serializer& serializer) const {
    entries->serialize(serializer);
}

void NamespaceCatalog::serializeSnapshot(common::Serializer& serializer,
    const transaction::Transaction* transaction) const {
    entries->serializeSnapshot(serializer, transaction);
}

void NamespaceCatalog::deserialize(common::Deserializer& deserializer) {
    entries = CatalogSet::deserialize(owner, deserializer);
    entries->enableNamespaceLookup();
    for (const auto& [_, entry] : entries->getEntries(&transaction::DUMMY_CHECKPOINT_TRANSACTION)) {
        if (entry->getType() != CatalogEntryType::NAMESPACE_MEMBERSHIP_ENTRY) {
            continue;
        }
        const auto& group = entry->constCast<NamespaceCatalogEntry>();
        if (group.memberships) {
            for (auto id : *group.memberships) {
                if (id != common::GLOBAL_NAMESPACE_ID) {
                    registerGroup(group.tableID, id, group.groupIndex);
                }
            }
        }
    }
}

void NamespaceCatalog::registerGroup(common::table_id_t tableID, common::namespace_id_t namespaceID,
    uint64_t groupIndex) {
    std::lock_guard lock{directoryMutex};
    namespaceGroups[tableID][namespaceID].insert(groupIndex);
}

std::vector<common::namespace_id_t> NamespaceCatalog::getActualNamespaces(
    const transaction::Transaction* transaction, common::table_id_t tableID) const {
    std::unordered_set<uint64_t> groups;
    {
        std::lock_guard lock{directoryMutex};
        if (const auto table = namespaceGroups.find(tableID); table != namespaceGroups.end()) {
            for (const auto& [_, indices] : table->second) {
                groups.insert(indices.begin(), indices.end());
            }
        }
    }
    std::vector<common::namespace_id_t> result{common::GLOBAL_NAMESPACE_ID};
    for (auto index : groups) {
        const auto* group = getGroup(transaction, tableID, index);
        if (group && group->memberships) {
            result.insert(result.end(), group->memberships->begin(), group->memberships->end());
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::unordered_map<common::namespace_id_t, common::row_idx_t>
NamespaceCatalog::countNamedMemberships(const transaction::Transaction* transaction,
    common::table_id_t tableID) const {
    std::unordered_set<uint64_t> groups;
    {
        std::lock_guard lock{directoryMutex};
        if (const auto table = namespaceGroups.find(tableID); table != namespaceGroups.end()) {
            for (const auto& [_, indices] : table->second) {
                groups.insert(indices.begin(), indices.end());
            }
        }
    }
    std::unordered_map<common::namespace_id_t, common::row_idx_t> counts;
    for (auto index : groups) {
        const auto* group = getGroup(transaction, tableID, index);
        if (!group || !group->memberships) {
            continue;
        }
        for (auto id : *group->memberships) {
            if (id != common::GLOBAL_NAMESPACE_ID) {
                ++counts[id];
            }
        }
    }
    return counts;
}

std::vector<common::offset_t> NamespaceCatalog::collectOffsets(
    const transaction::Transaction* transaction, common::table_id_t tableID,
    const common::NamespaceSet& scope, common::offset_t maxOffset) const {
    std::vector<common::offset_t> result;
    forEachMatchedRange(transaction, tableID, scope, maxOffset, [&](auto begin, auto end) {
        for (auto offset = begin; offset < end; ++offset) {
            result.push_back(offset);
        }
    });
    return result;
}

void NamespaceCatalog::populateMask(const transaction::Transaction* transaction,
    common::table_id_t tableID, const common::NamespaceSet& scope, common::offset_t maxOffset,
    common::SemiMask& mask) const {
    forEachMatchedRange(transaction, tableID, scope, maxOffset, [&](auto begin, auto end) {
        if (end == begin + 1) {
            mask.mask(begin);
        } else {
            mask.maskRange(begin, end);
        }
    });
}

common::row_idx_t NamespaceCatalog::countMembers(const transaction::Transaction* transaction,
    common::table_id_t tableID, const common::NamespaceSet& scope) const {
    common::row_idx_t count = 0;
    forEachMatchedRange(transaction, tableID, scope, common::INVALID_OFFSET,
        [&](auto begin, auto end) { count += end - begin; });
    return count;
}

void NamespaceCatalog::forEachMatchedRange(const transaction::Transaction* transaction,
    common::table_id_t tableID, const common::NamespaceSet& scope, common::offset_t maxOffset,
    const std::function<void(common::offset_t, common::offset_t)>& callback) const {
    std::vector<uint64_t> groups;
    const auto includesGlobal = scope.contains(common::GLOBAL_NAMESPACE_ID);
    if (includesGlobal) {
        // Global gaps need a finite table bound; cardinality estimation of
        // GLOBAL uses visible table rows minus named counts instead.
        if (maxOffset == common::INVALID_OFFSET) {
            throw common::CatalogException("Global namespace ranges require a finite table bound.");
        }
        groups = entries->getNamespaceGroupIndices(tableID);
    } else {
        std::lock_guard lock{directoryMutex};
        const auto table = namespaceGroups.find(tableID);
        if (table != namespaceGroups.end()) {
            for (auto id : scope.getIDs()) {
                if (const auto item = table->second.find(id); item != table->second.end()) {
                    groups.insert(groups.end(), item->second.begin(), item->second.end());
                }
            }
        }
    }
    std::sort(groups.begin(), groups.end());
    groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
    const auto singleton = scope.getSingleton();
    std::array<common::sel_t, NamespaceCatalogEntry::GROUP_SIZE> positions;
    common::offset_t cursor = 0;
    for (auto groupIndex : groups) {
        const auto start = groupIndex * NamespaceCatalogEntry::GROUP_SIZE;
        if (start >= maxOffset) {
            break;
        }
        if (includesGlobal && start > cursor) {
            callback(cursor, start);
        }
        const auto count = std::min(NamespaceCatalogEntry::GROUP_SIZE, maxOffset - start);
        cursor = start + count;
        const auto* group = getGroup(transaction, tableID, groupIndex);
        if (!group || !group->memberships) {
            if (includesGlobal) {
                callback(start, cursor);
            }
            continue;
        }
        if (singleton && group->getTimestamp() < transaction::Transaction::START_TRANSACTION_ID) {
            const auto& cached = group->getMemberOffsets(*singleton);
            for (size_t i = 0; i < cached.size() && cached[i] < count;) {
                const auto begin = cached[i++];
                auto end = uint64_t{begin} + 1;
                while (i < cached.size() && cached[i] == end && cached[i] < count) {
                    ++i;
                    ++end;
                }
                callback(start + begin, start + end);
            }
            continue;
        }
        if (singleton) {
            const auto selected = function::simd::selectConstant(group->memberships->data(),
                static_cast<common::sel_t>(count), &*singleton, nullptr, positions.data(),
                common::PhysicalTypeID::UINT64, function::ComparisonOperation::EQUAL);
            for (common::sel_t i = 0; i < selected;) {
                const auto begin = positions[i++];
                auto end = begin + 1;
                while (i < selected && positions[i] == end) {
                    ++i;
                    ++end;
                }
                callback(start + begin, start + end);
            }
            continue;
        }
        for (uint64_t i = 0; i < count;) {
            if (scope.contains((*group->memberships)[i])) {
                const auto begin = i++;
                while (i < count && scope.contains((*group->memberships)[i])) {
                    ++i;
                }
                callback(start + begin, start + i);
            } else {
                ++i;
            }
        }
    }
    if (includesGlobal && cursor < maxOffset) {
        callback(cursor, maxOffset);
    }
}

common::namespace_id_t NamespaceReadState::get(common::offset_t offset) {
    if (!catalog) {
        return common::GLOBAL_NAMESPACE_ID;
    }
    if (transaction->isWriteTransaction()) {
        const auto generation = transaction->getNamespaceGeneration();
        if (generation != cachedGeneration) {
            groups.clear();
            currentGroupIndex = common::INVALID_ROW_IDX;
            currentGroup = nullptr;
            cachedGeneration = generation;
        }
    }
    const auto groupIndex = offset / NamespaceCatalogEntry::GROUP_SIZE;
    if (groupIndex != currentGroupIndex) {
        if (currentGroupIndex == common::INVALID_ROW_IDX) {
            // A point probe or a single-group scan needs no hash-cache allocation.
            currentGroup = catalog->getGroup(transaction, tableID, groupIndex);
        } else {
            groups.try_emplace(currentGroupIndex, currentGroup);
            auto [it, inserted] = groups.try_emplace(groupIndex, nullptr);
            if (inserted) {
                it->second = catalog->getGroup(transaction, tableID, groupIndex);
            }
            currentGroup = it->second;
        }
        currentGroupIndex = groupIndex;
    }
    return currentGroup && currentGroup->memberships ?
               (*currentGroup->memberships)[offset % NamespaceCatalogEntry::GROUP_SIZE] :
               common::GLOBAL_NAMESPACE_ID;
}

void NamespaceWriteState::set(common::offset_t offset, common::namespace_id_t id) {
    if (id == common::GLOBAL_NAMESPACE_ID && groups.empty()) {
        const auto generation = transaction->getNamespaceGeneration();
        if (generation != globalCheckGeneration) {
            const auto* policy = catalog->getPolicy(transaction, tableID);
            mayHaveMembership = policy && policy->hasMembership;
            globalCheckGeneration = generation;
        }
        if (!mayHaveMembership) {
            return;
        }
    }
    const auto groupIndex = offset / NamespaceCatalogEntry::GROUP_SIZE;
    auto [it, inserted] = groups.try_emplace(groupIndex, nullptr);
    if (!it->second) {
        const auto* current = catalog->getGroup(transaction, tableID, groupIndex);
        const auto currentID =
            current && current->memberships ?
                (*current->memberships)[offset % NamespaceCatalogEntry::GROUP_SIZE] :
                common::GLOBAL_NAMESPACE_ID;
        if (currentID == id) {
            return;
        }
        it->second = catalog->getWritableGroup(transaction, tableID, groupIndex, relationship);
    }
    auto& value = (*it->second->memberships)[offset % NamespaceCatalogEntry::GROUP_SIZE];
    if (value != id) {
        if (id != common::GLOBAL_NAMESPACE_ID && registeredGroup != std::pair{groupIndex, id}) {
            catalog->registerGroup(tableID, id, groupIndex);
            registeredGroup = std::pair{groupIndex, id};
        }
        value = id;
        transaction->incrementNamespaceGeneration();
    }
}

void NamespaceWriteState::setRange(common::offset_t offset, common::offset_t count,
    common::namespace_id_t id) {
    while (count != 0) {
        const auto groupIndex = offset / NamespaceCatalogEntry::GROUP_SIZE;
        const auto inGroup = offset % NamespaceCatalogEntry::GROUP_SIZE;
        const auto length = std::min(count, NamespaceCatalogEntry::GROUP_SIZE - inGroup);
        auto& group = groups[groupIndex];
        if (!group) {
            group = catalog->getWritableGroup(transaction, tableID, groupIndex, relationship);
        }
        std::fill_n(group->memberships->begin() + inGroup, length, id);
        if (id != common::GLOBAL_NAMESPACE_ID && registeredGroup != std::pair{groupIndex, id}) {
            catalog->registerGroup(tableID, id, groupIndex);
            registeredGroup = std::pair{groupIndex, id};
        }
        offset += length;
        count -= length;
    }
    transaction->incrementNamespaceGeneration();
}

} // namespace riag::catalog
