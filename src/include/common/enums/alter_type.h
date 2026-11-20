#pragma once

#include <cstdint>

namespace riag {
namespace common {

enum class AlterType : uint8_t {
    RENAME = 0,

    ADD_PROPERTY = 10,
    DROP_PROPERTY = 11,
    RENAME_PROPERTY = 12,
    ADD_FROM_TO_CONNECTION = 13,
    DROP_FROM_TO_CONNECTION = 14,
    SET_SORTED_BY = 15,
    ADD_NAMESPACES = 16,
    REMOVE_NAMESPACES = 17,
    SET_NAMESPACES = 18,

    COMMENT = 201,
    INVALID = 255
};

} // namespace common
} // namespace riag
