#ifndef FIZMO_UNITS_PRESSURE_HPP
#define FIZMO_UNITS_PRESSURE_HPP

#include "force.hpp"
#include "area.hpp"

namespace fizmo {
namespace units {
namespace pressure {

constexpr PressureUnit pascal_ = force::newton / area::square_meter;     
FIZMO_SI_PREFIXED(pascal_, pascal_);                 

constexpr PressureUnit bar(1e5L);
FIZMO_SI_PREFIXED(bar, bar);

constexpr PressureUnit atmosphere(101325.0L);
constexpr PressureUnit torr      (101325.0L / 760.0L);
constexpr PressureUnit mmHg      (133.322387415L);
constexpr PressureUnit psi = force::pound_force / area::square_inch;  

} // namespace pressure
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_PRESSURE_HPP