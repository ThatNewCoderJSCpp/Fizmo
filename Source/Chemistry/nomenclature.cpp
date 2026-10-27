#include "nomenclature.hpp"
#include "organic_naming.hpp"

#include "oxidation_state.hpp"
#include "substance.hpp"
#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace fizmo {
namespace chemistry {

namespace {

using Composition = std::map<int, double>;

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = ascii_lower(c);
    return out;
}

std::string collapse(std::string_view s) {
    std::string out;
    bool space = false;
    for (char c : trim(s)) {
        if (c == ' ' || c == '\t' || c == '\n') {
            space = true;
            continue;
        }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += ascii_lower(c);
    }
    return out;
}

bool starts_with(std::string_view s, std::string_view p) noexcept {
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

bool ends_with(std::string_view s, std::string_view p) noexcept {
    return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

std::vector<std::string> split_names(const char* list) {
    std::vector<std::string> out;
    std::string cur;
    for (const char* p = list; *p; ++p) {
        if (*p == ';') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += *p;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

Composition composition_of(const Formula& f) {
    Composition c;
    for (const auto& [e, n] : f.composition()) c[e->atomic_number] += n;
    return c;
}

Composition scaled(const Composition& c, double k) {
    Composition out;
    for (const auto& [z, n] : c) out[z] = n * k;
    return out;
}

Composition added(const Composition& a, const Composition& b) {
    Composition out = a;
    for (const auto& [z, n] : b) out[z] += n;
    return out;
}

bool same(const Composition& a, const Composition& b) {
    Composition x, y;
    for (const auto& [z, n] : a) {
        if (std::fabs(n) > 1e-9) x[z] = n;
    }
    for (const auto& [z, n] : b) {
        if (std::fabs(n) > 1e-9) y[z] = n;
    }
    if (x.size() != y.size()) return false;
    for (const auto& [z, n] : x) {
        auto it = y.find(z);
        if (it == y.end() || std::fabs(it->second - n) > 1e-9) return false;
    }
    return true;
}

bool subset(const Composition& part, const Composition& whole) {
    for (const auto& [z, n] : part) {
        auto it = whole.find(z);
        if (it == whole.end() || it->second + 1e-9 < n) return false;
    }
    return true;
}

std::string count_text(double n) {
    if (std::fabs(n - std::round(n)) < 1e-9) {
        const long v = std::lround(n);
        return v == 1 ? std::string() : std::to_string(v);
    }
    return format_number(n, 4);
}

std::string charge_suffix(int charge) {
    if (charge == 0) return std::string();
    const int a = charge < 0 ? -charge : charge;
    return (a != 1 ? std::to_string(a) : std::string()) + (charge < 0 ? "-" : "+");
}

std::string unicode_formula(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (is_ascii_digit(c) && i > 0) out += subscript(std::string(1, c));
        else out += c;
    }
    return out;
}

const char* const PREFIXES[] = { "", "mono", "di", "tri", "tetra", "penta", "hexa", "hepta", "octa", "nona", "deca", "undeca", "dodeca" };

const char* const STEMS[] = { "", "meth", "eth", "prop", "but", "pent", "hex", "hept", "oct", "non", "dec", "undec", "dodec", "tridec", "tetradec", "pentadec", "hexadec", "heptadec", "octadec", "nonadec", "icos" };

constexpr int ELECTROPOSITIVE_ORDER[] = { 86, 54, 36, 18, 10, 2, 5, 14, 6, 51, 33, 15, 7, 1, 52, 34, 16, 85, 53, 35, 17, 8, 9 };

int naming_rank(const Element& e) {
    int i = 0;
    for (int z : ELECTROPOSITIVE_ORDER) {
        if (z == e.atomic_number) return i;
        ++i;
    }
    return static_cast<int>(value_or(e.electronegativity_pauling, 2.0) * 10.0) - 100;
}

struct Classical {
    int z;
    int low;
    const char* low_name;
    int high;
    const char* high_name;
};

const Classical CLASSICAL[] = {
    { 26, 2, "ferrous", 3, "ferric" }, { 29, 1, "cuprous", 2, "cupric" }, { 50, 2, "stannous", 4, "stannic" },
    { 82, 2, "plumbous", 4, "plumbic" }, { 80, 1, "mercurous", 2, "mercuric" }, { 27, 2, "cobaltous", 3, "cobaltic" },
    { 24, 2, "chromous", 3, "chromic" }, { 25, 2, "manganous", 3, "manganic" }, { 79, 1, "aurous", 3, "auric" },
    { 28, 2, "nickelous", 3, "nickelic" }, { 22, 3, "titanous", 4, "titanic" }, { 58, 3, "cerous", 4, "ceric" },
    { 23, 3, "vanadous", 4, "vanadic" }, { 51, 3, "antimonous", 5, "antimonic" }, { 33, 3, "arsenous", 5, "arsenic" },
};

struct RootEntry {
    int z;
    const char* root;
};

const RootEntry ROOTS[] = {
    { 1, "hydr" }, { 5, "bor" }, { 6, "carb" }, { 7, "nitr" }, { 8, "ox" }, { 9, "fluor" }, { 14, "silic" }, { 15, "phosph" },
    { 16, "sulf" }, { 17, "chlor" }, { 32, "german" }, { 33, "arsen" }, { 34, "selen" }, { 35, "brom" }, { 51, "antimon" },
    { 52, "tellur" }, { 53, "iod" }, { 84, "polon" }, { 85, "astat" },
};

struct AcidRoot {
    const char* anion;
    const char* acid;
};

const AcidRoot ACID_ROOTS[] = {
    { "sulfate", "sulfuric acid" }, { "sulfite", "sulfurous acid" }, { "sulfide", "hydrosulfuric acid" },
    { "phosphate", "phosphoric acid" }, { "phosphite", "phosphorous acid" }, { "phosphide", "hydrophosphoric acid" },
    { "cyanide", "hydrocyanic acid" }, { "azide", "hydrazoic acid" }, { "selenide", "hydroselenic acid" },
    { "telluride", "hydrotelluric acid" },
};

std::string element_lower_name(const Element& e) {
    std::string n = lower(e.name);
    return n;
}

double score_cation_state(const Element& e, int charge) {
    if (e.is_common_oxidation_state(charge)) return 0.0;
    if (e.has_oxidation_state(charge)) return 2.0;
    return 50.0;
}

struct IonCandidate {
    IonDescriptor ion;
    Composition comp;
    double penalty = 0.0;
};

std::vector<IonCandidate> cation_candidates(const Composition& total) {
    std::vector<IonCandidate> out;
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge <= 0) continue;
        IonCandidate c;
        c.ion = polyatomic_ion(p);
        c.comp = composition_of(c.ion.composition());
        if (!subset(c.comp, total)) continue;
        const std::string_view pf(p.formula);
        const bool classic = pf == "NH4" || pf == "H3O" || pf == "Hg2" || pf == "PH4" || pf == "N2H5" || pf == "CH3NH3" || pf == "N(CH3)4" || pf == "C5H5NH";
        c.penalty = std::string_view(p.category) == "complex" ? 1.5 : (classic ? 0.0 : 2.5);
        bool metal_inside = false;
        for (const auto& [z, k] : c.comp) metal_inside = metal_inside || PeriodicTable::element(z).is_metal();
        bool metal_in_total = false;
        for (const auto& [z, k] : total) metal_in_total = metal_in_total || PeriodicTable::element(z).is_metal();
        if (metal_in_total && !metal_inside) c.penalty += 3.0;
        out.push_back(c);
    }
    for (const auto& [z, n] : total) {
        const Element& e = PeriodicTable::element(z);
        if (!(e.is_metal() || (e.is_metalloid() && z != 14 && z != 5 && z != 52 && z != 34 && z != 33))) continue;
        std::set<int> states;
        for (const OxidationStateEntry& s : e.oxidation_states()) {
            if (s.state > 0) states.insert(s.state);
        }
        if (states.empty()) states.insert(e.most_common_oxidation_state() > 0 ? e.most_common_oxidation_state() : 1);
        for (int q : states) {
            IonCandidate c;
            c.ion = monatomic_ion(e, q);
            c.comp[z] = 1.0;
            c.penalty = score_cation_state(e, q);
            out.push_back(c);
        }
    }
    return out;
}

std::vector<IonCandidate> anion_candidates(const Composition& total) {
    std::vector<IonCandidate> out;
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge >= 0) continue;
        IonCandidate c;
        c.ion = polyatomic_ion(p);
        c.comp = composition_of(c.ion.composition());
        if (!subset(c.comp, total)) continue;
        const std::string_view cat(p.category);
        c.penalty = cat == "complex" ? 1.0 : (cat == "oxygen" && std::string_view(p.name) != "hydroxide" ? 0.6 : 0.0);
        if (std::string_view(p.name) == "superoxide" || std::string_view(p.name) == "ozonide") c.penalty += 0.5;
        if (std::string_view(p.name) == "acetylide") c.penalty += 0.3;
        out.push_back(c);
    }
    for (const auto& [z, n] : total) {
        const Element& e = PeriodicTable::element(z);
        if (e.is_metal() && z != 1) continue;
        if (e.is_noble_gas()) continue;
        int q = 0;
        for (const OxidationStateEntry& s : e.oxidation_states()) {
            if (s.state < 0 && (q == 0 || (s.common && s.state < q) || s.state < q)) q = s.state;
        }
        if (q == 0) {
            if (e.group >= 13 && e.group <= 17) q = static_cast<int>(e.group) - 18;
            else continue;
        }
        IonCandidate c;
        c.ion = monatomic_ion(e, q);
        c.comp[z] = 1.0;
        c.penalty = z == 1 ? 0.8 : 0.1;
        out.push_back(c);
    }
    return out;
}

struct IonicSolution {
    IonCandidate cation;
    IonCandidate anion;
    int a = 0;
    int b = 0;
    double score = 1e9;
    std::optional<IonCandidate> second;
    int a2 = 0;
};

std::string group_text(const IonDescriptor& ion, int count) {
    if (count == 1) return ion.formula;
    const bool needs_parens = ion.polyatomic && ion.entry && std::string_view(ion.entry->formula) != "Hg2";
    if (needs_parens) return "(" + ion.formula + ")" + std::to_string(count);
    if (ion.polyatomic) return ion.formula + std::to_string(count);
    return ion.formula + std::to_string(count);
}

std::optional<IonicSolution> solve_ionic(const Composition& total, int total_charge) {
    std::optional<IonicSolution> best;
    const std::vector<IonCandidate> cats = cation_candidates(total);
    const std::vector<IonCandidate> ans = anion_candidates(total);
    for (const IonCandidate& c : cats) {
        for (const IonCandidate& an : ans) {
            for (int a = 1; a <= 12; ++a) {
                for (int b = 1; b <= 12; ++b) {
                    if (a * c.ion.charge + b * an.ion.charge != total_charge) continue;
                    const Composition sum = added(scaled(c.comp, a), scaled(an.comp, b));
                    if (!same(sum, total)) continue;
                    double score = c.penalty + an.penalty + 0.01 * (a + b);
                    if (c.ion.polyatomic) score -= 0.05;
                    if (an.ion.polyatomic) score -= 0.2;
                    if (!best || score < best->score) best = IonicSolution{ c, an, a, b, score, std::nullopt, 0 };
                }
            }
        }
    }
    if (!best) {
        for (std::size_t i = 0; i < cats.size(); ++i) {
            for (std::size_t j = i + 1; j < cats.size(); ++j) {
                if (cats[i].ion.formula == cats[j].ion.formula) continue;
                for (const IonCandidate& an : ans) {
                    for (int a1 = 1; a1 <= 4; ++a1) {
                        for (int a2 = 1; a2 <= 4; ++a2) {
                            for (int b = 1; b <= 8; ++b) {
                                if (a1 * cats[i].ion.charge + a2 * cats[j].ion.charge + b * an.ion.charge != total_charge) continue;
                                const Composition sum = added(added(scaled(cats[i].comp, a1), scaled(cats[j].comp, a2)), scaled(an.comp, b));
                                if (!same(sum, total)) continue;
                                const double score = cats[i].penalty + cats[j].penalty + an.penalty + 0.01 * (a1 + a2 + b) + 1.0;
                                if (!best || score < best->score) {
                                    IonicSolution sol{ cats[i], an, a1, b, score, std::nullopt, 0 };
                                    sol.second = cats[j];
                                    sol.a2 = a2;
                                    best = sol;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return best;
}

std::string ionic_name(const IonDescriptor& cation, const IonDescriptor& anion) {
    return cation.name + " " + anion.name;
}

std::string ionic_formula(const IonDescriptor& cation, int a, const IonDescriptor& anion, int b) {
    return group_text(cation, a) + group_text(anion, b);
}

bool is_inorganic_carbon(const Composition& c) {
    if (!c.count(6)) return true;
    if (!c.count(1)) return true;
    return false;
}

bool has_metal(const Composition& c) {
    for (const auto& [z, n] : c) {
        if (PeriodicTable::element(z).is_metal()) return true;
    }
    return false;
}

int split_hydrate(std::string_view source, std::string& base) {
    base = std::string(source);
    const std::string s(source);
    std::size_t pos = std::string::npos;
    std::size_t sep_len = 0;
    const char* const seps[] = { "\xC2\xB7", "\xE2\x8B\x85", "\xE2\x80\xA2", "*", "." };
    for (const char* sep : seps) {
        const std::size_t p = s.find(sep);
        if (p != std::string::npos && p > 0) {
            pos = p;
            sep_len = std::string_view(sep).size();
            break;
        }
    }
    if (pos == std::string::npos) return 0;
    std::string tail = s.substr(pos + sep_len);
    std::size_t i = 0;
    while (i < tail.size() && tail[i] == ' ') ++i;
    int n = 0;
    while (i < tail.size() && is_ascii_digit(tail[i])) n = n * 10 + (tail[i++] - '0');
    if (n == 0) n = 1;
    if (tail.substr(i) != "H2O") return 0;
    base = s.substr(0, pos);
    return n;
}

std::string capitalize_formula_phase(std::string_view s) {
    std::string out(trim(s));
    const std::size_t paren = out.rfind('(');
    if (paren != std::string::npos && out.back() == ')') {
        const std::string inside = lower(std::string_view(out).substr(paren + 1, out.size() - paren - 2));
        if (inside == "aq" || inside == "s" || inside == "l" || inside == "g") out = out.substr(0, paren);
    }
    return out;
}

const char* const MULTIPLIER_WORDS[] = { "", "mono", "di", "tri", "tetra", "penta", "hexa", "hepta", "octa", "nona", "deca" };

int read_prefix(std::string_view word, std::size_t& used) {
    static const struct {
        const char* p;
        int n;
    } table[] = {
        { "dodeca", 12 }, { "undeca", 11 }, { "deca", 10 }, { "dec", 10 }, { "ennea", 9 }, { "nona", 9 }, { "non", 9 }, { "octa", 8 }, { "oct", 8 },
        { "hepta", 7 }, { "hept", 7 }, { "hexa", 6 }, { "hex", 6 }, { "penta", 5 }, { "pent", 5 }, { "tetra", 4 }, { "tetr", 4 },
        { "tri", 3 }, { "di", 2 }, { "mono", 1 }, { "mon", 1 },
    };
    for (const auto& t : table) {
        if (starts_with(word, t.p)) {
            used = std::string_view(t.p).size();
            return t.n;
        }
    }
    used = 0;
    return 1;
}

const Element* element_from_root(std::string_view root_with_ide) {
    std::string_view r = root_with_ide;
    if (ends_with(r, "ide")) r = r.substr(0, r.size() - 3);
    for (const RootEntry& re : ROOTS) {
        if (r == re.root) return &PeriodicTable::element(re.z);
    }
    if (r == "sulph") return &PeriodicTable::element(16);
    if (r == "phosphor") return &PeriodicTable::element(15);
    for (const Element& e : PeriodicTable::elements()) {
        const std::string n = lower(e.name);
        if (r == n || (r.size() >= 3 && starts_with(n, r) && n.size() - r.size() <= 3)) return &e;
    }
    return nullptr;
}

const Element* element_from_name(std::string_view word) {
    const std::string w = lower(word);
    if (w == "aluminium") return &PeriodicTable::element(13);
    if (w == "sulphur") return &PeriodicTable::element(16);
    if (w == "caesium") return &PeriodicTable::element(55);
    for (const Element& e : PeriodicTable::elements()) {
        if (w == lower(e.name)) return &e;
    }
    return nullptr;
}

int roman_value(std::string_view r) {
    int total = 0;
    int prev = 0;
    for (std::size_t i = r.size(); i-- > 0;) {
        int v = 0;
        switch (ascii_upper(r[i])) {
            case 'I': v = 1; break;
            case 'V': v = 5; break;
            case 'X': v = 10; break;
            default: return 0;
        }
        if (v < prev) total -= v;
        else total += v;
        prev = v;
    }
    return total;
}

std::optional<IonDescriptor> cation_from_words(const std::string& words) {
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge <= 0) continue;
        if (words == lower(p.name)) return polyatomic_ion(p);
        for (const std::string& alt : split_names(p.other_names)) {
            if (words == lower(alt)) return polyatomic_ion(p);
        }
    }
    const std::size_t paren = words.find('(');
    if (paren != std::string::npos) {
        const std::string name = std::string(trim(words.substr(0, paren)));
        const std::size_t close = words.find(')', paren);
        const std::string inside = close == std::string::npos ? std::string() : words.substr(paren + 1, close - paren - 1);
        const Element* e = element_from_name(name);
        int q = roman_value(inside);
        if (!q) q = std::atoi(inside.c_str());
        if (e && q > 0) return monatomic_ion(*e, q);
        return std::nullopt;
    }
    for (const Classical& c : CLASSICAL) {
        if (words == c.low_name) return monatomic_ion(PeriodicTable::element(c.z), c.low);
        if (words == c.high_name) return monatomic_ion(PeriodicTable::element(c.z), c.high);
    }
    if (words == "hydrogen") return monatomic_ion(PeriodicTable::element(1), 1);
    if (const Element* e = element_from_name(words)) {
        if (!e->is_metal()) return std::nullopt;
        int q = e->most_common_oxidation_state();
        if (q <= 0) {
            for (const OxidationStateEntry& s : e->oxidation_states()) {
                if (s.state > 0) {
                    q = s.state;
                    break;
                }
            }
        }
        if (q > 0) return monatomic_ion(*e, q);
    }
    return std::nullopt;
}

std::optional<IonDescriptor> anion_from_words(const std::string& words) {
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge >= 0) continue;
        if (words == lower(p.name)) return polyatomic_ion(p);
        for (const std::string& alt : split_names(p.other_names)) {
            if (words == lower(alt)) return polyatomic_ion(p);
        }
    }
    if (ends_with(words, "ide")) {
        if (const Element* e = element_from_root(words)) {
            if (monatomic_anion_name(*e) == words) {
                int q = static_cast<int>(e->group) - 18;
                if (e->atomic_number == 1) q = -1;
                for (const OxidationStateEntry& s : e->oxidation_states()) {
                    if (s.state < 0 && s.state < q) q = s.state;
                }
                if (e->group >= 13 && e->group <= 17) q = static_cast<int>(e->group) - 18;
                if (e->atomic_number == 1) q = -1;
                if (q < 0) return monatomic_ion(*e, q);
            }
        }
    }
    return std::nullopt;
}

struct Stem {
    const char* text;
    int carbons;
};

const Stem STEM_TABLE[] = {
    { "icos", 20 }, { "eicos", 20 }, { "nonadec", 19 }, { "octadec", 18 }, { "heptadec", 17 }, { "hexadec", 16 }, { "pentadec", 15 },
    { "tetradec", 14 }, { "tridec", 13 }, { "dodec", 12 }, { "undec", 11 }, { "dec", 10 }, { "non", 9 }, { "oct", 8 }, { "hept", 7 },
    { "hex", 6 }, { "pent", 5 }, { "but", 4 }, { "prop", 3 }, { "eth", 2 }, { "meth", 1 },
};

int read_stem(std::string_view word, std::size_t& used) {
    for (const Stem& s : STEM_TABLE) {
        if (starts_with(word, s.text)) {
            used = std::string_view(s.text).size();
            return s.carbons;
        }
    }
    used = 0;
    return 0;
}

std::string ch(int c, int h) {
    std::string out;
    if (c > 0) out += "C" + count_text(c);
    if (h > 0) out += "H" + count_text(h);
    return out;
}

std::string alkyl(int n) {
    if (n == 1) return "CH3";
    return "C" + std::to_string(n) + "H" + std::to_string(2 * n + 1);
}

std::optional<CompoundName> organic_from_name(const std::string& raw) {
    std::string name = raw;
    CompoundName out;
    out.kind = CompoundKind::Organic;
    if (ends_with(name, " acid")) {
        std::string base = name.substr(0, name.size() - 5);
        if (ends_with(base, "anoic")) {
            std::size_t used = 0;
            const int n = read_stem(base, used);
            if (n > 0 && used + 5 == base.size()) {
                out.formula = n == 1 ? "HCOOH" : alkyl(n - 1) + "COOH";
                out.name = name;
                out.kind = CompoundKind::Acid;
                out.ok = true;
                return out;
            }
        }
        return std::nullopt;
    }
    const std::size_t space = name.find(' ');
    if (space != std::string::npos) {
        const std::string first = name.substr(0, space);
        const std::string second = name.substr(space + 1);
        if (ends_with(first, "yl") && ends_with(second, "anoate")) {
            std::size_t u1 = 0, u2 = 0;
            const int r = read_stem(first, u1);
            const int n = read_stem(second, u2);
            if (r > 0 && n > 0 && u1 + 2 == first.size() && u2 + 6 == second.size()) {
                out.formula = (n == 1 ? std::string("HCOO") : alkyl(n - 1) + "COO") + alkyl(r);
                out.name = name;
                out.ok = true;
                return out;
            }
        }
        return std::nullopt;
    }
    std::string word = name;
    for (const char* sep : { "-1-", "-2-", "-3-" }) {
        const std::size_t p = word.find(sep);
        if (p != std::string::npos) word = word.substr(0, p) + word.substr(p + 2);
    }
    if (word.find("-") != std::string::npos) {
        const std::size_t p = word.find('-');
        word = word.substr(0, p) + word.substr(p + 1);
    }
    int halo_count = 0;
    int halo_z = 0;
    {
        std::size_t used = 0;
        const int mult = read_prefix(word, used);
        const std::string rest = word.substr(used);
        static const struct {
            const char* p;
            int z;
        } halos[] = { { "fluoro", 9 }, { "chloro", 17 }, { "bromo", 35 }, { "iodo", 53 } };
        for (const auto& hx : halos) {
            if (starts_with(rest, hx.p)) {
                halo_count = used > 0 ? mult : 1;
                halo_z = hx.z;
                word = rest.substr(std::string_view(hx.p).size());
                break;
            }
            if (starts_with(word, hx.p)) {
                halo_count = 1;
                halo_z = hx.z;
                word = word.substr(std::string_view(hx.p).size());
                break;
            }
        }
    }
    std::size_t used = 0;
    const int n = read_stem(word, used);
    if (n == 0) return std::nullopt;
    const std::string suffix = word.substr(used);
    std::string formula;
    if (suffix == "ane") formula = ch(n, 2 * n + 2);
    else if (suffix == "ene" && n >= 2) formula = ch(n, 2 * n);
    else if (suffix == "yne" && n >= 2) formula = ch(n, 2 * n - 2);
    else if (suffix == "anol" || suffix == "an1ol" || suffix == "anl1ol" || suffix == "an2ol") formula = alkyl(n) + "OH";
    else if (suffix == "anal") formula = n == 1 ? std::string("HCHO") : alkyl(n - 1) + "CHO";
    else if ((suffix == "anone" || suffix == "an2one") && n >= 3) formula = alkyl(1) + "CO" + alkyl(n - 2);
    else if (suffix == "anamine" || suffix == "ylamine") formula = alkyl(n) + "NH2";
    else return std::nullopt;
    if (halo_count > 0) {
        const Formula f = Formula::parse(formula);
        const int h = static_cast<int>(f.count(1));
        if (halo_count > h) return std::nullopt;
        formula = ch(n, h - halo_count) + PeriodicTable::element(halo_z).symbol + count_text(halo_count);
    }
    out.formula = formula;
    out.name = name;
    out.ok = true;
    return out;
}

} // namespace

const PolyatomicIonEntry* find_polyatomic_ion(std::string_view name_or_formula, int charge) {
    const std::string q = collapse(name_or_formula);
    std::string formula_part(trim(name_or_formula));
    int parsed_charge = charge;
    const std::size_t caret = formula_part.find('^');
    if (caret != std::string::npos) {
        const Formula f = Formula::parse(formula_part);
        parsed_charge = f.charge();
        formula_part = formula_part.substr(0, caret);
    }
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (parsed_charge != 0 && p.charge != parsed_charge) continue;
        if (q == lower(p.name)) return &p;
        for (const std::string& alt : split_names(p.other_names)) {
            if (q == lower(alt)) return &p;
        }
    }
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (parsed_charge != 0 && p.charge != parsed_charge) continue;
        if (formula_part == p.formula) return &p;
    }
    try {
        Formula f = Formula::parse(name_or_formula);
        const Composition c = composition_of(f);
        for (const PolyatomicIonEntry& p : polyatomic_ions()) {
            if (f.charge() != 0 && p.charge != f.charge()) continue;
            if (parsed_charge != 0 && p.charge != parsed_charge) continue;
            if (same(composition_of(ion_formula(p)), c)) return &p;
        }
    } catch (...) {
    }
    return nullptr;
}

std::vector<const PolyatomicIonEntry*> find_polyatomic_ions(const Formula& composition) {
    std::vector<const PolyatomicIonEntry*> out;
    const Composition c = composition_of(composition);
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (composition.charge() != 0 && p.charge != composition.charge()) continue;
        if (same(composition_of(ion_formula(p)), c)) out.push_back(&p);
    }
    return out;
}

std::vector<const PolyatomicIonEntry*> polyatomic_ions_in(std::string_view category) {
    std::vector<const PolyatomicIonEntry*> out;
    const std::string c = lower(category);
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (c.empty() || c == "all" || c == p.category) out.push_back(&p);
    }
    return out;
}

std::vector<std::string> polyatomic_ion_categories() {
    std::vector<std::string> out;
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (std::find(out.begin(), out.end(), p.category) == out.end()) out.emplace_back(p.category);
    }
    return out;
}

std::vector<const PolyatomicIonEntry*> search_polyatomic_ions(std::string_view text) {
    std::vector<const PolyatomicIonEntry*> out;
    const std::string q = collapse(text);
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (q.empty()) {
            out.push_back(&p);
            continue;
        }
        const std::string hay = lower(p.name) + ";" + lower(p.other_names) + ";" + lower(p.formula) + ";" + lower(p.acid) + ";" + lower(p.category);
        if (hay.find(q) != std::string::npos) out.push_back(&p);
    }
    return out;
}

Formula ion_formula(const PolyatomicIonEntry& ion) {
    Formula f = Formula::parse(ion.formula);
    f.set_charge(ion.charge);
    return f;
}

std::string ion_formula_text(const PolyatomicIonEntry& ion, bool unicode) {
    if (!unicode) return std::string(ion.formula) + "^" + charge_suffix(ion.charge);
    return unicode_formula(ion.formula) + superscript(charge_suffix(ion.charge));
}

std::vector<const CommonCompoundEntry*> find_common_compounds(std::string_view name_or_formula) {
    std::vector<const CommonCompoundEntry*> out;
    const std::string q = collapse(name_or_formula);
    const std::string raw(capitalize_formula_phase(name_or_formula));
    for (const CommonCompoundEntry& c : common_compounds()) {
        bool hit = q == lower(c.name) || raw == c.formula;
        for (const std::string& alt : split_names(c.other_names)) hit = hit || q == lower(alt);
        if (hit) out.push_back(&c);
    }
    if (!out.empty()) return out;
    try {
        const Formula f = Formula::parse(raw);
        if (f.empty() || f.charge() != 0) return out;
        const Composition comp = composition_of(f);
        for (const CommonCompoundEntry& c : common_compounds()) {
            if (same(composition_of(Formula::parse(c.formula)), comp)) out.push_back(&c);
        }
    } catch (...) {
    }
    return out;
}

Formula IonDescriptor::composition() const {
    Formula f = Formula::parse(formula);
    f.set_charge(charge);
    return f;
}

std::string IonDescriptor::text(bool unicode) const {
    if (!unicode) return formula + "^" + charge_suffix(charge);
    return unicode_formula(formula) + superscript(charge_suffix(charge));
}

IonDescriptor monatomic_ion(const Element& element, int charge) {
    IonDescriptor d;
    d.formula = element.symbol;
    d.charge = charge;
    d.element = &element;
    if (charge < 0) d.name = monatomic_anion_name(element);
    else d.name = monatomic_cation_name(element, charge);
    if (charge > 0) d.classical_name = classical_cation_name(element, charge);
    return d;
}

IonDescriptor polyatomic_ion(const PolyatomicIonEntry& entry) {
    IonDescriptor d;
    d.formula = entry.formula;
    d.charge = entry.charge;
    d.name = entry.name;
    d.polyatomic = true;
    d.entry = &entry;
    const std::vector<std::string> alts = split_names(entry.other_names);
    if (!alts.empty()) d.classical_name = alts.front();
    return d;
}

std::optional<IonDescriptor> parse_ion(std::string_view text) {
    const std::string t(trim(text));
    if (t.empty()) return std::nullopt;
    std::string words = collapse(t);
    if (ends_with(words, " ion")) words = words.substr(0, words.size() - 4);
    if (auto c = cation_from_words(words)) return c;
    if (auto a = anion_from_words(words)) return a;
    try {
        std::string text_no_space;
        for (char ch2 : t) {
            if (ch2 != ' ') text_no_space += ch2;
        }
        const auto describe = [](const std::string& formula_text, int q) -> std::optional<IonDescriptor> {
            if (q == 0 || formula_text.empty()) return std::nullopt;
            Formula f = Formula::parse(formula_text);
            if (f.empty()) return std::nullopt;
            if (f.is_element() && std::fabs(f.atom_count() - 1.0) < 1e-9) return monatomic_ion(*f.elements().front(), q);
            for (const PolyatomicIonEntry& p : polyatomic_ions()) {
                if (p.charge == q && (formula_text == p.formula || same(composition_of(ion_formula(p)), composition_of(f)))) return polyatomic_ion(p);
            }
            return std::nullopt;
        };
        if (text_no_space.find('^') != std::string::npos) {
            const Formula f = Formula::parse(text_no_space);
            const std::string base = text_no_space.substr(0, text_no_space.find('^'));
            if (auto d = describe(base, f.charge())) return d;
            IonDescriptor d;
            d.formula = base;
            d.charge = f.charge();
            d.name = base + " ion";
            d.polyatomic = true;
            if (d.charge != 0) return d;
            return std::nullopt;
        }
        std::size_t i = text_no_space.size();
        while (i > 0 && (text_no_space[i - 1] == '+' || text_no_space[i - 1] == '-')) --i;
        if (i == text_no_space.size() || i == 0) return std::nullopt;
        const std::string signs = text_no_space.substr(i);
        const int sign = signs[0] == '+' ? 1 : -1;
        const std::string body = text_no_space.substr(0, i);
        std::size_t j = body.size();
        while (j > 0 && is_ascii_digit(body[j - 1])) --j;
        std::optional<IonDescriptor> whole;
        std::optional<IonDescriptor> split;
        try {
            whole = describe(body, sign * static_cast<int>(signs.size()));
        } catch (...) {
        }
        if (signs.size() == 1 && j < body.size()) {
            for (std::size_t k = std::max<std::size_t>(j, 1); k < body.size() && !split; ++k) {
                try {
                    split = describe(body.substr(0, k), sign * std::atoi(body.substr(k).c_str()));
                } catch (...) {
                }
            }
        }
        if (split && split->element) return split;
        if (whole) return whole;
        if (split) return split;
        IonDescriptor d;
        d.formula = body;
        d.charge = sign * static_cast<int>(signs.size());
        d.name = body + " ion";
        d.polyatomic = true;
        return d;
    } catch (...) {
    }
    return std::nullopt;
}

std::vector<IonDescriptor> common_cations() {
    std::vector<IonDescriptor> out;
    const int metals[][2] = { { 1, 1 }, { 3, 1 }, { 11, 1 }, { 19, 1 }, { 37, 1 }, { 55, 1 }, { 4, 2 }, { 12, 2 }, { 20, 2 }, { 38, 2 }, { 56, 2 },
                              { 13, 3 }, { 30, 2 }, { 47, 1 }, { 48, 2 }, { 26, 2 }, { 26, 3 }, { 29, 1 }, { 29, 2 }, { 27, 2 }, { 27, 3 },
                              { 28, 2 }, { 24, 2 }, { 24, 3 }, { 25, 2 }, { 25, 4 }, { 50, 2 }, { 50, 4 }, { 82, 2 }, { 82, 4 }, { 80, 2 },
                              { 79, 1 }, { 79, 3 }, { 22, 4 }, { 78, 2 }, { 78, 4 }, { 31, 3 }, { 49, 3 }, { 81, 1 }, { 83, 3 }, { 51, 3 } };
    for (const auto& m : metals) out.push_back(monatomic_ion(PeriodicTable::element(m[0]), m[1]));
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge > 0) out.push_back(polyatomic_ion(p));
    }
    return out;
}

std::vector<IonDescriptor> common_anions() {
    std::vector<IonDescriptor> out;
    const int nonmetals[][2] = { { 9, -1 }, { 17, -1 }, { 35, -1 }, { 53, -1 }, { 8, -2 }, { 16, -2 }, { 34, -2 }, { 52, -2 }, { 7, -3 }, { 15, -3 }, { 1, -1 }, { 6, -4 } };
    for (const auto& m : nonmetals) out.push_back(monatomic_ion(PeriodicTable::element(m[0]), m[1]));
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge < 0) out.push_back(polyatomic_ion(p));
    }
    return out;
}

IonicCombination combine_ions(const IonDescriptor& cation, const IonDescriptor& anion) {
    IonicCombination out;
    out.cation = cation;
    out.anion = anion;
    if (cation.charge <= 0 || anion.charge >= 0) return out;
    const int qc = cation.charge;
    const int qa = -anion.charge;
    int a = qa, b = qc;
    int g = a, h = b;
    while (h) {
        const int t = g % h;
        g = h;
        h = t;
    }
    a /= g;
    b /= g;
    out.cation_count = a;
    out.anion_count = b;
    if (cation.element && cation.element->atomic_number == 1 && !anion.polyatomic) out.formula = "H" + count_text(a) + group_text(anion, b);
    else out.formula = ionic_formula(cation, a, anion, b);
    if (cation.element && cation.element->atomic_number == 1) {
        out.name = acid_name_for_anion(anion, true);
    } else {
        out.name = ionic_name(cation, anion);
    }
    return out;
}

std::string element_root(const Element& element) {
    for (const RootEntry& r : ROOTS) {
        if (r.z == element.atomic_number) return r.root;
    }
    std::string n = lower(element.name);
    static const char* const endings[] = { "ium", "ine", "ogen", "on", "en", "ur", "us", "um", "ic", "y" };
    for (const char* e : endings) {
        if (ends_with(n, e) && n.size() > std::string_view(e).size() + 2) return n.substr(0, n.size() - std::string_view(e).size());
    }
    return n;
}

std::string monatomic_anion_name(const Element& element) {
    if (element.atomic_number == 8) return "oxide";
    return element_root(element) + "ide";
}

bool has_single_common_charge(const Element& element) noexcept {
    const int z = element.atomic_number;
    return element.group == 1 || element.group == 2 || z == 13 || z == 21 || z == 30 || z == 31 || z == 39 || z == 47 || z == 48 || z == 57;
}

std::string monatomic_cation_name(const Element& element, int charge, bool force_numeral) {
    const std::string base = element_lower_name(element);
    if (element.atomic_number == 1) return "hydrogen";
    if (!force_numeral && has_single_common_charge(element) && element.is_common_oxidation_state(charge)) return base;
    return base + "(" + OxidationState(charge).roman() + ")";
}

std::string classical_cation_name(const Element& element, int charge) {
    for (const Classical& c : CLASSICAL) {
        if (c.z != element.atomic_number) continue;
        if (charge == c.low) return c.low_name;
        if (charge == c.high) return c.high_name;
    }
    return std::string();
}

std::string acid_name_for_anion(const IonDescriptor& anion, bool aqueous) {
    if (anion.entry && *anion.entry->acid) {
        const std::string acid = anion.entry->acid;
        if (ends_with(acid, " acid")) return acid;
    }
    const std::string n = anion.name;
    for (const AcidRoot& r : ACID_ROOTS) {
        if (n == r.anion) return aqueous || !ends_with(n, "ide") ? r.acid : "hydrogen " + n;
    }
    if (ends_with(n, "ide")) {
        if (!aqueous) return "hydrogen " + n;
        return "hydro" + n.substr(0, n.size() - 3) + "ic acid";
    }
    if (ends_with(n, "ate")) return n.substr(0, n.size() - 3) + "ic acid";
    if (ends_with(n, "ite")) return n.substr(0, n.size() - 3) + "ous acid";
    return "hydrogen " + n;
}

const char* numeric_prefix(int n) noexcept {
    if (n < 0 || n > 12) return "";
    return PREFIXES[n];
}

std::string prefixed(int n, const std::string& word, bool drop_mono) {
    if (n == 1 && drop_mono) return word;
    std::string p = numeric_prefix(n);
    if (p.empty()) return std::to_string(n) + "-" + word;
    if (!word.empty() && (word[0] == 'o' || word[0] == 'a') && (p.back() == 'a' || p.back() == 'o')) p.pop_back();
    return p + word;
}

std::string hydrate_suffix(int waters) {
    if (waters <= 0) return std::string();
    if (waters == 1) return "monohydrate";
    if (waters <= 12) return std::string(numeric_prefix(waters)) + "hydrate";
    return std::to_string(waters) + "-hydrate";
}

std::string alkane_stem(int carbons) {
    if (carbons >= 1 && carbons <= 20) return STEMS[carbons];
    return "C" + std::to_string(carbons) + "-alk";
}

std::string organic_name(const Formula& formula, std::string_view source) {
    const std::string src(trim(source));
    const int c = static_cast<int>(std::lround(formula.count(6)));
    const int h = static_cast<int>(std::lround(formula.count(1)));
    const int o = static_cast<int>(std::lround(formula.count(8)));
    const int n = static_cast<int>(std::lround(formula.count(7)));
    int halogens = 0;
    int halo_z = 0;
    for (int z : { 9, 17, 35, 53 }) {
        const int k = static_cast<int>(std::lround(formula.count(z)));
        if (k > 0) {
            halogens += k;
            halo_z = z;
        }
    }
    const int others = formula.element_count() - (c > 0) - (h > 0) - (o > 0) - (n > 0) - (halogens > 0 ? 1 : 0);
    if (c <= 0 || others > 0) return std::string();
    const auto alkyl_carbons = [](const std::string& s, std::size_t& pos) -> int {
        if (pos >= s.size()) return 0;
        if (s.compare(pos, 3, "CH3") == 0) {
            pos += 3;
            return 1;
        }
        if (s[pos] != 'C') return 0;
        std::size_t p = pos + 1;
        int k = 0;
        while (p < s.size() && is_ascii_digit(s[p])) k = k * 10 + (s[p++] - '0');
        if (k < 2 || p >= s.size() || s[p] != 'H') return 0;
        ++p;
        int hh = 0;
        while (p < s.size() && is_ascii_digit(s[p])) hh = hh * 10 + (s[p++] - '0');
        if (hh != 2 * k + 1) return 0;
        pos = p;
        return k;
    };
    if (!src.empty()) {
        std::size_t pos = 0;
        const int r = alkyl_carbons(src, pos);
        const std::string rest = pos < src.size() ? src.substr(pos) : std::string();
        if (r > 0) {
            if (rest == "OH") return alkane_stem(r) + (r <= 2 ? "anol" : "an-1-ol");
            if (rest == "COOH") return alkane_stem(r + 1) + "anoic acid";
            if (rest == "CHO") return alkane_stem(r + 1) + "anal";
            if (rest == "NH2") return alkane_stem(r) + "anamine";
            if (rest == "Cl" || rest == "Br" || rest == "I" || rest == "F") {
                const char* hp = rest == "Cl" ? "chloro" : rest == "Br" ? "bromo" : rest == "I" ? "iodo" : "fluoro";
                return std::string(hp) + alkane_stem(r) + "ane";
            }
            if (starts_with(rest, "COO")) {
                std::size_t p2 = 3;
                const int r2 = alkyl_carbons(rest, p2);
                if (r2 > 0 && p2 == rest.size()) return alkane_stem(r2) + "yl " + alkane_stem(r + 1) + "anoate";
            }
            if (starts_with(rest, "O")) {
                std::size_t p2 = 1;
                const int r2 = alkyl_carbons(rest, p2);
                if (r2 > 0 && p2 == rest.size()) {
                    if (r == r2) return std::string(r == 1 ? "dimethyl" : "di" + alkane_stem(r) + "yl") + " ether";
                    const int a = std::min(r, r2);
                    const int b = std::max(r, r2);
                    return alkane_stem(a) + "oxy" + alkane_stem(b) + "ane";
                }
            }
            if (starts_with(rest, "CO")) {
                std::size_t p2 = 2;
                const int r2 = alkyl_carbons(rest, p2);
                if (r2 > 0 && p2 == rest.size()) {
                    const int total = r + r2 + 1;
                    return alkane_stem(total) + (total == 3 ? "anone" : "an-2-one");
                }
            }
        }
        if (src == "HCOOH") return "methanoic acid";
        if (src == "HCHO") return "methanal";
    }
    if (o == 0 && n == 0 && halogens == 0) {
        if (h == 2 * c + 2) return alkane_stem(c) + "ane";
        if (h == 2 * c && c >= 2) return alkane_stem(c) + "ene";
        if (h == 2 * c - 2 && c >= 2) return alkane_stem(c) + "yne";
    }
    if (o == 0 && n == 0 && halogens > 0 && h + halogens == 2 * c + 2) {
        const char* hp = halo_z == 9 ? "fluoro" : halo_z == 17 ? "chloro" : halo_z == 35 ? "bromo" : "iodo";
        return prefixed(halogens, hp, true) + alkane_stem(c) + "ane";
    }
    if (n == 0 && halogens == 0 && o == 1 && h == 2 * c + 2) return alkane_stem(c) + (c <= 2 ? "anol" : "an-1-ol");
    if (n == 0 && halogens == 0 && o == 2 && h == 2 * c && c >= 1) return alkane_stem(c) + "anoic acid";
    if (n == 0 && halogens == 0 && c >= 3 && c <= 7 && h == 2 * c && o == c) return std::string();
    return std::string();
}

CompoundName name_of(const Formula& formula) {
    return name_compound(formula.source().empty() ? formula.to_string() : formula.source(), formula.phase());
}

CompoundName name_compound(std::string_view formula_text, Phase phase) {
    CompoundName out;
    std::string base;
    std::string cleaned = capitalize_formula_phase(formula_text);
    const int waters = split_hydrate(cleaned, base);
    Formula f;
    try {
        f = Formula::parse(base);
        if (phase == Phase::Unknown) phase = Formula::parse(formula_text).phase();
    } catch (const std::exception& ex) {
        out.explanation = ex.what();
        return out;
    }
    if (f.empty() && f.charge() == -1) {
        out.ok = true;
        out.name = "electron";
        out.kind = CompoundKind::MonatomicIon;
        out.formula = "e-";
        return out;
    }
    if (f.empty()) {
        out.explanation = "there's no formula to name";
        return out;
    }
    out.hydrate_waters = waters;
    const Composition comp = composition_of(f);
    const int charge = f.charge();
    std::string base_display = base;
    out.formula = waters > 0 ? base + "\xC2\xB7" + (waters > 1 ? std::to_string(waters) : std::string()) + "H2O" : base;
    const auto finish = [&](CompoundName r) {
        if (waters > 0) {
            r.name += " " + hydrate_suffix(waters);
            for (std::string& alt : r.other_names) alt += " " + hydrate_suffix(waters);
            r.hydrate_waters = waters;
            r.formula = out.formula;
            r.explanation += (r.explanation.empty() ? "" : " ") + std::string("The ") + hydrate_suffix(waters) + " has " + std::to_string(waters) + " water molecule" + (waters > 1 ? "s" : "") + " per formula unit.";
        }
        if (waters == 0) {
            for (const CommonCompoundEntry* cc : find_common_compounds(base)) {
                if (lower(cc->name) != lower(r.name) && std::find(r.other_names.begin(), r.other_names.end(), cc->name) == r.other_names.end()) r.other_names.push_back(cc->name);
                for (const std::string& alt : split_names(cc->other_names)) {
                    if (lower(alt) != lower(r.name) && std::find(r.other_names.begin(), r.other_names.end(), alt) == r.other_names.end()) r.other_names.push_back(alt);
                }
            }
        }
        if (r.formula.empty()) r.formula = out.formula;
        r.ok = !r.name.empty();
        return r;
    };

    const std::string structural = (charge == 0 && waters == 0 && !has_metal(comp)) ? iupac_name_from_condensed(base) : std::string();
    if (charge == 0 && waters == 0) {
        const std::vector<const CommonCompoundEntry*> common = find_common_compounds(base);
        const CommonCompoundEntry* exact = nullptr;
        for (const CommonCompoundEntry* cc : common) {
            if (base == cc->formula) {
                exact = cc;
                break;
            }
        }
        const bool organic = !is_inorganic_carbon(comp) && !has_metal(comp);
        if (exact && (std::string_view(exact->category) != "inorganic" || organic || !has_metal(comp))) {
            CompoundName r;
            r.name = exact->name;
            r.formula = base;
            r.other_names = split_names(exact->other_names);
            const std::string_view cat(exact->category);
            r.kind = cat == "carbohydrate" ? CompoundKind::Carbohydrate : (cat == "inorganic" ? CompoundKind::Covalent : CompoundKind::Organic);
            if (cat == "carboxylic acid") r.kind = CompoundKind::Acid;
            r.explanation = std::string("A common ") + exact->category + (cat == "inorganic" ? " compound." : ".");
            if (!structural.empty() && structural != r.name && std::find(r.other_names.begin(), r.other_names.end(), structural) == r.other_names.end()) r.other_names.insert(r.other_names.begin(), structural);
            if (common.size() > 1) {
                for (const CommonCompoundEntry* cc : common) {
                    if (cc != exact) r.other_names.push_back(std::string(cc->name) + " (same formula)");
                }
            }
            if (!has_metal(comp)) return finish(r);
        }
    }

    if (comp.size() == 1 && charge == 0) {
        const Element& e = PeriodicTable::element(comp.begin()->first);
        const int n = static_cast<int>(std::lround(comp.begin()->second));
        CompoundName r;
        r.kind = CompoundKind::Element;
        r.name = element_lower_name(e);
        if (e.atomic_number == 8 && n == 3) {
            r.name = "ozone";
            r.other_names.push_back("trioxygen");
        } else if (n > 1) {
            r.other_names.push_back(prefixed(n, element_lower_name(e), false));
            if (e.atomic_number == 15 && n == 4) r.other_names.push_back("white phosphorus");
        }
        r.explanation = n > 1 ? std::string("A molecule of ") + std::to_string(n) + " " + element_lower_name(e) + " atoms." : std::string("A single element.");
        if (e.is_diatomic() && n == 2) r.explanation = std::string(e.name) + " is one of the seven diatomic elements (H, N, O, F, Cl, Br, I).";
        return finish(r);
    }

    if (comp.size() == 1 && charge != 0 && std::fabs(comp.begin()->second - 1.0) < 1e-9) {
        const Element& e = PeriodicTable::element(comp.begin()->first);
        CompoundName r;
        r.kind = CompoundKind::MonatomicIon;
        IonDescriptor d = monatomic_ion(e, charge);
        r.name = d.name + " ion";
        if (!d.classical_name.empty()) r.other_names.push_back(d.classical_name + " ion");
        if (e.atomic_number == 1 && charge == 1) r.other_names.push_back("proton");
        r.ions.push_back(d);
        r.explanation = charge > 0 ? std::string(e.name) + " has lost " + std::to_string(charge) + " electron" + (charge > 1 ? "s." : ".") : std::string(e.name) + " has gained " + std::to_string(-charge) + " electron" + (charge < -1 ? "s." : ".");
        return finish(r);
    }

    if (charge != 0) {
        for (const PolyatomicIonEntry* p : find_polyatomic_ions(f)) {
            CompoundName r;
            r.kind = CompoundKind::PolyatomicIon;
            r.name = std::string(p->name) + " ion";
            for (const std::string& alt : split_names(p->other_names)) r.other_names.push_back(alt);
            r.ions.push_back(polyatomic_ion(*p));
            r.formula = ion_formula_text(*p, false);
            r.explanation = std::string("A ") + p->category + " polyatomic ion";
            if (*p->acid) r.explanation += std::string("; its parent is ") + p->acid + " (" + p->acid_formula + ")";
            r.explanation += ".";
            return finish(r);
        }
    }

    const bool metal = has_metal(comp);
    bool polyatomic_cation_present = false;
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        if (p.charge > 0 && std::string_view(p.category) != "complex" && subset(composition_of(ion_formula(p)), comp) && (std::string_view(p.formula) == "NH4" || std::string_view(p.formula) == "H3O" || std::string_view(p.formula) == "Hg2" || std::string_view(p.formula) == "PH4")) {
            polyatomic_cation_present = true;
        }
    }

    const bool starts_with_h = !base.empty() && base[0] == 'H' && (base.size() == 1 || !is_ascii_lower(base[1]));
    if (!metal && starts_with_h && comp.count(1) && charge == 0) {
        const int nh = static_cast<int>(std::lround(comp.at(1)));
        Composition rest = comp;
        for (int strip = nh; strip >= 1; --strip) {
            rest = comp;
            rest[1] -= strip;
            if (rest[1] < 1e-9) rest.erase(1);
            if (rest.empty()) break;
            std::optional<IonDescriptor> anion;
            for (const PolyatomicIonEntry& p : polyatomic_ions()) {
                if (p.charge != -strip) continue;
                if (same(composition_of(ion_formula(p)), rest)) {
                    anion = polyatomic_ion(p);
                    break;
                }
            }
            if (!anion && rest.size() == 1 && std::fabs(rest.begin()->second - 1.0) < 1e-9) {
                const Element& e = PeriodicTable::element(rest.begin()->first);
                if ((e.group == 17 && strip == 1) || (e.group == 16 && strip == 2 && e.atomic_number != 8)) anion = monatomic_ion(e, -strip);
            }
            if (anion) {
                CompoundName r;
                r.kind = CompoundKind::Acid;
                r.ions.push_back(monatomic_ion(PeriodicTable::element(1), 1));
                r.ions.push_back(*anion);
                const bool binary = !anion->polyatomic || std::string_view(anion->name) == "cyanide" || std::string_view(anion->name) == "azide";
                if (binary) {
                    if (phase == Phase::Aqueous) {
                        r.name = acid_name_for_anion(*anion, true);
                        r.other_names.push_back("hydrogen " + anion->name + " (as a gas)");
                    } else {
                        r.name = "hydrogen " + anion->name;
                        r.other_names.push_back(acid_name_for_anion(*anion, true) + " (in water)");
                    }
                    r.explanation = "A binary acid: H+ with the " + anion->name + " ion. In water it's named hydro-___-ic acid.";
                } else {
                    r.name = acid_name_for_anion(*anion, true);
                    r.other_names.push_back(prefixed(strip, "hydrogen", true) + " " + anion->name);
                    r.explanation = "An oxyacid: H+ with the " + anion->name + " ion (-ate becomes -ic acid, -ite becomes -ous acid).";
                }
                return finish(r);
            }
        }
    }

    if (metal || polyatomic_cation_present) {
        auto sol = solve_ionic(comp, charge);
        if (sol && sol->score >= 1.0 && waters == 0 && !sol->second) {
            for (const CommonCompoundEntry* cc : find_common_compounds(base)) {
                if (base != cc->formula) continue;
                CompoundName r;
                r.kind = CompoundKind::Ionic;
                r.name = cc->name;
                r.other_names = split_names(cc->other_names);
                r.formula = base;
                r.explanation = "A compound with mixed or unusual charges.";
                return finish(r);
            }
        }
        if (sol && sol->second) {
            CompoundName r;
            r.kind = CompoundKind::Ionic;
            const bool swap = sol->second->ion.charge < sol->cation.ion.charge;
            const IonDescriptor& c1 = swap ? sol->second->ion : sol->cation.ion;
            const IonDescriptor& c2 = swap ? sol->cation.ion : sol->second->ion;
            const int n1 = swap ? sol->a2 : sol->a;
            const int n2 = swap ? sol->a : sol->a2;
            const IonDescriptor& a = sol->anion.ion;
            r.ions = { c1, c2, a };
            r.name = c1.name + " " + c2.name + " " + a.name;
            r.formula = group_text(c1, n1) + group_text(c2, n2) + group_text(a, sol->b);
            if (waters > 0) r.formula += "\xC2\xB7" + (waters > 1 ? std::to_string(waters) : std::string()) + "H2O";
            r.explanation = "A double salt with two cations: " + c1.text() + " and " + c2.text() + ", balanced by " + a.text() + ".";
            if (std::string_view(a.name) == "sulfate" && c2.charge == 3 && c1.charge == 1) r.other_names.push_back("an alum (as the dodecahydrate)");
            return finish(r);
        }
        if (sol) {
            CompoundName r;
            r.kind = CompoundKind::Ionic;
            const IonDescriptor& c = sol->cation.ion;
            const IonDescriptor& a = sol->anion.ion;
            r.ions = { c, a };
            r.name = ionic_name(c, a);
            if (!c.classical_name.empty() && !c.polyatomic) r.other_names.push_back(c.classical_name + " " + a.name);
            if (a.entry) {
                for (const std::string& alt : split_names(a.entry->other_names)) {
                    bool formula_like = false;
                    for (char ch2 : alt) formula_like = formula_like || is_ascii_digit(ch2) || is_ascii_upper(ch2);
                    if (!formula_like && alt.find(' ') == std::string::npos && alt.find('(') == std::string::npos && alt != a.name && alt.size() > 3) r.other_names.push_back(c.name + " " + alt);
                }
            }
            r.formula = ionic_formula(c, sol->a, a, sol->b);
            if (waters > 0) r.formula += "\xC2\xB7" + (waters > 1 ? std::to_string(waters) : std::string()) + "H2O";
            if (a.polyatomic && a.entry && std::string_view(a.entry->name) == "hydroxide") r.kind = CompoundKind::Base;
            r.explanation = std::to_string(sol->a) + " " + c.text() + " for every " + std::to_string(sol->b) + " " + a.text() + ", so the charges cancel.";
            if (!c.polyatomic && c.element && !has_single_common_charge(*c.element)) r.explanation += " " + std::string(c.element->name) + " has more than one common charge, so the Roman numeral gives it (" + format_signed(c.charge) + ").";
            return finish(r);
        }
        if (metal && waters == 0) {
            for (const CommonCompoundEntry* cc : find_common_compounds(base)) {
                CompoundName r;
                r.kind = CompoundKind::Ionic;
                r.name = cc->name;
                r.other_names = split_names(cc->other_names);
                r.explanation = "A compound with mixed or unusual charges.";
                return finish(r);
            }
        }
    }

    if (!structural.empty()) {
        CompoundName r;
        r.kind = ends_with(structural, "acid") ? CompoundKind::Acid : CompoundKind::Organic;
        r.name = structural;
        r.explanation = "Named from its structure with IUPAC rules: the longest carbon chain (or the ring) is the parent, it is numbered to give the main group and then the branches the lowest numbers, and the branches are listed in alphabetical order.";
        return finish(r);
    }
    if (!is_inorganic_carbon(comp) && !metal) {
        const std::string org = organic_name(f, base);
        if (!org.empty()) {
            CompoundName r;
            r.kind = ends_with(org, "acid") ? CompoundKind::Acid : CompoundKind::Organic;
            r.name = org;
            r.explanation = "Named with IUPAC organic rules: the stem gives the number of carbons and the ending gives the functional group.";
            return finish(r);
        }
        const int c = static_cast<int>(std::lround(f.count(6)));
        const int h = static_cast<int>(std::lround(f.count(1)));
        const int o = static_cast<int>(std::lround(f.count(8)));
        if (f.element_count() == 3 && c >= 3 && c <= 7 && h == 2 * c && o == c) {
            static const char* const kinds[] = { "", "", "", "triose", "tetrose", "pentose", "hexose", "heptose" };
            CompoundName r;
            r.kind = CompoundKind::Carbohydrate;
            r.name = std::string("a ") + kinds[c] + " (monosaccharide)";
            r.explanation = "C" + std::to_string(c) + "(H2O)" + std::to_string(c) + ": a simple sugar. Aldoses have a C=O at the end (an aldehyde) and ketoses have it inside the chain (a ketone).";
            return finish(r);
        }
        if (f.element_count() == 3 && c % 6 == 0 && c >= 12 && h == 2 * (c - c / 6) + 2 && o == c - c / 6 + 1) {
            CompoundName r;
            r.kind = CompoundKind::Carbohydrate;
            r.name = c == 12 ? "a disaccharide" : "an oligosaccharide of " + std::to_string(c / 6) + " hexoses";
            r.explanation = "Hexose units joined by glycosidic bonds, losing one water per link.";
            return finish(r);
        }
        CompoundName r;
        r.kind = CompoundKind::Organic;
        r.name = "organic compound " + f.hill();
        r.explanation = "Degree of unsaturation " + format_number(f.degree_of_unsaturation(), 3) + " (rings plus pi bonds). Build it in the covalent sandbox to name a specific structure.";
        r.ok = true;
        return finish(r);
    }

    if (!metal && charge == 0 && comp.size() == 2) {
        std::vector<std::pair<const Element*, int>> parts;
        for (const auto& [z, n] : comp) parts.emplace_back(&PeriodicTable::element(z), static_cast<int>(std::lround(n)));
        std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return naming_rank(*a.first) < naming_rank(*b.first); });
        CompoundName r;
        r.kind = CompoundKind::CovalentBinary;
        const std::string first = prefixed(parts[0].second, element_lower_name(*parts[0].first), true);
        const std::string second = prefixed(parts[1].second, monatomic_anion_name(*parts[1].first), false);
        r.name = first + " " + second;
        r.explanation = "Two nonmetals share electrons, so prefixes (mono-, di-, tri-, ...) give the number of each atom. Mono- is left off the first element.";
        return finish(r);
    }

    if (!metal && charge == 0) {
        CompoundName r;
        r.kind = CompoundKind::Covalent;
        std::vector<std::pair<const Element*, int>> parts;
        for (const auto& [z, n] : comp) parts.emplace_back(&PeriodicTable::element(z), static_cast<int>(std::lround(n)));
        std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return naming_rank(*a.first) < naming_rank(*b.first); });
        std::string name;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (!name.empty()) name += " ";
            name += i + 1 == parts.size() ? prefixed(parts[i].second, monatomic_anion_name(*parts[i].first), false) : prefixed(parts[i].second, element_lower_name(*parts[i].first), i == 0);
        }
        r.name = name;
        r.explanation = "A molecular compound named with additive prefixes; it may also have a traditional name.";
        return finish(r);
    }

    out.explanation = "couldn't find a set of ions that balance this formula";
    return out;
}

CompoundName formula_from_name(std::string_view name_text) {
    CompoundName out;
    std::string name = collapse(name_text);
    if (name.empty()) return out;
    if (ends_with(name, " ion")) name = name.substr(0, name.size() - 4);
    for (const CommonCompoundEntry& c : common_compounds()) {
        bool hit = name == lower(c.name);
        for (const std::string& alt : split_names(c.other_names)) hit = hit || name == lower(alt);
        if (hit) {
            out.ok = true;
            out.name = c.name;
            out.formula = c.formula;
            out.other_names = split_names(c.other_names);
            out.kind = std::string_view(c.category) == "carbohydrate" ? CompoundKind::Carbohydrate : CompoundKind::Covalent;
            return out;
        }
    }
    int waters = 0;
    {
        const std::size_t sp = name.rfind(' ');
        if (sp != std::string::npos && ends_with(name, "hydrate")) {
            const std::string word = name.substr(sp + 1);
            const std::string pre = word.substr(0, word.size() - 7);
            if (pre.empty()) waters = 1;
            else {
                for (int i = 1; i <= 12; ++i) {
                    if (pre == PREFIXES[i] || (pre + "a") == PREFIXES[i] || (pre + "o") == PREFIXES[i]) waters = i;
                }
            }
            if (waters > 0) name = name.substr(0, sp);
        }
    }
    const auto finish = [&](CompoundName r) {
        if (waters > 0) {
            r.formula += "\xC2\xB7" + (waters > 1 ? std::to_string(waters) : std::string()) + "H2O";
            r.name += " " + hydrate_suffix(waters);
            r.hydrate_waters = waters;
        }
        r.ok = true;
        return r;
    };
    if (const PolyatomicIonEntry* p = find_polyatomic_ion(name)) {
        CompoundName r;
        r.kind = CompoundKind::PolyatomicIon;
        r.name = std::string(p->name) + " ion";
        r.formula = ion_formula_text(*p, false);
        r.ions.push_back(polyatomic_ion(*p));
        return finish(r);
    }
    if (ends_with(name, " acid")) {
        for (const PolyatomicIonEntry& p : polyatomic_ions()) {
            if (p.charge < 0 && name == lower(p.acid) && *p.acid_formula) {
                CompoundName r;
                r.kind = CompoundKind::Acid;
                r.name = p.acid;
                r.formula = p.acid_formula;
                return finish(r);
            }
        }
        std::string root = name.substr(0, name.size() - 5);
        if (starts_with(root, "hydro") && ends_with(root, "ic")) {
            const std::string mid = root.substr(5, root.size() - 7);
            const IonDescriptor* found = nullptr;
            static std::vector<IonDescriptor> anions;
            if (anions.empty()) anions = common_anions();
            for (const IonDescriptor& a : anions) {
                const std::string an = a.name;
                if (an == mid + "ide" || (mid == "sulfur" && an == "sulfide") || (mid == "cyan" && an == "cyanide")) {
                    found = &a;
                    break;
                }
            }
            if (found) {
                CompoundName r;
                r.kind = CompoundKind::Acid;
                r.name = name;
                r.formula = "H" + count_text(-found->charge) + found->formula;
                r.ions = { monatomic_ion(PeriodicTable::element(1), 1), *found };
                return finish(r);
            }
        }
        if (auto org = organic_from_name(name)) return finish(*org);
        std::string anion_name;
        if (ends_with(root, "ic")) anion_name = root.substr(0, root.size() - 2) + "ate";
        else if (ends_with(root, "ous")) anion_name = root.substr(0, root.size() - 3) + "ite";
        if (!anion_name.empty()) {
            if (auto a = anion_from_words(anion_name)) {
                CompoundName r;
                r.kind = CompoundKind::Acid;
                r.name = name;
                r.formula = "H" + count_text(-a->charge) + a->formula;
                r.ions = { monatomic_ion(PeriodicTable::element(1), 1), *a };
                return finish(r);
            }
        }
        out.explanation = "unknown acid name";
        return out;
    }
    for (std::size_t sp = name.find(' '); sp != std::string::npos; sp = name.find(' ', sp + 1)) {
        const std::string left = name.substr(0, sp);
        const std::string right = name.substr(sp + 1);
        auto c = cation_from_words(left);
        auto a = anion_from_words(right);
        if (c && a) {
            const IonicCombination combo = combine_ions(*c, *a);
            CompoundName r;
            r.kind = CompoundKind::Ionic;
            if (c->element && c->element->atomic_number == 1) r.kind = CompoundKind::Acid;
            r.name = combo.name;
            if (c->element && c->element->atomic_number == 1) r.name = name;
            r.formula = combo.formula;
            r.ions = { *c, *a };
            if (a->entry && std::string_view(a->entry->name) == "hydroxide") r.kind = CompoundKind::Base;
            return finish(r);
        }
    }
    if (const Element* e = element_from_name(name)) {
        CompoundName r;
        r.kind = CompoundKind::Element;
        r.name = element_lower_name(*e);
        r.formula = e->is_diatomic() ? std::string(e->symbol) + "2" : std::string(e->symbol);
        if (e->atomic_number == 15) r.other_names.push_back("P4 (white phosphorus)");
        if (e->atomic_number == 16) r.other_names.push_back("S8 (rhombic sulfur)");
        return finish(r);
    }
    {
        std::vector<std::string> words;
        std::string cur;
        for (char ch2 : name) {
            if (ch2 == ' ') {
                if (!cur.empty()) words.push_back(cur);
                cur.clear();
            } else {
                cur += ch2;
            }
        }
        if (!cur.empty()) words.push_back(cur);
        if (words.size() >= 2) {
            std::string formula;
            bool ok = true;
            for (std::size_t i = 0; i < words.size() && ok; ++i) {
                std::size_t used = 0;
                int n = read_prefix(words[i], used);
                std::string rest = words[i].substr(used);
                const Element* e = nullptr;
                if (i + 1 == words.size()) {
                    e = element_from_root(rest);
                    if (!e && used > 0) {
                        e = element_from_root(words[i].substr(used - 1));
                        if (e) rest = words[i].substr(used - 1);
                    }
                    if (!e && used > 0) {
                        e = element_from_root("o" + rest);
                    }
                    if (!e) {
                        std::size_t u2 = 0;
                        const std::string w = words[i];
                        if (starts_with(w, "monox") || starts_with(w, "pentox") || starts_with(w, "tetrox") || starts_with(w, "hexox") || starts_with(w, "heptox") || starts_with(w, "decox")) {
                            n = read_prefix(w, u2);
                            e = &PeriodicTable::element(8);
                        }
                    }
                } else {
                    e = element_from_name(rest);
                    if (!e && used == 0) e = element_from_name(words[i]);
                }
                if (!e) {
                    ok = false;
                    break;
                }
                formula += std::string(e->symbol) + count_text(n);
            }
            if (ok) {
                CompoundName r;
                r.kind = CompoundKind::CovalentBinary;
                r.name = name;
                r.formula = formula;
                return finish(r);
            }
        }
    }
    if (auto org = organic_from_name(name)) return finish(*org);
    out.explanation = "couldn't work out a formula for that name";
    return out;
}

} // namespace chemistry
} // namespace fizmo
