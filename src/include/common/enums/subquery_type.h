#pragma once

#include <cstdint>

namespace riag {
namespace common {

enum class SubqueryType : uint8_t {
    COUNT = 1,
    EXISTS = 2,
};

}
} // namespace riag
