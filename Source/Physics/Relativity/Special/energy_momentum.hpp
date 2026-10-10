#ifndef FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP
#define FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename P, typename M, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<P, M, double>::type>::type
energy_momentum_energy(const P momentum, const M mass, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    if (mass < 0 || momentum < 0) { return constants::QUIET_NAN<CT>; }
    const CT p = static_cast<CT>(momentum);
    const CT m = static_cast<CT>(mass);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT pc = p * c;
    const CT mc2 = m * c2;
    const CT value = math::sqrt_constexpr(pc * pc + mc2 * mc2);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::energy::joule, return_unit));
}

template <typename P, typename M, class PU, class MU, class RU = units::EnergyUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<PU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && PU::dimension() == units::DIMENSION_MOMENTUM && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_ENERGY, typename std::common_type<P, M, double>::type>::type
energy_momentum_energy(const P momentum, const M mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::energy::joule) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return energy_momentum_energy(p, m, return_unit);
}

template <typename E, typename M, class RU = units::MomentumUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MOMENTUM, typename std::common_type<E, M, double>::type>::type
energy_momentum_momentum(const E energy, const M mass, const RU return_unit = units::momentum::kilogram_meter_per_second) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    if (mass < 0 || energy < 0) { return constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT m = static_cast<CT>(mass);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT mc2 = m * c2;
    const CT arg = E * E - mc2 * mc2;
    if (arg < 0) { return constants::QUIET_NAN<CT>; }
    const CT value = math::sqrt_constexpr(arg) / c;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::momentum::kilogram_meter_per_second, return_unit));
}

template <typename E, typename M, class EU, class MU, class RU = units::MomentumUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<EU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && EU::dimension() == units::DIMENSION_ENERGY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_MOMENTUM, typename std::common_type<E, M, double>::type>::type
energy_momentum_momentum(const E energy, const M mass, const EU energy_unit, const MU mass_unit, const RU return_unit = units::momentum::kilogram_meter_per_second) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT E = static_cast<CT>(units::convert(static_cast<long double>(energy), energy_unit, units::energy::joule));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return energy_momentum_momentum(E, m, return_unit);
}

template <typename E, typename P, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<E, P, double>::type>::type
energy_momentum_mass(const E energy, const P momentum, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    if (energy < 0 || momentum < 0) { return constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT p = static_cast<CT>(momentum);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT pc = p * c;
    const CT arg = E * E - pc * pc;
    if (arg < 0) { return constants::QUIET_NAN<CT>; }
    const CT value = math::sqrt_constexpr(arg) / c2;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::mass::kilogram, return_unit));
}

template <typename E, typename P, class EU, class PU, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value && is_fizmo_unit_v<EU> && is_fizmo_unit_v<PU> && is_fizmo_unit_v<RU> && EU::dimension() == units::DIMENSION_ENERGY && PU::dimension() == units::DIMENSION_MOMENTUM && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<E, P, double>::type>::type
energy_momentum_mass(const E energy, const P momentum, const EU energy_unit, const PU momentum_unit, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    const CT E = static_cast<CT>(units::convert(static_cast<long double>(energy), energy_unit, units::energy::joule));
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    return energy_momentum_mass(E, p, return_unit);
}

template <typename E, typename P, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<E, P, double>::type>::type
energy_momentum_velocity(const E energy, const P momentum, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    if (energy <= 0 || momentum < 0) { return constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT p = static_cast<CT>(momentum);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT value = (p * c * c) / E;
    if (value > c) { return constants::QUIET_NAN<CT>; }
    return static_cast<CT>(units::convert(static_cast<long double>(value / c), units::velocity::light_speed, return_unit));
}

template <typename E, typename P, class EU, class PU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value && is_fizmo_unit_v<EU> && is_fizmo_unit_v<PU> && is_fizmo_unit_v<RU> && EU::dimension() == units::DIMENSION_ENERGY && PU::dimension() == units::DIMENSION_MOMENTUM && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<E, P, double>::type>::type
energy_momentum_velocity(const E energy, const P momentum, const EU energy_unit, const PU momentum_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    const CT E = static_cast<CT>(units::convert(static_cast<long double>(energy), energy_unit, units::energy::joule));
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    return energy_momentum_velocity(E, p, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_ENERGY_MOMENTUM_HPP