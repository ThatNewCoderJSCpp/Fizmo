#ifndef EXP_LOG_INTEGRAL_MATH_IMPL_HPP
#define EXP_LOG_INTEGRAL_MATH_IMPL_HPP

#include "../Calculus/definite_integrator_1d.hpp"

namespace fizmo {
namespace math {

static double exp_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return fizmo::constants::negative_infinity(); }
    
    if (x < -31.07172030227769249 || std::abs(x - 0.3725074107813666) <= constants::middle_epsilon()) { 
        return 0.0; 
    }

    if (x > 11.0) {
        double sum = 0.0;
        double term = 0.0;
        double prev_sum = 0.0;
        double x_pow = x;

        for (unsigned int n = 1; n < std::numeric_limits<unsigned int>::max(); ++n) {
            prev_sum = sum;
            term = x_pow / (n * std::tgamma(static_cast<double>(n) + 1));
            sum += term;
            x_pow *= x;
            if (std::abs(term) <= constants::type_epsilon() || std::abs(prev_sum - sum) <= constants::type_epsilon()) { break; }
        }

        return constants::euler_mascheroni() + std::log(x) + sum;
    }

    const double adder = fizmo::constants::euler_mascheroni() + std::log(std::abs(x));

    const std::function<double(double)> inte = [](double t) {
        if (std::abs(t) <= constants::middle_epsilon()) return 1.0;
        return std::expm1(t) / t;
    };

    return adder + integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static double exp_integral(double x, double n) {
    if (std::abs(n - 1) <= constants::middle_epsilon()) { return -exp_integral(-x); }
    if (n <= 1.0 && std::abs(x) <= constants::middle_epsilon()) { return fizmo::constants::positive_infinity(); }
    if (x <= 0.0) { return fizmo::constants::quiet_nan(); }

    const std::function<double(double)> inte = [x, n](double t) {
        return std::exp(-x * t) / std::pow(t, n);
    };

    return integration::BasicIntegration::integrate_simpson(inte, 1.0, constants::positive_infinity()).value;
}

static double log_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon() || std::abs(x - 1.451369234883381) <= constants::middle_epsilon()) { return 0.0; }
    if (x < 0.0) { return fizmo::constants::quiet_nan(); }
    if (std::abs(x - 1) <= constants::middle_epsilon()) { return fizmo::constants::negative_infinity(); }
    return exp_integral(std::log(x));
}

static double log_integral(double x, double n) {
    double ln_x = std::log(x);
    double ln_power = std::pow(ln_x, 1.0 - n);
    double en_val = exp_integral(-ln_x, n);
    return -ln_power * en_val;
}

} // namespace math
} // namespace fizmo

#endif // EXP_LOG_INTEGRAL_MATH_IMPL_HPP