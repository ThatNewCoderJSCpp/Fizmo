#ifndef FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP
#define FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V2, typename V3, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V2>::value && std::is_arithmetic<V3>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V2, V3, double>::type>::type
velocity_addition_vel1(const V2 vel2, const V3 vel3, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V2, V3, double>::type;
    const CT v2 = static_cast<CT>(vel2);
    const CT v3 = static_cast<CT>(vel3);
    const CT denom = v2 * v3 - CT(1);
    if (abs_constexpr(denom) <= constants::TYPE_EPSILON<CT>) { return constants::QUIET_NAN<CT>; }
    const CT value = (v2 + v3) / denom;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename V2, typename V3, class U2, class U3, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V2>::value && std::is_arithmetic<V3>::value && is_fizmo_unit_v<U2> && is_fizmo_unit_v<U3> && is_fizmo_unit_v<RU> && U2::dimension() == units::DIMENSION_VELOCITY && U3::dimension() == units::DIMENSION_VELOCITY && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V2, V3, double>::type>::type
velocity_addition_vel1(const V2 vel2, const V3 vel3, const U2 vel2_unit, const U3 vel3_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V2, V3, double>::type;
    const CT v2 = static_cast<CT>(units::convert(static_cast<long double>(vel2), vel2_unit, units::velocity::light_speed));
    const CT v3 = static_cast<CT>(units::convert(static_cast<long double>(vel3), vel3_unit, units::velocity::light_speed));
    return velocity_addition_vel1(v2, v3, return_unit);
}

template <typename V1, typename V3, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V3>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V1, V3, double>::type>::type
velocity_addition_vel2(const V1 vel1, const V3 vel3, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V1, V3, double>::type;
    const CT v1 = static_cast<CT>(vel1);
    const CT v3 = static_cast<CT>(vel3);
    const CT denom = v1 * v3 - CT(1);
    if (abs_constexpr(denom) <= constants::TYPE_EPSILON<CT>) { return constants::QUIET_NAN<CT>; }
    const CT value = (v1 - v3) / denom;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename V1, typename V3, class U1, class U3, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V3>::value && is_fizmo_unit_v<U1> && is_fizmo_unit_v<U3> && is_fizmo_unit_v<RU> && U1::dimension() == units::DIMENSION_VELOCITY && U3::dimension() == units::DIMENSION_VELOCITY && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V1, V3, double>::type>::type
velocity_addition_vel2(const V1 vel1, const V3 vel3, const U1 vel1_unit, const U3 vel3_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V1, V3, double>::type;
    const CT v1 = static_cast<CT>(units::convert(static_cast<long double>(vel1), vel1_unit, units::velocity::light_speed));
    const CT v3 = static_cast<CT>(units::convert(static_cast<long double>(vel3), vel3_unit, units::velocity::light_speed));
    return velocity_addition_vel2(v1, v3, return_unit);
}

template <typename V1, typename V2, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V2>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V1, V2, double>::type>::type
velocity_addition_vel3(const V1 vel1, const V2 vel2, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V1, V2, double>::type;
    const CT v1 = static_cast<CT>(vel1);
    const CT v2 = static_cast<CT>(vel2);
    const CT denom = v1 * v2 + CT(1);
    if (abs_constexpr(denom) <= constants::TYPE_EPSILON<CT>) { return constants::QUIET_NAN<CT>; }
    const CT value = (v1 + v2) / denom;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename V1, typename V2, class U1, class U2, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<V1>::value && std::is_arithmetic<V2>::value && is_fizmo_unit_v<U1> && is_fizmo_unit_v<U2> && is_fizmo_unit_v<RU> && U1::dimension() == units::DIMENSION_VELOCITY && U2::dimension() == units::DIMENSION_VELOCITY && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<V1, V2, double>::type>::type
velocity_addition_vel3(const V1 vel1, const V2 vel2, const U1 vel1_unit, const U2 vel2_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<V1, V2, double>::type;
    const CT v1 = static_cast<CT>(units::convert(static_cast<long double>(vel1), vel1_unit, units::velocity::light_speed));
    const CT v2 = static_cast<CT>(units::convert(static_cast<long double>(vel2), vel2_unit, units::velocity::light_speed));
    return velocity_addition_vel3(v1, v2, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_VELOCITY_ADDITION_HPP