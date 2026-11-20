#pragma once

#include <array>
#include <mutex>
#include <unordered_map>

#include "catalog_entry.h"
#include "common/namespace_set.h"

namespace riag::catalog {

// Membership is stored in independent MVCC catalog groups. A missing group is
// entirely global; it does not allocate a column or rewrite the user table.
class NamespaceCatalogEntry final : public CatalogEntry {
public:
    static constexpr uint64_t GROUP_SIZE = 1024;
    using Memberships = std::array<common::namespace_id_t, GROUP_SIZE>;

    NamespaceCatalogEntry(CatalogEntryType type, std::string name)
        : CatalogEntry{type, std::move(name)} {}

    common::table_id_t tableID = common::INVALID_TABLE_ID;
    uint64_t groupIndex = 0;
    bool relationship = false;
    bool localPrimaryKey = false;
    bool hasMembership = false;
    common::NamespaceSet allowed;
    std::unique_ptr<Memberships> memberships;

    std::unique_ptr<NamespaceCatalogEntry> copy() const;
    // Only committed, immutable membership versions may populate this cache.
    // Copies start without cached offsets; mutable versions use their live array.
    const std::vector<uint16_t>& getMemberOffsets(common::namespace_id_t id) const;
    void serialize(common::Serializer& serializer) const override;
    static std::unique_ptr<NamespaceCatalogEntry> deserialize(common::Deserializer& deserializer,
        CatalogEntryType type);

private:
    mutable std::mutex offsetCacheMutex;
    mutable std::unique_ptr<std::unordered_map<common::namespace_id_t, std::vector<uint16_t>>>
        offsetCache;
};

} // namespace riag::catalog
