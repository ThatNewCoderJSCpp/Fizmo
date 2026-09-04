#ifndef UTILITY_CHRONO_FUNCTIONS_HPP
#define UTILITY_CHRONO_FUNCTIONS_HPP

#include "countdown.hpp"

namespace fizmo {
namespace time {

inline std::uint64_t get_tick_frequency() noexcept {
#ifdef OS_WINDOWS
    static std::uint64_t frequency = 0;
    if (frequency == 0) {
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq)) {
            frequency = static_cast<std::uint64_t>(freq.QuadPart);
        } else {
            frequency = 1000000; // fallback to 1MHz if QueryPerformanceFrequency fails
        }
    }
    return frequency;
#else
    return 1000000000; 
#endif
}

template <typename T, typename = typename std::enable_if<is_fizmo_time_v<T>>::type>
inline std::uint64_t time_to_ticks(const T time) noexcept { 
    const std::uint64_t freq = get_tick_frequency();
    if (freq == 0) return 0ULL;
    const std::uint64_t ns_value = static_cast<nanosecond>(time).value();
    return (ns_value * freq) / 1000000000ULL;
}

inline std::uint64_t time_to_ticks(DURATION_PARAM duration) noexcept { return time_to_ticks(duration.to_nanoseconds()); }
inline std::uint64_t time_to_ticks(const CompleteDuration& duration) { return time_to_ticks(duration.to_nanosecond()); }

template <Duration::unit T = Duration::unit::nanosecond>
inline fizmo_time_type_t<T> time_per_tick() noexcept {
    const std::uint64_t freq = get_tick_frequency();
    if (freq == 0) { return fizmo_time_type_t<T>(0); }
    const std::uint64_t ns_per_tick = 1000000000ULL / freq;
    const nanosecond ns_time(ns_per_tick);
    return static_cast<fizmo_time_type_t<T>>(ns_time);
}

inline CompleteDuration ticks_to_duration(const std::uint64_t num_ticks = 1) noexcept {
    if (num_ticks == 0) { return CompleteDuration(nanosecond(0)); }
    const std::uint64_t freq = get_tick_frequency();
    if (freq == 0) return CompleteDuration(nanosecond(0));
    const std::uint64_t ns_per_tick = time_per_tick().value();
    const std::uint64_t remainder = 1000000000ULL % freq;
    const nanosecond ns = nanosecond(num_ticks * ns_per_tick + (num_ticks * remainder) / freq);
    return CompleteDuration(ns);
}

template <Duration::unit T = Duration::unit::nanosecond>
inline fizmo_time_type_t<T> ticks_to_time(const std::uint64_t num_ticks = 1) noexcept {
    return static_cast<fizmo_time_type_t<T>>(ticks_to_duration(num_ticks).to_nanosecond());
}

template<typename T>
constexpr Duration::unit get_unit_from_time_type() noexcept;

template<>
inline constexpr Duration::unit get_unit_from_time_type<nanosecond>() noexcept { return Duration::unit::nanosecond; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<microsecond>() noexcept { return Duration::unit::microsecond; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<millisecond>() noexcept { return Duration::unit::millisecond; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<centisecond>() noexcept { return Duration::unit::centisecond; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<decisecond>() noexcept { return Duration::unit::decisecond; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<second>() noexcept { return Duration::unit::second; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<minute>() noexcept { return Duration::unit::minute; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<hour>() noexcept { return Duration::unit::hour; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<day>() noexcept { return Duration::unit::day; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<week>() noexcept { return Duration::unit::week; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<year>() noexcept { return Duration::unit::year; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<decade>() noexcept { return Duration::unit::decade; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<century>() noexcept { return Duration::unit::century; }

template<>
inline constexpr Duration::unit get_unit_from_time_type<millennium>() noexcept { return Duration::unit::millennium; }

template<Duration::unit U>
constexpr fizmo_time_type_t<U> create_time_from_unit(std::uint64_t value = 0) noexcept { return fizmo_time_type_t<U>(value); }

} // namespace time
} // namespace fizmo

#endif // UTILITY_CHRONO_FUNCTIONS_HPP