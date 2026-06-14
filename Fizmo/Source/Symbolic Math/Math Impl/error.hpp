#ifndef FIZMO_MATH_ERF_IMPL_HPP
#define FIZMO_MATH_ERF_IMPL_HPP

#include "../../Basic/constants.hpp"
#include "../../Misc Math/alternating_power.hpp"
#include <cmath>

namespace fizmo {
namespace math {

static constexpr double inverse_erf_coefficient(const unsigned int k) noexcept {
    if (k == 0) { return 1; }
    double sum = 0.0;

    for (unsigned int n = 0; n < k; ++n) {
        const double denom = (2 * static_cast<double>(n) + 3) * static_cast<double>(n) + 1;
        const double num = inverse_erf_coefficient(k - n - 1) * inverse_erf_coefficient(n);
        sum += num / denom;
    }

    return sum;
}

static inline double inverse_erf(const double x) noexcept {
    if (x < 0) { return -inverse_erf(-x); }
    if (std::abs(x - 1) == 0) { return fizmo::constants::positive_infinity(); }
    if (x == 0) { return 0.0; }
    if (x > 1) { return fizmo::constants::quiet_nan(); }
    double sum = 0;

    for (unsigned int k = 0; k <= 10; ++k) {
        const double pow_2 = std::pow(static_cast<double>(2), -(2 * static_cast<double>(k) + 1));
        const double pow_pi = std::pow(fizmo::constants::pi(), k + 0.5);
        const double pow_x = std::pow(x, 2 * k + 1);
        const double num = pow_2 * pow_pi * pow_x * inverse_erf_coefficient(k);
        const double denom = 2 * static_cast<double>(k) + 1;
        sum += num / denom;
    }

    return sum;
}

static inline double inverse_erfc(const double x) noexcept { return inverse_erf(1.0 - x); }

constexpr static inline double erfi(double x) noexcept {
    if (x < 0.0) return -erfi(-x);
    if (x >= 20.0) { const double inv = constants::reciprocal_sqrt_pi() / x; return std::exp(x * x) * inv; }
    unsigned int N = 4 * (static_cast<unsigned int>(x) + 25);
    constexpr double two_over_sqrt_pi = 2.0 * constants::reciprocal_sqrt_pi(); 
    double term = x; 
    double sum = x; 
    double x2 = x * x; 
    for (unsigned n = 1; n <= N; ++n) { 
        term *= x2 / n; 
        sum += term / (2 * n + 1); 
    } 
    return two_over_sqrt_pi * sum;
}

static inline double inverse_erfi(const double y) noexcept {
    if (y == 0.0) return 0.0;
    const double sign = (y < 0.0) ? -1.0 : 1.0;
    const double ay   = std::abs(y);
    if (ay <= 1.0) {
        const double t = constants::sqrt_pi() * 0.5 * ay;
        double mult = t; 
        double sum  = 0.0;
        for (unsigned int k = 0; k <= 15; ++k) {
            const double coeff = alternating_power(k) * inverse_erf_coefficient(k);
            const double term = mult * coeff / static_cast<double>(2 * k + 1);
            sum += term;
            mult *= t * t; 
            if (std::abs(term) <= constants::middle_epsilon()) break;
        }
        return sign * sum;
    }
    double x;
    {
        const double L_raw = std::log(ay * constants::sqrt_pi());
        const double L     = std::max(L_raw, 1.0);
        const double inner = L - 0.5 * std::log(L);
        x = std::sqrt(inner);
    }
    if (x > 15.0) x = 15.0;
    const double reciprocal_sqrt_pi = constants::reciprocal_sqrt_pi();
    const double eps = constants::type_epsilon();
    for (unsigned int iter = 0; iter < 6; ++iter) {
        const double erfi_x = erfi(x);
        const double f      = erfi_x - ay;
        if (std::abs(f) <= eps * (ay + 1.0)) break;
        const double x2  = x * x;
        const double ex2 = std::exp(x2);
        const double fp  = 2.0 * reciprocal_sqrt_pi * ex2;
        const double fpp = 4.0 * x * reciprocal_sqrt_pi * ex2;
        const double denom = fp - 0.5 * f * fpp / fp;
        const double delta = f / denom;
        x -= delta;
        if (!std::isfinite(x)) break;
    }

    return sign * x;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_ERF_IMPL_HPP