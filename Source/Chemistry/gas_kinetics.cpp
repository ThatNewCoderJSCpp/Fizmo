#include "gas_kinetics.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;
const double K = BOLTZMANN_CONSTANT;

} // namespace

double rms_speed(double t, double m) noexcept {
    return std::sqrt(3.0 * R * t / (m / 1000.0));
}

double mean_speed(double t, double m) noexcept {
    return std::sqrt(8.0 * R * t / (PI * m / 1000.0));
}

double most_probable_speed(double t, double m) noexcept {
    return std::sqrt(2.0 * R * t / (m / 1000.0));
}

double maxwell_boltzmann(double v, double t, double m) noexcept {
    const double a = (m / 1000.0) / (2.0 * R * t);
    return 4.0 * PI * std::pow(a / PI, 1.5) * v * v * std::exp(-a * v * v);
}

double maxwell_boltzmann_fraction_above(double v, double t, double m) noexcept {
    const double x = v / most_probable_speed(t, m);
    return std::erfc(x) + 2.0 * x / std::sqrt(PI) * std::exp(-x * x);
}

double maxwell_boltzmann_energy(double e, double t) noexcept {
    const double kt = K * t;
    return 2.0 * std::sqrt(e / PI) * std::pow(1.0 / kt, 1.5) * std::exp(-e / kt);
}

double average_kinetic_energy(double t) noexcept {
    return 1.5 * K * t;
}

double molar_kinetic_energy(double t) noexcept {
    return 1.5 * R * t;
}

double number_density(double p, double t) noexcept {
    return p / (K * t);
}

double mean_free_path(double p, double t, double d) noexcept {
    return K * t / (std::sqrt(2.0) * PI * d * d * p);
}

double collision_frequency(double p, double t, double d, double m) noexcept {
    return mean_speed(t, m) / mean_free_path(p, t, d);
}

double wall_collision_flux(double p, double t, double m) noexcept {
    return 0.25 * number_density(p, t) * mean_speed(t, m);
}

double effusion_rate_ratio(double ma, double mb) noexcept {
    return std::sqrt(mb / ma);
}

double graham_molar_mass(double known, double rate_known, double rate_unknown) noexcept {
    const double r = rate_known / rate_unknown;
    return known * r * r;
}

double partial_pressure(double x, double total) noexcept {
    return x * total;
}

std::vector<double> partial_pressures(const std::vector<double>& moles, double total) {
    double n = 0.0;
    for (double v : moles) n += v;
    std::vector<double> out;
    for (double v : moles) out.push_back(total * v / n);
    return out;
}

double average_molar_mass(const std::vector<double>& x, const std::vector<double>& m) {
    if (x.size() != m.size()) throw std::invalid_argument("fizmo::chemistry: one molar mass per mole fraction");
    double s = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) s += x[i] * m[i];
    return s;
}

double barometric_pressure(double p0, double h, double t, double m, double g) noexcept {
    return p0 * std::exp(-(m / 1000.0) * g * h / (R * t));
}

double collision_integral_viscosity(double ts) noexcept {
    return 1.16145 / std::pow(ts, 0.14874) + 0.52487 * std::exp(-0.77320 * ts) + 2.16178 * std::exp(-2.43787 * ts);
}

double collision_integral_diffusion(double ts) noexcept {
    return 1.06036 / std::pow(ts, 0.15610) + 0.19300 * std::exp(-0.47635 * ts) + 1.03587 * std::exp(-1.52996 * ts) + 1.76474 * std::exp(-3.89411 * ts);
}

double chapman_enskog_viscosity(double t, double m, double sigma, double eps_k) noexcept {
    const double omega = collision_integral_viscosity(t / eps_k);
    return 26.69 * std::sqrt(m * t) / (sigma * sigma * omega) * 1e-7;
}

double chapman_enskog_diffusion(double t, double p, double ma, double mb, double sa, double sb, double ea, double eb) noexcept {
    const double sigma = 0.5 * (sa + sb);
    const double eps = std::sqrt(ea * eb);
    const double omega = collision_integral_diffusion(t / eps);
    const double p_atm = p / ATMOSPHERE;
    return 0.0018583 * std::sqrt(t * t * t * (1.0 / ma + 1.0 / mb)) / (p_atm * sigma * sigma * omega) * 1e-4;
}

double eucken_thermal_conductivity(double viscosity, double cv, double m) noexcept {
    return viscosity * (cv + 2.25 * R) / (m / 1000.0);
}

double gas_viscosity(const Substance& s, double t) noexcept {
    if (!has_value(s.lennard_jones_sigma) || !has_value(s.lennard_jones_epsilon_k)) return NO_VALUE;
    return chapman_enskog_viscosity(t, s.molar_mass, s.lennard_jones_sigma, s.lennard_jones_epsilon_k);
}

double gas_diffusion(const Substance& a, const Substance& b, double t, double p) noexcept {
    if (!has_value(a.lennard_jones_sigma) || !has_value(b.lennard_jones_sigma)) return NO_VALUE;
    return chapman_enskog_diffusion(t, p, a.molar_mass, b.molar_mass, a.lennard_jones_sigma, b.lennard_jones_sigma, a.lennard_jones_epsilon_k, b.lennard_jones_epsilon_k);
}

double gas_thermal_conductivity(const Substance& s, double t) noexcept {
    const double cp = s.heat_capacity(Phase::Gas, t);
    if (!has_value(cp)) return NO_VALUE;
    return eucken_thermal_conductivity(gas_viscosity(s, t), cp - R, s.molar_mass);
}

} // namespace chemistry
} // namespace fizmo
