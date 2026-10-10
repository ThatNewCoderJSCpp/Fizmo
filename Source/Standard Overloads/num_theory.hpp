#ifndef NUMBER_THEORY_CONSTEXPR_HPP
#define NUMBER_THEORY_CONSTEXPR_HPP

#include "abs.hpp"

namespace fizmo {

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T floor_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isinf_constexpr(x)) { return x; }
    constexpr T max_exact_int = static_cast<T>(1LL << 53); 
    if (abs_constexpr(x) >= max_exact_int) { return x; }
    const T integer_part = static_cast<T>(static_cast<long long>(x));
    return (x < 0 && x != integer_part) ? integer_part - 1 : integer_part;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void> 
constexpr T floor_constexpr(const T x) noexcept { return x; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T ceil_constexpr(const T x) {
    if (isnan_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isinf_constexpr(x)) { return x; }
    constexpr T max_exact_int = static_cast<T>(1LL << 53); 
    if (abs_constexpr(x) >= max_exact_int) { return x; }
    const T integer_part = static_cast<T>(static_cast<long long>(x));
    return (x > 0 && x != integer_part) ? integer_part + 1 : integer_part;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void> 
constexpr T ceil_constexpr(const T x) noexcept { return x; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T round_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isinf_constexpr(x)) { return x; }
    constexpr T max_exact_int = static_cast<T>(1LL << 53); 
    if (abs_constexpr(x) >= max_exact_int) { return x; }
    if (x < 0) { return -round_constexpr(-x); }
    return floor_constexpr(x + static_cast<T>(0.5));
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T round_constexpr(const T x) noexcept { return x; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T fractional_constexpr(const T x) noexcept { 
    if (isnan_constexpr(x) || isinf_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    constexpr T max_exact_int = static_cast<T>(1LL << 53); 
    if (abs_constexpr(x) >= max_exact_int) { return static_cast<T>(0); }
    return x - floor_constexpr(x); 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void> 
constexpr T fractional_constexpr(const T x) noexcept { return 0; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T truncate_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isinf_constexpr(x)) { return x; }
    constexpr T max_exact_int = static_cast<T>(1LL << 53); 
    if (abs_constexpr(x) >= max_exact_int) { return x; }
    return (x >= 0) ? floor_constexpr(x) : ceil_constexpr(x);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T truncate_constexpr(const T x) noexcept { return x; }

namespace math {

template <typename T, typename U>
constexpr typename std::enable_if<std::is_floating_point<T>::value && std::is_floating_point<U>::value, typename std::common_type<T, U>::type>::type
mod_constexpr(const T x, const U y) noexcept {
    using result_type = typename std::common_type<T, U>::type;
    if (isnan_constexpr(x) || isnan_constexpr(y)) { return std::numeric_limits<result_type>::quiet_NaN(); }
    if (y == 0) { return std::numeric_limits<result_type>::quiet_NaN(); }
    if (isinf_constexpr(x)) { return std::numeric_limits<result_type>::quiet_NaN(); }
    if (isinf_constexpr(y)) { return x; }
    const bool x_negative = signbit_constexpr(x);
    const result_type abs_x = abs_constexpr(static_cast<result_type>(x));
    const result_type abs_y = abs_constexpr(static_cast<result_type>(y));
    if (abs_x == 0) { return x_negative ? -static_cast<result_type>(0) : static_cast<result_type>(0); }
    if (abs_x < abs_y) { return x; }
    constexpr result_type max_exact_int = static_cast<result_type>(1LL << 53);
    result_type remainder = abs_x;
    
    if (abs_x >= max_exact_int && abs_y <= max_exact_int) {
        result_type scaled_y = abs_y;
        while (scaled_y < max_exact_int / static_cast<result_type>(2)) { scaled_y *= static_cast<result_type>(2); }
        while (remainder >= scaled_y) { remainder -= scaled_y; }
    }
    
    result_type subtractor = abs_y;
    while (remainder >= subtractor * static_cast<result_type>(2)) { subtractor *= static_cast<result_type>(2); }
    
    while (subtractor >= abs_y) {
        if (remainder >= subtractor) { remainder -= subtractor; }
        subtractor *= static_cast<result_type>(0.5);
    }
    
    while (remainder >= abs_y) { remainder -= abs_y; }
    return remainder * (x < 0 ? -1 : 1);
}

} // namespace math
} // namespace fizmo

#endif // NUMBER_THEORY_CONSTEXPR_HPP