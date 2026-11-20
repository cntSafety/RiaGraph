#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"
#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"

using namespace riag::main;

namespace riag {
namespace testing {

// Database paths cross the API as UTF-8 std::strings. On Windows std::filesystem::path's narrow
// constructor and string() use the process ANSI code page instead, so a path that went through
// them came back in that code page (or failed to convert). In a directory named with a non-ASCII
// character the first open, DDL and inserts worked, but CHECKPOINT failed to retire the frozen
// WAL and the database could not be reopened ("locale not supported" with libc++).
// The names below are written as escapes so that the test does not depend on the source
// encoding: "ü" and "ž" exist in code page 1252, the CJK and Cyrillic ones do not.
static std::filesystem::path pathFromUTF8(const std::u8string& utf8) {
    return std::filesystem::path(utf8);
}

static std::string toUTF8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

// A path for a Cypher string literal: forward slashes, so that a Windows path has no escapes.
static std::string toCypherUTF8(const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return std::string(utf8.begin(), utf8.end());
}

// (directory name, database file name)
using UnicodePathParam = std::tuple<std::u8string, std::u8string>;

class UnicodeDbPathTest : public ::testing::TestWithParam<UnicodePathParam> {
protected:
    void SetUp() override {
        rootDir = std::filesystem::temp_directory_path() /
                  ("riag_unicode_db_path_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::remove_all(rootDir);
        directory = rootDir / pathFromUTF8(std::get<0>(GetParam()));
        std::filesystem::create_directories(directory);
        dbFile = directory / pathFromUTF8(std::get<1>(GetParam()));
        dbPath = toUTF8(dbFile);
    }
    void TearDown() override {
        connection.reset();
        database.reset();
        std::error_code ec;
        std::filesystem::remove_all(rootDir, ec);
    }

    void open(const std::string& path) {
        connection.reset();
        database.reset();
        SystemConfig config;
        config.bufferPoolSize = 64 * 1024 * 1024;
        config.maxNumThreads = 2;
        database = std::make_unique<Database>(path, config);
        connection = std::make_unique<Connection>(database.get());
    }
    void open() { open(dbPath); }
    void close() {
        connection.reset();
        database.reset();
    }

    void run(const std::string& query) const {
        auto result = connection->query(query);
        ASSERT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
    }
    std::string scalar(const std::string& query) const {
        auto result = connection->query(query);
        EXPECT_TRUE(result->isSuccess()) << query << "\n" << result->getErrorMessage();
        if (!result->isSuccess() || !result->hasNext()) {
            return "";
        }
        return result->getNext()->getValue(0)->toString();
    }

    // Names of the files next to the database whose name starts with the database file's stem
    // followed by `infix`, excluding the database file itself.
    std::vector<std::string> siblings(const std::string& infix) const {
        const auto prefix = toUTF8(dbFile.stem()) + infix;
        const auto self = toUTF8(dbFile.filename());
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            auto name = toUTF8(entry.path().filename());
            if (name != self && name.starts_with(prefix)) {
                names.push_back(name);
            }
        }
        return names;
    }

    std::filesystem::path rootDir;
    std::filesystem::path directory;
    std::filesystem::path dbFile;
    std::string dbPath;
    std::unique_ptr<Database> database;
    std::unique_ptr<Connection> connection;
};

// The reproduction of the report: write, CHECKPOINT, close, reopen and read back.
TEST_P(UnicodeDbPathTest, CheckpointAndReopen) {
    open();
    run("CREATE NODE TABLE T(id SERIAL, v STRING, PRIMARY KEY(id))");
    run("CREATE (:T {v: 'a'})");
    run("CHECKPOINT");
    // Committed after the checkpoint: replayed from the WAL on reopen.
    run("CREATE (:T {v: 'b'})");
    close();
    // Only the database file and its WAL are left: the checkpoint retired its frozen WAL and
    // removed its shadow file.
    EXPECT_TRUE(std::filesystem::exists(dbFile));
    for (const auto& name : siblings("")) {
        EXPECT_FALSE(name.ends_with(".checkpoint") || name.ends_with(".shadow")) << name;
    }

    open();
    EXPECT_EQ(scalar("MATCH (t:T) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (t:T) WHERE t.id = 0 RETURN t.v"), "a");
    EXPECT_EQ(scalar("MATCH (t:T) WHERE t.id = 1 RETURN t.v"), "b");
    run("CREATE (:T {v: 'c'})");
    run("CHECKPOINT");
    close();

    open();
    EXPECT_EQ(scalar("MATCH (t:T) RETURN count(*)"), "3");
}

// Graph and partition-child files are named after the database file and removed through the same
// allow-list as the WAL.
TEST_P(UnicodeDbPathTest, GraphAndPartitionChildFiles) {
    open();
    run("CREATE GRAPH g");
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY, k STRING) PARTITION BY HASH (k) PARTITIONS 2");
    run("UNWIND range(1, 10) AS i CREATE (:P {id: i, k: cast(i AS STRING)})");
    run("CHECKPOINT");
    EXPECT_FALSE(siblings(".g").empty()) << "the graph has a file";
    EXPECT_FALSE(siblings(".P_p").empty()) << "the partition children have files";
    close();

    open();
    EXPECT_EQ(scalar("MATCH (p:P) RETURN count(*)"), "10");
    run("DROP GRAPH g");
    run("DROP TABLE P");
    run("CHECKPOINT");
    EXPECT_TRUE(siblings(".g").empty());
    EXPECT_TRUE(siblings(".P_p").empty());
    close();

    open();
    run("CREATE GRAPH g");
    run("DROP GRAPH g");
}

TEST_P(UnicodeDbPathTest, ExportAndImportDatabase) {
    open();
    run("CREATE NODE TABLE T(id INT64 PRIMARY KEY, v STRING)");
    run("CREATE (:T {id: 1, v: 'a'}), (:T {id: 2, v: 'b'})");
    const auto exportDir = directory / pathFromUTF8(u8"export ü世");
    run("EXPORT DATABASE '" + toCypherUTF8(exportDir) + "'");
    close();
    ASSERT_TRUE(std::filesystem::is_directory(exportDir));

    open(toUTF8(directory / pathFromUTF8(u8"imported ž.riag")));
    run("IMPORT DATABASE '" + toCypherUTF8(exportDir) + "'");
    EXPECT_EQ(scalar("MATCH (t:T) RETURN count(*)"), "2");
    EXPECT_EQ(scalar("MATCH (t:T) WHERE t.id = 2 RETURN t.v"), "b");
    run("CHECKPOINT");
}

// COPY FROM resolves its file through glob, with and without a wildcard.
TEST_P(UnicodeDbPathTest, CopyFromFilesInDirectory) {
    for (const auto* name : {u8"rows ä 1.csv", u8"rows ä 2.csv"}) {
        std::ofstream file(directory / pathFromUTF8(name));
        file << "id,v\n" << (std::u8string(name).ends_with(u8"1.csv") ? "1,a\n" : "2,b\n");
    }
    open();
    run("CREATE NODE TABLE One(id INT64 PRIMARY KEY, v STRING)");
    run("CREATE NODE TABLE Many(id INT64 PRIMARY KEY, v STRING)");
    run("COPY One FROM '" + toCypherUTF8(directory / pathFromUTF8(u8"rows ä 1.csv")) +
        "' (HEADER=true)");
    EXPECT_EQ(scalar("MATCH (t:One) RETURN count(*)"), "1");
    run("COPY Many FROM '" + toCypherUTF8(directory / pathFromUTF8(u8"rows ä *.csv")) +
        "' (HEADER=true)");
    EXPECT_EQ(scalar("MATCH (t:Many) RETURN count(*)"), "2");
}

INSTANTIATE_TEST_SUITE_P(UnicodePaths, UnicodeDbPathTest,
    ::testing::Values(UnicodePathParam{u8"dir üž", u8"db.riag"},
        UnicodePathParam{u8"dir üž", u8"dätén.riag"}, UnicodePathParam{u8"世界 Ø", u8"база.riag"}));

} // namespace testing
} // namespace riag
