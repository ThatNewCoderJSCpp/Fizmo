#include "formula.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const char* const MIDDLE_DOT = "\xC2\xB7";
const char* const BULLET = "\xE2\x80\xA2";

bool is_integral(double v) {
    return std::fabs(v - std::round(v)) < 1e-9;
}

std::string format_count(double c) {
    if (is_integral(c)) {
        const long long n = std::llround(c);
        return n == 1 ? std::string() : std::to_string(n);
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6g", c);
    return buf;
}

class Parser {
public:
    explicit Parser(std::string_view text) : m_text(text) {}

    std::vector<FormulaTerm> parse_sequence(char closing) {
        std::vector<FormulaTerm> out;
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == closing) return out;
            if (c == '(' || c == '[' || c == '{') {
                const char close = c == '(' ? ')' : (c == '[' ? ']' : '}');
                if (c == '[' && is_isotope_label()) {
                    parse_isotope_label(out);
                    continue;
                }
                ++m_pos;
                std::vector<FormulaTerm> inner = parse_sequence(close);
                if (m_pos >= m_text.size() || m_text[m_pos] != close) throw std::invalid_argument("fizmo::chemistry: unbalanced brackets in formula");
                ++m_pos;
                const double n = parse_count(1.0);
                for (FormulaTerm& t : inner) {
                    t.count *= n;
                    out.push_back(t);
                }
                continue;
            }
            if (c == '^' && m_pos + 1 < m_text.size() && is_ascii_digit(m_text[m_pos + 1])) {
                ++m_pos;
                const int a = static_cast<int>(parse_integer());
                FormulaTerm t = parse_element();
                t.mass_number = a;
                t.count = parse_count(1.0);
                out.push_back(t);
                continue;
            }
            if (is_ascii_upper(c)) {
                FormulaTerm t = parse_element();
                t.count = parse_count(1.0);
                out.push_back(t);
                continue;
            }
            throw std::invalid_argument(std::string("fizmo::chemistry: unexpected '") + c + "' in formula");
        }
        if (closing != '\0') throw std::invalid_argument("fizmo::chemistry: unbalanced brackets in formula");
        return out;
    }

    std::size_t position() const noexcept { return m_pos; }

private:
    bool is_isotope_label() const {
        std::size_t p = m_pos + 1;
        if (p >= m_text.size() || !is_ascii_digit(m_text[p])) return false;
        while (p < m_text.size() && is_ascii_digit(m_text[p])) ++p;
        if (p >= m_text.size() || !is_ascii_upper(m_text[p])) return false;
        ++p;
        if (p < m_text.size() && is_ascii_lower(m_text[p])) ++p;
        return p < m_text.size() && m_text[p] == ']';
    }

    void parse_isotope_label(std::vector<FormulaTerm>& out) {
        ++m_pos;
        const int a = static_cast<int>(parse_integer());
        FormulaTerm t = parse_element();
        ++m_pos;
        t.mass_number = a;
        t.count = parse_count(1.0);
        out.push_back(t);
    }

    long long parse_integer() {
        long long n = 0;
        while (m_pos < m_text.size() && is_ascii_digit(m_text[m_pos])) n = n * 10 + (m_text[m_pos++] - '0');
        return n;
    }

    double parse_count(double fallback) {
        const std::size_t start = m_pos;
        while (m_pos < m_text.size() && (is_ascii_digit(m_text[m_pos]) || (m_text[m_pos] == '.' && m_pos + 1 < m_text.size() && is_ascii_digit(m_text[m_pos + 1]) && m_pos > start))) ++m_pos;
        if (m_pos == start) return fallback;
        return std::stod(std::string(m_text.substr(start, m_pos - start)));
    }

    FormulaTerm parse_element() {
        FormulaTerm t;
        const std::size_t start = m_pos++;
        if (m_pos < m_text.size() && is_ascii_lower(m_text[m_pos])) {
            const Element* two = PeriodicTable::find_symbol(m_text.substr(start, 2));
            if (two && m_text.substr(start, 2) == two->symbol) {
                ++m_pos;
                t.element = two;
                return t;
            }
        }
        const std::string_view one = m_text.substr(start, 1);
        if (one == "D") {
            t.element = &PeriodicTable::element(1);
            t.mass_number = 2;
            return t;
        }
        if (one == "T") {
            t.element = &PeriodicTable::element(1);
            t.mass_number = 3;
            return t;
        }
        const Element* e = PeriodicTable::find_symbol(one);
        if (!e || one != e->symbol) throw std::invalid_argument("fizmo::chemistry: unknown element '" + std::string(m_text.substr(start, m_pos - start + (m_pos < m_text.size() && is_ascii_lower(m_text[m_pos]) ? 1 : 0))) + "' in formula");
        t.element = e;
        return t;
    }

    std::string_view m_text;
    std::size_t m_pos = 0;
};

struct ChargeSplit {
    std::string_view body;
    int charge = 0;
};

int sign_run(std::string_view s, std::size_t& end) {
    int count = 0;
    char sign = 0;
    while (end > 0 && (s[end - 1] == '+' || s[end - 1] == '-')) {
        if (sign == 0) sign = s[end - 1];
        if (s[end - 1] != sign) break;
        ++count;
        --end;
    }
    return sign == '+' ? count : -count;
}

int count_symbols(std::string_view body) {
    int n = 0;
    for (char c : body) {
        if (is_ascii_upper(c)) ++n;
    }
    return n;
}

ChargeSplit split_charge(std::string_view s) {
    ChargeSplit out;
    out.body = s;
    const std::size_t caret = s.rfind('^');
    if (caret != std::string_view::npos) {
        std::string_view tail = s.substr(caret + 1);
        if (!tail.empty() && tail.front() == '{') tail.remove_prefix(1);
        if (!tail.empty() && tail.back() == '}') tail.remove_suffix(1);
        bool only_charge = !tail.empty();
        bool has_sign = false;
        for (char ch : tail) {
            if (ch == '+' || ch == '-') has_sign = true;
            else if (!is_ascii_digit(ch)) only_charge = false;
        }
        if (only_charge && has_sign) {
            int magnitude = 0;
            bool digits = false;
            int sign = 0;
            int signs = 0;
            for (char ch : tail) {
                if (is_ascii_digit(ch)) {
                    magnitude = magnitude * 10 + (ch - '0');
                    digits = true;
                } else {
                    sign = ch == '+' ? 1 : -1;
                    ++signs;
                }
            }
            out.body = trim(s.substr(0, caret));
            out.charge = sign * (digits ? magnitude : signs);
            return out;
        }
    }
    std::size_t end = s.size();
    const int signs = sign_run(s, end);
    if (signs == 0) return out;
    std::size_t digit_start = end;
    while (digit_start > 0 && is_ascii_digit(s[digit_start - 1])) --digit_start;
    const std::size_t digit_len = end - digit_start;
    const int sign = signs < 0 ? -1 : 1;
    const bool repeated_sign = signs > 1 || signs < -1;
    if (digit_len == 0 || repeated_sign) {
        out.body = trim(s.substr(0, end));
        out.charge = signs;
        return out;
    }
    auto read = [&](std::size_t from) {
        int value = 0;
        for (std::size_t i = from; i < end; ++i) value = value * 10 + (s[i] - '0');
        return value;
    };
    if (digit_start > 0 && s[digit_start - 1] == ' ') {
        out.body = trim(s.substr(0, digit_start));
        out.charge = sign * read(digit_start);
        return out;
    }
    const bool after_bracket = digit_start > 0 && s[digit_start - 1] == ']';
    if (after_bracket || (digit_len == 1 && count_symbols(s.substr(0, digit_start)) == 1)) {
        out.body = trim(s.substr(0, digit_start));
        out.charge = sign * read(digit_start);
        return out;
    }
    if (digit_len >= 2) {
        out.body = trim(s.substr(0, end - 1));
        out.charge = sign * read(end - 1);
        return out;
    }
    out.body = trim(s.substr(0, end));
    out.charge = sign;
    return out;
}

int script_digit(std::string_view s, std::size_t i, std::size_t& length, bool& superscript) {
    const auto byte = [&](std::size_t k) { return k < s.size() ? static_cast<unsigned char>(s[k]) : 0u; };
    length = 0;
    if (byte(i) == 0xE2 && byte(i + 1) == 0x82 && byte(i + 2) >= 0x80 && byte(i + 2) <= 0x89) {
        length = 3;
        superscript = false;
        return '0' + static_cast<int>(byte(i + 2) - 0x80);
    }
    if (byte(i) == 0xE2 && byte(i + 1) == 0x81) {
        const unsigned c = byte(i + 2);
        superscript = true;
        length = 3;
        if (c == 0xB0) return '0';
        if (c >= 0xB4 && c <= 0xB9) return '4' + static_cast<int>(c - 0xB4);
        if (c == 0xBA) return '+';
        if (c == 0xBB) return '-';
        length = 0;
    }
    if (byte(i) == 0xC2 && (byte(i + 1) == 0xB9 || byte(i + 1) == 0xB2 || byte(i + 1) == 0xB3)) {
        superscript = true;
        length = 2;
        return byte(i + 1) == 0xB9 ? '1' : byte(i + 1) == 0xB2 ? '2' : '3';
    }
    if (byte(i) == 0xE2 && byte(i + 1) == 0x88 && byte(i + 2) == 0x92) {
        superscript = false;
        length = 3;
        return '-';
    }
    return 0;
}

std::string plain_scripts(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool in_superscript = false;
    for (std::size_t i = 0; i < s.size();) {
        std::size_t length = 0;
        bool superscript = false;
        const int c = script_digit(s, i, length, superscript);
        if (length > 0) {
            if (superscript && !in_superscript) out += '^';
            in_superscript = superscript;
            out += static_cast<char>(c);
            i += length;
            continue;
        }
        in_superscript = false;
        if (s[i] == '_') {
            if (i + 1 < s.size() && s[i + 1] == '{') {
                const std::size_t close = s.find('}', i + 2);
                if (close != std::string_view::npos) {
                    out.append(s.substr(i + 2, close - i - 2));
                    i = close + 1;
                    continue;
                }
            }
            ++i;
            continue;
        }
        out += s[i++];
    }
    return out;
}

bool dot_is_separator(std::string_view body, std::size_t i) {
    const bool digit_before = i > 0 && is_ascii_digit(body[i - 1]);
    const bool digit_after = i + 1 < body.size() && is_ascii_digit(body[i + 1]);
    if (!(digit_before && digit_after)) return true;
    std::size_t k = i + 1;
    while (k < body.size() && is_ascii_digit(body[k])) ++k;
    const std::string_view rest = body.substr(k);
    return rest.substr(0, 3) == "H2O" || rest.substr(0, 3) == "NH3" || rest.substr(0, 3) == "D2O";
}

} // namespace

double FormulaTerm::mass() const noexcept {
    if (!element) return 0.0;
    if (mass_number != 0) {
        const Isotope* i = element->isotope(mass_number);
        if (i) return i->mass;
    }
    return element->atomic_weight;
}

Formula::Formula(const Element& element, double count, int mass_number) {
    add(&element, mass_number, count);
    m_source = to_string();
}

Formula Formula::electron() {
    Formula f;
    f.m_charge = -1;
    f.m_source = "e-";
    return f;
}

void Formula::add(const Element* e, int mass_number, double count) {
    if (count == 0.0) return;
    for (FormulaTerm& t : m_terms) {
        if (t.element == e && t.mass_number == mass_number) {
            t.count += count;
            return;
        }
    }
    m_terms.push_back(FormulaTerm{ e, mass_number, count });
}

Formula Formula::parse(std::string_view text) {
    Formula f;
    const std::string plain = plain_scripts(trim(text));
    std::string_view s = trim(plain);
    f.m_source = std::string(s);
    if (s.empty()) return f;

    static const std::pair<const char*, Phase> phases[] = {
        { "(aq)", Phase::Aqueous }, { "(s)", Phase::Solid }, { "(l)", Phase::Liquid }, { "(g)", Phase::Gas },
        { "(sc)", Phase::Supercritical }, { "(cr)", Phase::Solid }, { "(pl)", Phase::Plasma }
    };
    for (const auto& p : phases) {
        const std::string_view tag = p.first;
        if (s.size() > tag.size() && s.substr(s.size() - tag.size()) == tag) {
            f.m_phase = p.second;
            s = trim(s.substr(0, s.size() - tag.size()));
            break;
        }
    }

    if (s == "e-" || s == "e^-" || s == "e") {
        f.m_charge = -1;
        return f;
    }

    std::string normalized;
    normalized.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s.compare(i, 2, MIDDLE_DOT) == 0) {
            normalized += '*';
            ++i;
        } else if (s.compare(i, 3, BULLET) == 0) {
            normalized += '*';
            i += 2;
        } else {
            normalized += s[i];
        }
    }

    const ChargeSplit split = split_charge(normalized);
    f.m_charge = split.charge;
    std::string_view body = split.body;

    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= body.size(); ++i) {
        const bool sep = i == body.size() || body[i] == '*' || (body[i] == '.' && dot_is_separator(body, i));
        if (sep) {
            parts.push_back(trim(body.substr(start, i - start)));
            start = i + 1;
        }
    }

    for (std::string_view part : parts) {
        if (part.empty()) continue;
        std::size_t k = 0;
        double multiplier = 1.0;
        while (k < part.size() && (is_ascii_digit(part[k]) || part[k] == '.')) ++k;
        if (k > 0) multiplier = std::stod(std::string(part.substr(0, k)));
        Parser p(trim(part.substr(k)));
        std::vector<FormulaTerm> terms = p.parse_sequence('\0');
        for (const FormulaTerm& t : terms) f.add(t.element, t.mass_number, t.count * multiplier);
    }
    return f;
}

bool Formula::is_element() const noexcept {
    if (m_terms.empty()) return false;
    for (const FormulaTerm& t : m_terms) {
        if (t.element != m_terms.front().element) return false;
    }
    return true;
}

bool Formula::is_integral() const noexcept {
    for (const FormulaTerm& t : m_terms) {
        if (!chemistry::is_integral(t.count)) return false;
    }
    return true;
}

double Formula::count(int atomic_number) const noexcept {
    double n = 0.0;
    for (const FormulaTerm& t : m_terms) {
        if (t.element->atomic_number == atomic_number) n += t.count;
    }
    return n;
}

double Formula::count(std::string_view symbol) const {
    const Element* e = PeriodicTable::find(symbol);
    return e ? count(e->atomic_number) : 0.0;
}

double Formula::atom_count() const noexcept {
    double n = 0.0;
    for (const FormulaTerm& t : m_terms) n += t.count;
    return n;
}

int Formula::element_count() const noexcept {
    return static_cast<int>(elements().size());
}

std::vector<const Element*> Formula::elements() const {
    std::vector<const Element*> out;
    for (const FormulaTerm& t : m_terms) {
        if (std::find(out.begin(), out.end(), t.element) == out.end()) out.push_back(t.element);
    }
    return out;
}

std::vector<std::pair<const Element*, double>> Formula::composition() const {
    std::vector<std::pair<const Element*, double>> out;
    for (const Element* e : elements()) out.emplace_back(e, count(e->atomic_number));
    return out;
}

double Formula::molar_mass() const noexcept {
    double m = 0.0;
    for (const FormulaTerm& t : m_terms) m += t.count * t.mass();
    return m - m_charge * ELECTRON_MASS_U;
}

double Formula::molar_mass_uncertainty() const noexcept {
    double u = 0.0;
    for (const FormulaTerm& t : m_terms) {
        double ui = t.element->atomic_weight_uncertainty;
        if (t.mass_number != 0) {
            const Isotope* i = t.element->isotope(t.mass_number);
            if (i) ui = i->mass_uncertainty;
        }
        if (has_value(ui)) u += t.count * ui;
    }
    return u;
}

double Formula::monoisotopic_mass() const noexcept {
    double m = 0.0;
    for (const FormulaTerm& t : m_terms) {
        const Isotope* i = t.mass_number != 0 ? t.element->isotope(t.mass_number) : t.element->most_abundant_isotope();
        if (!i) i = t.element->reference_isotope();
        m += t.count * (i ? i->mass : t.element->atomic_weight);
    }
    return m - m_charge * ELECTRON_MASS_U;
}

int Formula::nominal_mass() const noexcept {
    double m = 0.0;
    for (const FormulaTerm& t : m_terms) {
        const int a = t.mass_number != 0 ? t.mass_number : (t.element->most_abundant_mass_number ? t.element->most_abundant_mass_number : t.element->reference_mass_number);
        m += t.count * a;
    }
    return static_cast<int>(std::lround(m));
}

double Formula::mass_fraction(int atomic_number) const noexcept {
    double part = 0.0;
    double total = 0.0;
    for (const FormulaTerm& t : m_terms) {
        const double m = t.count * t.mass();
        total += m;
        if (t.element->atomic_number == atomic_number) part += m;
    }
    return total > 0.0 ? part / total : 0.0;
}

double Formula::protons() const noexcept {
    double p = 0.0;
    for (const FormulaTerm& t : m_terms) p += t.count * t.element->atomic_number;
    return p;
}

double Formula::valence_electrons() const noexcept {
    double v = 0.0;
    for (const FormulaTerm& t : m_terms) v += t.count * t.element->valence_electrons();
    return v - m_charge;
}

double Formula::degree_of_unsaturation() const noexcept {
    double c = 0.0, h = 0.0, n = 0.0, x = 0.0, other_tetravalent = 0.0, trivalent = 0.0;
    for (const auto& p : composition()) {
        const int z = p.first->atomic_number;
        if (z == 6) c += p.second;
        else if (z == 14) other_tetravalent += p.second;
        else if (z == 1) h += p.second;
        else if (z == 7 || z == 15) n += p.second;
        else if (z == 5) trivalent += p.second;
        else if (p.first->group == 17) x += p.second;
    }
    return (2.0 * (c + other_tetravalent) + 2.0 + n - trivalent * 0.0 - h - x) / 2.0;
}

bool Formula::is_organic() const noexcept {
    return contains(6) && contains(1);
}

bool Formula::is_hydrocarbon() const noexcept {
    return element_count() == 2 && contains(6) && contains(1);
}

bool Formula::contains_metal() const noexcept {
    for (const FormulaTerm& t : m_terms) {
        if (t.element->is_metal()) return true;
    }
    return false;
}

bool Formula::is_probably_ionic() const noexcept {
    double lo = 10.0, hi = 0.0;
    bool metal = false, nonmetal = false;
    for (const FormulaTerm& t : m_terms) {
        if (t.element->is_metal()) metal = true;
        else nonmetal = true;
        const double chi = t.element->electronegativity_pauling;
        if (has_value(chi)) {
            lo = std::min(lo, chi);
            hi = std::max(hi, chi);
        }
    }
    if (contains(7) && contains(1) && count(7) * 4 <= count(1) && !metal) return true;
    return metal && nonmetal && hi - lo >= 1.5;
}

bool Formula::is_diatomic() const noexcept {
    return chemistry::is_integral(atom_count()) && std::llround(atom_count()) == 2;
}

Formula Formula::empirical() const {
    if (!is_integral()) return *this;
    long long g = 0;
    for (const FormulaTerm& t : m_terms) {
        long long a = std::llround(t.count);
        long long b = g;
        while (b != 0) {
            const long long r = a % b;
            a = b;
            b = r;
        }
        g = a;
    }
    if (g <= 1) return without_charge();
    Formula f = scaled(1.0 / static_cast<double>(g));
    f.m_charge = 0;
    f.m_source = f.to_string();
    return f;
}

Formula Formula::without_charge() const {
    Formula f = *this;
    f.m_charge = 0;
    f.m_source = f.to_string();
    return f;
}

Formula Formula::scaled(double factor) const {
    Formula f;
    for (const FormulaTerm& t : m_terms) f.add(t.element, t.mass_number, t.count * factor);
    const double q = m_charge * factor;
    f.m_charge = static_cast<int>(std::lround(q));
    f.m_phase = m_phase;
    f.m_source = f.to_string();
    return f;
}

std::string Formula::charge_string() const {
    if (m_charge == 0) return std::string();
    const int a = m_charge < 0 ? -m_charge : m_charge;
    return (a == 1 ? std::string() : std::to_string(a)) + (m_charge < 0 ? "-" : "+");
}

std::string Formula::to_string() const {
    if (is_electron()) return "e-";
    std::string out;
    for (const FormulaTerm& t : m_terms) {
        if (t.mass_number != 0) {
            if (t.element->atomic_number == 1 && t.mass_number == 2) out += "D";
            else if (t.element->atomic_number == 1 && t.mass_number == 3) out += "T";
            else out += "[" + std::to_string(t.mass_number) + t.element->symbol + "]";
        } else {
            out += t.element->symbol;
        }
        out += format_count(t.count);
    }
    if (m_charge != 0) out += "^" + charge_string();
    return out;
}

std::string Formula::hill() const {
    std::vector<FormulaTerm> terms = m_terms;
    const bool carbon = contains(6);
    std::stable_sort(terms.begin(), terms.end(), [carbon](const FormulaTerm& a, const FormulaTerm& b) {
        auto rank = [carbon](const FormulaTerm& t) {
            if (carbon && t.element->atomic_number == 6) return 0;
            if (carbon && t.element->atomic_number == 1) return 1;
            return 2;
        };
        const int ra = rank(a), rb = rank(b);
        if (ra != rb) return ra < rb;
        const std::string_view sa = a.element->symbol, sb = b.element->symbol;
        if (sa != sb) return sa < sb;
        return a.mass_number < b.mass_number;
    });
    Formula f;
    for (const FormulaTerm& t : terms) f.add(t.element, t.mass_number, t.count);
    f.m_charge = m_charge;
    return f.to_string();
}

std::string Formula::to_unicode_string() const {
    if (is_electron()) return "e\xE2\x81\xBB";
    std::string out;
    for (const FormulaTerm& t : m_terms) {
        if (t.mass_number != 0) out += superscript(std::to_string(t.mass_number));
        out += t.element->symbol;
        out += subscript(format_count(t.count));
    }
    if (m_charge != 0) out += superscript(charge_string());
    return out;
}

std::string Formula::display_string(bool unicode) const {
    if (is_electron()) return unicode ? to_unicode_string() : to_string();
    std::string src(trim(m_source));
    static const char* const tags[] = { "(aq)", "(s)", "(l)", "(g)", "(sc)", "(cr)", "(pl)" };
    for (const char* tag : tags) {
        const std::string_view t(tag);
        if (src.size() > t.size() && std::string_view(src).substr(src.size() - t.size()) == t) {
            src = std::string(trim(std::string_view(src).substr(0, src.size() - t.size())));
            break;
        }
    }
    std::string compact;
    for (char c : src) {
        if (c != ' ') compact += c;
    }
    bool usable = !compact.empty();
    if (usable) {
        try {
            const Formula check = Formula::parse(compact);
            usable = check.same_composition(*this) && check.charge() == m_charge;
        } catch (...) {
            usable = false;
        }
    }
    if (!usable) return unicode ? to_unicode_string() : to_string();
    if (!unicode) return compact;
    std::string out;
    const std::size_t caret = compact.find('^');
    const std::string body = caret == std::string::npos ? compact : compact.substr(0, caret);
    for (std::size_t i = 0; i < body.size(); ++i) {
        const char c = body[i];
        if (c == '*' || c == '.') {
            out += "\xC2\xB7";
            continue;
        }
        const bool after_symbol = i > 0 && (is_ascii_lower(body[i - 1]) || is_ascii_upper(body[i - 1]) || body[i - 1] == ')' || body[i - 1] == ']' || (is_ascii_digit(body[i - 1]) && !out.empty() && out.back() != ' ' && static_cast<unsigned char>(out.back()) >= 0x80));
        if (is_ascii_digit(c) && after_symbol) out += subscript(std::string(1, c));
        else out += c;
    }
    if (caret != std::string::npos) out += superscript(compact.substr(caret + 1));
    else if (m_charge != 0 && body.size() == compact.size() && compact.find('+') == std::string::npos && compact.find('-') == std::string::npos) out += superscript(charge_string());
    return out;
}

Formula Formula::operator+(const Formula& o) const {
    Formula f = *this;
    f += o;
    return f;
}

Formula& Formula::operator+=(const Formula& o) {
    for (const FormulaTerm& t : o.m_terms) add(t.element, t.mass_number, t.count);
    m_charge += o.m_charge;
    m_source = to_string();
    return *this;
}

bool Formula::same_composition(const Formula& o) const noexcept {
    if (m_charge != o.m_charge) return false;
    for (const FormulaTerm& t : m_terms) {
        double other = 0.0;
        for (const FormulaTerm& u : o.m_terms) {
            if (u.element == t.element && u.mass_number == t.mass_number) other += u.count;
        }
        double mine = 0.0;
        for (const FormulaTerm& u : m_terms) {
            if (u.element == t.element && u.mass_number == t.mass_number) mine += u.count;
        }
        if (std::fabs(mine - other) > 1e-9) return false;
    }
    for (const FormulaTerm& u : o.m_terms) {
        bool found = false;
        for (const FormulaTerm& t : m_terms) found = found || (t.element == u.element && t.mass_number == u.mass_number);
        if (!found) return false;
    }
    return true;
}

bool Formula::operator==(const Formula& o) const noexcept {
    return same_composition(o);
}

Formula operator*(double factor, const Formula& f) {
    return f.scaled(factor);
}

std::vector<OxidationAssignment> assign_oxidation_states(const Formula& formula) {
    std::vector<OxidationAssignment> out;
    if (!formula.is_integral() || formula.empty()) return out;
    std::vector<std::pair<const Element*, int>> comp;
    for (const auto& p : formula.composition()) comp.emplace_back(p.first, static_cast<int>(std::lround(p.second)));
    const int q = formula.charge();

    if (comp.size() == 1) {
        out.push_back(OxidationAssignment{ comp[0].first, AverageOxidationState(q, comp[0].second) });
        return out;
    }

    bool all_metals = true;
    for (const auto& p : comp) all_metals = all_metals && p.first->is_metal();
    if (all_metals && q == 0) {
        for (const auto& p : comp) out.push_back(OxidationAssignment{ p.first, AverageOxidationState(0) });
        return out;
    }

    std::map<int, int> fixed;
    bool has_fluorine = false;
    double highest_other_chi = 0.0;
    for (const auto& p : comp) {
        if (p.first->atomic_number == 9) has_fluorine = true;
        if (p.first->atomic_number != 1) highest_other_chi = std::max(highest_other_chi, value_or(p.first->electronegativity_pauling, 0.0));
    }
    for (const auto& p : comp) {
        const Element& e = *p.first;
        const int z = e.atomic_number;
        if (z == 9) fixed[z] = -1;
        else if (e.group == 1 && z != 1) fixed[z] = 1;
        else if (e.group == 2) fixed[z] = 2;
        else if (z == 13 || z == 21 || z == 31 || z == 39) fixed[z] = 3;
        else if (z == 30 || z == 48) fixed[z] = 2;
        else if (z == 47) fixed[z] = 1;
        else if (z == 1) fixed[z] = highest_other_chi < 2.1 ? -1 : 1;
    }

    std::vector<std::pair<const Element*, int>> unknown;
    for (const auto& p : comp) {
        if (!fixed.count(p.first->atomic_number)) unknown.push_back(p);
    }
    if (unknown.size() >= 2 && !has_fluorine) {
        for (const auto& p : unknown) {
            if (p.first->atomic_number == 8) fixed[8] = -2;
        }
        unknown.erase(std::remove_if(unknown.begin(), unknown.end(), [&](const std::pair<const Element*, int>& p) { return fixed.count(p.first->atomic_number) != 0; }), unknown.end());
    }

    int fixed_sum = 0;
    for (const auto& p : comp) {
        auto it = fixed.find(p.first->atomic_number);
        if (it != fixed.end()) fixed_sum += it->second * p.second;
    }

    std::map<int, AverageOxidationState> result;
    for (const auto& f : fixed) result[f.first] = AverageOxidationState(f.second);

    if (unknown.size() == 1) {
        result[unknown[0].first->atomic_number] = AverageOxidationState(q - fixed_sum, unknown[0].second);
    } else if (unknown.size() >= 2) {
        std::sort(unknown.begin(), unknown.end(), [](const std::pair<const Element*, int>& a, const std::pair<const Element*, int>& b) {
            return value_or(a.first->electronegativity_pauling, 0.0) < value_or(b.first->electronegativity_pauling, 0.0);
        });
        const auto solved = unknown.back();
        std::vector<std::pair<const Element*, int>> others(unknown.begin(), unknown.end() - 1);
        std::vector<std::vector<std::pair<int, bool>>> options;
        for (const auto& p : others) {
            std::vector<std::pair<int, bool>> o;
            o.emplace_back(0, false);
            for (const OxidationStateEntry& s : p.first->oxidation_states()) o.emplace_back(s.state, s.common);
            options.push_back(o);
        }
        double best_score = 1e18;
        std::vector<int> best_choice;
        AverageOxidationState best_solved;
        std::vector<std::size_t> idx(others.size(), 0);
        while (true) {
            double score = 0.0;
            int sum = fixed_sum;
            bool any_negative_other = false;
            for (std::size_t k = 0; k < others.size(); ++k) {
                const auto& o = options[k][idx[k]];
                sum += o.first * others[k].second;
                score += o.second ? 0.0 : (others[k].first->is_metal() ? 2.0 : 1.0);
                if (o.first == 0) score += 2.0;
                if (o.first < 0 && !others[k].first->is_metal()) any_negative_other = true;
                if (o.first < 0 && others[k].first->is_metal()) score += 4.0;
            }
            const int numerator = q - sum;
            const AverageOxidationState s(numerator, solved.second);
            if (!s.is_integer()) {
                score += 5.0;
            } else {
                const int v = s.numerator();
                if (!solved.first->has_oxidation_state(v)) score += 3.0;
                else if (!solved.first->is_common_oxidation_state(v)) score += 1.5;
            }
            if (s.value() > 0 && any_negative_other) score += 2.0;
            if (!s.in_range()) score += 100.0;
            if (score < best_score) {
                best_score = score;
                best_choice.assign(idx.begin(), idx.end());
                best_solved = s;
            }
            std::size_t k = 0;
            while (k < idx.size()) {
                if (++idx[k] < options[k].size()) break;
                idx[k] = 0;
                ++k;
            }
            if (k == idx.size()) break;
        }
        for (std::size_t k = 0; k < others.size(); ++k) result[others[k].first->atomic_number] = AverageOxidationState(options[k][best_choice[k]].first);
        result[solved.first->atomic_number] = best_solved;
    } else if (fixed_sum != q) {
        for (const auto& p : comp) {
            if (p.first->atomic_number == 8 && !has_fluorine) {
                int others_sum = 0;
                for (const auto& o : comp) {
                    if (o.first->atomic_number != 8) others_sum += fixed[o.first->atomic_number] * o.second;
                }
                result[8] = AverageOxidationState(q - others_sum, p.second);
            }
        }
    }

    for (const auto& p : comp) {
        auto it = result.find(p.first->atomic_number);
        if (it != result.end()) out.push_back(OxidationAssignment{ p.first, it->second });
    }
    return out;
}

AverageOxidationState oxidation_state_of(const Formula& formula, const Element& element) {
    for (const OxidationAssignment& a : assign_oxidation_states(formula)) {
        if (a.element == &element) return a.state;
    }
    throw std::invalid_argument("fizmo::chemistry: element is not in the formula");
}

} // namespace chemistry
} // namespace fizmo
