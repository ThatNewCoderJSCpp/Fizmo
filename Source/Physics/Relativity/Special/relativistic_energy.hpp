#ifndef FIZMO_RELATIVISTIC_ENERGY_HPP
#define FIZMO_RELATIVISTIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

template <class N>
constexpr N relativistic_energy_(const N& beta, const N& m) {
    if (neg_(m)) return nan_<N>();
    const N c = c_<N>();
    return gamma_(beta) * m * c * c;
}

template <class N>
constexpr N relativistic_energy_velocity_(const N& e, const N& m) {
    if (neg_(m) || !(e > N(0))) return nan_<N>();
    const N c = c_<N>();
    const N r = m * c * c / e;
    if (r > N(1)) return nan_<N>();
    return sqrt_((N(1) - r) * (N(1) + r));
}

template <class N>
constexpr N relativistic_energy_mass_(const N& beta, const N& e) {
    if (neg_(e)) return nan_<N>();
    const N c = c_<N>();
    return e * s_(beta) / (c * c);
}

} // namespace rdetail

template <typename V, typename M, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<V, M> relativistic_energy(const V& velocity, const M& mass, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_energy_(rdetail::to_<N>(velocity), rdetail::to_<N>(mass)), units::energy::joule, return_unit);
    });
}

template <typename V, typename M, class VU, class MU, class RU = units::EnergyUnit,
          rdetail::enable_t<rdetail::all_values<V, M>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_ENERGY)> = 0>
constexpr rdetail::result_t<V, M> relativistic_energy(const V& velocity, const M& mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::energy::joule) {
    return rdetail::run<rdetail::result_t<V, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::relativistic_energy_(v, m), units::energy::joule, return_unit);
    });
}

template <typename E, typename M, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<E, M> relativistic_energy_velocity(const E& energy, const M& mass, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_energy_velocity_(rdetail::to_<N>(energy), rdetail::to_<N>(mass)), units::velocity::light_speed, return_unit);
    });
}

template <typename E, typename M, class EU, class MU, class RU = units::VelocityUnit,
          rdetail::enable_t<rdetail::all_values<E, M>::value && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<MU>(units::DIMENSION_MASS) && rdetail::has_dim<RU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<E, M> relativistic_energy_velocity(const E& energy, const M& mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) {
    return rdetail::run<rdetail::result_t<E, M>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N e = rdetail::in_<N>(energy, energy_unit, units::energy::joule);
        const N m = rdetail::in_<N>(mass, mass_unit, units::mass::kilogram);
        return rdetail::conv(rdetail::relativistic_energy_velocity_(e, m), units::velocity::light_speed, return_unit);
    });
}

template <typename V, typename E, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<V, E>::value && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<V, E> relativistic_energy_mass(const V& velocity, const E& energy, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<V, E>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::conv(rdetail::relativistic_energy_mass_(rdetail::to_<N>(velocity), rdetail::to_<N>(energy)), units::mass::kilogram, return_unit);
    });
}

template <typename V, typename E, class VU, class EU, class RU = units::MassUnit,
          rdetail::enable_t<rdetail::all_values<V, E>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY) && rdetail::has_dim<EU>(units::DIMENSION_ENERGY) && rdetail::has_dim<RU>(units::DIMENSION_MASS)> = 0>
constexpr rdetail::result_t<V, E> relativistic_energy_mass(const V& velocity, const E& energy, const VU velocity_unit, const EU energy_unit, const RU return_unit = units::mass::kilogram) {
    return rdetail::run<rdetail::result_t<V, E>>([&](auto t) {
        using N = typename decltype(t)::type;
        const N v = rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed);
        const N e = rdetail::in_<N>(energy, energy_unit, units::energy::joule);
        return rdetail::conv(rdetail::relativistic_energy_mass_(v, e), units::mass::kilogram, return_unit);
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_ENERGY_HPP
