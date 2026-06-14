#ifndef RELATIVISTIC_KINETIC_ENERGY_HPP
#define RELATIVISTIC_KINETIC_ENERGY_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
kinetic_energy(
    const V velocity, const M mass, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT value = static_cast<CT>(mass) * fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT> * (lorentz - 1);
    return static_cast<CT>(fizmo::units::convert_energy(value, fizmo::units::EnergyUnit::joule(), return_unit));
}

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
kinetic_energy(
    const V velocity, const M mass, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return kinetic_energy(v, m, return_unit);
}

template <typename P, typename M>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value, typename std::common_type<P, M, double>::type>::type
kinetic_energy(
    const P momentum, const M mass, 
    const fizmo::units::WeightUnit mass_unit, const fizmo::units::MomentumUnit momentum_unit = fizmo::units::MomentumUnit(),
    const fizmo::units::EnergyUnit return_unit = fizmo::units::EnergyUnit::joule()
) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::SI()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    const CT mc2 = m * fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT>;
    const CT pc = p * fizmo::constants::LIGHT_SPEED<CT>;
    const CT pc2 = pc * pc;
    const CT ke = fizmo::math::sqrt_constexpr(pc2 + mc2 * mc2) - mc2;
    return fizmo::units::convert_energy(ke, fizmo::units::EnergyUnit::joule(), return_unit);
}

template <typename K, typename M>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<M>::value, typename std::common_type<K, M, double>::type>::type
kinetic_energy_velocity(
    const K kinetic_energy, const M mass, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<K, M, double>::type;
    if (mass <= 0 || kinetic_energy < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT denom = static_cast<CT>(kinetic_energy) / (static_cast<CT>(mass) * fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT>) + 1;
    if (denom == CT(0)) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT denom2 = denom * denom;
    const CT inv_denom2 = CT(1) / denom2;
    if (inv_denom2 > 1) { return fizmo::constants::QUIET_NAN<CT>; } 
    const CT value = fizmo::math::sqrt_constexpr(1 - inv_denom2);
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename K, typename M>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<M>::value, typename std::common_type<K, M, double>::type>::type
kinetic_energy_velocity(
    const K kinetic_energy, const M mass, 
    const fizmo::units::EnergyUnit energy_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<K, M, double>::type;
    const CT E = static_cast<CT>(fizmo::units::convert_energy(kinetic_energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return kinetic_energy_velocity(E, m, return_unit); 
}

template <typename E, typename M>
constexpr typename std::enable_if<std::is_arithmetic<E>::value && std::is_arithmetic<M>::value, typename std::common_type<E, M, double>::type>::type
kinetic_energy_momentum(
    const E kinetic_energy, const M mass, 
    const fizmo::units::WeightUnit mass_unit, const fizmo::units::EnergyUnit energy_unit = fizmo::units::EnergyUnit::joule(),
    const fizmo::units::MomentumUnit return_unit = fizmo::units::MomentumUnit::SI()
) noexcept {
    using CT = typename std::common_type<E, M, double>::type;
    const CT ke = static_cast<CT>(fizmo::units::convert_energy(kinetic_energy, energy_unit, fizmo::units::EnergyUnit::joule()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    const CT mc2 = m * fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT>;
    const CT p = fizmo::math::sqrt_constexpr((ke + mc2) * (ke + mc2) - (mc2 * mc2)) * fizmo::constants::RECIPROCAL_C<CT>;
    return fizmo::units::convert_momentum(p, fizmo::units::MomentumUnit::SI(), return_unit);
}

template <typename K, typename V>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<V>::value, typename std::common_type<K, V, double>::type>::type
kinetic_energy_mass(
    const V velocity, const K kinetic_energy, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) {
    using CT = typename std::common_type<K, V, double>::type;
    if (kinetic_energy < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c2 = fizmo::constants::LIGHT_SPEED<CT> * fizmo::constants::LIGHT_SPEED<CT>;
    if (fizmo::abs_constexpr(1 - lorentz) == CT(0)) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(kinetic_energy) / ((1 - lorentz) * c2);
    return static_cast<CT>(fizmo::units::convert_weight(value, fizmo::units::WeightUnit::KILOGRAM, return_unit));
}

template <typename K, typename V>
constexpr typename std::enable_if<std::is_arithmetic<K>::value && std::is_arithmetic<V>::value, typename std::common_type<K, V, double>::type>::type
kinetic_energy_mass(
    const V velocity, const K kinetic_energy, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::EnergyUnit energy_unit, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) {
    using CT = typename std::common_type<K, V, double>::type;
    const CT v = fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed());
    const CT E = fizmo::units::convert_energy(kinetic_energy, energy_unit, fizmo::units::EnergyUnit::joule());
    return kinetic_energy_mass(v, E, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_KINETIC_ENERGY_HPP