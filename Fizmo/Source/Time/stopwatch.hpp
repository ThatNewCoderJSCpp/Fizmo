#ifndef FIZMO_STOPWATCH_CLASS_HPP
#define FIZMO_STOPWATCH_CLASS_HPP

#include "wait_sleep.hpp"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

namespace fizmo {
namespace time {

class Stopwatch {
private:
#ifdef OS_WINDOWS
    LARGE_INTEGER start_time_;
    LARGE_INTEGER stop_time_;
    LARGE_INTEGER frequency_;
    LARGE_INTEGER accumulated_time_; 
#endif
    bool is_running_;
    bool has_been_started_;

public:
    Stopwatch() noexcept : is_running_(false) , has_been_started_(false) {
    #ifdef OS_WINDOWS
        QueryPerformanceFrequency(&frequency_);
        start_time_.QuadPart = 0;
        stop_time_.QuadPart = 0;
        accumulated_time_.QuadPart = 0;
    #endif
    }

    void start() noexcept {
        if (!is_running_) {
        #ifdef OS_WINDOWS
            LARGE_INTEGER current_time;
            QueryPerformanceCounter(&current_time);
            
            if (has_been_started_) {
                start_time_.QuadPart = current_time.QuadPart - accumulated_time_.QuadPart;
            } else {
                start_time_ = current_time;
                accumulated_time_.QuadPart = 0;
            }
        #endif
            is_running_ = true;
            has_been_started_ = true;
        }
    }

    void stop() noexcept {
        if (is_running_) {
        #ifdef OS_WINDOWS
            QueryPerformanceCounter(&stop_time_);
            accumulated_time_.QuadPart = stop_time_.QuadPart - start_time_.QuadPart;
        #endif
            is_running_ = false;
        }
    }

    void reset() noexcept {
    #ifdef OS_WINDOWS
        start_time_.QuadPart = 0;
        stop_time_.QuadPart = 0;
        accumulated_time_.QuadPart = 0;
    #endif
        is_running_ = false;
        has_been_started_ = false;
    }

    void restart() noexcept {
        reset();
        start();
    }

    constexpr bool is_running() const noexcept { return is_running_; }
    constexpr bool has_started() const noexcept { return has_been_started_; }

public:
    std::uint64_t elapsed_ticks() const noexcept {
        if (!has_been_started_) { return 0; }

    #ifdef OS_WINDOWS
        LARGE_INTEGER end_time;

        if (is_running_) {
            QueryPerformanceCounter(&end_time);
        } else {
            end_time = stop_time_;
        }

        return static_cast<std::uint64_t>(end_time.QuadPart - start_time_.QuadPart);
    #else
        return 0;
    #endif
    }

    nanosecond elapsed_nanoseconds() const noexcept {
    #ifdef OS_WINDOWS
        const std::uint64_t ticks = elapsed_ticks();
        if (frequency_.QuadPart == 0) { return nanosecond(0); }
        const std::uint64_t ns_per_tick = 1000000000ULL / frequency_.QuadPart;
        const std::uint64_t remainder = 1000000000ULL % frequency_.QuadPart;
        const std::uint64_t ns = ticks * ns_per_tick + (ticks * remainder) / frequency_.QuadPart;
        return nanosecond(ns);
    #else
        return nanosecond(0);
    #endif
    }

public:
    CompleteDuration elapsed() const noexcept { return CompleteDuration(elapsed_nanoseconds()); }

    CompleteDuration split() noexcept {
        CompleteDuration elapsed_time = elapsed();
        restart();
        return elapsed_time;
    }

public:
    operator Duration() const noexcept { return elapsed().to_duration(); }
    operator CompleteDuration() const noexcept { return elapsed(); }
    bool operator==(const Stopwatch& other) const noexcept { return elapsed_ticks() == other.elapsed_ticks(); }
    bool operator!=(const Stopwatch& other) const noexcept { return !(*this == other); }
    bool operator<(const Stopwatch& other) const noexcept { return elapsed_ticks() < other.elapsed_ticks(); }
    bool operator<=(const Stopwatch& other) const noexcept { return elapsed_ticks() <= other.elapsed_ticks(); }
    bool operator>(const Stopwatch& other) const noexcept { return elapsed_ticks() > other.elapsed_ticks(); }
    bool operator>=(const Stopwatch& other) const noexcept { return elapsed_ticks() >= other.elapsed_ticks(); }

public:
    std::string to_string(const bool only_numbers = true, const bool verbose = true) const {
        std::string result = "";
        
        if (!only_numbers) {
            if (!has_been_started_) { return "Stopwatch: Not started"; }
            const std::string status = is_running_ ? "Running" : "Stopped";
            result = "Stopwatch (" + status + "): ";
        }

        const CompleteDuration& cd = elapsed();
        std::string time_str = cd.to_string(verbose);
        
        if (time_str.empty() || (verbose && time_str == "0 nanoseconds") || (!verbose && time_str == "0 ns")) {
            result += verbose ? "0 nanoseconds" : "0 ns";
        } else {
            result += time_str;
        }
        
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const Stopwatch& sw) {
        os << sw.to_string();
        return os;
    }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_STOPWATCH_CLASS_HPP