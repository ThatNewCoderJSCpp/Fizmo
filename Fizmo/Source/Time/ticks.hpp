#ifndef FIZMO_TICK_UTILS_HPP
#define FIZMO_TICK_UTILS_HPP

#include "wait_sleep.hpp"

namespace fizmo {
namespace time {

inline std::uint64_t tick_frequency() noexcept {
#ifdef OS_WINDOWS
    static std::uint64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER li;
        if (QueryPerformanceFrequency(&li) && li.QuadPart > 0) { freq = static_cast<std::uint64_t>(li.QuadPart); }
    }
    return freq;
#else
    return 0;
#endif
}

namespace detail {

template<typename R>
R ns_to_ticks(std::uint64_t ns) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return R(0);
    const R wide_ns   = static_cast<R>(ns);
    const R wide_freq = static_cast<R>(freq);
    const R billion   = static_cast<R>(1000000000ULL);
    return (wide_ns * wide_freq) / billion;
}

template<typename R, Unit Tag, typename V>
R planck_to_ticks(const TimeUnit<Tag, V>& t) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return R(0);
    using W = wider_t<R, default_storage_uint>;
    const W planck = static_cast<W>(t.count()) * static_cast<W>(unit_traits<Tag>::planck_per_unit());
    const W sec_planck = static_cast<W>(unit_traits<Unit::second>::planck_per_unit());
    const W wide_freq = static_cast<W>(freq);
    return static_cast<R>((planck * wide_freq) / sec_planck);
}

template<typename R, typename T>
typename std::enable_if<is_time_unit_v<T>, R>::type
sum_ticks(const T& t) noexcept { return planck_to_ticks<R>(t); }

template<typename R, typename T, typename... Rest>
typename std::enable_if<is_time_unit_v<T>, R>::type
sum_ticks(const T& first, const Rest&... rest) noexcept { return planck_to_ticks<R>(first) + sum_ticks<R>(rest...); }

template<typename R, typename T, typename = typename std::enable_if<is_unsigned_integer_like_v<T>>::type>
R runtime_to_ticks(const T& value, Unit u) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return R(0);
    using W = wider_t<R, default_storage_uint>;
    const W sec_planck = static_cast<W>(unit_traits<Unit::second>::planck_per_unit());
    const W wide_freq  = static_cast<W>(freq);
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
        default: return R(0);
    }

    return static_cast<R>((planck * wide_freq) / sec_planck);
}

template<typename R, typename T>
R sum_pairs_ticks(std::initializer_list<std::pair<T, Unit>> pairs) noexcept {
    R total(0);
    for (const auto& p : pairs) total = total + runtime_to_ticks<R>(p.first, p.second);
    return total;
}

} // namespace detail

template<typename R = default_storage_uint, typename... Args, typename = typename std::enable_if<(is_time_unit_v<Args> && ...)>::type>
R time_to_ticks(const Args&... durations) noexcept { return detail::sum_ticks<R>(durations...); }

template<typename R = default_storage_uint, typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
R time_to_ticks(const T& value, Unit unit) noexcept { return detail::runtime_to_ticks<R>(value, unit); }

template<typename R = default_storage_uint, typename T, typename = typename std::enable_if<detail::is_unsigned_integer_like_v<T>>::type>
R time_to_ticks(std::initializer_list<std::pair<T, Unit>> pairs) noexcept { return detail::sum_pairs_ticks<R>(pairs); }

template<
    Unit Tag,
    typename V = default_storage_uint,
    typename TickT = std::uint64_t,
    typename = typename std::enable_if<detail::is_unsigned_integer_like_v<TickT> && detail::is_unsigned_integer_like_v<V>>::type
>
TimeUnit<Tag, V> ticks_to_time(const TickT& num_ticks) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return TimeUnit<Tag, V>(V(0));
    using W = detail::wider_t<V, default_storage_uint>;
    const W wide_ticks  = static_cast<W>(num_ticks);
    const W sec_planck  = static_cast<W>(unit_traits<Unit::second>::planck_per_unit());
    const W wide_freq   = static_cast<W>(freq);
    const W tag_planck  = static_cast<W>(unit_traits<Tag>::planck_per_unit());
    const W planck  = (wide_ticks * sec_planck) / wide_freq;
    const W result  = planck / tag_planck;
    return TimeUnit<Tag, V>(static_cast<V>(result));
}

template<
    typename R = default_storage_uint,
    typename TickT = std::uint64_t,
    typename = typename std::enable_if<detail::is_unsigned_integer_like_v<TickT>>::type
>
R ticks_to_time(const TickT& num_ticks, Unit unit) noexcept {
    const std::uint64_t freq = tick_frequency();
    if (freq == 0) return R(0);
    using W = detail::wider_t<R, default_storage_uint>;
    const W wide_ticks = static_cast<W>(num_ticks);
    const W sec_planck = static_cast<W>(unit_traits<Unit::second>::planck_per_unit());
    const W wide_freq  = static_cast<W>(freq);
    const W planck     = (wide_ticks * sec_planck) / wide_freq;
    W target_planck;

    switch (unit) {
    #define FIZMO_TT_CASE(TAG) \
        case Unit::TAG: target_planck = static_cast<W>(unit_traits<Unit::TAG>::planck_per_unit()); break;
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
        default: return R(0);
    }

    return static_cast<R>(planck / target_planck);
}

template<Unit Tag, typename V = default_storage_uint>
TimeUnit<Tag, V> time_per_tick() noexcept {
    return ticks_to_time<Tag, V>(std::uint64_t(1));
}

template<typename R = default_storage_uint>
R time_per_tick(Unit unit) noexcept { return ticks_to_time<R>(std::uint64_t(1), unit); }

} // namespace time
} // namespace fizmo

#endif // FIZMO_time_TICK_UTILS_HPP