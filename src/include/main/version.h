#pragma once
#include <cstdint>

#include "common/api.h"
namespace riag {
namespace main {

struct Version {
public:
    /**
     * @brief Get the version of the RiaG library.
     * @return const char* The version of the RiaG library.
     */
    RIAG_API static const char* getVersion();

    /**
     * @brief Get the storage version of the RiaG library.
     * @return uint64_t The storage version of the RiaG library.
     */
    RIAG_API static uint64_t getStorageVersion();
};
} // namespace main
} // namespace riag
