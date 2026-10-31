#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API ConversionException : public Exception {
public:
    explicit ConversionException(const std::string& msg)
        : Exception("Conversion exception: " + msg) {}
};

} // namespace common
} // namespace riag
