#ifndef FIZMO_UNITS_VOLUME_HPP
#define FIZMO_UNITS_VOLUME_HPP

#include "distance.hpp"

namespace fizmo {
namespace units {
namespace volume {

FIZMO_SI_PREFIXED_POWERED(cubic, meter, distance::meter, 3);

constexpr VolumeUnit liter(1e-3L);
FIZMO_SI_PREFIXED(liter, liter);    

constexpr VolumeUnit us_fluid_ounce(2.95735295625e-5L);
constexpr VolumeUnit us_tablespoon       = us_fluid_ounce * 0.5L;
constexpr VolumeUnit us_teaspoon         = us_fluid_ounce / 6.0L;
constexpr VolumeUnit us_cup              = us_fluid_ounce * 8.0L;
constexpr VolumeUnit us_pint             = us_fluid_ounce * 16.0L;
constexpr VolumeUnit us_quart            = us_fluid_ounce * 32.0L;
constexpr VolumeUnit us_gallon(3.785411784e-3L);

constexpr VolumeUnit cubic_foot(0.028316846592L);
constexpr VolumeUnit cubic_inch(1.6387064e-5L);
constexpr VolumeUnit cubic_yard(0.764554857984L);

constexpr VolumeUnit imperial_fluid_ounce(2.84130625e-5L);
constexpr VolumeUnit imperial_tablespoon = imperial_fluid_ounce * 0.5L;
constexpr VolumeUnit imperial_teaspoon   = imperial_fluid_ounce / 6.0L;
constexpr VolumeUnit imperial_cup        = imperial_fluid_ounce * 8.0L;
constexpr VolumeUnit imperial_pint       = imperial_fluid_ounce * 16.0L;
constexpr VolumeUnit imperial_quart      = imperial_fluid_ounce * 32.0L;
constexpr VolumeUnit imperial_gallon(3.785411784e-3L);

constexpr VolumeUnit uk_fluid_ounce(2.84130625e-5L);
constexpr VolumeUnit uk_pint             = uk_fluid_ounce * 20.0L;
constexpr VolumeUnit uk_quart            = uk_fluid_ounce * 40.0L;
constexpr VolumeUnit uk_gallon(4.54609e-3L);

constexpr VolumeUnit oil_barrel(0.158987294928L);

} // namespace volume
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_VOLUME_HPP