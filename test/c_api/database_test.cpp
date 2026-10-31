#include "c_api/riag.h"
#include "graph_test/base_graph_test.h"
#include "gtest/gtest.h"

using namespace riag::main;
using namespace riag::testing;

// This class starts database without initializing graph.
class APIEmptyDBTest : public BaseGraphTest {
    std::string getInputDir() override { UNREACHABLE_CODE; }
};

class CApiDatabaseTest : public APIEmptyDBTest {
public:
    void SetUp() override {
        APIEmptyDBTest::SetUp();
        defaultSystemConfig = riag_default_system_config();

        // limit memory usage by keeping max number of threads small
        defaultSystemConfig.max_num_threads = 2;
        auto maxDBSizeEnv = TestHelper::getSystemEnv("MAX_DB_SIZE");
        if (!maxDBSizeEnv.empty()) {
            defaultSystemConfig.max_db_size = std::stoull(maxDBSizeEnv);
        }
    }

    riag_system_config defaultSystemConfig;
};

TEST_F(CApiDatabaseTest, CreationAndDestroy) {
    riag_database database;
    riag_state state;
    auto databasePathCStr = databasePath.c_str();
    auto systemConfig = defaultSystemConfig;
    state = riag_database_init(databasePathCStr, systemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);
    auto databaseCpp = static_cast<Database*>(database._database);
    ASSERT_NE(databaseCpp, nullptr);
    riag_database_destroy(&database);
}

TEST_F(CApiDatabaseTest, CreationReadOnly) {
    riag_database database;
    riag_connection connection;
    riag_query_result queryResult;
    riag_state state;
    auto databasePathCStr = databasePath.c_str();
    auto systemConfig = defaultSystemConfig;
    // First, create a read-write database.
    state = riag_database_init(databasePathCStr, systemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);
    auto databaseCpp = static_cast<Database*>(database._database);
    ASSERT_NE(databaseCpp, nullptr);
    riag_database_destroy(&database);
    // Now, access the same database read-only.
    systemConfig.read_only = true;
    state = riag_database_init(databasePathCStr, systemConfig, &database);
    if (databasePath == "" || databasePath == ":memory:") {
        ASSERT_EQ(state, RiaGError);
        ASSERT_EQ(database._database, nullptr);
        return;
    }
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);
    databaseCpp = static_cast<Database*>(database._database);
    ASSERT_NE(databaseCpp, nullptr);
    // Try to write to the database.
    state = riag_connection_init(&database, &connection);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_connection_query(&connection,
        "CREATE NODE TABLE User(name STRING, age INT64, reg_date DATE, PRIMARY KEY (name))",
        &queryResult);
    ASSERT_EQ(state, RiaGError);
    ASSERT_FALSE(riag_query_result_is_success(&queryResult));
    riag_query_result_destroy(&queryResult);
    riag_connection_destroy(&connection);
    riag_database_destroy(&database);
}

TEST_F(CApiDatabaseTest, CreationInMemory) {
    riag_database database;
    riag_state state;
    auto databasePathCStr = (char*)"";
    state = riag_database_init(databasePathCStr, defaultSystemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    riag_database_destroy(&database);
    databasePathCStr = (char*)":memory:";
    state = riag_database_init(databasePathCStr, defaultSystemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    riag_database_destroy(&database);
}

TEST_F(CApiDatabaseTest, CreationWithEnableMultiWrites) {
    riag_database database;
    auto systemConfig = defaultSystemConfig;
    systemConfig.enable_multi_writes = true;

    auto state = riag_database_init(databasePath.c_str(), systemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);

    auto databaseCpp = static_cast<Database*>(database._database);
    ASSERT_TRUE(databaseCpp->isMultiWritesEnabled());
    riag_database_destroy(&database);
}

TEST_F(CApiDatabaseTest, CreationInUnicodeDirectory) {
    // Database paths cross the C API as UTF-8. The file system must resolve them as
    // Unicode on Windows rather than via the process ANSI code page.
    const auto unicodeDir = std::filesystem::path(databasePath).parent_path() /
                            std::filesystem::path(std::u8string(u8"riag_Øresund_世界"));
    std::error_code ec;
    std::filesystem::create_directories(unicodeDir, ec);
    ASSERT_FALSE(ec) << ec.message();
    const auto dbUTF8 = (unicodeDir / "test.db").u8string();
    const std::string dbPath(dbUTF8.begin(), dbUTF8.end());
    riag_database database;
    auto state = riag_database_init(dbPath.c_str(), defaultSystemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);
    riag_connection connection;
    state = riag_connection_init(&database, &connection);
    ASSERT_EQ(state, RiaGSuccess);
    riag_query_result queryResult;
    state = riag_connection_query(&connection,
        "CREATE NODE TABLE Person(name STRING, PRIMARY KEY(name));", &queryResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&queryResult));
    riag_query_result_destroy(&queryResult);
    riag_connection_destroy(&connection);
    riag_database_destroy(&database);
    std::filesystem::remove_all(unicodeDir, ec);
    ASSERT_FALSE(ec) << ec.message();
}

#ifndef __WASM__ // home directory is not available in WASM
TEST_F(CApiDatabaseTest, CreationHomeDir) {
    riag_database database;
    riag_connection connection;
    riag_state state;
    auto databasePathCStr = (char*)"~/test.db";
    state = riag_database_init(databasePathCStr, defaultSystemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_connection_init(&database, &connection);
    ASSERT_EQ(state, RiaGSuccess);
    auto homePath =
        getClientContext(*(Connection*)(connection._connection))->getClientConfig()->homeDirectory;
    riag_connection_destroy(&connection);
    riag_database_destroy(&database);
    std::filesystem::remove_all(homePath + "/test.db");
}
#endif

TEST_F(CApiDatabaseTest, CloseQueryResultAndConnectionAfterDatabaseDestroy) {
    riag_database database;
    auto databasePathCStr = (char*)":memory:";
    auto systemConfig = riag_default_system_config();
    systemConfig.buffer_pool_size = 10 * 1024 * 1024; // 10MB
    systemConfig.max_db_size = 1 << 30;               // 1GB
    systemConfig.max_num_threads = 2;
    riag_state state = riag_database_init(databasePathCStr, systemConfig, &database);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_NE(database._database, nullptr);
    riag_connection conn;
    riag_query_result queryResult;
    state = riag_connection_init(&database, &conn);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_connection_query(&conn, "RETURN 1+1", &queryResult);
    ASSERT_EQ(state, RiaGSuccess);
    ASSERT_TRUE(riag_query_result_is_success(&queryResult));
    riag_flat_tuple tuple;
    riag_state resultState = riag_query_result_get_next(&queryResult, &tuple);
    ASSERT_EQ(resultState, RiaGSuccess);
    riag_value value;
    riag_state valueState = riag_flat_tuple_get_value(&tuple, 0, &value);
    ASSERT_EQ(valueState, RiaGSuccess);
    int64_t valueInt = INT64_MAX;
    riag_state valueIntState = riag_value_get_int64(&value, &valueInt);
    ASSERT_EQ(valueIntState, RiaGSuccess);
    ASSERT_EQ(valueInt, 2);
    // Destroy database first, this should not crash
    riag_database_destroy(&database);
    // Call riag_connection_query should not crash, but return an error
    state = riag_connection_query(&conn, "RETURN 1+1", &queryResult);
    ASSERT_EQ(state, RiaGError);
    // Call riag_query_result_get_next should not crash, but return an error
    resultState = riag_query_result_get_next(&queryResult, &tuple);
    ASSERT_EQ(resultState, RiaGError);
    // Now destroy everything, this should not crash
    riag_query_result_destroy(&queryResult);
    riag_connection_destroy(&conn);
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&tuple);
}

TEST_F(CApiDatabaseTest, UseConnectionAfterDatabaseDestroy) {
    riag_database db;
    riag_connection conn;
    riag_query_result result;

    auto systemConfig = riag_default_system_config();
    systemConfig.buffer_pool_size = 10 * 1024 * 1024; // 10MB
    systemConfig.max_db_size = 1 << 30;               // 1GB
    systemConfig.max_num_threads = 2;
    auto state = riag_database_init("", systemConfig, &db);
    ASSERT_EQ(state, RiaGSuccess);
    state = riag_connection_init(&db, &conn);
    ASSERT_EQ(state, RiaGSuccess);
    riag_database_destroy(&db);
    state = riag_connection_query(&conn, "RETURN 0", &result);
    ASSERT_EQ(state, RiaGError);

    riag_connection_destroy(&conn);
}
