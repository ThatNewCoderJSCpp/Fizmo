#ifndef FIZMO_CHEMISTRY_THERMOCHEMISTRY_HPP
#define FIZMO_CHEMISTRY_THERMOCHEMISTRY_HPP

#include "formula.hpp"
#include "substance.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

class Species {
public:
    Species() = default;
    explicit Species(const Substance& substance, Phase phase = Phase::Unknown);
    Species(Formula formula, Phase phase, double enthalpy_of_formation, double standard_entropy, std::vector<HeatCapacityRange> heat_capacity = {});

    static Species find(std::string_view name_or_formula, Phase phase = Phase::Unknown);
    static bool exists(const Formula& formula, Phase phase = Phase::Unknown);
    static Species of(const Formula& formula, Phase phase = Phase::Unknown);

    const std::string& name() const noexcept { return m_name; }
    const Formula& formula() const noexcept { return m_formula; }
    Phase phase() const noexcept { return m_phase; }
    const Substance* substance() const noexcept { return m_substance; }
    double molar_mass() const noexcept { return m_formula.molar_mass(); }
    bool has_thermo() const noexcept { return has_value(m_hf) && has_value(m_s); }
    bool is_electron() const noexcept { return m_formula.is_electron(); }

    double enthalpy_of_formation() const noexcept { return m_hf; }
    double standard_entropy() const noexcept { return m_s; }
    double gibbs_of_formation() const noexcept { return m_gf; }
    void set_gibbs_of_formation(double gf) noexcept { m_gf = gf; }
    void set_heat_capacity(std::vector<HeatCapacityRange> ranges) { m_cp = std::move(ranges); }
    void set_constant_heat_capacity(double cp);

    double heat_capacity(double t) const noexcept;
    double enthalpy(double t = STANDARD_TEMPERATURE) const noexcept;
    double entropy(double t = STANDARD_TEMPERATURE) const noexcept;
    double gibbs_energy(double t = STANDARD_TEMPERATURE) const noexcept;
    double entropy_at_pressure(double t, double p) const noexcept;

private:
    std::string m_name;
    Formula m_formula;
    Phase m_phase = Phase::Unknown;
    const Substance* m_substance = nullptr;
    double m_hf = NO_VALUE;
    double m_s = NO_VALUE;
    double m_gf = NO_VALUE;
    std::vector<HeatCapacityRange> m_cp;
};

double gibbs_free_energy(double delta_h, double delta_s, double t) noexcept;
double equilibrium_constant(double delta_g, double t) noexcept;
double gibbs_from_equilibrium_constant(double k, double t) noexcept;
double reaction_gibbs(double delta_g_standard, double t, double reaction_quotient) noexcept;
double van_t_hoff(double k1, double t1, double t2, double delta_h) noexcept;
double van_t_hoff_enthalpy(double k1, double t1, double k2, double t2) noexcept;
double kirchhoff(double delta_h1, double t1, double t2, double delta_cp) noexcept;
double crossover_temperature(double delta_h, double delta_s) noexcept;
double clausius_clapeyron_pressure(double p1, double t1, double t2, double delta_h_vap) noexcept;
double clausius_clapeyron_temperature(double p1, double t1, double p2, double delta_h_vap) noexcept;
double clausius_clapeyron_enthalpy(double p1, double t1, double p2, double t2) noexcept;
double clapeyron_slope(double delta_h, double t, double delta_v) noexcept;
double trouton_vaporization_enthalpy(double boiling_point) noexcept;
double heat(double mass_g, double specific_heat, double delta_t) noexcept;
double mixing_final_temperature(const std::vector<double>& heat_capacities, const std::vector<double>& temperatures);
double kp_from_kc(double kc, double t, double delta_n_gas) noexcept;
double kc_from_kp(double kp, double t, double delta_n_gas) noexcept;
double ideal_mixing_entropy(const std::vector<double>& mole_fractions);
double ideal_mixing_gibbs(double t, const std::vector<double>& mole_fractions);
double adiabatic_flame_temperature(const std::vector<Species>& products, const std::vector<double>& product_moles, double reactant_enthalpy, double t_guess = 2000.0);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_THERMOCHEMISTRY_HPP
