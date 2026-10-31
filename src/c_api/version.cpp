#include "main/version.h"

#include "c_api/helpers.h"
#include "c_api/riag.h"

char* riag_get_version() {
    RIAG_C_API_GUARD_BEGIN
    auto version = riag::main::Version::getVersion();
    if (version == nullptr || version[0] == '\0') {
        version = "0.22.0";
    }
    return convertToOwnedCString(version);
    RIAG_C_API_GUARD_END(nullptr)
}

uint64_t riag_get_storage_version() {
    RIAG_C_API_GUARD_BEGIN
    return riag::main::Version::getStorageVersion();
    RIAG_C_API_GUARD_END(0)
}
