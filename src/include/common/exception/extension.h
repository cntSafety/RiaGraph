#pragma once

#include "exception.h"

namespace riag {
namespace common {

class RIAG_API ExtensionException : public Exception {
public:
    explicit ExtensionException(const std::string& msg)
        : Exception("Extension exception: " + msg) {}
};

} // namespace common
} // namespace riag
