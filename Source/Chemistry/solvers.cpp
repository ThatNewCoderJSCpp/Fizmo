#include "solvers.hpp"

#include "substance.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace fizmo {
namespace chemistry {

namespace {

constexpr double R = GAS_CONSTANT;
constexpr double F = FARADAY_CONSTANT;
constexpr double H = PLANCK_CONSTANT;
constexpr double C = SPEED_OF_LIGHT;

SolverVariable V(const char* id, const char* label, Dimension d, const char* unit, bool positive = true, const char* unit_label = "") {
    return SolverVariable{ id, label, d, unit, positive, unit_label };
}

double ln(double x) {
    return std::log(x);
}

std::vector<ChemistryEquation> build_equations() {
    using D = Dimension;
    using Vec = const std::vector<double>&;
    std::vector<ChemistryEquation> e;
    const auto add = [&](const char* id, const char* name, const char* cat, const char* formula, std::vector<SolverVariable> vars, std::function<double(Vec)> f, const char* notes = "") {
        e.push_back(ChemistryEquation{ id, name, cat, formula, std::move(vars), std::move(f), notes });
    };
    add("ideal_gas", "Ideal gas law", "Gases", "PV = nRT",
        { V("P", "pressure", D::Pressure, "atm"), V("V", "volume", D::Volume, "L"), V("n", "amount", D::Amount, "mol"), V("T", "temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] * v[1] - v[2] * R * v[3]; });
    add("combined_gas", "Combined gas law", "Gases", "P\xE2\x82\x81V\xE2\x82\x81/T\xE2\x82\x81 = P\xE2\x82\x82V\xE2\x82\x82/T\xE2\x82\x82",
        { V("P1", "initial pressure", D::Pressure, "atm"), V("V1", "initial volume", D::Volume, "L"), V("T1", "initial temperature", D::Temperature, "K"),
          V("P2", "final pressure", D::Pressure, "atm"), V("V2", "final volume", D::Volume, "L"), V("T2", "final temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] * v[1] * v[5] - v[3] * v[4] * v[2]; });
    add("boyle", "Boyle's law", "Gases", "P\xE2\x82\x81V\xE2\x82\x81 = P\xE2\x82\x82V\xE2\x82\x82",
        { V("P1", "initial pressure", D::Pressure, "atm"), V("V1", "initial volume", D::Volume, "L"), V("P2", "final pressure", D::Pressure, "atm"), V("V2", "final volume", D::Volume, "L") },
        [](Vec v) { return v[0] * v[1] - v[2] * v[3]; });
    add("charles", "Charles's law", "Gases", "V\xE2\x82\x81/T\xE2\x82\x81 = V\xE2\x82\x82/T\xE2\x82\x82",
        { V("V1", "initial volume", D::Volume, "L"), V("T1", "initial temperature", D::Temperature, "K"), V("V2", "final volume", D::Volume, "L"), V("T2", "final temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] * v[3] - v[2] * v[1]; });
    add("gay_lussac", "Gay-Lussac's law", "Gases", "P\xE2\x82\x81/T\xE2\x82\x81 = P\xE2\x82\x82/T\xE2\x82\x82",
        { V("P1", "initial pressure", D::Pressure, "atm"), V("T1", "initial temperature", D::Temperature, "K"), V("P2", "final pressure", D::Pressure, "atm"), V("T2", "final temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] * v[3] - v[2] * v[1]; });
    add("avogadro", "Avogadro's law", "Gases", "V\xE2\x82\x81/n\xE2\x82\x81 = V\xE2\x82\x82/n\xE2\x82\x82",
        { V("V1", "initial volume", D::Volume, "L"), V("n1", "initial amount", D::Amount, "mol"), V("V2", "final volume", D::Volume, "L"), V("n2", "final amount", D::Amount, "mol") },
        [](Vec v) { return v[0] * v[3] - v[2] * v[1]; });
    add("gas_density", "Gas density", "Gases", "\xCF\x81 = PM/(RT)",
        { V("rho", "density", D::Density, "g/L"), V("P", "pressure", D::Pressure, "atm"), V("M", "molar mass", D::MolarMass, "g/mol"), V("T", "temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] * R * v[3] - v[1] * v[2] / 1000.0; });
    add("van_der_waals", "Van der Waals gas", "Gases", "(P + an\xC2\xB2/V\xC2\xB2)(V - nb) = nRT",
        { V("P", "pressure", D::Pressure, "atm"), V("V", "volume", D::Volume, "L"), V("n", "amount", D::Amount, "mol"), V("T", "temperature", D::Temperature, "K"),
          V("a", "a", D::Dimensionless, "", true, "L\xC2\xB2\xC2\xB7" "bar/mol\xC2\xB2"), V("b", "b", D::Dimensionless, "", true, "L/mol") },
        [](Vec v) {
            const double a = v[4] * 0.1, b = v[5] * 1e-3;
            return (v[0] + a * v[2] * v[2] / (v[1] * v[1])) * (v[1] - v[2] * b) - v[2] * R * v[3];
        },
        "a and b are in L\xC2\xB2\xC2\xB7" "bar/mol\xC2\xB2 and L/mol (CO2: a = 3.64, b = 0.04267)");
    add("dalton", "Dalton's law", "Gases", "P = P\xE2\x82\x81 + P\xE2\x82\x82 + P\xE2\x82\x83",
        { V("P", "total pressure", D::Pressure, "atm"), V("P1", "partial pressure 1", D::Pressure, "atm"), V("P2", "partial pressure 2", D::Pressure, "atm"), V("P3", "partial pressure 3", D::Pressure, "atm") },
        [](Vec v) { return v[0] - v[1] - v[2] - v[3]; }, "put 0 for unused partial pressures");
    add("partial_pressure", "Partial pressure", "Gases", "P\xE1\xB5\xA2 = x\xE1\xB5\xA2P",
        { V("Pi", "partial pressure", D::Pressure, "atm"), V("x", "mole fraction", D::Dimensionless, ""), V("P", "total pressure", D::Pressure, "atm") },
        [](Vec v) { return v[0] - v[1] * v[2]; });
    add("graham", "Graham's law", "Gases", "r\xE2\x82\x81/r\xE2\x82\x82 = \xE2\x88\x9A(M\xE2\x82\x82/M\xE2\x82\x81)",
        { V("r1", "rate 1", D::Dimensionless, ""), V("r2", "rate 2", D::Dimensionless, ""), V("M1", "molar mass 1", D::MolarMass, "g/mol"), V("M2", "molar mass 2", D::MolarMass, "g/mol") },
        [](Vec v) { return v[0] * std::sqrt(v[2]) - v[1] * std::sqrt(v[3]); });
    add("rms_speed", "RMS molecular speed", "Gases", "u = \xE2\x88\x9A(3RT/M)",
        { V("u", "speed (m/s)", D::Dimensionless, "", true, "m/s"), V("T", "temperature", D::Temperature, "K"), V("M", "molar mass", D::MolarMass, "g/mol") },
        [](Vec v) { return v[0] * v[0] - 3.0 * R * v[1] / (v[2] / 1000.0); });

    add("moles_mass", "Moles from mass", "Stoichiometry", "n = m/M",
        { V("n", "amount", D::Amount, "mol"), V("m", "mass", D::Mass, "g"), V("M", "molar mass", D::MolarMass, "g/mol") },
        [](Vec v) { return v[0] * v[2] / 1000.0 - v[1]; });
    add("particles", "Particles from moles", "Stoichiometry", "N = nN\xE2\x82\x90",
        { V("N", "number of particles", D::Dimensionless, ""), V("n", "amount", D::Amount, "mol") },
        [](Vec v) { return v[0] - v[1] * AVOGADRO_CONSTANT; });
    add("percent_yield", "Percent yield", "Stoichiometry", "% yield = actual/theoretical \xC3\x97 100",
        { V("yield", "percent yield", D::Dimensionless, "", true, "%"), V("actual", "actual yield", D::Mass, "g"), V("theoretical", "theoretical yield", D::Mass, "g") },
        [](Vec v) { return v[0] * v[2] - 100.0 * v[1]; });
    add("density", "Density", "Stoichiometry", "\xCF\x81 = m/V",
        { V("rho", "density", D::Density, "g/mL"), V("m", "mass", D::Mass, "g"), V("V", "volume", D::Volume, "mL") },
        [](Vec v) { return v[0] * v[2] - v[1]; });

    add("molarity", "Molarity", "Solutions", "M = n/V",
        { V("M", "molarity", D::Concentration, "M"), V("n", "amount of solute", D::Amount, "mol"), V("V", "volume of solution", D::Volume, "L") },
        [](Vec v) { return v[0] * v[2] - v[1]; });
    add("molarity_mass", "Molarity from mass", "Solutions", "M = m/(M_w V)",
        { V("M", "molarity", D::Concentration, "M"), V("m", "mass of solute", D::Mass, "g"), V("Mw", "molar mass", D::MolarMass, "g/mol"), V("V", "volume of solution", D::Volume, "L") },
        [](Vec v) { return v[0] * v[3] * (v[2] / 1000.0) - v[1]; });
    add("dilution", "Dilution", "Solutions", "M\xE2\x82\x81V\xE2\x82\x81 = M\xE2\x82\x82V\xE2\x82\x82",
        { V("M1", "initial molarity", D::Concentration, "M"), V("V1", "initial volume", D::Volume, "mL"), V("M2", "final molarity", D::Concentration, "M"), V("V2", "final volume", D::Volume, "mL") },
        [](Vec v) { return v[0] * v[1] - v[2] * v[3]; });
    add("molality", "Molality", "Solutions", "b = n/m_solvent",
        { V("b", "molality", D::Molality, "mol/kg"), V("n", "amount of solute", D::Amount, "mol"), V("m", "mass of solvent", D::Mass, "kg") },
        [](Vec v) { return v[0] * v[2] - v[1]; });
    add("mass_percent", "Mass percent", "Solutions", "w = m_solute/m_solution \xC3\x97 100",
        { V("w", "mass percent", D::Dimensionless, "", true, "%"), V("ms", "mass of solute", D::Mass, "g"), V("mt", "mass of solution", D::Mass, "g") },
        [](Vec v) { return v[0] * v[2] - 100.0 * v[1]; });
    add("mole_fraction", "Mole fraction", "Solutions", "x\xE2\x82\x90 = n\xE2\x82\x90/(n\xE2\x82\x90 + n_b)",
        { V("x", "mole fraction of A", D::Dimensionless, ""), V("na", "moles of A", D::Amount, "mol"), V("nb", "moles of B", D::Amount, "mol") },
        [](Vec v) { return v[0] * (v[1] + v[2]) - v[1]; });
    add("ppm", "Parts per million", "Solutions", "ppm = m_solute/m_solution \xC3\x97 10\xE2\x81\xB6",
        { V("ppm", "concentration (ppm)", D::Dimensionless, ""), V("ms", "mass of solute", D::Mass, "mg"), V("mt", "mass of solution", D::Mass, "kg") },
        [](Vec v) { return v[0] * v[2] - 1e6 * v[1]; });
    add("osmotic", "Osmotic pressure", "Solutions", "\xCE\xA0 = iMRT",
        { V("Pi", "osmotic pressure", D::Pressure, "atm"), V("i", "van 't Hoff factor", D::Dimensionless, ""), V("M", "molarity", D::Concentration, "M"), V("T", "temperature", D::Temperature, "K") },
        [](Vec v) { return v[0] - v[1] * v[2] * R * v[3]; });
    add("boiling_elevation", "Boiling point elevation", "Solutions", "\xCE\x94T_b = iK_b b",
        { V("dT", "boiling point rise", D::TemperatureDifference, "K"), V("i", "van 't Hoff factor", D::Dimensionless, ""), V("Kb", "K_b", D::Dimensionless, "", true, "K\xC2\xB7kg/mol"), V("b", "molality", D::Molality, "mol/kg") },
        [](Vec v) { return v[0] - v[1] * v[2] * v[3]; }, "water: K_b = 0.512 K\xC2\xB7kg/mol");
    add("freezing_depression", "Freezing point depression", "Solutions", "\xCE\x94T_f = iK_f b",
        { V("dT", "freezing point drop", D::TemperatureDifference, "K"), V("i", "van 't Hoff factor", D::Dimensionless, ""), V("Kf", "K_f", D::Dimensionless, "", true, "K\xC2\xB7kg/mol"), V("b", "molality", D::Molality, "mol/kg") },
        [](Vec v) { return v[0] - v[1] * v[2] * v[3]; }, "water: K_f = 1.86 K\xC2\xB7kg/mol");
    add("raoult", "Raoult's law", "Solutions", "P = x P\xC2\xB0",
        { V("P", "vapor pressure of solution", D::Pressure, "torr"), V("x", "mole fraction of solvent", D::Dimensionless, ""), V("P0", "vapor pressure of pure solvent", D::Pressure, "torr") },
        [](Vec v) { return v[0] - v[1] * v[2]; });
    add("henry", "Henry's law", "Solutions", "C = k_H P",
        { V("C", "dissolved concentration", D::Concentration, "M"), V("kH", "Henry constant", D::Dimensionless, "", true, "M/atm"), V("P", "gas pressure", D::Pressure, "atm") },
        [](Vec v) { return v[0] / 1000.0 - v[1] * v[2] / ATMOSPHERE; });

    add("ph", "pH", "Acids and bases", "pH = -log[H\xE2\x82\x83O\xE2\x81\xBA]",
        { V("pH", "pH", D::Dimensionless, "", false), V("H", "[H\xE2\x82\x83O\xE2\x81\xBA]", D::Concentration, "M") },
        [](Vec v) { return v[0] + std::log10(v[1] / 1000.0); });
    add("poh", "pOH", "Acids and bases", "pOH = -log[OH\xE2\x81\xBB]",
        { V("pOH", "pOH", D::Dimensionless, "", false), V("OH", "[OH\xE2\x81\xBB]", D::Concentration, "M") },
        [](Vec v) { return v[0] + std::log10(v[1] / 1000.0); });
    add("ph_poh", "pH + pOH", "Acids and bases", "pH + pOH = pK_w",
        { V("pH", "pH", D::Dimensionless, "", false), V("pOH", "pOH", D::Dimensionless, "", false), V("pKw", "pK_w", D::Dimensionless, "") },
        [](Vec v) { return v[0] + v[1] - v[2]; }, "pK_w = 14.00 at 25 \xC2\xB0" "C");
    add("weak_acid", "Weak acid", "Acids and bases", "K_a = [H\xE2\x81\xBA]\xC2\xB2/(C - [H\xE2\x81\xBA])",
        { V("Ka", "K_a", D::Dimensionless, ""), V("C", "acid concentration", D::Concentration, "M"), V("H", "[H\xE2\x82\x83O\xE2\x81\xBA]", D::Concentration, "M") },
        [](Vec v) {
            const double c = v[1] / 1000.0, h = v[2] / 1000.0;
            return v[0] * (c - h) - h * h;
        });
    add("weak_base", "Weak base", "Acids and bases", "K_b = [OH\xE2\x81\xBB]\xC2\xB2/(C - [OH\xE2\x81\xBB])",
        { V("Kb", "K_b", D::Dimensionless, ""), V("C", "base concentration", D::Concentration, "M"), V("OH", "[OH\xE2\x81\xBB]", D::Concentration, "M") },
        [](Vec v) {
            const double c = v[1] / 1000.0, h = v[2] / 1000.0;
            return v[0] * (c - h) - h * h;
        });
    add("henderson", "Henderson-Hasselbalch", "Acids and bases", "pH = pK_a + log([A\xE2\x81\xBB]/[HA])",
        { V("pH", "pH", D::Dimensionless, "", false), V("pKa", "pK_a", D::Dimensionless, "", false), V("A", "[A\xE2\x81\xBB]", D::Concentration, "M"), V("HA", "[HA]", D::Concentration, "M") },
        [](Vec v) { return v[0] - v[1] - std::log10(v[2] / v[3]); });
    add("pka", "pK_a and K_a", "Acids and bases", "pK_a = -log K_a",
        { V("pKa", "pK_a", D::Dimensionless, "", false), V("Ka", "K_a", D::Dimensionless, "") },
        [](Vec v) { return v[0] + std::log10(v[1]); });
    add("titration", "Titration", "Acids and bases", "n\xE2\x82\x90M\xE2\x82\x90V\xE2\x82\x90 = n_bM_bV_b",
        { V("Ma", "acid molarity", D::Concentration, "M"), V("Va", "acid volume", D::Volume, "mL"), V("na", "acidic protons per acid", D::Dimensionless, ""),
          V("Mb", "base molarity", D::Concentration, "M"), V("Vb", "base volume", D::Volume, "mL"), V("nb", "hydroxides per base", D::Dimensionless, "") },
        [](Vec v) { return v[2] * v[0] * v[1] - v[5] * v[3] * v[4]; });

    add("calorimetry", "Heat (q = mc\xCE\x94T)", "Thermochemistry", "q = mc\xCE\x94T",
        { V("q", "heat", D::Energy, "J", false), V("m", "mass", D::Mass, "g"), V("c", "specific heat", D::SpecificHeat, "J/(g\xC2\xB7K)"), V("dT", "temperature change", D::TemperatureDifference, "K", false) },
        [](Vec v) { return v[0] - v[1] * v[2] * v[3]; }, "water: c = 4.184 J/(g\xC2\xB7K)");
    add("phase_heat", "Heat of a phase change", "Thermochemistry", "q = n\xCE\x94H",
        { V("q", "heat", D::Energy, "kJ", false), V("n", "amount", D::Amount, "mol"), V("dH", "enthalpy of the change", D::MolarEnergy, "kJ/mol", false) },
        [](Vec v) { return v[0] - v[1] * v[2]; }, "water: \xCE\x94H_fus = 6.01 kJ/mol, \xCE\x94H_vap = 40.7 kJ/mol");
    add("gibbs", "Gibbs energy", "Thermochemistry", "\xCE\x94G = \xCE\x94H - T\xCE\x94S",
        { V("dG", "\xCE\x94G", D::MolarEnergy, "kJ/mol", false), V("dH", "\xCE\x94H", D::MolarEnergy, "kJ/mol", false), V("T", "temperature", D::Temperature, "K"), V("dS", "\xCE\x94S", D::MolarEntropy, "J/(mol\xC2\xB7K)", false) },
        [](Vec v) { return v[0] - v[1] + v[2] * v[3]; });
    add("gibbs_k", "\xCE\x94G\xC2\xB0 and K", "Thermochemistry", "\xCE\x94G\xC2\xB0 = -RT ln K",
        { V("dG", "\xCE\x94G\xC2\xB0", D::MolarEnergy, "kJ/mol", false), V("T", "temperature", D::Temperature, "K"), V("K", "equilibrium constant", D::Dimensionless, "") },
        [](Vec v) { return v[0] + R * v[1] * ln(v[2]); });
    add("gibbs_q", "\xCE\x94G away from standard", "Thermochemistry", "\xCE\x94G = \xCE\x94G\xC2\xB0 + RT ln Q",
        { V("dG", "\xCE\x94G", D::MolarEnergy, "kJ/mol", false), V("dG0", "\xCE\x94G\xC2\xB0", D::MolarEnergy, "kJ/mol", false), V("T", "temperature", D::Temperature, "K"), V("Q", "reaction quotient", D::Dimensionless, "") },
        [](Vec v) { return v[0] - v[1] - R * v[2] * ln(v[3]); });
    add("vant_hoff", "Van 't Hoff equation", "Thermochemistry", "ln(K\xE2\x82\x82/K\xE2\x82\x81) = -\xCE\x94H/R (1/T\xE2\x82\x82 - 1/T\xE2\x82\x81)",
        { V("K1", "K at T\xE2\x82\x81", D::Dimensionless, ""), V("T1", "T\xE2\x82\x81", D::Temperature, "K"), V("K2", "K at T\xE2\x82\x82", D::Dimensionless, ""), V("T2", "T\xE2\x82\x82", D::Temperature, "K"), V("dH", "\xCE\x94H", D::MolarEnergy, "kJ/mol", false) },
        [](Vec v) { return ln(v[2] / v[0]) + v[4] / R * (1.0 / v[3] - 1.0 / v[1]); });
    add("clausius", "Clausius-Clapeyron", "Thermochemistry", "ln(P\xE2\x82\x82/P\xE2\x82\x81) = -\xCE\x94H_vap/R (1/T\xE2\x82\x82 - 1/T\xE2\x82\x81)",
        { V("P1", "vapor pressure at T\xE2\x82\x81", D::Pressure, "torr"), V("T1", "T\xE2\x82\x81", D::Temperature, "K"), V("P2", "vapor pressure at T\xE2\x82\x82", D::Pressure, "torr"), V("T2", "T\xE2\x82\x82", D::Temperature, "K"), V("dH", "\xCE\x94H_vap", D::MolarEnergy, "kJ/mol") },
        [](Vec v) { return ln(v[2] / v[0]) + v[4] / R * (1.0 / v[3] - 1.0 / v[1]); });

    add("arrhenius", "Arrhenius equation", "Kinetics", "k = A e^(-E\xE2\x82\x90/RT)",
        { V("k", "rate constant", D::RateConstant, "1/s"), V("A", "pre-exponential factor", D::RateConstant, "1/s"), V("Ea", "activation energy", D::MolarEnergy, "kJ/mol"), V("T", "temperature", D::Temperature, "K") },
        [](Vec v) { return ln(v[0]) - ln(v[1]) + v[2] / (R * v[3]); });
    add("arrhenius_two", "Arrhenius (two temperatures)", "Kinetics", "ln(k\xE2\x82\x82/k\xE2\x82\x81) = E\xE2\x82\x90/R (1/T\xE2\x82\x81 - 1/T\xE2\x82\x82)",
        { V("k1", "k at T\xE2\x82\x81", D::RateConstant, "1/s"), V("T1", "T\xE2\x82\x81", D::Temperature, "K"), V("k2", "k at T\xE2\x82\x82", D::RateConstant, "1/s"), V("T2", "T\xE2\x82\x82", D::Temperature, "K"), V("Ea", "activation energy", D::MolarEnergy, "kJ/mol") },
        [](Vec v) { return ln(v[2] / v[0]) - v[4] / R * (1.0 / v[1] - 1.0 / v[3]); });
    add("first_order", "First-order kinetics", "Kinetics", "ln([A]\xE2\x82\x80/[A]) = kt",
        { V("A0", "starting concentration", D::Concentration, "M"), V("A", "concentration at t", D::Concentration, "M"), V("k", "rate constant", D::RateConstant, "1/s"), V("t", "time", D::Time, "s") },
        [](Vec v) { return ln(v[0] / v[1]) - v[2] * v[3]; });
    add("second_order", "Second-order kinetics", "Kinetics", "1/[A] - 1/[A]\xE2\x82\x80 = kt",
        { V("A0", "starting concentration", D::Concentration, "M"), V("A", "concentration at t", D::Concentration, "M"), V("k", "rate constant (1/(M\xC2\xB7s))", D::Dimensionless, "", true, "1/(M\xC2\xB7s)"), V("t", "time", D::Time, "s") },
        [](Vec v) { return 1000.0 / v[1] - 1000.0 / v[0] - v[2] * v[3]; });
    add("zero_order", "Zero-order kinetics", "Kinetics", "[A]\xE2\x82\x80 - [A] = kt",
        { V("A0", "starting concentration", D::Concentration, "M"), V("A", "concentration at t", D::Concentration, "M"), V("k", "rate constant (M/s)", D::Dimensionless, "", true, "M/s"), V("t", "time", D::Time, "s") },
        [](Vec v) { return (v[0] - v[1]) / 1000.0 - v[2] * v[3]; });
    add("half_life", "First-order half-life", "Kinetics", "t\xC2\xBD = ln 2/k",
        { V("t", "half-life", D::Time, "s"), V("k", "rate constant", D::RateConstant, "1/s") },
        [](Vec v) { return v[0] * v[1] - LN2; });
    add("decay", "Radioactive decay", "Kinetics", "N = N\xE2\x82\x80(1/2)^(t/t\xC2\xBD)",
        { V("N", "amount left", D::Mass, "g"), V("N0", "starting amount", D::Mass, "g"), V("t", "elapsed time", D::Time, "yr"), V("th", "half-life", D::Time, "yr") },
        [](Vec v) { return ln(v[0] / v[1]) + LN2 * v[2] / v[3]; });

    add("cell_potential", "Standard cell potential", "Electrochemistry", "E\xC2\xB0_cell = E\xC2\xB0_cathode - E\xC2\xB0_anode",
        { V("E", "E\xC2\xB0 cell", D::Potential, "V", false), V("Ec", "E\xC2\xB0 cathode", D::Potential, "V", false), V("Ea", "E\xC2\xB0 anode", D::Potential, "V", false) },
        [](Vec v) { return v[0] - v[1] + v[2]; });
    add("nernst", "Nernst equation", "Electrochemistry", "E = E\xC2\xB0 - (RT/nF) ln Q",
        { V("E", "cell potential", D::Potential, "V", false), V("E0", "standard potential", D::Potential, "V", false), V("T", "temperature", D::Temperature, "K"), V("n", "electrons transferred", D::Dimensionless, ""), V("Q", "reaction quotient", D::Dimensionless, "") },
        [](Vec v) { return v[0] - v[1] + R * v[2] / (v[3] * F) * ln(v[4]); });
    add("gibbs_cell", "\xCE\x94G from a cell", "Electrochemistry", "\xCE\x94G = -nFE",
        { V("dG", "\xCE\x94G", D::MolarEnergy, "kJ/mol", false), V("n", "electrons transferred", D::Dimensionless, ""), V("E", "cell potential", D::Potential, "V", false) },
        [](Vec v) { return v[0] + v[1] * F * v[2]; });
    add("faraday", "Electrolysis (Faraday)", "Electrochemistry", "m = ItM/(nF)",
        { V("m", "mass deposited", D::Mass, "g"), V("I", "current", D::Current, "A"), V("t", "time", D::Time, "s"), V("M", "molar mass", D::MolarMass, "g/mol"), V("n", "electrons per ion", D::Dimensionless, "") },
        [](Vec v) { return v[0] * v[4] * F - v[1] * v[2] * v[3] / 1000.0; });
    add("cell_k", "E\xC2\xB0 and K", "Electrochemistry", "E\xC2\xB0 = (RT/nF) ln K",
        { V("E", "E\xC2\xB0", D::Potential, "V", false), V("T", "temperature", D::Temperature, "K"), V("n", "electrons transferred", D::Dimensionless, ""), V("K", "equilibrium constant", D::Dimensionless, "") },
        [](Vec v) { return v[0] - R * v[1] / (v[2] * F) * ln(v[3]); });

    add("photon_wavelength", "Photon energy", "Light and atoms", "E = hc/\xCE\xBB",
        { V("E", "energy per photon", D::Energy, "eV"), V("lambda", "wavelength", D::Length, "nm") },
        [](Vec v) { return v[0] * v[1] - H * C; });
    add("photon_frequency", "Photon energy and frequency", "Light and atoms", "E = h\xCE\xBD",
        { V("E", "energy per photon", D::Energy, "eV"), V("nu", "frequency", D::Frequency, "THz") },
        [](Vec v) { return v[0] - H * v[1]; });
    add("wave", "Wavelength and frequency", "Light and atoms", "c = \xCE\xBB\xCE\xBD",
        { V("lambda", "wavelength", D::Length, "nm"), V("nu", "frequency", D::Frequency, "THz") },
        [](Vec v) { return v[0] * v[1] - C; });
    add("de_broglie", "De Broglie wavelength", "Light and atoms", "\xCE\xBB = h/(mv)",
        { V("lambda", "wavelength", D::Length, "pm"), V("m", "mass", D::Mass, "u"), V("v", "speed (m/s)", D::Dimensionless, "", true, "m/s") },
        [](Vec v) { return v[0] * v[1] * v[2] - H; });
    add("rydberg", "Hydrogen spectrum (Rydberg)", "Light and atoms", "1/\xCE\xBB = R_H(1/n\xE2\x82\x81\xC2\xB2 - 1/n\xE2\x82\x82\xC2\xB2)",
        { V("lambda", "wavelength", D::Length, "nm"), V("n1", "lower level", D::Dimensionless, ""), V("n2", "upper level", D::Dimensionless, "") },
        [](Vec v) { return 1.0 / v[0] - RYDBERG_CONSTANT / (1.0 + ELECTRON_MASS / PROTON_MASS) * (1.0 / (v[1] * v[1]) - 1.0 / (v[2] * v[2])); },
        "Balmer lines have n\xE2\x82\x81 = 2");
    return e;
}

double brent(const std::function<double(double)>& f, double a, double b, double fa, double fb) {
    double c = a, fc = fa, d = b - a, e = d;
    for (int iter = 0; iter < 200; ++iter) {
        if ((fb > 0 && fc > 0) || (fb < 0 && fc < 0)) {
            c = a;
            fc = fa;
            d = e = b - a;
        }
        if (std::fabs(fc) < std::fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        const double tol = 2.0 * 1e-15 * std::fabs(b) + 1e-300;
        const double m = 0.5 * (c - b);
        if (std::fabs(m) <= tol || fb == 0.0) return b;
        if (std::fabs(e) >= tol && std::fabs(fa) > std::fabs(fb)) {
            double p, q, r;
            const double s = fb / fa;
            if (a == c) {
                p = 2.0 * m * s;
                q = 1.0 - s;
            } else {
                q = fa / fc;
                r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = std::fabs(p);
            if (2.0 * p < std::min(3.0 * m * q - std::fabs(tol * q), std::fabs(e * q))) {
                e = d;
                d = p / q;
            } else {
                d = m;
                e = d;
            }
        } else {
            d = m;
            e = d;
        }
        a = b;
        fa = fb;
        b += std::fabs(d) > tol ? d : (m > 0 ? tol : -tol);
        fb = f(b);
    }
    return b;
}

} // namespace

int ChemistryEquation::index_of(std::string_view variable_id) const noexcept {
    for (std::size_t i = 0; i < variables.size(); ++i) {
        if (variables[i].id == variable_id) return static_cast<int>(i);
    }
    return -1;
}

const std::vector<ChemistryEquation>& chemistry_equations() {
    static const std::vector<ChemistryEquation> list = build_equations();
    return list;
}

const ChemistryEquation* find_equation(std::string_view id) {
    for (const ChemistryEquation& e : chemistry_equations()) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::vector<std::string> equation_categories() {
    std::vector<std::string> out;
    for (const ChemistryEquation& e : chemistry_equations()) {
        if (std::find(out.begin(), out.end(), e.category) == out.end()) out.push_back(e.category);
    }
    return out;
}

EquationSolution solve_equation(const ChemistryEquation& eq, const std::vector<double>& si, int unknown) {
    EquationSolution out;
    if (si.size() != eq.variables.size()) {
        out.message = "expected " + std::to_string(eq.variables.size()) + " values";
        return out;
    }
    if (unknown < 0) {
        int missing = 0;
        for (std::size_t i = 0; i < si.size(); ++i) {
            if (!(si[i] == si[i])) {
                unknown = static_cast<int>(i);
                ++missing;
            }
        }
        if (missing != 1) {
            out.message = missing == 0 ? "leave exactly one box empty to solve for it" : "fill in all but one box (" + std::to_string(missing) + " are empty)";
            return out;
        }
    }
    out.unknown = unknown;
    std::vector<double> vals = si;
    const auto f = [&](double x) {
        vals[static_cast<std::size_t>(unknown)] = x;
        return eq.residual(vals);
    };
    const bool positive = eq.variables[static_cast<std::size_t>(unknown)].positive;
    std::vector<double> grid;
    for (int k = -40; k <= 40; ++k) {
        for (double m : { 1.0, 2.0, 5.0 }) grid.push_back(m * std::pow(10.0, k));
    }
    std::vector<double> xs;
    if (!positive) {
        for (auto it = grid.rbegin(); it != grid.rend(); ++it) xs.push_back(-*it);
        xs.push_back(0.0);
    }
    xs.insert(xs.end(), grid.begin(), grid.end());
    double best_x = NO_VALUE;
    double prev_x = NO_VALUE, prev_f = NO_VALUE;
    double best_gap = 1e300;
    for (double x : xs) {
        const double fx = f(x);
        if (!(fx == fx) || std::isinf(fx)) {
            prev_x = NO_VALUE;
            continue;
        }
        if (fx == 0.0) {
            best_x = x;
            break;
        }
        if (prev_x == prev_x && ((prev_f < 0) != (fx < 0))) {
            const double root = brent(f, prev_x, x, prev_f, fx);
            const double fr = f(root);
            const double scale = std::max(std::fabs(prev_f), std::fabs(fx));
            if (fr == fr && std::fabs(fr) <= 1e-6 * scale + 1e-300) {
                const double gap = std::fabs(fr);
                if (!(best_x == best_x) || gap < best_gap) {
                    best_x = root;
                    best_gap = gap;
                }
                if (positive) break;
            }
        }
        prev_x = x;
        prev_f = fx;
    }
    if (!(best_x == best_x)) {
        out.message = "no value of " + eq.variables[static_cast<std::size_t>(unknown)].label + " satisfies the equation with these numbers";
        return out;
    }
    vals[static_cast<std::size_t>(unknown)] = best_x;
    out.ok = true;
    out.value = best_x;
    out.values = vals;
    return out;
}

const char* to_string(AmountKind k) noexcept {
    switch (k) {
        case AmountKind::Mass: return "mass";
        case AmountKind::Moles: return "moles";
        case AmountKind::Particles: return "particles";
        case AmountKind::GasVolume: return "gas volume";
        default: return "solution volume";
    }
}

double amount_to_moles(AmountKind kind, double v, double molar_mass, double t, double p, double molarity) {
    switch (kind) {
        case AmountKind::Mass: return v * 1000.0 / molar_mass;
        case AmountKind::Moles: return v;
        case AmountKind::Particles: return v / AVOGADRO_CONSTANT;
        case AmountKind::GasVolume: return p * v / (R * t);
        default: return v * molarity;
    }
}

double moles_to_amount(AmountKind kind, double n, double molar_mass, double t, double p, double molarity) {
    switch (kind) {
        case AmountKind::Mass: return n * molar_mass / 1000.0;
        case AmountKind::Moles: return n;
        case AmountKind::Particles: return n * AVOGADRO_CONSTANT;
        case AmountKind::GasVolume: return n * R * t / p;
        default: return n / molarity;
    }
}

StoichiometryResult solve_stoichiometry(const Reaction& input, const std::vector<StoichiometryKnown>& knowns, double t, double p) {
    StoichiometryResult out;
    Reaction r = input;
    try {
        if (!r.is_balanced()) r.balance();
    } catch (const std::exception& ex) {
        out.message = std::string("couldn't balance the reaction: ") + ex.what();
        return out;
    }
    out.reaction = r;
    const std::size_t nr = r.reactants().size();
    for (std::size_t i = 0; i < nr + r.products().size(); ++i) {
        const ReactionTerm& term = i < nr ? r.reactants()[i] : r.products()[i - nr];
        StoichiometrySpecies s;
        s.formula = term.formula.display_string(false);
        s.reactant = i < nr;
        s.coefficient = term.coefficient;
        s.molar_mass = term.formula.molar_mass();
        s.gas = term.formula.phase() == Phase::Gas;
        out.species.push_back(s);
    }
    if (knowns.empty()) {
        out.message = "give an amount for at least one substance";
        return out;
    }
    for (const StoichiometryKnown& k : knowns) {
        if (k.species >= out.species.size()) continue;
        StoichiometrySpecies& s = out.species[k.species];
        s.given_moles = amount_to_moles(k.kind, k.value, s.molar_mass, t, p, k.molarity);
        out.steps.push_back(s.formula + ": " + format_number(s.given_moles, 5) + " mol (from " + to_string(k.kind) + ")");
    }
    double extent = 1e300;
    int limiting = -1;
    for (std::size_t i = 0; i < nr; ++i) {
        const StoichiometrySpecies& s = out.species[i];
        if (!(s.given_moles == s.given_moles)) continue;
        const double x = s.given_moles / s.coefficient;
        if (x < extent) {
            extent = x;
            limiting = static_cast<int>(i);
        }
    }
    if (limiting < 0) {
        for (std::size_t i = nr; i < out.species.size(); ++i) {
            const StoichiometrySpecies& s = out.species[i];
            if (!(s.given_moles == s.given_moles)) continue;
            extent = s.given_moles / s.coefficient;
            out.steps.push_back("working backward from " + s.formula);
            break;
        }
    } else {
        out.species[static_cast<std::size_t>(limiting)].limiting = true;
        int known_reactants = 0;
        for (std::size_t i = 0; i < nr; ++i) known_reactants += out.species[i].given_moles == out.species[i].given_moles;
        if (known_reactants > 1) out.steps.push_back(out.species[static_cast<std::size_t>(limiting)].formula + " runs out first, so it's the limiting reactant");
    }
    if (!(extent < 1e299)) {
        out.message = "no usable amounts";
        return out;
    }
    out.extent = extent;
    out.limiting = limiting;
    out.steps.push_back("reaction extent \xCE\xBE = " + format_number(extent, 5) + " mol");
    for (std::size_t i = 0; i < out.species.size(); ++i) {
        StoichiometrySpecies& s = out.species[i];
        s.reacted_moles = s.coefficient * extent;
        if (s.reactant) s.final_moles = s.given_moles == s.given_moles ? s.given_moles - s.reacted_moles : 0.0;
        else s.final_moles = (s.given_moles == s.given_moles && limiting >= 0 ? s.given_moles : 0.0) + (limiting >= 0 || !(s.given_moles == s.given_moles) ? s.reacted_moles : 0.0);
        if (s.final_moles < 0 && s.final_moles > -1e-12) s.final_moles = 0.0;
    }
    out.ok = true;
    return out;
}

double translational_entropy(double mass_u, double t, double p) noexcept {
    const double m = mass_u * ATOMIC_MASS_CONSTANT;
    const double lambda3 = std::pow(2.0 * PI * m * BOLTZMANN_CONSTANT * t / (PLANCK_CONSTANT * PLANCK_CONSTANT), 1.5);
    return R * (std::log(lambda3 * BOLTZMANN_CONSTANT * t / p) + 2.5);
}

double rotational_entropy_linear(double moment_of_inertia, double t, int symmetry) noexcept {
    const double theta = PLANCK_CONSTANT * PLANCK_CONSTANT / (8.0 * PI * PI * moment_of_inertia * BOLTZMANN_CONSTANT);
    return R * (1.0 + std::log(t / (symmetry * theta)));
}

BondThermo bond_dissociation(const Element& a, const Element& b, BondOrder order, double t, double p, double length_pm, double energy) {
    BondThermo out;
    if (!(energy == energy)) energy = bond_energy(a, b, order);
    if (!(length_pm == length_pm)) length_pm = bond_length(a, b, order);
    out.enthalpy_298 = energy;
    out.length = length_pm;
    if (!(energy == energy)) return out;
    const auto thermo = [&](double temp, double& dh, double& ds) {
        dh = energy + 1.5 * R * (temp - STANDARD_TEMPERATURE) / 1000.0;
        const double ma = a.atomic_weight, mb = b.atomic_weight;
        const double mu = ma * mb / (ma + mb) * ATOMIC_MASS_CONSTANT;
        const double r = (length_pm == length_pm ? length_pm : 150.0) * PICOMETER;
        const double inertia = mu * r * r;
        const int sym = a.atomic_number == b.atomic_number ? 2 : 1;
        const double ga = a.ground_term.twice_j >= 0 ? a.ground_term.twice_j + 1.0 : 1.0;
        const double gb = b.ground_term.twice_j >= 0 ? b.ground_term.twice_j + 1.0 : 1.0;
        ds = translational_entropy(ma, temp) + translational_entropy(mb, temp) - translational_entropy(ma + mb, temp) - rotational_entropy_linear(inertia, temp, sym) + R * std::log(ga * gb);
    };
    double dh = 0.0, ds = 0.0;
    thermo(t, dh, ds);
    out.enthalpy = dh;
    out.entropy = ds;
    out.gibbs = dh - t * ds / 1000.0;
    out.equilibrium_constant = std::exp(-out.gibbs * 1000.0 / (R * t));
    const double ratio = p / STANDARD_PRESSURE;
    const double K = out.equilibrium_constant;
    out.fraction_broken = std::sqrt(K / (K + ratio));
    double lo = 50.0, hi = 60000.0;
    const auto g = [&](double temp) {
        double h2, s2;
        thermo(temp, h2, s2);
        return h2 * 1000.0 - temp * s2 + R * temp * std::log(ratio / 3.0);
    };
    if (g(lo) > 0 && g(hi) < 0) {
        for (int i = 0; i < 100; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (g(mid) > 0) lo = mid;
            else hi = mid;
        }
        out.half_broken_temperature = 0.5 * (lo + hi);
    }
    return out;
}

std::vector<PercentComposition> percent_composition(const Formula& formula) {
    std::vector<PercentComposition> out;
    const double total = formula.molar_mass();
    for (const auto& [e, n] : formula.composition()) {
        PercentComposition pc;
        pc.element = e;
        pc.count = n;
        pc.mass = n * e->atomic_weight;
        pc.percent = 100.0 * pc.mass / total;
        out.push_back(pc);
    }
    return out;
}

Formula empirical_formula_from_masses(const std::vector<std::pair<const Element*, double>>& masses) {
    std::vector<std::pair<const Element*, double>> moles;
    double smallest = 1e300;
    for (const auto& [e, m] : masses) {
        if (!e || !(m > 0)) continue;
        const double n = m / e->atomic_weight;
        moles.emplace_back(e, n);
        smallest = std::min(smallest, n);
    }
    Formula out;
    if (moles.empty()) return out;
    for (int mult = 1; mult <= 12; ++mult) {
        bool good = true;
        for (const auto& [e, n] : moles) {
            const double r = n / smallest * mult;
            if (std::fabs(r - std::round(r)) > 0.08 * mult) good = false;
        }
        if (good || mult == 12) {
            std::string text;
            for (const auto& [e, n] : moles) {
                const long c = std::lround(n / smallest * mult);
                text += std::string(e->symbol) + (c > 1 ? std::to_string(c) : std::string());
            }
            return Formula::parse(text);
        }
    }
    return out;
}

Formula molecular_formula(const Formula& empirical, double molar_mass) {
    const double k = molar_mass / empirical.molar_mass();
    const double m = std::max(1.0, std::round(k));
    std::string text;
    for (const auto& [e, n] : empirical.composition()) {
        const long c = std::lround(n * m);
        text += std::string(e->symbol) + (c > 1 ? std::to_string(c) : std::string());
    }
    return Formula::parse(text);
}

} // namespace chemistry
} // namespace fizmo
