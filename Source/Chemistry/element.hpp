#ifndef FIZMO_ELEMENT_CLASS_HPP
#define FIZMO_ELEMENT_CLASS_HPP

#include "../Basic/basic_includes.hpp"
#include "../Basic/fizmo_defines.hpp"
#include "../Basic/constants.hpp"
#include "../Util Hpp/util_functions.hpp"
#include "../Standard Overloads/span.hpp"

namespace fizmo {
namespace chemistry {

class Nuclide {
public:
    enum class Parity : std::int8_t {
        POSITIVE = 1,
        NEGATIVE = -1
    };

    const std::uint16_t atomic_number;
    const std::uint16_t num_neutrons;
    const bool is_common_isotope;
    const bool is_stable;
    const double half_life_seconds;
    const double nuclear_spin;
    const Parity parity;
    const double abundance_proportion;
    const double nucleus_mass;
    const double nuclear_binding;

    constexpr Nuclide(
        const std::uint16_t atomic_number_,
        const std::uint16_t num_neutrons_,
        const double nuclear_spin_,
        const Parity parity_,
        const double abundance,
        const double nucleus_mass_,
        const double nuclear_binding_,
        const bool is_common_isotope_ = false,
        const bool is_stable_ = true,
        const double half_life_seconds_ = 0.0
    ) noexcept
        : atomic_number(atomic_number_),
          num_neutrons(num_neutrons_),
          nuclear_spin(nuclear_spin_),
          parity(parity_),
          is_common_isotope(is_common_isotope_),
          is_stable(is_stable_),
          half_life_seconds(half_life_seconds_),
          abundance_proportion(abundance),
          nucleus_mass(nucleus_mass_),
          nuclear_binding(nuclear_binding_)
    {}

    constexpr std::uint16_t mass_number() const noexcept { return atomic_number + num_neutrons; }
    constexpr std::uint16_t proton_count() const noexcept { return atomic_number; }
    constexpr std::uint16_t neutron_count() const noexcept { return num_neutrons; }
    constexpr double nucleon_binding_energy() const noexcept { return nuclear_binding / mass_number(); }
    constexpr double binding_mass_defect() const noexcept { return nuclear_binding * fizmo::constants::RECIPROCAL_C_SQUARED<double>; }
    constexpr double nucleon_binding_mass_defect() const noexcept { return nucleon_binding_energy() * fizmo::constants::RECIPROCAL_C_SQUARED<double>; }
    constexpr double total_proton_mass() const noexcept { return atomic_number * fizmo::constants::proton_mass_amu(); }
    constexpr double total_neutron_mass() const noexcept { return num_neutrons * fizmo::constants::neutron_mass_amu(); }
    constexpr double approximate_nucleus_mass() const noexcept { return total_proton_mass() + total_neutron_mass() - binding_mass_defect(); }
    constexpr double exact_nucleus_mass() const noexcept { return nucleus_mass; }

    constexpr double total_electron_mass(const std::int16_t charge = 0) const noexcept {
        const std::int16_t electrons = static_cast<std::int16_t>(atomic_number) - charge;
        return (electrons > 0 ? electrons : 0) * fizmo::constants::electron_mass_amu();
    }

    constexpr double approximate_atomic_mass(const std::int16_t charge = 0) const noexcept { return approximate_nucleus_mass() + total_electron_mass(charge); }
    constexpr double exact_atomic_mass(const std::int16_t charge = 0) const noexcept { return nucleus_mass + total_electron_mass(charge); }

public:
    constexpr bool operator==(const Nuclide& other) const noexcept { return atomic_number == other.atomic_number && num_neutrons == other.num_neutrons; }
    constexpr bool operator!=(const Nuclide& other) const noexcept { return !(*this == other); }
};

enum class Block : std::uint8_t { 
    S = 0, 
    P, 
    D, 
    F 
};

enum class Classification : std::uint8_t { 
    METAL = 0, 
    NONMETAL, 
    METALLOID 
};

enum class Category : std::uint8_t {
    ALKALI_METAL = 0,
    ALKALINE_EARTH_METAL,
    TRANSITION_METAL,
    POST_TRANSITION_METAL,
    METALLOID,
    REACTIVE_NONMETAL,
    HALOGEN,
    NOBLE_GAS,
    LANTHANIDE,
    ACTINIDE,
    SYNTHETIC
};

class Element {
public:
    const std::uint16_t atomic_number;
    const constexpr_string symbol;
    const constexpr_string name;
    const double molar_mass;
    const constexpr_string electron_config;
    const constexpr_string noble_gas_electron_config;
    const std::uint8_t period;
    const std::uint8_t group;
    const Block block;
    const std::uint8_t valence_electrons;
    const Classification classification;
    const Category category;
    const bool is_diatomic;
    const bool has_stable_isotopes;

private:
    Span<const Nuclide> isotopes_;

public:
    constexpr Element(
        const std::uint16_t atomic_number_,
        const constexpr_string symbol_,
        const constexpr_string name_,
        const constexpr_string elec_config,
        const constexpr_string ng_elec_config,
        const double molar_mass_,
        const std::uint8_t per,
        const std::uint8_t gro,
        const Block block_,
        const std::uint8_t val_elec,
        const Classification class_,
        const Category category_,
        Span<const Nuclide> isotopes,
        const bool is_diatomic_ = false,
        const bool has_stable_isotopes_ = true
    ) noexcept
        : atomic_number(atomic_number_),
          symbol(symbol_),
          name(name_),
          molar_mass(molar_mass_),
          electron_config(elec_config),
          noble_gas_electron_config(ng_elec_config),
          period(per),
          group(gro),
          block(block_),
          valence_electrons(val_elec),
          classification(class_),
          category(category_),
          is_diatomic(is_diatomic_),
          has_stable_isotopes(has_stable_isotopes_),
          isotopes_(isotopes)
    {}

public:
    constexpr Span<const Nuclide> isotopes() const noexcept { return isotopes_; }
    constexpr std::size_t isotope_count() const noexcept { return isotopes_.size(); }
    constexpr const Nuclide* get_isotope(const std::size_t index) const noexcept { return index < isotopes_.size() ? &isotopes_[index] : nullptr; }
    constexpr const Nuclide& common_isotope() const noexcept { return isotopes_[common_isotope_index()]; }

    constexpr std::size_t common_isotope_index() const noexcept {
        for (std::size_t i = 0; i < isotopes_.size(); ++i) { if (isotopes_[i].is_common_isotope) return i; }
        return 0;
    }

    constexpr const Nuclide* find_by_mass_number(const std::uint16_t mass_num) const noexcept {
        for (std::size_t i = 0; i < isotopes_.size(); ++i) { if (isotopes_[i].mass_number() == mass_num) return &isotopes_[i]; }
        return nullptr;
    }

    constexpr const Nuclide* find_by_neutrons(const std::uint16_t neutrons) const noexcept {
        for (std::size_t i = 0; i < isotopes_.size(); ++i) { if (isotopes_[i].num_neutrons == neutrons) return &isotopes_[i]; }
        return nullptr;
    }

    constexpr const Nuclide& most_stable_isotope() const noexcept {
        if (!has_stable_isotopes) {
            std::size_t best = 0;
            for (std::size_t i = 1; i < isotopes_.size(); ++i) {
                if (isotopes_[i].half_life_seconds > isotopes_[best].half_life_seconds) { best = i; }
            }
            return isotopes_[best];
        }

        std::size_t stable_count = 0;
        std::size_t first_stable = 0;

        for (std::size_t i = 0; i < isotopes_.size(); ++i) {
            if (isotopes_[i].is_stable) {
                if (stable_count == 0) first_stable = i;
                ++stable_count;
            }
        }
        return (stable_count == 1) ? isotopes_[first_stable] : common_isotope();
    }

    std::vector<Nuclide> stable_isotopes() const {
        std::vector<Nuclide> result;
        for (std::size_t i = 0; i < isotopes_.size(); ++i) {
            if (isotopes_[i].is_stable) result.push_back(isotopes_[i]);
        }
        return result;
    }

    std::vector<Nuclide> unstable_isotopes() const {
        std::vector<Nuclide> result;
        for (std::size_t i = 0; i < isotopes_.size(); ++i) {
            if (!isotopes_[i].is_stable) result.push_back(isotopes_[i]);
        }
        return result;
    }

    constexpr double approximate_molar_mass() const noexcept {
        double sum = 0.0;
        for (std::size_t i = 0; i < isotopes_.size(); ++i) {
            sum += isotopes_[i].abundance_proportion * isotopes_[i].approximate_atomic_mass();
        }
        return sum;
    }

    constexpr double exact_molar_mass() const noexcept {
        double sum = 0.0;
        for (std::size_t i = 0; i < isotopes_.size(); ++i) {
            sum += isotopes_[i].abundance_proportion * isotopes_[i].exact_atomic_mass();
        }
        return sum;
    }

    constexpr std::uint16_t num_electrons(std::int16_t charge = 0) const noexcept {
        fizmo::clamp_value(charge, static_cast<std::int16_t>(-5), static_cast<std::int16_t>(atomic_number));
        return static_cast<std::uint16_t>(static_cast<std::int16_t>(atomic_number) - charge);
    }

public:
    constexpr bool operator==(const Element& e) const noexcept { return atomic_number == e.atomic_number; }
    constexpr bool operator!=(const Element& e) const noexcept { return atomic_number != e.atomic_number; }
    constexpr bool operator<(const Element& e) const noexcept { return atomic_number < e.atomic_number; }
    constexpr bool operator>(const Element& e) const noexcept { return atomic_number > e.atomic_number; }
    constexpr bool operator<=(const Element& e) const noexcept { return atomic_number <= e.atomic_number; }
    constexpr bool operator>=(const Element& e) const noexcept { return atomic_number >= e.atomic_number; }

    std::string to_string() const {
        return std::string(name) + "-" + std::to_string(common_isotope().mass_number());
    }
};

inline std::ostream& operator<<(std::ostream& os, const Element& element) {
    os << element.name << "-" << element.common_isotope().mass_number();
    return os;
}

inline std::string block_to_string(Block block) {
    switch (block) {
        case Block::S: return "s";
        case Block::P: return "p";
        case Block::D: return "d";
        case Block::F: return "f";
        default: return "unknown";
    }
}

inline std::string classification_to_string(Classification c) {
    switch (c) {
        case Classification::METAL: return "metal";
        case Classification::NONMETAL: return "nonmetal";
        case Classification::METALLOID: return "metalloid";
        default: return "unknown";
    }
}

inline std::string category_to_string(Category cat) {
    switch (cat) {
        case Category::ACTINIDE: return "actinide";
        case Category::ALKALI_METAL: return "alkali metal";
        case Category::ALKALINE_EARTH_METAL: return "alkaline earth metal";
        case Category::HALOGEN: return "halogen";
        case Category::LANTHANIDE: return "lanthanide";
        case Category::METALLOID: return "metalloid";
        case Category::NOBLE_GAS: return "noble gas";
        case Category::POST_TRANSITION_METAL: return "post-transition metal";
        case Category::REACTIVE_NONMETAL: return "reactive nonmetal";
        case Category::TRANSITION_METAL: return "transition metal";
        case Category::SYNTHETIC: return "synthetic";
        default: return "unknown";
    }
}

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_ELEMENT_CLASS_HPP