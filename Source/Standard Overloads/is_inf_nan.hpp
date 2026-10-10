#ifndef FIZMO_CONSTEXPR_CHECK_INFINITY_AND_NAN_HPP
#define FIZMO_CONSTEXPR_CHECK_INFINITY_AND_NAN_HPP

#include <type_traits>

namespace fizmo {

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_positive_infinity(const T x) noexcept { 
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return x == std::numeric_limits<T>::infinity() || x > std::numeric_limits<T>::max(); 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_positive_infinity(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_negative_infinity(const T x) noexcept { 
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return x == -std::numeric_limits<T>::infinity() || x < std::numeric_limits<T>::lowest(); 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_negative_infinity(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr bool is_infinity(const T x) noexcept { return is_negative_infinity(x) || is_positive_infinity(x); }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr bool isinf_constexpr(const T x) noexcept { return is_infinity(x); }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr bool isfinite_constexpr(const T x) noexcept { return !isinf_constexpr(x); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_nan(const T x) noexcept {
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return x != x; 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_nan(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr bool isnan_constexpr(const T x) noexcept { return is_nan(x); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_out_of_range(const T x) noexcept {
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return x > std::numeric_limits<T>::max() || x < std::numeric_limits<T>::lowest();
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_out_of_range(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_below_epsilon(const T x) noexcept {
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return (x > 0 ? x : -x) < std::numeric_limits<T>::epsilon();
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_below_epsilon(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_uncountable(const T x) noexcept {
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return is_infinity(x) || is_nan(x) || is_out_of_range(x) || is_below_epsilon(x);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_uncountable(const T x) noexcept { return false; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool is_countable(const T x) noexcept {
    static_assert(std::numeric_limits<T>::is_iec559, "Type must be IEEE 754 compliant");
    return !is_uncountable(x);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool is_countable(const T x) noexcept { return true; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr bool signbit_constexpr(const T x) noexcept { return (static_cast<T>(1) / x) < 0 ? (x <= 0) : (x < 0); }

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr bool signbit_constexpr(const T x) noexcept { return x < 0; }

} // namespace fizmo

#endif // FIZMO_CONSTEXPR_CHECK_INFINITY_AND_NAN_HPP