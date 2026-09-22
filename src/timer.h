#ifndef TIMER_H
#define TIMER_H

#include <chrono>   // steady_clock, duration_cast
#include <cstdint>  // uint64_t

// Times a block of code, reporting elapsed time in whatever unit the caller asks for.
// The clock starts running as soon as the Timer is constructed.
class Timer {
 public:
    // The standard <chrono> durations we let callers name.
    using Nanos = std::chrono::nanoseconds;
    using Micros = std::chrono::microseconds;
    using Millis = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    using Hours = std::chrono::hours;

    Timer() : mLast(std::chrono::steady_clock::now()) {}

    // Makes it look like the timer was just constructed.
    void restart() { mLast = std::chrono::steady_clock::now(); }

    // Elapsed time since the last click/restart/construction, and starts a new interval.
    template <typename T>
    uint64_t click() {
        auto now = std::chrono::steady_clock::now();
        uint64_t elapsed = static_cast<uint64_t>(std::chrono::duration_cast<T>(now - mLast).count());
        mLast = now;
        return elapsed;
    }

    // Same measurement as click(), but leaves the current interval running.
    template <typename T>
    uint64_t glance() const {
        auto now = std::chrono::steady_clock::now();
        return static_cast<uint64_t>(std::chrono::duration_cast<T>(now - mLast).count());
    }

 private:
    std::chrono::steady_clock::time_point mLast;
};

#endif  // TIMER_H
