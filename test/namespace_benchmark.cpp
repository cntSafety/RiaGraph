// Compile this same source against the frozen baseline and candidate libraries.
// JSON output is consumed by run_namespace_benchmark.py; setup is excluded.
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "main/connection.h"
#include "main/database.h"
#include "namespace_benchmark_metrics.h"
#include "processor/result/flat_tuple.h"
#include <sys/syscall.h>

using namespace riag::main;
using Clock = std::chrono::steady_clock;

struct TemporaryCsv {
    std::filesystem::path path;
    ~TemporaryCsv() {
        if (!path.empty()) {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }
};

static void check(const QueryResult& result) {
    if (!result.isSuccess()) {
        throw std::runtime_error(result.getErrorMessage());
    }
}

struct ThreadPlacement {
    int clientCPU = -1;
    int workerCPU = -1;
};

static ThreadPlacement pinQueryThreads() {
    cpu_set_t available;
    CPU_ZERO(&available);
    if (sched_getaffinity(0, sizeof(available), &available) != 0 || CPU_COUNT(&available) != 2) {
        throw std::runtime_error("Pinned query threads require exactly two allowed CPUs.");
    }
    ThreadPlacement placement;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (CPU_ISSET(cpu, &available)) {
            if (placement.clientCPU == -1) {
                placement.clientCPU = cpu;
            } else {
                placement.workerCPU = cpu;
            }
        }
    }
    const auto clientID = static_cast<pid_t>(syscall(SYS_gettid));
    uint64_t workers = 0;
    for (const auto& task : std::filesystem::directory_iterator("/proc/self/task")) {
        const auto threadID = static_cast<pid_t>(std::stol(task.path().filename().string()));
        cpu_set_t target;
        CPU_ZERO(&target);
        CPU_SET(threadID == clientID ? placement.clientCPU : placement.workerCPU, &target);
        if (sched_setaffinity(threadID, sizeof(target), &target) != 0) {
            throw std::runtime_error("Could not pin the benchmark's own query thread.");
        }
        workers += threadID != clientID;
    }
    if (workers != 1) {
        throw std::runtime_error("Pinned benchmark requires exactly one query worker.");
    }
    return placement;
}

int main(int argc, char** argv) try {
    const std::string mode = argc > 1 ? argv[1] : "global";
    const uint64_t rows = argc > 2 ? std::stoull(argv[2]) : 131072;
    const uint64_t domains = argc > 3 ? std::stoull(argv[3]) : 64;
    const double seconds = argc > 4 ? std::stod(argv[4]) : 1.0;
    const std::string workload = argc > 5 ? argv[5] : "all";
    const std::string layout = argc > 6 ? argv[6] : "clustered";
    const uint64_t fanout = argc > 7 ? std::stoull(argv[7]) : 1;
    const uint64_t fixedIterations = argc > 8 ? std::stoull(argv[8]) : 0;
    const double preheatMilliseconds = argc > 9 ? std::stod(argv[9]) : 250;
    const bool directQuery = argc > 10 && std::string(argv[10]) == "query";
    const bool cross = argc > 11 && std::string(argv[11]) == "cross";
    const bool pinThreads = argc > 12 && std::string(argv[12]) == "pinned";
    const bool mixed = mode == "mixed";
    const bool named = mode == "named" || mode == "local" || mixed;
    const bool local = mode == "local";
    const bool property = mode == "property";
    if (cross && (local || mixed || layout != "clustered" ||
                     (workload != "traverse" && workload != "recursive" && workload != "detach"))) {
        throw std::runtime_error(
            "Cross-edge fixtures require clustered global/property/named graph queries.");
    }
    if (layout != "clustered" && layout != "interleaved" && layout != "skew") {
        throw std::runtime_error("Unsupported query benchmark layout.");
    }
    if (rows == 0 || domains == 0 || rows % domains != 0 || seconds <= 0 || fanout == 0 ||
        (layout == "skew" && domains < 2)) {
        throw std::runtime_error("Require rows divisible by domains and positive duration.");
    }
    SystemConfig config;
    config.bufferPoolSize = 2ull * 1024 * 1024 * 1024;
    config.maxNumThreads = 1;
#ifdef NAMESPACE_BENCHMARK_CALLGRIND
    // Valgrind cannot reserve the engine's default multi-terabyte address region.
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxDBSize = 1024ull * 1024 * 1024;
#endif
    Database database("", config);
    const auto placement = pinThreads ? pinQueryThreads() : ThreadPlacement{};
    Connection connection(&database);
    const auto run = [&](const std::string& query) {
        auto result = connection.query(query);
        check(*result);
    };
    run(std::string("CREATE NODE TABLE P(id INT64 ") + (local ? "LOCAL " : "") +
        "PRIMARY KEY, x INT64, tenant INT64)");
    std::vector<uint64_t> starts(domains), counts(domains, rows / domains);
    if (layout == "skew") {
        counts[0] = rows / 2;
        for (uint64_t domain = 1; domain < domains; ++domain) {
            counts[domain] = (rows - counts[0]) / (domains - 1);
        }
        counts.back() += rows - counts[0] - counts[1] * (domains - 1);
    }
    for (uint64_t domain = 1; domain < domains; ++domain) {
        starts[domain] = starts[domain - 1] + counts[domain - 1];
    }
    const auto block = counts[0];
    const auto selector = [&](uint64_t domain) -> std::string {
        return !named               ? "" :
               mixed && domain == 0 ? " IN GLOBAL" :
                                      " IN ns" + std::to_string(domain);
    };
    for (uint64_t domain = 0; domain < domains; ++domain) {
        if (named || mode == "empty" || mode == "occupied") {
            run("CREATE NAMESPACE ns" + std::to_string(domain));
        }
    }
    if (mode == "occupied") {
        run("CREATE NODE TABLE NamedData(id INT64 PRIMARY KEY)");
        for (uint64_t domain = 0; domain < domains; ++domain) {
            run("CREATE (:NamedData {id:" + std::to_string(domain) + "} IN ns" +
                std::to_string(domain) + ")");
        }
    }
    if (layout == "interleaved") {
        std::string query = "UNWIND range(0," + std::to_string(block - 1) + ") AS i CREATE ";
        for (uint64_t domain = 0; domain < domains; ++domain) {
            const auto key = "(i+" + std::to_string(local ? 0 : starts[domain]) + ")";
            query += (domain ? "," : "") + std::string("(:P {id:") + key + ",x:" + key +
                     "%97,tenant:" + std::to_string(domain) + "}" + selector(domain) + ")";
        }
        run(query);
    } else {
        for (uint64_t domain = 0; domain < domains; ++domain) {
            const auto start = local ? 0 : starts[domain];
            const auto end = start + counts[domain] - 1;
            run("UNWIND range(" + std::to_string(start) + "," + std::to_string(end) +
                ") AS i CREATE (:P {id:i, x:i%97, tenant:" + std::to_string(domain) + "}" +
                (cross ? "" : selector(domain)) + ")");
        }
    }
    const bool graphWork = workload == "all" || workload == "traverse" || workload == "recursive" ||
                           workload == "detach";
    if (graphWork) {
        const auto taggedEdges = property || named;
        run(std::string("CREATE REL TABLE R(FROM P TO P") + (taggedEdges ? ", tenant INT64" : "") +
            ")");
        run("BEGIN TRANSACTION");
        for (uint64_t domain = 0; domain < domains; ++domain) {
            const auto start = local ? 0 : starts[domain];
            const auto end = start + counts[domain] - 1;
            const auto ns = selector(domain);
            TemporaryCsv edges;
            edges.path = std::filesystem::temp_directory_path() /
                         ("riag_namespace_edges_" +
                             std::to_string(Clock::now().time_since_epoch().count()) + ".csv");
            {
                std::ofstream output(edges.path);
                if (!output) {
                    throw std::runtime_error("Cannot create benchmark edge fixture.");
                }
                output << (taggedEdges ? "from,to,tenant\n" : "from,to\n");
                for (auto source = start; source <= end; ++source) {
                    for (uint64_t step = 1; step <= fanout; ++step) {
                        const auto targetDomain =
                            cross && step % 2 == 0 ? (domain + 1) % domains : domain;
                        const auto targetStart = local ? 0 : starts[targetDomain];
                        output << source << ','
                               << targetStart + (source - start + step) % counts[targetDomain];
                        if (taggedEdges) {
                            // Some edges have a different owner even when both
                            // endpoints remain in their source namespace.
                            output << ',' << (domain + (step % 4 == 3)) % domains;
                        }
                        output << '\n';
                    }
                }
            }
            run("COPY R" + (cross ? "" : ns) + " FROM '" + edges.path.generic_string() +
                "' (HEADER=true)");
        }
        run("COMMIT");
        if (named) {
            // COPY uses one namespace for both endpoints. Bulk-load globally,
            // then assign independent edge/node memberships without changing IDs.
            for (uint64_t domain = 0; domain < domains; ++domain) {
                const auto target = selector(domain);
                run("MATCH ()-[r:R]->() WHERE r.tenant=" + std::to_string(domain) + " SET r" +
                    target);
                if (cross) {
                    run("MATCH (n:P) WHERE n.tenant=" + std::to_string(domain) + " SET n" + target);
                }
            }
        }
    }
    const auto ns = selector(0);
    const auto where = property ? " WHERE n.tenant=0" : "";
    for (const auto& table : {"W", "C"}) {
        run(std::string("CREATE NODE TABLE ") + table + "(id INT64 " + (local ? "LOCAL " : "") +
            "PRIMARY KEY" + (property ? ", tenant INT64" : "") + ")");
    }
    TemporaryCsv csv;
    if (workload == "all" || workload == "copy") {
        run(std::string("CREATE NODE TABLE Bulk(id SERIAL ") + (local ? "LOCAL " : "") +
            "PRIMARY KEY, x INT64" + (property ? ", tenant INT64" : "") + ")");
        csv.path = std::filesystem::temp_directory_path() /
                   ("riag_namespace_benchmark_" +
                       std::to_string(Clock::now().time_since_epoch().count()) + ".csv");
        std::ofstream output(csv.path);
        if (!output) {
            throw std::runtime_error("Cannot create benchmark CSV.");
        }
        output << (property ? "x,tenant\n" : "x\n");
        for (uint64_t row = 0; row < 4096; ++row) {
            output << row << (property ? ",0\n" : "\n");
        }
    }
    std::vector<std::pair<std::string, std::string>> queries{
        {"scan", "MATCH (n:P" + std::string(ns) + ")" + where + " RETURN sum(n.x)"},
        {"point", "MATCH (n:P {id:$id}" + std::string(ns) + ")" + where + " RETURN n.x"},
        {"merge", "MERGE (n:P {id:$id" + std::string(property ? ",tenant:0" : "") + "}" + ns +
                      ") RETURN n.x"},
        {"update",
            "MATCH (n:P {id:$id}" + std::string(ns) + ")" + where + " SET n.x=n.x+1 RETURN n.x"},
    };
    if (graphWork) {
        queries.emplace_back("traverse",
            "MATCH (n:P {id:$id}" + std::string(ns) + ")-[" + (property ? "r" : "") + ":R" + ns +
                "]->(b:P" + ns + ")" +
                (property ? " WHERE n.tenant=0 AND r.tenant=0 AND b.tenant=0" : "") +
                " RETURN sum(b.x)");
        queries.emplace_back("recursive",
            "MATCH (n:P {id:$id}" + std::string(ns) + ")-[:R*1..4" +
                (property ? " (e, v | WHERE e.tenant=0)" : ns) + "]->(b:P" + ns + ")" +
                (property ? " WHERE n.tenant=0 AND b.tenant=0" : "") + " RETURN count(*)");
    }
    const std::string tenantProperty = property ? ",tenant:0" : "";
    queries.emplace_back("create",
        "CREATE (n:W {id:$id" + tenantProperty + "}" + ns + ") RETURN n.id");
    queries.emplace_back("churn",
        "CREATE (n:C {id:$id" + tenantProperty + "}" + ns + ") DELETE n RETURN count(*)");
    if (!csv.path.empty()) {
        queries.emplace_back("copy", std::string("COPY Bulk(") + (property ? "x,tenant" : "x") +
                                         ")" + ns + " FROM '" + csv.path.generic_string() +
                                         "' (HEADER=true)");
    }
    if (preheatMilliseconds > 0) {
        auto preheat = connection.prepare("MATCH (n:P {id:0}" + ns + ")" + where + " RETURN n.x");
        if (!preheat->isSuccess()) {
            throw std::runtime_error(preheat->getErrorMessage());
        }
        const auto deadline =
            Clock::now() + std::chrono::duration<double, std::milli>(preheatMilliseconds);
        do {
            auto result = connection.execute(preheat.get());
            check(*result);
        } while (Clock::now() < deadline);
    }
    queries.emplace_back("delete",
        "MATCH (n:P {id:$id}" + std::string(ns) + ")" + where + " DELETE n RETURN count(*)");
    if (graphWork) {
        // Ordinary DELETE cannot remove the cycle's incident relationships.
        queries.pop_back();
        queries.emplace_back("detach", "MATCH (n:P {id:$id}" + std::string(ns) + ")" + where +
                                           " DETACH DELETE n RETURN count(*)");
    }
    for (const auto& [name, query] : queries) {
        if (workload != "all" && workload != name) {
            continue;
        }
        const auto deletions = name == "delete" || name == "detach";
        const auto deleteLimit = named || property ? block : rows;
        if (deletions && deleteLimit <= 32) {
            throw std::runtime_error("Deletion benchmarks require more than 32 selected rows.");
        }
        auto prepared = connection.prepare(query);
        if (!prepared->isSuccess()) {
            throw std::runtime_error(prepared->getErrorMessage() + " Query: " + query);
        }
        uint64_t checksum = 0;
        uint64_t iteration = 0;
        const auto parameterPosition = query.find("$id");
        const auto execute = [&]() {
            std::unique_ptr<QueryResult> result;
            const auto id = int64_t(name == "create" || name == "churn" ?
                                        iteration :
                                        iteration % (named || property ? block : rows));
            if (directQuery) {
                auto statement = query;
                if (parameterPosition != std::string::npos) {
                    statement.replace(parameterPosition, 3, std::to_string(id));
                }
                result = connection.query(statement);
            } else if (parameterPosition != std::string::npos) {
                result = connection.execute(prepared.get(), std::make_pair(std::string("id"), id));
            } else {
                result = connection.execute(prepared.get());
            }
            check(*result);
            while (result->hasNext()) {
                const auto value = result->getNext()->getValue(0);
                if (deletions && value->toString() != "1") {
                    throw std::runtime_error("Deletion benchmark selected an already deleted row.");
                }
                checksum += name == "copy"  ? 4096 :
                            value->isNull() ? 0 :
                                              std::stoull(value->toString());
            }
            ++iteration;
        };
        for (uint64_t warmup = 0; warmup < 32; ++warmup) {
            execute();
        }
        const auto warmupIterations = iteration;
        NamespaceBenchmarkAllocations allocations;
        allocations.start();
        const auto cpuStart = std::clock();
        const auto start = Clock::now();
        const auto deadline = start + std::chrono::duration<double>(seconds);
        if (fixedIterations) {
            for (uint64_t measured = 0;
                 measured < fixedIterations && (!deletions || iteration < deleteLimit);
                 ++measured) {
                execute();
            }
        } else {
            do {
                execute();
            } while (Clock::now() < deadline && (!deletions || iteration < deleteLimit));
        }
        const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
        const auto cpuElapsed = double(std::clock() - cpuStart) * 1e9 / CLOCKS_PER_SEC;
        allocations.stop();
        if (name == "create" || name == "churn" || name == "copy") {
            const auto table = name == "create" ? "W" : name == "churn" ? "C" : "Bulk";
            auto result = connection.query(
                std::string("MATCH (n:") + table + ns + ")" + where + " RETURN count(*)");
            check(*result);
            const auto expected = name == "churn" ? 0 :
                                  name == "copy"  ? iteration * 4096 :
                                                    iteration;
            if (!result->hasNext() ||
                result->getNext()->getValue(0)->toString() != std::to_string(expected)) {
                throw std::runtime_error("Incorrect final write-benchmark row count.");
            }
        }
        iteration -= warmupIterations;
        struct rusage usage {};
        getrusage(RUSAGE_SELF, &usage);
        std::cout << "{\"mode\":\"" << mode << "\",\"workload\":\"" << name
                  << "\",\"rows\":" << rows << ",\"domains\":" << domains << ",\"layout\":\""
                  << layout << "\",\"fanout\":" << fanout << ",\"topology\":\""
                  << (cross ? "cross" : "isolated") << "\",\"iterations\":" << iteration
                  << ",\"preheat_ms\":" << preheatMilliseconds << ",\"execution\":\""
                  << (directQuery ? "query" : "prepared") << "\""
                  << ",\"thread_placement\":\"" << (pinThreads ? "pinned" : "automatic") << "\""
                  << ",\"client_cpu\":" << placement.clientCPU
                  << ",\"worker_cpu\":" << placement.workerCPU
                  << ",\"tenant_write_control\":" << (property ? "true" : "false")
                  << ",\"edge_tenant_control\":" << (property && graphWork ? "true" : "false")
                  << ",\"primary_key_scope\":\"" << (local ? "LOCAL" : "GLOBAL") << "\""
                  << ",\"ns_per_iteration\":" << elapsed / iteration
                  << ",\"cpu_ns_per_iteration\":" << cpuElapsed / iteration
                  << ",\"peak_rss_kib\":" << usage.ru_maxrss << ",\"allocations_per_iteration\":"
                  << (allocations.count ?
                             std::to_string(double(allocations.totalCount) / iteration) :
                             "null")
                  << ",\"allocated_bytes_per_iteration\":"
                  << (allocations.count ?
                             std::to_string(double(allocations.totalBytes) / iteration) :
                             "null")
                  << ",\"checksum\":" << checksum << "}\n";
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
