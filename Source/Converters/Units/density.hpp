#ifndef FIZMO_UNITS_DENSITY_HPP
#define FIZMO_UNITS_DENSITY_HPP

#include "mass.hpp"
#include "area.hpp"
#include "volume.hpp"

namespace fizmo {
namespace units {
namespace density {

constexpr Density3DUnit gram_per_cubic_meter       = mass::gram     / volume::cubic_meter;
FIZMO_SI_PREFIXED(gram_per_cubic_meter, gram_per_cubic_meter);

constexpr Density3DUnit gram_per_cubic_centimeter  = mass::gram     / volume::cubic_centimeter; 
FIZMO_SI_PREFIXED(gram_per_cubic_centimeter, gram_per_cubic_centimeter);

constexpr Density3DUnit gram_per_liter             = mass::gram     / volume::liter;
FIZMO_SI_PREFIXED(gram_per_liter, gram_per_liter);

constexpr Density3DUnit gram_per_milliliter        = mass::gram     / volume::milliliter;
FIZMO_SI_PREFIXED(gram_per_milliliter, gram_per_milliliter);

constexpr Density3DUnit tonne_per_cubic_meter      = mass::tonne    / volume::cubic_meter;

constexpr Density3DUnit pound_per_cubic_yard       = mass::pound    / volume::cubic_yard;
constexpr Density3DUnit pound_per_cubic_foot       = mass::pound    / volume::cubic_foot;
constexpr Density3DUnit pound_per_cubic_inch       = mass::pound    / volume::cubic_inch;
constexpr Density3DUnit ounce_per_cubic_inch       = mass::ounce    / volume::cubic_inch;
constexpr Density3DUnit ounce_per_cubic_foot       = mass::ounce    / volume::cubic_foot;
constexpr Density3DUnit ounce_per_cubic_yard       = mass::ounce    / volume::cubic_yard;
constexpr Density3DUnit slug_per_cubic_foot        = mass::slug     / volume::cubic_foot;
constexpr Density3DUnit slug_per_cubic_yard        = mass::slug     / volume::cubic_yard;
constexpr Density3DUnit slug_per_cubic_inch        = mass::slug     / volume::cubic_inch;
constexpr Density3DUnit pound_per_us_gallon        = mass::pound    / volume::us_gallon;

constexpr Density2DUnit gram_per_square_meter      = mass::gram     / area::square_meter;    
FIZMO_SI_PREFIXED(gram_per_square_meter, gram_per_square_meter);

constexpr Density2DUnit gram_per_square_centimeter = mass::gram     / area::square_centimeter;
FIZMO_SI_PREFIXED(gram_per_square_centimeter, gram_per_square_centimeter);

constexpr Density2DUnit gram_per_hectare           = mass::gram     / area::hectare;
FIZMO_SI_PREFIXED(gram_per_hectare, gram_per_hectare);

constexpr Density2DUnit tonne_per_hectare          = mass::tonne    / area::hectare;

constexpr Density2DUnit pound_per_square_inch      = mass::pound    / area::square_inch;
constexpr Density2DUnit pound_per_square_foot      = mass::pound    / area::square_foot;
constexpr Density2DUnit pound_per_square_yard      = mass::pound    / area::square_yard;
constexpr Density2DUnit ounce_per_square_inch      = mass::ounce    / area::square_inch;
constexpr Density2DUnit ounce_per_square_foot      = mass::ounce    / area::square_foot;
constexpr Density2DUnit ounce_per_square_yard      = mass::ounce    / area::square_yard;

} // namespace density
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_DENSITY_HPP