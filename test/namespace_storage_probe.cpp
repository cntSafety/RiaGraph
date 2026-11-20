// Standalone subprocess helper for the namespace format/recovery verification.
// Compile against either baseline or candidate; only the candidate executes the
// namespace statements. "promote-crash" bypasses destructors and checkpoints.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;

int main(int argc, char** argv) try {
    if (argc < 3) {
        throw std::runtime_error("Expected database path and mode.");
    }
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 1;
    config.autoCheckpoint = false;
    config.forceCheckpointOnClose = false;
    config.throwOnWalReplayFailure = std::string(argv[2]) != "query-lenient";
    Database database(argv[1], config);
    Connection connection(&database);
    const auto run = [&](const std::string& query, bool print = false) {
        auto result = connection.query(query);
        for (auto* current = result.get(); current; current = current->getNextQueryResult()) {
            if (!current->isSuccess()) {
                throw std::runtime_error(query + ": " + current->getErrorMessage());
            }
            if (print) {
                while (current->hasNext()) {
                    std::cout << current->getNext()->toString();
                }
            }
        }
    };
    const std::string mode = argv[2];
    if (mode == "create-global") {
        run("CREATE NODE TABLE P(id INT64 PRIMARY KEY)");
        run("CREATE (:P {id:0})");
        run("CHECKPOINT");
    } else if (mode == "promote-crash") {
        run("CREATE NAMESPACE red");
        run("CREATE NAMESPACE blue");
        run("CREATE NODE TABLE L(id INT64 LOCAL PRIMARY KEY)");
        run("CREATE REL TABLE R(FROM L TO L)");
        run("CREATE (:P {id:1} IN red)");
        run("CREATE (:L {id:1} IN red), (:L {id:1} IN blue)");
        run("MATCH (a:L IN red), (b:L IN blue) CREATE (a)-[:R IN red]->(b)");
        run("MATCH ()-[r:R]->() SET r IN blue");
        std::_Exit(0);
    } else if ((mode == "query" || mode == "query-lenient") && argc == 4) {
        run(argv[3], true);
    } else if ((mode == "script" || mode == "lines") && argc == 4) {
        std::ifstream input(argv[3]);
        if (!input) {
            throw std::runtime_error("Cannot read script.");
        }
        if (mode == "lines") {
            for (std::string line; std::getline(input, line);) {
                if (!line.empty()) {
                    run(line, true);
                }
            }
        } else {
            std::stringstream script;
            script << input.rdbuf();
            run(script.str(), true);
        }
    } else {
        throw std::runtime_error("Unknown mode.");
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 2;
}
