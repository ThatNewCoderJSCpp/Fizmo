#ifndef FIZMO_CHEMISTRY_SPECTROSCOPY_HPP
#define FIZMO_CHEMISTRY_SPECTROSCOPY_HPP

#include "common.hpp"
#include "constants.hpp"

namespace fizmo {
namespace chemistry {

double photon_energy_from_wavelength(double wavelength_m) noexcept;
double photon_energy_from_frequency(double frequency_hz) noexcept;
double photon_wavelength(double energy_j) noexcept;
double photon_frequency(double energy_j) noexcept;
double photon_momentum(double wavelength_m) noexcept;
double de_broglie_wavelength(double mass_kg, double speed) noexcept;
double hydrogen_like_energy(int atomic_number, int n, double nuclear_mass_u = INFINITE_TIME) noexcept;
double hydrogen_like_radius(int atomic_number, int n, double nuclear_mass_u = INFINITE_TIME) noexcept;
double rydberg_wavelength(int atomic_number, int n_lower, int n_upper, double nuclear_mass_u = INFINITE_TIME) noexcept;
double reduced_rydberg(double nuclear_mass_u) noexcept;
double beer_lambert_absorbance(double molar_absorptivity, double path_cm, double concentration) noexcept;
double transmittance(double absorbance) noexcept;
double absorbance_from_transmittance(double transmittance_value) noexcept;
double wien_peak_wavelength(double t) noexcept;
double planck_spectral_radiance(double wavelength_m, double t) noexcept;
double rotational_constant(double reduced_mass_u, double bond_length_m) noexcept;
double rotational_energy(int j, double rotational_constant_per_cm) noexcept;
double vibrational_energy(int v, double wavenumber_per_cm, double anharmonicity = 0.0) noexcept;
double boltzmann_population_ratio(double energy_gap_j, double t, double degeneracy_ratio = 1.0) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SPECTROSCOPY_HPP
