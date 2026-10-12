#ifndef FIZMO_UNITS_VOLUME_HPP
#define FIZMO_UNITS_VOLUME_HPP

#include "distance.hpp"

namespace fizmo {
namespace units {
namespace volume {

FIZMO_SI_PREFIXED_POWERED(cubic, meter, distance::meter, 3);

constexpr VolumeUnit liter(exact("1e-3"));
FIZMO_SI_PREFIXED(liter, liter);    

constexpr VolumeUnit us_fluid_ounce(exact("2.95735295625e-5"));
constexpr VolumeUnit us_tablespoon       = us_fluid_ounce * ratio(1, 2);
constexpr VolumeUnit us_teaspoon         = us_fluid_ounce * ratio(1, 6);
constexpr VolumeUnit us_cup              = us_fluid_ounce * ratio(8);
constexpr VolumeUnit us_pint             = us_fluid_ounce * ratio(16);
constexpr VolumeUnit us_quart            = us_fluid_ounce * ratio(32);
constexpr VolumeUnit us_gallon(exact("3.785411784e-3"));

constexpr VolumeUnit cubic_foot(exact("0.028316846592"));
constexpr VolumeUnit cubic_inch(exact("1.6387064e-5"));
constexpr VolumeUnit cubic_yard(exact("0.764554857984"));

constexpr VolumeUnit imperial_fluid_ounce(exact("2.84130625e-5"));
constexpr VolumeUnit imperial_tablespoon = imperial_fluid_ounce * ratio(5, 8);
constexpr VolumeUnit imperial_teaspoon   = imperial_fluid_ounce * ratio(5, 24);
constexpr VolumeUnit imperial_cup        = imperial_fluid_ounce * ratio(10);
constexpr VolumeUnit imperial_pint       = imperial_fluid_ounce * ratio(20);
constexpr VolumeUnit imperial_quart      = imperial_fluid_ounce * ratio(40);
constexpr VolumeUnit imperial_gallon     = imperial_fluid_ounce * ratio(160);

constexpr VolumeUnit uk_fluid_ounce(exact("2.84130625e-5"));
constexpr VolumeUnit uk_pint             = uk_fluid_ounce * ratio(20);
constexpr VolumeUnit uk_quart            = uk_fluid_ounce * ratio(40);
constexpr VolumeUnit uk_gallon(exact("4.54609e-3"));

constexpr VolumeUnit oil_barrel(exact("0.158987294928"));

} // namespace volume
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_VOLUME_HPP