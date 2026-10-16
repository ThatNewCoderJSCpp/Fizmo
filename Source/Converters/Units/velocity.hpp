#ifndef FIZMO_UNITS_VELOCITY_HPP
#define FIZMO_UNITS_VELOCITY_HPP

#include "distance.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {
namespace velocity {

constexpr VelocityUnit meters_per_second = distance::meter / time::second;
FIZMO_SI_PREFIXED(meters_per_second, meters_per_second);

constexpr VelocityUnit meters_per_hour = distance::meter / time::hour;
FIZMO_SI_PREFIXED(meters_per_hour, meters_per_hour);

constexpr VelocityUnit miles_per_hour  = distance::mile / time::hour;
constexpr VelocityUnit feet_per_second = distance::foot / time::second;
constexpr VelocityUnit knot            = distance::nautical_mile / time::hour;
constexpr VelocityUnit light_speed(exact_constants::speed_of_light);

} // namespace velocity
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_VELOCITY_HPP