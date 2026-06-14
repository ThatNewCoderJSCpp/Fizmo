#ifndef RELATIVISTIC_MOMENTUM_HPP
#define RELATIVISTIC_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
relativistic_momentum(
    const V velocity, const M mass, 
    const fizmo::units::MomentumUnit return_unit = fizmo::units::MomentumUnit(fizmo::units::WeightUnit::KILOGRAM, fizmo::units::VelocityUnit::meters_per_second())
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT value = lorentz * static_cast<CT>(mass) * static_cast<CT>(velocity) * c;
    return static_cast<CT>(fizmo::units::convert_momentum(value, fizmo::units::MomentumUnit(fizmo::units::WeightUnit::KILOGRAM, fizmo::units::VelocityUnit::meters_per_second()), return_unit));
}

template <typename V, typename M>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value, typename std::common_type<V, M, double>::type>::type
relativistic_momentum(
    const V velocity, const M mass, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::MomentumUnit return_unit = fizmo::units::MomentumUnit(fizmo::units::WeightUnit::KILOGRAM, fizmo::units::VelocityUnit::meters_per_second())
) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return relativistic_momentum(v, m, return_unit);
}

template <typename P, typename M>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value, typename std::common_type<P, M, double>::type>::type
relativistic_momentum_velocity(
    const P momentum, const M mass, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    if (mass <= 0 || momentum < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT p = static_cast<CT>(momentum);
    const CT m = static_cast<CT>(mass);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT p_over_mc = p / (m * c);
    const CT p_over_mc_sq = p_over_mc * p_over_mc;
    const CT denom_sq = CT(1) + p_over_mc_sq;
    if (denom_sq <= 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = p_over_mc / fizmo::math::sqrt_constexpr(denom_sq);
    return static_cast<CT>(fizmo::units::convert_speed(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename P, typename M>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value, typename std::common_type<P, M, double>::type>::type
relativistic_momentum_velocity(
    const P momentum, const M mass, 
    const fizmo::units::MomentumUnit momentum_unit, const fizmo::units::WeightUnit mass_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND));
    const CT m = static_cast<CT>(fizmo::units::convert_weight(mass, mass_unit, fizmo::units::WeightUnit::KILOGRAM));
    return relativistic_momentum_velocity(p, m, return_unit);
}

template <typename P, typename V>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<V>::value, typename std::common_type<P, V, double>::type>::type
relativistic_momentum_mass(
    const V velocity, const P momentum, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<P, V, double>::type;
    if (momentum < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT v = static_cast<CT>(velocity);
    const CT c = fizmo::constants::LIGHT_SPEED<CT>;
    const CT denom = lorentz * v * c;
    if (fizmo::abs_constexpr(denom) <= fizmo::constants::TYPE_EPSILON<CT>) { 
        return fizmo::constants::QUIET_NAN<CT>; 
    }
    const CT value = static_cast<CT>(momentum) / denom;
    return static_cast<CT>(fizmo::units::convert_weight(value, fizmo::units::WeightUnit::KILOGRAM, return_unit));
}

template <typename P, typename V>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<V>::value, typename std::common_type<P, V, double>::type>::type
relativistic_momentum_mass(
    const V velocity, const P momentum, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::MomentumUnit momentum_unit, 
    const fizmo::units::WeightUnit return_unit = fizmo::units::WeightUnit::KILOGRAM
) noexcept {
    using CT = typename std::common_type<P, V, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT p = static_cast<CT>(fizmo::units::convert_momentum(momentum, momentum_unit, fizmo::units::MomentumUnit::KILOGRAM_METER_PER_SECOND));
    return relativistic_momentum_mass(v, p, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_MOMENTUM_HPP