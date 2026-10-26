#ifndef FIZMO_CHEMISTRY_BONDS_HPP
#define FIZMO_CHEMISTRY_BONDS_HPP

#include "periodic_table.hpp"
#include "reference_types.hpp"
#include "Data/reference_data.hpp"

#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class BondOrder : std::uint8_t {
    Single = 1,
    Double = 2,
    Triple = 3,
    Aromatic = 4
};

constexpr double numeric_bond_order(BondOrder o) noexcept {
    return o == BondOrder::Aromatic ? 1.5 : static_cast<double>(static_cast<int>(o));
}

enum class BondLengthMethod : std::uint8_t {
    Table,
    Pyykko,
    SchomakerStevenson,
    Cordero
};

enum class BondCharacter : std::uint8_t {
    NonpolarCovalent,
    PolarCovalent,
    Ionic,
    Metallic
};

constexpr const char* to_string(BondCharacter c) noexcept {
    switch (c) {
        case BondCharacter::NonpolarCovalent: return "nonpolar covalent";
        case BondCharacter::PolarCovalent: return "polar covalent";
        case BondCharacter::Ionic: return "ionic";
        default: return "metallic";
    }
}

struct Bond {
    const Element* a = nullptr;
    const Element* b = nullptr;
    BondOrder order = BondOrder::Single;

    Bond() = default;
    Bond(const Element& first, const Element& second, BondOrder o = BondOrder::Single) : a(&first), b(&second), order(o) {}
    Bond(std::string_view first, std::string_view second, BondOrder o = BondOrder::Single);

    double energy() const;
    double length(BondLengthMethod method = BondLengthMethod::Table) const;
    double electronegativity_difference() const noexcept;
    double ionic_character() const noexcept;
    double dipole_moment() const;
    double reduced_mass() const noexcept;
    BondCharacter character() const noexcept;
    std::string to_string() const;
};

constexpr Span<BondEnergyEntry> bond_energy_table() noexcept {
    return Span<BondEnergyEntry>(data::BOND_ENERGIES, sizeof(data::BOND_ENERGIES) / sizeof(data::BOND_ENERGIES[0]));
}

constexpr Span<BondLengthEntry> bond_length_table() noexcept {
    return Span<BondLengthEntry>(data::BOND_LENGTHS, sizeof(data::BOND_LENGTHS) / sizeof(data::BOND_LENGTHS[0]));
}

constexpr Span<MadelungEntry> madelung_table() noexcept {
    return Span<MadelungEntry>(data::MADELUNG_CONSTANTS, sizeof(data::MADELUNG_CONSTANTS) / sizeof(data::MADELUNG_CONSTANTS[0]));
}

constexpr double tabulated_bond_energy(int z1, int z2, int order = 1) noexcept {
    for (const BondEnergyEntry& e : bond_energy_table()) {
        if (e.order == order && ((e.a == z1 && e.b == z2) || (e.a == z2 && e.b == z1))) return e.energy;
    }
    return NO_VALUE;
}

constexpr double tabulated_bond_length(int z1, int z2, int order = 1) noexcept {
    for (const BondLengthEntry& e : bond_length_table()) {
        if (e.order == order && ((e.a == z1 && e.b == z2) || (e.a == z2 && e.b == z1))) return e.length;
    }
    return NO_VALUE;
}

double bond_energy(const Element& a, const Element& b, BondOrder order = BondOrder::Single);
double pauling_bond_energy(const Element& a, const Element& b);
double bond_length(const Element& a, const Element& b, BondOrder order = BondOrder::Single, BondLengthMethod method = BondLengthMethod::Table);
double covalent_radius(const Element& e, BondOrder order = BondOrder::Single);
double bond_order_from_length(double single_bond_length_pm, double length_pm);
double length_from_bond_order(double single_bond_length_pm, double bond_order);
double percent_ionic_character(double electronegativity_difference) noexcept;
double hannay_smyth_ionic_character(double electronegativity_difference) noexcept;
double bond_dipole(double partial_charge, double length_pm) noexcept;
double reaction_enthalpy_from_bonds(const std::vector<std::pair<Bond, double>>& broken, const std::vector<std::pair<Bond, double>>& formed);
double reduced_mass(double mass1_u, double mass2_u) noexcept;
double van_der_waals_contact(const Element& a, const Element& b) noexcept;

struct MorsePotential {
    double well_depth = NO_VALUE;
    double width = NO_VALUE;
    double equilibrium_distance = NO_VALUE;

    static MorsePotential from_bond(const Bond& bond, double force_constant = NO_VALUE);
    static MorsePotential from_force_constant(double well_depth_j_per_mol, double force_constant, double equilibrium_m);
    double energy(double r) const noexcept;
    double force(double r) const noexcept;
    double force_constant() const noexcept;
    double vibrational_wavenumber(double reduced_mass_u) const noexcept;
    double vibrational_level(int v, double reduced_mass_u) const noexcept;
    int bound_levels(double reduced_mass_u) const noexcept;
};

struct LennardJonesPotential {
    double epsilon = NO_VALUE;
    double sigma = NO_VALUE;

    double energy(double r) const noexcept;
    double force(double r) const noexcept;
    double minimum_distance() const noexcept;
    double mix_epsilon(const LennardJonesPotential& o) const noexcept;
    double mix_sigma(const LennardJonesPotential& o) const noexcept;
};

double vibrational_frequency(double force_constant, double reduced_mass_u) noexcept;
double force_constant_from_wavenumber(double wavenumber_per_cm, double reduced_mass_u) noexcept;
double gordy_force_constant(const Element& a, const Element& b, double length_pm, double bond_order = 1.0) noexcept;

double madelung_constant(std::string_view structure) noexcept;
double born_exponent(const Element& cation, int cation_charge, const Element& anion, int anion_charge);
double born_lande_energy(double madelung, int cation_charge, int anion_charge, double distance_pm, double born_n) noexcept;
double born_mayer_energy(double madelung, int cation_charge, int anion_charge, double distance_pm, double rho_pm = 34.5) noexcept;
double kapustinskii_energy(int ions_per_formula, int cation_charge, int anion_charge, double cation_radius_pm, double anion_radius_pm) noexcept;
double kapustinskii_energy(const Element& cation, int cation_charge, int cation_count, const Element& anion, int anion_charge, int anion_count);
double born_haber_lattice_energy(double formation_enthalpy, double metal_atomization, double ionization_energies, double nonmetal_atomization, double electron_affinities) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_BONDS_HPP
