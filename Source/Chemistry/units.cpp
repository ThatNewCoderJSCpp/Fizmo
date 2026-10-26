#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

constexpr double FAHRENHEIT_SCALE = 5.0 / 9.0;
constexpr double FAHRENHEIT_OFFSET = 459.67 * 5.0 / 9.0;
constexpr double WAVENUMBER_J_PER_MOL = 100.0 * PLANCK_CONSTANT * SPEED_OF_LIGHT * AVOGADRO_CONSTANT;
constexpr double POUND = 0.45359237;
constexpr double OUNCE = POUND / 16.0;
constexpr double INCH = 0.0254;
constexpr double FOOT = 0.3048;
constexpr double US_GALLON = 3.785411784e-3;
constexpr double BTU = 1055.05585262;

const UnitInfo UNITS[] = {
    { "", "no unit", Dimension::Dimensionless, 1.0, 0.0 },
    { "%", "percent", Dimension::Dimensionless, 0.01, 0.0 },
    { "ppm", "parts per million", Dimension::Dimensionless, 1e-6, 0.0 },
    { "ppb", "parts per billion", Dimension::Dimensionless, 1e-9, 0.0 },

    { "K", "kelvin", Dimension::Temperature, 1.0, 0.0 },
    { "\xC2\xB0" "C", "degree Celsius", Dimension::Temperature, 1.0, ZERO_CELSIUS },
    { "\xC2\xB0" "F", "degree Fahrenheit", Dimension::Temperature, FAHRENHEIT_SCALE, FAHRENHEIT_OFFSET },
    { "\xC2\xB0" "R", "degree Rankine", Dimension::Temperature, FAHRENHEIT_SCALE, 0.0 },

    { "K", "kelvin", Dimension::TemperatureDifference, 1.0, 0.0 },
    { "\xC2\xB0" "C", "degree Celsius", Dimension::TemperatureDifference, 1.0, 0.0 },
    { "\xC2\xB0" "F", "degree Fahrenheit", Dimension::TemperatureDifference, FAHRENHEIT_SCALE, 0.0 },

    { "Pa", "pascal", Dimension::Pressure, 1.0, 0.0 },
    { "kPa", "kilopascal", Dimension::Pressure, 1e3, 0.0 },
    { "MPa", "megapascal", Dimension::Pressure, 1e6, 0.0 },
    { "GPa", "gigapascal", Dimension::Pressure, 1e9, 0.0 },
    { "hPa", "hectopascal", Dimension::Pressure, 1e2, 0.0 },
    { "bar", "bar", Dimension::Pressure, BAR, 0.0 },
    { "mbar", "millibar", Dimension::Pressure, BAR * 1e-3, 0.0 },
    { "atm", "standard atmosphere", Dimension::Pressure, ATMOSPHERE, 0.0 },
    { "torr", "torr", Dimension::Pressure, TORR, 0.0 },
    { "mmHg", "millimetre of mercury", Dimension::Pressure, MMHG, 0.0 },
    { "inHg", "inch of mercury", Dimension::Pressure, 3386.389, 0.0 },
    { "psi", "pound per square inch", Dimension::Pressure, PSI, 0.0 },

    { "kg", "kilogram", Dimension::Mass, 1.0, 0.0 },
    { "g", "gram", Dimension::Mass, 1e-3, 0.0 },
    { "mg", "milligram", Dimension::Mass, 1e-6, 0.0 },
    { "\xC2\xB5" "g", "microgram", Dimension::Mass, 1e-9, 0.0 },
    { "ng", "nanogram", Dimension::Mass, 1e-12, 0.0 },
    { "t", "tonne", Dimension::Mass, 1e3, 0.0 },
    { "lb", "pound", Dimension::Mass, POUND, 0.0 },
    { "oz", "ounce", Dimension::Mass, OUNCE, 0.0 },
    { "u", "atomic mass unit", Dimension::Mass, ATOMIC_MASS_CONSTANT, 0.0 },

    { "m\xC2\xB3", "cubic metre", Dimension::Volume, 1.0, 0.0 },
    { "L", "litre", Dimension::Volume, 1e-3, 0.0 },
    { "mL", "millilitre", Dimension::Volume, 1e-6, 0.0 },
    { "\xC2\xB5" "L", "microlitre", Dimension::Volume, 1e-9, 0.0 },
    { "dL", "decilitre", Dimension::Volume, 1e-4, 0.0 },
    { "cm\xC2\xB3", "cubic centimetre", Dimension::Volume, 1e-6, 0.0 },
    { "dm\xC2\xB3", "cubic decimetre", Dimension::Volume, 1e-3, 0.0 },
    { "mm\xC2\xB3", "cubic millimetre", Dimension::Volume, 1e-9, 0.0 },
    { "ft\xC2\xB3", "cubic foot", Dimension::Volume, FOOT * FOOT * FOOT, 0.0 },
    { "in\xC2\xB3", "cubic inch", Dimension::Volume, INCH * INCH * INCH, 0.0 },
    { "gal", "US gallon", Dimension::Volume, US_GALLON, 0.0 },
    { "qt", "US quart", Dimension::Volume, US_GALLON / 4.0, 0.0 },
    { "cup", "US cup", Dimension::Volume, US_GALLON / 16.0, 0.0 },
    { "fl oz", "US fluid ounce", Dimension::Volume, US_GALLON / 128.0, 0.0 },

    { "mol", "mole", Dimension::Amount, 1.0, 0.0 },
    { "mmol", "millimole", Dimension::Amount, 1e-3, 0.0 },
    { "\xC2\xB5" "mol", "micromole", Dimension::Amount, 1e-6, 0.0 },
    { "nmol", "nanomole", Dimension::Amount, 1e-9, 0.0 },
    { "kmol", "kilomole", Dimension::Amount, 1e3, 0.0 },
    { "particles", "particles", Dimension::Amount, 1.0 / AVOGADRO_CONSTANT, 0.0 },

    { "J", "joule", Dimension::Energy, 1.0, 0.0 },
    { "kJ", "kilojoule", Dimension::Energy, 1e3, 0.0 },
    { "MJ", "megajoule", Dimension::Energy, 1e6, 0.0 },
    { "cal", "calorie", Dimension::Energy, CALORIE, 0.0 },
    { "kcal", "kilocalorie", Dimension::Energy, CALORIE * 1e3, 0.0 },
    { "eV", "electronvolt", Dimension::Energy, ELECTRONVOLT, 0.0 },
    { "Wh", "watt hour", Dimension::Energy, 3600.0, 0.0 },
    { "kWh", "kilowatt hour", Dimension::Energy, 3.6e6, 0.0 },
    { "BTU", "British thermal unit", Dimension::Energy, BTU, 0.0 },

    { "J/mol", "joule per mole", Dimension::MolarEnergy, 1.0, 0.0 },
    { "kJ/mol", "kilojoule per mole", Dimension::MolarEnergy, 1e3, 0.0 },
    { "cal/mol", "calorie per mole", Dimension::MolarEnergy, CALORIE, 0.0 },
    { "kcal/mol", "kilocalorie per mole", Dimension::MolarEnergy, CALORIE * 1e3, 0.0 },
    { "eV", "electronvolt per particle", Dimension::MolarEnergy, EV_PER_PARTICLE_TO_J_PER_MOL, 0.0 },
    { "cm\xE2\x81\xBB\xC2\xB9", "wavenumber", Dimension::MolarEnergy, WAVENUMBER_J_PER_MOL, 0.0 },
    { "Eh", "hartree per particle", Dimension::MolarEnergy, HARTREE_ENERGY * AVOGADRO_CONSTANT, 0.0 },

    { "m", "metre", Dimension::Length, 1.0, 0.0 },
    { "km", "kilometre", Dimension::Length, 1e3, 0.0 },
    { "cm", "centimetre", Dimension::Length, 1e-2, 0.0 },
    { "mm", "millimetre", Dimension::Length, 1e-3, 0.0 },
    { "\xC2\xB5" "m", "micrometre", Dimension::Length, 1e-6, 0.0 },
    { "nm", "nanometre", Dimension::Length, 1e-9, 0.0 },
    { "pm", "picometre", Dimension::Length, 1e-12, 0.0 },
    { "\xC3\x85", "angstrom", Dimension::Length, ANGSTROM, 0.0 },
    { "a\xE2\x82\x80", "bohr", Dimension::Length, BOHR_RADIUS, 0.0 },
    { "in", "inch", Dimension::Length, INCH, 0.0 },
    { "ft", "foot", Dimension::Length, FOOT, 0.0 },

    { "M", "mole per litre", Dimension::Concentration, 1e3, 0.0 },
    { "mM", "millimole per litre", Dimension::Concentration, 1.0, 0.0 },
    { "\xC2\xB5" "M", "micromole per litre", Dimension::Concentration, 1e-3, 0.0 },
    { "nM", "nanomole per litre", Dimension::Concentration, 1e-6, 0.0 },
    { "mol/m\xC2\xB3", "mole per cubic metre", Dimension::Concentration, 1.0, 0.0 },

    { "mol/kg", "mole per kilogram", Dimension::Molality, 1.0, 0.0 },
    { "mmol/kg", "millimole per kilogram", Dimension::Molality, 1e-3, 0.0 },

    { "s", "second", Dimension::Time, 1.0, 0.0 },
    { "ms", "millisecond", Dimension::Time, 1e-3, 0.0 },
    { "\xC2\xB5" "s", "microsecond", Dimension::Time, 1e-6, 0.0 },
    { "ns", "nanosecond", Dimension::Time, 1e-9, 0.0 },
    { "min", "minute", Dimension::Time, 60.0, 0.0 },
    { "h", "hour", Dimension::Time, 3600.0, 0.0 },
    { "d", "day", Dimension::Time, 86400.0, 0.0 },
    { "yr", "year", Dimension::Time, 365.25 * 86400.0, 0.0 },

    { "g/mol", "gram per mole", Dimension::MolarMass, 1.0, 0.0 },
    { "kg/mol", "kilogram per mole", Dimension::MolarMass, 1e3, 0.0 },
    { "Da", "dalton", Dimension::MolarMass, 1.0, 0.0 },
    { "kDa", "kilodalton", Dimension::MolarMass, 1e3, 0.0 },

    { "kg/m\xC2\xB3", "kilogram per cubic metre", Dimension::Density, 1.0, 0.0 },
    { "g/cm\xC2\xB3", "gram per cubic centimetre", Dimension::Density, 1e3, 0.0 },
    { "g/mL", "gram per millilitre", Dimension::Density, 1e3, 0.0 },
    { "g/L", "gram per litre", Dimension::Density, 1.0, 0.0 },
    { "kg/L", "kilogram per litre", Dimension::Density, 1e3, 0.0 },
    { "lb/ft\xC2\xB3", "pound per cubic foot", Dimension::Density, POUND / (FOOT * FOOT * FOOT), 0.0 },

    { "m\xC2\xB3/kg", "cubic metre per kilogram", Dimension::SpecificVolume, 1.0, 0.0 },
    { "L/kg", "litre per kilogram", Dimension::SpecificVolume, 1e-3, 0.0 },
    { "cm\xC2\xB3/g", "cubic centimetre per gram", Dimension::SpecificVolume, 1e-3, 0.0 },
    { "mL/g", "millilitre per gram", Dimension::SpecificVolume, 1e-3, 0.0 },
    { "L/g", "litre per gram", Dimension::SpecificVolume, 1.0, 0.0 },
    { "ft\xC2\xB3/lb", "cubic foot per pound", Dimension::SpecificVolume, (FOOT * FOOT * FOOT) / POUND, 0.0 },

    { "m\xC2\xB3/mol", "cubic metre per mole", Dimension::MolarVolume, 1.0, 0.0 },
    { "L/mol", "litre per mole", Dimension::MolarVolume, 1e-3, 0.0 },
    { "cm\xC2\xB3/mol", "cubic centimetre per mole", Dimension::MolarVolume, 1e-6, 0.0 },
    { "mL/mol", "millilitre per mole", Dimension::MolarVolume, 1e-6, 0.0 },

    { "J/(kg\xC2\xB7K)", "joule per kilogram kelvin", Dimension::SpecificHeat, 1.0, 0.0 },
    { "J/(g\xC2\xB7K)", "joule per gram kelvin", Dimension::SpecificHeat, 1e3, 0.0 },
    { "kJ/(kg\xC2\xB7K)", "kilojoule per kilogram kelvin", Dimension::SpecificHeat, 1e3, 0.0 },
    { "cal/(g\xC2\xB7K)", "calorie per gram kelvin", Dimension::SpecificHeat, CALORIE * 1e3, 0.0 },
    { "BTU/(lb\xC2\xB7\xC2\xB0" "F)", "BTU per pound degree Fahrenheit", Dimension::SpecificHeat, 4186.8, 0.0 },

    { "J/(mol\xC2\xB7K)", "joule per mole kelvin", Dimension::MolarHeatCapacity, 1.0, 0.0 },
    { "kJ/(mol\xC2\xB7K)", "kilojoule per mole kelvin", Dimension::MolarHeatCapacity, 1e3, 0.0 },
    { "cal/(mol\xC2\xB7K)", "calorie per mole kelvin", Dimension::MolarHeatCapacity, CALORIE, 0.0 },

    { "J/(mol\xC2\xB7K)", "joule per mole kelvin", Dimension::MolarEntropy, 1.0, 0.0 },
    { "kJ/(mol\xC2\xB7K)", "kilojoule per mole kelvin", Dimension::MolarEntropy, 1e3, 0.0 },
    { "cal/(mol\xC2\xB7K)", "calorie per mole kelvin", Dimension::MolarEntropy, CALORIE, 0.0 },

    { "D", "debye", Dimension::Dipole, DEBYE, 0.0 },
    { "C\xC2\xB7m", "coulomb metre", Dimension::Dipole, 1.0, 0.0 },
    { "e\xC2\xB7\xC3\x85", "elementary charge angstrom", Dimension::Dipole, ELEMENTARY_CHARGE * ANGSTROM, 0.0 },

    { "C", "coulomb", Dimension::Charge, 1.0, 0.0 },
    { "mC", "millicoulomb", Dimension::Charge, 1e-3, 0.0 },
    { "e", "elementary charge", Dimension::Charge, ELEMENTARY_CHARGE, 0.0 },
    { "F", "faraday", Dimension::Charge, FARADAY_CONSTANT, 0.0 },
    { "mAh", "milliampere hour", Dimension::Charge, 3.6, 0.0 },
    { "Ah", "ampere hour", Dimension::Charge, 3600.0, 0.0 },

    { "V", "volt", Dimension::Potential, 1.0, 0.0 },
    { "mV", "millivolt", Dimension::Potential, 1e-3, 0.0 },

    { "A", "ampere", Dimension::Current, 1.0, 0.0 },
    { "mA", "milliampere", Dimension::Current, 1e-3, 0.0 },
    { "\xC2\xB5" "A", "microampere", Dimension::Current, 1e-6, 0.0 },

    { "Hz", "hertz", Dimension::Frequency, 1.0, 0.0 },
    { "kHz", "kilohertz", Dimension::Frequency, 1e3, 0.0 },
    { "MHz", "megahertz", Dimension::Frequency, 1e6, 0.0 },
    { "GHz", "gigahertz", Dimension::Frequency, 1e9, 0.0 },
    { "THz", "terahertz", Dimension::Frequency, 1e12, 0.0 },

    { "1/s", "per second", Dimension::RateConstant, 1.0, 0.0 },
    { "1/min", "per minute", Dimension::RateConstant, 1.0 / 60.0, 0.0 },
    { "1/h", "per hour", Dimension::RateConstant, 1.0 / 3600.0, 0.0 },
};

struct Alias {
    const char* alias;
    const char* symbol;
};

const Alias ALIASES[] = {
    { "kelvin", "K" }, { "kelvins", "K" },
    { "C", "\xC2\xB0" "C" }, { "degC", "\xC2\xB0" "C" }, { "celsius", "\xC2\xB0" "C" }, { "deg C", "\xC2\xB0" "C" },
    { "F", "\xC2\xB0" "F" }, { "degF", "\xC2\xB0" "F" }, { "fahrenheit", "\xC2\xB0" "F" }, { "deg F", "\xC2\xB0" "F" },
    { "R", "\xC2\xB0" "R" }, { "degR", "\xC2\xB0" "R" }, { "rankine", "\xC2\xB0" "R" },
    { "pascal", "Pa" }, { "pascals", "Pa" }, { "atmosphere", "atm" }, { "atmospheres", "atm" }, { "Torr", "torr" },
    { "mm Hg", "mmHg" }, { "in Hg", "inHg" }, { "bars", "bar" },
    { "gram", "g" }, { "grams", "g" }, { "kilogram", "kg" }, { "kilograms", "kg" }, { "milligram", "mg" }, { "milligrams", "mg" },
    { "ug", "\xC2\xB5" "g" }, { "mcg", "\xC2\xB5" "g" }, { "\xCE\xBC" "g", "\xC2\xB5" "g" }, { "pound", "lb" }, { "pounds", "lb" }, { "lbs", "lb" },
    { "ounce", "oz" }, { "ounces", "oz" }, { "amu", "u" }, { "tonne", "t" }, { "tonnes", "t" },
    { "l", "L" }, { "liter", "L" }, { "liters", "L" }, { "litre", "L" }, { "litres", "L" },
    { "ml", "mL" }, { "milliliter", "mL" }, { "milliliters", "mL" }, { "millilitre", "mL" }, { "millilitres", "mL" },
    { "ul", "\xC2\xB5" "L" }, { "uL", "\xC2\xB5" "L" }, { "\xCE\xBC" "L", "\xC2\xB5" "L" }, { "dl", "dL" },
    { "m3", "m\xC2\xB3" }, { "m^3", "m\xC2\xB3" }, { "cm3", "cm\xC2\xB3" }, { "cm^3", "cm\xC2\xB3" }, { "cc", "cm\xC2\xB3" },
    { "dm3", "dm\xC2\xB3" }, { "dm^3", "dm\xC2\xB3" }, { "mm3", "mm\xC2\xB3" }, { "ft3", "ft\xC2\xB3" }, { "ft^3", "ft\xC2\xB3" },
    { "in3", "in\xC2\xB3" }, { "in^3", "in\xC2\xB3" }, { "gallon", "gal" }, { "gallons", "gal" }, { "quart", "qt" }, { "cups", "cup" },
    { "floz", "fl oz" },
    { "mole", "mol" }, { "moles", "mol" }, { "umol", "\xC2\xB5" "mol" }, { "\xCE\xBC" "mol", "\xC2\xB5" "mol" },
    { "molecules", "particles" }, { "atoms", "particles" }, { "ions", "particles" }, { "formula units", "particles" }, { "particle", "particles" },
    { "joule", "J" }, { "joules", "J" }, { "kilojoule", "kJ" }, { "kilojoules", "kJ" }, { "calorie", "cal" }, { "calories", "cal" },
    { "Cal", "kcal" }, { "Btu", "BTU" }, { "btu", "BTU" },
    { "kJ mol-1", "kJ/mol" }, { "kj/mol", "kJ/mol" }, { "J mol-1", "J/mol" }, { "kcal mol-1", "kcal/mol" },
    { "cm-1", "cm\xE2\x81\xBB\xC2\xB9" }, { "cm^-1", "cm\xE2\x81\xBB\xC2\xB9" }, { "1/cm", "cm\xE2\x81\xBB\xC2\xB9" }, { "hartree", "Eh" },
    { "meter", "m" }, { "meters", "m" }, { "metre", "m" }, { "metres", "m" }, { "um", "\xC2\xB5" "m" }, { "\xCE\xBC" "m", "\xC2\xB5" "m" },
    { "micron", "\xC2\xB5" "m" }, { "A", "\xC3\x85" }, { "angstrom", "\xC3\x85" }, { "angstroms", "\xC3\x85" }, { "Angstrom", "\xC3\x85" },
    { "bohr", "a\xE2\x82\x80" }, { "a0", "a\xE2\x82\x80" }, { "inch", "in" }, { "inches", "in" }, { "foot", "ft" }, { "feet", "ft" },
    { "mol/L", "M" }, { "mol/l", "M" }, { "molar", "M" }, { "mmol/L", "mM" }, { "umol/L", "\xC2\xB5" "M" }, { "uM", "\xC2\xB5" "M" },
    { "mol/m3", "mol/m\xC2\xB3" }, { "m", "mol/kg" }, { "molal", "mol/kg" },
    { "sec", "s" }, { "second", "s" }, { "seconds", "s" }, { "minute", "min" }, { "minutes", "min" }, { "hr", "h" }, { "hour", "h" },
    { "hours", "h" }, { "day", "d" }, { "days", "d" }, { "year", "yr" }, { "years", "yr" }, { "us", "\xC2\xB5" "s" },
    { "g mol-1", "g/mol" }, { "amu/molecule", "g/mol" },
    { "kg/m3", "kg/m\xC2\xB3" }, { "g/cm3", "g/cm\xC2\xB3" }, { "g/cc", "g/cm\xC2\xB3" }, { "g/ml", "g/mL" }, { "g/l", "g/L" },
    { "lb/ft3", "lb/ft\xC2\xB3" }, { "m3/kg", "m\xC2\xB3/kg" }, { "cm3/g", "cm\xC2\xB3/g" }, { "ml/g", "mL/g" }, { "ft3/lb", "ft\xC2\xB3/lb" },
    { "m3/mol", "m\xC2\xB3/mol" }, { "cm3/mol", "cm\xC2\xB3/mol" }, { "l/mol", "L/mol" }, { "ml/mol", "mL/mol" },
    { "J/(g*K)", "J/(g\xC2\xB7K)" }, { "J/gK", "J/(g\xC2\xB7K)" }, { "J/g/K", "J/(g\xC2\xB7K)" }, { "J/(kg*K)", "J/(kg\xC2\xB7K)" },
    { "J/kgK", "J/(kg\xC2\xB7K)" }, { "cal/(g*K)", "cal/(g\xC2\xB7K)" }, { "cal/gC", "cal/(g\xC2\xB7K)" }, { "J/(mol*K)", "J/(mol\xC2\xB7K)" },
    { "J/molK", "J/(mol\xC2\xB7K)" }, { "J/mol/K", "J/(mol\xC2\xB7K)" }, { "debye", "D" }, { "Cm", "C\xC2\xB7m" },
    { "coulomb", "C" }, { "volt", "V" }, { "volts", "V" }, { "amp", "A" }, { "ampere", "A" }, { "amps", "A" }, { "uA", "\xC2\xB5" "A" },
    { "s-1", "1/s" }, { "s^-1", "1/s" }, { "/s", "1/s" }, { "min-1", "1/min" }, { "h-1", "1/h" },
};

const UnitInfo* exact(std::string_view s, Dimension d, bool any) noexcept {
    for (const UnitInfo& u : UNITS) {
        if ((any || u.dimension == d) && s == u.symbol) return &u;
    }
    for (const UnitInfo& u : UNITS) {
        if ((any || u.dimension == d) && s == u.name) return &u;
    }
    return nullptr;
}

const UnitInfo* lookup(std::string_view text, Dimension d, bool any) noexcept {
    const std::string_view s = trim(text);
    if (const UnitInfo* u = exact(s, d, any)) return u;
    for (const Alias& a : ALIASES) {
        if (s == a.alias) {
            if (const UnitInfo* u = exact(a.symbol, d, any)) return u;
        }
    }
    for (const UnitInfo& u : UNITS) {
        if ((any || u.dimension == d) && equals_ignore_case(s, u.symbol)) return &u;
    }
    for (const Alias& a : ALIASES) {
        if (equals_ignore_case(s, a.alias)) {
            if (const UnitInfo* u = exact(a.symbol, d, any)) return u;
        }
    }
    return nullptr;
}

} // namespace

Span<UnitInfo> unit_table() noexcept {
    return Span<UnitInfo>(UNITS, sizeof(UNITS) / sizeof(UNITS[0]));
}

std::vector<const UnitInfo*> units_of(Dimension d) {
    std::vector<const UnitInfo*> out;
    for (const UnitInfo& u : UNITS) {
        if (u.dimension == d) out.push_back(&u);
    }
    return out;
}

std::vector<std::string> unit_symbols(Dimension d) {
    std::vector<std::string> out;
    for (const UnitInfo& u : UNITS) {
        if (u.dimension == d) out.emplace_back(u.symbol);
    }
    return out;
}

const UnitInfo* find_unit(std::string_view symbol_or_name) noexcept {
    return lookup(symbol_or_name, Dimension::Dimensionless, true);
}

const UnitInfo* find_unit(std::string_view symbol_or_name, Dimension d) noexcept {
    return lookup(symbol_or_name, d, false);
}

const UnitInfo& si_unit(Dimension d) noexcept {
    for (const UnitInfo& u : UNITS) {
        if (u.dimension == d && u.scale == 1.0 && u.offset == 0.0) return u;
    }
    return UNITS[0];
}

Dimension dimension_named(std::string_view name) noexcept {
    for (int i = 0; i < static_cast<int>(Dimension::Count); ++i) {
        const Dimension d = static_cast<Dimension>(i);
        if (equals_ignore_case(name, to_string(d))) return d;
    }
    return Dimension::Dimensionless;
}

double to_si(double value, std::string_view unit, Dimension d) {
    const UnitInfo* u = find_unit(unit, d);
    if (!u) throw std::invalid_argument("fizmo::chemistry: '" + std::string(unit) + "' is not a unit of " + to_string(d));
    return u->to_si(value);
}

double from_si(double value, std::string_view unit, Dimension d) {
    const UnitInfo* u = find_unit(unit, d);
    if (!u) throw std::invalid_argument("fizmo::chemistry: '" + std::string(unit) + "' is not a unit of " + to_string(d));
    return u->from_si(value);
}

double convert(double value, std::string_view from, std::string_view to) {
    static const Dimension priority[] = { Dimension::Temperature, Dimension::Pressure, Dimension::Mass, Dimension::Volume, Dimension::Amount, Dimension::Length,
                                          Dimension::Energy, Dimension::MolarEnergy, Dimension::Time, Dimension::Concentration, Dimension::Molality, Dimension::Density,
                                          Dimension::SpecificVolume, Dimension::MolarVolume, Dimension::MolarMass, Dimension::SpecificHeat, Dimension::MolarHeatCapacity,
                                          Dimension::MolarEntropy, Dimension::Dipole, Dimension::Potential, Dimension::Current, Dimension::Frequency, Dimension::RateConstant,
                                          Dimension::TemperatureDifference, Dimension::Dimensionless, Dimension::Charge };
    for (Dimension d : priority) {
        const UnitInfo* a = find_unit(from, d);
        const UnitInfo* b = find_unit(to, d);
        if (a && b) return b->from_si(a->to_si(value));
    }
    throw std::invalid_argument("fizmo::chemistry: can't convert " + std::string(from) + " to " + std::string(to));
}

double convert(double value, std::string_view from, std::string_view to, Dimension d) {
    return from_si(to_si(value, from, d), to, d);
}

bool convertible(std::string_view from, std::string_view to) noexcept {
    for (int i = 0; i < static_cast<int>(Dimension::Count); ++i) {
        const Dimension d = static_cast<Dimension>(i);
        if (find_unit(from, d) && find_unit(to, d)) return true;
    }
    return false;
}

Quantity Quantity::parse(std::string_view text) {
    Quantity q;
    const std::string s(trim(text));
    if (s.empty()) return q;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return q;
    q.value = v;
    q.unit = find_unit(std::string_view(end));
    if (!q.unit && trim(std::string_view(end)).empty()) q.unit = &UNITS[0];
    return q;
}

Quantity Quantity::parse(std::string_view text, Dimension expected, std::string_view default_unit) {
    Quantity q;
    const std::string s(trim(text));
    if (s.empty()) return q;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return q;
    q.value = v;
    const std::string_view rest = trim(std::string_view(end));
    if (rest.empty()) q.unit = default_unit.empty() ? &si_unit(expected) : find_unit(default_unit, expected);
    else q.unit = find_unit(rest, expected);
    return q;
}

double Quantity::in(std::string_view symbol) const {
    if (!unit) return NO_VALUE;
    const UnitInfo* target = find_unit(symbol, unit->dimension);
    if (!target) throw std::invalid_argument("fizmo::chemistry: '" + std::string(symbol) + "' is not a unit of " + chemistry::to_string(unit->dimension));
    return target->from_si(unit->to_si(value));
}

std::string Quantity::to_string(int significant) const {
    if (!unit) return format_number(value, significant);
    std::string out = format_number(value, significant);
    if (*unit->symbol) {
        const std::string_view sym(unit->symbol);
        if (sym != "%" && sym.substr(0, 2) != "\xC2\xB0") out += " ";
        out += unit->symbol;
    }
    return out;
}

std::string format_number(double value, int significant) {
    if (value != value) return "n/a";
    if (std::isinf(value)) return value > 0 ? "\xE2\x88\x9E" : "-\xE2\x88\x9E";
    if (significant < 1) significant = 1;
    if (significant > 17) significant = 17;
    const double a = std::fabs(value);
    char buf[64];
    if (a != 0.0 && (a < 1e-4 || a >= 1e7)) {
        std::snprintf(buf, sizeof(buf), "%.*e", significant - 1, value);
        std::string s(buf);
        const std::size_t e = s.find('e');
        std::string mant = s.substr(0, e);
        std::string ex = s.substr(e + 1);
        if (mant.find('.') != std::string::npos) {
            while (!mant.empty() && mant.back() == '0') mant.pop_back();
            if (!mant.empty() && mant.back() == '.') mant.pop_back();
        }
        int exponent = std::atoi(ex.c_str());
        return mant + "e" + std::to_string(exponent);
    }
    std::snprintf(buf, sizeof(buf), "%.*g", significant, value);
    std::string s(buf);
    if (s.find('e') != std::string::npos) {
        const int magnitude = static_cast<int>(std::floor(std::log10(a)));
        const int decimals = std::max(0, significant - 1 - magnitude);
        double rounded = value;
        if (significant - 1 - magnitude < 0) {
            const double step = std::pow(10.0, magnitude + 1 - significant);
            rounded = std::round(value / step) * step;
        }
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, rounded);
        s = buf;
    }
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    return s;
}

std::string format_quantity(double value_si, Dimension d, std::string_view unit, int significant) {
    const UnitInfo* u = find_unit(unit, d);
    if (!u) u = &si_unit(d);
    Quantity q{ u->from_si(value_si), u };
    return q.to_string(significant);
}

} // namespace chemistry
} // namespace fizmo
