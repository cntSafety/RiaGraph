#pragma once

#include "function/aggregate_function.h"

namespace riag {
namespace function {

struct AggregatePercentileDiscFunction {
    static constexpr const char* name = "PERCENTILEDISC";
    static constexpr const char* prettyName = "percentileDisc";

    static function_set getFunctionSet();
};

} // namespace function
} // namespace riag
