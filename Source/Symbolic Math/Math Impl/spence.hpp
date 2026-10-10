#ifndef FIZMO_MATH_SPENCE_IMPL_HPP
#define FIZMO_MATH_SPENCE_IMPL_HPP

#include "polylog.hpp"

namespace fizmo {
namespace math {

static inline double spence_function(double x) { return -dilogarithm(-x); }
static inline double spence_integral(double x) { return dilogarithm(1.0 - x); }

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SPENCE_IMPL_HPP