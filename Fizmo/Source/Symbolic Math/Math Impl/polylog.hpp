#ifndef FIZMO_MATH_POLYLOG_IMPL_HPP
#define FIZMO_MATH_POLYLOG_IMPL_HPP

#include "riemann.hpp"

namespace fizmo {
namespace math {

static inline double dilogarithm(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return 0.0; }
    if (std::abs(x - 1.0) <= constants::middle_epsilon()) { return constants::pi() * constants::pi() / 6.0; }
    if (x > 1.0) { return constants::quiet_nan(); }

    if (std::abs(x) <= 1.0) {
        double x_pow = x;
        double term = 0.0;
        double sum = 0.0;
        double last_sum = 0.0;

        for (std::size_t k = 1; k < std::numeric_limits<std::size_t>::max(); ++k) {
            last_sum = sum;
            term = x_pow / (k * k);
            if (std::abs(sum - last_sum) <= constants::middle_epsilon() || std::abs(term) <= constants::middle_epsilon()) { break; }
            x_pow *= x;
        }

        return sum;
    }

    const std::function<double(double)> inte = [](double t) { 
        if (std::abs(t) <= constants::middle_epsilon()) { return 1.0; }
        return -std::log(1.0 - t) / t; 
    };

    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static inline double trilogarithm(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return 0.0; }
    if (std::abs(x - 1.0) <= constants::middle_epsilon()) { return constants::apery(); /* riemann_zeta(3) */ }
    if (x > 1.0) { return constants::quiet_nan(); }

    if (std::abs(x) <= 1.0) {
        double x_pow = x;
        double term = 0.0;
        double sum = 0.0;
        double last_sum = 0.0;

        for (std::size_t k = 1; k < std::numeric_limits<std::size_t>::max(); ++k) {
            last_sum = sum;
            term = x_pow / (k * k * k);
            if (std::abs(sum - last_sum) <= constants::middle_epsilon() || std::abs(term) <= constants::middle_epsilon()) { break; }
            x_pow *= x;
        }

        return sum;
    }

    const std::function<double(double)> inte = [x](double t) {  return (t * t) / (std::exp(t) - x); };
    return 0.5 * x * integration::BasicIntegration::integrate_simpson(inte, 0.0, constants::positive_infinity()).value;
}

static inline double polylog(double x, double n) {
    if (std::abs(x) <= constants::middle_epsilon()) { return 0.0; }
    const bool n_is_int = n == std::floor(n);

    if (
        (
            (n_is_int && n > 0.0) ||
            (!n_is_int && n < 0.0)
        ) && x >= 1.0
    ) { return fizmo::constants::quiet_nan(); }

    if (std::abs(x + 1) <= fizmo::constants::middle_epsilon()) { return (std::pow(2, 1.0 - n) - 1.0) * riemann_zeta(n); }
 
    if (std::abs(x - 1) <= fizmo::constants::middle_epsilon()) {
        if (std::abs(n - 1) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::positive_infinity(); }
        if (n < 1) { return fizmo::constants::quiet_nan(); }
        return riemann_zeta(n);
    }

    if (n_is_int) {
        const double X = x;
        const double X_2 = X * X;
        const double X_3 = X_2 * X;
        const double X_4 = X_3 * X;
        const double X_5 = X_4 * X;
        const double X_6 = X_5 * X;
        const double X_7 = X_6 * X;
        const double X_8 = X_7 * X;
        const double X_9 = X_8 * X;
        const double X_10 = X_9 * X;
        const double negative_multiplier = std::pow(1.0 - X, static_cast<long long>(n) - 1);

        switch (static_cast<long long>(n)) {
            case -10: return negative_multiplier * (X_10 + 1013 * X_9 + 47840 * X_8 + 455192 * X_7 + 1310354 * X_6 + 1310354 * X_5 + 455192 * X_4 + 47840 * X_3 + 1013 * X_2 + X);
            case -9: return negative_multiplier * (X_9 + 502 * X_8 + 14608 * X_7 + 88234 * X_6 + 156190 * X_5 + 88234 * X_4 + 14608 * X_3 + 502 * X_2 + X);
            case -8: return negative_multiplier * (X_8 + 247 * X_7 + 4293 * X_6 + 15619 * X_5 + 15619 * X_4 + 4293 * X_3 + 247 * X_2 + X);
            case -7: return negative_multiplier * (X_7 + 120 * X_6 + 1191 * X_5 + 2416 * X_4 + 1191 * X_3 + 120 * X_2 + X);
            case -6: return negative_multiplier * (X_6 + 57 * X_5 + 302 * X_4 + 302 * X_3 + 57 * X_2 + X);
            case -5: return negative_multiplier * (X_5 + 26 * X_4 + 66 * X_3 + 26 * X_2 + X);
            case -4: return negative_multiplier * (X_4 + 11 * X_3 + 11 * X_2 + X);
            case -3: return negative_multiplier * (X_3 + 4 * X_2 + X);
            case -2: return negative_multiplier * (X_2 + X);
            case -1: return negative_multiplier * X;
            case 0: return X / (1 - X);
            case 1: return -std::log(1 - X);
            case 2: return dilogarithm(x);
            case 3: return trilogarithm(x);
        };
    }

    if (std::abs(x) < 1.0) {
        double sum = 0.0;
        double prev_sum = 0.0;
        double term = 0.0;
        double x_pow = x;

        for (std::uint64_t k = 1; k < std::numeric_limits<std::uint64_t>::max(); ++k) {
            prev_sum = sum;
            term = x_pow / std::pow(k, n);
            sum += term;
            x_pow *= x;
            if (std::abs(term) <= constants::middle_epsilon() || std::abs(sum - prev_sum) <= constants::middle_epsilon()) { break; }
        }

        return sum;
    } else if (n < 0.0 && n_is_int) {
        return -alternating_power(-static_cast<long long>(n)) * polylog(1.0 / x, n);
    } else if (n > 0.0) {
        const double mult = 1.0 / std::tgamma(n);

        const std::function<double(double)> inte = [x, n](double t) {
            return std::pow(t, n - 1) / (t / x - 1); 
        };

        return integration::BasicIntegration::integrate_simpson(inte, 0.0, constants::positive_infinity()).value / std::tgamma(n);
    } else if (n < 0.0) {
        const double mu = std::log(std::abs(x));  
        constexpr double pi = constants::pi();
        const double r = std::sqrt(mu * mu + pi * pi);
        // Convergence radius: |ln(x)| < 2π
        if (r >= 2.0 * pi) { return fizmo::constants::quiet_nan(); }
        const double theta_neg = std::atan2(-pi, -mu);
        const double theta_pos = std::atan2(pi, mu);
        const double gamma_term = std::tgamma(1.0 - n)  * std::pow(r, n - 1.0)  * std::cos((n - 1.0) * theta_neg);
        double sum = 0.0;
        double r_pow = 1.0;
        double factorial = 1.0;
        double prev_sum = 0.0;
        double term = 0.0;
        
        for (unsigned int k = 0; k < std::numeric_limits<unsigned int>::max(); ++k) {
            prev_sum = sum;
            const double zeta_val = riemann_zeta(n - static_cast<double>(k));
            term = zeta_val * r_pow / factorial * std::cos(k * theta_pos);
            sum += term;
            r_pow *= r;
            factorial *= (k + 1);
            if (std::abs(term) <= fizmo::constants::middle_epsilon() || std::abs(sum - prev_sum) <= fizmo::constants::middle_epsilon()) break;
        }
        
        return gamma_term + sum;
    }

    return 1.23412341237;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_POLYLOG_IMPL_HPP