#pragma once

#include "c_api/riag.h"
#include "graph_test/base_graph_test.h"

namespace riag {
namespace testing {

// This class starts database in on-disk mode.
class APIDBTest : public BaseGraphTest {
public:
    void SetUp() override {
        BaseGraphTest::SetUp();
        createDBAndConn();
        initGraph();
    }
};

class CApiTest : public APIDBTest {
public:
    riag_database _database;
    riag_connection connection;

    void SetUp() override {
        APIDBTest::SetUp();
        auto* connCppPointer = conn.release();
        auto* databaseCppPointer = database.release();
        connection = riag_connection{connCppPointer};
        _database = riag_database{databaseCppPointer};
    }

    std::string getDatabasePath() { return databasePath; }

    riag_database* getDatabase() { return &_database; }

    riag_connection* getConnection() { return &connection; }

    void TearDown() override {
        riag_connection_destroy(&connection);
        riag_database_destroy(&_database);
        APIDBTest::TearDown();
    }
};

} // namespace testing
} // namespace riag
