#ifndef FIZMO_UNITS_CONCENTRATION_HPP
#define FIZMO_UNITS_CONCENTRATION_HPP

#include "amount.hpp"
#include "mass.hpp"
#include "volume.hpp"
#include "density.hpp"   

namespace fizmo {
namespace units {
namespace concentration {

constexpr MolarityVolumeUnit mole_per_cubic_meter = amount::mole / volume::cubic_meter;
FIZMO_SI_PREFIXED(mole_per_cubic_meter, mole_per_cubic_meter);

constexpr MolarityVolumeUnit mole_per_cubic_centimeter = amount::mole / volume::cubic_centimeter;
FIZMO_SI_PREFIXED(mole_per_cubic_centimeter, mole_per_cubic_centimeter);

constexpr MolarityVolumeUnit mole_per_cubic_millimeter = amount::mole / volume::cubic_millimeter;
FIZMO_SI_PREFIXED(mole_per_cubic_millimeter, mole_per_cubic_millimeter);

constexpr MolarityVolumeUnit molar = amount::mole / volume::liter;
FIZMO_SI_PREFIXED(molar, molar);  

constexpr MolarityVolumeUnit mole_per_milliliter = amount::mole / volume::milliliter;
FIZMO_SI_PREFIXED(mole_per_milliliter, mole_per_milliliter);  

} // namespace concentration
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_CONCENTRATION_HPP