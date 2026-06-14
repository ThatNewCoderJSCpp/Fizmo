#ifndef FIZMO_CONSTEXPR_MIN_MAX_HPP
#define FIZMO_CONSTEXPR_MIN_MAX_HPP

#include "is_inf_nan.hpp"

namespace fizmo {

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T max_constexpr() noexcept { return std::numeric_limits<T>::max(); }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T min_constexpr() noexcept { return std::numeric_limits<T>::lowest(); }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T max_constexpr(const T a) noexcept { return a; }

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr T min_constexpr(const T a) noexcept { return a; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T max_constexpr(const T a, const T b) noexcept { 
    if (isnan_constexpr(a) && isnan_constexpr(b)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isnan_constexpr(a) && !isnan_constexpr(b)) { return b; }
    if (!isnan_constexpr(a) && isnan_constexpr(b)) { return a; }
    
    if (isinf_constexpr(a) && isinf_constexpr(b)) {
        if (signbit_constexpr(a) == signbit_constexpr(b)) { return signbit_constexpr(a) ? b : a; }
        return signbit_constexpr(a) ? b : a;
    }
    
    if (isinf_constexpr(a)) { return signbit_constexpr(a) ? b : a; } 
    if (isinf_constexpr(b)) { return signbit_constexpr(b) ? a : b; }
    return a > b ? a : b; 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T max_constexpr(const T a, const T b) noexcept { return a > b ? a : b; }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T min_constexpr(const T a, const T b) noexcept { 
    if (isnan_constexpr(a) && isnan_constexpr(b)) { return std::numeric_limits<T>::quiet_NaN(); }
    if (isnan_constexpr(a) && !isnan_constexpr(b)) { return b; }
    if (!isnan_constexpr(a) && isnan_constexpr(b)) { return a; }
    
    if (isinf_constexpr(a) && isinf_constexpr(b)) {
        if (signbit_constexpr(a) == signbit_constexpr(b)) { return signbit_constexpr(a) ? a : b; }
        return signbit_constexpr(a) ? a : b;
    }
    
    if (isinf_constexpr(a)) { return signbit_constexpr(a) ? a : b; } 
    if (isinf_constexpr(b)) { return signbit_constexpr(b) ? b : a; }
    return a < b ? a : b; 
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T min_constexpr(const T a, const T b) noexcept { return a < b ? a : b; }

template <typename T, typename... Args, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T max_constexpr(const T a, const T b, const Args... args) noexcept {
    T ab_max = max_constexpr(a, b);
    return max_constexpr(ab_max, args...);
}

template <typename T, typename... Args, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T max_constexpr(const T a, const T b, const Args... args) noexcept { return max_constexpr(max_constexpr(a, b), args...); }

template <typename T, typename... Args, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T min_constexpr(const T a, const T b, const Args... args) noexcept {
    T ab_min = min_constexpr(a, b);
    return min_constexpr(ab_min, args...);
}

template <typename T, typename... Args, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T min_constexpr(const T a, const T b, const Args... args) noexcept { return min_constexpr(min_constexpr(a, b), args...); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T max_constexpr(std::initializer_list<T> values) noexcept {
    if (values.size() == 0) { return std::numeric_limits<T>::lowest(); }
    T result = *values.begin();
    bool all_nan = isnan_constexpr(result);
    
    for (const T value : values) {
        all_nan = all_nan && isnan_constexpr(value);
        result = max_constexpr(result, value);
    }
    
    if (all_nan) { return std::numeric_limits<T>::quiet_NaN(); }
    return result;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T max_constexpr(std::initializer_list<T> values) noexcept {
    T result = *values.begin();
    for (const T value : values) { result = max_constexpr(result, value); }
    return result;
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T min_constexpr(std::initializer_list<T> values) noexcept {
    if (values.size() == 0) { return std::numeric_limits<T>::max(); }
    T result = *values.begin();
    bool all_nan = isnan_constexpr(result);
    
    for (const T value : values) {
        all_nan = all_nan && isnan_constexpr(value);
        result = min_constexpr(result, value);
    }
    
    if (all_nan) { return std::numeric_limits<T>::quiet_NaN(); }
    return result;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type, typename = void>
constexpr T min_constexpr(std::initializer_list<T> values) noexcept {
    T result = *values.begin();
    for (const T value : values) { result = min_constexpr(result, value); }
    return result;
}

} // namesapce fizmo

#endif // FIZMO_CONSTEXPR_MIN_MAX_HPP