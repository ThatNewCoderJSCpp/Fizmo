#ifndef FIZMO_RELATIVISTIC_MOMENTUM_HPP
#define FIZMO_RELATIVISTIC_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N relativistic_momentum_(const N& beta, const N& m) {
    if (neg_(m)) return nan_<N>();
    return gamma_(beta) * m * beta * c_<N>();
}

template <class N>
constexpr N relativistic_momentum_velocity_(const N& p, const N& m) {
    if (!(m > N(0)) || neg_(p)) return nan_<N>();
    const N one(1);
    const N x = p / (m * c_<N>());
    if (x > one) {
        const N q = one / x;
        return one / sqrt_(one + q * q);
    }
    return x / sqrt_(one + x * x);
}

template <class N>
constexpr N relativistic_momentum_mass_(const N& beta, const N& p) {
    if (neg_(p)) return nan_<N>();
    const N s = s_(beta);
    if (beta == N(0) || is_nan_(s)) return nan_<N>();
    return p * s / (beta * c_<N>());
}

} // namespace rdetail

template <typename V, typename M, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<V, M> relativistic_momentum(const V& velocity, const M& mass, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_momentum_(rdetail::to_<N>(velocity), rdetail::to_<N>(mass)), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename V, typename M, class VU, class MU, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<V, M> relativistic_momentum(const V& velocity, const M& mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::relativistic_momentum_(v, m), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename P, typename M, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<P, M> relativistic_momentum_velocity(const P& momentum, const M& mass, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_momentum_velocity_(rdetail::to_<N>(momentum), rdetail::to_<N>(mass)), units::velocity::light_speed, return_unit);
    });
}

template <typename P, typename M, class PU, class MU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<P, M> relativistic_momentum_velocity(const P& momentum, const M& mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::relativistic_momentum_velocity_(p, m), units::velocity::light_speed, return_unit);
    });
}

template <typename P, typename V, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<P, V>::value && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<P, V> relativistic_momentum_mass(const V& velocity, const P& momentum, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<P, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_momentum_mass_(rdetail::to_<N>(velocity), rdetail::to_<N>(momentum)), units::mass::kilogram, return_unit);
    });
}

template <typename P, typename V, class VU, class PU, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<P, V>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<P, V> relativistic_momentum_mass(const V& velocity, const P& momentum, const VU velocity_unit, const PU momentum_unit, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<P, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        return rdetail::conv(rdetail::relativistic_momentum_mass_(v, p), units::mass::kilogram, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_MOMENTUM_HPP
