#ifndef LENGTH_CONTRACTION_HPP
#define LENGTH_CONTRACTION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename L, typename V>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value, typename std::common_type<L, V, double>::type>::type
contracted_length(
    const V object_velocity, const L original_length, 
    const fizmo::units::DistanceUnit return_unit = fizmo::units::DistanceUnit::METER
) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT lorentz = lorentz_factor(object_velocity);
    if (original_length <= 0 || lorentz == CT(0)) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT contracted_length = static_cast<CT>(original_length) / lorentz; 
    return static_cast<CT>(fizmo::units::convert_distance(contracted_length, fizmo::units::DistanceUnit::METER, return_unit));
}

template <typename L, typename V>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value, typename std::common_type<L, V, double>::type>::type
contracted_length(
    const V object_velocity, const L original_length, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::DistanceUnit length_unit,
    const fizmo::units::DistanceUnit return_unit = fizmo::units::DistanceUnit::METER
) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(object_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT l = static_cast<CT>(fizmo::units::convert_distance(original_length, length_unit, fizmo::units::DistanceUnit::METER));
    return contracted_length(v, l, return_unit);
}

template <typename L, typename V>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value, typename std::common_type<L, V, double>::type>::type
original_length(
    const V object_velocity, const L contracted_length, 
    const fizmo::units::DistanceUnit return_unit = fizmo::units::DistanceUnit::METER
) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    if (contracted_length <= 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    const CT true_length = static_cast<CT>(contracted_length) * lorentz;
    return static_cast<CT>(fizmo::units::convert_distance(true_length, fizmo::units::DistanceUnit::METER, return_unit));
}

template <typename L, typename V>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value, typename std::common_type<L, V, double>::type>::type
original_length(
    const V object_velocity, const L contracted_length, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::DistanceUnit length_unit, 
    const fizmo::units::DistanceUnit return_unit = fizmo::units::DistanceUnit::METER
) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(object_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT l = static_cast<CT>(fizmo::units::convert_distance(contracted_length, length_unit, fizmo::units::DistanceUnit::METER));
    return original_length(v, l, return_unit);
}

template <typename O, typename C>
constexpr typename std::enable_if<std::is_arithmetic<C>::value && std::is_arithmetic<O>::value, typename std::common_type<C, O, double>::type>::type
length_contraction_velocity(
    const O original_length, const C contracted_length, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<O, C, double>::type;
    if (original_length <= 0 || contracted_length <= 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT arg1 = CT(1) + static_cast<CT>(contracted_length) / static_cast<CT>(original_length);
    const CT arg2 = CT(1) - static_cast<CT>(contracted_length) / static_cast<CT>(original_length);
    if (arg1 < 0 || arg2 < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT velocity = fizmo::math::sqrt_constexpr(arg1) * fizmo::math::sqrt_constexpr(arg2);
    return static_cast<CT>(fizmo::units::convert_velocity(velocity, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename O, typename C>
constexpr typename std::enable_if<std::is_arithmetic<C>::value && std::is_arithmetic<O>::value, typename std::common_type<C, O, double>::type>::type
length_contraction_velocity(
    const O original_length, const C contracted_length, 
    const fizmo::units::DistanceUnit original_unit, const fizmo::units::DistanceUnit contracted_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<O, C, double>::type;
    const CT l = static_cast<CT>(fizmo::units::convert_distance(original_length, original_unit, fizmo::units::DistanceUnit::METER));
    const CT l_prime = static_cast<CT>(fizmo::units::convert_distance(contracted_length, contracted_unit, fizmo::units::DistanceUnit::METER));
    return length_contraction_velocity(l, l_prime, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // LENGTH_CONTRACTION_HPP