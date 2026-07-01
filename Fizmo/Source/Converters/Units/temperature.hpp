#ifndef FIZMO_UNITS_TEMPERATURE_HPP
#define FIZMO_UNITS_TEMPERATURE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace temperature {

// POINT units (absolute temperatures) carry the offset
// Use these with convert(). DO NOT multiply/divide them
constexpr TemperatureUnit kelvin    (1.0L,        0.0L);
constexpr TemperatureUnit celsius   (1.0L,        273.15L);
constexpr TemperatureUnit fahrenheit(5.0L/9.0L,   273.15L - 32.0L*5.0L/9.0L);
constexpr TemperatureUnit rankine   (5.0L/9.0L,   0.0L);
constexpr TemperatureUnit reaumur   (5.0L/4.0L,   273.15L);
constexpr TemperatureUnit romer     (40.0L/21.0L, 273.15L - 7.5L*40.0L/21.0L);
constexpr TemperatureUnit delisle   (-2.0L/3.0L,  373.15L);   
constexpr TemperatureUnit newton    (100.0L/33.0L,273.15L);

// Offset 0, so they compose correctly inside rates like J/(kg*K) or cal/(g*C)
// DO NOT use these with convert() - they are for multiplying/dividing only
constexpr TemperatureUnit kelvin_interval    (1.0L);
constexpr TemperatureUnit celsius_interval   (1.0L);          
constexpr TemperatureUnit fahrenheit_interval(5.0L/9.0L);   

} // namespace temperature
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_TEMPERATURE_HPP