#ifndef FIZMO_MATH_RIEMANN_FUNCTIONS_IMPL_HPP
#define FIZMO_MATH_RIEMANN_FUNCTIONS_IMPL_HPP

#include "../../Basic/constants.hpp"
#include "../Calculus/definite_integrator_1d.hpp"
#include "../../Misc Math/alternating_power.hpp"

namespace fizmo {
namespace math {

static inline std::function<double(double)> riemann_zeta_integrand(const double s) noexcept {
    return [s](double t) {
        if (s <= 1.0) { return constants::quiet_nan(); }
        return std::pow(t, s - 1.0) / (std::tgamma(s) * std::expm1(t));
    };
}

static inline double riemann_zeta(double s) {
    if (std::abs(s) <= constants::middle_epsilon()) { return -0.5; }

    if (s > 1.0) {
        return integration::BasicIntegration::integrate_simpson(riemann_zeta_integrand(s), 0.0, fizmo::constants::positive_infinity()).value;
    }

    if (s < 0.0) {
        if (s == std::round(s) && static_cast<long long>(s) % 2 == 0) { return 0.0; }
        return std::pow(2, s) * std::pow(constants::pi(), s - 1) * std::tgamma(1 - s) * std::sin(s * constants::pi_2()) * riemann_zeta(1.0 - s);
    }

    constexpr unsigned int N = 55; 
    double eta = 0.0;
    double half_pow = 0.5;
    
    for (unsigned int n = 0; n < N; ++n) {
        double inner = 0.0;
        double binom = 1.0; 
        
        for (unsigned int k = 0; k <= n; ++k) {
            const double term = binom * std::pow(static_cast<double>(k + 1), -s);
            inner += (k & 1) ? -term : term;
            if (k < n) { binom *= static_cast<double>(n - k) / static_cast<double>(k + 1); }
        }
        
        eta += half_pow * inner;
        half_pow *= 0.5;
    }
    
    return eta / (1.0 - std::pow(2.0, 1.0 - s));
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_RIEMANN_FUNCTIONS_IMPL_HPP