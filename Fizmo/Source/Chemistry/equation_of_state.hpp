#ifndef FIZMO_CHEMISTRY_EQUATION_OF_STATE_HPP
#define FIZMO_CHEMISTRY_EQUATION_OF_STATE_HPP

#include "conditions_state.hpp"
#include "../Symbolic Calculus/Real Symbols/expression.hpp"
#include "../Symbolic Calculus/Real/equation_solver.hpp"
#include "../Symbolic Calculus/Real/convienience_1.hpp"
#include "../Basic/constants.hpp"
#include <cmath>
#include <vector>
#include "../Converters/unit_converters.hpp"

namespace fizmo {
namespace chemistry {

struct MolecularParameters {
    double molar_mass;              // kg/mol
    
    // Intermolecular potential parameters (Lennard-Jones)
    double lj_epsilon;              // J (depth of potential well)
    double lj_sigma;                // m (collision diameter)
    
    double T_boiling_1atm;          // K (boiling point at 101325 Pa)
    double T_melting_1atm;          // K (melting point at 101325 Pa)

    double H_fusion;                // J/mol (enthalpy of fusion)
    double V_liquid_at_melt;        // m^3/mol (liquid molar volume at melting point)
    double V_solid_at_melt;         // m^3/mol (solid molar volume at melting point)
    
    double H_sublimation;  // J/mol
    
    constexpr double delta_V_fusion() const noexcept {
        return V_liquid_at_melt - V_solid_at_melt;
    }
};

struct CriticalPoint {
    const double temperature;   // critical temperature (K)
    const double pressure;   // critical pressure (Pa)
    const double volume;   // critical molar volume (m^3/mol)
    constexpr CriticalPoint(double Tc, double Pc, double Vc) noexcept : temperature(Tc), pressure(Pc), volume(Vc) {}
};

struct DerivedCriticalData {
    CriticalPoint critical_point;
    double Z_critical;      // compressibility factor at critical point
    double acentric_factor; 
    
    double T_triple;        // K
    double P_triple;        // Pa
    
    double H_vaporization_triple;   // J/mol at triple point
    double H_sublimation_triple;    // J/mol at triple point
};

enum class EOSError : std::uint8_t {
    NONE = 0,
    INVALID_TEMPERATURE,      // T <= 0 K
    INVALID_PRESSURE,         // P < 0
    INVALID_VOLUME,           // V <= 0
    INVALID_MOLES,            // n <= 0
    INVALID_MOLAR_MASS,       // M <= 0
    INVALID_VDW_CONSTANTS,    // a < 0 or b < 0
    NO_REAL_SOLUTION,         // Cubic has no physical solution
    NUMERICAL_ERROR           // Convergence failure
};

inline std::string eos_error_to_string(EOSError err) {
    switch (err) {
        case EOSError::NONE: return "No error";
        case EOSError::INVALID_TEMPERATURE: return "Temperature must be positive (T > 0 K)";
        case EOSError::INVALID_PRESSURE: return "Pressure must be non-negative (P >= 0)";
        case EOSError::INVALID_VOLUME: return "Volume must be positive (V > 0)";
        case EOSError::INVALID_MOLES: return "Number of moles must be positive (n > 0)";
        case EOSError::INVALID_MOLAR_MASS: return "Molar mass must be positive (M > 0)";
        case EOSError::INVALID_VDW_CONSTANTS: return "Van der Waals constants must be non-negative";
        case EOSError::NO_REAL_SOLUTION: return "No physically meaningful solution exists";
        case EOSError::NUMERICAL_ERROR: return "Numerical computation failed to converge";
        default: return "Unknown error";
    }
}

template<typename T = double>
struct EOSResult {
    T value;
    EOSError error;
    
    constexpr EOSResult() noexcept : value{}, error(EOSError::NUMERICAL_ERROR) {}
    constexpr EOSResult(T val) noexcept : value(val), error(EOSError::NONE) {}
    constexpr EOSResult(EOSError err) noexcept : value{}, error(err) {}
    constexpr EOSResult(T val, EOSError err) noexcept : value(val), error(err) {}
    
    constexpr bool ok() const noexcept { return error == EOSError::NONE; }
    constexpr explicit operator bool() const noexcept { return ok(); }
};


}  // namespace chemistry
}  // namespace fizmo

#endif