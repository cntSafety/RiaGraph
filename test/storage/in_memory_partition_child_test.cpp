#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;

namespace riag {
namespace testing {

// An in-memory database keeps the storage of its partition children in memory too. The registry
// used to name a child ":<childName>", which is not recognised as an in-memory path, so the child
// opened a real file of that name (plus WAL and shadow files) in the working directory and DROP
// TABLE never removed it. The working directory must stay empty for the whole life of the table.
class InMemoryPartitionChildNoFileTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        originalCwd = std::filesystem::current_path();
        scratchDir = std::filesystem::temp_directory_path() /
                     ("riag_inmem_partition_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
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

TEST_P(InMemoryPartitionChildNoFileTest, PartitionedTableLeavesNoFile) {
    SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 2;
    Database database(GetParam(), config);
    Connection connection(&database);
    auto run = [&](const std::string& query) {
        auto result = connection.query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    };
    auto count = [&](const std::string& query) {
        auto result = connection.query(query);
        EXPECT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
        EXPECT_TRUE(result->hasNext());
        return result->getNext()->getValue(0)->toString();
    };
    run("CREATE NODE TABLE T(id INT64 PRIMARY KEY, k STRING) PARTITION BY HASH (k) "
        "PARTITIONS 3");
    // Write to every child: the child's storage manager is opened on first touch.
    run("CREATE (:T_p0 {id: 1, k: 'a'}), (:T_p1 {id: 2, k: 'b'}), (:T_p2 {id: 3, k: 'c'})");
    EXPECT_EQ(count("MATCH (t:T) RETURN count(*)"), "3");
    run("CHECKPOINT");
    EXPECT_EQ(count("MATCH (t:T) RETURN count(*)"), "3");
    EXPECT_TRUE(filesInScratchDir().empty());
    // Renaming the parent renames its children; there is no child file to move.
    run("ALTER TABLE T RENAME TO U");
    EXPECT_EQ(count("MATCH (u:U) RETURN count(*)"), "3");
    run("CHECKPOINT");
    EXPECT_EQ(count("MATCH (u:U) RETURN count(*)"), "3");
    EXPECT_TRUE(filesInScratchDir().empty());
    run("DROP TABLE U");
    EXPECT_TRUE(filesInScratchDir().empty());
}

INSTANTIATE_TEST_SUITE_P(InMemoryPaths, InMemoryPartitionChildNoFileTest,
    ::testing::Values(std::string(""), std::string(":memory:")));

} // namespace testing
} // namespace riag
