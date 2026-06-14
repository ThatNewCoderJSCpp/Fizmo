#ifndef THERMAL_PROPERTIES_HPP
#define THERMAL_PROPERTIES_HPP

#include <cmath>

namespace fizmo {
namespace converters {
namespace thermal {

namespace thermal_conductivity {}
namespace thermal_resistance {}
namespace heat_transfer_coefficient {}
namespace specific_heat {}
namespace thermal_diffusivity {}
namespace heat_flux {}
namespace thermal_expansion {}

// Thermal conductivity conversions (SI unit: W/m·K)
namespace thermal_conductivity {
    // To SI unit (W/m·K)
    inline constexpr long double btu_per_hour_foot_fahrenheit_to_si(const long double k) noexcept { return k / 0.5778; }
    inline constexpr long double kilocalorie_per_hour_meter_celsius_to_si(const long double k) noexcept { return k / 0.85984; }

    // From SI unit (W/m·K)
    inline constexpr long double si_to_btu_per_hour_foot_fahrenheit(const long double k) noexcept { return k * 0.5778; }
    inline constexpr long double si_to_kilocalorie_per_hour_meter_celsius(const long double k) noexcept { return k * 0.85984; }
}

// Heat transfer coefficient conversions (SI unit: W/m²·K)
namespace heat_transfer_coefficient {
    // To SI unit (W/m²·K)
    inline constexpr long double btu_per_hour_square_foot_fahrenheit_to_si(const long double h) noexcept { return h / 0.176110; }

    // From SI unit (W/m²·K)
    inline constexpr long double si_to_btu_per_hour_square_foot_fahrenheit(const long double h) noexcept { return h * 0.176110; }
}

// Specific heat conversions (SI unit: J/kg·K)
namespace specific_heat {
    // To SI unit (J/kg·K)
    inline constexpr long double btu_per_pound_fahrenheit_to_si(const long double cp) noexcept { return cp * 4186.8; }
    inline constexpr long double calorie_per_gram_celsius_to_si(const long double cp) noexcept { return cp * 4186.8; }

    // From SI unit (J/kg·K)
    inline constexpr long double si_to_btu_per_pound_fahrenheit(const long double cp) noexcept { return cp / 4186.8; }
    inline constexpr long double si_to_calorie_per_gram_celsius(const long double cp) noexcept { return cp / 4186.8; }
}

// Thermal diffusivity conversions (SI unit: m²/s)
namespace thermal_diffusivity {
    // To SI unit (m²/s)
    inline constexpr long double square_foot_per_hour_to_si(const long double alpha) noexcept { return alpha / 38750.0; }

    // From SI unit (m²/s)
    inline constexpr long double si_to_square_foot_per_hour(const long double alpha) noexcept { return alpha * 38750.0; }
}

// Heat flux conversions (SI unit: W/m²)
namespace heat_flux {
    // To SI unit (W/m²)
    inline constexpr long double btu_per_hour_square_foot_to_si(const long double q) noexcept { return q / 0.316998; }

    // From SI unit (W/m²)
    inline constexpr long double si_to_btu_per_hour_square_foot(const long double q) noexcept { return q * 0.316998; }
}

namespace calculations {
    inline constexpr long double thermal_resistance(const long double thickness, 
                                       const long double area, 
                                       const long double conductivity) noexcept {
        return thickness / (conductivity * area);
    }

    inline constexpr long double natural_convection_coefficient(const long double characteristic_length,
                                                    const long double delta_t) noexcept {
        return 1.31 * std::pow(delta_t / characteristic_length, 0.33);
    }

    inline constexpr long double radiation_coefficient(const long double emissivity,
                                          const long double t_surface,
                                          const long double t_surroundings) noexcept {
        const long double stefan_boltzmann = 5.67e-8;  // W/(m²·K⁴)
        return emissivity * stefan_boltzmann * (t_surface + t_surroundings) * 
               (t_surface * t_surface + t_surroundings * t_surroundings);
    }

    inline constexpr long double overall_heat_transfer_coefficient(const long double h_inside, 
                                                       const long double h_outside,
                                                       const long double thickness,
                                                       const long double conductivity) noexcept {
        return 1.0 / (1.0/h_inside + thickness/conductivity + 1.0/h_outside);
    }

    inline constexpr long double heat_exchanger_lmtd(const long double t_hot_in, 
                                         const long double t_hot_out,
                                         const long double t_cold_in,
                                         const long double t_cold_out) noexcept {
        const long double delta_t1 = t_hot_in - t_cold_out;
        const long double delta_t2 = t_hot_out - t_cold_in;
        return (delta_t1 - delta_t2) / std::log(delta_t1/delta_t2);
    }

    inline constexpr long double heat_exchanger_effectiveness(const long double q_actual,
                                                  const long double q_max) noexcept {
        return q_actual / q_max;
    }

    inline constexpr long double critical_insulation_radius(const long double k_insulation,
                                                const long double h_outer) noexcept {
        return k_insulation / h_outer;
    }

    inline constexpr long double thermal_time_constant(const long double mass,
                                           const long double specific_heat,
                                           const long double h,
                                           const long double area) noexcept {
        return mass * specific_heat / (h * area);
    }

    inline constexpr long double fin_efficiency(const long double h,
                                    const long double perimeter,
                                    const long double k,
                                    const long double area,
                                    const long double length) noexcept {
        const long double m = std::sqrt(2.0 * h * perimeter / (k * area));
        return std::tanh(m * length) / (m * length);
    }

    inline constexpr long double linear_expansion(const long double initial_length,
                                      const long double alpha,
                                      const long double delta_t) noexcept {
        return initial_length * (1.0 + alpha * delta_t);
    }

    inline constexpr long double volumetric_expansion(const long double initial_volume,
                                          const long double beta,
                                          const long double delta_t) noexcept {
        return initial_volume * (1.0 + beta * delta_t);
    }

    inline constexpr long double thermal_stress(const long double elastic_modulus,
                                    const long double alpha,
                                    const long double delta_t) noexcept {
        return elastic_modulus * alpha * delta_t;
    }
}

} // namespace thermal
} // namespace converters
} // namespace fizmo

#endif // THERMAL_PROPERTIES_HPP