#include "thermochemistry.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;

Phase resolve_phase(const Substance& s, Phase phase) {
    if (phase == Phase::Unknown) return s.standard_state;
    return phase;
}

const Substance* best_substance(const Formula& formula, Phase phase) {
    const Substance* fallback = nullptr;
    for (const Substance* s : find_substances(formula)) {
        const Phase p = resolve_phase(*s, phase);
        if (s->has_phase_data(p)) {
            if (phase != Phase::Unknown || s->standard_state == p) return s;
            if (!fallback) fallback = s;
        }
    }
    return fallback;
}

} // namespace

Species::Species(const Substance& substance, Phase phase) : m_name(substance.name), m_formula(formula_of(substance)), m_phase(resolve_phase(substance, phase)), m_substance(&substance) {
    m_formula.set_phase(m_phase);
    m_hf = substance.enthalpy_of_formation(m_phase);
    m_s = substance.standard_entropy(m_phase);
    m_gf = substance.gibbs_of_formation(m_phase);
    const Span<HeatCapacityRange> ranges = substance.heat_capacity_ranges(m_phase);
    m_cp.assign(ranges.begin(), ranges.end());
}

Species::Species(Formula formula, Phase phase, double enthalpy_of_formation, double standard_entropy, std::vector<HeatCapacityRange> heat_capacity)
    : m_name(formula.to_string()), m_formula(std::move(formula)), m_phase(phase), m_hf(enthalpy_of_formation), m_s(standard_entropy), m_cp(std::move(heat_capacity)) {
    m_formula.set_phase(phase);
}

Species Species::find(std::string_view name_or_formula, Phase phase) {
    const Substance* s = find_substance(name_or_formula);
    if (s) return Species(*s, phase);
    return of(Formula::parse(name_or_formula), phase);
}

bool Species::exists(const Formula& formula, Phase phase) {
    if (formula.is_electron()) return true;
    return best_substance(formula, phase) != nullptr;
}

Species Species::of(const Formula& formula, Phase phase) {
    if (formula.is_electron()) {
        Species e(formula, Phase::Unknown, 0.0, 0.0);
        e.m_name = "electron";
        return e;
    }
    if (phase == Phase::Unknown) phase = formula.phase();
    const Substance* s = best_substance(formula, phase);
    if (!s) throw std::invalid_argument("fizmo::chemistry: no thermochemical data for " + formula.to_string());
    return Species(*s, phase);
}

void Species::set_constant_heat_capacity(double cp) {
    m_cp.assign(1, HeatCapacityRange{ 0.0, 1e9, cp, 0.0, 0.0, 0.0, 0.0, 0.0 });
}

double Species::heat_capacity(double t) const noexcept {
    return heat_capacity_at(Span<HeatCapacityRange>(m_cp.data(), m_cp.size()), t);
}

double Species::enthalpy(double t) const noexcept {
    if (!has_value(m_hf)) return NO_VALUE;
    if (t == STANDARD_TEMPERATURE) return m_hf;
    return m_hf + heat_capacity_enthalpy(Span<HeatCapacityRange>(m_cp.data(), m_cp.size()), STANDARD_TEMPERATURE, t);
}

double Species::entropy(double t) const noexcept {
    if (!has_value(m_s)) return NO_VALUE;
    if (t == STANDARD_TEMPERATURE) return m_s;
    return m_s + heat_capacity_entropy(Span<HeatCapacityRange>(m_cp.data(), m_cp.size()), STANDARD_TEMPERATURE, t);
}

double Species::gibbs_energy(double t) const noexcept {
    return enthalpy(t) - t * entropy(t);
}

double Species::entropy_at_pressure(double t, double p) const noexcept {
    const double s = entropy(t);
    return m_phase == Phase::Gas ? s - R * std::log(p / STANDARD_PRESSURE) : s;
}

double gibbs_free_energy(double delta_h, double delta_s, double t) noexcept {
    return delta_h - t * delta_s;
}

double equilibrium_constant(double delta_g, double t) noexcept {
    return std::exp(-delta_g / (R * t));
}

double gibbs_from_equilibrium_constant(double k, double t) noexcept {
    return -R * t * std::log(k);
}

double reaction_gibbs(double delta_g_standard, double t, double reaction_quotient) noexcept {
    return delta_g_standard + R * t * std::log(reaction_quotient);
}

double van_t_hoff(double k1, double t1, double t2, double delta_h) noexcept {
    return k1 * std::exp(-delta_h / R * (1.0 / t2 - 1.0 / t1));
}

double van_t_hoff_enthalpy(double k1, double t1, double k2, double t2) noexcept {
    return -R * std::log(k2 / k1) / (1.0 / t2 - 1.0 / t1);
}

double kirchhoff(double delta_h1, double t1, double t2, double delta_cp) noexcept {
    return delta_h1 + delta_cp * (t2 - t1);
}

double crossover_temperature(double delta_h, double delta_s) noexcept {
    return delta_h / delta_s;
}

double clausius_clapeyron_pressure(double p1, double t1, double t2, double delta_h_vap) noexcept {
    return p1 * std::exp(-delta_h_vap / R * (1.0 / t2 - 1.0 / t1));
}

double clausius_clapeyron_temperature(double p1, double t1, double p2, double delta_h_vap) noexcept {
    return 1.0 / (1.0 / t1 - R * std::log(p2 / p1) / delta_h_vap);
}

double clausius_clapeyron_enthalpy(double p1, double t1, double p2, double t2) noexcept {
    return -R * std::log(p2 / p1) / (1.0 / t2 - 1.0 / t1);
}

double clapeyron_slope(double delta_h, double t, double delta_v) noexcept {
    return delta_h / (t * delta_v);
}

double trouton_vaporization_enthalpy(double boiling_point) noexcept {
    return 87.0 * boiling_point;
}

double heat(double mass_g, double specific_heat, double delta_t) noexcept {
    return mass_g * specific_heat * delta_t;
}

double mixing_final_temperature(const std::vector<double>& heat_capacities, const std::vector<double>& temperatures) {
    if (heat_capacities.size() != temperatures.size()) throw std::invalid_argument("fizmo::chemistry: one temperature per heat capacity");
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < heat_capacities.size(); ++i) {
        num += heat_capacities[i] * temperatures[i];
        den += heat_capacities[i];
    }
    return num / den;
}

double kp_from_kc(double kc, double t, double delta_n_gas) noexcept {
    return kc * std::pow(STANDARD_CONCENTRATION * R * t / STANDARD_PRESSURE, delta_n_gas);
}

double kc_from_kp(double kp, double t, double delta_n_gas) noexcept {
    return kp / std::pow(STANDARD_CONCENTRATION * R * t / STANDARD_PRESSURE, delta_n_gas);
}

double ideal_mixing_entropy(const std::vector<double>& mole_fractions) {
    double s = 0.0;
    for (double x : mole_fractions) {
        if (x > 0.0) s -= x * std::log(x);
    }
    return R * s;
}

double ideal_mixing_gibbs(double t, const std::vector<double>& mole_fractions) {
    return -t * ideal_mixing_entropy(mole_fractions);
}

double adiabatic_flame_temperature(const std::vector<Species>& products, const std::vector<double>& product_moles, double reactant_enthalpy, double t_guess) {
    if (products.size() != product_moles.size()) throw std::invalid_argument("fizmo::chemistry: one amount per product");
    auto residual = [&](double t) {
        double h = 0.0;
        for (std::size_t i = 0; i < products.size(); ++i) h += product_moles[i] * products[i].enthalpy(t);
        return h - reactant_enthalpy;
    };
    double lo = STANDARD_TEMPERATURE, hi = std::max(t_guess, 1000.0);
    double flo = residual(lo), fhi = residual(hi);
    int guard = 0;
    while (fhi < 0.0 && guard++ < 40) {
        hi *= 1.5;
        fhi = residual(hi);
    }
    if (flo > 0.0 || fhi < 0.0) return NO_VALUE;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = residual(mid);
        if (fm < 0.0) lo = mid;
        else hi = mid;
        if (hi - lo < 1e-9) break;
    }
    return 0.5 * (lo + hi);
}

} // namespace chemistry
} // namespace fizmo
