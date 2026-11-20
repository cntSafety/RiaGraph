#include <iostream>
#include <stdexcept>

#include "main/connection.h"
#include "main/database.h"

using namespace riag::main;

int main(int argc, char** argv) try {
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 1;
    Database database("", config);
    Connection connection(&database);
    const auto run = [&](const std::string& query, bool print = false) {
        auto result = connection.query(query);
        if (!result->isSuccess()) {
            throw std::runtime_error(result->getErrorMessage());
        }
        if (print) {
            std::cout << result->toString();
        }
    };
    const bool local = argc > 1 && std::string(argv[1]) == "local";
    if (local) {
        run("CREATE NAMESPACE red");
    }
    run(std::string("CREATE NODE TABLE P(id INT64 ") + (local ? "LOCAL " : "") +
        "PRIMARY KEY, x INT64)");
    run("CREATE REL TABLE R(FROM P TO P)");
    run(std::string("CREATE (:P {id:0, x:1}") + (local ? " IN red" : "") + ")");
    const auto selector = local ? " IN red" : "";
    for (const auto& query : {"MATCH (n:P" + std::string(selector) + ") RETURN sum(n.x)",
             "MATCH (n:P {id:0}" + std::string(selector) + ") RETURN n.x",
             "MERGE (n:P {id:0}" + std::string(selector) + ") RETURN n.x",
             std::string("MATCH (a:P)-[r:R]->(b:P) RETURN count(*)")}) {
        std::cout << "QUERY " << query << '\n';
        run("EXPLAIN " + query, true);
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
