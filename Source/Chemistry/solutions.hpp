#ifndef FIZMO_CHEMISTRY_SOLUTIONS_HPP
#define FIZMO_CHEMISTRY_SOLUTIONS_HPP

#include "common.hpp"
#include "constants.hpp"

#include <vector>

namespace fizmo {
namespace chemistry {

inline constexpr double WATER_EBULLIOSCOPIC_CONSTANT = 0.512;
inline constexpr double WATER_CRYOSCOPIC_CONSTANT = 1.86;

struct IonConcentration {
    double concentration = 0.0;
    int charge = 0;
};

double molarity(double moles, double liters) noexcept;
double molality(double moles_solute, double kg_solvent) noexcept;
double mole_fraction(double moles, double total_moles) noexcept;
std::vector<double> mole_fractions(const std::vector<double>& moles);
double mass_percent(double solute_g, double solution_g) noexcept;
double parts_per_million(double solute_g, double solution_g) noexcept;
double molarity_to_molality(double molarity_value, double solution_density_g_per_ml, double solute_molar_mass) noexcept;
double molality_to_molarity(double molality_value, double solution_density_g_per_ml, double solute_molar_mass) noexcept;
double molality_to_mole_fraction(double molality_value, double solvent_molar_mass) noexcept;
double mole_fraction_to_molality(double x_solute, double solvent_molar_mass) noexcept;
double dilution_volume(double c1, double v1, double c2) noexcept;
double dilution_concentration(double c1, double v1, double v2) noexcept;
double mixed_concentration(const std::vector<double>& concentrations, const std::vector<double>& volumes);

double ebullioscopic_constant(double boiling_point, double vaporization_enthalpy, double solvent_molar_mass_g) noexcept;
double cryoscopic_constant(double melting_point, double fusion_enthalpy, double solvent_molar_mass_g) noexcept;
double boiling_point_elevation(double molality_value, double kb = WATER_EBULLIOSCOPIC_CONSTANT, double vant_hoff = 1.0) noexcept;
double freezing_point_depression(double molality_value, double kf = WATER_CRYOSCOPIC_CONSTANT, double vant_hoff = 1.0) noexcept;
double osmotic_pressure(double molarity_value, double t, double vant_hoff = 1.0) noexcept;
double vant_hoff_factor(int ions_per_formula, double degree_of_dissociation = 1.0) noexcept;
double molar_mass_from_freezing_point(double solute_g, double solvent_kg, double delta_t, double kf = WATER_CRYOSCOPIC_CONSTANT, double vant_hoff = 1.0) noexcept;
double raoult_pressure(double x_solvent, double pure_vapor_pressure) noexcept;
double raoult_total_pressure(const std::vector<double>& x, const std::vector<double>& pure_pressures);
std::vector<double> raoult_vapor_composition(const std::vector<double>& x, const std::vector<double>& pure_pressures);
double henry_concentration(double partial_pressure, double henry_constant) noexcept;
double henry_partial_pressure(double concentration, double henry_constant) noexcept;
double henry_temperature(double kh_ref, double t, double t_ref, double enthalpy_of_solution) noexcept;
double bubble_point_pressure(const std::vector<double>& x, const std::vector<double>& pure_pressures);
double dew_point_pressure(const std::vector<double>& y, const std::vector<double>& pure_pressures);

double ionic_strength(const std::vector<IonConcentration>& ions);
double debye_huckel_limiting(int charge, double ionic_strength_value, double a = 0.509) noexcept;
double debye_huckel_extended(int charge, double ionic_strength_value, double ion_size_pm, double a = 0.509, double b = 0.328) noexcept;
double davies(int charge, double ionic_strength_value, double a = 0.509) noexcept;
double activity(double concentration, double activity_coefficient) noexcept;
double solubility_from_ksp(double ksp, int cations, int anions) noexcept;
double ksp_from_solubility(double solubility, int cations, int anions) noexcept;
bool precipitates(double ion_product, double ksp) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SOLUTIONS_HPP
