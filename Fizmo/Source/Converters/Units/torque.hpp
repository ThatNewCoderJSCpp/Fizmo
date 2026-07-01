#ifndef FIZMO_UNITS_TORQUE_HPP
#define FIZMO_UNITS_TORQUE_HPP

#include "force.hpp"
#include "distance.hpp"

namespace fizmo {
namespace units {
namespace torque {

constexpr TorqueUnit newton_meter = force::newton * distance::meter;
constexpr TorqueUnit pound_foot   = force::pound_force * distance::foot;
constexpr TorqueUnit pound_inch   = force::pound_force * distance::inch;

} // namespace torque
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_TORQUE_HPP