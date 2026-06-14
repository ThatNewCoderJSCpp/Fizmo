#ifndef RELATIVISTIC_VELOCITY_ADDITION_HPP
#define RELATIVISTIC_VELOCITY_ADDITION_HPP

#include "../../../Basic/constants.hpp"
#include "../../../Converters/unit_converters.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V2, typename V3>
constexpr typename std::enable_if<std::is_arithmetic<V2>::value && std::is_arithmetic<V3>::value, typename std::common_type<V2, V3, double>::type>::type
velocity_addition_vel1(
    const V2 vel2, const V3 vel3, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V2, V3, double>::type;
    const CT v2 = static_cast<CT>(vel2);
    const CT v3 = static_cast<CT>(vel3);
    const CT denom = v2 * v3 - CT(1);
    if (fizmo::abs_constexpr(denom) <= fizmo::constants::TYPE_EPSILON<CT>) { 
        return fizmo::constants::QUIET_NAN<CT>; 
    }
    const CT value = (v2 + v3) / denom;
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename V2, typename V3>
constexpr typename std::enable_if<std::is_arithmetic<V2>::value && std::is_arithmetic<V3>::value, typename std::common_type<V2, V3, double>::type>::type
velocity_addition_vel1(
    const V2 vel2, const V3 vel3, 
    const fizmo::units::VelocityUnit vel2_unit, const fizmo::units::VelocityUnit vel3_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V2, V3, double>::type;
    const CT v2 = static_cast<CT>(fizmo::units::convert_velocity(vel2, vel2_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT v3 = static_cast<CT>(fizmo::units::convert_velocity(vel3, vel3_unit, fizmo::units::VelocityUnit::light_speed()));
    return velocity_addition_vel1(v2, v3, return_unit);
}

template <typename V1, typename V3>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V3>::value, typename std::common_type<V1, V3, double>::type>::type
velocity_addition_vel2(
    const V1 vel1, const V3 vel3, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V1, V3, double>::type;
    const CT v1 = static_cast<CT>(vel1);
    const CT v3 = static_cast<CT>(vel3);
    const CT denom = v1 * v3 - CT(1);
    if (fizmo::abs_constexpr(denom) <= fizmo::constants::TYPE_EPSILON<CT>) { 
        return fizmo::constants::QUIET_NAN<CT>; 
    }
    const CT value = (v1 - v3) / denom;
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename V1, typename V3>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V3>::value, typename std::common_type<V1, V3, double>::type>::type
velocity_addition_vel2(
    const V1 vel1, const V3 vel3, 
    const fizmo::units::VelocityUnit vel1_unit, const fizmo::units::VelocityUnit vel3_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V1, V3, double>::type;
    const CT v1 = static_cast<CT>(fizmo::units::convert_velocity(vel1, vel1_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT v3 = static_cast<CT>(fizmo::units::convert_velocity(vel3, vel3_unit, fizmo::units::VelocityUnit::light_speed()));
    return velocity_addition_vel2(v1, v3, return_unit);
}

template <typename V1, typename V2>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V2>::value, typename std::common_type<V1, V2, double>::type>::type
velocity_addition_vel3(
    const V1 vel1, const V2 vel2, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V1, V2, double>::type;
    const CT v1 = static_cast<CT>(vel1);
    const CT v2 = static_cast<CT>(vel2);
    const CT denom = v1 * v2 + CT(1);
    if (fizmo::abs_constexpr(denom) <= fizmo::constants::TYPE_EPSILON<CT>) { 
        return fizmo::constants::QUIET_NAN<CT>; 
    }
    const CT value = (v1 + v2) / denom;
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename V1, typename V2>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V2>::value, typename std::common_type<V1, V2, double>::type>::type
velocity_addition_vel3(
    const V1 vel1, const V2 vel2, 
    const fizmo::units::VelocityUnit vel1_unit, const fizmo::units::VelocityUnit vel2_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<V1, V2, double>::type;
    const CT v1 = static_cast<CT>(fizmo::units::convert_velocity(vel1, vel1_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT v2 = static_cast<CT>(fizmo::units::convert_velocity(vel2, vel2_unit, fizmo::units::VelocityUnit::light_speed()));
    return velocity_addition_vel3(v1, v2, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_VELOCITY_ADDITION_HPP