#include <filesystem>
#include <string>

#include "graph_test/private_graph_test.h"
#include "main/connection.h"
#include "main/database.h"
#include "test_helper/test_helper.h"

using namespace riag::main;
using namespace riag::testing;

// A database whose string primary-key index was written before the string hash portability fix
// (e35ba2cff): fixtures/pre_portability_string_pk.db was created by the upstream 0.20.2 Python
// package on x86-64, with
//   CREATE NODE TABLE T(name STRING PRIMARY KEY, n INT64);
//   CREATE (:T {name: <name>, n: <i>})  for 'plain', 'Café', 'Zürich', '→', 'allen-p' (i = 0..4)
//   CHECKPOINT;
// The non-ASCII keys sit where the old hash put them, so the current hash misses them. Such a
// database still opens, so lookups and inserts of those keys must fail with the remedy instead of
// returning nothing and accepting a duplicate.
// riag-rename:skip-next upstream provenance
// LadybugDB/ladybug#1092
class StaleStringHashIndexTest : public EmptyDBTest {
protected:
    void SetUp() override {
        EmptyDBTest::SetUp();
        if (inMemMode) {
            GTEST_SKIP();
        }
        std::filesystem::copy_file(
            TestHelper::appendRiaGRootPath("test/storage/fixtures/pre_portability_string_pk.db"),
            databasePath, std::filesystem::copy_options::overwrite_existing);
        createDBAndConn();
    }

    void expectStale(const std::string& query) {
        auto result = conn->query(query);
        ASSERT_FALSE(result->isSuccess()) << query;
        EXPECT_NE(result->getErrorMessage().find("stale string primary-key hash index"),
            std::string::npos)
            << result->getErrorMessage();
    }

    std::string single(const std::string& query) {
        auto result = conn->query(query);
        EXPECT_TRUE(result->isSuccess()) << result->getErrorMessage();
        return TestHelper::convertResultToString(*result)[0];
    }
};

TEST_F(StaleStringHashIndexTest, AsciiKeysAreFound) {
    EXPECT_EQ(single("MATCH (t:T {name: 'plain'}) RETURN t.n;"), "0");
    EXPECT_EQ(single("MATCH (t:T {name: 'allen-p'}) RETURN t.n;"), "4");
    EXPECT_EQ(single("MATCH (t:T) RETURN count(*);"), "5");
}

TEST_F(StaleStringHashIndexTest, LookupOfMisplacedKeyFails) {
    expectStale("MATCH (t:T {name: 'Caf\xC3\xA9'}) RETURN t.n;");
    expectStale("MATCH (t:T) WHERE t.name = 'Z\xC3\xBCrich' RETURN t.n;");
    expectStale("MATCH (t:T {name: '\xE2\x86\x92'}) RETURN t.n;");
}

TEST_F(StaleStringHashIndexTest, DuplicateOfMisplacedKeyIsRejected) {
    expectStale("CREATE (:T {name: 'Caf\xC3\xA9', n: 99});");
    EXPECT_EQ(single("MATCH (t:T) RETURN count(*);"), "5");
}

TEST_F(StaleStringHashIndexTest, KeysAbsentUnderBothHashesStillMiss) {
    auto result = conn->query("MATCH (t:T {name: 'na\xC3\xAFve'}) RETURN t.n;");
    ASSERT_TRUE(result->isSuccess()) << result->getErrorMessage();
    EXPECT_FALSE(result->hasNext());
    ASSERT_TRUE(conn->query("CREATE (:T {name: 'na\xC3\xAFve', n: 5});")->isSuccess());
    EXPECT_EQ(single("MATCH (t:T {name: 'na\xC3\xAFve'}) RETURN t.n;"), "5");
}

// Keys written by this build use the current hash; the check must not misfire on them, before or
// after they reach the persistent index.
TEST_F(StaleStringHashIndexTest, NonAsciiKeysWrittenNowAreFound) {
    ASSERT_TRUE(conn->query("CREATE NODE TABLE U(name STRING PRIMARY KEY, n INT64);")->isSuccess());
    ASSERT_TRUE(conn->query("CREATE (:U {name: 'Caf\xC3\xA9', n: 1}), (:U {name: '\xE2\x86\x92', "
                            "n: 2});")
                    ->isSuccess());
    EXPECT_EQ(single("MATCH (u:U {name: 'Caf\xC3\xA9'}) RETURN u.n;"), "1");
    ASSERT_TRUE(conn->query("CHECKPOINT;")->isSuccess());
    EXPECT_EQ(single("MATCH (u:U {name: '\xE2\x86\x92'}) RETURN u.n;"), "2");
    EXPECT_FALSE(conn->query("CREATE (:U {name: 'Caf\xC3\xA9', n: 3});")->isSuccess());
}
