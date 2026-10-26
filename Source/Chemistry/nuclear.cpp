#include "nuclear.hpp"

#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

const double AV = 15.75;
const double AS = 17.8;
const double AC = 0.711;
const double AA = 23.7;
const double AP = 11.18;

} // namespace

double semi_empirical_binding_energy(int z, int a) noexcept {
    if (a <= 0 || z < 0 || z > a) return NO_VALUE;
    const int n = a - z;
    const double ad = a;
    double pairing = 0.0;
    if (z % 2 == 0 && n % 2 == 0) pairing = AP / std::sqrt(ad);
    else if (z % 2 == 1 && n % 2 == 1) pairing = -AP / std::sqrt(ad);
    return AV * ad - AS * std::pow(ad, 2.0 / 3.0) - AC * z * (z - 1) / std::cbrt(ad) - AA * (a - 2.0 * z) * (a - 2.0 * z) / ad + pairing;
}

double semi_empirical_mass(int z, int a) noexcept {
    return z * HYDROGEN_ATOM_MASS_U + (a - z) * NEUTRON_MASS_U - semi_empirical_binding_energy(z, a) / ATOMIC_MASS_UNIT_MEV;
}

int most_stable_proton_number(int a) noexcept {
    const double z = a / (2.0 + AC * std::pow(static_cast<double>(a), 2.0 / 3.0) / (2.0 * AA));
    return static_cast<int>(std::lround(z));
}

double nuclear_radius_fm(int a, double r0) noexcept {
    return r0 * std::cbrt(static_cast<double>(a));
}

double coulomb_barrier(int z1, int a1, int z2, int a2) noexcept {
    const double r = (nuclear_radius_fm(a1) + nuclear_radius_fm(a2)) * 1e-15;
    return COULOMB_CONSTANT * z1 * z2 * ELEMENTARY_CHARGE * ELEMENTARY_CHARGE / r / ELECTRONVOLT / 1e6;
}

double mass_defect(const Isotope& i) noexcept {
    return i.atomic_number * HYDROGEN_ATOM_MASS_U + i.neutrons() * NEUTRON_MASS_U - i.mass;
}

double energy_from_mass(double m) noexcept {
    return m * ATOMIC_MASS_UNIT_MEV;
}

double reaction_q_value(const std::vector<const Isotope*>& reactants, const std::vector<const Isotope*>& products) noexcept {
    double m = 0.0;
    for (const Isotope* i : reactants) m += i ? i->mass : NO_VALUE;
    for (const Isotope* i : products) m -= i ? i->mass : NO_VALUE;
    return m * ATOMIC_MASS_UNIT_MEV;
}

double becquerel_to_curie(double bq) noexcept {
    return bq / CURIE;
}

double curie_to_becquerel(double ci) noexcept {
    return ci * CURIE;
}

double decay_remaining(double n0, double half_life, double t) noexcept {
    return n0 * std::exp(-LN2 * t / half_life);
}

double half_life_from_counts(double n0, double n, double t) noexcept {
    return LN2 * t / std::log(n0 / n);
}

double age_from_ratio(double ratio, double half_life) noexcept {
    return half_life / LN2 * std::log(1.0 + ratio);
}

std::vector<const Isotope*> decay_chain(const Isotope& start, int max_length) {
    std::vector<const Isotope*> chain{ &start };
    const Isotope* current = &start;
    for (int i = 0; i < max_length && !current->is_stable; ++i) {
        const DecayBranch* b = current->primary_decay();
        if (!b) break;
        const Isotope* next = daughter(*current, *b);
        if (!next || next == current) break;
        chain.push_back(next);
        current = next;
    }
    return chain;
}

std::vector<DecayChainLink> chain_links(const std::vector<const Isotope*>& chain) {
    std::vector<DecayChainLink> out;
    for (std::size_t i = 0; i < chain.size(); ++i) {
        double branching = 1.0;
        if (i > 0) {
            const Isotope* parent = chain[i - 1];
            branching = 0.0;
            for (const DecayBranch& b : parent->decays()) {
                if (b.sub_branch || !b.percent_known()) continue;
                if (daughter(*parent, b) == chain[i]) branching += b.fraction();
            }
            if (branching <= 0.0) branching = 1.0;
        }
        out.push_back(DecayChainLink{ chain[i], branching });
    }
    return out;
}

std::vector<double> bateman(const std::vector<double>& lambda, double n0, double t) {
    const std::size_t n = lambda.size();
    std::vector<double> out(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        double product = n0;
        for (std::size_t j = 0; j < k; ++j) product *= lambda[j];
        double sum = 0.0;
        for (std::size_t i = 0; i <= k; ++i) {
            double denom = 1.0;
            for (std::size_t j = 0; j <= k; ++j) {
                if (j != i) denom *= lambda[j] - lambda[i];
            }
            sum += std::exp(-lambda[i] * t) / denom;
        }
        out[k] = product * sum;
    }
    return out;
}

std::vector<double> bateman(const std::vector<DecayChainLink>& chain, double n0, double t) {
    std::vector<double> lambda;
    double branching = 1.0;
    for (const DecayChainLink& l : chain) lambda.push_back(l.isotope->is_stable ? 1e-300 : l.isotope->decay_constant());
    std::vector<double> out = bateman(lambda, n0, t);
    for (std::size_t k = 0; k < chain.size(); ++k) {
        if (k > 0) branching *= chain[k].branching;
        out[k] *= branching;
    }
    return out;
}

std::vector<double> chain_activities(const std::vector<DecayChainLink>& chain, double n0, double t) {
    std::vector<double> n = bateman(chain, n0, t);
    for (std::size_t k = 0; k < chain.size(); ++k) n[k] *= chain[k].isotope->is_stable ? 0.0 : chain[k].isotope->decay_constant();
    return n;
}

} // namespace chemistry
} // namespace fizmo
