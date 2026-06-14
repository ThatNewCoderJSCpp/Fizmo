#ifndef FIZMO_LAMBERT_W_FUNCTION_IMPL_HPP
#define FIZMO_LAMBERT_W_FUNCTION_IMPL_HPP

#include "../../Basic/constants.hpp"
#include <cmath>

namespace fizmo {
namespace math {

static inline double lambert_w0(double x) {
    if (std::isnan(x))  return constants::quiet_nan();
    if (std::isinf(x))  return x > 0.0 ? constants::positive_infinity() : constants::quiet_nan();
    constexpr double neg_inv_e = -constants::reciprocal_e();
    if (x < neg_inv_e)  return constants::quiet_nan();
    constexpr double eps = constants::middle_epsilon();
    if (std::abs(x - neg_inv_e) <= eps) return -1.0;
    if (std::abs(x)             <= eps) return  0.0;
    if (std::abs(x - 1.0)       <= eps) return  constants::omega();
    if (std::abs(x - constants::euler()) <= eps) return 1.0;
    double guess;
    if (x > constants::euler()) {
        double l1 = std::log(x);
        double l2 = std::log(l1);
        double r  = l2 / l1;
        guess = l1 - l2 + r + r * (l2 - 2.0) / (2.0 * l1);
    } else if (x >= 0.0) {
        guess = x / (1.0 + x);
    } else {
        double p = std::sqrt(2.0 * (1.0 + constants::euler() * x));
        guess = -1.0 + p - (1.0 / 3.0) * p * p;
    }
    double w = guess;
    for (int i = 0; i <= 64; ++i) {
        double ew  = std::exp(w);
        double wew = w * ew;
        double f   = wew - x;
        if (std::abs(f) <= eps * (std::abs(x) + 1.0)) break;
        double fp  = (w + 1.0) * ew;
        double fpp = (w + 2.0) * ew;
        w -= f / (fp - 0.5 * f * fpp / fp);
    }
    return w;
}

static inline double lambert_wn1(double x) {
    if (std::isnan(x))  return constants::quiet_nan();
    constexpr double neg_inv_e = -constants::reciprocal_e();
    if (x < neg_inv_e || x >= 0.0) return constants::quiet_nan();
    constexpr double eps = constants::middle_epsilon();
    if (std::abs(x - neg_inv_e) <= eps) return -1.0;
    double guess;
    if (x < -0.3) {
        double p = std::sqrt(2.0 * (1.0 + constants::euler() * x));
        guess = -1.0 - p - (1.0 / 3.0) * p * p;
    } else if (x < -0.05) {
        double l1 = std::log(-x);
        double l2 = std::log(-l1);
        guess = l1 - l2;
    } else {
        double l1 = std::log(-x);
        double l2 = std::log(-l1);
        guess = l1 - l2 + l2 / l1 + l2 * (l2 - 2.0) / (2.0 * l1 * l1);
    }
    if (guess > -1.0) guess = -1.0 - std::abs(guess + 1.0);
    double w = guess;
    for (int i = 0; i < 64; ++i) {
        double ew  = std::exp(w);
        double wew = w * ew;
        double f   = wew - x;
        if (std::abs(f) <= eps * (std::abs(x) + 1.0)) break;
        double fp  = (w + 1.0) * ew;
        double fpp = (w + 2.0) * ew;
        w -= f / (fp - 0.5 * f * fpp / fp);
    }

    if (w > -1.0 + eps) return constants::quiet_nan();
    return w;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_LAMBERT_W_FUNCTION_IMPL_HPP