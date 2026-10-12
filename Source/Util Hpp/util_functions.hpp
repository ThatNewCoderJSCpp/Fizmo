#ifndef FIZMO_UTIL_FUNCTIONS_HPP
#define FIZMO_UTIL_FUNCTIONS_HPP

#include "../Basic/basic_includes.hpp"
#include <algorithm>
#include <iostream>
#include <cstring>
#include <cstdio>

namespace fizmo {

std::string to_string_with_precision(const double value, const unsigned int precision);

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T clamp(const T value, const T min, const T max) noexcept {
    if (min > max) { return std::max(max, std::min(min, value)); }
    return std::max(min, std::min(max, value));
}

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr void clamp_value(T& value, const T min, const T max) noexcept { value = clamp(value, min, max); }

std::uint64_t string_compare(const std::string& a, const std::string& b) noexcept;

} // namespace fizmo

#endif