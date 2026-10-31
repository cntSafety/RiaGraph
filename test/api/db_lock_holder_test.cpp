// POSIX only: the test needs fork() and POSIX record locks. (db_locking_test.cpp, which also forks,
// is not part of the build.)
#if !defined(_WIN32) && !defined(__WASM__)

#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "api_test/api_test.h"

using namespace riag::testing;

// ApiTest::SetUp would open the database in this process; the lock holder must be the child.
class DBLockHolderTest : public ApiTest {
    void SetUp() override { BaseGraphTest::SetUp(); }
};

// When opening is refused because another process holds the lock, the error must name that
// process. LocalFileSystem::openFile used to close the descriptor before asking fcntl(F_GETLK) who
// held the lock, so the query always failed with EBADF and the PID was never reported.
TEST_F(DBLockHolderTest, LockErrorNamesHolderPid) {
    if (databasePath == "" || databasePath == ":memory:") {
        return;
    }
    auto* ready = (uint64_t*)mmap(NULL, sizeof(uint64_t), PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    ASSERT_NE(ready, MAP_FAILED);
    *ready = 0;
    pid_t pid = fork();
    ASSERT_NE(pid, -1);
    if (pid == 0) {
        // Child: hold the write lock until killed. Never return into the test runner.
        systemConfig->readOnly = false;
        try {
            createDBAndConn();
        } catch (...) {
            _exit(1);
        }
        *ready = 1;
        while (true) {
            usleep(100);
        }
    }
    // Wait for the child to hold the lock, but stop if it died instead.
    while (*ready == 0 && waitpid(pid, NULL, WNOHANG) == 0) {
        usleep(100);
    }
    ASSERT_EQ(*ready, 1u) << "child could not open the database";

    // Opening for writing conflicts with the child's write lock, so it must be refused, naming the
    // child.
    const std::string expected = "Lock is held by PID " + std::to_string(pid);
    systemConfig->readOnly = false;
    std::string message;
    try {
        createDBAndConn();
    } catch (const std::exception& e) {
        message = e.what();
    }
    EXPECT_NE(message.find(expected), std::string::npos) << "error was: \"" << message << "\"";
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    munmap(ready, sizeof(uint64_t));
}

#endif
