#ifndef FIZMO_UNITS_EXACT_CONSTANTS_HPP
#define FIZMO_UNITS_EXACT_CONSTANTS_HPP

#include "units.hpp"

namespace fizmo {
namespace units {
namespace exact_constants {

constexpr ExactScale speed_of_light     = exact("299792458");
constexpr ExactScale standard_gravity   = exact("9.80665");
constexpr ExactScale elementary_charge  = exact("1.602176634e-19");
constexpr ExactScale avogadro           = exact("6.02214076e23");
constexpr ExactScale faraday            = avogadro * elementary_charge;
constexpr ExactScale astronomical_unit  = exact("149597870700");
constexpr ExactScale parsec             = astronomical_unit * exact("648000") / ExactScale::pi();
constexpr ExactScale bohr_radius        = exact("5.29177210544e-11");
constexpr ExactScale planck_length      = exact("1.616255e-35");
constexpr ExactScale planck_mass        = exact("2.176434e-8");
constexpr ExactScale planck_time        = exact("5.391247e-44");
constexpr ExactScale dalton             = exact("1.66053906892e-27");
constexpr ExactScale julian_year        = exact("31557600");
constexpr ExactScale pound              = exact("0.45359237");
constexpr ExactScale foot               = exact("0.3048");
constexpr ExactScale inch               = exact("0.0254");

} // namespace exact_constants
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_EXACT_CONSTANTS_HPP
