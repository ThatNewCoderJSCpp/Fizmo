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
    std::uint64_t start_;   
    std::uint64_t stop_;    
    std::uint64_t accum_;   
    bool running_;
    bool started_;

public:
    Stopwatch() noexcept : start_(0), stop_(0), accum_(0), running_(false), started_(false) {}

    void start() noexcept {
        if (running_) return;
        const std::uint64_t now = tick_counter();

        if (started_) {
            start_ = now - accum_;      
        } else {
            start_ = now;
            accum_ = 0;
        }

        running_ = true;
        started_ = true;
    }

    void stop() noexcept {
        if (!running_) return;
        stop_    = tick_counter();
        accum_   = stop_ - start_;
        running_ = false;
    }

    void reset() noexcept {
        start_   = 0;
        stop_    = 0;
        accum_   = 0;
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
        if (!started_) return 0;
        const std::uint64_t now = running_ ? tick_counter() : stop_;
        return now - start_;
    }

    duration_ns elapsed_nanoseconds() const noexcept {
        return duration_ns(static_cast<Int>(ticks_to_ns(elapsed_ticks())));
    }

    float_type exact_elapsed_nanoseconds() const noexcept {
        return float_type(ticks_to_ns(elapsed_ticks()));
    }

    duration_ms elapsed_milliseconds() const noexcept {
        return duration_ms(static_cast<Int>(ticks_to_ns(elapsed_ticks()) / 1000000ULL));
    }

    float_type exact_elapsed_milliseconds() const noexcept {
        return exact_elapsed_nanoseconds() / float_type(1000000ULL);
    }

    duration_s elapsed_seconds() const noexcept {
        return duration_s(static_cast<Int>(ticks_to_ns(elapsed_ticks()) / 1000000000ULL));
    }

    float_type exact_elapsed_seconds() const noexcept {
        return exact_elapsed_milliseconds() / float_type(1000ULL);
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