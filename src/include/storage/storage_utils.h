#pragma once

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <type_traits>

#include "common/checksum.h"
#include "common/constants.h"
#include "common/system_config.h"
#include "common/types/types.h"
#include <concepts>
#include <format>

namespace riag {
namespace storage {

struct PageCursor {
    PageCursor() : PageCursor{UINT32_MAX, UINT16_MAX} {};
    PageCursor(common::page_idx_t pageIdx, uint32_t posInPage)
        : pageIdx{pageIdx}, elemPosInPage{posInPage} {};

    void nextPage() {
        pageIdx++;
        elemPosInPage = 0;
    }

    common::page_idx_t pageIdx;
    // Larger than necessary, but PageCursor is directly written to disk
    // and adding an explicit padding field messes with structured bindings
    uint32_t elemPosInPage;
};
static_assert(std::has_unique_object_representations_v<PageCursor>);

template<typename T>
concept NumericType = std::is_integral_v<T> || std::floating_point<T>;

class StorageUtils {
public:
    enum class ColumnType {
        DEFAULT = 0,
        INDEX = 1,  // This is used for index columns in STRING columns.
        OFFSET = 2, // This is used for offset columns in LIST and STRING columns.
        DATA = 3,   // This is used for data columns in LIST and STRING columns.
        CSR_OFFSET = 4,
        CSR_LENGTH = 5,
        STRUCT_CHILD = 6,
        NULL_MASK = 7,
    };

    template<NumericType T1, NumericType T2>
    static uint64_t divideAndRoundUpTo(T1 v1, T2 v2) {
        return std::ceil(static_cast<double>(v1) / static_cast<double>(v2));
    }

    static std::string getColumnName(const std::string& propertyName, ColumnType type,
        const std::string& prefix);

    static common::offset_t getStartOffsetOfNodeGroup(common::node_group_idx_t nodeGroupIdx) {
        return nodeGroupIdx << common::StorageConfig::NODE_GROUP_SIZE_LOG2;
    }
    static common::node_group_idx_t getNodeGroupIdx(common::offset_t nodeOffset) {
        return nodeOffset >> common::StorageConfig::NODE_GROUP_SIZE_LOG2;
    }
    static std::pair<common::node_group_idx_t, common::offset_t> getNodeGroupIdxAndOffsetInChunk(
        common::offset_t nodeOffset) {
        auto nodeGroupIdx = getNodeGroupIdx(nodeOffset);
        auto offsetInChunk = nodeOffset - getStartOffsetOfNodeGroup(nodeGroupIdx);
        return std::make_pair(nodeGroupIdx, offsetInChunk);
    }

    static std::string getWALFilePath(const std::string& path) {
        return std::format("{}.{}", path, common::StorageConstants::WAL_FILE_SUFFIX);
    }
    static std::string getCheckpointWALFilePath(const std::string& path) {
        return std::format("{}.{}", path, common::StorageConstants::CHECKPOINT_WAL_FILE_SUFFIX);
    }
    static std::string getShadowFilePath(const std::string& path) {
        return std::format("{}.{}", path, common::StorageConstants::SHADOWING_SUFFIX);
    }
    // Marker proving a pinned checkpoint (CHECKPOINT record in the active WAL with no WAL
    // rotation) completed and its shadow was applied before cleanup removed the shadow.
    // Recovery only treats a trailing/mid-file active-WAL CHECKPOINT with no shadow as
    // durable when this marker exists; otherwise a missing shadow is an error, never a
    // silent WAL delete (out-of-band shadow loss must stay loud). Uses a `.tmp` suffix so
    // the file-deletion allowlist permits cleanup.
    static std::string getPinnedCheckpointMarkerPath(const std::string& path) {
        return std::format("{}.pinned.tmp", path);
    }
    // Content of the pinned-checkpoint marker: the offset in the active WAL at which the
    // CHECKPOINT record of the completed checkpoint ends, followed by a checksum of that offset,
    // so a marker that was only partly written reads as "no offset". The marker exists after any
    // completed pinned checkpoint and stays while later checkpoints run, so it only vouches for
    // the CHECKPOINT record that ends at the offset it names.
    static constexpr uint64_t PINNED_CHECKPOINT_MARKER_SIZE = 2 * sizeof(uint64_t);
    static std::array<uint8_t, PINNED_CHECKPOINT_MARKER_SIZE> serializePinnedCheckpointMarker(
        uint64_t walEndOffset) {
        std::array<uint8_t, PINNED_CHECKPOINT_MARKER_SIZE> content{};
        std::memcpy(content.data(), &walEndOffset, sizeof(walEndOffset));
        const auto offsetChecksum = common::checksum(content.data(), sizeof(walEndOffset));
        std::memcpy(content.data() + sizeof(walEndOffset), &offsetChecksum, sizeof(offsetChecksum));
        return content;
    }
    // The offset the marker names, or nothing for a marker without one (an older format, or
    // one that was only partly written).
    static std::optional<uint64_t> parsePinnedCheckpointMarker(
        std::array<uint8_t, PINNED_CHECKPOINT_MARKER_SIZE> content, uint64_t size) {
        if (size != PINNED_CHECKPOINT_MARKER_SIZE) {
            return std::nullopt;
        }
        uint64_t walEndOffset = 0;
        uint64_t storedChecksum = 0;
        std::memcpy(&walEndOffset, content.data(), sizeof(walEndOffset));
        std::memcpy(&storedChecksum, content.data() + sizeof(walEndOffset), sizeof(storedChecksum));
        if (common::checksum(content.data(), sizeof(walEndOffset)) != storedChecksum) {
            return std::nullopt;
        }
        return walEndOffset;
    }
    static std::string getCheckpointIntentLockFilePath(const std::string& path) {
        return std::format("{}.checkpoint.intent.lock", path);
    }
    static std::string getCheckpointApplyLockFilePath(const std::string& path) {
        return std::format("{}.checkpoint.apply.lock", path);
    }
    static std::string getTmpFilePath(const std::string& path) {
        return std::format("{}.{}", path, common::StorageConstants::TEMP_FILE_SUFFIX);
    }
    // <dir>/<stem>.<graphName><ext> for the database path <dir>/<stem><ext>.
    static std::string getGraphPath(const std::string& dbPath, const std::string& graphName);

    static std::string expandPath(const main::ClientContext* context, const std::string& path);

    // Note: This is a relatively slow function because of division and mod and making std::pair.
    // It is not meant to be used in performance critical code path.
    static std::pair<uint64_t, uint64_t> getQuotientRemainder(uint64_t i, uint64_t divisor) {
        return std::make_pair(i / divisor, i % divisor);
    }

    static uint32_t getDataTypeSize(const common::LogicalType& type);
};

} // namespace storage
} // namespace riag
