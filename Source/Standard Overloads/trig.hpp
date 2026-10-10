#ifndef FIZMO_CONSTEXPR_TRIG_HPP
#define FIZMO_CONSTEXPR_TRIG_HPP

#include <type_traits>
#include "../Basic/constants.hpp"
#include "abs.hpp"
#include "../Misc Math/alternating_power.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
sin_constexpr(T x) noexcept {
    if (abs_constexpr(x) <= fizmo::constants::TYPE_EPSILON<T>) { return static_cast<T>(0); }
    bool is_negative = (x < 0);
    if (is_negative) { x = -x; }
    
    if (x > 2 * fizmo::constants::PI<T>) {
        T k = static_cast<T>(static_cast<std::int64_t>(x / (2 * fizmo::constants::PI<T>)));
        x -= k * 2 * fizmo::constants::PI<T>;
    }
    
    bool flip_sign = false;

    if (x > fizmo::constants::PI<T>) {
        x -= fizmo::constants::PI<T>;
        flip_sign = true;
    }

    if (x > fizmo::constants::PI_2<T>) { x = fizmo::constants::PI<T> - x; }
    T result = 0;
    T compensation = 0; 
    T term = x;
    T x_squared = x * x;
    T prev_sum = 0;

    for (std::uint64_t n = 1; n < (std::numeric_limits<std::uint64_t>::max() - 1) / 2; ++n) {
        prev_sum = result;
        T y = term - compensation;
        T t = result + y;
        compensation = (t - result) - y;
        result = t;
        term *= -x_squared / ((2 * n) * (2 * n + 1));
        
        if (abs_constexpr(term) <= fizmo::constants::TYPE_EPSILON<T> || 
            abs_constexpr(result - prev_sum) <= fizmo::constants::TYPE_EPSILON<T>
        ) { 
            break; 
        }
    }

    if (flip_sign) { result = -result; }
    if (is_negative) { result = -result; }
    return result;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
sin_constexpr(const T x) noexcept { return sin_constexpr(static_cast<double>(x)); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr T sinc_constexpr(const T x) noexcept {
    if (x == 0) { return T(1); }
    return sin_constexpr(x) / x;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
constexpr double sinc_constexpr(const T x) noexcept {
    if (x == 0) { return 1.0; }
    return sin_constexpr(x) / static_cast<double>(x);
}

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cos_constexpr(const T x) noexcept { return sin_constexpr(fizmo::constants::PI<T> * 0.5 - x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cos_constexpr(const T x) noexcept { return cos_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
tan_constexpr(const T x) noexcept { 
    const T cos_val = cos_constexpr(x);
    const T sin_val = sin_constexpr(x);
    if (abs_constexpr(cos_val) <= fizmo::constants::TYPE_EPSILON<T>) { return sin_val > 0 ? fizmo::constants::POSITIVE_INFINITY<T> : fizmo::constants::NEGATIVE_INFINITY<T>; }
    return sin_val / cos_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
tan_constexpr(const T x) noexcept { return tan_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
csc_constexpr(const T x) noexcept { 
    const T sin_val = sin_constexpr(x);
    if (abs_constexpr(sin_val) <= fizmo::constants::TYPE_EPSILON<T>) { return sin_val > 0 ? fizmo::constants::POSITIVE_INFINITY<T> : fizmo::constants::NEGATIVE_INFINITY<T>; }
    return static_cast<T>(1) / sin_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
csc_constexpr(const T x) noexcept { return csc_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
sec_constexpr(const T x) noexcept { 
    const T cos_val = cos_constexpr(x);
    if (abs_constexpr(cos_val) <= fizmo::constants::TYPE_EPSILON<T>) { return cos_val > 0 ? fizmo::constants::POSITIVE_INFINITY<T> : fizmo::constants::NEGATIVE_INFINITY<T>; }
    return static_cast<T>(1) / cos_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
sec_constexpr(const T x) noexcept { return sec_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
cot_constexpr(const T x) noexcept { 
    const T tan_val = tan_constexpr(x);
    if (abs_constexpr(tan_val) <= fizmo::constants::TYPE_EPSILON<T>) { return tan_val > 0 ? fizmo::constants::POSITIVE_INFINITY<T> : fizmo::constants::NEGATIVE_INFINITY<T>; }
    return static_cast<T>(1) / tan_val;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
cot_constexpr(const T x) noexcept { return cot_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
asin_constexpr(T x) noexcept {
    if (x < -1 || x > 1) { return fizmo::constants::QUIET_NAN<T>; }
    if (abs_constexpr(x) <= fizmo::constants::TYPE_EPSILON<T>) { return static_cast<T>(0); }
    if (abs_constexpr(x - 1) <= fizmo::constants::TYPE_EPSILON<T>) { return fizmo::constants::PI_2<T>; }
    bool is_negative = (x < 0);
    if (is_negative) { x = -x; }
    
    if (x > 0.85) {
        const T arg = sqrt_constexpr((1 - x) * 0.5);
        T result = fizmo::constants::PI_2<T> - 2 * asin_constexpr(arg);
        return is_negative ? -result : result;
    }
    
    T result = 0;
    T compensation = 0;
    T term = x;
    T prev_sum = 0;
    T x_squared = x * x;
    
    for (std::uint64_t n = 0; n < (std::numeric_limits<std::uint64_t>::max() - 1) / 2; ++n) {
        prev_sum = result;
        T y = term - compensation;
        T t = result + y;
        compensation = (t - result) - y;
        result = t;
        
        if (n == 0) {
            term = x * x_squared / 6; 
        } else {
            term *= x_squared * (2 * n + 1) * (2 * n + 1) / ((2 * n + 2) * (2 * n + 3));
        }
        
        if (abs_constexpr(term) <= fizmo::constants::TYPE_EPSILON<T> || 
            abs_constexpr(result - prev_sum) <= fizmo::constants::TYPE_EPSILON<T>
        ) { 
            break; 
        }
    }
    
    return is_negative ? -result : result;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
asin_constexpr(const T x) noexcept { return asin_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acos_constexpr(const T x) noexcept { return fizmo::constants::PI_2<T> - asin_constexpr(x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acos_constexpr(const T x) noexcept { return acos_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
atan_constexpr(T x) noexcept {
    if (abs_constexpr(x) <= fizmo::constants::EPSILON<T>) { return static_cast<T>(0); }
    if (abs_constexpr(x - 1) <= fizmo::constants::EPSILON<T>) { return fizmo::constants::PI_4<T>; }
    if (abs_constexpr(x + 1) <= fizmo::constants::EPSILON<T>) { return -fizmo::constants::PI_4<T>; }
    if (x >= std::numeric_limits<T>::max() * 0.5) { return fizmo::constants::PI<T> * 0.5; }
    if (x <= std::numeric_limits<T>::lowest() * 0.5) { return -fizmo::constants::PI<T> * 0.5; }

    if (abs_constexpr(x) > 1) { 
        T result = 0.0;
        T compensation = 0.0; 
        const T const_add = (x > 0 ? 1 : -1) * fizmo::constants::PI_2<T>;
        const T x_squared = x * x;
        T x_pow = 1.0;
        T prev_sum = 0;
        
        for (std::size_t k = 0; k < std::numeric_limits<std::size_t>::max(); ++k) {
            const int num = alternating_power(k);
            const T denom = (2 * k + 1) * x * x_pow;
            const T term = num / denom;
            prev_sum = result;
            T y = term - compensation;
            T t = result + y;
            compensation = (t - result) - y;
            result = t;
            
            if (abs_constexpr(term) <= fizmo::constants::TYPE_EPSILON<T> ||
                abs_constexpr(result - prev_sum) <= fizmo::constants::TYPE_EPSILON<T>
            ) { 
                break; 
            }
            x_pow *= x_squared;
        }

        return const_add - result;
    }

    bool is_negative = (x < 0);
    if (is_negative) { x = -x; }
    T result = 0.0;
    T compensation = 0.0;  
    const T x_squared = x * x;
    T x_pow = 1.0;
    T prev_sum = 0;
    
    for (std::size_t k = 0; k < std::numeric_limits<std::size_t>::max(); ++k) {
        const T num = alternating_power(k) * x * x_pow;
        const T denom = (2 * k + 1);
        const T term = num / denom;
        prev_sum = result;
        T y = term - compensation;
        T t = result + y;
        compensation = (t - result) - y;
        result = t;
        
        if (abs_constexpr(term) <= fizmo::constants::TYPE_EPSILON<T> ||
            abs_constexpr(result - prev_sum) <= fizmo::constants::TYPE_EPSILON<T>
        ) { 
            break; 
        }
        x_pow *= x_squared;
    }
    
    return is_negative ? -result : result;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
atan_constexpr(const T x) noexcept { return atan_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
atan2_constexpr(const T y, const T x) noexcept {
    const T eps = fizmo::constants::TYPE_EPSILON<T>;
    if (abs_constexpr(x) <= eps && abs_constexpr(y) <= eps) { return fizmo::constants::QUIET_NAN<T>; }
    if (abs_constexpr(x) <= eps) { return (y > 0) ? fizmo::constants::PI_2<T> : -fizmo::constants::PI_2<T>; }
    if (abs_constexpr(y) <= eps) { return (x > 0) ? static_cast<T>(0) : fizmo::constants::PI<T>; }
    const T a = atan_constexpr(y / x);

    if (x > 0) {
        return a;  
    } else {
        return (y > 0) ? (a + fizmo::constants::PI<T>) : (a - fizmo::constants::PI<T>);
    }
}

template <typename T, typename U>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_floating_point<U>::value, U>::type
atan2_constexpr(const T y, const U x) noexcept { return atan2_constexpr(static_cast<U>(y), x); }

template <typename T, typename U>
constexpr typename std::enable_if<std::is_floating_point<T>::value && std::is_integral<U>::value, T>::type
atan2_constexpr(const T y, const U x) noexcept { return atan2_constexpr(y, static_cast<T>(x)); }

template <typename T, typename U>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_integral<U>::value, double>::type
atan2_constexpr(const T y, const U x) noexcept { return atan2_constexpr(static_cast<double>(y), static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acsc_constexpr(const T x) noexcept { return asin_constexpr(static_cast<T>(1) / x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acsc_constexpr(const T x) noexcept { return acsc_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
asec_constexpr(const T x) noexcept { return acos_constexpr(static_cast<T>(1) / x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
asec_constexpr(const T x) noexcept { return asec_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
acot_constexpr(const T x) noexcept { return atan_constexpr(static_cast<T>(1) / x); }

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
acot_constexpr(const T x) noexcept { return acot_constexpr(static_cast<double>(x)); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_TRIG_HPP