#include "organic_naming.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace fizmo {
namespace chemistry {

namespace {

const char* const STEM_NAMES[] = { "", "meth", "eth", "prop", "but", "pent", "hex", "hept", "oct", "non", "dec", "undec", "dodec", "tridec", "tetradec", "pentadec", "hexadec", "heptadec", "octadec", "nonadec", "icos", "henicos", "docos", "tricos", "tetracos", "pentacos", "hexacos", "heptacos", "octacos", "nonacos", "triacont" };

std::string stem(int n) {
    if (n >= 1 && n <= 30) return STEM_NAMES[n];
    return "C" + std::to_string(n) + "-alk";
}

const char* const SIMPLE_MULTIPLIERS[] = { "", "", "di", "tri", "tetra", "penta", "hexa", "hepta", "octa", "nona", "deca" };
const char* const COMPLEX_MULTIPLIERS[] = { "", "", "bis", "tris", "tetrakis", "pentakis", "hexakis", "heptakis", "octakis", "nonakis", "decakis" };

std::string multiplier(int n, bool complex) {
    if (n <= 1) return "";
    if (n <= 10) return complex ? COMPLEX_MULTIPLIERS[n] : SIMPLE_MULTIPLIERS[n];
    return std::to_string(n) + "-";
}

enum Group : int {
    NoGroup = 0,
    AmineGroup = 1,
    AlcoholGroup = 2,
    KetoneGroup = 3,
    AldehydeGroup = 4,
    AcidGroup = 5
};

const char* group_word(int g) {
    switch (g) {
        case AcidGroup: return "carboxylic acid";
        case AldehydeGroup: return "aldehyde";
        case KetoneGroup: return "ketone";
        case AlcoholGroup: return "alcohol";
        case AmineGroup: return "amine";
        default: return "";
    }
}

std::string join_locants(const std::vector<int>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) s += ",";
        s += std::to_string(v[i]);
    }
    return s;
}

bool starts_with_vowel(const std::string& s) {
    if (s.empty()) return false;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

struct Prefix {
    std::string name;
    std::string alpha;
    bool complex = false;
    int locant = 0;
};

struct Failure {
    std::string reason;
};

class Namer {
public:
    explicit Namer(const Molecule& m) : m_mol(m) {}

    OrganicName run();

private:
    const Molecule& m_mol;
    std::vector<int> m_heavy;
    std::vector<std::vector<std::pair<int, int>>> m_adj;
    std::vector<int> m_h;
    std::vector<int> m_group;
    std::vector<bool> m_ring;
    std::string m_fail;

    std::string sym(int a) const { return m_mol.atoms()[static_cast<std::size_t>(a)].element->symbol; }
    bool is_c(int a) const { return sym(a) == "C"; }
    int heavy_degree(int a) const { return static_cast<int>(m_adj[static_cast<std::size_t>(a)].size()); }
    int order(int a, int b) const {
        for (const auto& e : m_adj[static_cast<std::size_t>(a)]) if (e.first == b) return e.second;
        return 0;
    }
    std::vector<int> carbon_neighbors(int a) const {
        std::vector<int> out;
        for (const auto& e : m_adj[static_cast<std::size_t>(a)]) if (is_c(e.first)) out.push_back(e.first);
        return out;
    }

    bool fail(const std::string& why) {
        if (m_fail.empty()) m_fail = why;
        return false;
    }

    bool prepare();
    void classify();
    void find_rings();

    bool oxygen_is_hydroxyl(int o, int c) const { return heavy_degree(o) == 1 && order(o, c) == 1; }
    bool oxygen_is_carbonyl(int o, int c) const { return heavy_degree(o) == 1 && order(o, c) == 2; }
    bool nitrogen_is_amine(int n, int c) const { return heavy_degree(n) == 1 && order(n, c) == 1; }
    bool is_nitro(int n) const {
        if (sym(n) != "N" || heavy_degree(n) != 3) return false;
        int oxygens = 0;
        for (const auto& e : m_adj[static_cast<std::size_t>(n)]) oxygens += sym(e.first) == "O" && heavy_degree(e.first) == 1;
        return oxygens == 2;
    }

    bool group_atom(int c, int other) const;
    bool attachments(int c, const std::set<int>& skeleton, int principal, int from, int& principal_here, std::vector<Prefix>& prefixes, std::vector<int>& external_rings);
    bool substituent(int start, int from, Prefix& out);
    bool alkyl_substituent(int start, int from, Prefix& out);
    bool ring_substituent(int start, int from, Prefix& out);
    std::vector<int> ring_cycle(int start) const;

    struct Numbering {
        std::vector<int> chain;
        int principal_count = 0;
        int length = 0;
        int multiple_count = 0;
        int prefix_count = 0;
        std::vector<int> principal_locants;
        std::vector<int> multiple_locants;
        std::vector<int> double_locants;
        std::vector<int> triple_locants;
        std::vector<int> prefix_locants;
        std::vector<int> alpha_locants;
        std::vector<Prefix> prefixes;
    };

    bool number_chain(const std::vector<int>& chain, int principal, int from, bool ring, Numbering& out);
    static bool better(const Numbering& a, const Numbering& b, bool compare_size);
    std::string prefix_text(const std::vector<Prefix>& prefixes, bool omit_locants) const;
    std::string chain_name(const Numbering& n, int principal, bool substituent_form) const;
    std::string ring_name(const Numbering& n, int principal, bool aromatic, bool substituent_form) const;
    bool aromatic_ring(const std::vector<int>& cycle) const;

    bool name_chain_parent(const std::set<int>& component, int principal, OrganicName& out);
    bool name_ring_parent(const std::vector<int>& cycle, int principal, OrganicName& out);
};

bool Namer::prepare() {
    const auto& atoms = m_mol.atoms();
    if (atoms.empty()) return fail("there are no atoms");
    m_adj.assign(atoms.size(), {});
    m_h.assign(atoms.size(), 0);
    bool carbon = false;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        const std::string s = atoms[i].element->symbol;
        if (s == "H") continue;
        if (atoms[i].formal_charge != 0) {
            bool nitro_charge = false;
            for (const MoleculeBond& b : m_mol.bonds()) {
                if (b.a != i && b.b != i) continue;
                const MoleculeAtom& o = atoms[b.other(i)];
                nitro_charge = nitro_charge || (s == "N" && atoms[i].formal_charge == 1 && std::string(o.element->symbol) == "O" && o.formal_charge == -1)
                    || (s == "O" && atoms[i].formal_charge == -1 && std::string(o.element->symbol) == "N" && o.formal_charge == 1);
            }
            if (!nitro_charge) return fail("charged atoms aren't named this way");
        }
        if (s != "C" && s != "O" && s != "N" && s != "F" && s != "Cl" && s != "Br" && s != "I") return fail("only C, H, O, N and the halogens are named this way");
        carbon = carbon || s == "C";
        m_heavy.push_back(static_cast<int>(i));
    }
    if (!carbon) return fail("there's no carbon");
    for (const MoleculeBond& b : m_mol.bonds()) {
        const bool ha = std::string(atoms[b.a].element->symbol) == "H";
        const bool hb = std::string(atoms[b.b].element->symbol) == "H";
        if (ha && hb) continue;
        if (ha) { ++m_h[b.b]; continue; }
        if (hb) { ++m_h[b.a]; continue; }
        const int o = static_cast<int>(b.order);
        m_adj[b.a].push_back({ static_cast<int>(b.b), o });
        m_adj[b.b].push_back({ static_cast<int>(b.a), o });
    }
    for (int a : m_heavy) {
        const int missing = m_mol.missing_bonds(static_cast<std::size_t>(a));
        if (missing > 0) m_h[static_cast<std::size_t>(a)] += missing;
    }
    std::set<int> seen;
    std::vector<int> stack{ m_heavy.front() };
    while (!stack.empty()) {
        const int a = stack.back();
        stack.pop_back();
        if (!seen.insert(a).second) continue;
        for (const auto& e : m_adj[static_cast<std::size_t>(a)]) stack.push_back(e.first);
    }
    if (seen.size() != m_heavy.size()) return fail("the structure is in more than one piece");
    for (int a : m_heavy) {
        if (sym(a) != "O") continue;
        if (heavy_degree(a) == 2) {
            const int x = m_adj[static_cast<std::size_t>(a)][0].first, y = m_adj[static_cast<std::size_t>(a)][1].first;
            if (!is_c(x) || !is_c(y)) return fail("an O joined to something other than two carbons");
            for (int c : { x, y }) {
                for (const auto& e : m_adj[static_cast<std::size_t>(c)]) {
                    if (sym(e.first) == "O" && e.second == 2) return fail("esters and anhydrides aren't named here yet");
                }
            }
        } else if (heavy_degree(a) > 2) {
            return fail("an O with more than two bonds");
        }
    }
    for (int a : m_heavy) {
        if (sym(a) != "N") continue;
        if (is_nitro(a)) continue;
        if (heavy_degree(a) != 1) return fail("only NH2 (amine) and NO2 (nitro) nitrogen is named here");
        const int c = m_adj[static_cast<std::size_t>(a)][0].first;
        if (!is_c(c) || order(a, c) != 1) return fail("nitriles and imines aren't named here yet");
        for (const auto& e : m_adj[static_cast<std::size_t>(c)]) {
            if (sym(e.first) == "O" && e.second == 2) return fail("amides aren't named here yet");
        }
    }
    for (int a : m_heavy) {
        for (const auto& e : m_adj[static_cast<std::size_t>(a)]) {
            if (e.second == static_cast<int>(BondOrder::Aromatic) && !(is_c(a) && is_c(e.first))) return fail("aromatic rings with other atoms aren't named here yet");
        }
    }
    return true;
}

void Namer::classify() {
    m_group.assign(m_mol.atoms().size(), NoGroup);
    for (int c : m_heavy) {
        if (!is_c(c)) continue;
        bool carbonyl = false, hydroxyl = false, amine = false;
        for (const auto& e : m_adj[static_cast<std::size_t>(c)]) {
            if (sym(e.first) == "O" && oxygen_is_carbonyl(e.first, c)) carbonyl = true;
            if (sym(e.first) == "O" && oxygen_is_hydroxyl(e.first, c)) hydroxyl = true;
            if (sym(e.first) == "N" && nitrogen_is_amine(e.first, c)) amine = true;
        }
        int g = NoGroup;
        if (carbonyl && hydroxyl) g = AcidGroup;
        else if (carbonyl && carbon_neighbors(c).size() <= 1 && !m_ring[static_cast<std::size_t>(c)]) g = AldehydeGroup;
        else if (carbonyl) g = KetoneGroup;
        else if (hydroxyl) g = AlcoholGroup;
        else if (amine) g = AmineGroup;
        m_group[static_cast<std::size_t>(c)] = g;
    }
}

void Namer::find_rings() {
    m_ring.assign(m_mol.atoms().size(), false);
    for (const MoleculeBond& b : m_mol.bonds()) {
        const int a = static_cast<int>(b.a), c = static_cast<int>(b.b);
        if (!is_c(a) || !is_c(c)) continue;
        std::set<int> seen{ a };
        std::vector<int> stack{ a };
        bool found = false;
        while (!stack.empty() && !found) {
            const int x = stack.back();
            stack.pop_back();
            for (const auto& e : m_adj[static_cast<std::size_t>(x)]) {
                if (!is_c(e.first)) continue;
                if (x == a && e.first == c) continue;
                if (e.first == c) { found = true; break; }
                if (seen.insert(e.first).second) stack.push_back(e.first);
            }
        }
        if (found) {
            m_ring[static_cast<std::size_t>(a)] = true;
            m_ring[static_cast<std::size_t>(c)] = true;
        }
    }
}

std::vector<int> Namer::ring_cycle(int start) const {
    std::vector<int> cycle{ start };
    std::set<int> used{ start };
    int prev = -1, cur = start;
    while (true) {
        int next = -1;
        for (const auto& e : m_adj[static_cast<std::size_t>(cur)]) {
            if (!m_ring[static_cast<std::size_t>(e.first)] || e.first == prev) continue;
            if (e.first == start && cycle.size() > 2) return cycle;
            if (!used.count(e.first)) { next = e.first; break; }
        }
        if (next < 0) return {};
        cycle.push_back(next);
        used.insert(next);
        prev = cur;
        cur = next;
    }
}

bool Namer::aromatic_ring(const std::vector<int>& cycle) const {
    if (cycle.size() != 6) return false;
    int aromatic = 0, doubles = 0;
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        const int o = order(cycle[i], cycle[(i + 1) % cycle.size()]);
        aromatic += o == static_cast<int>(BondOrder::Aromatic);
        doubles += o == 2;
    }
    return aromatic == 6 || doubles == 3;
}

bool Namer::group_atom(int c, int other) const {
    const std::string s = sym(other);
    if (s == "O") return oxygen_is_carbonyl(other, c) || oxygen_is_hydroxyl(other, c);
    if (s == "N") return nitrogen_is_amine(other, c);
    return false;
}

bool Namer::attachments(int c, const std::set<int>& skeleton, int principal, int from, int& principal_here, std::vector<Prefix>& prefixes, std::vector<int>& external_rings) {
    principal_here = 0;
    const int g = m_group[static_cast<std::size_t>(c)];
    const bool is_principal = principal != NoGroup && g == principal;
    for (const auto& e : m_adj[static_cast<std::size_t>(c)]) {
        const int x = e.first;
        if (x == from || skeleton.count(x)) continue;
        const std::string s = sym(x);
        if (s == "O" && oxygen_is_carbonyl(x, c)) {
            if (is_principal) continue;
            if (g == AcidGroup) {
                Prefix p; p.name = "carboxy"; p.alpha = "carboxy"; prefixes.push_back(p);
            } else if (g == AldehydeGroup) {
                Prefix p; p.name = "oxo"; p.alpha = "oxo"; prefixes.push_back(p);
            } else {
                Prefix p; p.name = "oxo"; p.alpha = "oxo"; prefixes.push_back(p);
            }
            continue;
        }
        if (s == "O" && oxygen_is_hydroxyl(x, c)) {
            if (is_principal || g == AcidGroup) continue;
            Prefix p; p.name = "hydroxy"; p.alpha = "hydroxy"; prefixes.push_back(p);
            continue;
        }
        if (s == "N" && nitrogen_is_amine(x, c)) {
            if (is_principal) continue;
            Prefix p; p.name = "amino"; p.alpha = "amino"; prefixes.push_back(p);
            continue;
        }
        if (is_c(x) && m_ring[static_cast<std::size_t>(x)] && !m_ring[static_cast<std::size_t>(c)]) {
            external_rings.push_back(x);
        }
        if (is_c(x) && principal != NoGroup && (principal == AcidGroup || principal == AldehydeGroup) && m_group[static_cast<std::size_t>(x)] == principal && carbon_neighbors(x).size() == 1 && m_ring[static_cast<std::size_t>(c)]) {
            ++principal_here;
            continue;
        }
        Prefix p;
        if (!substituent(x, c, p)) return false;
        prefixes.push_back(p);
    }
    if (is_principal) {
        if (g == AcidGroup || g == AldehydeGroup) principal_here += 1;
        else {
            for (const auto& e : m_adj[static_cast<std::size_t>(c)]) {
                if (e.first == from || skeleton.count(e.first)) continue;
                if (group_atom(c, e.first)) ++principal_here;
            }
        }
    }
    return true;
}

bool Namer::substituent(int start, int from, Prefix& out) {
    const std::string s = sym(start);
    if (s == "F") { out.name = out.alpha = "fluoro"; return true; }
    if (s == "Cl") { out.name = out.alpha = "chloro"; return true; }
    if (s == "Br") { out.name = out.alpha = "bromo"; return true; }
    if (s == "I") { out.name = out.alpha = "iodo"; return true; }
    if (s == "N") {
        if (is_nitro(start)) { out.name = out.alpha = "nitro"; return true; }
        if (heavy_degree(start) == 1) { out.name = out.alpha = "amino"; return true; }
        return fail("a nitrogen group that isn't named here yet");
    }
    if (s == "O") {
        if (heavy_degree(start) == 1) { out.name = out.alpha = order(start, from) == 2 ? "oxo" : "hydroxy"; return true; }
        int next = -1;
        for (const auto& e : m_adj[static_cast<std::size_t>(start)]) if (e.first != from) next = e.first;
        if (next < 0) return fail("a broken O group");
        Prefix alkyl;
        if (!substituent(next, start, alkyl)) return false;
        static const std::map<std::string, std::string> simple = {
            { "methyl", "methoxy" }, { "ethyl", "ethoxy" }, { "propyl", "propoxy" }, { "butyl", "butoxy" },
            { "isopropyl", "isopropoxy" }, { "tert-butyl", "tert-butoxy" }, { "phenyl", "phenoxy" }
        };
        auto it = simple.find(alkyl.name);
        if (it != simple.end()) {
            out.name = it->second;
            out.alpha = it->second == "tert-butoxy" ? "butoxy" : it->second;
            return true;
        }
        std::string inner = alkyl.name;
        if (alkyl.complex && inner.size() > 2 && inner.front() == '(' && inner.back() == ')') inner = inner.substr(1, inner.size() - 2);
        out.name = "(" + inner + "oxy)";
        out.alpha = inner + "oxy";
        out.complex = true;
        return true;
    }
    if (!is_c(start)) return fail("an atom that isn't named here yet");
    if (m_ring[static_cast<std::size_t>(start)]) return ring_substituent(start, from, out);
    return alkyl_substituent(start, from, out);
}

bool Namer::alkyl_substituent(int start, int from, Prefix& out) {
    std::set<int> branch;
    std::vector<int> stack{ start };
    while (!stack.empty()) {
        const int x = stack.back();
        stack.pop_back();
        if (!branch.insert(x).second) continue;
        for (const auto& e : m_adj[static_cast<std::size_t>(x)]) {
            if (e.first == from || !is_c(e.first) || m_ring[static_cast<std::size_t>(e.first)]) continue;
            stack.push_back(e.first);
        }
    }
    std::vector<std::vector<int>> chains;
    std::function<void(int, int, std::vector<int>&)> walk = [&](int x, int prev, std::vector<int>& path) {
        path.push_back(x);
        bool extended = false;
        for (const auto& e : m_adj[static_cast<std::size_t>(x)]) {
            if (e.first == prev || !branch.count(e.first)) continue;
            extended = true;
            walk(e.first, x, path);
        }
        if (!extended) chains.push_back(path);
        else chains.push_back(path);
        path.pop_back();
    };
    std::vector<int> path;
    walk(start, from, path);
    bool have = false;
    Numbering best;
    for (const std::vector<int>& chain : chains) {
        Numbering n;
        if (!number_chain(chain, NoGroup, from, false, n)) return false;
        if (!have || better(n, best, true)) {
            best = n;
            have = true;
        }
    }
    if (!have) return fail("an empty branch");
    const std::string name = chain_name(best, NoGroup, true);
    static const std::map<std::string, std::pair<std::string, std::string>> common = {
        { "(1-methylethyl)", { "isopropyl", "isopropyl" } },
        { "(1,1-dimethylethyl)", { "tert-butyl", "butyl" } },
        { "(1-methylpropyl)", { "sec-butyl", "butyl" } },
        { "(2-methylpropyl)", { "isobutyl", "isobutyl" } },
        { "(phenylmethyl)", { "benzyl", "benzyl" } },
        { "ethenyl", { "ethenyl", "ethenyl" } }
    };
    auto it = common.find(name);
    if (it != common.end()) {
        out.name = it->second.first;
        out.alpha = it->second.second;
        out.complex = false;
        return true;
    }
    out.name = name;
    out.complex = !name.empty() && name.front() == '(';
    out.alpha = out.complex ? name.substr(1, name.size() - 2) : name;
    return true;
}

bool Namer::ring_substituent(int start, int from, Prefix& out) {
    const std::vector<int> cycle = ring_cycle(start);
    if (cycle.empty()) return fail("a ring system that isn't named here yet");
    for (int a : cycle) {
        if (!is_c(a)) return fail("rings with other atoms aren't named here yet");
    }
    const bool aromatic = aromatic_ring(cycle);
    bool have = false;
    Numbering best;
    for (int dir = 0; dir < 2; ++dir) {
        std::vector<int> order_list;
        for (std::size_t k = 0; k < cycle.size(); ++k) order_list.push_back(cycle[dir == 0 ? k : (cycle.size() - k) % cycle.size()]);
        Numbering n;
        if (!number_chain(order_list, NoGroup, from, true, n)) return false;
        if (!have || better(n, best, false)) {
            best = n;
            have = true;
        }
    }
    out.name = ring_name(best, NoGroup, aromatic, true);
    out.complex = !out.name.empty() && out.name.front() == '(';
    out.alpha = out.complex ? out.name.substr(1, out.name.size() - 2) : out.name;
    return true;
}

bool Namer::number_chain(const std::vector<int>& chain, int principal, int from, bool ring, Numbering& out) {
    out = Numbering{};
    out.chain = chain;
    out.length = static_cast<int>(chain.size());
    const std::set<int> skeleton(chain.begin(), chain.end());
    for (std::size_t i = 0; i < chain.size(); ++i) {
        const int locant = static_cast<int>(i) + 1;
        int here = 0;
        std::vector<Prefix> prefixes;
        std::vector<int> rings;
        const int skip = i == 0 ? from : -1;
        if (!attachments(chain[i], skeleton, principal, skip, here, prefixes, rings)) return false;
        for (int k = 0; k < here; ++k) out.principal_locants.push_back(locant);
        out.principal_count += here;
        for (Prefix& p : prefixes) {
            p.locant = locant;
            out.prefixes.push_back(p);
            out.prefix_locants.push_back(locant);
        }
        const std::size_t last = ring ? chain.size() : chain.size() - 1;
        if (i < last) {
            const int o = order(chain[i], chain[(i + 1) % chain.size()]);
            if (o == static_cast<int>(BondOrder::Aromatic)) {
                if (!ring) return fail("an aromatic bond outside a ring");
            } else if (o == 2) {
                out.double_locants.push_back(locant);
                out.multiple_locants.push_back(locant);
            } else if (o == 3) {
                out.triple_locants.push_back(locant);
                out.multiple_locants.push_back(locant);
            }
        }
    }
    out.multiple_count = static_cast<int>(out.multiple_locants.size());
    out.prefix_count = static_cast<int>(out.prefixes.size());
    std::vector<Prefix> sorted = out.prefixes;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Prefix& a, const Prefix& b) { return a.alpha < b.alpha; });
    for (const Prefix& p : sorted) out.alpha_locants.push_back(p.locant);
    for (auto* v : { &out.principal_locants, &out.multiple_locants, &out.double_locants, &out.prefix_locants }) std::sort(v->begin(), v->end());
    return true;
}

bool Namer::better(const Numbering& a, const Numbering& b, bool compare_size) {
    if (a.principal_count != b.principal_count) return a.principal_count > b.principal_count;
    if (compare_size) {
        if (a.length != b.length) return a.length > b.length;
        if (a.multiple_count != b.multiple_count) return a.multiple_count > b.multiple_count;
        if (a.prefix_count != b.prefix_count) return a.prefix_count > b.prefix_count;
    }
    if (a.principal_locants != b.principal_locants) return a.principal_locants < b.principal_locants;
    if (a.multiple_locants != b.multiple_locants) return a.multiple_locants < b.multiple_locants;
    if (a.double_locants != b.double_locants) return a.double_locants < b.double_locants;
    if (a.prefix_locants != b.prefix_locants) return a.prefix_locants < b.prefix_locants;
    return a.alpha_locants < b.alpha_locants;
}

std::string Namer::prefix_text(const std::vector<Prefix>& prefixes, bool omit_locants) const {
    std::map<std::string, std::vector<int>> by_name;
    std::map<std::string, std::string> alpha;
    std::map<std::string, bool> complex;
    for (const Prefix& p : prefixes) {
        by_name[p.name].push_back(p.locant);
        alpha[p.name] = p.alpha;
        complex[p.name] = p.complex;
    }
    std::vector<std::string> names;
    for (const auto& kv : by_name) names.push_back(kv.first);
    std::sort(names.begin(), names.end(), [&](const std::string& a, const std::string& b) { return alpha[a] < alpha[b]; });
    std::string out;
    for (const std::string& n : names) {
        std::vector<int> locs = by_name[n];
        std::sort(locs.begin(), locs.end());
        const std::string mult = multiplier(static_cast<int>(locs.size()), complex[n]);
        std::string piece = mult + n;
        if (!omit_locants) {
            piece = join_locants(locs) + "-" + piece;
            if (!out.empty()) out += "-";
        }
        out += piece;
    }
    return out;
}

std::string Namer::chain_name(const Numbering& n, int principal, bool substituent_form) const {
    const int length = n.length;
    std::string body = stem(length);
    const int doubles = static_cast<int>(n.double_locants.size());
    const int triples = static_cast<int>(n.triple_locants.size());
    const int principal_count = static_cast<int>(n.principal_locants.size());
    std::string suffix;
    bool suffix_locants = true;
    switch (principal) {
        case AcidGroup: suffix = multiplier(principal_count, false) + "oic acid"; suffix_locants = false; break;
        case AldehydeGroup: suffix = multiplier(principal_count, false) + "al"; suffix_locants = false; break;
        case KetoneGroup: suffix = multiplier(principal_count, false) + "one"; break;
        case AlcoholGroup: suffix = multiplier(principal_count, false) + "ol"; break;
        case AmineGroup: suffix = multiplier(principal_count, false) + "amine"; break;
        default: break;
    }
    if (substituent_form) suffix = "yl";
    if (length <= 1 || (length == 2 && principal_count == 1)) suffix_locants = false;
    const bool omit_multiple = length <= 2 || (length == 3 && doubles + triples == 1 && principal == NoGroup && n.prefixes.empty() && !substituent_form);
    const bool multi_unsat = doubles > 1 || triples > 1;
    if (doubles + triples == 0) {
        body += "an";
    } else {
        if (multi_unsat) body += "a";
        if (doubles > 0) {
            body += omit_multiple ? std::string() : "-" + join_locants(n.double_locants) + "-";
            body += multiplier(doubles, false) + "en";
        }
        if (triples > 0) {
            body += omit_multiple ? std::string() : "-" + join_locants(n.triple_locants) + "-";
            body += multiplier(triples, false) + "yn";
        }
    }
    if (substituent_form) {
        if (doubles + triples == 0) body = stem(length);
        body += "yl";
    } else if (!suffix.empty()) {
        if (!starts_with_vowel(suffix)) body += "e";
        if (suffix_locants) body += "-" + join_locants(n.principal_locants) + "-";
        body += suffix;
    } else {
        body += "e";
    }
    const bool omit_prefix_locants = length == 1 || (length == 2 && n.prefixes.size() == 1 && principal == NoGroup && doubles + triples == 0 && !substituent_form);
    std::string prefix = prefix_text(n.prefixes, omit_prefix_locants);
    std::string out = prefix + body;
    if (substituent_form && (!prefix.empty() || (doubles + triples > 0 && length > 2))) out = "(" + out + ")";
    return out;
}

std::string Namer::ring_name(const Numbering& n, int principal, bool aromatic, bool substituent_form) const {
    const int size = n.length;
    const int principal_count = static_cast<int>(n.principal_locants.size());
    std::string parent;
    bool retained = false;
    if (aromatic) {
        if (substituent_form) parent = "phenyl";
        else if (principal == AcidGroup && principal_count == 1) { parent = "benzoic acid"; retained = true; }
        else if (principal == AldehydeGroup && principal_count == 1) { parent = "benzaldehyde"; retained = true; }
        else if (principal == AlcoholGroup && principal_count == 1) { parent = "phenol"; retained = true; }
        else if (principal == AmineGroup && principal_count == 1) { parent = "aniline"; retained = true; }
        else parent = "benzene";
    } else {
        parent = "cyclo" + stem(size);
        const int doubles = static_cast<int>(n.double_locants.size());
        const int triples = static_cast<int>(n.triple_locants.size());
        if (doubles + triples == 0) parent += "an";
        else {
            if (doubles > 1 || triples > 1) parent += "a";
            const bool omit = doubles + triples == 1;
            if (doubles > 0) parent += (omit ? std::string() : "-" + join_locants(n.double_locants) + "-") + multiplier(doubles, false) + "en";
            if (triples > 0) parent += (omit ? std::string() : "-" + join_locants(n.triple_locants) + "-") + multiplier(triples, false) + "yn";
        }
        if (substituent_form) parent += "yl";
    }
    if (!aromatic && !substituent_form) {
        std::string suffix;
        switch (principal) {
            case AcidGroup: suffix = multiplier(principal_count, false) + "carboxylic acid"; break;
            case AldehydeGroup: suffix = multiplier(principal_count, false) + "carbaldehyde"; break;
            case KetoneGroup: suffix = multiplier(principal_count, false) + "one"; break;
            case AlcoholGroup: suffix = multiplier(principal_count, false) + "ol"; break;
            case AmineGroup: suffix = multiplier(principal_count, false) + "amine"; break;
            default: break;
        }
        if (suffix.empty()) parent += "e";
        else {
            if (!starts_with_vowel(suffix)) parent += "e";
            if (principal_count > 1) parent += "-" + join_locants(n.principal_locants) + "-";
            parent += suffix;
        }
    } else if (aromatic && !substituent_form && !retained && principal != NoGroup) {
        std::string suffix;
        switch (principal) {
            case AcidGroup: suffix = multiplier(principal_count, false) + "carboxylic acid"; break;
            case AldehydeGroup: suffix = multiplier(principal_count, false) + "carbaldehyde"; break;
            case AlcoholGroup: suffix = multiplier(principal_count, false) + "ol"; break;
            case AmineGroup: suffix = multiplier(principal_count, false) + "amine"; break;
            default: break;
        }
        parent += "-" + join_locants(n.principal_locants) + "-" + suffix;
    }
    const bool omit_prefix_locants = n.prefixes.size() == 1 && principal_count == 0 && !substituent_form && (aromatic || n.double_locants.empty());
    const std::string prefix = prefix_text(n.prefixes, omit_prefix_locants);
    std::string out = prefix + parent;
    if (substituent_form && !prefix.empty()) out = "(" + out + ")";
    return out;
}

bool Namer::name_chain_parent(const std::set<int>& component, int principal, OrganicName& out) {
    std::vector<int> carbons;
    for (int a : component) if (is_c(a) && !m_ring[static_cast<std::size_t>(a)]) carbons.push_back(a);
    if (carbons.empty()) return fail("no open-chain carbons");
    std::set<int> allowed(carbons.begin(), carbons.end());
    bool have = false;
    Numbering best;
    for (std::size_t i = 0; i < carbons.size(); ++i) {
        std::map<int, int> parent{ { carbons[i], -1 } };
        std::vector<int> queue{ carbons[i] };
        for (std::size_t q = 0; q < queue.size(); ++q) {
            const int x = queue[q];
            for (const auto& e : m_adj[static_cast<std::size_t>(x)]) {
                if (!allowed.count(e.first) || parent.count(e.first)) continue;
                parent[e.first] = x;
                queue.push_back(e.first);
            }
        }
        for (std::size_t j = 0; j < carbons.size(); ++j) {
            if (!parent.count(carbons[j])) continue;
            std::vector<int> chain;
            for (int x = carbons[j]; x != -1; x = parent[x]) chain.push_back(x);
            Numbering n;
            if (!number_chain(chain, principal, -1, false, n)) return false;
            if (!have || better(n, best, true)) {
                best = n;
                have = true;
            }
        }
    }
    if (!have) return fail("no chain found");
    out.name = chain_name(best, principal, false);
    out.parent = stem(best.length) + "ane chain of " + std::to_string(best.length) + " carbon" + (best.length == 1 ? "" : "s");
    return true;
}

bool Namer::name_ring_parent(const std::vector<int>& cycle, int principal, OrganicName& out) {
    const bool aromatic = aromatic_ring(cycle);
    bool have = false;
    Numbering best;
    for (std::size_t start = 0; start < cycle.size(); ++start) {
        for (int dir = 0; dir < 2; ++dir) {
            std::vector<int> ordered;
            for (std::size_t k = 0; k < cycle.size(); ++k) {
                const std::size_t idx = dir == 0 ? (start + k) % cycle.size() : (start + cycle.size() - k) % cycle.size();
                ordered.push_back(cycle[idx]);
            }
            Numbering n;
            if (!number_chain(ordered, principal, -1, true, n)) return false;
            if (aromatic) {
                n.multiple_locants.clear();
                n.double_locants.clear();
                n.triple_locants.clear();
                n.multiple_count = 0;
            }
            if (!have || better(n, best, false)) {
                best = n;
                have = true;
            }
        }
    }
    if (!have) return fail("no ring numbering");
    out.name = ring_name(best, principal, aromatic, false);
    out.parent = aromatic ? "benzene ring" : "ring of " + std::to_string(cycle.size()) + " carbons";
    return true;
}

OrganicName Namer::run() {
    OrganicName out;
    if (!prepare()) {
        out.reason = m_fail;
        return out;
    }
    find_rings();
    classify();
    for (int a : m_heavy) {
        if (!m_ring[static_cast<std::size_t>(a)]) continue;
        int ring_neighbors = 0;
        for (const auto& e : m_adj[static_cast<std::size_t>(a)]) ring_neighbors += m_ring[static_cast<std::size_t>(e.first)] ? 1 : 0;
        if (ring_neighbors != 2) {
            out.reason = "fused, bridged or several rings aren't named here yet";
            return out;
        }
    }
    int principal = NoGroup;
    for (int a : m_heavy) if (is_c(a)) principal = std::max(principal, m_group[static_cast<std::size_t>(a)]);
    std::vector<std::vector<int>> cycles;
    std::set<int> in_cycle;
    for (int a : m_heavy) {
        if (!m_ring[static_cast<std::size_t>(a)] || in_cycle.count(a)) continue;
        std::vector<int> c = ring_cycle(a);
        if (c.empty()) {
            out.reason = "a ring system that isn't named here yet";
            return out;
        }
        for (int x : c) in_cycle.insert(x);
        cycles.push_back(c);
    }
    for (const auto& c : cycles) {
        for (int x : c) {
            if (!is_c(x)) {
                out.reason = "rings with other atoms aren't named here yet";
                return out;
            }
        }
    }
    int ring_choice = -1;
    if (!cycles.empty()) {
        int best_on_ring = -1;
        bool principal_off_ring = false;
        for (int a : m_heavy) {
            if (!is_c(a) || m_ring[static_cast<std::size_t>(a)] || m_group[static_cast<std::size_t>(a)] != principal || principal == NoGroup) continue;
            bool attached_to_ring = false;
            if (principal == AcidGroup || principal == AldehydeGroup) {
                const std::vector<int> cn = carbon_neighbors(a);
                attached_to_ring = cn.size() == 1 && m_ring[static_cast<std::size_t>(cn[0])];
            }
            if (!attached_to_ring) principal_off_ring = true;
        }
        for (std::size_t i = 0; i < cycles.size(); ++i) {
            int count = 0;
            for (int x : cycles[i]) {
                if (principal != NoGroup && m_group[static_cast<std::size_t>(x)] == principal) ++count;
                for (const auto& e : m_adj[static_cast<std::size_t>(x)]) {
                    if (is_c(e.first) && !m_ring[static_cast<std::size_t>(e.first)] && (principal == AcidGroup || principal == AldehydeGroup) && m_group[static_cast<std::size_t>(e.first)] == principal && carbon_neighbors(e.first).size() == 1) ++count;
                }
            }
            const int score = count * 1000 + static_cast<int>(cycles[i].size());
            if (score > best_on_ring) {
                best_on_ring = score;
                ring_choice = static_cast<int>(i);
            }
        }
        bool any_chain = false;
        for (int a : m_heavy) any_chain = any_chain || (is_c(a) && !m_ring[static_cast<std::size_t>(a)]);
        const bool ring_has_principal = best_on_ring >= 1000;
        if (principal != NoGroup && principal_off_ring && !ring_has_principal) ring_choice = -1;
        if (principal == NoGroup && cycles.size() > 1) ring_choice = -1;
        if (ring_choice < 0 && !any_chain) {
            out.reason = "several separate rings aren't named here yet";
            return out;
        }
        if (cycles.size() > 1 && ring_choice >= 0) {
            out.reason = "several separate rings aren't named here yet";
            return out;
        }
    }
    bool ok;
    std::set<int> all(m_heavy.begin(), m_heavy.end());
    if (ring_choice >= 0) ok = name_ring_parent(cycles[static_cast<std::size_t>(ring_choice)], principal, out);
    else ok = name_chain_parent(all, principal, out);
    if (!ok) {
        out.name.clear();
        out.reason = m_fail.empty() ? "this structure isn't named here yet" : m_fail;
        return out;
    }
    out.ok = true;
    out.principal_group = group_word(principal);
    return out;
}

}

OrganicName name_organic_molecule(const Molecule& molecule) {
    Namer n(molecule);
    try {
        return n.run();
    } catch (const std::exception& e) {
        OrganicName out;
        out.reason = e.what();
        return out;
    }
}

std::string iupac_name(const Molecule& molecule) {
    const OrganicName n = name_organic_molecule(molecule);
    return n.ok ? n.name : std::string();
}

namespace {

struct CondensedToken {
    enum Kind { Atom, Open, Close, Bond };
    explicit CondensedToken(Kind k = Atom) : kind(k) {}
    Kind kind = Atom;
    std::string symbol;
    int count = 1;
    int order = 1;
};

class CondensedParser {
public:
    explicit CondensedParser(std::string_view text) : m_text(text) {}

    bool parse(Molecule& out, std::string& reason) {
        if (!tokenize()) {
            reason = m_error;
            return false;
        }
        std::size_t i = 0;
        int first = -1;
        if (!chain(i, -1, 1, first) || i != m_tokens.size()) {
            reason = m_error.empty() ? "unexpected ')'" : m_error;
            return false;
        }
        if (!infer_bonds()) {
            reason = m_error;
            return false;
        }
        m_mol.add_implicit_hydrogens();
        out = m_mol;
        return true;
    }

private:
    std::string_view m_text;
    std::vector<CondensedToken> m_tokens;
    Molecule m_mol;
    std::vector<int> m_h;
    std::vector<std::pair<int, int>> m_backbone;
    std::string m_error;

    bool error(const std::string& e) {
        if (m_error.empty()) m_error = e;
        return false;
    }

    bool tokenize() {
        const std::string_view t = m_text;
        for (std::size_t i = 0; i < t.size();) {
            const unsigned char c = static_cast<unsigned char>(t[i]);
            if (c == ' ') { ++i; continue; }
            if (c == '(' || c == '[') { m_tokens.push_back(CondensedToken{ CondensedToken::Open }); ++i; continue; }
            if (c == ')' || c == ']') {
                CondensedToken k{ CondensedToken::Close };
                ++i;
                std::size_t j = i;
                while (j < t.size() && std::isdigit(static_cast<unsigned char>(t[j]))) ++j;
                if (j > i) k.count = std::stoi(std::string(t.substr(i, j - i)));
                i = j;
                m_tokens.push_back(k);
                continue;
            }
            if (c == '=') { CondensedToken k{ CondensedToken::Bond }; k.order = 2; m_tokens.push_back(k); ++i; continue; }
            if (c == '#') { CondensedToken k{ CondensedToken::Bond }; k.order = 3; m_tokens.push_back(k); ++i; continue; }
            if (c == '-') { CondensedToken k{ CondensedToken::Bond }; k.order = 1; m_tokens.push_back(k); ++i; continue; }
            if (c == 0xE2 && i + 2 < t.size() && static_cast<unsigned char>(t[i + 1]) == 0x89 && static_cast<unsigned char>(t[i + 2]) == 0xA1) {
                CondensedToken k{ CondensedToken::Bond };
                k.order = 3;
                m_tokens.push_back(k);
                i += 3;
                continue;
            }
            if (c >= 'A' && c <= 'Z') {
                std::string symbol(1, static_cast<char>(c));
                ++i;
                if (i < t.size() && (t[i] == 'l' || t[i] == 'r')) {
                    symbol += t[i];
                    ++i;
                }
                if (symbol != "C" && symbol != "H" && symbol != "O" && symbol != "N" && symbol != "F" && symbol != "Cl" && symbol != "Br" && symbol != "I") return error("'" + symbol + "' isn't used in condensed organic formulas here");
                CondensedToken k{ CondensedToken::Atom };
                k.symbol = symbol;
                std::size_t j = i;
                while (j < t.size() && std::isdigit(static_cast<unsigned char>(t[j]))) ++j;
                if (j > i) k.count = std::stoi(std::string(t.substr(i, j - i)));
                i = j;
                m_tokens.push_back(k);
                continue;
            }
            return error("unexpected character in a condensed formula");
        }
        return !m_tokens.empty() || error("empty formula");
    }

    int add(const std::string& symbol, int h = -1) {
        const int a = static_cast<int>(m_mol.add_atom(symbol));
        m_h.push_back(h);
        return a;
    }

    void bond(int a, int b, int order, bool backbone) {
        m_mol.add_bond(static_cast<std::size_t>(a), static_cast<std::size_t>(b), static_cast<BondOrder>(order));
        if (backbone) m_backbone.push_back({ a, b });
    }

    int bonds_of(int a) const {
        int sum = 0;
        for (const MoleculeBond& b : m_mol.bonds()) {
            if (static_cast<int>(b.a) == a || static_cast<int>(b.b) == a) sum += static_cast<int>(b.order);
        }
        return sum;
    }

    bool is_atom(std::size_t i, const char* symbol) const {
        return i < m_tokens.size() && m_tokens[i].kind == CondensedToken::Atom && m_tokens[i].symbol == symbol;
    }

    bool continues(std::size_t i) const {
        if (i >= m_tokens.size()) return false;
        const CondensedToken& k = m_tokens[i];
        if (k.kind == CondensedToken::Close) return false;
        if (k.kind == CondensedToken::Atom && (k.symbol == "H" || k.symbol == "F" || k.symbol == "Cl" || k.symbol == "Br" || k.symbol == "I")) return false;
        return true;
    }

    bool chain(std::size_t& i, int anchor, int order_in, int& first) {
        int cur = -1;
        int pending_order = order_in;
        int pending_h = -1;
        std::vector<int> pending;
        first = -1;
        const auto attach = [&](int a) {
            if (cur >= 0) bond(cur, a, pending_order, true);
            else if (anchor >= 0) bond(anchor, a, pending_order, true);
            if (first < 0) first = a;
            for (int p : pending) bond(a, p, 1, false);
            pending.clear();
            pending_order = 1;
            if (pending_h >= 0) {
                m_h[static_cast<std::size_t>(a)] = pending_h;
                pending_h = -1;
            }
            cur = a;
        };
        while (i < m_tokens.size()) {
            const CondensedToken k = m_tokens[i];
            if (k.kind == CondensedToken::Close) break;
            if (k.kind == CondensedToken::Bond) {
                pending_order = k.order;
                ++i;
                continue;
            }
            if (k.kind == CondensedToken::Open) {
                std::size_t close = i + 1;
                int depth = 1;
                while (close < m_tokens.size()) {
                    if (m_tokens[close].kind == CondensedToken::Open) ++depth;
                    if (m_tokens[close].kind == CondensedToken::Close && --depth == 0) break;
                    ++close;
                }
                if (close >= m_tokens.size()) return error("a '(' isn't closed");
                const int n = m_tokens[close].count;
                const std::size_t start = i + 1;
                const bool repeat_unit = cur >= 0 && close == start + 2 && is_atom(start, "C") && m_tokens[start].count == 1
                    && m_tokens[start + 1].kind == CondensedToken::Atom && m_tokens[start + 1].count == 2
                    && (m_tokens[start + 1].symbol == "H" || m_tokens[start + 1].symbol == "F" || m_tokens[start + 1].symbol == "Cl" || m_tokens[start + 1].symbol == "Br");
                if (repeat_unit) {
                    for (int r = 0; r < n; ++r) {
                        const int a = add("C");
                        attach(a);
                        if (m_tokens[start + 1].symbol == "H") m_h[static_cast<std::size_t>(a)] = 2;
                        else {
                            m_h[static_cast<std::size_t>(a)] = 0;
                            for (int x = 0; x < 2; ++x) bond(a, add(m_tokens[start + 1].symbol), 1, false);
                        }
                    }
                } else {
                    for (int r = 0; r < n; ++r) {
                        std::size_t p = start;
                        int f = -1;
                        if (!chain(p, cur, 1, f)) return false;
                        if (p != close) return error("couldn't read the group in parentheses");
                        if (cur < 0) pending.push_back(f);
                        else if (f >= 0 && m_mol.find_bond(static_cast<std::size_t>(cur), static_cast<std::size_t>(f)) == nullptr) bond(cur, f, 1, false);
                    }
                }
                i = close + 1;
                continue;
            }
            const std::string sym = k.symbol;
            if (sym == "H") {
                if (cur >= 0 && m_h[static_cast<std::size_t>(cur)] < 0) m_h[static_cast<std::size_t>(cur)] = k.count;
                else pending_h = k.count;
                ++i;
                continue;
            }
            if (sym == "F" || sym == "Cl" || sym == "Br" || sym == "I") {
                for (int r = 0; r < k.count; ++r) {
                    const int a = add(sym);
                    if (cur >= 0) bond(cur, a, 1, false);
                    else pending.push_back(a);
                }
                ++i;
                continue;
            }
            if (sym == "C") {
                if (k.count == 6 && is_atom(i + 1, "H") && m_tokens[i + 1].count == 5) {
                    std::vector<int> ring;
                    for (int r = 0; r < 6; ++r) ring.push_back(add("C", r == 0 ? 0 : 1));
                    for (int r = 0; r < 6; ++r) bond(ring[static_cast<std::size_t>(r)], ring[static_cast<std::size_t>((r + 1) % 6)], r % 2 == 0 ? 2 : 1, false);
                    attach(ring[0]);
                    i += 2;
                    continue;
                }
                if (k.count != 1) return error("write a chain carbon by carbon (like CH3CH2CH3), not as C" + std::to_string(k.count));
                attach(add("C"));
                ++i;
                continue;
            }
            if (sym == "O") {
                if (k.count == 2 && is_atom(i + 1, "H") && cur >= 0) {
                    bond(cur, add("O"), 2, false);
                    bond(cur, add("O", 1), 1, false);
                    i += 2;
                    continue;
                }
                if (is_atom(i + 1, "O") && is_atom(i + 2, "H") && cur >= 0) {
                    bond(cur, add("O"), 2, false);
                    bond(cur, add("O", 1), 1, false);
                    i += 3;
                    continue;
                }
                if (is_atom(i + 1, "O") && cur >= 0) {
                    bond(cur, add("O"), 2, false);
                    ++i;
                    attach(add("O", 0));
                    ++i;
                    continue;
                }
                if (is_atom(i + 1, "H")) {
                    const int a = add("O", m_tokens[i + 1].count);
                    if (cur >= 0) bond(cur, a, 1, false);
                    else attach(a);
                    i += 2;
                    continue;
                }
                if (pending_h >= 0 || cur < 0) {
                    attach(add("O"));
                    ++i;
                    continue;
                }
                const int hcur = m_h[static_cast<std::size_t>(cur)];
                const int free = 4 - (hcur >= 0 ? hcur : 0) - bonds_of(cur) - (continues(i + 1) ? 1 : 0);
                if (m_mol.atoms()[static_cast<std::size_t>(cur)].element->symbol == std::string("C") && (free >= 2 || !continues(i + 1))) {
                    bond(cur, add("O"), 2, false);
                    ++i;
                    continue;
                }
                attach(add("O", 0));
                ++i;
                continue;
            }
            if (sym == "N") {
                if (is_atom(i + 1, "O") && m_tokens[i + 1].count == 2) {
                    const int n = add("N", 0);
                    const int o1 = add("O", 0), o2 = add("O", 0);
                    bond(n, o1, 2, false);
                    bond(n, o2, 1, false);
                    m_mol.set_formal_charge(static_cast<std::size_t>(n), 1);
                    m_mol.set_formal_charge(static_cast<std::size_t>(o2), -1);
                    if (cur >= 0) bond(cur, n, 1, false);
                    else pending.push_back(n);
                    i += 2;
                    continue;
                }
                if (is_atom(i + 1, "H")) {
                    const int hn = m_tokens[i + 1].count;
                    const int a = add("N", hn);
                    if (hn >= 2 && cur >= 0) bond(cur, a, 1, false);
                    else attach(a);
                    i += 2;
                    continue;
                }
                if (cur >= 0 && !continues(i + 1)) {
                    bond(cur, add("N", 0), 3, false);
                    ++i;
                    continue;
                }
                attach(add("N", 0));
                ++i;
                continue;
            }
            return error("couldn't read '" + sym + "'");
        }
        if (!pending.empty()) return error("a group at the start isn't attached to anything");
        if (first < 0 && anchor < 0) return error("there's no atom to start from");
        return true;
    }

    bool infer_bonds() {
        for (int pass = 0; pass < 2; ++pass) {
            for (const auto& b : m_backbone) {
                const MoleculeAtom& x = m_mol.atoms()[static_cast<std::size_t>(b.first)];
                const MoleculeAtom& y = m_mol.atoms()[static_cast<std::size_t>(b.second)];
                if (std::string(x.element->symbol) != "C" || std::string(y.element->symbol) != "C") continue;
                const int hx = m_h[static_cast<std::size_t>(b.first)], hy = m_h[static_cast<std::size_t>(b.second)];
                if (hx < 0 || hy < 0) continue;
                const MoleculeBond* bd = m_mol.find_bond(static_cast<std::size_t>(b.first), static_cast<std::size_t>(b.second));
                if (!bd) continue;
                int o = static_cast<int>(bd->order);
                while (o < 3 && 4 - hx - bonds_of(b.first) > 0 && 4 - hy - bonds_of(b.second) > 0) {
                    ++o;
                    m_mol.set_bond_order(static_cast<std::size_t>(b.first), static_cast<std::size_t>(b.second), static_cast<BondOrder>(o));
                }
            }
        }
        for (std::size_t a = 0; a < m_mol.atoms().size(); ++a) {
            if (std::string(m_mol.atoms()[a].element->symbol) != "C" || m_h[a] < 0) continue;
            if (4 - m_h[a] - bonds_of(static_cast<int>(a)) != 0) return error("the hydrogen counts don't add up to four bonds on every carbon");
        }
        return true;
    }
};

bool looks_condensed(std::string_view text) {
    int carbons = 0;
    bool structure = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == 'C' && (i + 1 >= text.size() || text[i + 1] != 'l')) ++carbons;
        if (text[i] == '(' || text[i] == '=' || text[i] == '#') structure = true;
    }
    return carbons >= 2 || (carbons >= 1 && structure);
}

}

bool molecule_from_condensed(std::string_view text, Molecule& out, std::string* reason) {
    try {
        CondensedParser p(text);
        std::string why;
        if (p.parse(out, why)) return true;
        if (reason) *reason = why;
    } catch (const std::exception& e) {
        if (reason) *reason = e.what();
    }
    return false;
}

std::string iupac_name_from_condensed(std::string_view text) {
    if (!looks_condensed(text)) return std::string();
    Molecule m;
    if (!molecule_from_condensed(text, m)) return std::string();
    return iupac_name(m);
}

} // namespace chemistry
} // namespace fizmo
