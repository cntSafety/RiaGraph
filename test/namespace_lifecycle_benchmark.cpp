// Compile the same public-API source against baseline and candidate libraries.
// File preparation and correctness queries are outside the measured interval.
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "main/connection.h"
#include "main/database.h"
#include "namespace_benchmark_metrics.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;
using Clock = std::chrono::steady_clock;

struct TemporaryDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("riag_namespace_lifecycle_" + std::to_string(Clock::now().time_since_epoch().count()));
    TemporaryDirectory() { std::filesystem::create_directory(path); }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

static std::unique_ptr<QueryResult> run(Connection& connection, const std::string& query) {
    auto result = connection.query(query);
    if (!result->isSuccess()) {
        throw std::runtime_error(result->getErrorMessage());
    }
    return result;
}

int main(int argc, char** argv) try {
    const uint64_t rows = argc > 2 ? std::stoull(argv[2]) : 2048;
    const double seconds = argc > 4 ? std::stod(argv[4]) : 1;
    const std::string workload = argc > 5 ? argv[5] : "all";
    const bool cold = argc > 6 && std::string(argv[6]) == "cold";
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 1;
    config.autoCheckpoint = false;
    config.forceCheckpointOnClose = false;
    TemporaryDirectory temporary;
    for (const auto& operation : {"startup", "checkpoint", "recovery"}) {
        if (workload != "all" && workload != operation) {
            continue;
        }
        const auto source = temporary.path / "source";
        const auto working = temporary.path / "working";
        std::filesystem::create_directory(source);
        const bool pending = std::string(operation) != "startup";
        {
            Database database((source / "database").string(), config);
            Connection connection(&database);
            run(connection, "CREATE NODE TABLE P(id INT64 PRIMARY KEY, x INT64)");
            run(connection,
                "UNWIND range(0," + std::to_string(rows - 1) + ") AS i CREATE (:P {id:i,x:i%97})");
            run(connection, "CHECKPOINT");
            if (pending) {
                run(connection, "UNWIND range(" + std::to_string(rows) + "," +
                                    std::to_string(rows * 2 - 1) +
                                    ") AS i CREATE (:P {id:i,x:i%97})");
            }
        }
        const auto dataBytes = std::filesystem::file_size(source / "database");
        uint64_t walBytes = 0;
        for (const auto& file : std::filesystem::directory_iterator(source)) {
            if (file.is_regular_file() &&
                file.path().filename().string().find("wal") != std::string::npos) {
                walBytes += file.file_size();
            }
        }
        double elapsed = 0;
        double cpuElapsed = 0;
        uint64_t iterations = 0;
        NamespaceBenchmarkAllocations allocations;
        for (uint64_t trial = 0; trial < 136; ++trial) {
            std::filesystem::copy(source, working, std::filesystem::copy_options::recursive);
            if (cold) {
                // Evict only this trial's private files, leaving system caches
                // and files belonging to other tasks untouched.
                for (const auto& file : std::filesystem::directory_iterator(working)) {
                    if (!file.is_regular_file()) {
                        continue;
                    }
                    const auto descriptor = open(file.path().c_str(), O_RDONLY);
                    if (descriptor < 0) {
                        throw std::runtime_error("Cannot open private cold-cache fixture.");
                    }
                    const auto synced = fsync(descriptor);
                    const auto advised = posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
                    close(descriptor);
                    if (synced != 0 || advised != 0) {
                        throw std::runtime_error("Cannot evict private cold-cache fixture.");
                    }
                }
            }
            {
                std::unique_ptr<Database> database;
                if (std::string(operation) == "checkpoint") {
                    database = std::make_unique<Database>((working / "database").string(), config);
                }
                if (trial >= 8) {
                    allocations.start();
                }
                const auto cpuStart = std::clock();
                const auto start = Clock::now();
                if (database) {
                    Connection connection(database.get());
                    run(connection, "CHECKPOINT");
                } else {
                    database = std::make_unique<Database>((working / "database").string(), config);
                }
                const auto duration =
                    std::chrono::duration<double, std::nano>(Clock::now() - start).count();
                const auto cpuDuration = double(std::clock() - cpuStart) * 1e9 / CLOCKS_PER_SEC;
                if (trial >= 8) {
                    allocations.stop();
                }
                Connection connection(database.get());
                auto result = run(connection, "MATCH (n:P) RETURN count(*)");
                const auto actual = std::stoull(result->getNext()->getValue(0)->toString());
                if (actual != rows * (pending ? 2 : 1)) {
                    throw std::runtime_error("Incorrect lifecycle row count.");
                }
                if (trial >= 8) {
                    elapsed += duration;
                    cpuElapsed += cpuDuration;
                    ++iterations;
                }
            }
            std::filesystem::remove_all(working);
            if (iterations >= 8 && elapsed >= seconds * 1e9) {
                break;
            }
        }
        struct rusage usage {};
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "{\"mode\":\"global\",\"workload\":\"" << operation << "\",\"rows\":" << rows
                  << ",\"cache\":\"" << (cold ? "cold-advised" : "warm") << "\""
                  << ",\"iterations\":" << iterations
                  << ",\"ns_per_iteration\":" << elapsed / iterations
                  << ",\"cpu_ns_per_iteration\":" << cpuElapsed / iterations
                  << ",\"data_bytes\":" << dataBytes << ",\"wal_bytes\":" << walBytes
                  << ",\"allocations_per_iteration\":"
                  << (allocations.count ?
                             std::to_string(double(allocations.totalCount) / iterations) :
                             "null")
                  << ",\"allocated_bytes_per_iteration\":"
                  << (allocations.count ?
                             std::to_string(double(allocations.totalBytes) / iterations) :
                             "null")
                  << ",\"peak_rss_kib\":" << usage.ru_maxrss << "}\n";
        std::filesystem::remove_all(source);
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
