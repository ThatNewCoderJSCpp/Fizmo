#ifndef FIZMO_UNITS_RADIATION_HPP
#define FIZMO_UNITS_RADIATION_HPP

#include "../units.hpp"

namespace fizmo {
namespace units {
namespace radiation {

constexpr AbsorbedDoseUnit gray   (1.0L);
constexpr AbsorbedDoseUnit sievert(1.0L);
constexpr AbsorbedDoseUnit rad    (0.01L);
constexpr AbsorbedDoseUnit rem    (0.01L);

constexpr FrequencyUnit becquerel (1.0L);
constexpr FrequencyUnit curie     (3.7e10L);
constexpr FrequencyUnit rutherford(1e6L);

} // namespace radiation
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_RADIATION_HPP