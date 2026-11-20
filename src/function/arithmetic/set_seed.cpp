#include "common/exception/runtime.h"
#include "common/random_engine.h"
#include "function/arithmetic/vector_arithmetic_functions.h"
#include "function/scalar_function.h"
#include <format>

namespace riag {
namespace function {

using namespace riag::common;

struct SetSeed {
    static void operation(double& seed, void* dataPtr) {
        // The seed is scaled to the full uint64_t range below. 2^64 (seed == 1) and anything
        // negative or NaN has no uint64_t representation, so casting it would be undefined.
        if (!(seed >= 0.0 && seed < 1.0)) {
            throw RuntimeException{
                std::format("Invalid seed {}: setseed expects a value in the range [0, 1).", seed)};
        }
        auto context = reinterpret_cast<FunctionBindData*>(dataPtr)->clientContext;
        RandomEngine::Get(*context)->setSeed(
            static_cast<uint64_t>(seed * static_cast<double>(UINT64_MAX)));
    }
};

function_set SetSeedFunction::getFunctionSet() {
    function_set result;
    result.push_back(
        std::make_unique<ScalarFunction>(name, std::vector<LogicalTypeID>{LogicalTypeID::DOUBLE},
            LogicalTypeID::INT32, ScalarFunction::UnarySetSeedFunction<double, int, SetSeed>));
    return result;
}

} // namespace function
} // namespace riag
