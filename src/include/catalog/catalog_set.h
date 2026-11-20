#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "catalog_entry/catalog_entry.h"
#include "common/case_insensitive_map.h"

namespace riag {
namespace binder {
struct BoundAlterInfo;
} // namespace binder

namespace storage {
class UndoBuffer;
} // namespace storage

namespace transaction {
class Transaction;
} // namespace transaction

using CatalogEntrySet = common::case_insensitive_map_t<catalog::CatalogEntry*>;

namespace catalog {
class Catalog;

class RIAG_API CatalogSet {
    friend class storage::UndoBuffer;

public:
    CatalogSet() = default;
    explicit CatalogSet(bool isInternal);
    explicit CatalogSet(Catalog* catalog, bool isInternal = false);
    bool containsEntry(const transaction::Transaction* transaction, const std::string& name);
    CatalogEntry* getEntry(const transaction::Transaction* transaction, const std::string& name);
    common::oid_t createEntry(transaction::Transaction* transaction,
        std::unique_ptr<CatalogEntry> entry, bool skipLoggingToWAL = false,
        std::optional<common::oid_t> restoredOID = std::nullopt);
    CatalogEntry* replaceNamespaceEntry(transaction::Transaction* transaction,
        std::unique_ptr<CatalogEntry> entry, bool skipLoggingToWAL = false);
    // Renames a namespace definition in place of its identity: the entry under oldName is
    // dropped and an entry with the same OID is created under newName, logged to the WAL as
    // that drop and create. Nothing keyed by the OID changes.
    void renameNamespaceEntry(transaction::Transaction* transaction, const std::string& oldName,
        const std::string& newName);
    void dropEntry(transaction::Transaction* transaction, const std::string& name,
        common::oid_t oid);

    void alterTableEntry(transaction::Transaction* transaction,
        const binder::BoundAlterInfo& alterInfo, bool skipLoggingToWAL = false);

    CatalogEntrySet getEntries(const transaction::Transaction* transaction);
    CatalogEntry* getEntryOfOID(const transaction::Transaction* transaction, common::oid_t oid);

    // Optional numeric access to namespace policy/membership MVCC chains. Other
    // catalog sets retain their existing lookup path and allocate no directory.
    void enableNamespaceLookup();
    CatalogEntry* getNamespaceEntry(const transaction::Transaction* transaction,
        common::table_id_t tableID, uint64_t groupIndex = common::INVALID_ROW_IDX);
    std::vector<uint64_t> getNamespaceGroupIndices(common::table_id_t tableID) const;
    void pruneNamespaceMembershipVersions(common::table_id_t tableID, uint64_t groupIndex,
        common::transaction_t horizon);

    void serialize(common::Serializer serializer) const;
    void serializeSnapshot(common::Serializer serializer,
        const transaction::Transaction* snapshotTxn) const;
    static std::unique_ptr<CatalogSet> deserialize(common::Deserializer& deserializer);
    static std::unique_ptr<CatalogSet> deserialize(Catalog* catalog,
        common::Deserializer& deserializer);

    Catalog* getCatalog() const { return catalog; }
    std::string getOwnerCatalogName() const;

    common::oid_t getNextOID() {
        std::unique_lock lck{mtx};
        return nextOID++;
    }

    common::oid_t getNextOIDNoLock() { return nextOID++; }

    common::oid_t peekNextOID() const {
        std::shared_lock lck{mtx};
        return nextOID;
    }

private:
    bool containsEntryNoLock(const transaction::Transaction* transaction,
        const std::string& name) const;
    CatalogEntry* getEntryNoLock(const transaction::Transaction* transaction,
        const std::string& name) const;
    CatalogEntry* createEntryNoLock(const transaction::Transaction* transaction,
        std::unique_ptr<CatalogEntry> entry);
    CatalogEntry* dropEntryNoLock(const transaction::Transaction* transaction,
        const std::string& name, common::oid_t oid);

    void validateExistNoLock(const transaction::Transaction* transaction,
        const std::string& name) const;
    void validateNotExistNoLock(const transaction::Transaction* transaction,
        const std::string& name) const;

    void emplaceNoLock(std::unique_ptr<CatalogEntry> entry);
    void eraseNoLock(const std::string& name);
    void indexNamespaceEntryNoLock(CatalogEntry* entry);

    static std::unique_ptr<CatalogEntry> createDummyEntryNoLock(std::string name,
        common::oid_t oid);

    static CatalogEntry* traverseVersionChainsForTransactionNoLock(
        const transaction::Transaction* transaction, CatalogEntry* currentEntry);
    static CatalogEntry* getCommittedEntryNoLock(CatalogEntry* entry);
    bool isInternal() const { return nextOID >= INTERNAL_CATALOG_SET_START_OID; }

public:
    // To ensure the uniqueness of the OID and avoid conflict with user tables/sequence, we make the
    // start OID of the internal catalog set to be 2^63.
    static constexpr common::oid_t INTERNAL_CATALOG_SET_START_OID = 1LL << 63;

private:
    struct NamespaceLookup {
        // Identity keys survive rollback to the original dummy/tombstone entry.
        std::unordered_map<common::oid_t, std::pair<common::table_id_t, uint64_t>> identities;
        std::unordered_map<common::table_id_t, std::unordered_map<uint64_t, CatalogEntry*>> heads;
        // Earlier names of renamed namespace definitions. A rename moves an identity to the
        // version chain of its new name, so a snapshot older than the rename finds the
        // identity's visible definition through one of these names. Empty unless a namespace
        // was renamed since the catalog was loaded.
        std::unordered_map<common::oid_t, std::vector<std::string>> previousNames;
    };
    mutable std::shared_mutex mtx;
    Catalog* catalog = nullptr;
    common::oid_t nextOID = 0;
    common::case_insensitive_map_t<std::unique_ptr<CatalogEntry>> entries;
    std::unique_ptr<NamespaceLookup> namespaceLookup;
};

} // namespace catalog
} // namespace riag
