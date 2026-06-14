#ifndef RELATIVISTIC_ENERGY_HPP
#define RELATIVISTIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
relativistic_energy(
    const V velocity, const M mass, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT value = static_cast<CT>(mass) * c * c * lorentz;
    return static_cast<CT>(fizmo::units::convert_energy(value, fizmo::units::EnergyUnit::joule(), return_unit));
}

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
relativistic_energy(
    const V velocity, const M mass, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return relativistic_energy(v, m, return_unit);
}

template <typename E, typename M>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value, typename std::common_type<E, M, double>::type>::type
relativistic_energy_velocity(
    const E energy, const M mass, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    if (mass < 0 || energy < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT const_value = static_cast<CT>(mass) * c * c / static_cast<CT>(energy);
    if (fizmo::abs_constexpr(const_value) > 1) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = fizmo::math::sqrt_constexpr(CT(1) - const_value * const_value);
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename E, typename M>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value, typename std::common_type<E, M, double>::type>::type
relativistic_energy_velocity(
    const E energy, const M mass, 
    const fizmo::units::EnergyUnit energy_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT E_val = static_cast<CT>(fizmo::units::convert_energy(energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return relativistic_energy_velocity(E_val, m, return_unit);
}

template <typename V, typename E>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<E>::value, typename std::common_type<V, E, double>::type>::type
relativistic_energy_mass(
    const V velocity, const E energy, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<V, E, double>::type;
    if (energy < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c2 = fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT>;
    if (lorentz == CT(0)) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(energy) / (lorentz * c2);
    return static_cast<CT>(fizmo::units::convert_weight(value, fizmo::units::WeightUnit::KILOGRAM, return_unit));
}

template <typename V, typename E>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<E>::value, typename std::common_type<V, E, double>::type>::type
relativistic_energy_mass(
    const V velocity, const E energy, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::EnergyUnit energy_unit, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<V, E, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT E_val = static_cast<CT>(fizmo::units::convert_energy(energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    return relativistic_energy_mass(v, E_val, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_ENERGY_HPP