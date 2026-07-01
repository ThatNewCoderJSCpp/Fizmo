#ifndef FIZMO_LORENTZ_FACTOR_HPP
#define FIZMO_LORENTZ_FACTOR_HPP

#include "../../../Converters/quantity.hpp"
#include "../../../Standard Overloads/sqrt.hpp"

namespace fizmo {
namespace relativity {
namespace special {

template <typename T>
constexpr typename std::enable_if<std::is_arithmetic<T>::value, typename std::common_type<T, double>::type>::type
lorentz_factor(const T velocity) noexcept {
    using CT = typename std::common_type<T, double>::type;
    const CT arg = CT(1) - static_cast<CT>(velocity) * static_cast<CT>(velocity);
    if (arg <= fizmo::constants::TYPE_EPSILON<CT>) { return fizmo::constants::QUIET_NAN<CT>; }
    return CT(1) / fizmo::math::sqrt_constexpr(arg);
}

template <typename T, class VU>
constexpr typename std::enable_if<std::is_arithmetic<T>::value && is_fizmo_unit_v<VU> && VU::dimension() == units::DIMENSION_VELOCITY, typename std::common_type<T, double>::type>::type
lorentz_factor(const T velocity, const VU velocity_unit) {
    return lorentz_factor(static_cast<typename std::common_type<T, double>::type>(fizmo::units::convert(static_cast<long double>(velocity), velocity_unit, units::velocity::light_speed)));
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_LORENTZ_FACTOR_HPP