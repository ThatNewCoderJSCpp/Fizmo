#ifndef FIZMO_CONSTEXPR_HYPERTRIG_HPP
#define FIZMO_CONSTEXPR_HYPERTRIG_HPP

#include <type_traits>
#include <limits>
#include "../Basic/constants.hpp"
#include "abs.hpp"
#include "log.hpp"
#include "sqrt.hpp"
#include "pow.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cosh_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return x; }
    if (is_positive_infinity(x) || is_negative_infinity(x)) { return std::numeric_limits<T>::infinity(); }
    if (abs_constexpr(x) <= constants::TYPE_EPSILON<T>) { return static_cast<T>(1); }
    const T x_abs = abs_constexpr(x);  
    if (x_abs > static_cast<T>(710)) { return std::numeric_limits<T>::infinity(); }

    if (x_abs <= static_cast<T>(13)) { // 13 is approximately where taylor series begins to fail
        T result = static_cast<T>(1);
        T term = static_cast<T>(1);
        const T x_squared = x_abs * x_abs;
        
        for (std::size_t n = 1; n < std::numeric_limits<std::size_t>::max(); ++n) {
            term *= x_squared / ((2 * n - 1) * (2 * n));
            result += term;
            if (abs_constexpr(term) <= constants::TYPE_EPSILON<T>) { break; }
        }
        
        return result;
    }
    
    T reduced_x = x_abs;
    std::size_t reduction_count = 0;
    
    while (reduced_x > static_cast<T>(1)) { 
        reduced_x *= static_cast<T>(0.5);
        ++reduction_count;
    }
    
    T result = static_cast<T>(1);

    if (reduced_x > constants::TYPE_EPSILON<T>) {
        T term = static_cast<T>(1);
        const T x_squared = reduced_x * reduced_x;
        
        for (std::size_t n = 1; n < std::numeric_limits<std::size_t>::max(); ++n) { 
            term *= x_squared / ((2 * n - 1) * (2 * n));
            result += term;
            if (abs_constexpr(term) <= constants::TYPE_EPSILON<T> * result) { break; }
        }
    }
    
    for (std::size_t i = 0; i < reduction_count; ++i) {
        result = static_cast<T>(2) * result * result - static_cast<T>(1);
    }
    
    return result;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cosh_constexpr(const T x) noexcept { return cosh_constexpr(static_cast<double>(x)); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value, T>::type>
constexpr T sinh_constexpr(const T x) noexcept {
    if (isnan_constexpr(x)) { return x; }
    if (is_positive_infinity(x)) { return std::numeric_limits<T>::infinity(); }
    if (is_negative_infinity(x)) { return -std::numeric_limits<T>::infinity(); }
    if (abs_constexpr(x) <= constants::TYPE_EPSILON<T>) { return x; }
    const T x_abs = abs_constexpr(x);
    const int sign = x < 0 ? -1 : 1; 
    if (x_abs > static_cast<T>(710)) { return sign * std::numeric_limits<T>::infinity(); }
    T result = x_abs;
    T term = x_abs;
    const T x_squared = x_abs * x_abs;
    
    for (std::size_t n = 1; n < std::numeric_limits<std::size_t>::max(); ++n) {
        term *= x_squared / ((2 * n) * (2 * n + 1));
        result += term;
        if (abs_constexpr(term) <= constants::TYPE_EPSILON<T>) { break; }
    }
    
    return sign * result;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value, double>::type>
constexpr double sinh_constexpr(const T x) noexcept { return sinh(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
tanh_constexpr(const T x) noexcept { return sinh_constexpr(x) / cosh_constexpr(x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
tanh_constexpr(const T x) noexcept { return tanh_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
csch_constexpr(const T x) noexcept {
    const T sinh_x = sinh_constexpr(x);
    if (abs_constexpr(sinh_x) <= static_cast<T>(1e-18)) { return std::numeric_limits<T>::quiet_NaN(); }
    return static_cast<T>(1) / sinh_x;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
csch_constexpr(const T x) noexcept { return csch_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
sech_constexpr(const T x) noexcept { return static_cast<T>(1) / cosh_constexpr(x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
sech_constexpr(const T x) noexcept { return sech_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
coth_constexpr(const T x) noexcept {
    const T tanh_x = tanh_constexpr(x);
    if (abs_constexpr(tanh_x) <= static_cast<T>(1e-18)) { return std::numeric_limits<T>::quiet_NaN(); }
    return static_cast<T>(1) / tanh_x;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
coth_constexpr(const T x) noexcept { return coth_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
asinh_constexpr(const T x) noexcept {
    if (abs_constexpr(x) <= constants::TYPE_EPSILON<T>) { return static_cast<T>(0); }
    if (x < 0) { return -asinh_constexpr(-x); }
    return log_constexpr(sqrt_constexpr(x * x + 1) + x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
asinh_constexpr(const T x) noexcept { return asinh_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acosh_constexpr(const T x) noexcept {
    if (x < 1) { return std::numeric_limits<T>::quiet_NaN(); }
    if (x == 1) { return static_cast<T>(0); }
    return log_constexpr(sqrt_constexpr(x * x - 1) + x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acosh_constexpr(const T x) noexcept { return acosh_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
atanh_constexpr(const T x) noexcept {
    if (abs_constexpr(x) >= 1) { return std::numeric_limits<T>::quiet_NaN(); }
    if (abs_constexpr(x) <= constants::TYPE_EPSILON<T>) { return static_cast<T>(0); }
    return static_cast<T>(0.5) * (log_constexpr(1 + x) - log_constexpr(1 - x));
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
atanh_constexpr(const T x) noexcept { return atanh_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acsch_constexpr(const T x) noexcept {
    if (abs_constexpr(x) <= constants::TYPE_EPSILON<T>) { return std::numeric_limits<T>::quiet_NaN(); }
    return asinh_constexpr(1.0 / x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acsch_constexpr(const T x) noexcept { return acsch_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
asech_constexpr(const T x) noexcept {
    if (x <= 0 || x > 1) { return std::numeric_limits<T>::quiet_NaN(); }
    if (x == 1) { return static_cast<T>(0); }
    return acosh_constexpr(1.0 / x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
asech_constexpr(const T x) noexcept { return asech_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acoth_constexpr(const T x) noexcept {
    if (abs_constexpr(x) <= 1) { return std::numeric_limits<T>::quiet_NaN(); }
    return atanh_constexpr(1.0 / x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acoth_constexpr(const T x) noexcept { return acoth_constexpr(static_cast<double>(x)); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_HYPERTRIG_HPP