#include "reaction.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;

long long checked_mul(long long a, long long b) {
    const long double p = static_cast<long double>(a) * static_cast<long double>(b);
    if (std::fabs(p) > 9.0e18L) throw std::overflow_error("fizmo::chemistry: coefficients too large to balance exactly");
    return a * b;
}

struct Rational {
    long long n = 0;
    long long d = 1;

    Rational() = default;
    Rational(long long num, long long den = 1) : n(num), d(den) { normalize(); }

    void normalize() {
        if (d == 0) throw std::domain_error("fizmo::chemistry: division by zero while balancing");
        if (d < 0) {
            n = -n;
            d = -d;
        }
        const long long g = std::gcd(n < 0 ? -n : n, d);
        if (g > 1) {
            n /= g;
            d /= g;
        }
    }

    bool zero() const { return n == 0; }
    Rational operator+(const Rational& o) const { return Rational(checked_mul(n, o.d) + checked_mul(o.n, d), checked_mul(d, o.d)); }
    Rational operator-(const Rational& o) const { return Rational(checked_mul(n, o.d) - checked_mul(o.n, d), checked_mul(d, o.d)); }
    Rational operator*(const Rational& o) const { return Rational(checked_mul(n, o.n), checked_mul(d, o.d)); }
    Rational operator/(const Rational& o) const { return Rational(checked_mul(n, o.d), checked_mul(d, o.n)); }
    Rational operator-() const { return Rational(-n, d); }
};

Rational to_rational(double x) {
    if (std::fabs(x - std::round(x)) < 1e-12) return Rational(std::llround(x), 1);
    for (long long den = 2; den <= 100000; ++den) {
        const double num = x * static_cast<double>(den);
        if (std::fabs(num - std::round(num)) < 1e-9 * static_cast<double>(den)) return Rational(std::llround(num), den);
    }
    throw std::invalid_argument("fizmo::chemistry: a count in the reaction is not a simple fraction");
}

AverageOxidationState average_state(double value) {
    for (int den = 1; den <= 1000; ++den) {
        const double num = value * den;
        if (std::fabs(num - std::round(num)) < 1e-7) return AverageOxidationState(static_cast<int>(std::lround(num)), den);
    }
    return AverageOxidationState(static_cast<int>(std::lround(value)));
}

std::string_view strip(std::string_view s) {
    return trim(s);
}

std::vector<std::string_view> split_terms(std::string_view side) {
    std::vector<std::string_view> out;
    if (side.find(" + ") != std::string_view::npos) {
        std::size_t start = 0;
        while (true) {
            const std::size_t p = side.find(" + ", start);
            if (p == std::string_view::npos) {
                out.push_back(strip(side.substr(start)));
                break;
            }
            out.push_back(strip(side.substr(start, p - start)));
            start = p + 3;
        }
        return out;
    }
    std::size_t start = 0;
    for (std::size_t i = 0; i < side.size(); ++i) {
        if (side[i] != '+') continue;
        const bool next_starts_term = i + 1 < side.size() && side[i + 1] != '+' && side[i + 1] != ' ' && side[i + 1] != ')' && side[i + 1] != '}';
        const bool prev_ok = i > start;
        if (next_starts_term && prev_ok) {
            out.push_back(strip(side.substr(start, i - start)));
            start = i + 1;
        }
    }
    out.push_back(strip(side.substr(start)));
    return out;
}

ReactionTerm parse_term(std::string_view text) {
    text = strip(text);
    ReactionTerm term;
    std::size_t i = 0;
    while (i < text.size() && (is_ascii_digit(text[i]) || text[i] == '.' || text[i] == '/')) ++i;
    if (i > 0) {
        const std::string number(text.substr(0, i));
        const std::size_t slash = number.find('/');
        if (slash != std::string::npos) term.coefficient = std::stod(number.substr(0, slash)) / std::stod(number.substr(slash + 1));
        else term.coefficient = std::stod(number);
        text = strip(text.substr(i));
    }
    if (text.empty()) throw std::invalid_argument("fizmo::chemistry: empty term in reaction");
    term.formula = Formula::parse(text);
    return term;
}

struct ArrowMatch {
    std::size_t position = std::string_view::npos;
    std::size_t length = 0;
    bool reversible = false;
};

ArrowMatch find_arrow(std::string_view s) {
    static const std::pair<const char*, bool> arrows[] = {
        { "<=>", true }, { "<->", true }, { "\xE2\x87\x8C", true }, { "\xE2\x87\x84", true }, { "-->", false }, { "->", false },
        { "=>", false }, { "\xE2\x86\x92", false }, { "\xE2\x9F\xB6", false }, { "=", false }
    };
    for (const auto& a : arrows) {
        const std::size_t p = s.find(a.first);
        if (p != std::string_view::npos) return ArrowMatch{ p, std::string_view(a.first).size(), a.second };
    }
    return ArrowMatch{};
}

struct ColumnKey {
    int z;
    int a;
    bool operator<(const ColumnKey& o) const { return z != o.z ? z < o.z : a < o.a; }
};

std::vector<std::vector<Rational>> composition_matrix(const std::vector<ReactionTerm>& reactants, const std::vector<ReactionTerm>& products) {
    std::map<ColumnKey, std::size_t> rows;
    auto add_keys = [&](const std::vector<ReactionTerm>& side) {
        for (const ReactionTerm& t : side) {
            for (const FormulaTerm& f : t.formula.terms()) rows.emplace(ColumnKey{ f.element->atomic_number, f.mass_number }, 0);
        }
    };
    add_keys(reactants);
    add_keys(products);
    std::size_t r = 0;
    for (auto& kv : rows) kv.second = r++;
    const std::size_t charge_row = r;
    const std::size_t n = reactants.size() + products.size();
    std::vector<std::vector<Rational>> m(r + 1, std::vector<Rational>(n));
    std::size_t col = 0;
    for (int side = 0; side < 2; ++side) {
        const auto& list = side == 0 ? reactants : products;
        const long long sign = side == 0 ? 1 : -1;
        for (const ReactionTerm& t : list) {
            for (const FormulaTerm& f : t.formula.terms()) {
                const std::size_t row = rows[ColumnKey{ f.element->atomic_number, f.mass_number }];
                m[row][col] = m[row][col] + to_rational(f.count) * Rational(sign);
            }
            m[charge_row][col] = Rational(sign * t.formula.charge());
            ++col;
        }
    }
    return m;
}

std::vector<std::vector<Rational>> null_space(std::vector<std::vector<Rational>> m, std::size_t columns) {
    std::vector<std::size_t> pivots;
    std::size_t row = 0;
    for (std::size_t c = 0; c < columns && row < m.size(); ++c) {
        std::size_t p = row;
        while (p < m.size() && m[p][c].zero()) ++p;
        if (p == m.size()) continue;
        std::swap(m[p], m[row]);
        const Rational inv = Rational(1) / m[row][c];
        for (Rational& x : m[row]) x = x * inv;
        for (std::size_t k = 0; k < m.size(); ++k) {
            if (k == row || m[k][c].zero()) continue;
            const Rational f = m[k][c];
            for (std::size_t j = 0; j < columns; ++j) m[k][j] = m[k][j] - f * m[row][j];
        }
        pivots.push_back(c);
        ++row;
    }
    std::vector<std::vector<Rational>> basis;
    for (std::size_t free = 0; free < columns; ++free) {
        if (std::find(pivots.begin(), pivots.end(), free) != pivots.end()) continue;
        std::vector<Rational> v(columns);
        v[free] = Rational(1);
        for (std::size_t k = 0; k < pivots.size(); ++k) v[pivots[k]] = -m[k][free];
        basis.push_back(v);
    }
    return basis;
}

bool same_species(const ReactionTerm& a, const ReactionTerm& b) {
    return a.formula.same_composition(b.formula) && a.formula.phase() == b.formula.phase();
}

void add_term(std::vector<ReactionTerm>& side, const ReactionTerm& t) {
    for (ReactionTerm& s : side) {
        if (same_species(s, t)) {
            s.coefficient += t.coefficient;
            return;
        }
    }
    side.push_back(t);
}

double side_count(const std::vector<ReactionTerm>& side, int z) {
    double n = 0.0;
    for (const ReactionTerm& t : side) n += t.coefficient * t.formula.count(z);
    return n;
}

std::string format_coefficient(double c) {
    if (std::fabs(c - 1.0) < 1e-12) return std::string();
    if (std::fabs(c - std::round(c)) < 1e-9) return std::to_string(std::llround(c)) + " ";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g ", c);
    return buf;
}

bool is_named(const ReactionTerm& t, std::string_view formula) {
    return t.formula.same_composition(Formula::parse(formula));
}

} // namespace

std::string ReactionTerm::to_string(bool unicode) const {
    std::string out = format_coefficient(coefficient);
    out += formula.display_string(unicode);
    if (formula.phase() != Phase::Unknown && !formula.is_electron()) {
        out += "(";
        out += phase_label(formula.phase());
        out += ")";
    }
    return out;
}

Reaction::Reaction(std::vector<ReactionTerm> reactants, std::vector<ReactionTerm> products, bool reversible)
    : m_reactants(std::move(reactants)), m_products(std::move(products)), m_reversible(reversible) {}

Reaction Reaction::parse(std::string_view equation) {
    const ArrowMatch arrow = find_arrow(equation);
    if (arrow.position == std::string_view::npos) throw std::invalid_argument("fizmo::chemistry: a reaction needs an arrow such as -> or =");
    Reaction r;
    r.m_reversible = arrow.reversible;
    const std::string_view left = strip(equation.substr(0, arrow.position));
    const std::string_view right = strip(equation.substr(arrow.position + arrow.length));
    if (left.empty() || right.empty()) throw std::invalid_argument("fizmo::chemistry: a reaction needs both sides");
    for (std::string_view t : split_terms(left)) r.m_reactants.push_back(parse_term(t));
    for (std::string_view t : split_terms(right)) r.m_products.push_back(parse_term(t));
    return r;
}

std::vector<std::pair<const Element*, double>> Reaction::element_imbalance() const {
    std::vector<const Element*> elements;
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) {
            for (const Element* e : t.formula.elements()) {
                if (std::find(elements.begin(), elements.end(), e) == elements.end()) elements.push_back(e);
            }
        }
    }
    std::vector<std::pair<const Element*, double>> out;
    for (const Element* e : elements) {
        const double d = side_count(m_reactants, e->atomic_number) - side_count(m_products, e->atomic_number);
        if (std::fabs(d) > 1e-12) out.emplace_back(e, d);
    }
    return out;
}

double Reaction::charge_imbalance() const {
    double q = 0.0;
    for (const ReactionTerm& t : m_reactants) q += t.coefficient * t.formula.charge();
    for (const ReactionTerm& t : m_products) q -= t.coefficient * t.formula.charge();
    return q;
}

bool Reaction::is_balanced(double tolerance) const {
    for (const auto& e : element_imbalance()) {
        if (std::fabs(e.second) > tolerance) return false;
    }
    return std::fabs(charge_imbalance()) <= tolerance;
}

void Reaction::balance() {
    const std::size_t n = species_count();
    if (n < 2) throw std::invalid_argument("fizmo::chemistry: nothing to balance");
    const std::vector<std::vector<Rational>> basis = null_space(composition_matrix(m_reactants, m_products), n);
    if (basis.empty()) throw std::invalid_argument("fizmo::chemistry: this reaction cannot be balanced");
    std::vector<Rational> v = basis.front();
    if (basis.size() > 1) {
        v.assign(n, Rational(0));
        for (const auto& b : basis) {
            for (std::size_t i = 0; i < n; ++i) v[i] = v[i] + b[i];
        }
    }
    long long lcm = 1;
    for (const Rational& x : v) lcm = checked_mul(lcm / std::gcd(lcm, x.d), x.d);
    std::vector<long long> c(n);
    for (std::size_t i = 0; i < n; ++i) c[i] = checked_mul(v[i].n, lcm / v[i].d);
    bool all_negative = true, all_positive = true;
    for (long long x : c) {
        if (x > 0) all_negative = false;
        if (x < 0) all_positive = false;
        if (x == 0) all_positive = all_negative = false;
    }
    if (all_negative) {
        for (long long& x : c) x = -x;
        all_positive = true;
    }
    if (!all_positive) throw std::invalid_argument("fizmo::chemistry: no balance with every species on the side it was written on");
    long long g = 0;
    for (long long x : c) g = std::gcd(g, x);
    for (std::size_t i = 0; i < n; ++i) {
        const double coefficient = static_cast<double>(c[i] / g);
        if (i < m_reactants.size()) m_reactants[i].coefficient = coefficient;
        else m_products[i - m_reactants.size()].coefficient = coefficient;
    }
}

Reaction Reaction::balanced() const {
    Reaction r = *this;
    r.balance();
    return r;
}

bool Reaction::balance_is_unique() const {
    return null_space(composition_matrix(m_reactants, m_products), species_count()).size() == 1;
}

Reaction Reaction::reversed() const {
    return Reaction(m_products, m_reactants, m_reversible);
}

Reaction Reaction::scaled(double factor) const {
    Reaction r = *this;
    for (ReactionTerm& t : r.m_reactants) t.coefficient *= factor;
    for (ReactionTerm& t : r.m_products) t.coefficient *= factor;
    return r;
}

Reaction Reaction::simplified() const {
    std::vector<ReactionTerm> left, right;
    for (const ReactionTerm& t : m_reactants) add_term(left, t);
    for (const ReactionTerm& t : m_products) add_term(right, t);
    for (ReactionTerm& l : left) {
        for (ReactionTerm& r : right) {
            if (!same_species(l, r)) continue;
            const double common = std::min(l.coefficient, r.coefficient);
            l.coefficient -= common;
            r.coefficient -= common;
        }
    }
    auto prune = [](std::vector<ReactionTerm>& side) {
        side.erase(std::remove_if(side.begin(), side.end(), [](const ReactionTerm& t) { return std::fabs(t.coefficient) < 1e-12; }), side.end());
    };
    prune(left);
    prune(right);
    return Reaction(left, right, m_reversible);
}

Reaction Reaction::operator+(const Reaction& o) const {
    Reaction r = *this;
    for (const ReactionTerm& t : o.m_reactants) r.m_reactants.push_back(t);
    for (const ReactionTerm& t : o.m_products) r.m_products.push_back(t);
    return r.simplified();
}

std::vector<Species> Reaction::species() const {
    std::vector<Species> out;
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) out.push_back(Species::of(t.formula, t.formula.phase()));
    }
    return out;
}

bool Reaction::has_thermo_data() const {
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) {
            if (!Species::exists(t.formula, t.formula.phase())) return false;
        }
    }
    return true;
}

double Reaction::delta_n_gas() const {
    double n = 0.0;
    auto gas = [](const ReactionTerm& t) {
        if (t.formula.phase() != Phase::Unknown) return t.formula.phase() == Phase::Gas;
        if (t.formula.is_electron()) return false;
        try {
            return Species::of(t.formula).phase() == Phase::Gas;
        } catch (...) {
            return false;
        }
    };
    for (const ReactionTerm& t : m_products) n += gas(t) ? t.coefficient : 0.0;
    for (const ReactionTerm& t : m_reactants) n -= gas(t) ? t.coefficient : 0.0;
    return n;
}

double Reaction::delta_h(double t) const {
    const std::vector<Species> s = species();
    double h = 0.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nu = i < m_reactants.size() ? -m_reactants[i].coefficient : m_products[i - m_reactants.size()].coefficient;
        h += nu * s[i].enthalpy(t);
    }
    return h;
}

double Reaction::delta_s(double t) const {
    const std::vector<Species> s = species();
    double sum = 0.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const double nu = i < m_reactants.size() ? -m_reactants[i].coefficient : m_products[i - m_reactants.size()].coefficient;
        sum += nu * s[i].entropy(t);
    }
    return sum;
}

double Reaction::delta_cp(double t) const {
    const std::vector<Species> s = species();
    double sum = 0.0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i].is_electron()) continue;
        const double nu = i < m_reactants.size() ? -m_reactants[i].coefficient : m_products[i - m_reactants.size()].coefficient;
        sum += nu * s[i].heat_capacity(t);
    }
    return sum;
}

double Reaction::delta_g(double t) const {
    return delta_h(t) - t * delta_s(t);
}

double Reaction::equilibrium_constant(double t) const {
    return std::exp(-delta_g(t) / (R * t));
}

double Reaction::kc(double t) const {
    return kc_from_kp(equilibrium_constant(t), t, delta_n_gas());
}

double Reaction::spontaneity_temperature() const {
    return crossover_temperature(delta_h(), delta_s());
}

double Reaction::reaction_quotient(const std::vector<double>& activities) const {
    if (activities.size() != species_count()) throw std::invalid_argument("fizmo::chemistry: one activity per species");
    double q = 1.0;
    for (std::size_t i = 0; i < activities.size(); ++i) {
        const double nu = i < m_reactants.size() ? -m_reactants[i].coefficient : m_products[i - m_reactants.size()].coefficient;
        q *= std::pow(activities[i], nu);
    }
    return q;
}

std::size_t Reaction::limiting_reactant(const std::vector<double>& moles) const {
    if (moles.size() != m_reactants.size()) throw std::invalid_argument("fizmo::chemistry: one amount per reactant");
    std::size_t best = 0;
    double best_extent = 1e300;
    for (std::size_t i = 0; i < moles.size(); ++i) {
        if (m_reactants[i].formula.is_electron()) continue;
        const double e = moles[i] / m_reactants[i].coefficient;
        if (e < best_extent) {
            best_extent = e;
            best = i;
        }
    }
    return best;
}

double Reaction::extent(const std::vector<double>& moles) const {
    const std::size_t i = limiting_reactant(moles);
    return moles[i] / m_reactants[i].coefficient;
}

std::vector<double> Reaction::product_moles(const std::vector<double>& moles) const {
    const double x = extent(moles);
    std::vector<double> out;
    for (const ReactionTerm& t : m_products) out.push_back(t.coefficient * x);
    return out;
}

std::vector<double> Reaction::leftover_moles(const std::vector<double>& moles) const {
    const double x = extent(moles);
    std::vector<double> out;
    for (std::size_t i = 0; i < m_reactants.size(); ++i) out.push_back(std::max(0.0, moles[i] - m_reactants[i].coefficient * x));
    return out;
}

double Reaction::theoretical_yield(std::size_t product_index, const std::vector<double>& moles) const {
    return m_products.at(product_index).coefficient * extent(moles);
}

std::vector<double> Reaction::grams_to_moles(const std::vector<double>& grams) const {
    if (grams.size() != m_reactants.size()) throw std::invalid_argument("fizmo::chemistry: one mass per reactant");
    std::vector<double> out;
    for (std::size_t i = 0; i < grams.size(); ++i) out.push_back(grams[i] / m_reactants[i].formula.molar_mass());
    return out;
}

double Reaction::theoretical_yield_grams(std::size_t product_index, const std::vector<double>& grams) const {
    return theoretical_yield(product_index, grams_to_moles(grams)) * m_products.at(product_index).formula.molar_mass();
}

double Reaction::mass_balance() const {
    double m = 0.0;
    for (const ReactionTerm& t : m_reactants) m += t.coefficient * t.formula.molar_mass();
    for (const ReactionTerm& t : m_products) m -= t.coefficient * t.formula.molar_mass();
    return m;
}

std::vector<OxidationChange> Reaction::oxidation_changes() const {
    struct Tally {
        double atoms = 0.0;
        double weighted = 0.0;
    };
    std::map<const Element*, Tally> before, after;
    auto tally = [](const std::vector<ReactionTerm>& side, std::map<const Element*, Tally>& out) {
        for (const ReactionTerm& t : side) {
            if (t.formula.is_electron()) continue;
            for (const OxidationAssignment& a : assign_oxidation_states(t.formula)) {
                const double atoms = t.coefficient * t.formula.count(*a.element);
                out[a.element].atoms += atoms;
                out[a.element].weighted += atoms * a.state.value();
            }
        }
    };
    tally(m_reactants, before);
    tally(m_products, after);
    std::vector<OxidationChange> out;
    for (const auto& b : before) {
        auto it = after.find(b.first);
        if (it == after.end() || b.second.atoms <= 0.0 || it->second.atoms <= 0.0) continue;
        const double sb = b.second.weighted / b.second.atoms;
        const double sa = it->second.weighted / it->second.atoms;
        if (std::fabs(sb - sa) < 1e-9) continue;
        out.push_back(OxidationChange{ b.first, average_state(sb), average_state(sa), b.second.atoms });
    }
    return out;
}

bool Reaction::is_redox() const {
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) {
            if (t.formula.is_electron()) return true;
        }
    }
    return !oxidation_changes().empty();
}

double Reaction::electrons_transferred() const {
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) {
            if (t.formula.is_electron()) return t.coefficient;
        }
    }
    double e = 0.0;
    for (const OxidationChange& c : oxidation_changes()) {
        if (c.oxidized()) e += c.electrons();
    }
    return e;
}

ReactionType Reaction::classify() const {
    bool o2 = false;
    for (const ReactionTerm& t : m_reactants) o2 = o2 || is_named(t, "O2");
    bool co2_or_water = false;
    for (const ReactionTerm& t : m_products) co2_or_water = co2_or_water || is_named(t, "CO2") || is_named(t, "H2O");
    bool fuel = false;
    for (const ReactionTerm& t : m_reactants) fuel = fuel || (!is_named(t, "O2") && (t.formula.contains(6) || t.formula.contains(1)));
    if (o2 && co2_or_water && fuel) return ReactionType::Combustion;
    const bool redox = is_redox();
    for (const auto* side : { &m_reactants, &m_products }) {
        for (const ReactionTerm& t : *side) {
            if (t.formula.is_electron()) return ReactionType::Redox;
        }
    }
    auto count_elements = [](const std::vector<ReactionTerm>& side) {
        int n = 0;
        for (const ReactionTerm& t : side) n += t.formula.is_element() ? 1 : 0;
        return n;
    };
    if (m_reactants.size() == 2 && m_products.size() == 2 && count_elements(m_reactants) == 1 && count_elements(m_products) == 1) return ReactionType::SingleReplacement;
    if (m_reactants.size() >= 2 && m_products.size() == 1) return ReactionType::Synthesis;
    if (m_reactants.size() == 1 && m_products.size() >= 2) return ReactionType::Decomposition;
    bool water = false;
    for (const ReactionTerm& t : m_products) water = water || is_named(t, "H2O");
    bool hydroxide_or_ammonia = false;
    for (const ReactionTerm& t : m_reactants) {
        const Formula& f = t.formula;
        hydroxide_or_ammonia = hydroxide_or_ammonia || is_named(t, "NH3") || is_named(t, "OH-") || (f.contains_metal() && f.contains(8) && f.contains(1));
    }
    if (!redox && water && hydroxide_or_ammonia) return ReactionType::AcidBase;
    if (!redox && m_reactants.size() == 2 && m_products.size() == 2 && count_elements(m_reactants) == 0 && count_elements(m_products) == 0) return ReactionType::DoubleReplacement;
    if (redox) return ReactionType::Redox;
    return ReactionType::Other;
}

std::string Reaction::to_string() const {
    std::string out;
    for (std::size_t i = 0; i < m_reactants.size(); ++i) out += (i ? " + " : "") + m_reactants[i].to_string();
    out += m_reversible ? " <=> " : " -> ";
    for (std::size_t i = 0; i < m_products.size(); ++i) out += (i ? " + " : "") + m_products[i].to_string();
    return out;
}

std::string Reaction::to_unicode_string() const {
    std::string out;
    for (std::size_t i = 0; i < m_reactants.size(); ++i) out += (i ? " + " : "") + m_reactants[i].to_string(true);
    out += m_reversible ? " \xE2\x87\x8C " : " \xE2\x86\x92 ";
    for (std::size_t i = 0; i < m_products.size(); ++i) out += (i ? " + " : "") + m_products[i].to_string(true);
    return out;
}

double percent_yield(double actual, double theoretical) noexcept {
    return 100.0 * actual / theoretical;
}

double atom_economy(const Reaction& reaction, std::size_t desired_product) {
    double reactant_mass = 0.0;
    for (const ReactionTerm& t : reaction.reactants()) reactant_mass += t.coefficient * t.formula.molar_mass();
    const ReactionTerm& p = reaction.products().at(desired_product);
    return 100.0 * p.coefficient * p.formula.molar_mass() / reactant_mass;
}

} // namespace chemistry
} // namespace fizmo
