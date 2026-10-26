#ifndef FIZMO_CHEMISTRY_SOLVERS_HPP
#define FIZMO_CHEMISTRY_SOLVERS_HPP

#include "bonds.hpp"
#include "reaction.hpp"
#include "units.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct SolverVariable {
    std::string id;
    std::string label;
    Dimension dimension = Dimension::Dimensionless;
    std::string unit;
    bool positive = true;
    std::string unit_label;
};

struct ChemistryEquation {
    std::string id;
    std::string name;
    std::string category;
    std::string formula;
    std::vector<SolverVariable> variables;
    std::function<double(const std::vector<double>&)> residual;
    std::string notes;

    int index_of(std::string_view variable_id) const noexcept;
};

const std::vector<ChemistryEquation>& chemistry_equations();
const ChemistryEquation* find_equation(std::string_view id);
std::vector<std::string> equation_categories();

struct EquationSolution {
    bool ok = false;
    int unknown = -1;
    double value = NO_VALUE;
    std::vector<double> values;
    std::string message;
};

EquationSolution solve_equation(const ChemistryEquation& equation, const std::vector<double>& si_values, int unknown = -1);

enum class AmountKind : std::uint8_t {
    Mass,
    Moles,
    Particles,
    GasVolume,
    SolutionVolume
};

const char* to_string(AmountKind k) noexcept;

struct StoichiometryKnown {
    std::size_t species = 0;
    AmountKind kind = AmountKind::Moles;
    double value = NO_VALUE;
    double molarity = NO_VALUE;
};

struct StoichiometrySpecies {
    std::string formula;
    bool reactant = true;
    double coefficient = 1.0;
    double molar_mass = NO_VALUE;
    double given_moles = NO_VALUE;
    double reacted_moles = 0.0;
    double final_moles = NO_VALUE;
    bool limiting = false;
    bool gas = false;
};

struct StoichiometryResult {
    bool ok = false;
    std::string message;
    Reaction reaction;
    double extent = NO_VALUE;
    int limiting = -1;
    std::vector<StoichiometrySpecies> species;
    std::vector<std::string> steps;
};

StoichiometryResult solve_stoichiometry(const Reaction& reaction, const std::vector<StoichiometryKnown>& knowns, double temperature = STANDARD_TEMPERATURE, double pressure = ATMOSPHERE);
double amount_to_moles(AmountKind kind, double value_si, double molar_mass, double temperature, double pressure, double molarity = NO_VALUE);
double moles_to_amount(AmountKind kind, double moles, double molar_mass, double temperature, double pressure, double molarity = NO_VALUE);

struct BondThermo {
    double enthalpy_298 = NO_VALUE;
    double enthalpy = NO_VALUE;
    double entropy = NO_VALUE;
    double gibbs = NO_VALUE;
    double equilibrium_constant = NO_VALUE;
    double fraction_broken = NO_VALUE;
    double half_broken_temperature = NO_VALUE;
    double length = NO_VALUE;
};

BondThermo bond_dissociation(const Element& a, const Element& b, BondOrder order, double temperature, double pressure, double length_pm = NO_VALUE, double energy_kj_per_mol = NO_VALUE);
double translational_entropy(double mass_u, double temperature, double pressure = STANDARD_PRESSURE) noexcept;
double rotational_entropy_linear(double moment_of_inertia, double temperature, int symmetry) noexcept;

struct PercentComposition {
    const Element* element = nullptr;
    double count = 0.0;
    double mass = 0.0;
    double percent = 0.0;
};

std::vector<PercentComposition> percent_composition(const Formula& formula);
Formula empirical_formula_from_masses(const std::vector<std::pair<const Element*, double>>& grams_or_percent);
Formula molecular_formula(const Formula& empirical, double molar_mass);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SOLVERS_HPP
