#include <thread>

#include "common/timer.h"
#include "gtest/gtest.h"

using namespace riag::common;

// The query timeout is computed from Timer. On a wall clock that is stepped backwards, the
// elapsed time of a query that started just before the step comes out negative, wraps to a
// huge uint64_t, and the query is interrupted at once. Only a monotonic clock rules that out,
// and a clock step cannot be provoked from a test, so the property is asserted on the type.
TEST(TimerTest, UsesMonotonicClock) {
    static_assert(Timer::Clock::is_steady, "Timer must measure with a monotonic clock");
}

TEST(TimerTest, ElapsedTimeIsNonDecreasing) {
    Timer timer;
    timer.start();
    uint64_t previous = 0;
    for (auto i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto elapsed = timer.getElapsedTimeInMS();
        EXPECT_GE(elapsed, previous);
        // A wrapped negative duration shows up as an absurdly large value.
        EXPECT_LT(elapsed, 60'000u);
        previous = elapsed;
    }
    timer.stop();
    EXPECT_GE(timer.getDuration(), 0.0);
}
