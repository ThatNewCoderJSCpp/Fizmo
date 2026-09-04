#ifndef FIZMO_CHRONO_DEFINES_HPP
#define FIZMO_CHRONO_DEFINES_HPP

#include "small_time.hpp"

namespace fizmo {
namespace time {

template <typename T>
struct is_fizmo_time : std::false_type{};

template <>
struct is_fizmo_time<nanosecond> : std::true_type{};

template <>
struct is_fizmo_time<microsecond> : std::true_type{};

template <>
struct is_fizmo_time<millisecond> : std::true_type{};

template <>
struct is_fizmo_time<centisecond> : std::true_type{};

template <>
struct is_fizmo_time<decisecond> : std::true_type{};

template <>
struct is_fizmo_time<second> : std::true_type{};

template <>
struct is_fizmo_time<minute> : std::true_type{};

template <>
struct is_fizmo_time<hour> : std::true_type{};

template <>
struct is_fizmo_time<day> : std::true_type{};

template <>
struct is_fizmo_time<week> : std::true_type{};

template <>
struct is_fizmo_time<year> : std::true_type{};

template <>
struct is_fizmo_time<decade> : std::true_type{};

template <>
struct is_fizmo_time<century> : std::true_type{};

template <>
struct is_fizmo_time<millennium> : std::true_type{};

template <typename T>
constexpr bool is_fizmo_time_v = is_fizmo_time<T>::value;

template <typename T>
struct is_small_fizmo_time : std::false_type{};

template <>
struct is_small_fizmo_time<nanosecond> : std::true_type{};

template <>
struct is_small_fizmo_time<microsecond> : std::true_type{};

template <>
struct is_small_fizmo_time<millisecond> : std::true_type{};

template <>
struct is_small_fizmo_time<centisecond> : std::true_type{};

template <>
struct is_small_fizmo_time<decisecond> : std::true_type{};

template <>
struct is_small_fizmo_time<second> : std::true_type{};

template <typename T>
constexpr bool is_small_fizmo_time_v = is_small_fizmo_time<T>::value;

template <typename T>
struct is_medium_fizmo_time : std::false_type{};

template <>
struct is_medium_fizmo_time<minute> : std::true_type{};

template <>
struct is_medium_fizmo_time<hour> : std::true_type{};

template <>
struct is_medium_fizmo_time<week> : std::true_type{};

template <typename T>
constexpr bool is_medium_fizmo_time_v = is_medium_fizmo_time<T>::value;

template <typename T>
struct is_large_fizmo_time : std::false_type{};

template <>
struct is_large_fizmo_time<year> : std::true_type{};

template <>
struct is_large_fizmo_time<decade> : std::true_type{};

template <>
struct is_large_fizmo_time<century> : std::true_type{};

template <>
struct is_large_fizmo_time<millennium> : std::true_type{};

template <typename T>
constexpr bool is_large_fizmo_time_v = is_large_fizmo_time<T>::value;

template<typename...>
struct all_are_time_units;

template<>
struct all_are_time_units<> {
    static constexpr bool value = true;
};

template<typename T, typename... Rest>
struct all_are_time_units<T, Rest...> {
    static constexpr bool value = is_fizmo_time_v<T> && all_are_time_units<Rest...>::value;
};

template<typename... Args>
constexpr bool all_are_time_units_v = all_are_time_units<Args...>::value;

} // namespace time
} // namespace fizmo

#endif // FIZMO_CHRONO_DEFINES_HPP