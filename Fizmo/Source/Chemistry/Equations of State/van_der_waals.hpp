#ifndef FIZMO_CHEMISTRY_VAN_DER_WAALS_HPP
#define FIZMO_CHEMISTRY_VAN_DER_WAALS_HPP

#include "../equation_of_state.hpp"

namespace fizmo {
namespace chemistry {

struct VDWConstants {
    double a;  // Pa*m^6/mol^2 (attraction parameter)
    double b;  // m^3/mol (volume exclusion parameter)
    
    constexpr VDWConstants() noexcept : a(0.0), b(0.0) {}
    constexpr VDWConstants(double a_, double b_) noexcept : a(a_), b(b_) {}
    
    static constexpr VDWConstants from_critical(double T_c, double P_c) noexcept {
        const double R = constants::ideal_gas_joules();
        // a = 27 R² Tc² / (64 Pc)
        // b = R Tc / (8 Pc)
        const double a = 27.0 * R * R * T_c * T_c / (64.0 * P_c);
        const double b = R * T_c / (8.0 * P_c);
        return VDWConstants(a, b);
    }
    
    static constexpr VDWConstants from_lennard_jones(double epsilon, double sigma, bool attractive_only = true) noexcept {
        constexpr double N_A = constants::avogadro();  // molecules per mole
        const double sigma3 = sigma * sigma * sigma;
        const double two_pi_over_3 = 2.0 * constants::pi_3() * N_A * sigma3; 
        const double a = two_pi_over_3 * N_A * epsilon * (attractive_only ? 8.0 : 10.0);
        const double b = two_pi_over_3;  
        return VDWConstants(a, b);
    }
    
    constexpr bool is_valid() const noexcept { return a >= 0.0 && b >= 0.0; }
};

class IdealGasEOS {
public:
    static constexpr EOSResult<double> pressure(
        double T, double V, double n,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) noexcept {
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double V_m3 = units::convert_volume(V, V_unit, units::VolumeUnit::SI());
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (V_m3 <= 0.0) return EOSError::INVALID_VOLUME;
        if (n <= 0.0) return EOSError::INVALID_MOLES;
        const double R = constants::ideal_gas_joules();
        const double P_Pa = n * R * T_K / V_m3;
        return units::convert_pressure(P_Pa, units::PressureUnit::SI(), P_unit);
    }
    
    static constexpr EOSResult<double> volume(
        double T, double P, double n,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI()
    ) noexcept {
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        if (n <= 0.0) return EOSError::INVALID_MOLES;
        if (P_Pa == 0.0) return EOSResult(constants::POSITIVE_INFINITY<double>, EOSError::NONE);
        const double R = constants::ideal_gas_joules();
        const double V_m3 = n * R * T_K / P_Pa;
        return units::convert_volume(V_m3, units::VolumeUnit::SI(), V_unit);
    }
    
    static constexpr EOSResult<double> temperature(
        double P, double V, double n,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN
    ) noexcept {
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        const double V_m3 = units::convert_volume(V, V_unit, units::VolumeUnit::SI());
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        if (V_m3 <= 0.0) return EOSError::INVALID_VOLUME;
        if (n <= 0.0) return EOSError::INVALID_MOLES;
        const double R = constants::ideal_gas_joules();
        const double T_K = P_Pa * V_m3 / (n * R);
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        return units::convert_temperature(T_K, units::TemperatureUnit::KELVIN, T_unit);
    }
    
    static constexpr EOSResult<double> moles(
        double P, double V, double T,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN
    ) noexcept {
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        const double V_m3 = units::convert_volume(V, V_unit, units::VolumeUnit::SI());
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        if (V_m3 <= 0.0) return EOSError::INVALID_VOLUME;
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        const double R = constants::ideal_gas_joules();
        return P_Pa * V_m3 / (R * T_K);
    }
    
    static constexpr EOSResult<double> molar_volume(
        double T, double P,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI()
    ) noexcept {
        return volume(T, P, 1.0, T_unit, P_unit, V_unit);
    }
    
    static constexpr EOSResult<double> density(
        double T, double P, double molar_mass,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::WeightUnit M_unit = units::WeightUnit::KILOGRAM,
        units::DensityUnit_3D rho_unit = units::DensityUnit_3D::SI()
    ) noexcept {
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        const double M_kg = units::convert_weight(molar_mass, M_unit, units::WeightUnit::KILOGRAM);
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        if (M_kg <= 0.0) return EOSError::INVALID_MOLAR_MASS;
        const double R = constants::ideal_gas_joules();
        const double rho_SI = P_Pa * M_kg / (R * T_K);
        return units::convert_density_3d(rho_SI, units::DensityUnit_3D::SI(), rho_unit);
    }
    
    static constexpr EOSResult<double> compressibility_factor(
        double /*T*/, double /*P*/, double /*Vm*/,
        units::TemperatureUnit /*T_unit*/ = units::TemperatureUnit::KELVIN,
        units::PressureUnit /*P_unit*/ = units::PressureUnit::SI(),
        units::VolumeUnit /*V_unit*/ = units::VolumeUnit::SI()
    ) noexcept {
        return 1.0;
    }
    
    static constexpr EOSResult<SubstanceState> state(
        const Conditions& cond, double molar_mass_kg_mol
    ) noexcept {
        if (cond.temperature() <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (cond.pressure() < 0.0) return EOSError::INVALID_PRESSURE;
        if (molar_mass_kg_mol <= 0.0) return EOSError::INVALID_MOLAR_MASS;
        const double R = constants::ideal_gas_joules();
        const double Vm = R * cond.temperature() / cond.pressure();
        const double rho = molar_mass_kg_mol / Vm;
        return SubstanceState(Phase::GAS, Vm, rho, 1.0, molar_mass_kg_mol);
    }
};

// Van der Waals Equation of State: (P + a/Vm²)(Vm - b) = RT
class VanDerWaalsEOS {
private:
    const VDWConstants m_constants;

    static constexpr double estimate_Vm_bounds(
        double T_K, double P_Pa, double a, double b,
        double& lower_bound, double& upper_bound, std::size_t& scan_points
    ) noexcept {
        const double R = constants::ideal_gas_joules();
        lower_bound = b * 0.995 - constants::epsilon();
        const double Vm_ideal = R * T_K / P_Pa;
        upper_bound = fizmo::max_constexpr(2.0 * Vm_ideal, 10.0 * b);
        const double ratio = upper_bound / lower_bound;
        const double log_ratio = fizmo::math::log10_constexpr(ratio);

        scan_points = static_cast<std::size_t>(
            fizmo::clamp(log_ratio * 200.0, 5e2, 1e6)
        );
        
        return Vm_ideal; 
    }
    
    static EOSResult<double> solve_cubic_for_Vm(
        double T_K, double P_Pa, double a, double b, bool prefer_liquid = false
    ) noexcept {
        if (P_Pa <= 0.0) { return EOSError::INVALID_PRESSURE; }
        const double R = constants::ideal_gas_joules();
        const double c2 = -(b + R * T_K / P_Pa);
        const double c1 = a / P_Pa;
        const double c0 = -a * b / P_Pa;
        using math::operator*;
        using math::operator+;
        const auto V = math::Var("v");
        const auto eq = V * (V * (V + c2) + c1) + c0;   
        double lower;
        double upper;
        std::size_t n_points;
        estimate_Vm_bounds(T_K, P_Pa, a, b, lower, upper, n_points);
        const auto solves = math::solvers::EquationSolver::find_roots(eq, "v", lower, upper, constants::type_epsilon(), n_points, 250); 
        bool any_converged = false;
        bool any_physical = false;
        double v = constants::quiet_nan();

        for (const auto sol : solves) {
            if (sol.converged) { any_converged = true; }
            if (!sol.converged || std::isnan(sol.value) || sol.value <= 0.0) { continue; }
            any_physical = true;

            if (std::isnan(v)) {
                v = sol.value;
            } else {
                v = prefer_liquid ? std::min(v, sol.value) : std::max(v, sol.value);
            }
        }

        if (any_physical) { return EOSResult<double>(v); }
        if (!any_converged) { return EOSResult<double>(EOSError::NUMERICAL_ERROR); }
        return EOSResult<double>(EOSError::NO_REAL_SOLUTION); 
    }

public:
    constexpr VanDerWaalsEOS() noexcept : m_constants() {}
    constexpr explicit VanDerWaalsEOS(const VDWConstants& constants) noexcept : m_constants(constants) {}
    constexpr VanDerWaalsEOS(double a, double b) noexcept : m_constants(a, b) {}
    
    static constexpr VanDerWaalsEOS from_critical(
        double T_c, double P_c,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) noexcept {
        const double T_c_K = units::convert_temperature(T_c, T_unit, units::TemperatureUnit::KELVIN);
        const double P_c_Pa = units::convert_pressure(P_c, P_unit, units::PressureUnit::SI());
        return VanDerWaalsEOS(VDWConstants::from_critical(T_c_K, P_c_Pa));
    }
    
    static constexpr VanDerWaalsEOS from_molecular(const MolecularParameters& params) noexcept {
        return VanDerWaalsEOS(VDWConstants::from_lennard_jones(params.lj_epsilon, params.lj_sigma));
    }
    
    constexpr const VDWConstants& constants() const noexcept { return m_constants; }
    
    constexpr EOSResult<double> pressure(
        double T, double Vm,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) const noexcept {
        if (!m_constants.is_valid()) return EOSError::INVALID_VDW_CONSTANTS;
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double Vm_m3 = units::convert_volume(Vm, V_unit, units::VolumeUnit::SI());
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (Vm_m3 <= 0.0) return EOSError::INVALID_VOLUME;
        if (Vm_m3 <= m_constants.b) return EOSError::INVALID_VOLUME;  
        const double R = constants::ideal_gas_joules();
        const double P_Pa = R * T_K / (Vm_m3 - m_constants.b) - m_constants.a / (Vm_m3 * Vm_m3);
        if (P_Pa < 0.0) { return EOSResult(P_Pa, EOSError::NO_REAL_SOLUTION); }
        return units::convert_pressure(P_Pa, units::PressureUnit::SI(), P_unit);
    }
    
    // Calculate pressure for n moles in volume V
    constexpr EOSResult<double> pressure_nV(
        double T, double V, double n,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) const noexcept {
        if (n <= 0.0) return EOSError::INVALID_MOLES;
        const double V_m3 = units::convert_volume(V, V_unit, units::VolumeUnit::SI());
        const double Vm = V_m3 / n;
        return pressure(T, Vm, T_unit, units::VolumeUnit::SI(), P_unit);
    }
    
    EOSResult<double> molar_volume(
        double T, double P, bool prefer_liquid = false,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI()
    ) const noexcept {
        if (!m_constants.is_valid()) return EOSError::INVALID_VDW_CONSTANTS;
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        auto result = solve_cubic_for_Vm(T_K, P_Pa, m_constants.a, m_constants.b, prefer_liquid);
        if (!result.ok()) return result.error;
        return units::convert_volume(result.value, units::VolumeUnit::SI(), V_unit);
    }
    
    // Calculate total volume for n moles
    EOSResult<double> volume(
        double T, double P, double n, bool prefer_liquid = false,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI()
    ) const noexcept {
        if (n <= 0.0) return EOSError::INVALID_MOLES;
        auto Vm_result = molar_volume(T, P, prefer_liquid, T_unit, P_unit, units::VolumeUnit::SI());
        if (!Vm_result.ok()) return Vm_result.error;
        const double V_m3 = Vm_result.value * n;
        return units::convert_volume(V_m3, units::VolumeUnit::SI(), V_unit);
    }
    
    EOSResult<double> temperature(
        double P, double Vm,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::VolumeUnit V_unit = units::VolumeUnit::SI(),
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        double tolerance = 1e-10,
        int max_iterations = 100
    ) const noexcept {
        if (!m_constants.is_valid()) return EOSError::INVALID_VDW_CONSTANTS;
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        const double Vm_m3 = units::convert_volume(Vm, V_unit, units::VolumeUnit::SI());
        if (P_Pa < 0.0) return EOSError::INVALID_PRESSURE;
        if (Vm_m3 <= 0.0) return EOSError::INVALID_VOLUME;
        if (Vm_m3 <= m_constants.b) return EOSError::INVALID_VOLUME;
        const double R = constants::ideal_gas_joules();
        const double T_K = (P_Pa + m_constants.a / (Vm_m3 * Vm_m3)) * (Vm_m3 - m_constants.b) / R;
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        return units::convert_temperature(T_K, units::TemperatureUnit::KELVIN, T_unit);
    }
    
    EOSResult<double> compressibility_factor(
        double T, double P, bool prefer_liquid = false,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) const noexcept {
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        if (T_K <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (P_Pa <= 0.0) return EOSError::INVALID_PRESSURE;   
        auto Vm_result = molar_volume(T, P, prefer_liquid, T_unit, P_unit);
        if (!Vm_result.ok()) return Vm_result.error;
        const double R = constants::ideal_gas_joules();
        return P_Pa * Vm_result.value / (R * T_K);
    }
    
    EOSResult<double> density(
        double T, double P, double molar_mass, bool prefer_liquid = false,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI(),
        units::WeightUnit M_unit = units::WeightUnit::KILOGRAM,
        units::DensityUnit_3D rho_unit = units::DensityUnit_3D::SI()
    ) const noexcept {
        const double M_kg = units::convert_weight(molar_mass, M_unit, units::WeightUnit::KILOGRAM);
        if (M_kg <= 0.0) return EOSError::INVALID_MOLAR_MASS;    
        auto Vm_result = molar_volume(T, P, prefer_liquid, T_unit, P_unit);
        if (!Vm_result.ok()) return Vm_result.error;
        const double rho_SI = M_kg / Vm_result.value;
        return units::convert_density_3d(rho_SI, units::DensityUnit_3D::SI(), rho_unit);
    }
    
    EOSResult<SubstanceState> state(
        const Conditions& cond, double molar_mass_kg_mol, bool prefer_liquid = false
    ) const noexcept {
        if (!m_constants.is_valid()) return EOSError::INVALID_VDW_CONSTANTS;
        if (cond.temperature() <= 0.0) return EOSError::INVALID_TEMPERATURE;
        if (cond.pressure() < 0.0) return EOSError::INVALID_PRESSURE;
        if (molar_mass_kg_mol <= 0.0) return EOSError::INVALID_MOLAR_MASS;
        auto Vm_result = molar_volume(cond.temperature(), cond.pressure(), prefer_liquid);
        if (!Vm_result.ok()) return Vm_result.error;
        const double Vm = Vm_result.value;
        const double rho = molar_mass_kg_mol / Vm;
        auto Z_result = compressibility_factor(cond.temperature(), cond.pressure(), prefer_liquid);
        const double Z = Z_result.ok() ? Z_result.value : 1.0;
        Phase phase = prefer_liquid ? Phase::LIQUID : Phase::GAS;
        return SubstanceState(phase, Vm, rho, Z, molar_mass_kg_mol);
    }
    
    constexpr DerivedCriticalData critical_point() const noexcept {
        const double R = constants::ideal_gas_joules();
        const double T_c = 8.0 * m_constants.a / (27.0 * R * m_constants.b);
        const double P_c = m_constants.a / (27.0 * m_constants.b * m_constants.b);
        const double V_c = 3.0 * m_constants.b;
        const double Z_c = 3.0 / 8.0; 

        return DerivedCriticalData{
            CriticalPoint(T_c, P_c, V_c),
            Z_c
        };
    }
    
    EOSResult<double> fugacity_coefficient(
        double T, double P, bool prefer_liquid = false,
        units::TemperatureUnit T_unit = units::TemperatureUnit::KELVIN,
        units::PressureUnit P_unit = units::PressureUnit::SI()
    ) const noexcept {
        const double T_K = units::convert_temperature(T, T_unit, units::TemperatureUnit::KELVIN);
        const double P_Pa = units::convert_pressure(P, P_unit, units::PressureUnit::SI());
        auto Z_result = compressibility_factor(T, P, prefer_liquid, T_unit, P_unit);
        if (!Z_result.ok()) return Z_result.error;
        const double Z = Z_result.value;
        const double R = constants::ideal_gas_joules();
        auto Vm_result = molar_volume(T, P, prefer_liquid, T_unit, P_unit);
        if (!Vm_result.ok()) return Vm_result.error;
        const double Vm = Vm_result.value;
        const double term1 = -std::log(1.0 - m_constants.b / Vm);
        const double term2 = m_constants.b / (Vm - m_constants.b);
        const double term3 = -m_constants.a / (R * T_K * Vm);
        const double ln_phi = term1 + term2 + term3;
        return std::exp(ln_phi);
    }
};

constexpr CriticalPoint critical_point_from_vdw(double a, double b) noexcept {
    const double R = constants::ideal_gas_joules();
    const double T_c = 8.0 * a / (27.0 * R * b);
    const double P_c = a / (27.0 * b * b);
    const double V_c = 3.0 * b;
    return CriticalPoint(T_c, P_c, V_c);
}

constexpr CriticalPoint critical_point_from_vdw(const VDWConstants& consts) noexcept {
    return critical_point_from_vdw(consts.a, consts.b);
}

static constexpr CriticalPoint critical_point_from_lennard(double epsilon, double sigma, bool attractive_only = true) noexcept {
    return critical_point_from_vdw(VDWConstants::from_lennard_jones(epsilon, sigma, attractive_only));
}

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_VAN_DER_WAALS_HPP