#pragma once

#include "exception.h"

namespace riag {
namespace common {

class RIAG_API IOException : public Exception {
public:
    explicit IOException(const std::string& msg) : Exception("IO exception: " + msg) {}
};

} // namespace common
} // namespace riag
