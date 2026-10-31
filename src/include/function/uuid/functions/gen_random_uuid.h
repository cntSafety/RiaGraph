#pragma once

#include "common/types/uuid.h"

namespace riag {
namespace function {

struct GenRandomUUID {
    static void operation(common::uuid& input, void* dataPtr);
};

} // namespace function
} // namespace riag
