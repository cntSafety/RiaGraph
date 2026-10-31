#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API ConnectionException : public Exception {
public:
    explicit ConnectionException(const std::string& msg)
        : Exception("Connection exception: " + msg) {};
};

} // namespace common
} // namespace riag
