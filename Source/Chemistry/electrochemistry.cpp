#include "electrochemistry.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;
const double F = FARADAY_CONSTANT;

} // namespace

GalvanicCell GalvanicCell::from_couples(std::string_view a, std::string_view b) {
    const ReductionPotentialEntry* ea = find_reduction_potential(a);
    const ReductionPotentialEntry* eb = find_reduction_potential(b);
    if (!ea || !eb) throw std::invalid_argument("fizmo::chemistry: unknown redox couple");
    GalvanicCell c;
    c.cathode = ea->potential >= eb->potential ? ea : eb;
    c.anode = ea->potential >= eb->potential ? eb : ea;
    return c;
}

int GalvanicCell::electrons() const noexcept {
    const int a = anode->electrons;
    const int c = cathode->electrons;
    return a / std::gcd(a, c) * c;
}

double GalvanicCell::standard_gibbs() const noexcept {
    return gibbs_from_potential(standard_potential(), electrons());
}

double GalvanicCell::equilibrium_constant(double t) const noexcept {
    return equilibrium_constant_from_potential(standard_potential(), electrons(), t);
}

double GalvanicCell::potential(double q, double t) const noexcept {
    return nernst(standard_potential(), electrons(), q, t);
}

std::string GalvanicCell::notation() const {
    return std::string(anode->couple) + " || " + cathode->couple;
}

double nernst(double e0, int n, double q, double t) noexcept {
    return e0 - R * t / (n * F) * std::log(q);
}

double nernst_slope(int n, double t) noexcept {
    return R * t * LN10 / (n * F);
}

double gibbs_from_potential(double e, int n) noexcept {
    return -n * F * e;
}

double potential_from_gibbs(double dg, int n) noexcept {
    return -dg / (n * F);
}

double equilibrium_constant_from_potential(double e, int n, double t) noexcept {
    return std::exp(n * F * e / (R * t));
}

double potential_from_equilibrium_constant(double k, int n, double t) noexcept {
    return R * t / (n * F) * std::log(k);
}

double combine_half_potentials(double e1, int n1, double e2, int n2) noexcept {
    return (n1 * e1 + n2 * e2) / (n1 + n2);
}

double charge(double current, double seconds) noexcept {
    return current * seconds;
}

double moles_of_electrons(double coulombs) noexcept {
    return coulombs / F;
}

double electrolysis_moles(double current, double seconds, int n) noexcept {
    return current * seconds / (n * F);
}

double electrolysis_mass(double current, double seconds, int n, double m) noexcept {
    return electrolysis_moles(current, seconds, n) * m;
}

double electrolysis_time(double mass_g, double m, int n, double current) noexcept {
    return mass_g / m * n * F / current;
}

double ph_dependent_potential(double e0, int n, int protons, double ph, double t) noexcept {
    return e0 - nernst_slope(n, t) * protons * ph;
}

double concentration_cell_potential(int n, double dilute, double concentrated, double t) noexcept {
    return R * t / (n * F) * std::log(concentrated / dilute);
}

double cell_energy(double e, double q) noexcept {
    return e * q;
}

double overpotential_tafel(double j, double j0, double alpha, int n, double t) noexcept {
    return R * t / (alpha * n * F) * std::log(j / j0);
}

double butler_volmer(double eta, double j0, double aa, double ac, int n, double t) noexcept {
    const double f = n * F / (R * t);
    return j0 * (std::exp(aa * f * eta) - std::exp(-ac * f * eta));
}

} // namespace chemistry
} // namespace fizmo
