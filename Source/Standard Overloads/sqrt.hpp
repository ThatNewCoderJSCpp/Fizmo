#ifndef FIZMO_CONSTEXPR_SQRT_HPP
#define FIZMO_CONSTEXPR_SQRT_HPP

#include <type_traits>
#include <limits>
#include "abs.hpp"

namespace fizmo {
namespace math {

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type 
sqrt_constexpr(const T x) noexcept {
    if (x < 0.0) { return std::numeric_limits<T>::quiet_NaN(); }
    if (x == 0.0 || x == 1.0) { return x; }
    T guess = x / 2.0;
    constexpr T epsilon = 1e-18;
    T next_guess;

    while (true) {
        next_guess = (guess + x / guess) / 2.0;
        if (abs_constexpr(next_guess - guess) < epsilon * next_guess) { break; }
        guess = next_guess;
    }
    
    return next_guess;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type 
sqrt_constexpr(const T x) noexcept {
    if (x < 0) { return std::numeric_limits<double>::quiet_NaN(); }
    if (x == 0 || x == 1) { return x; }
    return sqrt_constexpr(static_cast<double>(x));
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, double>::type 
cbrt_constexpr(const T x) noexcept {
    if (x == 1 || x == -1 || x == 0) { return x; }
    
    const bool is_negative = x < 0;
    const double abs_x = is_negative ? -static_cast<double>(x) : static_cast<double>(x);
    
    double guess = abs_x / 3.0;
    constexpr double epsilon = 1e-10;
    double next_guess = 0.0;

    while (true) {
        next_guess = (2.0 * guess + abs_x / (guess * guess)) / 3.0;
        if (abs_constexpr(next_guess - guess) < epsilon * abs_constexpr(next_guess)) { break; }
        guess = next_guess;
    }
    
    return is_negative ? -next_guess : next_guess;
}

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type 
cbrt_constexpr(const T x) noexcept {
    if (x == 0.0) { return 0.0; }
    if (x == 1.0 || x == -1.0) { return x; }
    
    const bool is_negative = x < 0;
    const T abs_x = is_negative ? -x : x;
    T guess = abs_x / 3.0;
    constexpr T epsilon = 1e-10;
    T next_guess;

    while (true) {
        next_guess = (2.0 * guess + abs_x / (guess * guess)) / 3.0;
        if (abs_constexpr(next_guess - guess) < epsilon * abs_constexpr(next_guess)) { break; }
        guess = next_guess;
    }
    
    return is_negative ? -next_guess : next_guess;
}

namespace detail {
    template <typename B>
    constexpr typename std::enable_if<std::is_integral<B>::value, bool>::type
    is_even_helper(const B n) noexcept { return n % 2 == 0; }

    template <typename B>
    constexpr typename std::enable_if<std::is_floating_point<B>::value, bool>::type
    is_even_helper(const B n) noexcept {
        const B n_floor = std::floor(n);
        return (n == n_floor) && (std::fmod(n_floor, 2.0) == 0.0);
    }

    template <typename T, typename B>
    constexpr typename std::enable_if<std::is_integral<B>::value, T>::type
    power_helper(T base, B exp) noexcept {
        if (exp == 0) return static_cast<T>(1);
        if (exp == 1) return base;
        T result = static_cast<T>(1);
        T current_base = base;
        B current_exp = exp < 0 ? -exp : exp;
        
        while (current_exp > 0) {
            if (current_exp & 1) { result *= current_base; }
            current_base *= current_base;
            current_exp >>= 1;
        }
        
        return exp < 0 ? static_cast<T>(1) / result : result;
    }
} // namespace detail

template <typename T, typename B>
constexpr typename std::enable_if<
    std::is_integral<B>::value,
    typename std::conditional<
        std::is_integral<T>::value,
        double,
        T
    >::type
>::type
nth_root_constexpr(const T x, const B n) noexcept {
    using result_type = typename std::conditional<
        std::is_integral<T>::value,
        double,
        T
    >::type;
    
    if (n == 0) { return std::numeric_limits<result_type>::quiet_NaN(); }
    if (x == 0) { return static_cast<result_type>(0); }
    if (x == 1) { return static_cast<result_type>(1); }
    if (n < 0) { return 1 / nth_root_constexpr(x, abs_constexpr(n)); }
    
    const bool is_negative = x < 0;
    if (is_negative && is_even_helper(n)) { return std::numeric_limits<result_type>::quiet_NaN(); }
    const result_type abs_x = is_negative ? -static_cast<result_type>(x) : static_cast<result_type>(x);
    
    if (n == 1) { return static_cast<result_type>(x); }
    if (n == 2) { return sqrt_constexpr(abs_x) * (is_negative ? -1 : 1); }
    if (n == 3) { return cbrt_constexpr(abs_x) * (is_negative ? -1 : 1); }
    
    const result_type root_index = static_cast<result_type>(n);
    result_type guess = abs_x / root_index;  
    constexpr result_type epsilon = 1e-15;
    result_type next_guess;

    for (std::size_t iterations = 0; iterations < std::numeric_limits<std::size_t>::max(); ++iterations) {
        result_type power = power_helper(guess, n - 1);
        next_guess = ((root_index - 1) * guess + abs_x / power) / root_index; 
        if (abs_constexpr(next_guess - guess) < epsilon * abs_constexpr(next_guess)) { break; }
        guess = next_guess;
    }
    
    return is_negative ? -next_guess : next_guess;
}

namespace detail {
    template <typename T, typename B>
    constexpr typename std::enable_if<std::is_floating_point<B>::value, T>::type
    power_helper(T base, B exp) noexcept {
        if (base == 0) return static_cast<T>(0);
        if (base == 1) return static_cast<T>(1);
        if (exp == 0) return static_cast<T>(1);
        if (exp == 1) return base;
        const B exp_floor = static_cast<B>(static_cast<long long>(exp));
        const B exp_frac = exp - exp_floor;
        if (abs_constexpr(exp_frac) < 1e-15) { return power_helper(base, static_cast<long long>(exp_floor)); }
        const long long q = 1000000; 
        const long long p = static_cast<long long>(exp * q);
        T base_p = power_helper(base, p);
        return nth_root_constexpr(base_p, q);
    }
} // namespace detail

template <typename T, typename B>
constexpr typename std::enable_if<
    std::is_floating_point<B>::value,
    double
>::type
nth_root_constexpr(const T x, const B n) noexcept {
    if (n == 0.0) { return std::numeric_limits<double>::quiet_NaN(); }
    if (x == 0) { return 0.0; }
    if (x == 1) { return 1.0; }
    if (n == 1.0) { return static_cast<double>(x); }
    if (n < 0) { return 1 / nth_root_constexpr(x, abs_constexpr(n)); }
    const double dx = static_cast<double>(x);
    const double dn = static_cast<double>(n);
    const bool is_negative = dx < 0;
    if (is_negative && detail::is_even_helper(n)) { return std::numeric_limits<double>::quiet_NaN(); }
    const double abs_x = is_negative ? -dx : dx;
    double guess = abs_x / dn;
    constexpr double epsilon = 1e-15;
    
    for (std::size_t iterations = 0; iterations < std::numeric_limits<std::size_t>::max(); ++iterations) {
        double power_n_minus_1 = 0.0;
        
        if (abs_constexpr(dn - 2.0) < epsilon) {
            power_n_minus_1 = 1.0; 
            power_n_minus_1 = 1.0;
        } else if (abs_constexpr(dn - 3.0) < epsilon) {
            power_n_minus_1 = guess * guess; 
        } else {
            const double exp_approx = dn - 1.0;
            power_n_minus_1 = 1.0;
            double base = guess;
            double exp_remaining = exp_approx;
            long long int_part = static_cast<long long>(exp_remaining);
            double frac_part = exp_remaining - int_part;
            if (int_part > 0) { power_n_minus_1 = detail::power_helper(base, int_part); }
            
            if (abs_constexpr(frac_part) > epsilon) {
                // For small fractional parts, use linear approximation
                // This is a simplification - more accurate methods exist
                power_n_minus_1 *= (1.0 + frac_part * (base - 1.0) / base);
            }
        }
        
        const double next_guess = ((dn - 1.0) * guess + abs_x / power_n_minus_1) / dn;
        
        if (abs_constexpr(next_guess - guess) < epsilon * abs_constexpr(next_guess)) {
            guess = next_guess;
            break;
        }
        
        guess = next_guess;
    }
    
    return is_negative ? -guess : guess;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_CONSTEXPR_SQRT_HPP