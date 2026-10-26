#ifndef FIZMO_CHEMISTRY_TRANSPORT_HPP
#define FIZMO_CHEMISTRY_TRANSPORT_HPP

#include "crystal.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class MagneticOrder : std::uint8_t {
    Diamagnetic,
    Paramagnetic,
    Ferromagnetic,
    Antiferromagnetic,
    Ferrimagnetic
};

const char* to_string(MagneticOrder m) noexcept;

enum class ConductorKind : std::uint8_t {
    Metal,
    Semimetal,
    Semiconductor,
    Insulator,
    IonicConductor
};

const char* to_string(ConductorKind k) noexcept;

struct MagneticInfo {
    MagneticOrder order = MagneticOrder::Diamagnetic;
    double ordering_temperature = NO_VALUE;
    double relative_permeability = 1.0;
    std::string note;
};

struct TransportProperties {
    double temperature = 298.15;
    double frequency = 60.0;
    ConductorKind kind = ConductorKind::Insulator;
    double molar_heat_capacity = NO_VALUE;
    double specific_heat = NO_VALUE;
    double lattice_heat_capacity = NO_VALUE;
    double electronic_heat_capacity = NO_VALUE;
    double dulong_petit = NO_VALUE;
    double debye_temperature = NO_VALUE;
    double thermal_conductivity = NO_VALUE;
    double thermal_diffusivity = NO_VALUE;
    double resistivity = NO_VALUE;
    double conductivity = NO_VALUE;
    double resistivity_293 = NO_VALUE;
    double temperature_coefficient = NO_VALUE;
    double carrier_density = NO_VALUE;
    double relaxation_time = NO_VALUE;
    double mean_free_path = NO_VALUE;
    double mobility = NO_VALUE;
    double hall_coefficient = NO_VALUE;
    double plasma_frequency = NO_VALUE;
    double plasma_wavelength = NO_VALUE;
    double skin_depth = NO_VALUE;
    double band_gap = NO_VALUE;
    double lorenz_number = NO_VALUE;
    double drift_velocity_per_volt_per_m = NO_VALUE;
    MagneticInfo magnetic;
    std::vector<std::string> notes;
};

double tabulated_resistivity(int atomic_number) noexcept;
double resistivity_temperature_coefficient(int atomic_number) noexcept;
double semiconductor_band_gap(int atomic_number) noexcept;
MagneticInfo magnetic_info(const Element& element, double temperature);
double debye_temperature_estimate(double melting_point, double molar_mass_g, double molar_volume_m3) noexcept;
double debye_heat_capacity(double temperature, double debye_temperature) noexcept;
double skin_depth(double resistivity, double frequency, double relative_permeability = 1.0) noexcept;
double wiedemann_franz_conductivity(double resistivity, double temperature) noexcept;

TransportProperties element_transport(const Element& element, double temperature = 298.15, double frequency = 60.0);
TransportProperties metal_transport(const MetallicCrystal& crystal, double temperature = 298.15, double frequency = 60.0);
TransportProperties ionic_transport(const IonicCrystal& crystal, double temperature = 298.15);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_TRANSPORT_HPP
