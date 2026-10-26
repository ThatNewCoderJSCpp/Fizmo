#ifndef FIZMO_CHEMISTRY_ELECTROCHEMISTRY_HPP
#define FIZMO_CHEMISTRY_ELECTROCHEMISTRY_HPP

#include "common.hpp"
#include "constants.hpp"
#include "reference_types.hpp"
#include "Data/reference_data.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

constexpr Span<ReductionPotentialEntry> reduction_potentials() noexcept {
    return Span<ReductionPotentialEntry>(data::REDUCTION_POTENTIALS, sizeof(data::REDUCTION_POTENTIALS) / sizeof(data::REDUCTION_POTENTIALS[0]));
}

constexpr const ReductionPotentialEntry* find_reduction_potential(std::string_view couple) noexcept {
    for (const ReductionPotentialEntry& e : reduction_potentials()) {
        if (equals_ignore_case(trim(couple), e.couple)) return &e;
    }
    return nullptr;
}

constexpr double standard_reduction_potential(std::string_view couple) {
    const ReductionPotentialEntry* e = find_reduction_potential(couple);
    return e ? e->potential : throw std::invalid_argument("fizmo::chemistry: unknown redox couple");
}

struct GalvanicCell {
    const ReductionPotentialEntry* cathode = nullptr;
    const ReductionPotentialEntry* anode = nullptr;

    static GalvanicCell from_couples(std::string_view a, std::string_view b);
    double standard_potential() const noexcept { return cathode->potential - anode->potential; }
    int electrons() const noexcept;
    double standard_gibbs() const noexcept;
    double equilibrium_constant(double t = STANDARD_TEMPERATURE) const noexcept;
    double potential(double reaction_quotient, double t = STANDARD_TEMPERATURE) const noexcept;
    bool spontaneous() const noexcept { return standard_potential() > 0.0; }
    std::string notation() const;
};

double nernst(double standard_potential, int electrons, double reaction_quotient, double t = STANDARD_TEMPERATURE) noexcept;
double nernst_slope(int electrons, double t = STANDARD_TEMPERATURE) noexcept;
double gibbs_from_potential(double potential, int electrons) noexcept;
double potential_from_gibbs(double delta_g, int electrons) noexcept;
double equilibrium_constant_from_potential(double potential, int electrons, double t = STANDARD_TEMPERATURE) noexcept;
double potential_from_equilibrium_constant(double k, int electrons, double t = STANDARD_TEMPERATURE) noexcept;
double combine_half_potentials(double e1, int n1, double e2, int n2) noexcept;
double charge(double current, double seconds) noexcept;
double moles_of_electrons(double coulombs) noexcept;
double electrolysis_moles(double current, double seconds, int electrons_per_unit) noexcept;
double electrolysis_mass(double current, double seconds, int electrons_per_unit, double molar_mass_g) noexcept;
double electrolysis_time(double mass_g, double molar_mass_g, int electrons_per_unit, double current) noexcept;
double ph_dependent_potential(double standard_potential, int electrons, int protons, double ph, double t = STANDARD_TEMPERATURE) noexcept;
double concentration_cell_potential(int electrons, double dilute, double concentrated, double t = STANDARD_TEMPERATURE) noexcept;
double cell_energy(double potential, double charge_coulombs) noexcept;
double overpotential_tafel(double current_density, double exchange_current_density, double transfer_coefficient = 0.5, int electrons = 1, double t = STANDARD_TEMPERATURE) noexcept;
double butler_volmer(double overpotential, double exchange_current_density, double alpha_a = 0.5, double alpha_c = 0.5, int electrons = 1, double t = STANDARD_TEMPERATURE) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ELECTROCHEMISTRY_HPP
