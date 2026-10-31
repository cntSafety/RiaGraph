#include "common/enums/storage_format.h"

#include "common/exception/binder.h"
#include <format>

namespace riag {
namespace common {

StorageFormat StorageFormatUtils::fromString(const std::string& str) {
    if (str == "icebug-disk") {
        return StorageFormat::ICEBUG_DISK;
    }
    throw BinderException(
        std::format("Unsupported storage format '{}'. Valid options are: icebug-disk.", str));
}

} // namespace common
} // namespace riag
