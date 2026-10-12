#ifndef FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP
#define FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N hypot_(const N& a, const N& b) {
    const N x = abs_(a);
    const N y = abs_(b);
    const N big   = (x > y) ? x : y;
    const N small = (x > y) ? y : x;
    if (big == N(0)) return N(0);
    const N r = small / big;
    return big * sqrt_(N(1) + r * r);
}

template <class N>
constexpr N energy_momentum_energy_(const N& p, const N& m) {
    if (neg_(m) || neg_(p)) return nan_<N>();
    const N c = c_<N>();
    return hypot_(p * c, m * c * c);
}

template <class N>
constexpr N energy_momentum_momentum_(const N& e, const N& m) {
    if (neg_(m) || neg_(e)) return nan_<N>();
    const N c   = c_<N>();
    const N mc2 = m * c * c;
    if (mc2 > e) return nan_<N>();
    return sqrt_((e - mc2) * (e + mc2)) / c;
}

template <class N>
constexpr N energy_momentum_mass_(const N& e, const N& p) {
    if (neg_(e) || neg_(p)) return nan_<N>();
    const N c  = c_<N>();
    const N pc = p * c;
    if (pc > e) return nan_<N>();
    return sqrt_((e - pc) * (e + pc)) / (c * c);
}

template <class N>
constexpr N energy_momentum_velocity_(const N& e, const N& p) {
    if (!(e > N(0)) || neg_(p)) return nan_<N>();
    const N beta = p * c_<N>() / e;
    if (beta > N(1)) return nan_<N>();
    return beta;
}

} // namespace rdetail

template <typename P, typename M, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<P, M> energy_momentum_energy(const P& momentum, const M& mass, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::energy_momentum_energy_(rdetail::to_<N>(momentum), rdetail::to_<N>(mass)), units::energy::joule, return_unit);
    });
}

template <typename P, typename M, class PU, class MU, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<P, M> energy_momentum_energy(const P& momentum, const M& mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::energy_momentum_energy_(p, m), units::energy::joule, return_unit);
    });
}

template <typename E, typename M, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<E, M> energy_momentum_momentum(const E& energy, const M& mass, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::energy_momentum_momentum_(rdetail::to_<N>(energy), rdetail::to_<N>(mass)), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename E, typename M, class EU, class MU, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<E, M> energy_momentum_momentum(const E& energy, const M& mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(energy, energy_unit, units::energy::joule);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::energy_momentum_momentum_(e, m), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename E, typename P, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<E, P>::value && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<E, P> energy_momentum_mass(const E& energy, const P& momentum, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<E, P>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::energy_momentum_mass_(rdetail::to_<N>(energy), rdetail::to_<N>(momentum)), units::mass::kilogram, return_unit);
    });
}

template <typename E, typename P, class EU, class PU, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<E, P>::value && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<E, P> energy_momentum_mass(const E& energy, const P& momentum, const EU energy_unit, const PU momentum_unit, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<E, P>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(energy, energy_unit, units::energy::joule);
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        return rdetail::conv(rdetail::energy_momentum_mass_(e, p), units::mass::kilogram, return_unit);
    });
}

template <typename E, typename P, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<E, P>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<E, P> energy_momentum_velocity(const E& energy, const P& momentum, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<E, P>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::energy_momentum_velocity_(rdetail::to_<N>(energy), rdetail::to_<N>(momentum)), units::velocity::light_speed, return_unit);
    });
}

template <typename E, typename P, class EU, class PU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<E, P>::value && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<E, P> energy_momentum_velocity(const E& energy, const P& momentum, const EU energy_unit, const PU momentum_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<E, P>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(energy, energy_unit, units::energy::joule);
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        return rdetail::conv(rdetail::energy_momentum_velocity_(e, p), units::velocity::light_speed, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP
