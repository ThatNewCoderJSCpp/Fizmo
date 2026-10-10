#ifndef FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP
#define FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<V, M, double>::type>::type
kinetic_energy(const V velocity, const M mass, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT value = static_cast<CT>(mass) * constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT> * (lorentz - 1);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::energy::joule, return_unit));
}

template <typename V, typename M, class VU, class MU, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<V, M, double>::type>::type
kinetic_energy(const V velocity, const M mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return kinetic_energy(v, m, return_unit);
}

template <typename P, typename M, class PU, class MU, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<PU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && PU::dimension() == units::DIMENSION_MOMENTUM && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<P, M, double>::type>::type
kinetic_energy_from_momentum(const P momentum, const M mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    const CT mc2 = m * constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT>;
    const CT pc = p * constants::LIGHT_SPEED<CT>;
    const CT pc2 = pc * pc;
    const CT ke = math::sqrt_constexpr(pc2 + mc2 * mc2) - mc2;
    return static_cast<CT>(units::convert(static_cast<long double>(ke), units::energy::joule, return_unit));
}

template <typename K, typename M, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<K, M, double>::type>::type
kinetic_energy_velocity(const K kinetic_energy, const M mass, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<K, M, double>::type;
    if (mass <= 0 || kinetic_energy < 0) { return constants::QUIET_NAN<CT>; }
    const CT denom = static_cast<CT>(kinetic_energy) / (static_cast<CT>(mass) * constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT>) + 1;
    if (denom == CT(0)) { return constants::QUIET_NAN<CT>; }
    const CT denom2 = denom * denom;
    const CT inv_denom2 = CT(1) / denom2;
    if (inv_denom2 > 1) { return constants::QUIET_NAN<CT>; }
    const CT value = math::sqrt_constexpr(1 - inv_denom2);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename K, typename M, class EU, class MU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<EU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && EU::dimension() == units::DIMENSION_ENERGY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<K, M, double>::type>::type
kinetic_energy_velocity(const K kinetic_energy, const M mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<K, M, double>::type;
    const CT E = static_cast<CT>(units::convert(static_cast<long double>(kinetic_energy), energy_unit, units::energy::joule));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return kinetic_energy_velocity(E, m, return_unit);
}

template <typename E, typename M, class MU, class EU = units::EnergyUnit, class RU = units::MomentumUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<MU> && is_fizmo_unit_v<EU> && is_fizmo_unit_v<RU> && MU::dimension() == units::DIMENSION_MASS && EU::dimension() == units::DIMENSION_ENERGY && RU::dimension() == units::DIMENSION_MOMENTUM, typename std::common_type<E, M, double>::type>::type
kinetic_energy_momentum(const E kinetic_energy, const M mass, const MU mass_unit, const EU energy_unit = units::energy::joule, const RU return_unit = units::momentum::kilogram_meter_per_second) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT ke = static_cast<CT>(units::convert(static_cast<long double>(kinetic_energy), energy_unit, units::energy::joule));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    const CT mc2 = m * constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT>;
    const CT p = math::sqrt_constexpr((ke + mc2) * (ke + mc2) - (mc2 * mc2)) * constants::RECIPROCAL_C<CT>;
    return static_cast<CT>(units::convert(static_cast<long double>(p), units::momentum::kilogram_meter_per_second, return_unit));
}

template <typename K, typename V, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<K, V, double>::type>::type
kinetic_energy_mass(const V velocity, const K kinetic_energy, const RU return_unit = units::mass::kilogram) {
    using CT = typename std::common_type<K, V, double>::type;
    if (kinetic_energy < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c2 = constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT>;
    if (abs_constexpr(1 - lorentz) == CT(0)) { return constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(kinetic_energy) / ((lorentz - 1) * c2);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::mass::kilogram, return_unit));
}

template <typename K, typename V, class VU, class EU, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<EU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && EU::dimension() == units::DIMENSION_ENERGY && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<K, V, double>::type>::type
kinetic_energy_mass(const V velocity, const K kinetic_energy, const VU velocity_unit, const EU energy_unit, const RU return_unit = units::mass::kilogram) {
    using CT = typename std::common_type<K, V, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT E = static_cast<CT>(units::convert(static_cast<long double>(kinetic_energy), energy_unit, units::energy::joule));
    return kinetic_energy_mass(v, E, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_KINETIC_ENERGY_HPP