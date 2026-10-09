#ifndef FIZMO_UTIL_FUNCTIONS_HPP
#define FIZMO_UTIL_FUNCTIONS_HPP

#include "../Basic/basic_includes.hpp"
#include "../Standard Overloads/min_max.hpp"
#include <iostream>
#include <cstring>
#include <cstdio>

namespace fizmo {

inline std::string to_string_with_precision(const double value, const unsigned int precision) {
    if (precision == 0) { return std::to_string(std::round(value)); }

    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    std::string result = stream.str();
    result.erase(result.find_last_not_of('0') + 1, std::string::npos);
    if (result.find('.') != std::string::npos && result.back() == '.') { result.pop_back(); }
    return result;
}

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T clamp(const T value, const T min, const T max) noexcept {
    if (min > max) { return fizmo::max_constexpr(max, fizmo::min_constexpr(min, value)); }
    return fizmo::max_constexpr(min, fizmo::min_constexpr(max, value));
}

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr void clamp_value(T& value, const T min, const T max) noexcept { value = clamp(value, min, max); }

inline std::uint64_t string_compare(const std::string& a, const std::string& b) noexcept { 
    const std::size_t min_len = (a.size() < b.size()) ? a.size() : b.size(); 
    for (std::size_t i = 0; i < min_len; ++i) { if (a[i] != b[i]) { return static_cast<std::uint64_t>(i + 1); }} 
    if (a.size() != b.size()) { return 0; } 
    return 0; 
}

} // namespace fizmo

#endif