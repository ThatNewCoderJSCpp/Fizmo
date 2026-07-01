#ifndef FIZMO_UNITS_MECHANICS_HPP
#define FIZMO_UNITS_MECHANICS_HPP

#include "mass.hpp"
#include "velocity.hpp"
#include "force.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {

namespace momentum {
    constexpr MomentumUnit kilogram_meter_per_second = mass::kilogram * velocity::meters_per_second;
    constexpr MomentumUnit gram_centimeter_per_second(1e-5L);
} // namespace momentum

namespace impulse {
    constexpr ImpulseUnit newton_second      = force::newton * time::second;
    constexpr ImpulseUnit pound_force_second = force::pound_force * time::second;
} // namespace impulse

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_MECHANICS_HPP