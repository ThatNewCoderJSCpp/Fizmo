#ifndef FIZMO_MATH_ROGERS_L_IMPL_HPP
#define FIZMO_MATH_ROGERS_L_IMPL_HPP

#include "polylog.hpp"

namespace fizmo {
namespace math {

// L_R(x)
static inline double rogers_dilogarithm(double x) { return dilogarithm(x) + 0.5 * std::log(x) * std::log(1.0 - x); }

// L(x)
static inline double rogers_function(double x) { return rogers_dilogarithm(x) * 6.0 * constants::reciprocal_pi_squared(); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_ROGERS_L_IMPL_HPP