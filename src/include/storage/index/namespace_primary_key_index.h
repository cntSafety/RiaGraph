#pragma once

#include <map>
#include <shared_mutex>

#include "catalog/namespace_catalog.h"
#include "storage/index/hash_index.h"

namespace riag::storage {

// LOCAL keys keep the native scalar key representation. A numeric directory selects
// a normal hash index before hashing/probing the key; equal keys in other namespaces
// neither collide nor require namespace-name comparisons.
class NamespacePrimaryKeyIndex final : public Index {
public:
    NamespacePrimaryKeyIndex(IndexInfo info, StorageManager* storage, MemoryManager* memory);
    static IndexType getIndexType();
    static std::unique_ptr<Index> load(main::ClientContext* context, StorageManager* storage,
        IndexInfo info, std::span<uint8_t> buffer);

    bool lookupNamespace(const transaction::Transaction* transaction, common::ValueVector* key,
        uint64_t position, common::namespace_id_t namespaceID, common::offset_t& result,
        visible_func visible);
    bool lookupNamespaces(const transaction::Transaction* transaction, common::ValueVector* key,
        uint64_t position, const common::NamespaceSet& scope,
        std::vector<common::offset_t>& results, visible_func visible);
    bool lookupPrimaryKey(const transaction::Transaction* transaction, common::ValueVector* key,
        uint64_t position, common::offset_t& result, visible_func visible) override;
    bool lookupAll(const transaction::Transaction* transaction, common::ValueVector* key,
        uint64_t position, std::vector<common::offset_t>& results, visible_func visible) override;
    void insertNamespace(transaction::Transaction* transaction, common::ValueVector* key,
        uint64_t position, common::offset_t offset, common::namespace_id_t namespaceID,
        visible_func visible);
    void discardAtOffset(const transaction::Transaction* transaction, common::ValueVector* key,
        common::offset_t offset);

    std::unique_ptr<Index::InsertState> initInsertState(main::ClientContext* context,
        visible_func visible) override;
    bool needCommitInsert() const override { return true; }
    void commitInsert(transaction::Transaction* transaction, const common::ValueVector& ids,
        const std::vector<common::ValueVector*>& properties, Index::InsertState& state) override;
    std::unique_ptr<Index::DeleteState> initDeleteState(const transaction::Transaction*,
        MemoryManager*, visible_func) override;
    void delete_(transaction::Transaction*, const common::ValueVector&,
        Index::DeleteState&) override {}
    void checkpointInMemory() override;
    void checkpoint(main::ClientContext*, PageAllocator&, ShadowFile&) override;
    void rollbackCheckpoint() override;
    void finalize(main::ClientContext*) override;
    void reclaimStorage(PageAllocator&) const override;
    std::vector<IndexStorageEntry> getStorageEntries() const override;

private:
    struct InsertState final : Index::InsertState {
        visible_func visible;
        explicit InsertState(visible_func visible) : visible{std::move(visible)} {}
    };
    using Directory = std::map<common::namespace_id_t, std::shared_ptr<PrimaryKeyIndex>>;
    Directory snapshot() const;
    std::shared_ptr<PrimaryKeyIndex> getOrCreate(common::namespace_id_t namespaceID);
    catalog::NamespaceReadState reader(const transaction::Transaction* transaction) const;

    StorageManager* storage;
    MemoryManager* memory;
    mutable std::shared_mutex directoryMutex;
    Directory directory;
    Directory checkpointed;
    std::shared_ptr<common::BufferWriter> previousStorageInfo;
};

} // namespace riag::storage
