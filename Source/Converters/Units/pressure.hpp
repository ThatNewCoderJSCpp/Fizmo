#ifndef FIZMO_UNITS_PRESSURE_HPP
#define FIZMO_UNITS_PRESSURE_HPP

#include "force.hpp"
#include "area.hpp"

namespace fizmo {
namespace units {
namespace pressure {

constexpr PressureUnit pascal_ = force::newton / area::square_meter;     
FIZMO_SI_PREFIXED(pascal_, pascal_);                 

constexpr PressureUnit bar(exact("1e5"));
FIZMO_SI_PREFIXED(bar, bar);

constexpr PressureUnit atmosphere(exact("101325.0"));
constexpr PressureUnit torr      (ratio(101325, 760));
constexpr PressureUnit mmHg      (exact("133.322387415"));
constexpr PressureUnit psi = force::pound_force / area::square_inch;  

} // namespace pressure
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_PRESSURE_HPP