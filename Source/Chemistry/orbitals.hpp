#ifndef FIZMO_CHEMISTRY_ORBITALS_HPP
#define FIZMO_CHEMISTRY_ORBITALS_HPP

#include "electron_configuration.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class OrbitalModel : std::uint8_t {
    Hydrogenic,
    Slater
};

std::string subshell_name(int n, int l);
std::string orbital_name(int n, int l, int m);
std::string orbital_shape_name(int l, int m);
int radial_node_count(int n, int l) noexcept;
int angular_node_count(int l) noexcept;
double slater_effective_n(int n) noexcept;

double effective_nuclear_charge(const Element& element, int n, int l, int charge = 0);
double radial_wavefunction(int n, int l, double z_eff, double r_bohr, OrbitalModel model = OrbitalModel::Hydrogenic);
double radial_probability(int n, int l, double z_eff, double r_bohr, OrbitalModel model = OrbitalModel::Hydrogenic);
double real_spherical_harmonic(int l, int m, double theta, double phi);
double wavefunction(int n, int l, int m, double z_eff, double x_bohr, double y_bohr, double z_bohr, OrbitalModel model = OrbitalModel::Hydrogenic);
double probability_density(int n, int l, int m, double z_eff, double x_bohr, double y_bohr, double z_bohr, OrbitalModel model = OrbitalModel::Hydrogenic);
double mean_radius_bohr(int n, int l, double z_eff) noexcept;
double most_probable_radius_bohr(int n, int l, double z_eff, OrbitalModel model = OrbitalModel::Hydrogenic);
double orbital_extent_bohr(int n, int l, double z_eff, double containment = 0.99, OrbitalModel model = OrbitalModel::Hydrogenic);
double orbital_energy_ev(int n, double z_eff) noexcept;

struct RadialCurve {
    int n = 0;
    int l = 0;
    int occupancy = 0;
    double z_eff = 0.0;
    std::string label;
    std::vector<double> r_pm;
    std::vector<double> probability;
    std::vector<double> cumulative;
    double most_probable_pm = 0.0;
    double mean_pm = 0.0;
    double radius_90_pm = 0.0;
    std::vector<double> node_radii_pm;
    double energy_ev = 0.0;
};

RadialCurve radial_distribution(int n, int l, double z_eff, double r_max_pm, int points = 400, OrbitalModel model = OrbitalModel::Hydrogenic);
RadialCurve radial_distribution(const Element& element, int n, int l, double r_max_pm, int points = 400, OrbitalModel model = OrbitalModel::Hydrogenic, int charge = 0);
std::vector<RadialCurve> atom_radial_distributions(const Element& element, double r_max_pm, int points = 400, OrbitalModel model = OrbitalModel::Hydrogenic, int charge = 0);
RadialCurve total_radial_density(const Element& element, double r_max_pm, int points = 400, OrbitalModel model = OrbitalModel::Hydrogenic, int charge = 0);
double suggested_radial_range_pm(const Element& element, int charge = 0);

struct CloudPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double density = 0.0;
    int sign = 1;
    int n = 0;
    int l = 0;
};

std::vector<CloudPoint> sample_orbital(int n, int l, int m, double z_eff, int count, unsigned seed = 1, OrbitalModel model = OrbitalModel::Hydrogenic);
std::vector<CloudPoint> sample_atom(const Element& element, int count, unsigned seed = 1, OrbitalModel model = OrbitalModel::Hydrogenic, int charge = 0, bool valence_only = false);

struct DensitySlice {
    int width = 0;
    int height = 0;
    double extent_pm = 0.0;
    std::vector<double> values;
    double max_value = 0.0;
    double min_value = 0.0;
};

DensitySlice orbital_slice(int n, int l, int m, double z_eff, char axis_a, char axis_b, double extent_pm, int resolution, bool signed_wavefunction = false, OrbitalModel model = OrbitalModel::Hydrogenic);
DensitySlice atom_slice(const Element& element, double extent_pm, int resolution, OrbitalModel model = OrbitalModel::Hydrogenic, int charge = 0);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ORBITALS_HPP
