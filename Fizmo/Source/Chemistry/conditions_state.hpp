#ifndef FIZMO_CHEMISTRY_CONDITIONS_HPP
#define FIZMO_CHEMISTRY_CONDITIONS_HPP

#include "../Converters/unit_converters.hpp"
#include "../Standard Overloads/abs.hpp"
#include <cstdint>
#include <string>
#include <iostream>
#include <sstream>

namespace fizmo {
namespace chemistry {

enum class Phase : std::uint8_t {
    BEC,
    SOLID,
    LIQUID,
    GAS,
    PLASMA,
    SUPERCRITICAL,
    LIQUID_VAPOR,
    SOLID_LIQUID,
    UNDEFINED
};

inline std::string phase_to_string(Phase p) {
    switch (p) {
        case Phase::BEC: return "Bose-Einstein condensate";
        case Phase::SOLID: return "solid";
        case Phase::LIQUID: return "liquid";
        case Phase::GAS: return "gas";
        case Phase::PLASMA: return "plasma";
        case Phase::SUPERCRITICAL: return "supercritical";
        case Phase::LIQUID_VAPOR: return "liquid-vapor equilibrium";
        case Phase::SOLID_LIQUID: return "solid-liquid equilibrium";
        case Phase::UNDEFINED: return "undefined";
        default: return "unknown";
    }
}

class Conditions {
private:
    double m_t;   
    double m_pa;  

public:
    constexpr Conditions(
        const double temp, 
        const double pressure, 
        const units::TemperatureUnit temp_unit = units::TemperatureUnit::KELVIN, 
        const units::PressureUnit pressure_unit = units::PressureUnit::SI()
    ) noexcept 
        : m_t(units::convert_temperature(temp, temp_unit, units::TemperatureUnit::KELVIN)), 
          m_pa(units::convert_pressure(pressure, pressure_unit, units::PressureUnit::SI())) 
    {}

    constexpr double temperature(const units::TemperatureUnit return_unit = units::TemperatureUnit::KELVIN) const noexcept {
        return units::convert_temperature(m_t, units::TemperatureUnit::KELVIN, return_unit);
    }

    constexpr double pressure(const units::PressureUnit return_unit = units::PressureUnit::SI()) const noexcept {
        return units::convert_pressure(m_pa, units::PressureUnit::SI(), return_unit);
    }

    constexpr void set_temperature(const double temp, const units::TemperatureUnit unit = units::TemperatureUnit::KELVIN) noexcept {
        m_t = units::convert_temperature(temp, unit, units::TemperatureUnit::KELVIN);
    }

    constexpr void set_pressure(const double pressure, const units::PressureUnit unit = units::PressureUnit::SI()) noexcept {
        m_pa = units::convert_pressure(pressure, unit, units::PressureUnit::SI());
    }

public:
    static constexpr Conditions STP() noexcept { return Conditions(273.15, 101325.0); }
    static constexpr Conditions standard() noexcept { return Conditions(298.15, 100000.0); }
    
public:
    constexpr bool operator==(const Conditions& other) const noexcept {
        return fizmo::abs_constexpr(m_t - other.m_t) <= constants::middle_epsilon() &&
               fizmo::abs_constexpr(m_pa - other.m_pa) <= constants::middle_epsilon();
    }
    
    constexpr bool operator!=(const Conditions& other) const noexcept { return !(*this == other); }
};

class SubstanceState {
private:
    Phase m_phase;
    double m_molar_volume;        // m^3/mol
    double m_density;             // kg/m^3
    double m_compressibility;     // dimensionless (Z = PV/nRT)
    double m_molar_mass;          // kg/mol 

public:
    constexpr SubstanceState() noexcept
        : m_phase(Phase::UNDEFINED),
          m_molar_volume(0.0),
          m_density(0.0),
          m_compressibility(1.0),
          m_molar_mass(0.0)
    {}

    constexpr SubstanceState(
        const Phase phase,
        const double molar_volume_m3_per_mol,
        const double density_kg_per_m3,
        const double compressibility_factor,
        const double molar_mass_kg_per_mol
    ) noexcept
        : m_phase(phase),
          m_molar_volume(molar_volume_m3_per_mol),
          m_density(density_kg_per_m3),
          m_compressibility(compressibility_factor),
          m_molar_mass(molar_mass_kg_per_mol)
    {}

    constexpr SubstanceState(
        const Phase phase,
        const double molar_volume,
        const units::VolumeUnit volume_unit,
        const double density,
        const units::DensityUnit_3D density_unit,
        const double compressibility_factor,
        const double molar_mass,
        const units::WeightUnit mass_unit
    ) noexcept
        : m_phase(phase),
          m_molar_volume(units::convert_volume(molar_volume, volume_unit, units::VolumeUnit::SI())),
          m_density(units::convert_density_3d(density, density_unit, units::DensityUnit_3D::SI())),
          m_compressibility(compressibility_factor),
          m_molar_mass(units::convert_weight(molar_mass, mass_unit, units::WeightUnit::KILOGRAM))
    {}

    constexpr Phase phase() const noexcept { return m_phase; }
    constexpr Phase& phase() noexcept { return m_phase; }

    constexpr double molar_volume(const units::VolumeUnit return_unit = units::VolumeUnit::SI()) const noexcept {
        return units::convert_volume(m_molar_volume, units::VolumeUnit::SI(), return_unit);
    }

    constexpr void set_molar_volume(const double vol, const units::VolumeUnit unit = units::VolumeUnit::SI()) noexcept {
        m_molar_volume = units::convert_volume(vol, unit, units::VolumeUnit::SI());
        if (m_molar_mass > 0.0 && m_molar_volume > 0.0) { m_density = m_molar_mass / m_molar_volume; }
    }

    constexpr double density(const units::DensityUnit_3D return_unit = units::DensityUnit_3D::SI()) const noexcept {
        return units::convert_density_3d(m_density, units::DensityUnit_3D::SI(), return_unit);
    }

    constexpr void set_density(const double dens, const units::DensityUnit_3D unit = units::DensityUnit_3D::SI()) noexcept {
        m_density = units::convert_density_3d(dens, unit, units::DensityUnit_3D::SI());
        if (m_molar_mass > 0.0 && m_density > 0.0) { m_molar_volume = m_molar_mass / m_density; }
    }

    constexpr double compressibility() const noexcept { return m_compressibility; }
    constexpr double& compressibility() noexcept { return m_compressibility; }

    constexpr double molar_mass(const units::WeightUnit return_unit = units::WeightUnit::KILOGRAM) const noexcept {
        return units::convert_weight(m_molar_mass, units::WeightUnit::KILOGRAM, return_unit);
    }

    constexpr void set_molar_mass(const double mass, const units::WeightUnit unit = units::WeightUnit::KILOGRAM) noexcept {
        m_molar_mass = units::convert_weight(mass, unit, units::WeightUnit::KILOGRAM);
    }

public:
    constexpr double specific_volume(
        const units::VolumeUnit vol_unit = units::VolumeUnit::SI(), 
        const units::WeightUnit mass_unit = units::WeightUnit::KILOGRAM
    ) const noexcept {
        if (m_density <= 0.0) return constants::POSITIVE_INFINITY<double>;
        const double specific_vol_si = 1.0 / m_density;  
        const double vol_converted = units::convert_volume(specific_vol_si, units::VolumeUnit::SI(), vol_unit);
        
        const double mass_factor = units::WeightUnit::KILOGRAM != mass_unit 
            ? units::convert_weight(1.0, mass_unit, units::WeightUnit::KILOGRAM) 
            : 1.0;

        return vol_converted * mass_factor;
    }

public:
    constexpr void compute_molar_volume_from_density() noexcept {
        if (m_density > 0.0 && m_molar_mass > 0.0) { m_molar_volume = m_molar_mass / m_density; }
    }

    constexpr void compute_density_from_molar_volume() noexcept {
        if (m_molar_volume > 0.0 && m_molar_mass > 0.0) { m_density = m_molar_mass / m_molar_volume; }
    }

public:
    constexpr bool is_defined() const noexcept { return m_phase != Phase::UNDEFINED; }
    constexpr bool is_condensed() const noexcept { return m_phase == Phase::SOLID || m_phase == Phase::LIQUID; }
    constexpr bool is_fluid() const noexcept { return m_phase == Phase::LIQUID || m_phase == Phase::GAS || m_phase == Phase::SUPERCRITICAL; }
    constexpr bool is_gas_like() const noexcept { return m_phase == Phase::GAS || m_phase == Phase::PLASMA; }
    constexpr bool is_two_phase() const noexcept { return m_phase == Phase::LIQUID_VAPOR || m_phase == Phase::SOLID_LIQUID; }
    constexpr bool is_ideal() const noexcept { return fizmo::abs_constexpr(m_compressibility - 1.0) <= constants::middle_epsilon(); }

public:
    constexpr bool operator==(const SubstanceState& other) const noexcept {
        return m_phase == other.m_phase &&
               fizmo::abs_constexpr(m_molar_volume - other.m_molar_volume) <= constants::middle_epsilon() &&
               fizmo::abs_constexpr(m_density - other.m_density) <= constants::middle_epsilon() &&
               fizmo::abs_constexpr(m_compressibility - other.m_compressibility) <= constants::middle_epsilon();
    }

    constexpr bool operator!=(const SubstanceState& other) const noexcept { return !(*this == other); }

public:
    static constexpr SubstanceState ideal_gas(const Conditions& cond, const double molar_mass_kg_per_mol) noexcept {
        // V_m = RT/P for ideal gas
        const double R = constants::ideal_gas_joules();  
        const double V_m = R * cond.temperature() / cond.pressure();
        const double rho = molar_mass_kg_per_mol / V_m;
        return SubstanceState(Phase::GAS, V_m, rho, 1.0, molar_mass_kg_per_mol);
    }

public:
    std::string to_string() const {
        std::stringstream ss;
        ss << "SubstanceState {\n"
           << "  phase: " << phase_to_string(m_phase) << ",\n"
           << "  molar_volume: " << m_molar_volume << " m³/mol,\n"
           << "  density: " << m_density << " kg/m³,\n"
           << "  Z: " << m_compressibility << "\n"
           << "}";
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const SubstanceState& state) {
        return os << state.to_string();
    }
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_CONDITIONS_HPP