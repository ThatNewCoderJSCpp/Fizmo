#ifndef FIZMO_CHEMISTRY_NUCLEAR_HPP
#define FIZMO_CHEMISTRY_NUCLEAR_HPP

#include "periodic_table.hpp"

#include <vector>

namespace fizmo {
namespace chemistry {

inline constexpr double CURIE = 3.7e10;
inline constexpr double JULIAN_YEAR = 365.25 * 86400.0;
inline constexpr double NUBASE_YEAR = 365.2422 * 86400.0;

struct DecayChainLink {
    const Isotope* isotope = nullptr;
    double branching = 1.0;
};

double semi_empirical_binding_energy(int protons, int mass_number) noexcept;
double semi_empirical_mass(int protons, int mass_number) noexcept;
int most_stable_proton_number(int mass_number) noexcept;
double nuclear_radius_fm(int mass_number, double r0 = 1.2) noexcept;
double coulomb_barrier(int z1, int a1, int z2, int a2) noexcept;
double mass_defect(const Isotope& isotope) noexcept;
double energy_from_mass(double mass_u) noexcept;
double reaction_q_value(const std::vector<const Isotope*>& reactants, const std::vector<const Isotope*>& products) noexcept;
double becquerel_to_curie(double bq) noexcept;
double curie_to_becquerel(double ci) noexcept;
double decay_remaining(double n0, double half_life, double t) noexcept;
double half_life_from_counts(double n0, double n, double t) noexcept;
double age_from_ratio(double daughter_to_parent, double half_life) noexcept;
std::vector<const Isotope*> decay_chain(const Isotope& start, int max_length = 64);
std::vector<double> bateman(const std::vector<double>& decay_constants, double n0, double t);
std::vector<double> bateman(const std::vector<DecayChainLink>& chain, double n0, double t);
std::vector<double> chain_activities(const std::vector<DecayChainLink>& chain, double n0, double t);
std::vector<DecayChainLink> chain_links(const std::vector<const Isotope*>& chain);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_NUCLEAR_HPP
