#ifndef TRIG_INTEGRALS_MATH_IMPL_HPP
#define TRIG_INTEGRALS_MATH_IMPL_HPP

#include "../Calculus/definite_integrator_1d.hpp"

namespace fizmo {
namespace math {

static double sin_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return 0.0; }
    std::function<double(double)> inte = [](double t) { return (std::abs(t) <= constants::middle_epsilon()) ? 1.0 : std::sin(t) / t; };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static double cos_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return constants::negative_infinity(); }
    if (x < 0.0) { return constants::quiet_nan(); }
    const double adder = std::log(x) + constants::euler_mascheroni();
    std::function<double(double)> inte = [](double t) { return (std::abs(t) <= constants::middle_epsilon()) ? 0.0 : (std::cos(t) - 1) / t; };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static double sinh_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return 0.0; }
    std::function<double(double)> inte = [](double t) { return (std::abs(t) <= constants::middle_epsilon()) ? 1.0 : std::sinh(t) / t; };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static double cosh_integral(double x) {
    if (std::abs(x) <= constants::middle_epsilon()) { return constants::negative_infinity(); }
    if (x < 0.0) { return constants::quiet_nan(); }
    const double adder = std::log(x) + constants::euler_mascheroni();
    std::function<double(double)> inte = [](double t) { return (std::abs(t) <= constants::middle_epsilon()) ? 0.0 : (std::cosh(t) - 1) / t; };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

} // namespace math
} // namespace fizmo

#endif // TRIG_INTEGRALS_MATH_IMPL_HPP