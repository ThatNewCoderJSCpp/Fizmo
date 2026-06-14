#ifndef FIZMO_CHEBYSHEV_FUNCTIONS_IMPL_HPP
#define FIZMO_CHEBYSHEV_FUNCTIONS_IMPL_HPP

#include "../../Basic/constants.hpp"
#include "../../Standard Overloads/trig.hpp"
#include "../../Standard Overloads/sqrt.hpp"

namespace fizmo {
namespace math {

constexpr static inline double chebyshev_u(double x, double y) noexcept {
    if (abs_constexpr(x) >= 1.0) { return constants::quiet_nan(); }
    return sin_constexpr((y + 1.0) * acos_constexpr(x)) / sqrt_constexpr(1.0 - x * x);
}

constexpr static inline double chebysev_t(double x, double y) noexcept {
    if (abs_constexpr(x) > 1.0) { return constants::quiet_nan(); }
    return cos_constexpr(y * acos_constexpr(x));
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_CHEBYSHEV_FUNCTIONS_IMPL_HPP