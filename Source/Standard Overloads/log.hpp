#ifndef FIZMO_CONSTEXPR_LOG_HPP
#define FIZMO_CONSTEXPR_LOG_HPP

#include <type_traits>
#include <limits>
#include <cmath>
#include "abs.hpp"
#include "../Basic/constants.hpp"
#include "../Util Hpp/util_functions.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
log_constexpr(T x) noexcept {
    if (x == 0) { return -std::numeric_limits<T>::infinity(); }
    if (x < 0) { return std::numeric_limits<T>::quiet_NaN(); }
    if (x == 1) { return 0; }
    if (x == fizmo::constants::EULER<T>) { return 1; }
    if (is_positive_infinity(x) || abs_constexpr(x) >= std::numeric_limits<T>::max()) { return std::numeric_limits<T>::infinity(); }
    
    // log1p
    if (x < 1) {
        const T z = x - 1; 
        if (abs_constexpr(z) <= constants::MIDDLE_EPSILON<T>) { return 0; }
        if (abs_constexpr(z - 1) <= constants::MIDDLE_EPSILON<T>) { return 0.693147180559945309417232121458176568075500134360255254121; }
        T result = 0;
        T z_power = z;
        int sign = 1;
        std::size_t i = 1;
        
        while (true) {
            const T term = z_power / i;
            result += sign * term;
            z_power *= z;
            sign = -sign;
            i++;
            if (abs_constexpr(term) <= 1e-18 || i == 0) { break; }
        }
        
        return result;
    }
    
    int power = 0;
    constexpr T sqrt_e = static_cast<T>(1.6487212707);
    constexpr T inv_sqrt_e = static_cast<T>(0.6065306597);
    
    while (x > sqrt_e) {
        x /= static_cast<T>(fizmo::constants::EULER<T>);
        power++;
    }
    
    while (x < inv_sqrt_e) {
        x *= static_cast<T>(fizmo::constants::EULER<T>);
        power--;
    }
    
    const T z = x - 1;
    T result = 0;
    T term = z;
    T z_power = z;
    T divisor = 1;
    int sign = 1;
    std::size_t i = 1;
    
    while (true) {
        result += sign * z_power / i;
        if (abs_constexpr(z_power / i) < constants::TYPE_EPSILON<T>) { break; }
        z_power *= z;
        sign = -sign;
        i++;
        if (i == 0) { break; }
    }
    
    return result + power * static_cast<T>(1.0);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
log_constexpr(const T x) noexcept { return log_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
log2_constexpr(const T x) noexcept {
    constexpr T rec_ln2 = static_cast<T>(1.44269504088896340735992468100189213742664595415298593413544940693110921918);
    return log_constexpr(x) * rec_ln2; 
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
log2_constexpr(const T x) noexcept { return log2_constexpr(static_cast<double>(x)); }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
log10_constexpr(const T x) noexcept { 
    constexpr T rec_ln10 = static_cast<T>(0.434294481903251827651128918916605082294397005803666566114453783165864649);
    return log_constexpr(x) * rec_ln10; 
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
log10_constexpr(const T x) noexcept { return log10_constexpr(static_cast<double>(x)); }

template <typename T, typename U>
constexpr typename std::conditional<
    std::is_integral<T>::value && std::is_integral<U>::value,
    double,
    typename std::conditional<
        (std::is_integral<T>::value && std::is_floating_point<U>::value) ||
        (std::is_floating_point<T>::value && std::is_integral<U>::value),
        double,
        typename std::common_type<T, U>::type
    >::type
>::type
log_base_constexpr(const T x, const U base) noexcept {
    using result_type = typename std::conditional<
        std::is_integral<T>::value && std::is_integral<U>::value,
        double,
        typename std::conditional<
            (std::is_integral<T>::value && std::is_floating_point<U>::value) ||
            (std::is_floating_point<T>::value && std::is_integral<U>::value),
            double,
            typename std::common_type<T, U>::type
        >::type
    >::type;
    
    return log_constexpr(static_cast<result_type>(x)) / log_constexpr(static_cast<result_type>(base));
}

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
log1p_constexpr(const T x) noexcept {
    if (x <= -1) { return x < -1 ? constants::QUIET_NAN<T> : constants::NEGATIVE_INFINITY<T>; }
    if (abs_constexpr(x) <= constants::MIDDLE_EPSILON<T>) { return 0; }
    if (abs_constexpr(x - 1) <= constants::MIDDLE_EPSILON<T>) { return 0.693147180559945309417232121458176568075500134360255254121; }
    
    if (abs_constexpr(x) < 1) {
        T result = 0;
        T x_power = x;
        int sign = 1;
        std::size_t i = 1;
        
        while (true) {
            const T term = x_power / i;
            result += sign * term;
            x_power *= x;
            sign = -sign;
            if (abs_constexpr(term) <= constants::TYPE_EPSILON<T> || i == 0) { break; }
        }
        
        return result;
    }
    
    return log_constexpr(1 + x);
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type
log1p_constexpr(const T x) noexcept { return log1p_constexpr(static_cast<double>(x)); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_LOG_HPP