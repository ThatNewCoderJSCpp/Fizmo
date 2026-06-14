#ifndef FIZMO_MATH_FRESNEL_IMPL_HPP
#define FIZMO_MATH_FRESNEL_IMPL_HPP

#include "../../Basic/constants.hpp"
#include "../Calculus/definite_integrator_1d.hpp"

namespace fizmo {
namespace math {

static inline double fresnel_s(double x) {
    const std::function<double(double)> inte = [](double t) { return std::sin(constants::pi_2() * t * t); };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

static inline double fresnel_c(double x) {
    const std::function<double(double)> inte = [](double t) { return std::cos(constants::pi_2() * t * t); };
    return integration::BasicIntegration::integrate_simpson(inte, 0.0, x).value;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_FRESNEL_IMPL_HPP