#ifndef RELATIVISTIC_DOPPLER_EFFECT_HPP
#define RELATIVISTIC_DOPPLER_EFFECT_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
doppler_source_frequency(
    const S source_velocity, const O observed_frequency, 
    const fizmo::units::FrequencyUnit return_unit = fizmo::units::FrequencyUnit::HERTZ
) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    if (observed_frequency < 0) { return fizmo::constants::QUIET_NAN<CT>; } 
    const CT lorentz = lorentz_factor(source_velocity);
    const CT value = static_cast<CT>(observed_frequency) * lorentz * (1 - static_cast<CT>(source_velocity));
    return static_cast<CT>(fizmo::units::convert_frequency(value, fizmo::units::FrequencyUnit::HERTZ, return_unit));
}

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
doppler_source_frequency(
    const S source_velocity, const O observed_frequency, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::FrequencyUnit frequency_unit, 
    const fizmo::units::FrequencyUnit return_unit = fizmo::units::FrequencyUnit::HERTZ
) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(source_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT o = static_cast<CT>(fizmo::units::convert_frequency(observed_frequency, frequency_unit, fizmo::units::FrequencyUnit::HERTZ));
    return doppler_source_frequency(v, o, return_unit);
}

template <typename V, typename F>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<F>::value, typename std::common_type<V, F, double>::type>::type
doppler_observed_frequency(
    const V source_velocity, const F source_frequency, 
    const fizmo::units::FrequencyUnit return_unit = fizmo::units::FrequencyUnit::HERTZ
) noexcept {
    using CT = typename std::common_type<V, F, double>::type;
    const CT v = static_cast<CT>(source_velocity);
    const CT f = static_cast<CT>(source_frequency);
    if (f < 0) { return fizmo::constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(source_velocity);
    const CT value = f / (lorentz * (1 - v));
    return static_cast<CT>(fizmo::units::convert_frequency(value, fizmo::units::FrequencyUnit::HERTZ, return_unit));
}

template <typename V, typename F>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<F>::value, typename std::common_type<V, F, double>::type>::type
doppler_observed_frequency(
    const V source_velocity, const F source_frequency, 
    const fizmo::units::VelocityUnit velocity_unit, const fizmo::units::FrequencyUnit frequency_unit, 
    const fizmo::units::FrequencyUnit return_unit = fizmo::units::FrequencyUnit::HERTZ
) noexcept {
    using CT = typename std::common_type<V, F, double>::type;
    const CT v = static_cast<CT>(fizmo::units::convert_velocity(source_velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()));
    const CT s = static_cast<CT>(fizmo::units::convert_frequency(source_frequency, frequency_unit, fizmo::units::FrequencyUnit::HERTZ));
    return doppler_observed_frequency(v, s, return_unit);
}

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
doppler_source_velocity(
    const S source_frequency, const O observed_frequency, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) {
    using CT = typename std::common_type<S, O, double>::type;
    const CT sf = static_cast<CT>(source_frequency);
    const CT sf2 = sf * sf;
    const CT of = static_cast<CT>(observed_frequency);
    const CT of2 = of * of;
    const CT value = (of2 - sf2) / (of2 + sf2);
    return static_cast<CT>(fizmo::units::convert_velocity(value, fizmo::units::VelocityUnit::light_speed(), return_unit));    
}

template <typename S, typename O>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value, typename std::common_type<S, O, double>::type>::type
doppler_source_velocity(
    const double source_frequency, const double observed_frequency, 
    const fizmo::units::FrequencyUnit source_unit, const fizmo::units::FrequencyUnit observed_unit, 
    const fizmo::units::VelocityUnit return_unit = fizmo::units::VelocityUnit::light_speed()
) {
    using CT = typename std::common_type<S, O, double>::type;
    const CT s = static_cast<CT>(fizmo::units::convert_frequency(source_frequency, source_unit, fizmo::units::FrequencyUnit::HERTZ));
    const CT o = static_cast<CT>(fizmo::units::convert_frequency(observed_frequency, observed_unit, fizmo::units::FrequencyUnit::HERTZ));
    return doppler_source_velocity(s, o, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // RELATIVISTIC_DOPPLER_EFFECT_HPP