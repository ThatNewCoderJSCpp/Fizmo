#ifndef FIZMO_UNITS_FREQUENCY_HPP
#define FIZMO_UNITS_FREQUENCY_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace frequency {

constexpr FrequencyUnit hertz(exact("1.0"));
FIZMO_SI_PREFIXED(hertz, hertz);   

constexpr FrequencyUnit rpm(ratio(1, 60));
constexpr FrequencyUnit bpm = rpm;                           

constexpr FIZMO_RECIPROCAL_OF(DIMENSION_FREQUENCY) period_second(ratio(1));
constexpr FIZMO_RECIPROCAL_OF(DIMENSION_FREQUENCY) period_minute(ratio(60));
constexpr FIZMO_RECIPROCAL_OF(DIMENSION_FREQUENCY) period_hour(ratio(3600));
constexpr FIZMO_RECIPROCAL_OF(DIMENSION_FREQUENCY) period_day(ratio(86400));

} // namespace frequency
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_FREQUENCY_HPP