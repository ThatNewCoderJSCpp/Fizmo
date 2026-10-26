#ifndef FIZMO_CHEMISTRY_PERIODIC_TABLE_HPP
#define FIZMO_CHEMISTRY_PERIODIC_TABLE_HPP

#include "element_types.hpp"
#include "isotope_types.hpp"
#include "Data/element_data.hpp"
#include "Data/isotope_data.hpp"
#include "Data/ionic_radius_data.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

inline constexpr int ELEMENT_COUNT = static_cast<int>(sizeof(data::ELEMENTS) / sizeof(data::ELEMENTS[0]));
inline constexpr int ISOTOPE_COUNT = static_cast<int>(sizeof(data::ISOTOPES) / sizeof(data::ISOTOPES[0]));
inline constexpr int IONIC_RADIUS_COUNT = static_cast<int>(sizeof(data::IONIC_RADII) / sizeof(data::IONIC_RADII[0]));

constexpr Span<Subshell> Element::subshells() const noexcept {
    return Span<Subshell>(data::SUBSHELLS + subshell_offset, subshell_count);
}

constexpr int Element::outermost_shell() const noexcept {
    int n = 0;
    for (const Subshell& s : subshells()) {
        if (s.electrons > 0 && s.n > n) n = s.n;
    }
    return n;
}

constexpr int Element::electrons_in_shell(int n) const noexcept {
    int total = 0;
    for (const Subshell& s : subshells()) {
        if (s.n == n) total += s.electrons;
    }
    return total;
}

constexpr int Element::electrons_in_subshell(int n, int l) const noexcept {
    for (const Subshell& s : subshells()) {
        if (s.n == n && s.l == l) return s.electrons;
    }
    return 0;
}

constexpr int Element::valence_electrons() const noexcept {
    const int n = outermost_shell();
    int count = electrons_in_shell(n);
    if (block == Block::D) {
        count = electrons_in_subshell(n, 0) + electrons_in_subshell(n, 1) + electrons_in_subshell(n - 1, 2);
    } else if (block == Block::F) {
        count = electrons_in_subshell(n, 0) + electrons_in_subshell(n, 1) + electrons_in_subshell(n - 1, 2) + electrons_in_subshell(n - 2, 3);
    }
    if (group == 18 && atomic_number != 2) count = 8;
    return count;
}

constexpr int Element::unpaired_electrons() const noexcept {
    int total = 0;
    for (const Subshell& s : subshells()) total += s.unpaired();
    return total;
}

constexpr Span<double> Element::ionization_energies() const noexcept {
    return Span<double>(data::IONIZATION_ENERGIES + ionization_offset, ionization_count);
}

constexpr double Element::ionization_energy(int n) const noexcept {
    return (n >= 1 && n <= ionization_count) ? data::IONIZATION_ENERGIES[ionization_offset + n - 1] : NO_VALUE;
}

constexpr double Element::ionization_energy_uncertainty(int n) const noexcept {
    return (n >= 1 && n <= ionization_count) ? data::IONIZATION_ENERGY_UNCERTAINTIES[ionization_offset + n - 1] : NO_VALUE;
}

constexpr double Element::cumulative_ionization_energy(int charge) const noexcept {
    if (charge == 0) return 0.0;
    if (charge < 0) return charge == -1 ? -electron_affinity : NO_VALUE;
    double total = 0.0;
    for (int k = 1; k <= charge; ++k) total += ionization_energy(k);
    return total;
}

constexpr double Element::mulliken_electronegativity() const noexcept {
    return (ionization_energy(1) + electron_affinity) / 2.0;
}

constexpr Span<OxidationStateEntry> Element::oxidation_states() const noexcept {
    return Span<OxidationStateEntry>(data::OXIDATION_STATES + oxidation_offset, oxidation_count);
}

constexpr bool Element::has_oxidation_state(int state) const noexcept {
    if (state == 0) return true;
    for (const OxidationStateEntry& e : oxidation_states()) {
        if (e.state == state) return true;
    }
    return false;
}

constexpr bool Element::is_common_oxidation_state(int state) const noexcept {
    for (const OxidationStateEntry& e : oxidation_states()) {
        if (e.state == state) return e.common;
    }
    return false;
}

constexpr int Element::highest_oxidation_state() const noexcept {
    int best = 0;
    for (const OxidationStateEntry& e : oxidation_states()) {
        if (e.state > best) best = e.state;
    }
    return best;
}

constexpr int Element::lowest_oxidation_state() const noexcept {
    int best = 0;
    for (const OxidationStateEntry& e : oxidation_states()) {
        if (e.state < best) best = e.state;
    }
    return best;
}

constexpr int Element::most_common_oxidation_state() const noexcept {
    int best = 0;
    bool found = false;
    const bool negative = !is_metal() && electronegativity_pauling >= 2.5;
    for (const OxidationStateEntry& e : oxidation_states()) {
        if (!e.common) continue;
        bool better = !found;
        if (found) {
            if (is_metal()) {
                better = (e.state > 0 && (best <= 0 || e.state < best));
            } else if (negative) {
                better = e.state < best;
            } else {
                better = e.state > best;
            }
        }
        if (better) {
            best = e.state;
            found = true;
        }
    }
    return best;
}

constexpr Span<Isotope> Element::isotopes() const noexcept {
    return Span<Isotope>(data::ISOTOPES + isotope_offset, isotope_count);
}

constexpr const Isotope* Element::isotope(int mass_number_) const noexcept {
    for (const Isotope& i : isotopes()) {
        if (i.mass_number == mass_number_) return &i;
    }
    return nullptr;
}

constexpr const Isotope* Element::isotope_with_neutrons(int neutron_count) const noexcept {
    return isotope(atomic_number + neutron_count);
}

constexpr const Isotope* Element::reference_isotope() const noexcept {
    return isotope(reference_mass_number);
}

constexpr const Isotope* Element::most_abundant_isotope() const noexcept {
    return most_abundant_mass_number ? isotope(most_abundant_mass_number) : nullptr;
}

constexpr int Element::stable_isotope_count() const noexcept {
    int n = 0;
    for (const Isotope& i : isotopes()) n += i.is_stable ? 1 : 0;
    return n;
}

constexpr int Element::natural_isotope_count() const noexcept {
    int n = 0;
    for (const Isotope& i : isotopes()) n += i.is_natural() ? 1 : 0;
    return n;
}

constexpr Span<IonicRadius> Element::ionic_radii() const noexcept {
    int first = -1;
    int count = 0;
    for (int i = 0; i < IONIC_RADIUS_COUNT; ++i) {
        if (data::IONIC_RADII[i].atomic_number == atomic_number) {
            if (first < 0) first = i;
            ++count;
        } else if (first >= 0) {
            break;
        }
    }
    return first < 0 ? Span<IonicRadius>() : Span<IonicRadius>(data::IONIC_RADII + first, static_cast<std::size_t>(count));
}

constexpr const IonicRadius* Element::ionic_radius(int charge, int coordination) const noexcept {
    const IonicRadius* best = nullptr;
    int best_score = 1 << 30;
    for (const IonicRadius& r : ionic_radii()) {
        if (r.charge != charge) continue;
        int diff = r.coordination_number() - coordination;
        if (diff < 0) diff = -diff;
        int score = diff * 100;
        if (r.spin == SpinState::Low) score += 10;
        if (r.is_square_planar() || r.is_pyramidal()) score += 5;
        if (!r.most_reliable) score += 1;
        if (score < best_score) {
            best_score = score;
            best = &r;
        }
    }
    return best;
}

constexpr Span<DecayBranch> Isotope::decays() const noexcept {
    return Span<DecayBranch>(data::DECAY_BRANCHES + decay_offset, decay_count);
}

constexpr const DecayBranch* Isotope::primary_decay() const noexcept {
    const DecayBranch* best = nullptr;
    for (const DecayBranch& b : decays()) {
        if (b.sub_branch) continue;
        if (!best || (b.percent_known() && (!best->percent_known() || b.percent > best->percent))) best = &b;
    }
    return best;
}

constexpr const Element& Isotope::element() const noexcept {
    return data::ELEMENTS[atomic_number - 1];
}

class PeriodicTable {
public:
    static constexpr int size() noexcept { return ELEMENT_COUNT; }

    static constexpr Span<Element> elements() noexcept { return Span<Element>(data::ELEMENTS, ELEMENT_COUNT); }
    static constexpr Span<Isotope> isotopes() noexcept { return Span<Isotope>(data::ISOTOPES, ISOTOPE_COUNT); }
    static constexpr Span<IonicRadius> ionic_radii() noexcept { return Span<IonicRadius>(data::IONIC_RADII, IONIC_RADIUS_COUNT); }

    static constexpr bool valid(int atomic_number) noexcept { return atomic_number >= 1 && atomic_number <= ELEMENT_COUNT; }

    static constexpr const Element& element(int atomic_number) {
        return valid(atomic_number) ? data::ELEMENTS[atomic_number - 1] : throw std::out_of_range("fizmo::chemistry: no element with that atomic number");
    }

    static constexpr const Element& element(std::string_view symbol_or_name) {
        const Element* e = find(symbol_or_name);
        return e ? *e : throw std::invalid_argument("fizmo::chemistry: unknown element");
    }

    static constexpr const Element* find(int atomic_number) noexcept {
        return valid(atomic_number) ? &data::ELEMENTS[atomic_number - 1] : nullptr;
    }

    static constexpr const Element* find_symbol(std::string_view symbol) noexcept {
        symbol = trim(symbol);
        for (const Element& e : elements()) {
            if (symbol == e.symbol) return &e;
        }
        for (const Element& e : elements()) {
            if (equals_ignore_case(symbol, e.symbol)) return &e;
        }
        if (symbol == "D" || symbol == "T") return &data::ELEMENTS[0];
        return nullptr;
    }

    static constexpr const Element* find_name(std::string_view name) noexcept {
        name = trim(name);
        for (const Element& e : elements()) {
            if (equals_ignore_case(name, e.name)) return &e;
        }
        if (equals_ignore_case(name, "aluminium")) return &data::ELEMENTS[12];
        if (equals_ignore_case(name, "caesium")) return &data::ELEMENTS[54];
        if (equals_ignore_case(name, "sulphur")) return &data::ELEMENTS[15];
        if (equals_ignore_case(name, "deuterium") || equals_ignore_case(name, "tritium") || equals_ignore_case(name, "protium")) return &data::ELEMENTS[0];
        return nullptr;
    }

    static constexpr const Element* find(std::string_view symbol_or_name) noexcept {
        const Element* e = find_symbol(symbol_or_name);
        if (e) return e;
        e = find_name(symbol_or_name);
        if (e) return e;
        symbol_or_name = trim(symbol_or_name);
        int z = 0;
        if (symbol_or_name.empty()) return nullptr;
        for (char c : symbol_or_name) {
            if (!is_ascii_digit(c)) return nullptr;
            z = z * 10 + (c - '0');
            if (z > 1000) return nullptr;
        }
        return find(z);
    }

    static constexpr const Isotope* find_isotope(int atomic_number, int mass_number) noexcept {
        const Element* e = find(atomic_number);
        return e ? e->isotope(mass_number) : nullptr;
    }

    static constexpr const Isotope* find_isotope(std::string_view text) noexcept {
        text = trim(text);
        if (text == "D") return find_isotope(1, 2);
        if (text == "T") return find_isotope(1, 3);
        if (equals_ignore_case(text, "deuterium")) return find_isotope(1, 2);
        if (equals_ignore_case(text, "tritium")) return find_isotope(1, 3);
        if (equals_ignore_case(text, "protium")) return find_isotope(1, 1);
        std::size_t i = 0;
        if (!text.empty() && text[0] == '^') ++i;
        int leading = 0;
        std::size_t digits_start = i;
        while (i < text.size() && is_ascii_digit(text[i])) leading = leading * 10 + (text[i++] - '0');
        if (i > digits_start) {
            const Element* e = find_symbol(text.substr(i));
            return e ? e->isotope(leading) : nullptr;
        }
        std::size_t sep = text.find_last_of("- ");
        if (sep == std::string_view::npos) {
            std::size_t d = text.size();
            while (d > 0 && is_ascii_digit(text[d - 1])) --d;
            if (d == 0 || d == text.size()) return nullptr;
            sep = d;
            const Element* e = find(text.substr(0, sep));
            int a = 0;
            for (std::size_t k = sep; k < text.size(); ++k) a = a * 10 + (text[k] - '0');
            return e ? e->isotope(a) : nullptr;
        }
        const Element* e = find(text.substr(0, sep));
        int a = 0;
        std::string_view tail = trim(text.substr(sep + 1));
        if (tail.empty()) return nullptr;
        for (char c : tail) {
            if (!is_ascii_digit(c)) return nullptr;
            a = a * 10 + (c - '0');
        }
        return e ? e->isotope(a) : nullptr;
    }

    static constexpr const Element* at_position(int period, int group) noexcept {
        for (const Element& e : elements()) {
            if (e.period == period && e.group == group) return &e;
        }
        return nullptr;
    }

    static std::vector<const Element*> in_period(int period);
    static std::vector<const Element*> in_group(int group);
    static std::vector<const Element*> in_block(Block block);
    static std::vector<const Element*> in_category(Category category);
    static std::vector<const Element*> with_classification(Classification classification);
    static std::vector<const Element*> in_phase(Phase phase);
    static std::vector<const Isotope*> stable_isotopes();
    static std::vector<const Isotope*> natural_isotopes();
    static std::vector<const Isotope*> isotones(int neutrons);
    static std::vector<const Isotope*> isobars(int mass_number);
    static int table_row(const Element& e) noexcept;
    static int table_column(const Element& e) noexcept;
};

constexpr const Element& element(int atomic_number) { return PeriodicTable::element(atomic_number); }
constexpr const Element& element(std::string_view symbol_or_name) { return PeriodicTable::element(symbol_or_name); }
constexpr const Element* find_element(std::string_view symbol_or_name) noexcept { return PeriodicTable::find(symbol_or_name); }
constexpr const Element* find_element(int atomic_number) noexcept { return PeriodicTable::find(atomic_number); }
constexpr const Isotope* find_isotope(int atomic_number, int mass_number) noexcept { return PeriodicTable::find_isotope(atomic_number, mass_number); }
constexpr const Isotope* find_isotope(std::string_view text) noexcept { return PeriodicTable::find_isotope(text); }

constexpr const Isotope& isotope(int atomic_number, int mass_number) {
    const Isotope* i = PeriodicTable::find_isotope(atomic_number, mass_number);
    return i ? *i : throw std::invalid_argument("fizmo::chemistry: unknown isotope");
}

constexpr const Isotope& isotope(std::string_view text) {
    const Isotope* i = PeriodicTable::find_isotope(text);
    return i ? *i : throw std::invalid_argument("fizmo::chemistry: unknown isotope");
}

constexpr const Isotope* daughter(const Isotope& parent, const DecayBranch& branch) noexcept {
    const NuclideChange c = branch.change();
    if (!c.defined) return nullptr;
    return PeriodicTable::find_isotope(parent.atomic_number + c.delta_z, parent.mass_number + c.delta_a);
}

double decay_q_value(const Isotope& parent, const DecayBranch& branch) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_PERIODIC_TABLE_HPP
