#ifndef FIZMO_CHEMISTRY_SUBSTANCE_TYPES_HPP
#define FIZMO_CHEMISTRY_SUBSTANCE_TYPES_HPP

#include "common.hpp"
#include "constants.hpp"

#include <cstdint>

namespace fizmo {
namespace chemistry {

struct HeatCapacityRange {
    double t_min = 0.0;
    double t_max = 0.0;
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    double d = 0.0;
    double e = 0.0;
    double f = 0.0;

    constexpr bool contains(double t) const noexcept { return t >= t_min && t <= t_max; }
    constexpr double cp(double t) const noexcept { return a + t * (b + t * (c + t * (d + t * e))) + f / (t * t); }
    constexpr double enthalpy_antiderivative(double t) const noexcept {
        return t * (a + t * (b / 2.0 + t * (c / 3.0 + t * (d / 4.0 + t * e / 5.0)))) - f / t;
    }
    double entropy_antiderivative(double t) const noexcept;
};

double heat_capacity_at(Span<HeatCapacityRange> ranges, double t) noexcept;
double heat_capacity_enthalpy(Span<HeatCapacityRange> ranges, double t1, double t2) noexcept;
double heat_capacity_entropy(Span<HeatCapacityRange> ranges, double t1, double t2) noexcept;

struct Substance {
    const char* name = "";
    const char* formula = "";
    const char* cas = "";
    double molar_mass = NO_VALUE;
    Phase standard_state = Phase::Unknown;
    double critical_temperature = NO_VALUE;
    double critical_pressure = NO_VALUE;
    double critical_volume = NO_VALUE;
    double critical_compressibility = NO_VALUE;
    double acentric_factor = NO_VALUE;
    double boiling_point = NO_VALUE;
    double melting_point = NO_VALUE;
    double triple_point_temperature = NO_VALUE;
    double triple_point_pressure = NO_VALUE;
    double dipole_moment = NO_VALUE;
    double enthalpy_of_formation_gas = NO_VALUE;
    double enthalpy_of_formation_liquid = NO_VALUE;
    double enthalpy_of_formation_solid = NO_VALUE;
    double entropy_gas = NO_VALUE;
    double entropy_liquid = NO_VALUE;
    double entropy_solid = NO_VALUE;
    double gibbs_of_formation_gas = NO_VALUE;
    double gibbs_of_formation_liquid = NO_VALUE;
    double gibbs_of_formation_solid = NO_VALUE;
    double vaporization_enthalpy = NO_VALUE;
    double vaporization_enthalpy_298 = NO_VALUE;
    double fusion_enthalpy = NO_VALUE;
    double lennard_jones_sigma = NO_VALUE;
    double lennard_jones_epsilon_k = NO_VALUE;
    double antoine_a = NO_VALUE;
    double antoine_b = NO_VALUE;
    double antoine_c = NO_VALUE;
    double antoine_t_min = NO_VALUE;
    double antoine_t_max = NO_VALUE;
    std::uint16_t cp_gas_offset = 0;
    std::uint8_t cp_gas_count = 0;
    std::uint16_t cp_liquid_offset = 0;
    std::uint8_t cp_liquid_count = 0;
    std::uint16_t cp_solid_offset = 0;
    std::uint8_t cp_solid_count = 0;

    constexpr bool has_critical_point() const noexcept { return critical_temperature == critical_temperature && critical_pressure == critical_pressure; }
    constexpr bool has_antoine() const noexcept { return antoine_a == antoine_a; }
    constexpr double molar_mass_kg() const noexcept { return molar_mass / 1000.0; }
    constexpr double enthalpy_of_formation(Phase p) const noexcept {
        return p == Phase::Gas ? enthalpy_of_formation_gas : (p == Phase::Liquid ? enthalpy_of_formation_liquid : (p == Phase::Solid ? enthalpy_of_formation_solid : NO_VALUE));
    }
    constexpr double standard_entropy(Phase p) const noexcept {
        return p == Phase::Gas ? entropy_gas : (p == Phase::Liquid ? entropy_liquid : (p == Phase::Solid ? entropy_solid : NO_VALUE));
    }
    constexpr double gibbs_of_formation(Phase p) const noexcept {
        return p == Phase::Gas ? gibbs_of_formation_gas : (p == Phase::Liquid ? gibbs_of_formation_liquid : (p == Phase::Solid ? gibbs_of_formation_solid : NO_VALUE));
    }
    constexpr double enthalpy_of_formation() const noexcept { return enthalpy_of_formation(standard_state); }
    constexpr double standard_entropy() const noexcept { return standard_entropy(standard_state); }
    constexpr double gibbs_of_formation() const noexcept { return gibbs_of_formation(standard_state); }
    constexpr double reduced_temperature(double t) const noexcept { return t / critical_temperature; }
    constexpr double reduced_pressure(double p) const noexcept { return p / critical_pressure; }
    constexpr bool has_phase_data(Phase p) const noexcept { return enthalpy_of_formation(p) == enthalpy_of_formation(p); }

    constexpr Span<HeatCapacityRange> heat_capacity_ranges(Phase p) const noexcept;

    double heat_capacity(Phase p, double t) const noexcept;
    double heat_capacity(double t) const noexcept { return heat_capacity(standard_state, t); }
    double enthalpy_change(Phase p, double t1, double t2) const noexcept;
    double entropy_change(Phase p, double t1, double t2) const noexcept;
    double enthalpy(Phase p, double t) const noexcept;
    double entropy(Phase p, double t) const noexcept;
    double gibbs_energy(Phase p, double t) const noexcept;
    double vapor_pressure(double t) const noexcept;
    double boiling_temperature(double pressure) const noexcept;
    double enthalpy_of_vaporization(double t) const noexcept;
    double clausius_clapeyron_pressure(double t) const noexcept;
    double rackett_liquid_volume(double t) const noexcept;
    double liquid_density(double t) const noexcept;
    Phase phase_at(double t, double pressure) const noexcept;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SUBSTANCE_TYPES_HPP
