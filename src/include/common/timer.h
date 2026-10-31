#pragma once

#include <chrono>
#include <string>

#include "common/assert.h"
#include "exception/exception.h"

namespace riag {
namespace common {

class Timer {

public:
    // Durations and query timeouts must come from a monotonic clock. high_resolution_clock is
    // system_clock in libstdc++, i.e. wall time: when the clock is stepped backwards (NTP, or a
    // Docker Desktop VM resyncing with its host, which steps it by about a second every 30 s),
    // now - startTime goes negative, getElapsedTimeInMS() wraps it to a huge uint64_t, and the
    // task scheduler interrupts a query that has barely started ("Interrupted.").
    using Clock = std::chrono::steady_clock;

    void start() {
        finished = false;
        startTime = Clock::now();
    }

    void stop() {
        stopTime = Clock::now();
        finished = true;
    }

    double getDuration() const {
        if (finished) {
            auto duration = stopTime - startTime;
            return (double)std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        }
        throw Exception("Timer is still running.");
    }

    uint64_t getElapsedTimeInMS() const {
        auto now = Clock::now();
        auto duration = now - startTime;
        auto count = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        DASSERT(count >= 0);
        return count;
    }

    double getElapsedTimeMS() const {
        auto now = Clock::now();
        auto duration = now - startTime;
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
    }

private:
    std::chrono::time_point<Clock> startTime;
    std::chrono::time_point<Clock> stopTime;
    bool finished = false;
};

} // namespace common
} // namespace riag
