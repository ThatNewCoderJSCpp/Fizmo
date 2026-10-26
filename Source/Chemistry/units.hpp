#ifndef FIZMO_CHEMISTRY_UNITS_HPP
#define FIZMO_CHEMISTRY_UNITS_HPP

#include "common.hpp"
#include "constants.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class Dimension : std::uint8_t {
    Dimensionless,
    Temperature,
    TemperatureDifference,
    Pressure,
    Mass,
    Volume,
    Amount,
    Energy,
    MolarEnergy,
    Length,
    Concentration,
    Molality,
    Time,
    MolarMass,
    Density,
    SpecificVolume,
    MolarVolume,
    SpecificHeat,
    MolarHeatCapacity,
    MolarEntropy,
    Dipole,
    Charge,
    Potential,
    Current,
    Frequency,
    RateConstant,
    Count
};

constexpr const char* to_string(Dimension d) noexcept {
    switch (d) {
        case Dimension::Dimensionless: return "dimensionless";
        case Dimension::Temperature: return "temperature";
        case Dimension::TemperatureDifference: return "temperature difference";
        case Dimension::Pressure: return "pressure";
        case Dimension::Mass: return "mass";
        case Dimension::Volume: return "volume";
        case Dimension::Amount: return "amount";
        case Dimension::Energy: return "energy";
        case Dimension::MolarEnergy: return "molar energy";
        case Dimension::Length: return "length";
        case Dimension::Concentration: return "concentration";
        case Dimension::Molality: return "molality";
        case Dimension::Time: return "time";
        case Dimension::MolarMass: return "molar mass";
        case Dimension::Density: return "density";
        case Dimension::SpecificVolume: return "specific volume";
        case Dimension::MolarVolume: return "molar volume";
        case Dimension::SpecificHeat: return "specific heat";
        case Dimension::MolarHeatCapacity: return "molar heat capacity";
        case Dimension::MolarEntropy: return "molar entropy";
        case Dimension::Dipole: return "dipole moment";
        case Dimension::Charge: return "charge";
        case Dimension::Potential: return "potential";
        case Dimension::Current: return "current";
        case Dimension::Frequency: return "frequency";
        case Dimension::RateConstant: return "rate constant";
        default: return "unknown";
    }
}

struct UnitInfo {
    const char* symbol = "";
    const char* name = "";
    Dimension dimension = Dimension::Dimensionless;
    double scale = 1.0;
    double offset = 0.0;

    constexpr double to_si(double v) const noexcept { return v * scale + offset; }
    constexpr double from_si(double v) const noexcept { return (v - offset) / scale; }
};

Span<UnitInfo> unit_table() noexcept;
std::vector<const UnitInfo*> units_of(Dimension d);
std::vector<std::string> unit_symbols(Dimension d);
const UnitInfo* find_unit(std::string_view symbol_or_name) noexcept;
const UnitInfo* find_unit(std::string_view symbol_or_name, Dimension d) noexcept;
const UnitInfo& si_unit(Dimension d) noexcept;
Dimension dimension_named(std::string_view name) noexcept;

double to_si(double value, std::string_view unit, Dimension d);
double from_si(double value, std::string_view unit, Dimension d);
double convert(double value, std::string_view from, std::string_view to);
double convert(double value, std::string_view from, std::string_view to, Dimension d);
bool convertible(std::string_view from, std::string_view to) noexcept;

struct Quantity {
    double value = NO_VALUE;
    const UnitInfo* unit = nullptr;

    static Quantity parse(std::string_view text);
    static Quantity parse(std::string_view text, Dimension expected, std::string_view default_unit = std::string_view());
    bool valid() const noexcept { return unit != nullptr && value == value; }
    Dimension dimension() const noexcept { return unit ? unit->dimension : Dimension::Dimensionless; }
    double si() const noexcept { return unit ? unit->to_si(value) : value; }
    double in(std::string_view symbol) const;
    std::string to_string(int significant = 6) const;
};

std::string format_number(double value, int significant = 6);
std::string format_quantity(double value_si, Dimension d, std::string_view unit, int significant = 6);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_UNITS_HPP
