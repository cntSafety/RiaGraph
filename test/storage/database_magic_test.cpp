#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "graph_test/private_graph_test.h"
#include "main/connection.h"
#include "main/database.h"
#include "storage/storage_version_info.h"

using namespace riag::common;
using namespace riag::main;
using namespace riag::storage;

namespace riag {
namespace testing {

// RiaGraph writes the RIAG header magic and accepts the magic written before the fork, so an
// existing ladybug database opens in place. // riag-rename:skip upstream name
// The exchange is deliberately one-way: once RiaGraph checkpoints, the header carries RIAG and
// the older build can no longer read the file.
class DatabaseMagicTest : public EmptyDBTest {
protected:
    void SetUp() override {
        EmptyDBTest::SetUp();
        createDBAndConn();
        ASSERT_TRUE(conn->query("CREATE NODE TABLE T(id INT64 PRIMARY KEY);")->isSuccess());
        // Checkpoint so a header is actually on disk before we go looking for it.
        ASSERT_TRUE(conn->query("CHECKPOINT;")->isSuccess());
        closeDB();
    }

    void closeDB() {
        conn.reset();
        database.reset();
    }

    // The magic is the first field of the database header, which lives at the start of page 0.
    // writeDebuggingInfo() is compiled out unless DESER_DEBUG is defined, so nothing precedes it.
    std::string readMagic() const {
        std::ifstream file(databasePath, std::ios::binary);
        EXPECT_TRUE(file.is_open()) << "cannot open " << databasePath;
        std::vector<char> buffer(StorageVersionInfo::NUM_MAGIC_BYTES, '\0');
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        EXPECT_EQ(file.gcount(), static_cast<std::streamsize>(buffer.size()));
        return std::string(buffer.begin(), buffer.end());
    }

    // Overwrite the header magic in place. Every other byte is untouched, which is the point:
    // the layout is identical across the fork, only these four bytes differ.
    void writeMagic(const char* magic) {
        std::fstream file(databasePath, std::ios::binary | std::ios::in | std::ios::out);
        ASSERT_TRUE(file.is_open()) << "cannot open " << databasePath;
        file.seekp(0);
        file.write(magic, static_cast<std::streamsize>(StorageVersionInfo::NUM_MAGIC_BYTES));
        file.flush();
        ASSERT_TRUE(file.good());
    }

    void assertTableIsReadable() {
        auto db = std::make_unique<Database>(databasePath, *systemConfig);
        auto connection = std::make_unique<Connection>(db.get());
        auto result = connection->query("MATCH (t:T) RETURN count(t);");
        ASSERT_TRUE(result->isSuccess()) << result->toString();
    }
};

TEST_F(DatabaseMagicTest, WritesCurrentMagic) {
    ASSERT_EQ(readMagic(), std::string(StorageVersionInfo::MAGIC_BYTES));
}

TEST_F(DatabaseMagicTest, OpensDatabaseWithCurrentMagic) {
    assertTableIsReadable();
}

TEST_F(DatabaseMagicTest, OpensDatabaseWithLegacyMagic) {
    writeMagic(StorageVersionInfo::LEGACY_MAGIC_BYTES);
    ASSERT_EQ(readMagic(), std::string(StorageVersionInfo::LEGACY_MAGIC_BYTES));
    assertTableIsReadable();
}

TEST_F(DatabaseMagicTest, UpgradeFromLegacyMagicIsOneWay) {
    writeMagic(StorageVersionInfo::LEGACY_MAGIC_BYTES);
    {
        auto db = std::make_unique<Database>(databasePath, *systemConfig);
        auto connection = std::make_unique<Connection>(db.get());
        ASSERT_TRUE(connection->query("CREATE (:T {id: 1});")->isSuccess());
        ASSERT_TRUE(connection->query("CHECKPOINT;")->isSuccess());
    }
    // Serialization always writes MAGIC_BYTES, so the file is no longer readable by the
    // release that wrote it.
    ASSERT_EQ(readMagic(), std::string(StorageVersionInfo::MAGIC_BYTES));
}

} // namespace testing
} // namespace riag
