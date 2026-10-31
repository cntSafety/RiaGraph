#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

class RIAG_API CatalogException : public Exception {
public:
    explicit CatalogException(const std::string& msg) : Exception("Catalog exception: " + msg) {};
};

} // namespace common
} // namespace riag
