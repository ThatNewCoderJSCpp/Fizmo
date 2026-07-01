#ifndef FIZMO_UNITS_VELOCITY_HPP
#define FIZMO_UNITS_VELOCITY_HPP

#include "distance.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {
namespace velocity {

constexpr auto meters_per_second = distance::meter / time::second;
FIZMO_SI_PREFIXED(meters_per_second, meters_per_second);

constexpr auto meters_per_hour = distance::meter / time::hour;
FIZMO_SI_PREFIXED(meters_per_hour, meters_per_hour);

constexpr auto miles_per_hour  = distance::mile / time::hour;
constexpr auto feet_per_second = distance::foot / time::second;
constexpr auto knot            = distance::nautical_mile / time::hour;
constexpr VelocityUnit light_speed(constants::LIGHT_SPEED<long double>);

} // namespace velocity
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_VELOCITY_HPP