#ifndef FIZMO_UNITS_RADIATION_HPP
#define FIZMO_UNITS_RADIATION_HPP

#include "../units.hpp"

namespace fizmo {
namespace units {
namespace radiation {

constexpr AbsorbedDoseUnit gray   (exact("1.0"));
constexpr AbsorbedDoseUnit sievert(exact("1.0"));
constexpr AbsorbedDoseUnit rad    (exact("0.01"));
constexpr AbsorbedDoseUnit rem    (exact("0.01"));

constexpr FrequencyUnit becquerel (exact("1.0"));
constexpr FrequencyUnit curie     (exact("3.7e10"));
constexpr FrequencyUnit rutherford(exact("1e6"));

} // namespace radiation
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_RADIATION_HPP