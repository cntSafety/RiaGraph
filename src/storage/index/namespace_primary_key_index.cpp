#include "storage/index/namespace_primary_key_index.h"

#include "catalog/catalog.h"
#include "common/exception/runtime.h"
#include "common/serializer/buffer_reader.h"
#include "main/client_context.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"
#include "transaction/transaction.h"

using namespace riag::common;

namespace riag::storage {
namespace {
struct DirectoryStorageInfo final : IndexStorageInfo {
    std::shared_ptr<BufferWriter> buffer = std::make_shared<BufferWriter>();
    DirectoryStorageInfo() { Serializer(buffer).write<uint64_t>(0); }
    std::shared_ptr<BufferWriter> serialize() const override {
        auto result = std::make_shared<BufferWriter>();
        result->write(buffer->getBlobData(), buffer->getSize());
        return result;
    }
};
struct EmptyDeleteState final : Index::DeleteState {};
} // namespace

NamespacePrimaryKeyIndex::NamespacePrimaryKeyIndex(IndexInfo info, StorageManager* storage,
    MemoryManager* memory)
    : Index{std::move(info), std::make_unique<DirectoryStorageInfo>()}, storage{storage},
      memory{memory} {}

IndexType NamespacePrimaryKeyIndex::getIndexType() {
    return {"NAMESPACE_HASH", IndexConstraintType::PRIMARY, IndexDefinitionType::BUILTIN, load};
}

NamespacePrimaryKeyIndex::Directory NamespacePrimaryKeyIndex::snapshot() const {
    std::shared_lock lock{directoryMutex};
    return directory;
}

std::shared_ptr<PrimaryKeyIndex> NamespacePrimaryKeyIndex::getOrCreate(namespace_id_t namespaceID) {
    {
        std::shared_lock lock{directoryMutex};
        if (const auto child = directory.find(namespaceID);
            child != directory.end() && child->second) {
            return child->second;
        }
    }
    std::unique_lock lock{directoryMutex};
    auto& child = directory[namespaceID];
    if (!child) {
        auto info = indexInfo;
        info.indexType = PrimaryKeyIndex::getIndexType().typeName;
        child = PrimaryKeyIndex::createNewIndex(std::move(info), storage->isInMemory(), *memory,
            *storage->getDataFH()->getPageManager(), &storage->getShadowFile());
    }
    return child;
}

catalog::NamespaceReadState NamespacePrimaryKeyIndex::reader(
    const transaction::Transaction* transaction) const {
    auto* table = storage->getTable(indexInfo.tableID);
    return {table->getOwningCatalog()->getNamespaces(), transaction, indexInfo.tableID};
}

bool NamespacePrimaryKeyIndex::lookupNamespace(const transaction::Transaction* transaction,
    ValueVector* key, uint64_t position, namespace_id_t namespaceID, offset_t& result,
    visible_func visible) {
    std::shared_ptr<PrimaryKeyIndex> child;
    {
        std::shared_lock lock{directoryMutex};
        auto it = directory.find(namespaceID);
        if (it == directory.end()) {
            return false;
        }
        child = it->second;
    }
    auto memberships = reader(transaction);
    // The native index copies its visibility callback. Capture one stack context
    // pointer so those copies fit std::function's inline storage on point probes.
    struct {
        visible_func& visible;
        catalog::NamespaceReadState& memberships;
        namespace_id_t namespaceID;
    } context{visible, memberships, namespaceID};
    return child->lookup(transaction, key, position, result, [context = &context](offset_t offset) {
        return context->visible(offset) && context->memberships.get(offset) == context->namespaceID;
    });
}

bool NamespacePrimaryKeyIndex::lookupNamespaces(const transaction::Transaction* transaction,
    ValueVector* key, uint64_t position, const NamespaceSet& scope, std::vector<offset_t>& results,
    visible_func visible) {
    if (!scope.isUnrestricted()) {
        for (auto id : scope.getIDs()) {
            offset_t offset;
            if (lookupNamespace(transaction, key, position, id, offset, visible)) {
                results.push_back(offset);
            }
        }
    } else {
        for (const auto& [id, _] : snapshot()) {
            offset_t offset;
            if (lookupNamespace(transaction, key, position, id, offset, visible)) {
                results.push_back(offset);
            }
        }
    }
    return !results.empty();
}

bool NamespacePrimaryKeyIndex::lookupPrimaryKey(const transaction::Transaction* transaction,
    ValueVector* key, uint64_t position, offset_t& result, visible_func visible) {
    std::vector<offset_t> results;
    lookupNamespaces(transaction, key, position, NamespaceSet{}, results, std::move(visible));
    if (results.size() > 1) {
        throw RuntimeException("A LOCAL primary key is ambiguous without a namespace selector.");
    }
    if (results.empty()) {
        return false;
    }
    result = results.front();
    return true;
}

bool NamespacePrimaryKeyIndex::lookupAll(const transaction::Transaction* transaction,
    ValueVector* key, uint64_t position, std::vector<offset_t>& results, visible_func visible) {
    return lookupNamespaces(transaction, key, position, NamespaceSet{}, results,
        std::move(visible));
}

void NamespacePrimaryKeyIndex::insertNamespace(transaction::Transaction* transaction,
    ValueVector* key, uint64_t position, offset_t offset, namespace_id_t namespaceID,
    visible_func visible) {
    auto memberships = reader(transaction);
    if (!getOrCreate(namespaceID)
             ->insert(transaction, key, position, offset, [&](offset_t existing) {
                 return visible(existing) && memberships.get(existing) == namespaceID;
             })) {
        throw RuntimeException("Duplicate LOCAL primary key in the destination namespace.");
    }
}

std::unique_ptr<Index::InsertState> NamespacePrimaryKeyIndex::initInsertState(main::ClientContext*,
    visible_func visible) {
    return std::make_unique<InsertState>(std::move(visible));
}

void NamespacePrimaryKeyIndex::discardAtOffset(const transaction::Transaction* transaction,
    ValueVector* key, offset_t offset) {
    // Membership undo can precede bulk-index cleanup. Locate the insertion by
    // numeric row offset, so a reverted membership cannot leave a stale key or
    // remove an equal key belonging to another namespace.
    const auto position = key->state->getSelVector()[0];
    for (const auto& [_, child] : snapshot()) {
        offset_t stored;
        if (child->lookup(transaction, key, position, stored,
                [&](offset_t candidate) { return candidate == offset; })) {
            child->discardPrimaryKey(key, offset - position);
        }
    }
}

void NamespacePrimaryKeyIndex::commitInsert(transaction::Transaction* transaction,
    const ValueVector& ids, const std::vector<ValueVector*>& properties,
    Index::InsertState& state) {
    auto memberships = reader(transaction);
    std::optional<namespace_id_t> currentNamespace;
    std::shared_ptr<PrimaryKeyIndex> child;
    const auto& visible = state.cast<InsertState>().visible;
    for (sel_t i = 0; i < ids.state->getSelSize(); ++i) {
        const auto offset = ids.readNodeOffset(ids.state->getSelVector()[i]);
        const auto position = properties[0]->state->getSelVector()[i];
        const auto namespaceID = memberships.get(offset);
        if (currentNamespace != namespaceID) {
            child = getOrCreate(namespaceID);
            currentNamespace = namespaceID;
        }
        if (!child->insert(transaction, properties[0], position, offset, [&](offset_t existing) {
                return visible(existing) && memberships.get(existing) == namespaceID;
            })) {
            throw RuntimeException("Duplicate LOCAL primary key in the destination namespace.");
        }
    }
}

std::unique_ptr<Index::DeleteState> NamespacePrimaryKeyIndex::initDeleteState(
    const transaction::Transaction*, MemoryManager*, visible_func) {
    return std::make_unique<EmptyDeleteState>();
}

void NamespacePrimaryKeyIndex::checkpointInMemory() {
    for (const auto& [_, child] : snapshot()) {
        child->checkpointInMemory();
    }
}

void NamespacePrimaryKeyIndex::checkpoint(main::ClientContext* context, PageAllocator& allocator,
    ShadowFile& shadow) {
    checkpointed = snapshot();
    auto& info = storageInfo->cast<DirectoryStorageInfo>();
    previousStorageInfo = info.buffer;
    auto buffer = std::make_shared<BufferWriter>();
    Serializer serializer(buffer);
    serializer.write<uint64_t>(checkpointed.size());
    for (const auto& [id, child] : checkpointed) {
        child->checkpoint(context, allocator, shadow);
        serializer.write<namespace_id_t>(id);
        auto childInfo = child->getStorageInfo().serialize();
        serializer.write<uint64_t>(childInfo->getSize());
        serializer.write(childInfo->getBlobData(), childInfo->getSize());
    }
    info.buffer = std::move(buffer);
}

void NamespacePrimaryKeyIndex::rollbackCheckpoint() {
    for (const auto& [_, child] : checkpointed) {
        child->rollbackCheckpoint();
    }
    if (previousStorageInfo) {
        storageInfo->cast<DirectoryStorageInfo>().buffer = std::move(previousStorageInfo);
    }
    checkpointed.clear();
}

void NamespacePrimaryKeyIndex::finalize(main::ClientContext* context) {
    for (const auto& [_, child] : checkpointed) {
        child->finalize(context);
    }
    checkpointed.clear();
    previousStorageInfo.reset();
}

void NamespacePrimaryKeyIndex::reclaimStorage(PageAllocator& allocator) const {
    for (const auto& [_, child] : snapshot()) {
        child->reclaimStorage(allocator);
    }
}

std::vector<IndexStorageEntry> NamespacePrimaryKeyIndex::getStorageEntries() const {
    std::vector<IndexStorageEntry> result;
    for (const auto& [id, child] : snapshot()) {
        for (auto entry : child->getStorageEntries()) {
            entry.component = std::to_string(id) + "/" + entry.component;
            result.push_back(std::move(entry));
        }
    }
    return result;
}

std::unique_ptr<Index> NamespacePrimaryKeyIndex::load(main::ClientContext* context,
    StorageManager* storage, IndexInfo info, std::span<uint8_t> buffer) {
    if (buffer.size() < sizeof(uint64_t)) {
        throw RuntimeException("Truncated LOCAL primary-key directory.");
    }
    auto result = std::make_unique<NamespacePrimaryKeyIndex>(std::move(info), storage,
        MemoryManager::Get(*context));
    auto& stored = result->storageInfo->cast<DirectoryStorageInfo>();
    stored.buffer = std::make_shared<BufferWriter>();
    stored.buffer->write(buffer.data(), buffer.size());
    Deserializer deserializer(std::make_unique<BufferReader>(buffer.data(), buffer.size()));
    deserializer.beginReadLimit(buffer.size());
    uint64_t count;
    deserializer.deserializeValue(count);
    if (count > buffer.size() / (sizeof(namespace_id_t) + sizeof(uint64_t))) {
        throw RuntimeException("Corrupted LOCAL primary-key directory.");
    }
    for (uint64_t i = 0; i < count; ++i) {
        namespace_id_t id;
        uint64_t size;
        deserializer.deserializeValue(id);
        deserializer.deserializeValue(size);
        if (size != 2 * sizeof(page_idx_t) || size > buffer.size() - deserializer.getReadOffset() ||
            result->directory.contains(id)) {
            throw RuntimeException("Corrupted LOCAL primary-key directory entry.");
        }
        std::vector<uint8_t> childBuffer(size);
        deserializer.read(childBuffer.data(), size);
        auto childInfo = result->indexInfo;
        childInfo.indexType = PrimaryKeyIndex::getIndexType().typeName;
        auto child = PrimaryKeyIndex::load(context, storage, std::move(childInfo), childBuffer);
        result->directory.emplace(id,
            std::shared_ptr<PrimaryKeyIndex>(static_cast<PrimaryKeyIndex*>(child.release())));
    }
    if (!deserializer.finished()) {
        throw RuntimeException("Trailing data in LOCAL primary-key directory.");
    }
    return result;
}

} // namespace riag::storage
