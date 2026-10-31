#pragma once

#include "common/types/types.h"

namespace riag {
namespace function {

struct Offset {

    static void operation(common::internalID_t& input, int64_t& result) { result = input.offset; }
};

} // namespace function
} // namespace riag
