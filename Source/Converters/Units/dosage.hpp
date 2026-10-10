#ifndef FIZMO_UNITS_DOSAGE_HPP
#define FIZMO_UNITS_DOSAGE_HPP

#include "amount.hpp"
#include "mass.hpp"
#include "volume.hpp"

namespace fizmo {
namespace units {
namespace dosage {

constexpr SpecificVolumeUnit liter_per_kilogram = volume::liter / mass::kilogram;
FIZMO_SI_PREFIXED(liter_per_kilogram, liter_per_kilogram);

constexpr SpecificVolumeUnit liter_per_gram = volume::liter / mass::gram;
FIZMO_SI_PREFIXED(liter_per_gram, liter_per_gram);

constexpr SpecificVolumeUnit liter_per_milligram = volume::liter / mass::milligram;
FIZMO_SI_PREFIXED(liter_per_milligram, liter_per_milligram);

constexpr SpecificVolumeUnit cubic_meter_per_kilogram       = volume::cubic_meter / mass::kilogram;
constexpr SpecificVolumeUnit cubic_meter_per_gram           = volume::cubic_meter / mass::gram;
constexpr SpecificVolumeUnit cubic_meter_per_milligram      = volume::cubic_meter / mass::milligram;
constexpr SpecificVolumeUnit cubic_centimeter_per_kilogram  = volume::cubic_centimeter / mass::kilogram;
constexpr SpecificVolumeUnit cubic_centimeter_per_gram      = volume::cubic_centimeter / mass::gram;
constexpr SpecificVolumeUnit cubic_centimeter_per_milligram = volume::cubic_centimeter / mass::milligram;
constexpr SpecificVolumeUnit cubic_millimeter_per_kilogram  = volume::cubic_millimeter / mass::kilogram;
constexpr SpecificVolumeUnit cubic_millimeter_per_gram      = volume::cubic_millimeter / mass::gram;
constexpr SpecificVolumeUnit cubic_millimeter_per_milligram = volume::cubic_millimeter / mass::milligram;

constexpr SpecificAreaUnit square_meter_per_kilogram       = area::square_meter / mass::kilogram;
constexpr SpecificAreaUnit square_meter_per_gram           = area::square_meter / mass::gram;
constexpr SpecificAreaUnit square_meter_per_milligram      = area::square_meter / mass::milligram;
constexpr SpecificAreaUnit square_centimeter_per_kilogram  = area::square_centimeter / mass::kilogram;
constexpr SpecificAreaUnit square_centimeter_per_gram      = area::square_centimeter / mass::gram;
constexpr SpecificAreaUnit square_centimeter_per_milligram = area::square_centimeter / mass::milligram;
constexpr SpecificAreaUnit square_millimeter_per_kilogram  = area::square_millimeter / mass::kilogram;
constexpr SpecificAreaUnit square_millimeter_per_gram      = area::square_millimeter / mass::gram;
constexpr SpecificAreaUnit square_millimeter_per_milligram = area::square_millimeter / mass::milligram;

constexpr MolarityMassUnit mole_per_kilogram = amount::mole / mass::kilogram;
FIZMO_SI_PREFIXED(mole_per_kilogram, mole_per_kilogram);

constexpr MolarityMassUnit mole_per_gram = amount::mole / mass::gram;
FIZMO_SI_PREFIXED(mole_per_gram, mole_per_gram);

constexpr MolarityMassUnit mole_per_milligram = amount::mole / mass::milligram;
FIZMO_SI_PREFIXED(mole_per_milligram, mole_per_milligram);

} // namespace dosage
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_DOSAGE_HPP