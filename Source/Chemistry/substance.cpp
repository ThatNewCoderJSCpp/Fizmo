#include "substance.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

template <typename F>
double integrate_ranges(Span<HeatCapacityRange> ranges, double t1, double t2, F antiderivative) {
    if (ranges.empty() || !has_value(t1) || !has_value(t2)) return NO_VALUE;
    if (t1 == t2) return 0.0;
    const double sign = t2 >= t1 ? 1.0 : -1.0;
    double lo = std::min(t1, t2);
    const double hi = std::max(t1, t2);
    double total = 0.0;
    const std::size_t n = ranges.size();
    for (std::size_t i = 0; i < n && lo < hi; ++i) {
        const double start = i == 0 ? -1e300 : ranges[i].t_min;
        const double stop = i + 1 == n ? 1e300 : ranges[i + 1].t_min;
        if (stop <= lo) continue;
        const double a = std::max(lo, start);
        const double b = std::min(hi, stop);
        if (b > a) {
            total += antiderivative(ranges[i], b) - antiderivative(ranges[i], a);
            lo = b;
        }
    }
    return sign * total;
}

} // namespace

double heat_capacity_at(Span<HeatCapacityRange> ranges, double t) noexcept {
    if (ranges.empty()) return NO_VALUE;
    for (std::size_t i = ranges.size(); i-- > 0;) {
        if (t >= ranges[i].t_min || i == 0) return ranges[i].cp(t);
    }
    return NO_VALUE;
}

double heat_capacity_enthalpy(Span<HeatCapacityRange> ranges, double t1, double t2) noexcept {
    return integrate_ranges(ranges, t1, t2, [](const HeatCapacityRange& r, double t) { return r.enthalpy_antiderivative(t); });
}

double heat_capacity_entropy(Span<HeatCapacityRange> ranges, double t1, double t2) noexcept {
    return integrate_ranges(ranges, t1, t2, [](const HeatCapacityRange& r, double t) { return r.entropy_antiderivative(t); });
}

double HeatCapacityRange::entropy_antiderivative(double t) const noexcept {
    return a * std::log(t) + t * (b + t * (c / 2.0 + t * (d / 3.0 + t * e / 4.0))) - f / (2.0 * t * t);
}

double Substance::heat_capacity(Phase p, double t) const noexcept {
    return heat_capacity_at(heat_capacity_ranges(p), t);
}

double Substance::enthalpy_change(Phase p, double t1, double t2) const noexcept {
    return heat_capacity_enthalpy(heat_capacity_ranges(p), t1, t2);
}

double Substance::entropy_change(Phase p, double t1, double t2) const noexcept {
    return heat_capacity_entropy(heat_capacity_ranges(p), t1, t2);
}

double Substance::enthalpy(Phase p, double t) const noexcept {
    const double h0 = enthalpy_of_formation(p);
    if (!has_value(h0)) return NO_VALUE;
    if (t == STANDARD_TEMPERATURE) return h0;
    return h0 + enthalpy_change(p, STANDARD_TEMPERATURE, t);
}

double Substance::entropy(Phase p, double t) const noexcept {
    const double s0 = standard_entropy(p);
    if (!has_value(s0)) return NO_VALUE;
    if (t == STANDARD_TEMPERATURE) return s0;
    return s0 + entropy_change(p, STANDARD_TEMPERATURE, t);
}

double Substance::gibbs_energy(Phase p, double t) const noexcept {
    return enthalpy(p, t) - t * entropy(p, t);
}

double Substance::vapor_pressure(double t) const noexcept {
    if (has_antoine()) return std::pow(10.0, antoine_a - antoine_b / (t + antoine_c));
    return clausius_clapeyron_pressure(t);
}

double Substance::boiling_temperature(double pressure) const noexcept {
    if (has_antoine()) return antoine_b / (antoine_a - std::log10(pressure)) - antoine_c;
    if (!has_value(boiling_point) || !has_value(vaporization_enthalpy)) return NO_VALUE;
    return 1.0 / (1.0 / boiling_point - GAS_CONSTANT * std::log(pressure / ATMOSPHERE) / vaporization_enthalpy);
}

double Substance::enthalpy_of_vaporization(double t) const noexcept {
    if (!has_value(vaporization_enthalpy) || !has_value(boiling_point)) return NO_VALUE;
    if (!has_value(critical_temperature)) return vaporization_enthalpy;
    if (t >= critical_temperature) return 0.0;
    return vaporization_enthalpy * std::pow((1.0 - t / critical_temperature) / (1.0 - boiling_point / critical_temperature), 0.38);
}

double Substance::clausius_clapeyron_pressure(double t) const noexcept {
    if (!has_value(boiling_point) || !has_value(vaporization_enthalpy)) return NO_VALUE;
    return ATMOSPHERE * std::exp(-vaporization_enthalpy / GAS_CONSTANT * (1.0 / t - 1.0 / boiling_point));
}

double Substance::rackett_liquid_volume(double t) const noexcept {
    if (!has_critical_point() || !has_value(critical_compressibility)) return NO_VALUE;
    const double tr = std::min(t / critical_temperature, 1.0);
    return GAS_CONSTANT * critical_temperature / critical_pressure * std::pow(critical_compressibility, 1.0 + std::pow(1.0 - tr, 2.0 / 7.0));
}

double Substance::liquid_density(double t) const noexcept {
    return molar_mass_kg() / rackett_liquid_volume(t);
}

Phase Substance::phase_at(double t, double pressure) const noexcept {
    if (has_critical_point() && t >= critical_temperature) return pressure >= critical_pressure ? Phase::Supercritical : Phase::Gas;
    if (has_value(melting_point) && t < melting_point) return Phase::Solid;
    const double psat = vapor_pressure(t);
    if (has_value(psat)) return pressure < psat ? Phase::Gas : Phase::Liquid;
    if (has_value(boiling_point)) return t >= boiling_point ? Phase::Gas : Phase::Liquid;
    return standard_state;
}

std::vector<const Substance*> find_substances(const Formula& formula) {
    std::vector<const Substance*> out;
    for (const Substance& s : substances()) {
        if (Formula::parse(s.formula).same_composition(formula.without_charge())) out.push_back(&s);
    }
    return out;
}

Formula formula_of(const Substance& s) {
    Formula f = Formula::parse(s.formula);
    f.set_phase(s.standard_state);
    return f;
}

} // namespace chemistry
} // namespace fizmo
