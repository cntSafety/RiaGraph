#include <algorithm>
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

// The storage files of a graph and of a partition child are named after the database file:
// <stem>.<name><ext> ("db.g.riag" for "db.riag", but plain "db.g" for "db"). The allow-list that
// guards file removal used to compare the child's extension with the database's, so a database
// path without an extension had its graph and partition-child files refused: DROP GRAPH failed,
// the CHECKPOINT after dropping a partitioned table failed, and the child files were orphaned.
// The extension-bearing paths ride along as the boundary that always worked.
class DbPathNamingTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {
        scratchDir = std::filesystem::temp_directory_path() /
                     ("riag_dbpath_naming_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(scratchDir);
        std::filesystem::create_directories(scratchDir);
        dbPath = (scratchDir / GetParam()).string();
        stem = std::filesystem::path(GetParam()).stem().string();
    }
    void TearDown() override { std::filesystem::remove_all(scratchDir); }

    std::unique_ptr<Database> open() const {
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        return std::make_unique<Database>(dbPath, config);
    }
    // Names (not paths) of the files in the scratch directory that start with the given prefix.
    std::vector<std::string> filesStartingWith(const std::string& prefix) const {
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(scratchDir)) {
            auto name = entry.path().filename().string();
            if (name.starts_with(prefix)) {
                names.push_back(name);
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }
    static std::string joined(const std::vector<std::string>& names) {
        std::string result;
        for (const auto& name : names) {
            result += name + " ";
        }
        return result;
    }

    std::filesystem::path scratchDir;
    std::string dbPath;
    std::string stem;
};

static void run(Connection& connection, const std::string& query) {
    auto result = connection.query(query);
    ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
}

static std::string firstValue(Connection& connection, const std::string& query) {
    auto result = connection.query(query);
    EXPECT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    EXPECT_TRUE(result->hasNext());
    return result->getNext()->getValue(0)->toString();
}

static void createPartitionedTable(Connection& connection) {
    run(connection,
        "CREATE NODE TABLE T(id INT64 PRIMARY KEY, k STRING) PARTITION BY HASH (k) PARTITIONS 2");
    run(connection, "UNWIND range(1, 10) AS i CREATE (:T {id: i, k: cast(i AS STRING)})");
}

TEST_P(DbPathNamingTest, DropGraphRemovesGraphFiles) {
    {
        auto database = open();
        Connection connection(database.get());
        run(connection, "CREATE GRAPH g");
        run(connection, "CHECKPOINT");
        ASSERT_FALSE(filesStartingWith(stem + ".g").empty()) << "the graph has a file";
        run(connection, "DROP GRAPH g");
        run(connection, "CHECKPOINT");
        EXPECT_TRUE(filesStartingWith(stem + ".g").empty())
            << joined(filesStartingWith(stem + ".g"));
    }
    // The drop is durable: the name is free again after a reopen.
    auto database = open();
    Connection connection(database.get());
    run(connection, "CREATE GRAPH g");
    run(connection, "DROP GRAPH g");
    EXPECT_TRUE(filesStartingWith(stem + ".g").empty());
}

TEST_P(DbPathNamingTest, DropPartitionedTableRemovesChildFiles) {
    {
        auto database = open();
        Connection connection(database.get());
        createPartitionedTable(connection);
        run(connection, "CHECKPOINT");
        ASSERT_FALSE(filesStartingWith(stem + ".T_p").empty()) << "the children have files";
        run(connection, "DROP TABLE T");
        run(connection, "CHECKPOINT");
        EXPECT_TRUE(filesStartingWith(stem + ".T_p").empty())
            << joined(filesStartingWith(stem + ".T_p"));
    }
    // A table of the same name starts empty: it did not pick up an orphaned child.
    auto database = open();
    Connection connection(database.get());
    createPartitionedTable(connection);
    EXPECT_EQ(firstValue(connection, "MATCH (t:T) RETURN count(*)"), "10");
    run(connection, "DROP TABLE T");
    run(connection, "CHECKPOINT");
    EXPECT_TRUE(filesStartingWith(stem + ".T_p").empty());
}

TEST_P(DbPathNamingTest, RenamedPartitionedTableDropsAfterReopen) {
    {
        auto database = open();
        Connection connection(database.get());
        createPartitionedTable(connection);
        run(connection, "CHECKPOINT");
        run(connection, "ALTER TABLE T RENAME TO U");
        run(connection, "CHECKPOINT");
    }
    auto database = open();
    Connection connection(database.get());
    EXPECT_EQ(firstValue(connection, "MATCH (u:U) RETURN count(*)"), "10");
    run(connection, "DROP TABLE U");
    run(connection, "CHECKPOINT");
    // Whatever the children are called after a rename, none is left behind.
    EXPECT_TRUE(filesStartingWith(stem + ".U_p").empty())
        << joined(filesStartingWith(stem + ".U_p"));
    EXPECT_TRUE(filesStartingWith(stem + ".T_p").empty())
        << joined(filesStartingWith(stem + ".T_p"));
}

// With the checkpoint triggered by the commit itself, the failure used to surface on the DROP
// TABLE as "post-commit checkpoint failed".
TEST_P(DbPathNamingTest, AutoCheckpointAfterDropPartitionedTable) {
    auto database = open();
    Connection connection(database.get());
    createPartitionedTable(connection);
    run(connection, "CHECKPOINT");
    run(connection, "CALL checkpoint_threshold=1");
    run(connection, "DROP TABLE T");
    EXPECT_TRUE(filesStartingWith(stem + ".T_p").empty())
        << joined(filesStartingWith(stem + ".T_p"));
}

INSTANTIATE_TEST_SUITE_P(DbPaths, DbPathNamingTest,
    ::testing::Values(std::string("db"), std::string("db.riag"), std::string("my.db"),
        std::string("data.kuzu")));

} // namespace testing
} // namespace riag
