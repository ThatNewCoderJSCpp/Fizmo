#ifndef FIZMO_TIME_STOPWATCH_HPP
#define FIZMO_TIME_STOPWATCH_HPP

#include "ticks.hpp"
#include "duration.hpp"
#include "time_unit.hpp"

namespace fizmo {
namespace time {

template<typename Int = default_wide_int, typename = typename std::enable_if<detail::is_signed_integer_like_v<Int>>::type>
class Stopwatch {
public:
    using int_type      = Int;
    using float_type    = fizmo_float_from_int_t<Int>;
    using duration_ns   = Duration<Unit::nanosecond, Int>;
    using duration_ms   = Duration<Unit::millisecond, Int>;
    using duration_s    = Duration<Unit::second, Int>;
    using duration_type = duration_ns;

private:
#ifdef OS_WINDOWS
    LARGE_INTEGER start_;
    LARGE_INTEGER stop_;
    LARGE_INTEGER freq_;
    LARGE_INTEGER accum_;
#endif

    bool running_;
    bool started_;

public:
    Stopwatch() noexcept : running_(false), started_(false) {
    #ifdef OS_WINDOWS
        QueryPerformanceFrequency(&freq_);
        start_.QuadPart = 0;
        stop_.QuadPart  = 0;
        accum_.QuadPart = 0;
    #endif
    }

    void start() noexcept {
        if (!running_) {
    #ifdef OS_WINDOWS
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);

            if (started_) {
                start_.QuadPart = now.QuadPart - accum_.QuadPart;
            } else {
                start_ = now;
                accum_.QuadPart = 0;
            }
    #endif
            running_ = true;
            started_ = true;
        }
    }

    void stop() noexcept {
        if (running_) {
    #ifdef OS_WINDOWS
            QueryPerformanceCounter(&stop_);
            accum_.QuadPart = stop_.QuadPart - start_.QuadPart;
    #endif
            running_ = false;
        }
    }

    void reset() noexcept {
    #ifdef OS_WINDOWS
        start_.QuadPart = 0;
        stop_.QuadPart  = 0;
        accum_.QuadPart = 0;
    #endif
        running_ = false;
        started_ = false;
    }

    void restart() noexcept {
        reset();
        start();
    }

    constexpr bool is_running() const noexcept { return running_; }
    constexpr bool has_started() const noexcept { return started_; }

public:
    std::uint64_t elapsed_ticks() const noexcept {
    #ifdef OS_WINDOWS
        if (!started_) return 0;
        LARGE_INTEGER now;

        if (running_) {
            QueryPerformanceCounter(&now);
        } else {
            now = stop_;
        }

        return static_cast<std::uint64_t>(now.QuadPart - start_.QuadPart);
    #else
        return 0;
    #endif
    }

    duration_ns elapsed_nanoseconds() const noexcept {
    #ifdef OS_WINDOWS
        const std::uint64_t ticks = elapsed_ticks();
        if (freq_.QuadPart == 0) return duration_ns(0);
        const std::uint64_t ns = ticks_to_time<std::uint64_t>(ticks, Unit::nanosecond);
        return duration_ns(static_cast<Int>(ns));
    #else
        return duration_ns(0);
    #endif
    }

    duration_ms elapsed_milliseconds() const noexcept {
    #ifdef OS_WINDOWS
        const std::uint64_t ticks = elapsed_ticks();
        if (freq_.QuadPart == 0) return duration_ms(0);
        const std::uint64_t ms = ticks_to_time<std::uint64_t>(ticks, Unit::millisecond);
        return duration_ms(static_cast<Int>(ms));
    #else
        return duration_ms(0);
    #endif
    }

    duration_s elapsed_seconds() const noexcept {
    #ifdef OS_WINDOWS
        const std::uint64_t ticks = elapsed_ticks();
        if (freq_.QuadPart == 0) return duration_s(0);
        const std::uint64_t s = ticks_to_time<std::uint64_t>(ticks, Unit::second);
        return duration_s(static_cast<Int>(s));
    #else
        return duration_s(0);
    #endif
    }

    template<Unit U>
    Duration<U, Int> elapsed_as() const noexcept { return Duration<U, Int>(elapsed_nanoseconds()); }

public:
    duration_type elapsed() const noexcept { return elapsed_nanoseconds(); }

    duration_type split() noexcept {
        duration_type d = elapsed();
        restart();
        return d;
    }

public:
    operator duration_type() const noexcept { return elapsed(); }

    bool operator==(const Stopwatch& o) const noexcept { return elapsed_ticks() == o.elapsed_ticks(); }
    bool operator!=(const Stopwatch& o) const noexcept { return !(*this == o); }
    bool operator<(const Stopwatch& o)  const noexcept { return elapsed_ticks() <  o.elapsed_ticks(); }
    bool operator<=(const Stopwatch& o) const noexcept { return elapsed_ticks() <= o.elapsed_ticks(); }
    bool operator>(const Stopwatch& o)  const noexcept { return elapsed_ticks() >  o.elapsed_ticks(); }
    bool operator>=(const Stopwatch& o) const noexcept { return elapsed_ticks() >= o.elapsed_ticks(); }

public:
    std::string to_string(bool only_numbers = true, bool verbose = true) const {
        if (!started_) { return only_numbers ? "0" : "Stopwatch: Not started"; }
        duration_ns d = elapsed();
        Int ns = d.count();
        if (!verbose) { return std::to_string(ns) + " ns"; }
        float_type seconds = static_cast<float_type>(ns) / static_cast<float_type>(1000000000.0);
        std::ostringstream oss;
        if (!only_numbers) { oss << "Stopwatch (" << (running_ ? "Running" : "Stopped") << "): "; }
        oss << seconds << " seconds (" << ns << " ns)";
        return oss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const Stopwatch& sw) {
        return os << sw.to_string();
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_TIME_STOPWATCH_HPP