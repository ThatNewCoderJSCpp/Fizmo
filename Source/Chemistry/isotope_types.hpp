#ifndef FIZMO_CHEMISTRY_ISOTOPE_TYPES_HPP
#define FIZMO_CHEMISTRY_ISOTOPE_TYPES_HPP

#include "element_types.hpp"

#include <cstdint>
#include <string>

namespace fizmo {
namespace chemistry {

enum class DecayMode : std::uint8_t {
    BetaMinus,
    BetaPlus,
    ElectronCapture,
    Positron,
    Alpha,
    Proton,
    TwoProtons,
    ThreeProtons,
    Neutron,
    TwoNeutrons,
    ThreeNeutrons,
    FourNeutrons,
    SpontaneousFission,
    IsomericTransition,
    DoubleBetaMinus,
    DoubleBetaPlus,
    BetaMinusNeutron,
    BetaMinusTwoNeutrons,
    BetaMinusThreeNeutrons,
    BetaMinusFourNeutrons,
    BetaMinusProton,
    BetaMinusDeuteron,
    BetaMinusTriton,
    BetaMinusAlpha,
    BetaMinusFission,
    BetaPlusProton,
    BetaPlusTwoProtons,
    BetaPlusThreeProtons,
    BetaPlusAlpha,
    BetaPlusProtonAlpha,
    BetaPlusFission,
    ClusterEmission
};

enum class Relation : std::uint8_t {
    Exact,
    Approximate,
    LessThan,
    GreaterThan,
    Unknown
};

constexpr const char* to_string(DecayMode m) noexcept {
    switch (m) {
        case DecayMode::BetaMinus: return "beta-";
        case DecayMode::BetaPlus: return "beta+";
        case DecayMode::ElectronCapture: return "electron capture";
        case DecayMode::Positron: return "positron emission";
        case DecayMode::Alpha: return "alpha";
        case DecayMode::Proton: return "proton emission";
        case DecayMode::TwoProtons: return "two-proton emission";
        case DecayMode::ThreeProtons: return "three-proton emission";
        case DecayMode::Neutron: return "neutron emission";
        case DecayMode::TwoNeutrons: return "two-neutron emission";
        case DecayMode::ThreeNeutrons: return "three-neutron emission";
        case DecayMode::FourNeutrons: return "four-neutron emission";
        case DecayMode::SpontaneousFission: return "spontaneous fission";
        case DecayMode::IsomericTransition: return "isomeric transition";
        case DecayMode::DoubleBetaMinus: return "double beta-";
        case DecayMode::DoubleBetaPlus: return "double beta+";
        case DecayMode::BetaMinusNeutron: return "beta- delayed neutron";
        case DecayMode::BetaMinusTwoNeutrons: return "beta- delayed two neutrons";
        case DecayMode::BetaMinusThreeNeutrons: return "beta- delayed three neutrons";
        case DecayMode::BetaMinusFourNeutrons: return "beta- delayed four neutrons";
        case DecayMode::BetaMinusProton: return "beta- delayed proton";
        case DecayMode::BetaMinusDeuteron: return "beta- delayed deuteron";
        case DecayMode::BetaMinusTriton: return "beta- delayed triton";
        case DecayMode::BetaMinusAlpha: return "beta- delayed alpha";
        case DecayMode::BetaMinusFission: return "beta- delayed fission";
        case DecayMode::BetaPlusProton: return "beta+ delayed proton";
        case DecayMode::BetaPlusTwoProtons: return "beta+ delayed two protons";
        case DecayMode::BetaPlusThreeProtons: return "beta+ delayed three protons";
        case DecayMode::BetaPlusAlpha: return "beta+ delayed alpha";
        case DecayMode::BetaPlusProtonAlpha: return "beta+ delayed proton and alpha";
        case DecayMode::BetaPlusFission: return "beta+ delayed fission";
        default: return "cluster emission";
    }
}

constexpr const char* decay_symbol(DecayMode m) noexcept {
    switch (m) {
        case DecayMode::BetaMinus: return "B-";
        case DecayMode::BetaPlus: return "B+";
        case DecayMode::ElectronCapture: return "EC";
        case DecayMode::Positron: return "e+";
        case DecayMode::Alpha: return "A";
        case DecayMode::Proton: return "p";
        case DecayMode::TwoProtons: return "2p";
        case DecayMode::ThreeProtons: return "3p";
        case DecayMode::Neutron: return "n";
        case DecayMode::TwoNeutrons: return "2n";
        case DecayMode::ThreeNeutrons: return "3n";
        case DecayMode::FourNeutrons: return "4n";
        case DecayMode::SpontaneousFission: return "SF";
        case DecayMode::IsomericTransition: return "IT";
        case DecayMode::DoubleBetaMinus: return "2B-";
        case DecayMode::DoubleBetaPlus: return "2B+";
        case DecayMode::BetaMinusNeutron: return "B-n";
        case DecayMode::BetaMinusTwoNeutrons: return "B-2n";
        case DecayMode::BetaMinusThreeNeutrons: return "B-3n";
        case DecayMode::BetaMinusFourNeutrons: return "B-4n";
        case DecayMode::BetaMinusProton: return "B-p";
        case DecayMode::BetaMinusDeuteron: return "B-d";
        case DecayMode::BetaMinusTriton: return "B-t";
        case DecayMode::BetaMinusAlpha: return "B-A";
        case DecayMode::BetaMinusFission: return "B-SF";
        case DecayMode::BetaPlusProton: return "B+p";
        case DecayMode::BetaPlusTwoProtons: return "B+2p";
        case DecayMode::BetaPlusThreeProtons: return "B+3p";
        case DecayMode::BetaPlusAlpha: return "B+A";
        case DecayMode::BetaPlusProtonAlpha: return "B+pA";
        case DecayMode::BetaPlusFission: return "B+SF";
        default: return "CE";
    }
}

struct NuclideChange {
    int delta_z = 0;
    int delta_a = 0;
    bool defined = true;
};

constexpr NuclideChange nuclide_change(DecayMode m, int cluster_z = 0, int cluster_a = 0) noexcept {
    switch (m) {
        case DecayMode::BetaMinus: return { 1, 0, true };
        case DecayMode::BetaPlus:
        case DecayMode::ElectronCapture:
        case DecayMode::Positron: return { -1, 0, true };
        case DecayMode::Alpha: return { -2, -4, true };
        case DecayMode::Proton: return { -1, -1, true };
        case DecayMode::TwoProtons: return { -2, -2, true };
        case DecayMode::ThreeProtons: return { -3, -3, true };
        case DecayMode::Neutron: return { 0, -1, true };
        case DecayMode::TwoNeutrons: return { 0, -2, true };
        case DecayMode::ThreeNeutrons: return { 0, -3, true };
        case DecayMode::FourNeutrons: return { 0, -4, true };
        case DecayMode::IsomericTransition: return { 0, 0, true };
        case DecayMode::DoubleBetaMinus: return { 2, 0, true };
        case DecayMode::DoubleBetaPlus: return { -2, 0, true };
        case DecayMode::BetaMinusNeutron: return { 1, -1, true };
        case DecayMode::BetaMinusTwoNeutrons: return { 1, -2, true };
        case DecayMode::BetaMinusThreeNeutrons: return { 1, -3, true };
        case DecayMode::BetaMinusFourNeutrons: return { 1, -4, true };
        case DecayMode::BetaMinusProton: return { 0, -1, true };
        case DecayMode::BetaMinusDeuteron: return { 0, -2, true };
        case DecayMode::BetaMinusTriton: return { 0, -3, true };
        case DecayMode::BetaMinusAlpha: return { -1, -4, true };
        case DecayMode::BetaPlusProton: return { -2, -1, true };
        case DecayMode::BetaPlusTwoProtons: return { -3, -2, true };
        case DecayMode::BetaPlusThreeProtons: return { -4, -3, true };
        case DecayMode::BetaPlusAlpha: return { -3, -4, true };
        case DecayMode::BetaPlusProtonAlpha: return { -4, -5, true };
        case DecayMode::ClusterEmission: return { -cluster_z, -cluster_a, true };
        default: return { 0, 0, false };
    }
}

struct DecayBranch {
    DecayMode mode = DecayMode::Alpha;
    double percent = NO_VALUE;
    Relation relation = Relation::Unknown;
    bool sub_branch = false;
    std::uint8_t cluster_z = 0;
    std::uint8_t cluster_a = 0;

    constexpr bool percent_known() const noexcept { return percent == percent; }
    constexpr double fraction() const noexcept { return percent / 100.0; }
    constexpr NuclideChange change() const noexcept { return nuclide_change(mode, cluster_z, cluster_a); }
    constexpr bool has_daughter() const noexcept { return change().defined; }
    constexpr const char* name() const noexcept { return to_string(mode); }
    constexpr const char* symbol() const noexcept { return decay_symbol(mode); }
};

struct Isotope {
    std::uint8_t atomic_number = 0;
    std::uint16_t mass_number = 0;
    double mass = NO_VALUE;
    double mass_uncertainty = NO_VALUE;
    double abundance = NO_VALUE;
    double abundance_uncertainty = NO_VALUE;
    bool is_stable = false;
    double half_life = NO_VALUE;
    double half_life_uncertainty = NO_VALUE;
    std::int8_t twice_spin = -1;
    std::int8_t parity = 0;
    double g_factor = NO_VALUE;
    double quadrupole_moment = NO_VALUE;
    std::uint16_t decay_offset = 0;
    std::uint8_t decay_count = 0;
    std::int16_t discovery_year = 0;

    constexpr int protons() const noexcept { return atomic_number; }
    constexpr int neutrons() const noexcept { return static_cast<int>(mass_number) - atomic_number; }
    constexpr int nucleons() const noexcept { return mass_number; }
    constexpr bool is_radioactive() const noexcept { return !is_stable; }
    constexpr bool is_natural() const noexcept { return abundance == abundance && abundance > 0.0; }
    constexpr bool half_life_known() const noexcept { return half_life == half_life; }
    constexpr bool has_spin() const noexcept { return twice_spin >= 0; }
    constexpr double spin() const noexcept { return twice_spin >= 0 ? twice_spin / 2.0 : NO_VALUE; }
    constexpr bool is_nmr_active() const noexcept { return twice_spin > 0; }
    constexpr bool has_quadrupole_moment() const noexcept { return twice_spin > 1; }
    constexpr double magnetic_moment() const noexcept { return (g_factor == g_factor && twice_spin >= 0) ? g_factor * twice_spin / 2.0 : NO_VALUE; }
    constexpr double gyromagnetic_ratio() const noexcept { return g_factor * NUCLEAR_MAGNETON / REDUCED_PLANCK_CONSTANT; }
    constexpr double molar_mass() const noexcept { return mass; }
    constexpr double exact_molar_mass() const noexcept { return mass * MOLAR_MASS_CONSTANT * 1000.0; }
    constexpr double mass_kg() const noexcept { return mass * ATOMIC_MASS_CONSTANT; }
    constexpr double mass_excess() const noexcept { return (mass - mass_number) * ATOMIC_MASS_UNIT_MEV; }
    constexpr double binding_energy() const noexcept {
        return (atomic_number * HYDROGEN_ATOM_MASS_U + neutrons() * NEUTRON_MASS_U - mass) * ATOMIC_MASS_UNIT_MEV;
    }
    constexpr double binding_energy_per_nucleon() const noexcept { return mass_number > 0 ? binding_energy() / mass_number : NO_VALUE; }
    constexpr double decay_constant() const noexcept {
        return is_stable ? 0.0 : (half_life == half_life && half_life > 0.0 ? LN2 / half_life : NO_VALUE);
    }
    constexpr double mean_lifetime() const noexcept { return is_stable ? INFINITE_TIME : half_life / LN2; }
    constexpr double specific_activity() const noexcept {
        return is_stable ? 0.0 : decay_constant() * AVOGADRO_CONSTANT / molar_mass();
    }

    double nuclear_mass() const noexcept;
    double electron_binding_energy() const noexcept;
    double nuclear_radius() const noexcept;
    double remaining_fraction(double seconds) const noexcept;
    double activity(double moles) const noexcept;
    double time_to_fraction(double fraction) const noexcept;

    constexpr Span<DecayBranch> decays() const noexcept;
    constexpr const DecayBranch* primary_decay() const noexcept;
    constexpr const Element& element() const noexcept;
    constexpr bool operator==(const Isotope& o) const noexcept { return atomic_number == o.atomic_number && mass_number == o.mass_number; }
    constexpr bool operator!=(const Isotope& o) const noexcept { return !(*this == o); }

    std::string name() const;
    std::string symbol() const;
    std::string notation() const;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ISOTOPE_TYPES_HPP
