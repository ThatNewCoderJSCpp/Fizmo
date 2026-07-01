#ifndef FIZMO_UNITS_ANGLE_HPP
#define FIZMO_UNITS_ANGLE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace angle {

constexpr AngleUnit radian   (1.0L);
constexpr AngleUnit degree   (constants::PI_180<long double>);
constexpr AngleUnit gradian  (constants::PI<long double> / 200.0L);
constexpr AngleUnit arcminute(constants::PI_180<long double> / 60.0L);
constexpr AngleUnit arcsecond(constants::PI_180<long double> / 3600.0L);
constexpr AngleUnit turn     (2.0L * constants::PI<long double>);
constexpr AngleUnit revolution = turn;

} // namespace angle
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ANGLE_HPP