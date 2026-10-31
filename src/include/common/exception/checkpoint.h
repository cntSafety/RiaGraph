#pragma once

#include <string>
#include <utility>

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API CheckpointException : public Exception {
public:
    explicit CheckpointException(const std::exception& e) : Exception(e.what()) {};
    explicit CheckpointException(std::string message) : Exception(std::move(message)) {};
};

} // namespace common
} // namespace riag
