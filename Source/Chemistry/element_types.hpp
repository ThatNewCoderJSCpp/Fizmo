#ifndef FIZMO_CHEMISTRY_ELEMENT_TYPES_HPP
#define FIZMO_CHEMISTRY_ELEMENT_TYPES_HPP

#include "common.hpp"
#include "constants.hpp"

#include <cstdint>
#include <string>

namespace fizmo {
namespace chemistry {

struct Isotope;

enum class Block : std::uint8_t {
    S,
    P,
    D,
    F
};

enum class Classification : std::uint8_t {
    Metal,
    Metalloid,
    Nonmetal
};

enum class Category : std::uint8_t {
    AlkaliMetal,
    AlkalineEarthMetal,
    Lanthanide,
    Actinide,
    TransitionMetal,
    PostTransitionMetal,
    Metalloid,
    ReactiveNonmetal,
    Halogen,
    NobleGas
};

enum class SpinState : std::uint8_t {
    Unspecified,
    High,
    Low
};

constexpr const char* to_string(Block b) noexcept {
    switch (b) {
        case Block::S: return "s";
        case Block::P: return "p";
        case Block::D: return "d";
        default: return "f";
    }
}

constexpr const char* to_string(Classification c) noexcept {
    switch (c) {
        case Classification::Metal: return "metal";
        case Classification::Metalloid: return "metalloid";
        default: return "nonmetal";
    }
}

constexpr const char* to_string(Category c) noexcept {
    switch (c) {
        case Category::AlkaliMetal: return "alkali metal";
        case Category::AlkalineEarthMetal: return "alkaline earth metal";
        case Category::Lanthanide: return "lanthanide";
        case Category::Actinide: return "actinide";
        case Category::TransitionMetal: return "transition metal";
        case Category::PostTransitionMetal: return "post-transition metal";
        case Category::Metalloid: return "metalloid";
        case Category::ReactiveNonmetal: return "reactive nonmetal";
        case Category::Halogen: return "halogen";
        default: return "noble gas";
    }
}

constexpr const char* group_name(int group) noexcept {
    switch (group) {
        case 1: return "alkali metals";
        case 2: return "alkaline earth metals";
        case 3: return "scandium group";
        case 4: return "titanium group";
        case 5: return "vanadium group";
        case 6: return "chromium group";
        case 7: return "manganese group";
        case 8: return "iron group";
        case 9: return "cobalt group";
        case 10: return "nickel group";
        case 11: return "coinage metals";
        case 12: return "zinc group";
        case 13: return "triels";
        case 14: return "tetrels";
        case 15: return "pnictogens";
        case 16: return "chalcogens";
        case 17: return "halogens";
        case 18: return "noble gases";
        default: return "";
    }
}

constexpr char orbital_letter(int l) noexcept {
    constexpr const char* letters = "spdfghiklmnoqrtuv";
    return (l >= 0 && l < 17) ? letters[l] : '?';
}

constexpr int orbital_index(char letter) noexcept {
    constexpr const char* letters = "spdfghiklmnoqrtuv";
    for (int i = 0; i < 17; ++i) {
        if (letters[i] == ascii_lower(letter)) return i;
    }
    return -1;
}

constexpr char shell_letter(int n) noexcept {
    constexpr const char* letters = "KLMNOPQRS";
    return (n >= 1 && n <= 9) ? letters[n - 1] : '?';
}

struct Subshell {
    std::uint8_t n = 0;
    std::uint8_t l = 0;
    std::uint8_t electrons = 0;

    constexpr int capacity() const noexcept { return 2 * (2 * l + 1); }
    constexpr int orbitals() const noexcept { return 2 * l + 1; }
    constexpr bool is_full() const noexcept { return electrons == capacity(); }
    constexpr bool is_empty() const noexcept { return electrons == 0; }
    constexpr int unpaired() const noexcept { return electrons <= orbitals() ? electrons : capacity() - electrons; }
    constexpr int madelung() const noexcept { return n + l; }
    constexpr bool operator==(const Subshell& o) const noexcept { return n == o.n && l == o.l && electrons == o.electrons; }
    constexpr bool operator!=(const Subshell& o) const noexcept { return !(*this == o); }
    std::string to_string() const;
};

struct TermSymbol {
    std::uint8_t multiplicity = 0;
    std::uint8_t orbital_l = 0;
    bool odd_parity = false;
    std::int8_t twice_j = -1;

    constexpr bool known() const noexcept { return multiplicity != 0; }
    constexpr double total_spin() const noexcept { return (multiplicity - 1) / 2.0; }
    constexpr double total_j() const noexcept { return twice_j / 2.0; }
    constexpr char l_letter() const noexcept { return ascii_upper(orbital_letter(orbital_l)); }
    constexpr bool operator==(const TermSymbol& o) const noexcept {
        return multiplicity == o.multiplicity && orbital_l == o.orbital_l && odd_parity == o.odd_parity && twice_j == o.twice_j;
    }
    constexpr bool operator!=(const TermSymbol& o) const noexcept { return !(*this == o); }
    std::string to_string() const;
};

struct OxidationStateEntry {
    std::int8_t state = 0;
    bool common = false;
};

struct IonicRadius {
    std::uint8_t atomic_number = 0;
    std::int8_t charge = 0;
    const char* coordination = "";
    SpinState spin = SpinState::Unspecified;
    double crystal_radius = NO_VALUE;
    double ionic_radius = NO_VALUE;
    bool most_reliable = false;

    constexpr int coordination_number() const noexcept {
        int value = 0;
        int last = 0;
        const char* p = coordination;
        for (; *p == 'I' || *p == 'V' || *p == 'X'; ++p) {
            int digit = *p == 'I' ? 1 : (*p == 'V' ? 5 : 10);
            value += digit;
            if (digit > last && last != 0) value -= 2 * last;
            last = digit;
        }
        return value;
    }

    constexpr bool is_square_planar() const noexcept {
        const char* p = coordination;
        while (*p) {
            if (p[0] == 'S' && p[1] == 'Q') return true;
            ++p;
        }
        return false;
    }

    constexpr bool is_pyramidal() const noexcept {
        const char* p = coordination;
        while (*p) {
            if (p[0] == 'P' && p[1] == 'Y') return true;
            ++p;
        }
        return false;
    }
};

struct Element {
    std::uint8_t atomic_number = 0;
    const char* symbol = "";
    const char* name = "";
    std::uint8_t period = 0;
    std::uint8_t group = 0;
    Block block = Block::S;
    Category category = Category::ReactiveNonmetal;
    Classification classification = Classification::Nonmetal;
    Phase standard_state = Phase::Unknown;
    bool is_stable = false;
    bool has_standard_atomic_weight = false;
    bool properties_predicted = false;
    double atomic_weight = NO_VALUE;
    double atomic_weight_uncertainty = NO_VALUE;
    double atomic_weight_min = NO_VALUE;
    double atomic_weight_max = NO_VALUE;
    std::uint16_t reference_mass_number = 0;
    std::uint16_t most_abundant_mass_number = 0;
    const char* electron_configuration = "";
    std::uint16_t subshell_offset = 0;
    std::uint8_t subshell_count = 0;
    TermSymbol ground_term{};
    double electronegativity_pauling = NO_VALUE;
    double electronegativity_allen = NO_VALUE;
    double electron_affinity = NO_VALUE;
    double electron_affinity_uncertainty = NO_VALUE;
    DataQuality electron_affinity_quality = DataQuality::Unknown;
    std::uint16_t ionization_offset = 0;
    std::uint8_t ionization_count = 0;
    std::uint16_t oxidation_offset = 0;
    std::uint8_t oxidation_count = 0;
    std::uint16_t isotope_offset = 0;
    std::uint16_t isotope_count = 0;
    double covalent_radius = NO_VALUE;
    double covalent_radius_double = NO_VALUE;
    double covalent_radius_triple = NO_VALUE;
    double covalent_radius_cordero = NO_VALUE;
    double van_der_waals_radius = NO_VALUE;
    double metallic_radius = NO_VALUE;
    double atomic_radius = NO_VALUE;
    double polarizability = NO_VALUE;
    double melting_point = NO_VALUE;
    double boiling_point = NO_VALUE;
    bool sublimes = false;
    double triple_point_temperature = NO_VALUE;
    double triple_point_pressure = NO_VALUE;
    double critical_temperature = NO_VALUE;
    double critical_pressure = NO_VALUE;
    double density = NO_VALUE;
    double fusion_heat = NO_VALUE;
    double vaporization_heat = NO_VALUE;
    double molar_heat_capacity = NO_VALUE;
    double specific_heat_capacity = NO_VALUE;
    double thermal_conductivity = NO_VALUE;
    const char* crystal_structure = "";
    double lattice_constant = NO_VALUE;
    double abundance_crust = NO_VALUE;
    double abundance_sea = NO_VALUE;
    std::uint32_t cpk_color = 0;
    std::int16_t discovery_year = 0;
    double atomization_enthalpy = NO_VALUE;

    constexpr int protons() const noexcept { return atomic_number; }
    constexpr int electrons() const noexcept { return atomic_number; }
    constexpr int neutrons() const noexcept { return static_cast<int>(reference_mass_number) - atomic_number; }
    constexpr int mass_number() const noexcept { return reference_mass_number; }
    constexpr double relative_atomic_mass() const noexcept { return atomic_weight; }
    constexpr double molar_mass() const noexcept { return atomic_weight; }
    constexpr double exact_molar_mass() const noexcept { return atomic_weight * MOLAR_MASS_CONSTANT * 1000.0; }
    constexpr double molar_mass_kg() const noexcept { return atomic_weight / 1000.0; }
    constexpr double atom_mass_kg() const noexcept { return atomic_weight * ATOMIC_MASS_CONSTANT; }

    constexpr bool is_metal() const noexcept { return classification == Classification::Metal; }
    constexpr bool is_metalloid() const noexcept { return classification == Classification::Metalloid; }
    constexpr bool is_nonmetal() const noexcept { return classification == Classification::Nonmetal; }
    constexpr bool is_radioactive() const noexcept { return !is_stable; }
    constexpr bool is_noble_gas() const noexcept { return category == Category::NobleGas; }
    constexpr bool is_halogen() const noexcept { return group == 17; }
    constexpr bool is_alkali_metal() const noexcept { return category == Category::AlkaliMetal; }
    constexpr bool is_alkaline_earth_metal() const noexcept { return category == Category::AlkalineEarthMetal; }
    constexpr bool is_transition_metal() const noexcept { return category == Category::TransitionMetal; }
    constexpr bool is_post_transition_metal() const noexcept { return category == Category::PostTransitionMetal; }
    constexpr bool is_lanthanide() const noexcept { return category == Category::Lanthanide; }
    constexpr bool is_actinide() const noexcept { return category == Category::Actinide; }
    constexpr bool is_rare_earth() const noexcept { return category == Category::Lanthanide || atomic_number == 21 || atomic_number == 39; }
    constexpr bool is_inner_transition_metal() const noexcept { return block == Block::F; }
    constexpr bool is_main_group() const noexcept { return block == Block::S || block == Block::P; }
    constexpr bool is_chalcogen() const noexcept { return group == 16; }
    constexpr bool is_pnictogen() const noexcept { return group == 15; }
    constexpr bool is_transuranic() const noexcept { return atomic_number > 92; }
    constexpr bool is_superheavy() const noexcept { return atomic_number >= 104; }
    constexpr bool is_gas_at_standard_conditions() const noexcept { return standard_state == Phase::Gas; }
    constexpr bool is_liquid_at_standard_conditions() const noexcept { return standard_state == Phase::Liquid; }
    constexpr bool is_solid_at_standard_conditions() const noexcept { return standard_state == Phase::Solid; }
    constexpr bool is_diatomic() const noexcept {
        return atomic_number == 1 || atomic_number == 7 || atomic_number == 8 || atomic_number == 9 || atomic_number == 17 || atomic_number == 35 || atomic_number == 53;
    }
    constexpr const char* group_name() const noexcept { return chemistry::group_name(group); }
    constexpr const char* block_name() const noexcept { return to_string(block); }
    constexpr const char* category_name() const noexcept { return to_string(category); }
    constexpr const char* classification_name() const noexcept { return to_string(classification); }
    constexpr bool has_electron_affinity() const noexcept { return electron_affinity == electron_affinity; }
    constexpr bool forms_stable_anion() const noexcept { return electron_affinity == electron_affinity && electron_affinity > 0.0; }
    constexpr double electron_affinity_kj_per_mol() const noexcept { return electron_affinity * EV_PER_PARTICLE_TO_KJ_PER_MOL; }
    constexpr double electronegativity() const noexcept { return electronegativity_pauling; }
    constexpr std::uint8_t cpk_red() const noexcept { return static_cast<std::uint8_t>((cpk_color >> 16) & 0xFF); }
    constexpr std::uint8_t cpk_green() const noexcept { return static_cast<std::uint8_t>((cpk_color >> 8) & 0xFF); }
    constexpr std::uint8_t cpk_blue() const noexcept { return static_cast<std::uint8_t>(cpk_color & 0xFF); }

    constexpr Span<Subshell> subshells() const noexcept;
    constexpr int outermost_shell() const noexcept;
    constexpr int electrons_in_shell(int n) const noexcept;
    constexpr int electrons_in_subshell(int n, int l) const noexcept;
    constexpr int valence_electrons() const noexcept;
    constexpr int unpaired_electrons() const noexcept;
    constexpr bool is_paramagnetic() const noexcept { return unpaired_electrons() > 0; }

    constexpr Span<double> ionization_energies() const noexcept;
    constexpr double ionization_energy(int n = 1) const noexcept;
    constexpr double ionization_energy_uncertainty(int n = 1) const noexcept;
    constexpr double ionization_energy_kj_per_mol(int n = 1) const noexcept { return ionization_energy(n) * EV_PER_PARTICLE_TO_KJ_PER_MOL; }
    constexpr double cumulative_ionization_energy(int charge) const noexcept;
    constexpr double mulliken_electronegativity() const noexcept;

    constexpr Span<OxidationStateEntry> oxidation_states() const noexcept;
    constexpr bool has_oxidation_state(int state) const noexcept;
    constexpr bool is_common_oxidation_state(int state) const noexcept;
    constexpr int highest_oxidation_state() const noexcept;
    constexpr int lowest_oxidation_state() const noexcept;
    constexpr int most_common_oxidation_state() const noexcept;

    constexpr Span<Isotope> isotopes() const noexcept;
    constexpr const Isotope* isotope(int mass_number) const noexcept;
    constexpr const Isotope* isotope_with_neutrons(int neutron_count) const noexcept;
    constexpr const Isotope* reference_isotope() const noexcept;
    constexpr const Isotope* most_abundant_isotope() const noexcept;
    constexpr int stable_isotope_count() const noexcept;
    constexpr int natural_isotope_count() const noexcept;

    constexpr Span<IonicRadius> ionic_radii() const noexcept;
    constexpr const IonicRadius* ionic_radius(int charge, int coordination = 6) const noexcept;

    std::string full_electron_configuration() const;
    std::string shell_configuration() const;

    constexpr bool operator==(const Element& o) const noexcept { return atomic_number == o.atomic_number; }
    constexpr bool operator!=(const Element& o) const noexcept { return atomic_number != o.atomic_number; }
    constexpr bool operator<(const Element& o) const noexcept { return atomic_number < o.atomic_number; }
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ELEMENT_TYPES_HPP
