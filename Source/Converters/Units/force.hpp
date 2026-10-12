#ifndef FIZMO_UNITS_FORCE_HPP
#define FIZMO_UNITS_FORCE_HPP

#include "mass.hpp"
#include "acceleration.hpp"

namespace fizmo {
namespace units {
namespace force {

constexpr ForceUnit newton      = mass::kilogram * acceleration::meters_per_second_squared;  
FIZMO_SI_PREFIXED(newton, newton);

constexpr ForceUnit dyne(exact("1e-5"));
constexpr ForceUnit gram_force  = mass::gram * acceleration::standard_gravity;
FIZMO_SI_PREFIXED(gram_force, gram_force);

constexpr ForceUnit pound_force = mass::pound * acceleration::standard_gravity;
constexpr ForceUnit poundal(exact("0.138254954376"));
constexpr ForceUnit sthene (exact("1e3"));
constexpr ForceUnit ton_force   = mass::tonne * acceleration::standard_gravity;

} // namespace force
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_FORCE_HPP