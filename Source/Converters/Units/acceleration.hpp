#ifndef FIZMO_UNITS_ACCELERATION_HPP
#define FIZMO_UNITS_ACCELERATION_HPP

#include "distance.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {
namespace acceleration {

constexpr AccelerationUnit meters_per_second_squared = distance::meter / time::second.pow<2>();
FIZMO_SI_PREFIXED(meters_per_second_squared, meters_per_second_squared);

constexpr AccelerationUnit feet_per_second_squared   = distance::foot / time::second.pow<2>();
constexpr AccelerationUnit gal(exact("1e-2"));  
constexpr AccelerationUnit standard_gravity(exact_constants::standard_gravity);

constexpr AccelerationUnit g_force(long double n) noexcept {
    return AccelerationUnit(exact_constants::standard_gravity * ExactScale::from_long_double(n));
}

} // namespace acceleration
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ACCELERATION_HPP