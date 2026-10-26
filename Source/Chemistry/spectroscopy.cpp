#include "spectroscopy.hpp"

#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

const double H = PLANCK_CONSTANT;
const double C = SPEED_OF_LIGHT;

double mass_factor(double nuclear_mass_u) {
    if (!(nuclear_mass_u < INFINITE_TIME)) return 1.0;
    return 1.0 / (1.0 + ELECTRON_MASS_U / nuclear_mass_u);
}

} // namespace

double photon_energy_from_wavelength(double l) noexcept {
    return H * C / l;
}

double photon_energy_from_frequency(double f) noexcept {
    return H * f;
}

double photon_wavelength(double e) noexcept {
    return H * C / e;
}

double photon_frequency(double e) noexcept {
    return e / H;
}

double photon_momentum(double l) noexcept {
    return H / l;
}

double de_broglie_wavelength(double m, double v) noexcept {
    return H / (m * v);
}

double hydrogen_like_energy(int z, int n, double nuclear_mass_u) noexcept {
    return -RYDBERG_ENERGY_EV * ELECTRONVOLT * mass_factor(nuclear_mass_u) * z * z / (static_cast<double>(n) * n);
}

double hydrogen_like_radius(int z, int n, double nuclear_mass_u) noexcept {
    return BOHR_RADIUS / mass_factor(nuclear_mass_u) * n * n / z;
}

double reduced_rydberg(double nuclear_mass_u) noexcept {
    return RYDBERG_CONSTANT * mass_factor(nuclear_mass_u);
}

double rydberg_wavelength(int z, int lower, int upper, double nuclear_mass_u) noexcept {
    const double inv = reduced_rydberg(nuclear_mass_u) * z * z * (1.0 / (static_cast<double>(lower) * lower) - 1.0 / (static_cast<double>(upper) * upper));
    return 1.0 / inv;
}

double beer_lambert_absorbance(double eps, double path, double c) noexcept {
    return eps * path * c;
}

double transmittance(double a) noexcept {
    return std::pow(10.0, -a);
}

double absorbance_from_transmittance(double t) noexcept {
    return -std::log10(t);
}

double wien_peak_wavelength(double t) noexcept {
    return 2.897771955e-3 / t;
}

double planck_spectral_radiance(double l, double t) noexcept {
    return 2.0 * H * C * C / std::pow(l, 5.0) / (std::exp(H * C / (l * BOLTZMANN_CONSTANT * t)) - 1.0);
}

double rotational_constant(double mu_u, double r) noexcept {
    const double inertia = mu_u * ATOMIC_MASS_CONSTANT * r * r;
    return H / (8.0 * PI * PI * C * inertia) / 100.0;
}

double rotational_energy(int j, double b) noexcept {
    return b * j * (j + 1) * 100.0 * H * C;
}

double vibrational_energy(int v, double w, double x) noexcept {
    const double n = v + 0.5;
    return (w * n - w * x * n * n) * 100.0 * H * C;
}

double boltzmann_population_ratio(double gap, double t, double g) noexcept {
    return g * std::exp(-gap / (BOLTZMANN_CONSTANT * t));
}

} // namespace chemistry
} // namespace fizmo
