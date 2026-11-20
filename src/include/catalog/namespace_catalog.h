#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "catalog_entry/namespace_catalog_entry.h"
#include "catalog_set.h"

namespace riag::common {
class SemiMask;
}

namespace riag::catalog {

class NamespaceCatalog {
public:
    explicit NamespaceCatalog(Catalog* owner);

    // Reserve format 50 once an identity has been allocated, including tombstones.
    // Merely constructing the lazy catalog for a no-op must preserve format 49.
    bool hasAllocatedEntries() const { return entries->peekNextOID() > 1; }

    bool contains(const transaction::Transaction* transaction, const std::string& name) const;
    common::namespace_id_t resolve(const transaction::Transaction* transaction,
        const std::string& name) const;
    std::string getName(const transaction::Transaction* transaction,
        common::namespace_id_t id) const;
    common::namespace_id_t create(transaction::Transaction* transaction, const std::string& name,
        bool ifNotExists);
    void drop(transaction::Transaction* transaction, const std::string& name, bool ifExists);
    // Changes only the definition's name. The ID, and everything keyed by it (memberships,
    // table policies, LOCAL key domains, session selections), stays as it is.
    void rename(transaction::Transaction* transaction, const std::string& name,
        const std::string& newName);
    std::vector<NamespaceCatalogEntry*> getDefinitions(
        const transaction::Transaction* transaction) const;

    const NamespaceCatalogEntry* getPolicy(const transaction::Transaction* transaction,
        common::table_id_t tableID) const;
    void setPolicy(transaction::Transaction* transaction, common::table_id_t tableID,
        bool relationship, common::NamespaceSet allowed, bool localPrimaryKey, bool hasMembership);
    const NamespaceCatalogEntry* getGroup(const transaction::Transaction* transaction,
        common::table_id_t tableID, uint64_t groupIndex) const;
    NamespaceCatalogEntry* getWritableGroup(transaction::Transaction* transaction,
        common::table_id_t tableID, uint64_t groupIndex, bool relationship);
    void replayEntry(transaction::Transaction* transaction,
        std::unique_ptr<NamespaceCatalogEntry> entry);
    void dropEntry(transaction::Transaction* transaction, common::oid_t oid);
    void dropTable(transaction::Transaction* transaction, common::table_id_t tableID);
    void finalizeLocalRelationshipGroup(transaction::Transaction* transaction,
        const NamespaceCatalogEntry& group);
    void registerGroup(common::table_id_t tableID, common::namespace_id_t namespaceID,
        uint64_t groupIndex);
    std::vector<common::offset_t> collectOffsets(const transaction::Transaction* transaction,
        common::table_id_t tableID, const common::NamespaceSet& scope,
        common::offset_t maxOffset) const;
    void populateMask(const transaction::Transaction* transaction, common::table_id_t tableID,
        const common::NamespaceSet& scope, common::offset_t maxOffset,
        common::SemiMask& mask) const;
    common::row_idx_t countMembers(const transaction::Transaction* transaction,
        common::table_id_t tableID, const common::NamespaceSet& scope) const;
    std::vector<common::namespace_id_t> getActualNamespaces(
        const transaction::Transaction* transaction, common::table_id_t tableID) const;
    std::unordered_map<common::namespace_id_t, common::row_idx_t> countNamedMemberships(
        const transaction::Transaction* transaction, common::table_id_t tableID) const;
    void validatePolicy(const transaction::Transaction* transaction, common::table_id_t tableID,
        common::row_idx_t visibleRows, const common::NamespaceSet& allowed) const;
    common::offset_t normalizeOffset(const transaction::Transaction* transaction,
        common::table_id_t tableID, common::offset_t offset) const;

    void serialize(common::Serializer& serializer) const;
    void serializeSnapshot(common::Serializer& serializer,
        const transaction::Transaction* transaction) const;
    void deserialize(common::Deserializer& deserializer);

private:
    void forEachMatchedRange(const transaction::Transaction* transaction,
        common::table_id_t tableID, const common::NamespaceSet& scope, common::offset_t maxOffset,
        const std::function<void(common::offset_t, common::offset_t)>& callback) const;
    Catalog* owner;
    std::unique_ptr<CatalogSet> entries;
    std::mutex groupWriteMutex;
    mutable std::mutex directoryMutex;
    std::unordered_map<common::table_id_t,
        std::unordered_map<common::namespace_id_t, std::unordered_set<uint64_t>>>
        namespaceGroups;
};

// Owned by a namespace-aware operator. The group cache is numeric and scoped to
// one execution snapshot; no catalog name lookup occurs for each row.
class NamespaceReadState {
public:
    NamespaceReadState(const NamespaceCatalog* catalog, const transaction::Transaction* transaction,
        common::table_id_t tableID)
        : catalog{catalog}, transaction{transaction}, tableID{tableID} {}

    common::namespace_id_t get(common::offset_t offset);

private:
    const NamespaceCatalog* catalog;
    const transaction::Transaction* transaction;
    common::table_id_t tableID;
    std::unordered_map<uint64_t, const NamespaceCatalogEntry*> groups;
    uint64_t cachedGeneration = 0;
    uint64_t currentGroupIndex = common::INVALID_ROW_IDX;
    const NamespaceCatalogEntry* currentGroup = nullptr;
};

class NamespaceWriteState {
public:
    NamespaceWriteState(NamespaceCatalog* catalog, transaction::Transaction* transaction,
        common::table_id_t tableID, bool relationship)
        : catalog{catalog}, transaction{transaction}, tableID{tableID}, relationship{relationship} {
    }

    void set(common::offset_t offset, common::namespace_id_t id);
    void setRange(common::offset_t offset, common::offset_t count, common::namespace_id_t id);

private:
    NamespaceCatalog* catalog;
    transaction::Transaction* transaction;
    common::table_id_t tableID;
    bool relationship;
    std::unordered_map<uint64_t, NamespaceCatalogEntry*> groups;
    std::optional<std::pair<uint64_t, common::namespace_id_t>> registeredGroup;
    uint64_t globalCheckGeneration = common::INVALID_TRANSACTION;
    bool mayHaveMembership = true;
};

} // namespace riag::catalog
