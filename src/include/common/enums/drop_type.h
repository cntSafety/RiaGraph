#pragma once

#include <cstdint>
#include <string>

namespace riag {
namespace common {

enum class DropType : uint8_t {
    TABLE = 0,
    SEQUENCE = 1,
    MACRO = 2,
    GRAPH = 3,
    INDEX = 4,
};

struct DropTypeUtils {
    static std::string toString(DropType type);
};

} // namespace common
} // namespace riag
