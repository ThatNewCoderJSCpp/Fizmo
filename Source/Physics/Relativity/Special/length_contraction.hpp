#ifndef FIZMO_LENGTH_CONTRACTION_HPP
#define FIZMO_LENGTH_CONTRACTION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename L, typename V, class RU = units::LengthUnit>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_LENGTH, typename std::common_type<L, V, double>::type>::type
contracted_length(const V object_velocity, const L original_length, const RU return_unit = units::distance::meter) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT lorentz = lorentz_factor(object_velocity);
    if (original_length <= 0 || lorentz == CT(0)) { return constants::QUIET_NAN<CT>; }
    const CT contracted = static_cast<CT>(original_length) / lorentz;
    return static_cast<CT>(units::convert(static_cast<long double>(contracted), units::distance::meter, return_unit));
}

template <typename L, typename V, class VU, class LU, class RU = units::LengthUnit>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<LU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && LU::dimension() == units::DIMENSION_LENGTH && RU::dimension() == units::DIMENSION_LENGTH, typename std::common_type<L, V, double>::type>::type
contracted_length(const V object_velocity, const L original_length, const VU velocity_unit, const LU length_unit, const RU return_unit = units::distance::meter) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(object_velocity), velocity_unit, units::velocity::light_speed));
    const CT l = static_cast<CT>(units::convert(static_cast<long double>(original_length), length_unit, units::distance::meter));
    return contracted_length(v, l, return_unit);
}

template <typename L, typename V, class RU = units::LengthUnit>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_LENGTH, typename std::common_type<L, V, double>::type>::type
original_length(const V object_velocity, const L contracted_length, const RU return_unit = units::distance::meter) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    if (contracted_length <= 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    const CT true_length = static_cast<CT>(contracted_length) * lorentz;
    return static_cast<CT>(units::convert(static_cast<long double>(true_length), units::distance::meter, return_unit));
}

template <typename L, typename V, class VU, class LU, class RU = units::LengthUnit>
constexpr typename std::enable_if<std::is_arithmetic<L>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<LU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && LU::dimension() == units::DIMENSION_LENGTH && RU::dimension() == units::DIMENSION_LENGTH, typename std::common_type<L, V, double>::type>::type
original_length(const V object_velocity, const L contracted_length, const VU velocity_unit, const LU length_unit, const RU return_unit = units::distance::meter) noexcept {
    using CT = typename std::common_type<L, V, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(object_velocity), velocity_unit, units::velocity::light_speed));
    const CT l = static_cast<CT>(units::convert(static_cast<long double>(contracted_length), length_unit, units::distance::meter));
    return original_length(v, l, return_unit);
}

template <typename O, typename C, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<C>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<C, O, double>::type>::type
length_contraction_velocity(const O original_length, const C contracted_length, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<O, C, double>::type;
    if (original_length <= 0 || contracted_length <= 0) { return constants::QUIET_NAN<CT>; }
    const CT arg1 = CT(1) + static_cast<CT>(contracted_length) / static_cast<CT>(original_length);
    const CT arg2 = CT(1) - static_cast<CT>(contracted_length) / static_cast<CT>(original_length);
    if (arg1 < 0 || arg2 < 0) { return constants::QUIET_NAN<CT>; }
    const CT velocity = math::sqrt_constexpr(arg1) * math::sqrt_constexpr(arg2);
    return static_cast<CT>(units::convert(static_cast<long double>(velocity), units::velocity::light_speed, return_unit));
}

template <typename O, typename C, class OU, class CU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<C>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<OU> && is_fizmo_unit_v<CU> && is_fizmo_unit_v<RU> && OU::dimension() == units::DIMENSION_LENGTH && CU::dimension() == units::DIMENSION_LENGTH && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<C, O, double>::type>::type
length_contraction_velocity(const O original_length, const C contracted_length, const OU original_unit, const CU contracted_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<O, C, double>::type;
    const CT l = static_cast<CT>(units::convert(static_cast<long double>(original_length), original_unit, units::distance::meter));
    const CT l_prime = static_cast<CT>(units::convert(static_cast<long double>(contracted_length), contracted_unit, units::distance::meter));
    return length_contraction_velocity(l, l_prime, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_LENGTH_CONTRACTION_HPP