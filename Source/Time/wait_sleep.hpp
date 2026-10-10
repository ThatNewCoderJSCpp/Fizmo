#ifndef FIZMO_time_WAIT_SLEEP_HPP
#define FIZMO_time_WAIT_SLEEP_HPP

#include "time_unit.hpp"
#include <initializer_list>
#include <utility>

namespace fizmo {
namespace time {

#ifdef OS_LINUX
    #ifndef FIZMO_STEADY_CLOCK_ID
        #define FIZMO_STEADY_CLOCK_ID CLOCK_MONOTONIC
    #endif

namespace detail {
std::uint64_t linux_now_ns(clockid_t id) noexcept;
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

void cpu_relax() noexcept;

namespace impl {

#ifdef OS_LINUX
std::uint64_t mono_ns() noexcept;

bool sleep_until_ns(std::uint64_t deadline_ns) noexcept;
#endif

bool do_sleep(std::uint64_t ms, std::uint64_t ns) noexcept;

bool do_wait(std::uint64_t target_ns) noexcept;

bool do_precise_wait(std::uint64_t total_ns) noexcept;

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

bool yield() noexcept;

} // namespace time
} // namespace fizmo

#endif // FIZMO_WAIT_SLEEP_HPP