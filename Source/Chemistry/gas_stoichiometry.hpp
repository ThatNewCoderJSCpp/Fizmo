#ifndef FIZMO_CHEMISTRY_GAS_STOICHIOMETRY_HPP
#define FIZMO_CHEMISTRY_GAS_STOICHIOMETRY_HPP

#include "solvers.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class GasModel : std::uint8_t {
    Ideal,
    VanDerWaals
};

struct VanDerWaalsConstants {
    double a = NO_VALUE;
    double b = NO_VALUE;

    bool ok() const noexcept { return a == a && b == b && a > 0.0 && b > 0.0; }
};

struct GasBehavior {
    bool can_be_gas = false;
    bool condensable = false;
    double vapor_pressure = NO_VALUE;
    double boiling_point = NO_VALUE;
    double critical_temperature = NO_VALUE;
    VanDerWaalsConstants vdw;
    std::string source;
};

GasBehavior gas_behavior(const Formula& formula, double temperature);
VanDerWaalsConstants van_der_waals_constants(double critical_temperature, double critical_pressure) noexcept;
VanDerWaalsConstants van_der_waals_mixture(const std::vector<VanDerWaalsConstants>& parts, const std::vector<double>& mole_fractions) noexcept;
double water_vapor_pressure(double temperature) noexcept;
double gas_moles(double pressure, double volume, double temperature, GasModel model = GasModel::Ideal, const VanDerWaalsConstants& c = {});
double gas_pressure(double moles, double volume, double temperature, GasModel model = GasModel::Ideal, const VanDerWaalsConstants& c = {});
double gas_volume(double moles, double pressure, double temperature, GasModel model = GasModel::Ideal, const VanDerWaalsConstants& c = {});

enum class GasAmountKind : std::uint8_t {
    Mass,
    Moles,
    Particles,
    SolutionVolume,
    GasVolume,
    PartialPressure,
    CollectedOverWater
};

const char* to_string(GasAmountKind k) noexcept;

struct GasKnown {
    std::size_t species = 0;
    GasAmountKind kind = GasAmountKind::Moles;
    double value = NO_VALUE;
    double molarity = NO_VALUE;
    double temperature = NO_VALUE;
    double pressure = NO_VALUE;
    double volume = NO_VALUE;
    AmountRole role = AmountRole::Auto;
};

enum class GasVesselMode : std::uint8_t {
    Open,
    Rigid
};

struct GasVessel {
    GasVesselMode mode = GasVesselMode::Open;
    double temperature = STANDARD_TEMPERATURE;
    double pressure = ATMOSPHERE;
    double volume = NO_VALUE;
    bool over_water = false;
    GasModel model = GasModel::Ideal;
    double inert_moles = 0.0;
    std::string inert_name = "inert gas";
};

struct GasSpeciesState {
    std::string formula;
    double moles = 0.0;
    double mole_fraction = 0.0;
    double partial_pressure = NO_VALUE;
    double volume = NO_VALUE;
    double condensed_moles = 0.0;
    double vapor_pressure = NO_VALUE;
    bool reactant = false;
    bool inert = false;
};

struct GasStoichiometryResult {
    bool ok = false;
    std::string message;
    StoichiometryResult stoichiometry;
    std::vector<double> given_moles;
    std::vector<GasSpeciesState> before;
    std::vector<GasSpeciesState> after;
    double moles_before = 0.0;
    double moles_after = 0.0;
    double pressure_before = NO_VALUE;
    double pressure_after = NO_VALUE;
    double volume_before = NO_VALUE;
    double volume_after = NO_VALUE;
    double water_vapor = NO_VALUE;
    double dry_gas_pressure = NO_VALUE;
    std::vector<std::string> steps;
    std::vector<std::string> notes;
};

double gas_known_moles(const GasKnown& known, const Formula& formula, const GasVessel& vessel, std::string* step = nullptr);
GasStoichiometryResult solve_gas_stoichiometry(const Reaction& reaction, const std::vector<GasKnown>& knowns, const GasVessel& vessel);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_GAS_STOICHIOMETRY_HPP
