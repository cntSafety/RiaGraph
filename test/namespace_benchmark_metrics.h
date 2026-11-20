#pragma once

#include <dlfcn.h>

#ifdef NAMESPACE_BENCHMARK_CALLGRIND
#include <valgrind/callgrind.h>
#endif

#include <cstdint>

// Optional LD_PRELOAD instrumentation. Its allocation overhead excludes these
// instrumented runs from latency acceptance; normal timing runs need no preload.
struct NamespaceBenchmarkAllocations {
    using Counter = uint64_t (*)();
    Counter count =
        reinterpret_cast<Counter>(dlsym(RTLD_DEFAULT, "namespace_benchmark_allocations"));
    Counter bytes = reinterpret_cast<Counter>(dlsym(RTLD_DEFAULT, "namespace_benchmark_bytes"));
    uint64_t startCount = 0, startBytes = 0, totalCount = 0, totalBytes = 0;
    void start() {
#ifdef NAMESPACE_BENCHMARK_CALLGRIND
        CALLGRIND_START_INSTRUMENTATION;
        CALLGRIND_ZERO_STATS;
#endif
        if (count && bytes) {
            startCount = count();
            startBytes = bytes();
        }
    }
    void stop() {
#ifdef NAMESPACE_BENCHMARK_CALLGRIND
        CALLGRIND_STOP_INSTRUMENTATION;
#endif
        if (count && bytes) {
            totalCount += count() - startCount;
            totalBytes += bytes() - startBytes;
        }
    }
};
