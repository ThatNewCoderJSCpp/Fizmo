#ifndef FIZMO_CLOCK_CLASS_HPP
#define FIZMO_CLOCK_CLASS_HPP

#include "time_point_util.hpp"
#include <chrono>

#ifdef OS_WINDOWS
#include <windows.h>
#endif

namespace fizmo {
namespace time {

class Clock {
public:
    enum class type {
        system,     // Wall clock time (can jump)
        steady,     // Monotonic clock (never goes backwards)
        high_res   // Highest resolution available
    };

private:
    static constexpr TimePoint system_epoch_ = TimePoint::epoch();
    static constexpr TimePoint windows_epoch_ = TimePoint(Date(1601, months::January, 1));
    static constexpr TimePoint unix_epoch_ = TimePoint(Date(1970, months::January, 1));

public:
    static TimePoint now(type clock_type = type::system) noexcept {
        switch (clock_type) {
            case type::system:
                return system_now();
            case type::steady:
                return steady_now();
            case type::high_res:
                return high_resolution_now();
            default:
                return system_now();
        }
    }

    template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    static T time_since_epoch(type clock_type = type::system) noexcept { return static_cast<T>(now(clock_type)); }

    static constexpr bool is_steady(type clock_type) noexcept { return clock_type == type::steady || clock_type == type::high_res; }

    static CompleteDuration resolution(type clock_type = type::system) noexcept {
        switch (clock_type) {
            case type::system:
                return system_resolution();
            case type::steady:
                return steady_resolution();
            case type::high_res:
                return high_resolution_resolution();
            default:
                return system_resolution();
        }
    }

    static CompleteDuration elapsed(const TimePoint& start, const TimePoint& end) noexcept {
        if (end >= start) { return static_cast<CompleteDuration>(end - start); }
        return CompleteDuration();
    }

    static CompleteDuration uptime() noexcept {
    #ifdef OS_WINDOWS
        ULONGLONG ticks = GetTickCount64();
        return CompleteDuration(millisecond(ticks));
    #else
        return CompleteDuration();
    #endif
    }

    static void sleep_until(const TimePoint& target_time, type clock_type = type::system) {
        TimePoint current = now(clock_type);
        if (target_time > current) {
            CompleteDuration sleep_duration = static_cast<CompleteDuration>(target_time - current);
            sleep(sleep_duration);
        }
    }

    static void wait_until(const TimePoint& target_time, type clock_type = type::system) {
        TimePoint current = now(clock_type);
        if (target_time > current) {
            CompleteDuration sleep_duration = static_cast<CompleteDuration>(target_time - current);
            wait(sleep_duration);
        }
    }

    template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    static void sleep_for(const T duration) { sleep(duration); }

    static void sleep_for(const CompleteDuration& duration) { sleep(duration); }
    static void sleep_for(DURATION_PARAM duration) { sleep(duration); }

    template<typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
    static void wait_for(const T duration) { wait(duration); }

    static void wait_for(const CompleteDuration& duration) { wait(duration); }
    static void wait_for(DURATION_PARAM duration) { wait(duration); }

    template<typename Func>
    static CompleteDuration benchmark(Func&& func) {
        TimePoint start = now(type::high_res);
        func();
        TimePoint end = now(type::high_res);
        return elapsed(start, end);
    }

    class ScopedTimer {
    private:
        TimePoint start_time_;
        type clock_type_;
        CompleteDuration* result_;
        
    public:
        explicit ScopedTimer(type clock_type = type::high_res, CompleteDuration* result = nullptr) : start_time_(Clock::now(clock_type)), clock_type_(clock_type), result_(result) {}
        
        ~ScopedTimer() {
            CompleteDuration elapsed_time = Clock::elapsed(start_time_, Clock::now(clock_type_));
            if (result_) { *result_ = elapsed_time; }
        }
        
        CompleteDuration elapsed() const { return Clock::elapsed(start_time_, Clock::now(clock_type_)); }
    };

private:
    static TimePoint system_now() noexcept {
    #ifdef OS_WINDOWS
        return 2.0;
    #else
        return TimePoint();
    #endif
    }

    static TimePoint steady_now() noexcept {
    #ifdef OS_WINDOWS
        return 3.0;
    #else
        return TimePoint(); 
    #endif
    }

    static TimePoint high_resolution_now() noexcept { 
    #ifdef OS_WINDOWS
        return steady_now();    
    #else
        return TimePoint(); 
    #endif
    }

    static CompleteDuration system_resolution() noexcept {
    #ifdef OS_WINDOWS
        DWORD time_adjustment, time_increment;
        BOOL time_adjustment_disabled;
        
        if (GetSystemTimeAdjustment(&time_adjustment, &time_increment, &time_adjustment_disabled)) {
            const nanosecond resolution_ns = nanosecond(static_cast<std::uint64_t>(time_increment) * 100);
            return CompleteDuration(resolution_ns);
        } else {
            constexpr nanosecond default_resolution_ns = nanosecond(15625000); 
            return CompleteDuration(default_resolution_ns);
        }
    #else
        return CompleteDuration(); 
    #endif
    }

    static CompleteDuration steady_resolution() noexcept { return CompleteDuration(time_per_tick()); }
    static CompleteDuration high_resolution_resolution() noexcept { return steady_resolution();  }
};

} // namespace time
} // namespace fizmo

#endif // FIZMO_CLOCK_CLASS_HPP