#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API InterruptException : public Exception {
public:
    explicit InterruptException() : Exception("Interrupted.") {};
};

} // namespace common
} // namespace riag
