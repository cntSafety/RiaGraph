// Optional Linux/glibc-only allocation counters for standalone benchmarks.
// g++ -std=c++20 -O3 -shared -fPIC namespace_allocation_probe.cpp -o allocations.so
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>

extern "C" {
void* __libc_malloc(size_t);
void* __libc_calloc(size_t, size_t);
void* __libc_realloc(void*, size_t);
void* __libc_memalign(size_t, size_t);
}

namespace {
constinit std::atomic<uint64_t> allocations{0};
constinit std::atomic<uint64_t> bytes{0};
void* record(void* result, size_t size) {
    if (result) {
        allocations.fetch_add(1, std::memory_order_relaxed);
        bytes.fetch_add(size, std::memory_order_relaxed);
    }
    return result;
}
} // namespace

extern "C" uint64_t namespace_benchmark_allocations() {
    return allocations.load(std::memory_order_relaxed);
}
extern "C" uint64_t namespace_benchmark_bytes() {
    return bytes.load(std::memory_order_relaxed);
}
extern "C" void* malloc(size_t size) {
    return record(__libc_malloc(size), size);
}
extern "C" void* calloc(size_t count, size_t size) {
    return record(__libc_calloc(count, size), count * size);
}
extern "C" void* realloc(void* previous, size_t size) {
    return record(__libc_realloc(previous, size), size);
}
extern "C" void* aligned_alloc(size_t alignment, size_t size) {
    return record(__libc_memalign(alignment, size), size);
}
extern "C" int posix_memalign(void** output, size_t alignment, size_t size) {
    if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) {
        return EINVAL;
    }
    auto* result = record(__libc_memalign(alignment, size), size);
    if (!result) {
        return ENOMEM;
    }
    *output = result;
    return 0;
}
