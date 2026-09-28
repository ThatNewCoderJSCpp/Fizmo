#ifndef FIZMO_time_WAIT_SLEEP_HPP
#define FIZMO_time_WAIT_SLEEP_HPP

#include "time_unit.hpp"
#include <initializer_list>
#include <utility>

#ifdef OS_WINDOWS
#include <windows.h>
#endif

namespace fizmo {
namespace time {

#ifdef OS_LINUX
    #ifndef FIZMO_STEADY_CLOCK_ID
        #define FIZMO_STEADY_CLOCK_ID CLOCK_MONOTONIC
    #endif

namespace detail {
inline std::uint64_t linux_now_ns(clockid_t id) noexcept {
    struct timespec ts;
    if (::clock_gettime(id, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}
} // namespace detail
#endif

namespace detail {

template<Unit Tag, typename V>
constexpr std::uint64_t to_ms(const TimeUnit<Tag, V>& t) noexcept {
    using W = wider_t<V, multiprecision::uint256>;
    const W planck = static_cast<W>(t.count()) * static_cast<W>(unit_traits<Tag>::planck_per_unit());
    const W ms_planck = static_cast<W>(unit_traits<Unit::millisecond>::planck_per_unit());
    return static_cast<std::uint64_t>(planck / ms_planck);
}

template<Unit Tag, typename V>
constexpr std::uint64_t to_ns(const TimeUnit<Tag, V>& t) noexcept {
    using W = wider_t<V, multiprecision::uint256>;
    const W planck = static_cast<W>(t.count()) * static_cast<W>(unit_traits<Tag>::planck_per_unit());
    const W ns_planck = static_cast<W>(unit_traits<Unit::nanosecond>::planck_per_unit());
    return static_cast<std::uint64_t>(planck / ns_planck);
}

template<Unit Tag, typename V>
constexpr bool is_sub_ms(const TimeUnit<Tag, V>& t) noexcept {
    return to_ms(t) == 0 && to_ns(t) > 0;
}

template<typename T>
constexpr typename std::enable_if<is_time_unit_v<T>, std::uint64_t>::type
sum_ns(const T& t) noexcept { return to_ns(t); }

template<typename T, typename... Rest>
constexpr typename std::enable_if<is_time_unit_v<T>, std::uint64_t>::type
sum_ns(const T& first, const Rest&... rest) noexcept { return to_ns(first) + sum_ns(rest...); }

template<typename T>
constexpr typename std::enable_if<is_time_unit_v<T>, std::uint64_t>::type
sum_ms(const T& t) noexcept { return to_ms(t); }

template<typename T, typename... Rest>
constexpr typename std::enable_if<is_time_unit_v<T>, std::uint64_t>::type
sum_ms(const T& first, const Rest&... rest) noexcept { return to_ms(first) + sum_ms(rest...); }

template<typename... Args>
constexpr bool sum_is_sub_ms(const Args&... args) noexcept {
    return sum_ms(args...) == 0 && sum_ns(args...) > 0;
}

template<typename T, typename = typename std::enable_if<is_unsigned_integer_like_v<T>>::type>
std::uint64_t runtime_to_ns(const T& value, Unit u) noexcept {
    using W = wider_t<T, multiprecision::uint256>;
    const W ns_planck = static_cast<W>(unit_traits<Unit::nanosecond>::planck_per_unit());
    W planck;

    switch (u) {
    #define FIZMO_TT_CASE(TAG) \
        case Unit::TAG: planck = static_cast<W>(value) * static_cast<W>(unit_traits<Unit::TAG>::planck_per_unit()); break;
        FIZMO_TT_CASE(planck_second)
        FIZMO_TT_CASE(quectosecond)
        FIZMO_TT_CASE(rontosecond)
        FIZMO_TT_CASE(yoctosecond)
        FIZMO_TT_CASE(zeptosecond)
        FIZMO_TT_CASE(attosecond)
        FIZMO_TT_CASE(femtosecond)
        FIZMO_TT_CASE(picosecond)
        FIZMO_TT_CASE(nanosecond)
        FIZMO_TT_CASE(microsecond)
        FIZMO_TT_CASE(millisecond)
        FIZMO_TT_CASE(centisecond)
        FIZMO_TT_CASE(decisecond)
        FIZMO_TT_CASE(second)
        FIZMO_TT_CASE(minute)
        FIZMO_TT_CASE(hour)
        FIZMO_TT_CASE(day)
        FIZMO_TT_CASE(week)
        FIZMO_TT_CASE(month)
        FIZMO_TT_CASE(year)
        FIZMO_TT_CASE(decade)
        FIZMO_TT_CASE(century)
        FIZMO_TT_CASE(millennium)
    #undef FIZMO_TT_CASE
        default: return 0;
    }

    return static_cast<std::uint64_t>(planck / ns_planck);
}

template<typename T, typename = typename std::enable_if<is_unsigned_integer_like_v<T>>::type>
std::uint64_t runtime_to_ms(const T& value, Unit u) noexcept {
    using W = wider_t<T, multiprecision::uint256>;
    const W ms_planck = static_cast<W>(unit_traits<Unit::millisecond>::planck_per_unit());
    W planck;

    switch (u) {
    #define FIZMO_TT_CASE(TAG) \
        case Unit::TAG: planck = static_cast<W>(value) * static_cast<W>(unit_traits<Unit::TAG>::planck_per_unit()); break;
        FIZMO_TT_CASE(planck_second)
        FIZMO_TT_CASE(quectosecond)
        FIZMO_TT_CASE(rontosecond)
        FIZMO_TT_CASE(yoctosecond)
        FIZMO_TT_CASE(zeptosecond)
        FIZMO_TT_CASE(attosecond)
        FIZMO_TT_CASE(femtosecond)
        FIZMO_TT_CASE(picosecond)
        FIZMO_TT_CASE(nanosecond)
        FIZMO_TT_CASE(microsecond)
        FIZMO_TT_CASE(millisecond)
        FIZMO_TT_CASE(centisecond)
        FIZMO_TT_CASE(decisecond)
        FIZMO_TT_CASE(second)
        FIZMO_TT_CASE(minute)
        FIZMO_TT_CASE(hour)
        FIZMO_TT_CASE(day)
        FIZMO_TT_CASE(week)
        FIZMO_TT_CASE(month)
        FIZMO_TT_CASE(year)
        FIZMO_TT_CASE(decade)
        FIZMO_TT_CASE(century)
        FIZMO_TT_CASE(millennium)
    #undef FIZMO_TT_CASE
        default: return 0;
    }

    return static_cast<std::uint64_t>(planck / ms_planck);
}

template<typename T>
std::uint64_t sum_pairs_ns(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    std::uint64_t total = 0;
    for (const auto& p : pairs) total += runtime_to_ns(p.first, p.second);
    return total;
}

template<typename T>
std::uint64_t sum_pairs_ms(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    std::uint64_t total = 0;
    for (const auto& p : pairs) total += runtime_to_ms(p.first, p.second);
    return total;
}

} // namespace detail

inline void cpu_relax() noexcept {
#if defined(OS_WINDOWS)
    YieldProcessor();
#elif defined(ARCH_X86_64) || defined(ARCH_X86_32)
    __builtin_ia32_pause();
#elif defined(ARCH_ARM64) || defined(ARCH_ARMV7_OR_GREATER)
    __asm__ __volatile__("yield" ::: "memory");
#endif
}

namespace impl {

#ifdef OS_LINUX
inline std::uint64_t mono_ns() noexcept {
    struct timespec ts;
    if (::clock_gettime(FIZMO_STEADY_CLOCK_ID, &ts) != 0) return 0;
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

inline bool sleep_until_ns(std::uint64_t deadline_ns) noexcept {
    struct timespec ts;
    ts.tv_sec  = static_cast<time_t>(deadline_ns / 1000000000ULL);
    ts.tv_nsec = static_cast<long>(deadline_ns % 1000000000ULL);
    int rc;
    while ((rc = ::clock_nanosleep(FIZMO_STEADY_CLOCK_ID, TIMER_ABSTIME, &ts, nullptr)) == EINTR) {}
    return rc == 0;
}
#endif

inline bool do_sleep(std::uint64_t ms, std::uint64_t ns) noexcept {
#ifdef OS_WINDOWS
    if (ms == 0 && ns > 0) {
        Sleep(1);
        return true;
    }

    std::uint64_t remaining = ms;

    while (remaining > 0) {
        const DWORD chunk = (remaining > 0xFFFFFFFE) ? 0xFFFFFFFE : static_cast<DWORD>(remaining);
        Sleep(chunk);
        remaining -= chunk;
    }

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t total_ns = (ns > 0) ? ns : ms * 1000000ULL;
    if (total_ns == 0) return true;
    return sleep_until_ns(mono_ns() + total_ns);
#else
    return false;
#endif
}

inline bool do_wait(std::uint64_t target_ns) noexcept {
#ifdef OS_WINDOWS
    LARGE_INTEGER freq, start, current;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) return false;
    if (!QueryPerformanceCounter(&start)) return false;
    const std::uint64_t target_ticks = (target_ns * freq.QuadPart) / 1000000000ULL;

    do {
        if (!QueryPerformanceCounter(&current)) return false;
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < target_ticks);

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t deadline = mono_ns() + target_ns;
    while (mono_ns() < deadline) cpu_relax();
    return true;
#else
    return false;
#endif
}

inline bool do_precise_wait(std::uint64_t total_ns) noexcept {
#ifdef OS_WINDOWS
    LARGE_INTEGER freq, start, current;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) return false;
    if (!QueryPerformanceCounter(&start)) return false;

    if (total_ns > 10000000ULL) {
        const std::uint64_t sleep_ms = (total_ns / 1000000ULL) - 2;
        if (sleep_ms > 0) do_sleep(sleep_ms, sleep_ms);
    }

    const std::uint64_t total_ticks = (total_ns * freq.QuadPart) / 1000000000ULL;

    do {
        if (!QueryPerformanceCounter(&current)) return false;
    } while (static_cast<std::uint64_t>(current.QuadPart - start.QuadPart) < total_ticks);

    return true;
#elif defined(OS_LINUX)
    const std::uint64_t deadline = mono_ns() + total_ns;
    const std::uint64_t guard = 250000ULL;      
    if (total_ns > guard * 2) sleep_until_ns(deadline - guard);
    while (mono_ns() < deadline) cpu_relax();
    return true;
#else
    return false;
#endif
}

} // namespace impl

template<typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
bool sleep(const Args&... durations) noexcept {
    return impl::do_sleep(detail::sum_ms(durations...), detail::sum_ns(durations...));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool sleep(const T& value, Unit unit) noexcept {
    return impl::do_sleep(detail::runtime_to_ms(value, unit), detail::runtime_to_ns(value, unit));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool sleep(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    return impl::do_sleep(detail::sum_pairs_ms(pairs), detail::sum_pairs_ns(pairs));
}

template<typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
bool wait(const Args&... durations) noexcept {
    return impl::do_wait(detail::sum_ns(durations...));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool wait(const T& value, Unit unit) noexcept {
    return impl::do_wait(detail::runtime_to_ns(value, unit));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool wait(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    return impl::do_wait(detail::sum_pairs_ns(pairs));
}

template<typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
bool precise_wait(const Args&... durations) noexcept {
    return impl::do_precise_wait(detail::sum_ns(durations...));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool precise_wait(const T& value, Unit unit) noexcept {
    return impl::do_precise_wait(detail::runtime_to_ns(value, unit));
}

template<typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
bool precise_wait(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    return impl::do_precise_wait(detail::sum_pairs_ns(pairs));
}

inline bool yield() noexcept {
#ifdef OS_WINDOWS
    Sleep(0);
    return true;
#elif defined(OS_LINUX)
    return ::sched_yield() == 0;
#else
    return false;
#endif
}

} // namespace time
} // namespace fizmo

#endif // FIZMO_WAIT_SLEEP_HPP