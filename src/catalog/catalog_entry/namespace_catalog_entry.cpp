#include "catalog/catalog_entry/namespace_catalog_entry.h"

#include "common/exception/runtime.h"
#include "common/serializer/deserializer.h"
#include "function/comparison/simd_filter.h"

namespace riag::catalog {

const std::vector<uint16_t>& NamespaceCatalogEntry::getMemberOffsets(
    common::namespace_id_t id) const {
    std::lock_guard lock{offsetCacheMutex};
    if (!offsetCache) {
        offsetCache = std::make_unique<decltype(offsetCache)::element_type>();
    }
    if (const auto found = offsetCache->find(id); found != offsetCache->end()) {
        return found->second;
    }
    std::vector<uint16_t> result;
    if (memberships) {
        std::array<common::sel_t, GROUP_SIZE> selected;
        const auto count = function::simd::selectConstant(memberships->data(), GROUP_SIZE, &id,
            nullptr, selected.data(), common::PhysicalTypeID::UINT64,
            function::ComparisonOperation::EQUAL);
        result.reserve(count);
        for (common::sel_t i = 0; i < count; ++i) {
            result.push_back(static_cast<uint16_t>(selected[i]));
        }
    }
    return offsetCache->emplace(id, std::move(result)).first->second;
}

std::unique_ptr<NamespaceCatalogEntry> NamespaceCatalogEntry::copy() const {
    auto result = std::make_unique<NamespaceCatalogEntry>(type, name);
    result->copyFrom(*this);
    result->tableID = tableID;
    result->groupIndex = groupIndex;
    result->relationship = relationship;
    result->localPrimaryKey = localPrimaryKey;
    result->hasMembership = hasMembership;
    result->allowed = allowed;
    if (memberships) {
        result->memberships = std::make_unique<Memberships>(*memberships);
    }
    return result;
}

void NamespaceCatalogEntry::serialize(common::Serializer& serializer) const {
    CatalogEntry::serialize(serializer);
    if (type == CatalogEntryType::NAMESPACE_ENTRY) {
        return;
    }
    serializer.write(tableID);
    serializer.write(relationship);
    if (type == CatalogEntryType::NAMESPACE_POLICY_ENTRY) {
        serializer.write(localPrimaryKey);
        serializer.write(hasMembership);
        serializer.write(allowed.isUnrestricted());
        const auto ids = allowed.getIDs();
        serializer.write<uint64_t>(ids.size());
        for (const auto id : ids) {
            serializer.write(id);
        }
        return;
    }
    serializer.write(groupIndex);
    struct Run {
        uint16_t start;
        uint16_t length;
        common::namespace_id_t id;
    };
    std::vector<Run> runs;
    uint64_t count = 0;
    if (memberships) {
        for (uint16_t row = 0; row < GROUP_SIZE;) {
            const auto start = row;
            const auto id = (*memberships)[row++];
            while (row < GROUP_SIZE && (*memberships)[row] == id) {
                ++row;
            }
            if (id != common::GLOBAL_NAMESPACE_ID) {
                count += row - start;
                runs.push_back({start, static_cast<uint16_t>(row - start), id});
            }
        }
    }
    if (runs.size() == 1 && runs[0].length == GROUP_SIZE) {
        serializer.write<uint64_t>(GROUP_SIZE + 1); // Uniform named group.
        serializer.write(runs[0].id);
        return;
    }
    if (sizeof(uint64_t) + runs.size() * (2 * sizeof(uint16_t) + sizeof(common::namespace_id_t)) <
        count * (sizeof(uint16_t) + sizeof(common::namespace_id_t))) {
        serializer.write<uint64_t>(GROUP_SIZE + 2); // Run encoding, excluding global runs.
        serializer.write<uint64_t>(runs.size());
        for (const auto& run : runs) {
            serializer.write(run.start);
            serializer.write(run.length);
            serializer.write(run.id);
        }
        return;
    }
    serializer.write(count);
    if (memberships) {
        for (uint16_t row = 0; row < GROUP_SIZE; ++row) {
            if ((*memberships)[row] != common::GLOBAL_NAMESPACE_ID) {
                serializer.write(row);
                serializer.write((*memberships)[row]);
            }
        }
    }
}

std::unique_ptr<NamespaceCatalogEntry> NamespaceCatalogEntry::deserialize(
    common::Deserializer& deserializer, CatalogEntryType type) {
    auto result = std::make_unique<NamespaceCatalogEntry>(type, "");
    if (type == CatalogEntryType::NAMESPACE_ENTRY) {
        return result;
    }
    deserializer.deserializeValue(result->tableID);
    deserializer.deserializeValue(result->relationship);
    if (type == CatalogEntryType::NAMESPACE_POLICY_ENTRY) {
        bool all = false;
        deserializer.deserializeValue(result->localPrimaryKey);
        deserializer.deserializeValue(result->hasMembership);
        deserializer.deserializeValue(all);
        std::vector<common::namespace_id_t> ids;
        deserializer.deserializeVector(ids);
        result->allowed =
            all ? common::NamespaceSet{} : common::NamespaceSet::finite(std::move(ids));
        return result;
    }
    deserializer.deserializeValue(result->groupIndex);
    uint64_t count = 0;
    deserializer.deserializeValue(count);
    if (count > GROUP_SIZE + 2) {
        throw common::RuntimeException("Invalid namespace membership group size.");
    }
    result->memberships = std::make_unique<Memberships>();
    if (count == GROUP_SIZE + 1) {
        common::namespace_id_t id = 0;
        deserializer.deserializeValue(id);
        if (id == common::GLOBAL_NAMESPACE_ID) {
            throw common::RuntimeException("Invalid uniform namespace membership group.");
        }
        result->memberships->fill(id);
        return result;
    }
    if (count == GROUP_SIZE + 2) {
        deserializer.deserializeValue(count);
        if (count == 0 || count > GROUP_SIZE) {
            throw common::RuntimeException("Invalid namespace membership run count.");
        }
        uint64_t end = 0;
        for (uint64_t i = 0; i < count; ++i) {
            uint16_t start = 0;
            uint16_t length = 0;
            common::namespace_id_t id = 0;
            deserializer.deserializeValue(start);
            deserializer.deserializeValue(length);
            deserializer.deserializeValue(id);
            if (start < end || length == 0 || uint64_t{start} + length > GROUP_SIZE ||
                id == common::GLOBAL_NAMESPACE_ID) {
                throw common::RuntimeException("Invalid namespace membership run.");
            }
            end = uint64_t{start} + length;
            std::fill(result->memberships->begin() + start, result->memberships->begin() + end, id);
        }
        return result;
    }
    for (uint64_t i = 0; i < count; ++i) {
        uint16_t row = 0;
        common::namespace_id_t id = 0;
        deserializer.deserializeValue(row);
        deserializer.deserializeValue(id);
        if (row >= GROUP_SIZE || id == common::GLOBAL_NAMESPACE_ID ||
            (*result->memberships)[row] != 0) {
            throw common::RuntimeException("Invalid namespace membership group entry.");
        }
        (*result->memberships)[row] = id;
    }
    return result;
}

} // namespace riag::catalog
