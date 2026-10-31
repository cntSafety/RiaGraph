#pragma once

#include "function/scalar_function.h"

namespace riag {
namespace function {

struct TimeBucketFunction {
    static constexpr const char* name = "TIME_BUCKET";

    static function_set getFunctionSet();
};

} // namespace function
} // namespace riag
