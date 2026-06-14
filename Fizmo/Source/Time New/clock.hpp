#ifndef FIZMO_CLOCK_HPP
#define FIZMO_CLOCK_HPP

#include "calendar_classes.hpp"
#include "ticks.hpp"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

namespace fizmo {
namespace temp_time {

template<typename T = multiprecision::int256, typename = typename std::enable_if<detail::is_signed_int_like_v<T>>::type>
class Clock {
public:
    using datetime_type   = DateTime<T>;
    using difference_type = DateTimeDifference<T>;
    using calendar_type   = CalendarPoint<T>;
    using time_type       = TimePoint<T>;
    using value_type      = T;

    enum class Type {
        system,     // Wall clock — can jump (NTP, DST, manual adjustment)
        steady,     // Monotonic  — never goes backwards, no calendar meaning
        high_res    // Highest resolution available 
    };

private:
    static constexpr std::uint64_t filetime_ticks_per_day_ = 864000000000ULL;
    static constexpr std::uint64_t ns_per_filetime_tick_ = 100ULL;
    static constexpr T planck_per_ns_ = planck_per_unit<T>(Unit::nanosecond);
    static constexpr T windows_epoch_days_ = calendar_type::windows_epoch().raw_days();

public:
    static datetime_type now(Type clock_type = Type::system) noexcept {
        switch (clock_type) {
            case Type::system:   return system_now();
            case Type::steady:   return steady_now();
            case Type::high_res: return high_res_now();
            default:             return system_now();
        }
    }

    static difference_type time_since_epoch(Type clock_type = Type::system) noexcept {
        datetime_type dt = now(clock_type);
        return difference_type(dt.raw_days(), dt.raw_planck());
    }

    template<Unit Tag, typename V = T>
    static Duration<Tag, V> time_since_epoch_as(Type clock_type = Type::system) noexcept {
        datetime_type dt = now(clock_type);
        return dt.template total_to<Tag>();
    }

    static difference_type elapsed(const datetime_type& start, const datetime_type& end) noexcept {
        T day_diff   = end.raw_days()   - start.raw_days();
        T plank_diff = end.raw_planck() - start.raw_planck();
        return difference_type(day_diff, plank_diff);
    }

    static TimeUnit<Unit::nanosecond, multiprecision::uint128> resolution(Type clock_type = Type::system) noexcept {
        switch (clock_type) {
            case Type::system:   return system_resolution();
            case Type::steady:   return steady_resolution();
            case Type::high_res: return steady_resolution();
            default:             return system_resolution();
        }
    }

    static difference_type uptime() noexcept {
    #ifdef OS_WINDOWS
        const ULONGLONG ms = GetTickCount64();
        const T planck = T(ms) * planck_per_unit<T>(Unit::millisecond);
        const T ppd    = planck_per_unit<T>(Unit::day);
        const T days   = planck / ppd;
        const T rem    = planck - days * ppd;
        return difference_type(days, rem);
    #else
        return difference_type();
    #endif
    }

    template<typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    static bool sleep_for(const Args&... durations) noexcept { return sleep(durations...); }

    template<typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
    static bool wait_for(const Args&... durations) noexcept { return wait(durations...); }

    static bool sleep_until(const datetime_type& target, Type clock_type = Type::system) noexcept {
        datetime_type current = now(clock_type);
        if (
            target.raw_days() > current.raw_days() ||
            (target.raw_days() == current.raw_days() && target.raw_planck() > current.raw_planck())
        ) {
            difference_type diff = elapsed(current, target);
            const T ppd = planck_per_unit<T>(Unit::day);
            const T total_planck = diff.raw_days() * ppd + diff.raw_planck();
            const T ppms = planck_per_unit<T>(Unit::millisecond);
            const T ms = total_planck / ppms;
            const std::uint64_t ms_val = static_cast<std::uint64_t>(ms);
            return impl::do_sleep(ms_val, ms_val * 1000000ULL);
        }

        return true;
    }

    static bool wait_until(const datetime_type& target, Type clock_type = Type::system) noexcept {
        datetime_type current = now(clock_type);

        if (
            target.raw_days() > current.raw_days() ||
            (target.raw_days() == current.raw_days() && target.raw_planck() > current.raw_planck())
        ) {
            difference_type diff = elapsed(current, target);
            const T ppd = planck_per_unit<T>(Unit::day);
            const T total_planck = diff.raw_days() * ppd + diff.raw_planck();
            const T ppns = planck_per_unit<T>(Unit::nanosecond);
            const T ns = total_planck / ppns;
            return impl::do_wait(static_cast<std::uint64_t>(ns));
        }

        return true;
    }

    template<typename Func>
    static difference_type benchmark(Func&& func) {
        datetime_type start = now(Type::high_res);
        func();
        datetime_type end = now(Type::high_res);
        return elapsed(start, end);
    }

    class ScopedTimer {
    private:
        datetime_type   start_;
        Type            clock_type_;
        difference_type* result_;

    public:
        explicit ScopedTimer(Type clock_type = Type::high_res, difference_type* result = nullptr) noexcept : start_(Clock::now(clock_type)), clock_type_(clock_type), result_(result) {}

        ~ScopedTimer() {
            difference_type e = Clock::elapsed(start_, Clock::now(clock_type_));
            if (result_) { *result_ = e; }
        }

        difference_type elapsed() const noexcept { return Clock::elapsed(start_, Clock::now(clock_type_)); }
    };

private:
    static datetime_type system_now() noexcept {
    #ifdef OS_WINDOWS
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        ULARGE_INTEGER uli;
        uli.LowPart  = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        const std::uint64_t ticks = uli.QuadPart;
        const std::uint64_t days_since_1601   = ticks / filetime_ticks_per_day_;
        const std::uint64_t remainder_ticks   = ticks % filetime_ticks_per_day_;
        const std::uint64_t remainder_ns = remainder_ticks * ns_per_filetime_tick_;
        const T sub_day_planck = T(remainder_ns) * planck_per_ns_;
        const T total_days = windows_epoch_days_ + T(days_since_1601);
        return datetime_type(total_days, sub_day_planck);
    #else
        return datetime_type();
    #endif
    }

    static datetime_type steady_now() noexcept {
    #ifdef OS_WINDOWS
        LARGE_INTEGER counter, freq;
        QueryPerformanceCounter(&counter);
        QueryPerformanceFrequency(&freq);
        if (freq.QuadPart == 0) { return datetime_type(); }
        const std::uint64_t ticks = static_cast<std::uint64_t>(counter.QuadPart);
        const std::uint64_t f     = static_cast<std::uint64_t>(freq.QuadPart);
        const std::uint64_t ns_per_tick = 1000000000ULL / f;
        const std::uint64_t ns_remainder_ratio = 1000000000ULL % f;
        const std::uint64_t whole_ns = ticks * ns_per_tick + (ticks * ns_remainder_ratio) / f;
        const T total_planck = T(whole_ns) * planck_per_ns_;
        const T ppd = planck_per_unit<T>(Unit::day);
        const T days = total_planck / ppd;
        const T rem  = total_planck - days * ppd;
        return datetime_type(days, rem);
    #else
        return datetime_type();
    #endif
    }

    static datetime_type high_res_now() noexcept {
    #ifdef OS_WINDOWS
        return steady_now();
    #else
        return datetime_type();
    #endif
    }

    static TimeUnit<Unit::nanosecond, multiprecision::uint128> system_resolution() noexcept {
    #ifdef OS_WINDOWS
        DWORD time_adjustment, time_increment;
        BOOL disabled;
        if (GetSystemTimeAdjustment(&time_adjustment, &time_increment, &disabled)) { return TimeUnit<Unit::nanosecond, multiprecision::uint128>(static_cast<std::uint64_t>(time_increment) * ns_per_filetime_tick_); }
        return TimeUnit<Unit::nanosecond, multiprecision::uint128>(std::uint64_t(15625000));
    #else
        return TimeUnit<Unit::nanosecond, multiprecision::uint128>(std::uint64_t(0));
    #endif
    }

    static TimeUnit<Unit::nanosecond, multiprecision::uint128> steady_resolution() noexcept {
    #ifdef OS_WINDOWS
        const std::uint64_t f = tick_frequency();
        if (f == 0) { return TimeUnit<Unit::nanosecond, multiprecision::uint128>(std::uint64_t(0)); }
        return TimeUnit<Unit::nanosecond, multiprecision::uint128>(std::uint64_t(1000000000ULL / f));
    #else
        return TimeUnit<Unit::nanosecond, multiprecision::uint128>(std::uint64_t(0));
    #endif
    }
};

} // namespace temp_time
} // namespace fizmo

#endif // FIZMO_TEMP_TIME_CLOCK_HPP