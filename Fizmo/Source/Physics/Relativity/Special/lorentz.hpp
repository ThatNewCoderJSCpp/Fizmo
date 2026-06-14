#ifndef LORENTZ_FACTOR_HPP
#define LORENTZ_FACTOR_HPP

#include "../../../Converters/unit_converters.hpp"
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

template <typename T>
constexpr typename std::enable_if<std::is_arithmetic<T>::value, typename std::common_type<T, double>::type>::type 
lorentz_factor(const T velocity, const fizmo::units::VelocityUnit velocity_unit) { 
    return lorentz_factor(
        static_cast<typename std::common_type<T, double>::type>(fizmo::units::convert_velocity(velocity, velocity_unit, fizmo::units::VelocityUnit::light_speed()))
    ); 
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // LORENTZ_FACTOR_HPP