#ifndef FIZMO_TIME_DILATION_HPP
#define FIZMO_TIME_DILATION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename T, class RU = units::TimeUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_TIME, typename std::common_type<V, T, double>::type>::type
dilated_time(const V object_velocity, const T object_observed_time, const RU return_unit = units::time::second) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    if (object_observed_time < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    const CT value = lorentz * static_cast<CT>(object_observed_time);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::time::second, return_unit));
}

template <typename V, typename T, class VU, class TU, class RU = units::TimeUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<TU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && TU::dimension() == units::DIMENSION_TIME && RU::dimension() == units::DIMENSION_TIME, typename std::common_type<V, T, double>::type>::type
dilated_time(const V object_velocity, const T object_observed_time, const VU velocity_unit, const TU time_unit, const RU return_unit = units::time::second) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(object_velocity), velocity_unit, units::velocity::light_speed));
    const CT t = static_cast<CT>(units::convert(static_cast<long double>(object_observed_time), time_unit, units::time::second));
    return dilated_time(v, t, return_unit);
}

template <typename V, typename T, class RU = units::TimeUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_TIME, typename std::common_type<V, T, double>::type>::type
proper_time(const V object_velocity, const T stationary_observed_time, const RU return_unit = units::time::second) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    if (stationary_observed_time < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    if (lorentz == CT(0)) { return constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(stationary_observed_time) / lorentz;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::time::second, return_unit));
}

template <typename V, typename T, class VU, class TU, class RU = units::TimeUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<TU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && TU::dimension() == units::DIMENSION_TIME && RU::dimension() == units::DIMENSION_TIME, typename std::common_type<V, T, double>::type>::type
proper_time(const V object_velocity, const T stationary_observed_time, const VU velocity_unit, const TU time_unit, const RU return_unit = units::time::second) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(object_velocity), velocity_unit, units::velocity::light_speed));
    const CT t = static_cast<CT>(units::convert(static_cast<long double>(stationary_observed_time), time_unit, units::time::second));
    return proper_time(v, t, return_unit);
}

template <typename S, typename O, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<S, O, double>::type>::type
time_dilation_velocity(const S stationary_observed_time, const O object_observed_time, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    if (stationary_observed_time <= 0 || object_observed_time <= 0) { return constants::QUIET_NAN<CT>; }
    const CT T = static_cast<CT>(stationary_observed_time);
    const CT T_prime = static_cast<CT>(object_observed_time);
    const CT arg1 = CT(1) - T / T_prime;
    const CT arg2 = CT(1) + T / T_prime;
    if (arg1 < 0 || arg2 < 0) { return constants::QUIET_NAN<CT>; }
    const CT velocity = fizmo::math::sqrt_constexpr(arg1) * fizmo::math::sqrt_constexpr(arg2);
    return static_cast<CT>(units::convert(static_cast<long double>(velocity), units::velocity::light_speed, return_unit));
}

template <typename S, typename O, class STU, class OTU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<STU> && is_fizmo_unit_v<OTU> && is_fizmo_unit_v<RU> && STU::dimension() == units::DIMENSION_TIME && OTU::dimension() == units::DIMENSION_TIME && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<S, O, double>::type>::type
time_dilation_velocity(const S stationary_observed_time, const O object_observed_time, const STU stationary_time_unit, const OTU object_time_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    const CT t_s = static_cast<CT>(units::convert(static_cast<long double>(stationary_observed_time), stationary_time_unit, units::time::second));
    const CT t_o = static_cast<CT>(units::convert(static_cast<long double>(object_observed_time), object_time_unit, units::time::second));
    return time_dilation_velocity(t_s, t_o, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_TIME_DILATION_HPP