#ifndef TIME_DILATION_HPP
#define TIME_DILATION_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename T>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value, typename std::common_type<V, T, double>::type>::type
dilated_time(
    const V object_velocity, const T object_observed_time, 
    const fizmo::units::TimeUnit return_unit = fizmo::units::TimeUnit::SECOND
) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    if (object_observed_time < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    const CT value = lorentz * static_cast<CT>(object_observed_time);
    return static_cast<CT>(fizmo::units::convert_time(value, fizmo::units::TimeUnit::SECOND, return_unit));
}

template <typename V, typename T>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value, typename std::common_type<V, T, double>::type>::type
dilated_time(
    const V object_velocity, const T object_observed_time, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::TimeUnit time_unit, 
    const fizmo::units::TimeUnit return_unit = fizmo::units::TimeUnit::SECOND
) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(object_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT t = static_cast<CT>(fizmo::units::convert_time(object_observed_time, time_unit, fizmo::units::TimeUnit::SECOND));
    return dilated_time(v, t, return_unit);
}

template <typename V, typename T>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value, typename std::common_type<V, T, double>::type>::type
proper_time(
    const V object_velocity, const T stationary_observed_time, 
    const fizmo::units::TimeUnit return_unit = fizmo::units::TimeUnit::SECOND
) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    if (stationary_observed_time < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(object_velocity);
    if (lorentz == CT(0)) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(stationary_observed_time) / lorentz;
    return static_cast<CT>(fizmo::units::convert_time(value, fizmo::units::TimeUnit::SECOND, return_unit));
}

template <typename V, typename T>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<T>::value, typename std::common_type<V, T, double>::type>::type
proper_time(
    const V object_velocity, const T stationary_observed_time, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::TimeUnit time_unit, 
    const fizmo::units::TimeUnit return_unit = fizmo::units::TimeUnit::SECOND
) noexcept {
    using CT = typename std::common_type<V, T, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(object_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT t = static_cast<CT>(fizmo::units::convert_time(stationary_observed_time, time_unit, fizmo::units::TimeUnit::SECOND));
    return proper_time(v, t, return_unit);
}

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
time_dilation_velocity(
    const S stationary_observed_time, const O object_observed_time, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    if (stationary_observed_time <= 0 || object_observed_time <= 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT T = static_cast<CT>(stationary_observed_time);
    const CT T_prime = static_cast<CT>(object_observed_time);
    const CT arg1 = CT(1) - T / T_prime;
    const CT arg2 = CT(1) + T / T_prime;
    if (arg1 < 0 || arg2 < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT velocity = fizmo::math::sqrt_constexpr(arg1) * fizmo::math::sqrt_constexpr(arg2);
    return static_cast<CT>(fizmo::units::convert_velocity(velocity, fizmo::units::VelocityUnit::light_speed(), return_unit));
}

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
time_dilation_velocity(
    const S stationary_observed_time, const O object_observed_time, 
    const fizmo::units::TimeUnit stationary_time_unit, const fizmo::units::TimeUnit object_time_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    const CT t_s = static_cast<CT>(fizmo::units::convert_time(stationary_observed_time, stationary_time_unit, fizmo::units::TimeUnit::SECOND));
    const CT t_o = static_cast<CT>(fizmo::units::convert_time(object_observed_time, object_time_unit, fizmo::units::TimeUnit::SECOND));
    return time_dilation_velocity(t_s, t_o, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // TIME_DILATION_HPP