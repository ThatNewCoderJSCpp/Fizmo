#include "kinetics.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;

} // namespace

double arrhenius(double a, double ea, double t) noexcept {
    return a * std::exp(-ea / (R * t));
}

double modified_arrhenius(double a, double n, double ea, double t, double t_ref) noexcept {
    return a * std::pow(t / t_ref, n) * std::exp(-ea / (R * t));
}

double activation_energy(double k1, double t1, double k2, double t2) noexcept {
    return R * std::log(k2 / k1) / (1.0 / t1 - 1.0 / t2);
}

double pre_exponential_factor(double k, double ea, double t) noexcept {
    return k * std::exp(ea / (R * t));
}

double rate_constant_at(double k1, double t1, double t2, double ea) noexcept {
    return k1 * std::exp(-ea / R * (1.0 / t2 - 1.0 / t1));
}

double eyring(double dh, double ds, double t, double transmission) noexcept {
    return transmission * BOLTZMANN_CONSTANT * t / PLANCK_CONSTANT * std::exp(ds / R) * std::exp(-dh / (R * t));
}

double eyring_from_gibbs(double dg, double t, double transmission) noexcept {
    return transmission * BOLTZMANN_CONSTANT * t / PLANCK_CONSTANT * std::exp(-dg / (R * t));
}

double collision_rate_constant(double cross_section, double reduced_mass_u, double t, double ea, double steric) noexcept {
    const double mu = reduced_mass_u * ATOMIC_MASS_CONSTANT;
    const double speed = std::sqrt(8.0 * BOLTZMANN_CONSTANT * t / (PI * mu));
    return steric * cross_section * speed * AVOGADRO_CONSTANT * std::exp(-ea / (R * t));
}

double collision_cross_section(double da, double db) noexcept {
    const double d = 0.5 * (da + db);
    return PI * d * d;
}

double q10(double k1, double k2, double t1, double t2) noexcept {
    return std::pow(k2 / k1, 10.0 / (t2 - t1));
}

double rate_from_q10(double k1, double q, double t1, double t2) noexcept {
    return k1 * std::pow(q, (t2 - t1) / 10.0);
}

double zero_order_concentration(double c0, double k, double t) noexcept {
    const double c = c0 - k * t;
    return c > 0.0 ? c : 0.0;
}

double first_order_concentration(double c0, double k, double t) noexcept {
    return c0 * std::exp(-k * t);
}

double second_order_concentration(double c0, double k, double t) noexcept {
    return c0 / (1.0 + k * c0 * t);
}

double nth_order_concentration(double c0, double k, double t, double n) noexcept {
    if (std::fabs(n - 1.0) < 1e-12) return first_order_concentration(c0, k, t);
    const double base = std::pow(c0, 1.0 - n) + (n - 1.0) * k * t;
    if (base <= 0.0) return 0.0;
    return std::pow(base, 1.0 / (1.0 - n));
}

double mixed_second_order_concentration(double a0, double b0, double k, double t) noexcept {
    if (std::fabs(a0 - b0) < 1e-15 * std::max(a0, b0)) return second_order_concentration(a0, k, t);
    const double e = std::exp((b0 - a0) * k * t);
    return a0 * (b0 - a0) / (b0 * e - a0);
}

double zero_order_half_life(double c0, double k) noexcept {
    return c0 / (2.0 * k);
}

double first_order_half_life(double k) noexcept {
    return LN2 / k;
}

double second_order_half_life(double c0, double k) noexcept {
    return 1.0 / (k * c0);
}

double nth_order_half_life(double c0, double k, double n) noexcept {
    if (std::fabs(n - 1.0) < 1e-12) return first_order_half_life(k);
    return (std::pow(2.0, n - 1.0) - 1.0) / ((n - 1.0) * k * std::pow(c0, n - 1.0));
}

double rate_constant_from_half_life(double half_life, double c0, double n) noexcept {
    if (std::fabs(n - 1.0) < 1e-12) return LN2 / half_life;
    if (std::fabs(n) < 1e-12) return c0 / (2.0 * half_life);
    return (std::pow(2.0, n - 1.0) - 1.0) / ((n - 1.0) * half_life * std::pow(c0, n - 1.0));
}

double first_order_time_to(double c0, double c, double k) noexcept {
    return std::log(c0 / c) / k;
}

double consecutive_intermediate(double a0, double k1, double k2, double t) noexcept {
    if (std::fabs(k1 - k2) < 1e-15 * std::max(k1, k2)) return a0 * k1 * t * std::exp(-k1 * t);
    return a0 * k1 / (k2 - k1) * (std::exp(-k1 * t) - std::exp(-k2 * t));
}

double consecutive_product(double a0, double k1, double k2, double t) noexcept {
    const double a = a0 * std::exp(-k1 * t);
    return a0 - a - consecutive_intermediate(a0, k1, k2, t);
}

double reversible_first_order(double a0, double kf, double kr, double t) noexcept {
    const double k = kf + kr;
    const double eq = a0 * kr / k;
    return eq + (a0 - eq) * std::exp(-k * t);
}

double michaelis_menten(double s, double v_max, double k_m) noexcept {
    return v_max * s / (k_m + s);
}

double competitive_inhibition(double s, double i, double v_max, double k_m, double k_i) noexcept {
    return v_max * s / (k_m * (1.0 + i / k_i) + s);
}

double lineweaver_burk_slope(double v_max, double k_m) noexcept {
    return k_m / v_max;
}

double rate_law(double k, const std::vector<double>& c, const std::vector<double>& orders) {
    if (c.size() != orders.size()) throw std::invalid_argument("fizmo::chemistry: one order per concentration");
    double r = k;
    for (std::size_t i = 0; i < c.size(); ++i) r *= std::pow(c[i], orders[i]);
    return r;
}

double overall_order(const std::vector<double>& orders) noexcept {
    double n = 0.0;
    for (double o : orders) n += o;
    return n;
}

double ElementaryStep::forward(const std::vector<double>& c) const {
    double r = forward_rate;
    for (const auto& p : reactants) r *= std::pow(std::max(c.at(p.first), 0.0), p.second);
    return r;
}

double ElementaryStep::reverse(const std::vector<double>& c) const {
    if (reverse_rate == 0.0) return 0.0;
    double r = reverse_rate;
    for (const auto& p : products) r *= std::pow(std::max(c.at(p.first), 0.0), p.second);
    return r;
}

std::size_t ReactionNetwork::add(ElementaryStep step) {
    for (const auto& p : step.reactants) {
        if (p.first >= m_species) m_species = p.first + 1;
    }
    for (const auto& p : step.products) {
        if (p.first >= m_species) m_species = p.first + 1;
    }
    m_steps.push_back(std::move(step));
    return m_steps.size() - 1;
}

std::size_t ReactionNetwork::add(std::vector<std::pair<std::size_t, double>> reactants, std::vector<std::pair<std::size_t, double>> products, double kf, double kr) {
    ElementaryStep s;
    s.reactants = std::move(reactants);
    s.products = std::move(products);
    s.forward_rate = kf;
    s.reverse_rate = kr;
    return add(std::move(s));
}

void ReactionNetwork::derivatives(const std::vector<double>& c, std::vector<double>& rates) const {
    rates.assign(m_species, 0.0);
    for (const ElementaryStep& s : m_steps) {
        const double r = s.net(c);
        for (const auto& p : s.reactants) rates[p.first] -= p.second * r;
        for (const auto& p : s.products) rates[p.first] += p.second * r;
    }
}

std::vector<double> ReactionNetwork::derivatives(const std::vector<double>& c) const {
    std::vector<double> out;
    derivatives(c, out);
    return out;
}

std::vector<double> ReactionNetwork::step_rates(const std::vector<double>& c) const {
    std::vector<double> out;
    for (const ElementaryStep& s : m_steps) out.push_back(s.net(c));
    return out;
}

std::vector<double> ReactionNetwork::simulate(std::vector<double> c, double time, int steps) const {
    if (steps <= 0) return c;
    const double h = time / steps;
    std::vector<double> k1, k2, k3, k4, tmp(c.size());
    for (int i = 0; i < steps; ++i) {
        derivatives(c, k1);
        for (std::size_t j = 0; j < c.size(); ++j) tmp[j] = c[j] + 0.5 * h * k1[j];
        derivatives(tmp, k2);
        for (std::size_t j = 0; j < c.size(); ++j) tmp[j] = c[j] + 0.5 * h * k2[j];
        derivatives(tmp, k3);
        for (std::size_t j = 0; j < c.size(); ++j) tmp[j] = c[j] + h * k3[j];
        derivatives(tmp, k4);
        for (std::size_t j = 0; j < c.size(); ++j) c[j] = std::max(0.0, c[j] + h / 6.0 * (k1[j] + 2.0 * k2[j] + 2.0 * k3[j] + k4[j]));
    }
    return c;
}

void ReactionNetwork::set_temperature_dependence(std::size_t step, double a, double ea, double t) {
    m_steps.at(step).forward_rate = arrhenius(a, ea, t);
}

} // namespace chemistry
} // namespace fizmo
