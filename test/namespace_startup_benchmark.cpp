// Open the exact same private legacy fixture with both engines. The coordinator
// verifies its hash before/after the run. Cache advice affects this fixture only.
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "main/connection.h"
#include "main/database.h"
#include "namespace_benchmark_metrics.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) try {
    if (argc != 14 || std::string(argv[5]) != "startup") {
        throw std::runtime_error("Expected a startup workload and a private fixture path.");
    }
    const auto rows = std::stoull(argv[2]);
    const auto seconds = std::stod(argv[4]);
    const bool cold = std::string(argv[6]) == "cold";
    const std::filesystem::path fixture(argv[13]);
    if (!std::filesystem::is_regular_file(fixture)) {
        throw std::runtime_error("Startup fixture is not a regular file.");
    }
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 1;
    config.autoCheckpoint = false;
    config.forceCheckpointOnClose = false;
    uint64_t iterations = 0;
    double elapsed = 0, cpuElapsed = 0;
    NamespaceBenchmarkAllocations allocations;
    for (uint64_t trial = 0; trial < 4104; ++trial) {
        if (cold) {
            const auto descriptor = open(fixture.c_str(), O_RDONLY);
            if (descriptor < 0) {
                throw std::runtime_error("Cannot open private startup fixture.");
            }
            const auto synced = fsync(descriptor);
            const auto advised = posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
            close(descriptor);
            if (synced != 0 || advised != 0) {
                throw std::runtime_error("Cannot advise eviction of the private fixture.");
            }
        }
        if (trial >= 8) {
            allocations.start();
        }
        const auto cpuStart = std::clock();
        const auto start = Clock::now();
        auto database = std::make_unique<Database>(fixture.string(), config);
        const auto duration =
            std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        const auto cpuDuration = double(std::clock() - cpuStart) * 1e9 / CLOCKS_PER_SEC;
        if (trial >= 8) {
            allocations.stop();
        }
        Connection connection(database.get());
        auto result = connection.query("MATCH (n:P) RETURN count(*)");
        if (!result->isSuccess() || !result->hasNext() ||
            std::stoull(result->getNext()->getValue(0)->toString()) != rows) {
            throw std::runtime_error("Incorrect startup fixture row count.");
        }
        if (trial >= 8) {
            elapsed += duration;
            cpuElapsed += cpuDuration;
            ++iterations;
        }
        if (iterations >= 8 && elapsed >= seconds * 1e9) {
            break;
        }
    }
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    std::cout << "{\"mode\":\"global\",\"workload\":\"startup\",\"rows\":" << rows
              << ",\"cache\":\"" << (cold ? "cold-advised-same-fixture" : "warm-same-fixture")
              << "\",\"iterations\":" << iterations
              << ",\"ns_per_iteration\":" << elapsed / iterations
              << ",\"cpu_ns_per_iteration\":" << cpuElapsed / iterations
              << ",\"data_bytes\":" << std::filesystem::file_size(fixture)
              << ",\"peak_rss_kib\":" << usage.ru_maxrss << ",\"allocations_per_iteration\":"
              << (allocations.count ? std::to_string(double(allocations.totalCount) / iterations) :
                                      "null")
              << ",\"allocated_bytes_per_iteration\":"
              << (allocations.count ? std::to_string(double(allocations.totalBytes) / iterations) :
                                      "null")
              << "}\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
