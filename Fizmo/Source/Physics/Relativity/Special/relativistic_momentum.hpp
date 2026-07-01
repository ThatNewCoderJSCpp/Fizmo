#ifndef FIZMO_RELATIVISTIC_MOMENTUM_HPP
#define FIZMO_RELATIVISTIC_MOMENTUM_HPP

#include "lorentz.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename V, typename M, class RU = units::MomentumUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MOMENTUM, typename std::common_type<V, M, double>::type>::type
relativistic_momentum(const V velocity, const M mass, const RU return_unit = units::momentum::kilogram_meter_per_second) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    if (mass < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT value = lorentz * static_cast<CT>(mass) * static_cast<CT>(velocity) * c;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::momentum::kilogram_meter_per_second, return_unit));
}

template <typename V, typename M, class VU, class MU, class RU = units::MomentumUnit>
constexpr typename std::enable_if<std::is_arithmetic<V>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_MOMENTUM, typename std::common_type<V, M, double>::type>::type
relativistic_momentum(const V velocity, const M mass, const VU velocity_unit, const MU mass_unit, const RU return_unit = units::momentum::kilogram_meter_per_second) noexcept {
    using CT = typename std::common_type<V, M, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return relativistic_momentum(v, m, return_unit);
}

template <typename P, typename M, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<P, M, double>::type>::type
relativistic_momentum_velocity(const P momentum, const M mass, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    if (mass <= 0 || momentum < 0) { return constants::QUIET_NAN<CT>; }
    const CT p = static_cast<CT>(momentum);
    const CT m = static_cast<CT>(mass);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT p_over_mc = p / (m * c);
    const CT p_over_mc_sq = p_over_mc * p_over_mc;
    const CT denom_sq = CT(1) + p_over_mc_sq;
    if (denom_sq <= 0) { return constants::QUIET_NAN<CT>; }
    const CT value = p_over_mc / math::sqrt_constexpr(denom_sq);
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::velocity::light_speed, return_unit));
}

template <typename P, typename M, class PU, class MU, class RU = units::VelocityUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<M>::value && is_fizmo_unit_v<PU> && is_fizmo_unit_v<MU> && is_fizmo_unit_v<RU> && PU::dimension() == units::DIMENSION_MOMENTUM && MU::dimension() == units::DIMENSION_MASS && RU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<P, M, double>::type>::type
relativistic_momentum_velocity(const P momentum, const M mass, const PU momentum_unit, const MU mass_unit, const RU return_unit = units::velocity::light_speed) noexcept {
    using CT = typename std::common_type<P, M, double>::type;
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    const CT m = static_cast<CT>(units::convert(static_cast<long double>(mass), mass_unit, units::mass::kilogram));
    return relativistic_momentum_velocity(p, m, return_unit);
}

template <typename P, typename V, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<RU> && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<P, V, double>::type>::type
relativistic_momentum_mass(const V velocity, const P momentum, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<P, V, double>::type;
    if (momentum < 0) { return constants::QUIET_NAN<CT>; }
    const CT lorentz = lorentz_factor(velocity);
    const CT v = static_cast<CT>(velocity);
    const CT c = constants::LIGHT_SPEED<CT>;
    const CT denom = lorentz * v * c;
    if (abs_constexpr(denom) <= constants::TYPE_EPSILON<CT>) { return constants::QUIET_NAN<CT>; }
    const CT value = static_cast<CT>(momentum) / denom;
    return static_cast<CT>(units::convert(static_cast<long double>(value), units::mass::kilogram, return_unit));
}

template <typename P, typename V, class VU, class PU, class RU = units::MassUnit>
constexpr typename std::enable_if<std::is_arithmetic<P>::value && std::is_arithmetic<V>::value && is_fizmo_unit_v<VU> && is_fizmo_unit_v<PU> && is_fizmo_unit_v<RU> && VU::dimension() == units::DIMENSION_VELOCITY && PU::dimension() == units::DIMENSION_MOMENTUM && RU::dimension() == units::DIMENSION_MASS, typename std::common_type<P, V, double>::type>::type
relativistic_momentum_mass(const V velocity, const P momentum, const VU velocity_unit, const PU momentum_unit, const RU return_unit = units::mass::kilogram) noexcept {
    using CT = typename std::common_type<P, V, double>::type;
    const CT v = static_cast<CT>(units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed));
    const CT p = static_cast<CT>(units::convert(static_cast<long double>(momentum), momentum_unit, units::momentum::kilogram_meter_per_second));
    return relativistic_momentum_mass(v, p, return_unit);
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_RELATIVISTIC_MOMENTUM_HPP