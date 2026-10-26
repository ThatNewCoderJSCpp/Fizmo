#include "atom.hpp"

#include <cmath>
#include <cstdio>

namespace fizmo {
namespace chemistry {

namespace {

const char* const SUPERSCRIPT_DIGITS[] = { "\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4", "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9" };
const char* const SUBSCRIPT_DIGITS[] = { "\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83", "\xE2\x82\x84", "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87", "\xE2\x82\x88", "\xE2\x82\x89" };

std::string roman_numeral(int value) {
    static const int values[] = { 10, 9, 5, 4, 1 };
    static const char* const symbols[] = { "X", "IX", "V", "IV", "I" };
    std::string out;
    for (int i = 0; i < 5; ++i) {
        while (value >= values[i]) {
            out += symbols[i];
            value -= values[i];
        }
    }
    return out;
}

std::string lower_name(const Element& e) {
    std::string s = e.name;
    for (char& c : s) c = ascii_lower(c);
    return s;
}

double emitted_mass(DecayMode mode, const Isotope& parent) noexcept {
    const double h1 = HYDROGEN_ATOM_MASS_U;
    const double n = NEUTRON_MASS_U;
    const Isotope* he4 = PeriodicTable::find_isotope(2, 4);
    const Isotope* h2 = PeriodicTable::find_isotope(1, 2);
    const Isotope* h3 = PeriodicTable::find_isotope(1, 3);
    const double alpha = he4 ? he4->mass : NO_VALUE;
    switch (mode) {
        case DecayMode::Alpha: return alpha;
        case DecayMode::Proton: return h1;
        case DecayMode::TwoProtons: return 2 * h1;
        case DecayMode::ThreeProtons: return 3 * h1;
        case DecayMode::Neutron: return n;
        case DecayMode::TwoNeutrons: return 2 * n;
        case DecayMode::ThreeNeutrons: return 3 * n;
        case DecayMode::FourNeutrons: return 4 * n;
        case DecayMode::Positron: return 2 * ELECTRON_MASS_U;
        case DecayMode::BetaMinusNeutron: return n;
        case DecayMode::BetaMinusTwoNeutrons: return 2 * n;
        case DecayMode::BetaMinusThreeNeutrons: return 3 * n;
        case DecayMode::BetaMinusFourNeutrons: return 4 * n;
        case DecayMode::BetaMinusProton: return h1;
        case DecayMode::BetaMinusDeuteron: return h2 ? h2->mass : NO_VALUE;
        case DecayMode::BetaMinusTriton: return h3 ? h3->mass : NO_VALUE;
        case DecayMode::BetaMinusAlpha: return alpha;
        case DecayMode::BetaPlusProton: return h1;
        case DecayMode::BetaPlusTwoProtons: return 2 * h1;
        case DecayMode::BetaPlusThreeProtons: return 3 * h1;
        case DecayMode::BetaPlusAlpha: return alpha;
        case DecayMode::BetaPlusProtonAlpha: return h1 + alpha;
        default: break;
    }
    (void)parent;
    return 0.0;
}

} // namespace

std::string format_signed(int value, bool sign_after) {
    if (value == 0) return "0";
    const std::string digits = std::to_string(value < 0 ? -value : value);
    const char sign = value < 0 ? '-' : '+';
    return sign_after ? digits + sign : std::string(1, sign) + digits;
}

std::string superscript(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (is_ascii_digit(c)) out += SUPERSCRIPT_DIGITS[c - '0'];
        else if (c == '+') out += "\xE2\x81\xBA";
        else if (c == '-') out += "\xE2\x81\xBB";
        else if (c == '(') out += "\xE2\x81\xBD";
        else if (c == ')') out += "\xE2\x81\xBE";
        else out += c;
    }
    return out;
}

std::string subscript(std::string_view text) {
    std::string out;
    for (char c : text) {
        if (is_ascii_digit(c)) out += SUBSCRIPT_DIGITS[c - '0'];
        else if (c == '+') out += "\xE2\x82\x8A";
        else if (c == '-') out += "\xE2\x82\x8B";
        else if (c == '(') out += "\xE2\x82\x8D";
        else if (c == ')') out += "\xE2\x82\x8E";
        else out += c;
    }
    return out;
}

std::string Subshell::to_string() const {
    return std::to_string(n) + orbital_letter(l) + std::to_string(electrons);
}

std::string TermSymbol::to_string() const {
    if (!known()) return std::string();
    std::string out = std::to_string(multiplicity);
    out += l_letter();
    if (odd_parity) out += 'o';
    if (twice_j >= 0) out += (twice_j % 2 == 0) ? std::to_string(twice_j / 2) : std::to_string(twice_j) + "/2";
    return out;
}

std::string Element::full_electron_configuration() const {
    return ElectronConfiguration::ground_state(*this).to_string();
}

std::string Element::shell_configuration() const {
    return ElectronConfiguration::ground_state(*this).shell_string();
}

double Isotope::electron_binding_energy() const noexcept {
    const double z = atomic_number;
    return 14.4381 * std::pow(z, 2.39) + 1.55468e-6 * std::pow(z, 5.35);
}

double Isotope::nuclear_mass() const noexcept {
    return mass - atomic_number * ELECTRON_MASS_U + electron_binding_energy() / (ATOMIC_MASS_UNIT_MEV * 1e6);
}

double Isotope::nuclear_radius() const noexcept {
    return 1.2 * std::cbrt(static_cast<double>(mass_number));
}

double Isotope::remaining_fraction(double seconds) const noexcept {
    if (is_stable) return 1.0;
    const double lambda = decay_constant();
    return has_value(lambda) ? std::exp(-lambda * seconds) : NO_VALUE;
}

double Isotope::activity(double moles) const noexcept {
    return decay_constant() * moles * AVOGADRO_CONSTANT;
}

double Isotope::time_to_fraction(double fraction) const noexcept {
    if (is_stable || fraction <= 0.0 || fraction > 1.0) return is_stable ? INFINITE_TIME : NO_VALUE;
    return -std::log(fraction) / decay_constant();
}

std::string Isotope::name() const {
    return std::string(element().name) + "-" + std::to_string(mass_number);
}

std::string Isotope::symbol() const {
    return std::string(element().symbol) + "-" + std::to_string(mass_number);
}

std::string Isotope::notation() const {
    return superscript(std::to_string(mass_number)) + element().symbol;
}

double decay_q_value(const Isotope& parent, const DecayBranch& branch) noexcept {
    if (branch.mode == DecayMode::IsomericTransition || branch.mode == DecayMode::SpontaneousFission ||
        branch.mode == DecayMode::BetaMinusFission || branch.mode == DecayMode::BetaPlusFission) {
        return NO_VALUE;
    }
    const Isotope* d = daughter(parent, branch);
    if (!d) return NO_VALUE;
    double emitted = emitted_mass(branch.mode, parent);
    if (branch.mode == DecayMode::ClusterEmission) {
        const Isotope* c = PeriodicTable::find_isotope(branch.cluster_z, branch.cluster_a);
        emitted = c ? c->mass : NO_VALUE;
    }
    return (parent.mass - d->mass - emitted) * ATOMIC_MASS_UNIT_MEV;
}

std::vector<const Element*> PeriodicTable::in_period(int period) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.period == period) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> PeriodicTable::in_group(int group) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.group == group) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> PeriodicTable::in_block(Block block) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.block == block) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> PeriodicTable::in_category(Category category) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.category == category) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> PeriodicTable::with_classification(Classification classification) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.classification == classification) out.push_back(&e);
    }
    return out;
}

std::vector<const Element*> PeriodicTable::in_phase(Phase phase) {
    std::vector<const Element*> out;
    for (const Element& e : elements()) {
        if (e.standard_state == phase) out.push_back(&e);
    }
    return out;
}

std::vector<const Isotope*> PeriodicTable::stable_isotopes() {
    std::vector<const Isotope*> out;
    for (const Isotope& i : isotopes()) {
        if (i.is_stable) out.push_back(&i);
    }
    return out;
}

std::vector<const Isotope*> PeriodicTable::natural_isotopes() {
    std::vector<const Isotope*> out;
    for (const Isotope& i : isotopes()) {
        if (i.is_natural()) out.push_back(&i);
    }
    return out;
}

std::vector<const Isotope*> PeriodicTable::isotones(int neutrons) {
    std::vector<const Isotope*> out;
    for (const Isotope& i : isotopes()) {
        if (i.neutrons() == neutrons) out.push_back(&i);
    }
    return out;
}

std::vector<const Isotope*> PeriodicTable::isobars(int mass_number) {
    std::vector<const Isotope*> out;
    for (const Isotope& i : isotopes()) {
        if (i.mass_number == mass_number) out.push_back(&i);
    }
    return out;
}

int PeriodicTable::table_row(const Element& e) noexcept {
    return e.group == 0 ? e.period + 3 : e.period;
}

int PeriodicTable::table_column(const Element& e) noexcept {
    if (e.group != 0) return e.group;
    return 3 + (e.period == 6 ? e.atomic_number - 57 : e.atomic_number - 89);
}

std::string OxidationState::to_string() const {
    if (!known()) return "?";
    return format_signed(m_value);
}

std::string OxidationState::roman() const {
    if (!known()) return "?";
    if (m_value == 0) return "0";
    return (m_value < 0 ? "-" : "") + roman_numeral(m_value < 0 ? -m_value : m_value);
}

std::string OxidationState::stock_notation(std::string_view symbol_or_name) const {
    return std::string(symbol_or_name) + "(" + roman() + ")";
}

std::string OxidationState::superscript() const {
    return chemistry::superscript(to_string());
}

std::string AverageOxidationState::to_string() const {
    if (m_denominator == 1) return format_signed(m_numerator);
    return format_signed(m_numerator) + "/" + std::to_string(m_denominator);
}

Atom Atom::parse(std::string_view text) {
    text = trim(text);
    std::size_t i = 0;
    if (i < text.size() && text[i] == '^') ++i;
    if (i < text.size() && text[i] == '{') ++i;
    int mass_number = 0;
    while (i < text.size() && is_ascii_digit(text[i])) mass_number = mass_number * 10 + (text[i++] - '0');
    if (i < text.size() && text[i] == '}') ++i;
    if (i >= text.size() || !is_ascii_upper(text[i])) throw std::invalid_argument("fizmo::chemistry: expected an element symbol");
    std::size_t start = i++;
    if (i < text.size() && is_ascii_lower(text[i])) ++i;
    std::string_view sym = text.substr(start, i - start);
    const Element* e = nullptr;
    const bool hydrogen_label = sym == "D" || sym == "T";
    if (sym == "D") {
        e = &PeriodicTable::element(1);
        mass_number = 2;
    } else if (sym == "T") {
        e = &PeriodicTable::element(1);
        mass_number = 3;
    } else {
        e = PeriodicTable::find_symbol(sym);
        if (!e || sym != e->symbol) {
            if (sym.size() == 2) {
                --i;
                sym = text.substr(start, 1);
                e = PeriodicTable::find_symbol(sym);
            }
        }
    }
    if (!e || (!hydrogen_label && sym != e->symbol)) throw std::invalid_argument("fizmo::chemistry: unknown element symbol");
    std::string_view rest = trim(text.substr(i));
    if (rest.size() > 1 && rest[0] == '-' && is_ascii_digit(rest[1])) {
        std::size_t k = 1;
        int a = 0;
        while (k < rest.size() && is_ascii_digit(rest[k])) a = a * 10 + (rest[k++] - '0');
        if (k == rest.size()) return Atom(*e, 0, a);
    }
    int charge = 0;
    if (!rest.empty()) {
        std::size_t k = 0;
        if (rest[k] == '^') ++k;
        if (k < rest.size() && rest[k] == '{') ++k;
        int magnitude = 0;
        bool digits = false;
        int sign = 0;
        if (k < rest.size() && (rest[k] == '+' || rest[k] == '-')) {
            sign = rest[k] == '+' ? 1 : -1;
            ++k;
        }
        while (k < rest.size() && is_ascii_digit(rest[k])) {
            magnitude = magnitude * 10 + (rest[k++] - '0');
            digits = true;
        }
        if (sign == 0) {
            if (k < rest.size() && (rest[k] == '+' || rest[k] == '-')) {
                sign = rest[k] == '+' ? 1 : -1;
                ++k;
            } else {
                throw std::invalid_argument("fizmo::chemistry: charge needs a sign");
            }
        }
        while (k < rest.size() && (rest[k] == '+' || rest[k] == '-') && !digits) {
            magnitude += 1;
            ++k;
        }
        if (k < rest.size() && rest[k] == '}') ++k;
        if (k != rest.size()) throw std::invalid_argument("fizmo::chemistry: unexpected text after the charge");
        charge = sign * (digits ? magnitude : (magnitude > 0 ? magnitude + 1 : 1));
    }
    return Atom(*e, charge, mass_number);
}

std::string Atom::to_string() const {
    std::string out;
    if (m_mass_number != 0) out += std::to_string(m_mass_number);
    out += m_element->symbol;
    if (m_charge != 0) {
        const int a = m_charge < 0 ? -m_charge : m_charge;
        if (a != 1) out += std::to_string(a);
        out += m_charge < 0 ? '-' : '+';
    }
    return out;
}

std::string Atom::to_unicode_string() const {
    std::string out;
    if (m_mass_number != 0) out += superscript(std::to_string(m_mass_number));
    out += m_element->symbol;
    if (m_charge != 0) {
        const int a = m_charge < 0 ? -m_charge : m_charge;
        std::string c = a != 1 ? std::to_string(a) : std::string();
        c += m_charge < 0 ? '-' : '+';
        out += superscript(c);
    }
    return out;
}

std::string Atom::stock_name() const {
    const Element& e = *m_element;
    if (m_charge < 0) {
        switch (e.atomic_number) {
            case 1: return "hydride";
            case 5: return "boride";
            case 6: return "carbide";
            case 7: return "nitride";
            case 8: return m_charge == -2 ? "oxide" : "oxide(" + format_signed(m_charge) + ")";
            case 9: return "fluoride";
            case 14: return "silicide";
            case 15: return "phosphide";
            case 16: return "sulfide";
            case 17: return "chloride";
            case 32: return "germanide";
            case 33: return "arsenide";
            case 51: return "antimonide";
            case 84: return "polonide";
            case 34: return "selenide";
            case 35: return "bromide";
            case 52: return "telluride";
            case 53: return "iodide";
            case 85: return "astatide";
            default: return lower_name(e) + "ide";
        }
    }
    if (m_charge == 0) return lower_name(e);
    const int z = e.atomic_number;
    const bool single_valence = e.group == 1 || e.group == 2 || z == 13 || z == 21 || z == 30 || z == 31 || z == 39 || z == 47 || z == 48 || z == 57;
    if (!single_valence || !e.is_common_oxidation_state(m_charge)) return lower_name(e) + "(" + roman_numeral(m_charge) + ")";
    return lower_name(e);
}

} // namespace chemistry
} // namespace fizmo
