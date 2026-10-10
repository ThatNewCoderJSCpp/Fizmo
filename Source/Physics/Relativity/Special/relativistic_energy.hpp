#ifndef FIZMO_RELATIVISTIC_ENERGY_HPP
#define FIZMO_RELATIVISTIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<V, M, double>::type>::type
relativistic_energy(const V velocity, const M mass, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT value = static_cast<CT>(mass) * c * c * lorentz;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::energy::joule, return_unit));
}

template <typename V, typename M, class VU, class MU, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<V, M, double>::type>::type
relativistic_energy(const V velocity, const M mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return relativistic_energy(v, m, return_unit);
}

template <typename E, typename M, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<E, M, double>::type>::type
relativistic_energy_velocity(const E energy, const M mass, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    if (mass < 0 || energy < 0) { return constants::QUIET_NAN<CT>; }
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT const_value = static_cast<CT>(mass) * c * c / static_cast<CT>(energy);
    if (abs_constexpr(const_value) > 1) { return constants::QUIET_NAN<CT>; }
    const CT value = math::sqrt_constexpr(CT(1) - const_value * const_value);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename E, typename M, class EU, class MU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<EU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && EU::dimension() == units::DIMENSION_ENERGY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<E, M, double>::type>::type
relativistic_energy_velocity(const E energy, const M mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT E_val = static_cast<CT>(units::convert(static_cast<long double>(energy), energy_unit, units::energy::joule));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return relativistic_energy_velocity(E_val, m, return_unit);
}

template <typename V, typename E, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<E>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<V, E, double>::type>::type
relativistic_energy_mass(const V velocity, const E energy, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<V, E, double>::type;
    if (energy < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c2 = constants::LIGHT_SPEED<CT> * constants::LIGHT_SPEED<CT>;
    if (lorentz == CT(0)) { return constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(energy) / (lorentz * c2);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::mass::kilogram, return_unit));
}

template <typename V, typename E, class VU, class EU, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<E>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<EU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && EU::dimension() == units::DIMENSION_ENERGY && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<V, E, double>::type>::type
relativistic_energy_mass(const V velocity, const E energy, const VU velocity_unit, const EU energy_unit, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<V, E, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT E_val = static_cast<CT>(units::convert(static_cast<long double>(energy), energy_unit, units::energy::joule));
    return relativistic_energy_mass(v, E_val, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_ENERGY_HPP