#ifndef FIZMO_CHEMISTRY_GAS_KINETICS_HPP
#define FIZMO_CHEMISTRY_GAS_KINETICS_HPP

#include "common.hpp"
#include "constants.hpp"
#include "substance_types.hpp"

#include <vector>

namespace fizmo {
namespace chemistry {

double rms_speed(double t, double molar_mass_g) noexcept;
double mean_speed(double t, double molar_mass_g) noexcept;
double most_probable_speed(double t, double molar_mass_g) noexcept;
double maxwell_boltzmann(double speed, double t, double molar_mass_g) noexcept;
double maxwell_boltzmann_fraction_above(double speed, double t, double molar_mass_g) noexcept;
double maxwell_boltzmann_energy(double energy_j, double t) noexcept;
double average_kinetic_energy(double t) noexcept;
double molar_kinetic_energy(double t) noexcept;
double number_density(double p, double t) noexcept;
double mean_free_path(double p, double t, double diameter_m) noexcept;
double collision_frequency(double p, double t, double diameter_m, double molar_mass_g) noexcept;
double wall_collision_flux(double p, double t, double molar_mass_g) noexcept;
double effusion_rate_ratio(double molar_mass_a, double molar_mass_b) noexcept;
double graham_molar_mass(double known_molar_mass, double rate_known, double rate_unknown) noexcept;
double partial_pressure(double mole_fraction, double total_pressure) noexcept;
std::vector<double> partial_pressures(const std::vector<double>& moles, double total_pressure);
double average_molar_mass(const std::vector<double>& mole_fractions, const std::vector<double>& molar_masses);
double barometric_pressure(double p0, double height, double t, double molar_mass_g, double g = GRAVITATIONAL_ACCELERATION) noexcept;

double collision_integral_viscosity(double reduced_temperature) noexcept;
double collision_integral_diffusion(double reduced_temperature) noexcept;
double chapman_enskog_viscosity(double t, double molar_mass_g, double sigma_angstrom, double epsilon_k) noexcept;
double chapman_enskog_diffusion(double t, double p, double molar_mass_a, double molar_mass_b, double sigma_a, double sigma_b, double epsilon_a, double epsilon_b) noexcept;
double eucken_thermal_conductivity(double viscosity, double cv_molar, double molar_mass_g) noexcept;
double gas_viscosity(const Substance& s, double t) noexcept;
double gas_diffusion(const Substance& a, const Substance& b, double t, double p) noexcept;
double gas_thermal_conductivity(const Substance& s, double t) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_GAS_KINETICS_HPP
