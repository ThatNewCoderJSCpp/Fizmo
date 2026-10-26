#ifndef FIZMO_CHEMISTRY_MATERIALS_HPP
#define FIZMO_CHEMISTRY_MATERIALS_HPP

#include "crystal.hpp"
#include "formula.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct DielectricProperties {
    double relative_permittivity = NO_VALUE;
    double high_frequency_permittivity = NO_VALUE;
    double refractive_index = NO_VALUE;
    double susceptibility = NO_VALUE;
    double absolute_permittivity = NO_VALUE;
    double polarizability = NO_VALUE;
    double polarizability_volume = NO_VALUE;
    double molar_refraction = NO_VALUE;
    double dielectric_strength = NO_VALUE;
    double speed_of_light = NO_VALUE;
    double reflectance = NO_VALUE;
    double brewster_angle = NO_VALUE;
    double critical_angle = NO_VALUE;
    double optical_permittivity = NO_VALUE;
    double optical_penetration_depth = NO_VALUE;
    bool permittivity_estimated = false;
    bool refractive_index_estimated = false;
    bool conductor = false;
    std::string method;
    std::vector<std::string> notes;
};

struct MechanicalProperties {
    double youngs_modulus = NO_VALUE;
    double bulk_modulus = NO_VALUE;
    double shear_modulus = NO_VALUE;
    double poisson_ratio = NO_VALUE;
    double longitudinal_sound_speed = NO_VALUE;
    double transverse_sound_speed = NO_VALUE;
    double bar_sound_speed = NO_VALUE;
    double linear_expansion = NO_VALUE;
    double volume_expansion = NO_VALUE;
    double mohs_hardness = NO_VALUE;
    double gruneisen = NO_VALUE;
    double compressibility = NO_VALUE;
    double acoustic_impedance = NO_VALUE;
    bool estimated = false;
    std::vector<std::string> notes;
};

struct ElectronGasProperties {
    double fermi_energy = NO_VALUE;
    double fermi_temperature = NO_VALUE;
    double fermi_velocity = NO_VALUE;
    double fermi_wavevector = NO_VALUE;
    double density_of_states = NO_VALUE;
    double work_function = NO_VALUE;
    double photoelectric_threshold = NO_VALUE;
    double seebeck_coefficient = NO_VALUE;
    double richardson_current = NO_VALUE;
    double thomas_fermi_length = NO_VALUE;
    double pauli_susceptibility = NO_VALUE;
};

struct SusceptibilityInfo {
    double molar_susceptibility_cgs = NO_VALUE;
    double molar_susceptibility = NO_VALUE;
    double volume_susceptibility = NO_VALUE;
    double mass_susceptibility = NO_VALUE;
    double effective_moment = NO_VALUE;
    double curie_constant = NO_VALUE;
    int unpaired_electrons = 0;
    bool estimated = false;
    std::string method;
};

double tabulated_relative_permittivity(std::string_view name_or_formula) noexcept;
double tabulated_refractive_index(std::string_view name_or_formula) noexcept;
double tabulated_dielectric_strength(std::string_view name_or_formula) noexcept;
double work_function(int atomic_number) noexcept;
double tabulated_molar_susceptibility_cgs(int atomic_number) noexcept;
double ion_polarizability_volume(const Element& element, int charge) noexcept;
double molecular_polarizability_volume(const Formula& formula);
double spin_only_moment(int unpaired) noexcept;
double curie_molar_susceptibility(double effective_moment_bohr, double temperature) noexcept;
double clausius_mossotti_permittivity(double number_density, double polarizability_si) noexcept;
double onsager_permittivity(double number_density, double polarizability_si, double dipole_si, double temperature) noexcept;
double debye_gas_permittivity(double number_density, double polarizability_si, double dipole_si, double temperature) noexcept;
double polarizability_volume_to_si(double angstrom3) noexcept;
void fill_optics(DielectricProperties& d);

MechanicalProperties element_mechanics(const Element& element, double temperature = 298.15);
MechanicalProperties metal_mechanics(const MetallicCrystal& crystal, double temperature = 298.15);
MechanicalProperties ionic_mechanics(const IonicCrystal& crystal, double temperature = 298.15);

DielectricProperties element_dielectric(const Element& element, double temperature = 298.15, double pressure = 101325.0);
DielectricProperties metal_dielectric(const MetallicCrystal& crystal, double wavelength = 550e-9);
DielectricProperties ionic_dielectric(const IonicCrystal& crystal);
DielectricProperties molecular_dielectric(const std::string& name, const Formula& formula, double dipole_debye, Phase phase, double density_kg_m3, double temperature = 298.15, double pressure = 101325.0);

ElectronGasProperties electron_gas(double electron_density, double temperature = 298.15, double work_function_ev = NO_VALUE);
ElectronGasProperties element_electron_gas(const Element& element, double temperature = 298.15);
ElectronGasProperties metal_electron_gas(const MetallicCrystal& crystal, double temperature = 298.15);

SusceptibilityInfo element_susceptibility(const Element& element, double temperature = 298.15);
SusceptibilityInfo ionic_susceptibility(const IonicCrystal& crystal, double temperature = 298.15);
SusceptibilityInfo metal_susceptibility(const MetallicCrystal& crystal, double temperature = 298.15);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_MATERIALS_HPP
