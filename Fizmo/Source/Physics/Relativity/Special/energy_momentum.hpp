#ifndef RELATIVISTIC_ENERGY_MOMENTUM_HPP
#define RELATIVISTIC_ENERGY_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename P, typename M>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value, typename std::common_type<P, M, double>::type>::type
energy_momentum_energy(
    const P momentum, const M mass, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    if (mass < 0 || momentum < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT p = static_cast<CT>(momentum);
    const CT m = static_cast<CT>(mass);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT pc = p * c;
    const CT mc2 = m * c2;
    const CT value = fizmo::math::sqrt_constexpr(pc * pc + mc2 * mc2);
    return static_cast<CT>(fizmo::units::convert_energy(value, fizmo::units::EnergyUnit::joule(), return_unit));
}

template <typename P, typename M>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value, typename std::common_type<P, M, double>::type>::type
energy_momentum_energy(
    const P momentum, const M mass, 
    const fizmo::units::MomentumUnit momentum_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return energy_momentum_energy(p, m, return_unit);
}

template <typename E, typename M>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value, typename std::common_type<E, M, double>::type>::type
energy_momentum_momentum(
    const E energy, const M mass, 
    const fizmo::units::MomentumUnit return_unit = fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND
) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    if (mass < 0 || energy < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT m = static_cast<CT>(mass);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT mc2 = m * c2;
    const CT arg = E * E - mc2 * mc2;
    if (arg < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = fizmo::math::sqrt_constexpr(arg) / c;
    return static_cast<CT>(fizmo::units::convert_momentum(value, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND, return_unit));
}

template <typename E, typename M>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value, typename std::common_type<E, M, double>::type>::type
energy_momentum_momentum(
    const E energy, const M mass, 
    const fizmo::units::EnergyUnit energy_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::MomentumUnit return_unit = fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND
) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT E = static_cast<CT>(fizmo::units::convert_energy(energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return energy_momentum_momentum(E, m, return_unit);
}

template <typename E, typename P>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value, typename std::common_type<E, P, double>::type>::type
energy_momentum_mass(
    const E energy, const P momentum, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    if (energy < 0 || momentum < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT p = static_cast<CT>(momentum);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT c2 = c * c;
    const CT pc = p * c;
    const CT arg = E * E - pc * pc;
    if (arg < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = fizmo::math::sqrt_constexpr(arg) / c2;
    return static_cast<CT>(fizmo::units::convert_weight(value, fizmo::units::WeightUnit::KILOGRAM, return_unit));
}

template <typename E, typename P>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value, typename std::common_type<E, P, double>::type>::type
energy_momentum_mass(
    const E energy, const P momentum, 
    const fizmo::units::EnergyUnit energy_unit, const fizmo::units::MomentumUnit momentum_unit, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    const CT E = static_cast<CT>(fizmo::units::convert_energy(energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND));
    return energy_momentum_mass(E, p, return_unit);
}

template <typename E, typename P>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value, typename std::common_type<E, P, double>::type>::type
energy_momentum_velocity(
    const E energy, const P momentum, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    if (energy <= 0 || momentum < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT E = static_cast<CT>(energy);
    const CT p = static_cast<CT>(momentum);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT value = (p * c * c) / E;
    if (value > c) { return fizmo::constants::QUIET_NAN<CT>; }
    return static_cast<CT>(fizmo::units::convert_velocity(value / c, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename E, typename P>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<P>::value, typename std::common_type<E, P, double>::type>::type
energy_momentum_velocity(
    const E energy, const P momentum, 
    const fizmo::units::EnergyUnit energy_unit, const fizmo::units::MomentumUnit momentum_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<E, P, double>::type;
    const CT E = static_cast<CT>(fizmo::units::convert_energy(energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND));
    return energy_momentum_velocity(E, p, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_ENERGY_MOMENTUM_HPP