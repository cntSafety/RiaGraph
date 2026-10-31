#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API InternalException : public Exception {
public:
    explicit InternalException(const std::string& msg) : Exception(msg) {};
};

} // namespace common
} // namespace riag
