#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <span>

namespace riag::common {

using namespace_id_t = uint64_t;
inline constexpr namespace_id_t GLOBAL_NAMESPACE_ID = 0;

// A bound namespace selector. Names are resolved by the binder; execution only
// compares IDs. The unrestricted, empty and singleton cases allocate no storage.
class NamespaceSet {
public:
    NamespaceSet() = default;

    static NamespaceSet none() { return NamespaceSet{Kind::NONE}; }
    static NamespaceSet single(namespace_id_t id) {
        auto result = NamespaceSet{Kind::ONE};
        result.singleton = id;
        return result;
    }
    static NamespaceSet finite(std::vector<namespace_id_t> ids) {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        if (ids.empty()) {
            return none();
        }
        if (ids.size() == 1) {
            return single(ids.front());
        }
        auto result = NamespaceSet{Kind::MANY};
        result.members = std::move(ids);
        return result;
    }

    bool isUnrestricted() const { return kind == Kind::ALL; }
    bool isEmpty() const { return kind == Kind::NONE; }
    std::optional<namespace_id_t> getSingleton() const {
        return kind == Kind::ONE ? std::optional{singleton} : std::nullopt;
    }
    std::span<const namespace_id_t> getIDs() const {
        if (kind == Kind::ONE) {
            return {&singleton, 1};
        }
        return members;
    }
    bool contains(namespace_id_t id) const {
        switch (kind) {
        case Kind::ALL:
            return true;
        case Kind::NONE:
            return false;
        case Kind::ONE:
            return singleton == id;
        case Kind::MANY:
            return std::binary_search(members.begin(), members.end(), id);
        }
        return false;
    }

    NamespaceSet intersect(const NamespaceSet& other) const {
        if (isUnrestricted()) {
            return other;
        }
        if (other.isUnrestricted()) {
            return *this;
        }
        if (isEmpty() || other.isEmpty()) {
            return none();
        }
        if (kind == Kind::ONE) {
            return other.contains(singleton) ? *this : none();
        }
        if (other.kind == Kind::ONE) {
            return contains(other.singleton) ? other : none();
        }
        std::vector<namespace_id_t> result;
        result.reserve(std::min(members.size(), other.members.size()));
        std::set_intersection(members.begin(), members.end(), other.members.begin(),
            other.members.end(), std::back_inserter(result));
        return finite(std::move(result));
    }

    bool operator==(const NamespaceSet& other) const {
        return kind == other.kind && (kind != Kind::ONE || singleton == other.singleton) &&
               members == other.members;
    }

private:
    enum class Kind : uint8_t { ALL, NONE, ONE, MANY };
    explicit NamespaceSet(Kind kind) : kind{kind} {}

    Kind kind = Kind::ALL;
    namespace_id_t singleton = GLOBAL_NAMESPACE_ID;
    std::vector<namespace_id_t> members;
};

} // namespace riag::common
