#include "solutions.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;

void require_same_size(std::size_t a, std::size_t b) {
    if (a != b) throw std::invalid_argument("fizmo::chemistry: lists must be the same length");
}

} // namespace

double molarity(double moles, double liters) noexcept {
    return moles / liters;
}

double molality(double moles_solute, double kg_solvent) noexcept {
    return moles_solute / kg_solvent;
}

double mole_fraction(double moles, double total) noexcept {
    return moles / total;
}

std::vector<double> mole_fractions(const std::vector<double>& moles) {
    double total = 0.0;
    for (double n : moles) total += n;
    std::vector<double> out;
    for (double n : moles) out.push_back(n / total);
    return out;
}

double mass_percent(double solute, double solution) noexcept {
    return 100.0 * solute / solution;
}

double parts_per_million(double solute, double solution) noexcept {
    return 1e6 * solute / solution;
}

double molarity_to_molality(double c, double density, double m) noexcept {
    return 1000.0 * c / (1000.0 * density - c * m);
}

double molality_to_molarity(double b, double density, double m) noexcept {
    return 1000.0 * b * density / (1000.0 + b * m);
}

double molality_to_mole_fraction(double b, double solvent_m) noexcept {
    return b / (b + 1000.0 / solvent_m);
}

double mole_fraction_to_molality(double x, double solvent_m) noexcept {
    return 1000.0 * x / ((1.0 - x) * solvent_m);
}

double dilution_volume(double c1, double v1, double c2) noexcept {
    return c1 * v1 / c2;
}

double dilution_concentration(double c1, double v1, double v2) noexcept {
    return c1 * v1 / v2;
}

double mixed_concentration(const std::vector<double>& c, const std::vector<double>& v) {
    require_same_size(c.size(), v.size());
    double n = 0.0, total = 0.0;
    for (std::size_t i = 0; i < c.size(); ++i) {
        n += c[i] * v[i];
        total += v[i];
    }
    return n / total;
}

double ebullioscopic_constant(double tb, double dh_vap, double m) noexcept {
    return R * tb * tb * m / 1000.0 / dh_vap;
}

double cryoscopic_constant(double tm, double dh_fus, double m) noexcept {
    return R * tm * tm * m / 1000.0 / dh_fus;
}

double boiling_point_elevation(double b, double kb, double i) noexcept {
    return i * kb * b;
}

double freezing_point_depression(double b, double kf, double i) noexcept {
    return i * kf * b;
}

double osmotic_pressure(double c, double t, double i) noexcept {
    return i * c * 1000.0 * R * t;
}

double vant_hoff_factor(int ions, double alpha) noexcept {
    return 1.0 + alpha * (ions - 1);
}

double molar_mass_from_freezing_point(double solute_g, double solvent_kg, double dt, double kf, double i) noexcept {
    return i * kf * solute_g / (dt * solvent_kg);
}

double raoult_pressure(double x, double p) noexcept {
    return x * p;
}

double raoult_total_pressure(const std::vector<double>& x, const std::vector<double>& p) {
    require_same_size(x.size(), p.size());
    double total = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) total += x[i] * p[i];
    return total;
}

std::vector<double> raoult_vapor_composition(const std::vector<double>& x, const std::vector<double>& p) {
    const double total = raoult_total_pressure(x, p);
    std::vector<double> y;
    for (std::size_t i = 0; i < x.size(); ++i) y.push_back(x[i] * p[i] / total);
    return y;
}

double henry_concentration(double partial_pressure, double kh) noexcept {
    return kh * partial_pressure;
}

double henry_partial_pressure(double c, double kh) noexcept {
    return c / kh;
}

double henry_temperature(double kh_ref, double t, double t_ref, double dh_sol) noexcept {
    return kh_ref * std::exp(-dh_sol / R * (1.0 / t - 1.0 / t_ref));
}

double bubble_point_pressure(const std::vector<double>& x, const std::vector<double>& p) {
    return raoult_total_pressure(x, p);
}

double dew_point_pressure(const std::vector<double>& y, const std::vector<double>& p) {
    require_same_size(y.size(), p.size());
    double s = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i) s += y[i] / p[i];
    return 1.0 / s;
}

double ionic_strength(const std::vector<IonConcentration>& ions) {
    double i = 0.0;
    for (const IonConcentration& ion : ions) i += ion.concentration * ion.charge * ion.charge;
    return 0.5 * i;
}

double debye_huckel_limiting(int z, double i, double a) noexcept {
    return std::pow(10.0, -a * z * z * std::sqrt(i));
}

double debye_huckel_extended(int z, double i, double size_pm, double a, double b) noexcept {
    const double s = std::sqrt(i);
    return std::pow(10.0, -a * z * z * s / (1.0 + b * (size_pm / 100.0) * s));
}

double davies(int z, double i, double a) noexcept {
    const double s = std::sqrt(i);
    return std::pow(10.0, -a * z * z * (s / (1.0 + s) - 0.3 * i));
}

double activity(double c, double gamma) noexcept {
    return c * gamma;
}

double solubility_from_ksp(double ksp, int m, int n) noexcept {
    return std::pow(ksp / (std::pow(m, m) * std::pow(n, n)), 1.0 / (m + n));
}

double ksp_from_solubility(double s, int m, int n) noexcept {
    return std::pow(m * s, m) * std::pow(n * s, n);
}

bool precipitates(double q, double ksp) noexcept {
    return q > ksp;
}

} // namespace chemistry
} // namespace fizmo
