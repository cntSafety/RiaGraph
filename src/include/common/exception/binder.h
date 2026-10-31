#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API BinderException : public Exception {
public:
    explicit BinderException(const std::string& msg) : Exception("Binder exception: " + msg) {};
};

} // namespace common
} // namespace riag
