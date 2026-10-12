#ifndef FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP
#define FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N kinetic_energy_(const N& beta, const N& m) {
    if (neg_(m)) return nan_<N>();
    const N c = c_<N>();
    return m * c * c * gamma_minus_one_(beta);
}

template <class N>
constexpr N kinetic_energy_from_momentum_(const N& p, const N& m) {
    if (neg_(m)) return nan_<N>();
    const N c   = c_<N>();
    const N pc  = abs_(p) * c;
    const N mc2 = m * c * c;
    if (pc == N(0)) return N(0);
    return pc * pc / (sqrt_(pc * pc + mc2 * mc2) + mc2);
}

template <class N>
constexpr N kinetic_energy_velocity_(const N& ke, const N& m) {
    if (!(m > N(0)) || neg_(ke)) return nan_<N>();
    const N c = c_<N>();
    const N x = ke / (m * c * c);
    return sqrt_(x * (N(2) + x)) / (N(1) + x);
}

template <class N>
constexpr N kinetic_energy_momentum_(const N& ke, const N& m) {
    if (neg_(m) || neg_(ke)) return nan_<N>();
    const N c = c_<N>();
    return sqrt_(ke * (ke + N(2) * m * c * c)) / c;
}

template <class N>
constexpr N kinetic_energy_mass_(const N& beta, const N& ke) {
    if (neg_(ke)) return nan_<N>();
    const N g = gamma_minus_one_(beta);
    if (g == N(0)) return nan_<N>();
    const N c = c_<N>();
    return ke / (g * c * c);
}

} // namespace rdetail

template <typename V, typename M, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<V, M> kinetic_energy(const V& velocity, const M& mass, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::kinetic_energy_(rdetail::to_<N>(velocity), rdetail::to_<N>(mass)), units::energy::joule, return_unit);
    });
}

template <typename V, typename M, class VU, class MU, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<V, M> kinetic_energy(const V& velocity, const M& mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::kinetic_energy_(v, m), units::energy::joule, return_unit);
    });
}

template <typename P, typename M, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<P, M> kinetic_energy_from_momentum(const P& momentum, const M& mass, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::kinetic_energy_from_momentum_(rdetail::to_<N>(momentum), rdetail::to_<N>(mass)), units::energy::joule, return_unit);
    });
}

template <typename P, typename M, class PU, class MU, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<P, M>::value && rdetail::has_dim<PU>(units::DIMENSION_MOMENTUM) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<P, M> kinetic_energy_from_momentum(const P& momentum, const M& mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<P, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N p = rdetail::in_<N>(momentum, momentum_unit, units::momentum::kilogram_meter_per_second);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::kinetic_energy_from_momentum_(p, m), units::energy::joule, return_unit);
    });
}

template <typename K, typename M, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<K, M>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<K, M> kinetic_energy_velocity(const K& kinetic_energy, const M& mass, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<K, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::kinetic_energy_velocity_(rdetail::to_<N>(kinetic_energy), rdetail::to_<N>(mass)), units::velocity::light_speed, return_unit);
    });
}

template <typename K, typename M, class EU, class MU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<K, M>::value && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<K, M> kinetic_energy_velocity(const K& kinetic_energy, const M& mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<K, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(kinetic_energy, energy_unit, units::energy::joule);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::kinetic_energy_velocity_(e, m), units::velocity::light_speed, return_unit);
    });
}

template <typename E, typename M, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<E, M> kinetic_energy_momentum(const E& kinetic_energy, const M& mass, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::kinetic_energy_momentum_(rdetail::to_<N>(kinetic_energy), rdetail::to_<N>(mass)), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename E, typename M, class MU, class EU = units::EnergyUnit, class RU = units::MomentumUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<RU>(units::DIMENSION_MOMENTUM)> = 0>
constexpr rdetail::result_t<E, M> kinetic_energy_momentum(const E& kinetic_energy, const M& mass, const MU mass_unit, const EU energy_unit = units::energy::joule, const RU return_unit = units::momentum::kilogram_meter_per_second) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(kinetic_energy, energy_unit, units::energy::joule);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::kinetic_energy_momentum_(e, m), units::momentum::kilogram_meter_per_second, return_unit);
    });
}

template <typename K, typename V, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<K, V>::value && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<K, V> kinetic_energy_mass(const V& velocity, const K& kinetic_energy, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<K, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::kinetic_energy_mass_(rdetail::to_<N>(velocity), rdetail::to_<N>(kinetic_energy)), units::mass::kilogram, return_unit);
    });
}

template <typename K, typename V, class VU, class EU, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<K, V>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<K, V> kinetic_energy_mass(const V& velocity, const K& kinetic_energy, const VU velocity_unit, const EU energy_unit, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<K, V>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N e = rdetail::in_<N>(kinetic_energy, energy_unit, units::energy::joule);
        return rdetail::conv(rdetail::kinetic_energy_mass_(v, e), units::mass::kilogram, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP
