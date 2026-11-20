#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"
#include "storage/storage_version_info.h"

using namespace riag::main;
using namespace riag::storage;

namespace riag {
namespace testing {

// An in-memory database creates its header lazily, on the first call that asks for it. Its data
// file handle has no file system behind it, so that first call must not try to read a persistent
// header first: under runtime checks it dereferenced the missing file system and crashed.
class InMemoryDatabaseHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        database = std::make_unique<Database>("", config);
        connection = std::make_unique<Connection>(database.get());
    }
    void run(const std::string& query) {
        auto result = connection->query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    }
    std::unique_ptr<Database> database;
    std::unique_ptr<Connection> connection;
};

TEST_F(InMemoryDatabaseHeaderTest, StorageVersionInitializesHeader) {
    auto result = connection->query("CALL storage_version() RETURN *");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    ASSERT_TRUE(result->hasNext());
    EXPECT_EQ(result->getNext()->getValue(0)->toString(),
        std::to_string(StorageVersionInfo::getStorageVersion()));
}

// DROP GRAPH rewrites the main database header, which is that header's first use here.
TEST_F(InMemoryDatabaseHeaderTest, DropGraphInitializesHeader) {
    run("CREATE GRAPH other");
    run("DROP GRAPH other");
    run("CREATE GRAPH other");
    run("USE GRAPH other");
    run("CREATE NODE TABLE T(id INT64 PRIMARY KEY)");
    run("CREATE (:T {id: 1})");
    run("USE GRAPH main");
    run("DROP GRAPH other");
}

// An in-memory database keeps its graphs in memory too: CREATE GRAPH used to open a real file
// named after the graph (":g") in the working directory and DROP GRAPH never removed it.
class InMemoryGraphNoFileTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        originalCwd = std::filesystem::current_path();
        scratchDir = std::filesystem::temp_directory_path() /
                     ("riag_inmem_graph_" +
                         std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                         std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(scratchDir);
        std::filesystem::create_directories(scratchDir);
        std::filesystem::current_path(scratchDir);
    }
    void TearDown() override {
        std::filesystem::current_path(originalCwd);
        std::filesystem::remove_all(scratchDir);
    }
    std::vector<std::string> filesInScratchDir() const {
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(scratchDir)) {
            names.push_back(entry.path().filename().string());
        }
        return names;
    }
    std::filesystem::path originalCwd;
    std::filesystem::path scratchDir;
};

TEST_P(InMemoryGraphNoFileTest, CreateUseDropGraphLeavesNoFile) {
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 2;
    Database database(GetParam(), config);
    Connection connection(&database);
    auto run = [&](const std::string& query) {
        auto result = connection.query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    };
    run("CREATE GRAPH g");
    run("USE GRAPH g");
    run("CREATE NODE TABLE T(id INT64 PRIMARY KEY)");
    run("CREATE (:T {id: 1}), (:T {id: 2})");
    auto result = connection.query("MATCH (t:T) RETURN count(*)");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    ASSERT_TRUE(result->hasNext());
    EXPECT_EQ(result->getNext()->getValue(0)->toString(), "2");
    run("CHECKPOINT");
    EXPECT_TRUE(filesInScratchDir().empty());
    run("USE GRAPH main");
    run("DROP GRAPH g");
    EXPECT_TRUE(filesInScratchDir().empty());
}

INSTANTIATE_TEST_SUITE_P(InMemoryPaths, InMemoryGraphNoFileTest,
    ::testing::Values(std::string(""), std::string(":memory:")));

} // namespace testing
} // namespace riag
