#pragma once

#include "function/aggregate_function.h"

namespace riag {
namespace function {

struct AggregatePercentileContFunction {
    static constexpr const char* name = "PERCENTILECONT";
    static constexpr const char* prettyName = "percentileCont";

    static function_set getFunctionSet();
};

} // namespace function
} // namespace riag
