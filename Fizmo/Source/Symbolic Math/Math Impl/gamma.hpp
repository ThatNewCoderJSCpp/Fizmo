#ifndef FIZMO_MATH_GAMMA_IMPL_HPP
#define FIZMO_MATH_GAMMA_IMPL_HPP

#include "../../Basic/constants.hpp"
#include "../../Standard Overloads/log.hpp"
#include "../../Standard Overloads/trig.hpp"
#include "../Calculus/definite_integrator_1d.hpp"

namespace fizmo {
namespace math {

constexpr static inline double digamma(double x) noexcept { 
    double result = 0.0; 
    constexpr double PI = constants::pi();
    constexpr double eps = constants::middle_epsilon();
    if (x < 0.5) { return digamma(1.0 - x) - PI * cos_constexpr(PI * x) / sin_constexpr(PI * x); } 
    while (x < 8.0) { result -= 1.0 / x; x += 1.0; } 
    double inv = 1.0 / x; 
    double inv2 = inv * inv; 
    return result + log_constexpr(x) - 0.5 * inv
    - inv2 * (
        1.0/12.0 -
        inv2 * (1.0/120.0 -
        inv2 * (1.0/252.0 -
        inv2 * (1.0/240.0 -
        inv2 * (1.0/132.0 -
        inv2 * (1.0/32760.0 -
        inv2 * (1.0/12.0)
    ))))));
} 
    
constexpr static inline double trigamma(double x) noexcept { 
    constexpr double PI = constants::pi();
    constexpr double eps = constants::middle_epsilon();
    double result = 0.0; 
    if (x < 0.5) { double s = sin_constexpr(PI * x); const double ps = PI / s; const double ps2 = ps * ps; return ps2 - trigamma(1.0 - x); } 
    while (x < 8.0) { result += 1.0 / (x * x); x += 1.0; } 
    double inv = 1.0 / x; 
    double inv2 = inv * inv; 
    return result + inv + 0.5 * inv2
    + inv2 * inv * (
        1.0/6.0 -
        inv2 * (1.0/30.0 -
        inv2 * (1.0/42.0 -
        inv2 * (1.0/30.0 -
        inv2 * (5.0/66.0 -
        inv2 * (691.0/2730.0 -
        inv2 * (7.0/6.0)
    ))))));
}

static inline double beta_function(double x, double y) {
    return (std::tgamma(x) * std::tgamma(y)) / std::tgamma(x + y);
}

static inline double polygamma(double x, int n) {
    if (x == 0.0) {
        if (n >= 0) { return fizmo::constants::quiet_nan(); }
        if (n == -1) { return fizmo::constants::positive_infinity(); }
        return 0.0;
    }

    if (x == std::floor(x) && x < 0) { return n == -1 ? constants::positive_infinity() : constants::quiet_nan(); }
    if (n == 0) { return digamma(x); }
    if (n == 1) { return trigamma(x); }
    if (x < 0.0 && n < 0) { return constants::quiet_nan(); }

    if (n > 1) {
        if (x > 0) {
            const std::function<double(double)> inte = [x, n](double t) {
                return (std::exp(t * (1.0 - x)) * std::pow(t, n)) / std::expm1(t);
            };

            return -alternating_power(n) * integration::BasicIntegration::integrate_simpson(inte, constants::middle_epsilon(), constants::positive_infinity()).value;
        } else if (std::abs(x) <= 1) {
            const double mult = -alternating_power(n) * std::tgamma(n + 1);
            double sum = 0.0;
            double prev_sum = 0.0;
            double term = 0.0;

            for (std::uint64_t k = 0; k < std::numeric_limits<std::uint64_t>::max(); ++k) {
                prev_sum = sum;
                term = std::pow(x + k, -(n + 1));
                sum += term;
                if (std::abs(sum - prev_sum) <= fizmo::constants::type_epsilon() || std::abs(term) <= fizmo::constants::type_epsilon()) { break; }
            }

            return mult * sum;
        } else {
            double shifted_x = x;
            double correction = 0.0;
            const double mult = alternating_power(n) * std::tgamma(n + 1);
            
            while (shifted_x < -1.0) {
                correction += mult * std::pow(shifted_x, -(n + 1));
                shifted_x += 1.0;
            }
            
            return polygamma(shifted_x, n) - correction;
        }
    } else if (n < 0) {
        const double x_pow = std::pow(x, -(n + 1));
        const double t1 = constants::euler_mascheroni() * x_pow;
        const double t2 = x_pow * std::log(x);
        const double t3 = x_pow * digamma(-static_cast<double>(n)); 
        const double sum_term = t1 - t2 + t3;

        const std::function<double(double)> inte = [x, n](double t) {
            return std::pow(x - t, -(n + 1)) * digamma(t + 1.0);
        };

        return (sum_term + integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value) / std::tgamma(-static_cast<double>(n));
    } 

    return constants::positive_infinity();
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_GAMMA_IMPL_HPP