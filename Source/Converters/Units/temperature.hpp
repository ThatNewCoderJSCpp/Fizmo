#ifndef FIZMO_UNITS_TEMPERATURE_HPP
#define FIZMO_UNITS_TEMPERATURE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace temperature {

// POINT units (absolute temperatures) carry the offset
// Use these with convert(). DO NOT multiply/divide them
constexpr TemperatureUnit kelvin    (ratio(1),       ExactScale::zero());
constexpr TemperatureUnit celsius   (ratio(1),       exact("273.15"));
constexpr TemperatureUnit fahrenheit(ratio(5, 9),    ratio(229835, 900));
constexpr TemperatureUnit rankine   (ratio(5, 9),    ExactScale::zero());
constexpr TemperatureUnit reaumur   (ratio(5, 4),    exact("273.15"));
constexpr TemperatureUnit romer     (ratio(40, 21),  ratio(181205, 700));
constexpr TemperatureUnit delisle   (ratio(-2, 3),   exact("373.15"));
constexpr TemperatureUnit newton    (ratio(100, 33), exact("273.15"));

// Offset 0, so they compose correctly inside rates like J/(kg*K) or cal/(g*C)
// DO NOT use these with convert() - they are for multiplying/dividing only
constexpr TemperatureUnit kelvin_interval    (exact("1.0"));
constexpr TemperatureUnit celsius_interval   (exact("1.0"));          
constexpr TemperatureUnit fahrenheit_interval(ratio(5, 9));

} // namespace temperature
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_TEMPERATURE_HPP