#ifndef FIZMO_MATH_FIBONACCI_LUCAS_IMPL_HPP
#define FIZMO_MATH_FIBONACCI_LUCAS_IMPL_HPP

#include "../../Basic/constants.hpp"
#include <cmath>

namespace fizmo {
namespace math {

static inline double fibonacci(double x) noexcept {
    constexpr double phi = constants::phi();
    constexpr double psi = -constants::psi();
    return (std::pow(phi, x) - std::pow(psi, x) * std::cos(constants::pi() * x)) / std::sqrt(5);
}

static inline double lucas(double x) noexcept {
    constexpr double phi = constants::phi();
    constexpr double psi = -constants::psi();
    return std::pow(phi, x) + std::pow(psi, x) * std::cos(constants::pi() * x);
}

static inline double fibonacci_polynomial(double x, int n) noexcept {
    const double a = 0.5 * (x + std::sqrt(x * x + 4));
    const double b = 0.5 * (x - std::sqrt(x * x + 4));
    return (std::pow(a, n) - std::pow(b, n)) / (a - b);
}

static inline double lucas_polynomial(double x, int n) noexcept {
    const double a = 0.5 * (x + std::sqrt(x * x + 4));
    const double b = 0.5 * (x - std::sqrt(x * x + 4));
    return std::pow(a, n) + std::pow(b, n);
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_FIBONACCI_LUCAS_IMPL_HPP