#include "electron_configuration.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const int NOBLE_GAS_NUMBERS[] = { 2, 10, 18, 36, 54, 86, 118 };

bool subshell_order(const Subshell& a, const Subshell& b) {
    return a.n != b.n ? a.n < b.n : a.l < b.l;
}

bool madelung_less(const Subshell& a, const Subshell& b) {
    return a.madelung() != b.madelung() ? a.madelung() < b.madelung() : a.n < b.n;
}

} // namespace

ElectronConfiguration::ElectronConfiguration(std::vector<Subshell> subshells) : m_subshells(std::move(subshells)) {
    normalize();
}

void ElectronConfiguration::normalize() {
    std::vector<Subshell> merged;
    for (const Subshell& s : m_subshells) {
        if (s.l >= s.n) throw std::invalid_argument("fizmo::chemistry: subshell l must be smaller than n");
        auto it = std::find_if(merged.begin(), merged.end(), [&](const Subshell& m) { return m.n == s.n && m.l == s.l; });
        if (it == merged.end()) {
            merged.push_back(s);
        } else {
            it->electrons = static_cast<std::uint8_t>(it->electrons + s.electrons);
        }
    }
    for (const Subshell& s : merged) {
        if (s.electrons > s.capacity()) throw std::invalid_argument("fizmo::chemistry: subshell holds too many electrons");
    }
    merged.erase(std::remove_if(merged.begin(), merged.end(), [](const Subshell& s) { return s.electrons == 0; }), merged.end());
    std::sort(merged.begin(), merged.end(), subshell_order);
    m_subshells = std::move(merged);
}

std::vector<std::pair<int, int>> ElectronConfiguration::madelung_order(int max_n) {
    std::vector<std::pair<int, int>> out;
    for (int sum = 1; sum <= 2 * max_n; ++sum) {
        for (int n = 1; n <= max_n; ++n) {
            const int l = sum - n;
            if (l >= 0 && l < n) out.emplace_back(n, l);
        }
    }
    return out;
}

ElectronConfiguration ElectronConfiguration::ground_state(const Element& element) {
    std::vector<Subshell> s(element.subshells().begin(), element.subshells().end());
    return ElectronConfiguration(std::move(s));
}

ElectronConfiguration ElectronConfiguration::of_ion(const Element& element, int charge) {
    if (charge > element.atomic_number) throw std::invalid_argument("fizmo::chemistry: an ion can't lose more electrons than it has");
    return ground_state(element).ionized(charge);
}

ElectronConfiguration ElectronConfiguration::aufbau(int electrons) {
    if (electrons < 0) throw std::invalid_argument("fizmo::chemistry: negative electron count");
    ElectronConfiguration c;
    for (int i = 0; i < electrons; ++i) c.add_electron();
    return c;
}

ElectronConfiguration ElectronConfiguration::parse(std::string_view text) {
    std::vector<Subshell> out;
    std::size_t i = 0;
    auto skip = [&] {
        while (i < text.size() && (text[i] == ' ' || text[i] == '.' || text[i] == ',' || text[i] == '\t')) ++i;
    };
    skip();
    if (i < text.size() && text[i] == '[') {
        const std::size_t close = text.find(']', i);
        if (close == std::string_view::npos) throw std::invalid_argument("fizmo::chemistry: unclosed noble gas core");
        const Element* core = PeriodicTable::find_symbol(text.substr(i + 1, close - i - 1));
        if (!core) throw std::invalid_argument("fizmo::chemistry: unknown noble gas core");
        for (const Subshell& s : core->subshells()) out.push_back(s);
        i = close + 1;
    }
    while (true) {
        skip();
        if (i >= text.size()) break;
        int n = 0;
        while (i < text.size() && is_ascii_digit(text[i])) n = n * 10 + (text[i++] - '0');
        if (n == 0 || i >= text.size()) throw std::invalid_argument("fizmo::chemistry: bad subshell in configuration");
        const int l = orbital_index(text[i++]);
        if (l < 0) throw std::invalid_argument("fizmo::chemistry: bad orbital letter in configuration");
        if (i < text.size() && text[i] == '^') ++i;
        int e = 0;
        bool any = false;
        while (i < text.size() && is_ascii_digit(text[i])) {
            e = e * 10 + (text[i++] - '0');
            any = true;
        }
        if (!any) e = 1;
        auto it = std::find_if(out.begin(), out.end(), [&](const Subshell& s) { return s.n == n && s.l == l; });
        if (it != out.end()) {
            it->electrons = static_cast<std::uint8_t>(e);
        } else {
            out.push_back(Subshell{ static_cast<std::uint8_t>(n), static_cast<std::uint8_t>(l), static_cast<std::uint8_t>(e) });
        }
    }
    return ElectronConfiguration(std::move(out));
}

int ElectronConfiguration::electrons() const noexcept {
    int total = 0;
    for (const Subshell& s : m_subshells) total += s.electrons;
    return total;
}

int ElectronConfiguration::occupancy(int n, int l) const noexcept {
    for (const Subshell& s : m_subshells) {
        if (s.n == n && s.l == l) return s.electrons;
    }
    return 0;
}

int ElectronConfiguration::shell_occupancy(int n) const noexcept {
    int total = 0;
    for (const Subshell& s : m_subshells) {
        if (s.n == n) total += s.electrons;
    }
    return total;
}

std::vector<int> ElectronConfiguration::shells() const {
    std::vector<int> out(static_cast<std::size_t>(highest_shell()), 0);
    for (const Subshell& s : m_subshells) out[static_cast<std::size_t>(s.n - 1)] += s.electrons;
    return out;
}

int ElectronConfiguration::highest_shell() const noexcept {
    int n = 0;
    for (const Subshell& s : m_subshells) n = std::max<int>(n, s.n);
    return n;
}

int ElectronConfiguration::unpaired_electrons() const noexcept {
    int total = 0;
    for (const Subshell& s : m_subshells) total += s.unpaired();
    return total;
}

bool ElectronConfiguration::is_closed_shell() const noexcept {
    return open_subshell_count() == 0;
}

int ElectronConfiguration::open_subshell_count() const noexcept {
    int open = 0;
    for (const Subshell& s : m_subshells) open += s.is_full() ? 0 : 1;
    return open;
}

bool ElectronConfiguration::is_noble_gas_like() const noexcept {
    const int e = electrons();
    for (int z : NOBLE_GAS_NUMBERS) {
        if (z == e) return *this == ground_state(PeriodicTable::element(z));
    }
    return e == 0;
}

bool ElectronConfiguration::add_electron() {
    for (const auto& nl : madelung_order(9)) {
        auto it = std::find_if(m_subshells.begin(), m_subshells.end(), [&](const Subshell& s) { return s.n == nl.first && s.l == nl.second; });
        if (it == m_subshells.end()) {
            m_subshells.push_back(Subshell{ static_cast<std::uint8_t>(nl.first), static_cast<std::uint8_t>(nl.second), 1 });
            std::sort(m_subshells.begin(), m_subshells.end(), subshell_order);
            return true;
        }
        if (!it->is_full()) {
            ++it->electrons;
            return true;
        }
    }
    return false;
}

bool ElectronConfiguration::remove_electron() {
    if (m_subshells.empty()) return false;
    auto it = std::max_element(m_subshells.begin(), m_subshells.end(), [](const Subshell& a, const Subshell& b) {
        return a.n != b.n ? a.n < b.n : a.l < b.l;
    });
    --it->electrons;
    if (it->electrons == 0) m_subshells.erase(it);
    return true;
}

ElectronConfiguration ElectronConfiguration::ionized(int charge) const {
    ElectronConfiguration c = *this;
    for (int i = 0; i < charge; ++i) {
        if (!c.remove_electron()) throw std::invalid_argument("fizmo::chemistry: no electrons left to remove");
    }
    for (int i = 0; i > charge; --i) c.add_electron();
    return c;
}

TermSymbol ElectronConfiguration::hund_term() const {
    if (open_subshell_count() > 1) return TermSymbol{};
    int twice_s = 0;
    int L = 0;
    int l_open = 0;
    int e_open = 0;
    for (const Subshell& s : m_subshells) {
        if (s.is_full()) continue;
        l_open = s.l;
        e_open = s.electrons;
        const int orbitals = s.orbitals();
        int ml = s.l;
        for (int k = 0; k < s.electrons; ++k) {
            L += ml;
            twice_s += k < orbitals ? 1 : -1;
            --ml;
            if (ml < -s.l) ml = s.l;
        }
    }
    if (L < 0) L = -L;
    TermSymbol t;
    t.multiplicity = static_cast<std::uint8_t>(twice_s + 1);
    t.orbital_l = static_cast<std::uint8_t>(L);
    t.odd_parity = parity() < 0;
    const int capacity = 2 * (2 * l_open + 1);
    if (e_open == 0 || e_open * 2 <= capacity) {
        t.twice_j = static_cast<std::int8_t>(std::abs(2 * L - twice_s));
    } else {
        t.twice_j = static_cast<std::int8_t>(2 * L + twice_s);
    }
    if (e_open * 2 == capacity) t.twice_j = static_cast<std::int8_t>(twice_s);
    return t;
}

QuantumNumbers ElectronConfiguration::last_electron() const {
    QuantumNumbers q;
    if (m_subshells.empty()) return q;
    const Subshell* last = &m_subshells.front();
    for (const Subshell& s : m_subshells) {
        if (madelung_less(*last, s)) last = &s;
    }
    q.n = last->n;
    q.l = last->l;
    const int orbitals = last->orbitals();
    const int k = last->electrons - 1;
    const int slot = k % orbitals;
    q.ml = last->l - slot;
    q.ms = k < orbitals ? 0.5 : -0.5;
    return q;
}

int ElectronConfiguration::parity() const noexcept {
    int sum = 0;
    for (const Subshell& s : m_subshells) sum += s.l * s.electrons;
    return sum % 2 == 0 ? 1 : -1;
}

double ElectronConfiguration::slater_shielding(int n, int l) const noexcept {
    double sigma = 0.0;
    const bool sp = l <= 1;
    for (const Subshell& s : m_subshells) {
        int count = s.electrons;
        if (s.n == n && s.l == l) count -= 1;
        if (count <= 0) continue;
        const bool same_group = sp ? (s.n == n && s.l <= 1) : (s.n == n && s.l == l);
        if (same_group) {
            sigma += count * ((n == 1) ? 0.30 : 0.35);
            continue;
        }
        if (sp) {
            if (s.n == n) continue;
            if (s.n == n - 1) sigma += count * 0.85;
            else if (s.n < n - 1) sigma += count * 1.00;
        } else {
            const bool inside = s.n < n || (s.n == n && s.l < l);
            if (inside) sigma += count * 1.00;
        }
    }
    return sigma;
}

double ElectronConfiguration::effective_nuclear_charge(int atomic_number, int n, int l) const noexcept {
    return atomic_number - slater_shielding(n, l);
}

double ElectronConfiguration::effective_nuclear_charge(int atomic_number) const noexcept {
    const QuantumNumbers q = last_electron();
    return effective_nuclear_charge(atomic_number, q.n, q.l);
}

std::string ElectronConfiguration::to_string() const {
    std::string out;
    for (const Subshell& s : m_subshells) {
        if (!out.empty()) out += ' ';
        out += s.to_string();
    }
    return out;
}

std::string ElectronConfiguration::to_noble_gas_string() const {
    int core = 0;
    const int e = electrons();
    for (int z : NOBLE_GAS_NUMBERS) {
        if (z > e) break;
        const ElectronConfiguration gas = ground_state(PeriodicTable::element(z));
        bool contained = true;
        for (const Subshell& s : gas.subshells()) {
            if (occupancy(s.n, s.l) != s.electrons) {
                contained = false;
                break;
            }
        }
        if (contained) core = z;
    }
    if (core == 0) return to_string();
    const ElectronConfiguration gas = ground_state(PeriodicTable::element(core));
    std::vector<Subshell> rest;
    for (const Subshell& s : m_subshells) {
        if (gas.occupancy(s.n, s.l) != s.electrons) rest.push_back(s);
    }
    std::string out = "[";
    out += PeriodicTable::element(core).symbol;
    out += "]";
    for (const Subshell& s : rest) {
        out += ' ';
        out += s.to_string();
    }
    return out;
}

std::string ElectronConfiguration::to_unicode_string(bool noble_gas_core) const {
    const std::string plain = noble_gas_core ? to_noble_gas_string() : to_string();
    std::string out;
    std::size_t i = 0;
    while (i < plain.size()) {
        if (is_ascii_digit(plain[i]) && i > 0 && is_ascii_lower(plain[i - 1])) {
            std::size_t j = i;
            while (j < plain.size() && is_ascii_digit(plain[j])) ++j;
            out += superscript(plain.substr(i, j - i));
            i = j;
        } else {
            out += plain[i++];
        }
    }
    return out;
}

std::string ElectronConfiguration::shell_string() const {
    std::string out;
    for (int count : shells()) {
        if (!out.empty()) out += ", ";
        out += std::to_string(count);
    }
    return out;
}

} // namespace chemistry
} // namespace fizmo
