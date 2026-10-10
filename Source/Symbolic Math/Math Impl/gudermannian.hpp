#ifndef FIZMO_MATH_GUDERMANNIAN_IMPL_HPP
#define FIZMO_MATH_GUDERMANNIAN_IMPL_HPP

#include "../../Basic/constants.hpp"
#include <cmath>

namespace fizmo {
namespace math {

static inline double inverse_gudermannian(double x) {
    if (std::abs(x - constants::pi_2()) <= constants::middle_epsilon()) { return constants::positive_infinity(); }
    if (std::abs(x + constants::pi_2()) <= constants::middle_epsilon()) { return constants::negative_infinity(); }
    if (std::abs(x) >= constants::pi_2()) { return constants::quiet_nan(); }
    return std::atanh(std::sin(x));
}

static inline double gudermannian(double x) {
    if (std::isinf(x)) { return (x > 0.0 ? -1 : 1) * constants::pi_2(); }
    return std::atan(std::sinh(x));
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_GUDERMANNIAN_IMPL_HPP