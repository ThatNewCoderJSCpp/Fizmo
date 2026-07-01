#ifndef FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP
#define FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename S, typename O, class RU = units::FrequencyUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_FREQUENCY, typename std::common_type<S, O, double>::type>::type
doppler_source_frequency(const S source_velocity, const O observed_frequency, const RU return_unit = units::frequency::hertz) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    if (observed_frequency < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(source_velocity);
    const CT value = static_cast<CT>(observed_frequency) * lorentz * (1 - static_cast<CT>(source_velocity));
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::frequency::hertz, return_unit));
}

template <typename S, typename O, class VU, class FU, class RU = units::FrequencyUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<FU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && FU::dimension() == units::DIMENSION_FREQUENCY && RU::dimension() == units::DIMENSION_FREQUENCY, typename std::common_type<S, O, double>::type>::type
doppler_source_frequency(const S source_velocity, const O observed_frequency, const VU velocity_unit, const FU frequency_unit, const RU return_unit = units::frequency::hertz) noexcept {
    using CT = typename std::common_type<S, O, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(source_velocity), velocity_unit, units::velocity::light_speed));
    const CT o = static_cast<CT>(units::convert(static_cast<long double>(observed_frequency), frequency_unit, units::frequency::hertz));
    return doppler_source_frequency(v, o, return_unit);
}

template <typename V, typename F, class RU = units::FrequencyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<F>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_FREQUENCY, typename std::common_type<V, F, double>::type>::type
doppler_observed_frequency(const V source_velocity, const F source_frequency, const RU return_unit = units::frequency::hertz) noexcept {
    using CT = typename std::common_type<V, F, double>::type;
    const CT v = static_cast<CT>(source_velocity);
    const CT f = static_cast<CT>(source_frequency);
    if (f < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(source_velocity);
    const CT value = f / (lorentz * (1 - v));
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::frequency::hertz, return_unit));
}

template <typename V, typename F, class VU, class FU, class RU = units::FrequencyUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<F>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<FU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && FU::dimension() == units::DIMENSION_FREQUENCY && RU::dimension() == units::DIMENSION_FREQUENCY, typename std::common_type<V, F, double>::type>::type
doppler_observed_frequency(const V source_velocity, const F source_frequency, const VU velocity_unit, const FU frequency_unit, const RU return_unit = units::frequency::hertz) noexcept {
    using CT = typename std::common_type<V, F, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(source_velocity), velocity_unit, units::velocity::light_speed));
    const CT s = static_cast<CT>(units::convert(static_cast<long double>(source_frequency), frequency_unit, units::frequency::hertz));
    return doppler_observed_frequency(v, s, return_unit);
}

template <typename S, typename O, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<S, O, double>::type>::type
doppler_source_velocity(const S source_frequency, const O observed_frequency, const RU return_unit = units::velocity::light_speed) {
    using CT = typename std::common_type<S, O, double>::type;
    const CT sf = static_cast<CT>(source_frequency);
    const CT sf2 = sf * sf;
    const CT of = static_cast<CT>(observed_frequency);
    const CT of2 = of * of;
    const CT value = (of2 - sf2) / (of2 + sf2);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename S, typename O, class SU, class OU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<S>::value && std::is_arithmetic<O>::value && is_fizmo_unit_v<SU> && is_fizmo_unit_v<OU> && is_fizmo_unit_v<RU> && SU::dimension() == units::DIMENSION_FREQUENCY && OU::dimension() == units::DIMENSION_FREQUENCY && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<S, O, double>::type>::type
doppler_source_velocity(const S source_frequency, const O observed_frequency, const SU source_unit, const OU observed_unit, const RU return_unit = units::velocity::light_speed) {
    using CT = typename std::common_type<S, O, double>::type;
    const CT s = static_cast<CT>(units::convert(static_cast<long double>(source_frequency), source_unit, units::frequency::hertz));
    const CT o = static_cast<CT>(units::convert(static_cast<long double>(observed_frequency), observed_unit, units::frequency::hertz));
    return doppler_source_velocity(s, o, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_DOPPLER_EFFECT_HPP