#include "molecule.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

int maximum_bonds(const Element& e) {
    if (e.atomic_number <= 2) return e.atomic_number == 1 ? 1 : 0;
    if (e.period == 2) return 4;
    if (e.is_main_group()) return 6;
    return 8;
}

int octet_target(const Element& e) {
    if (e.atomic_number <= 2) return 2;
    if (e.atomic_number <= 4) return 2 * (e.atomic_number - 2);
    if (e.is_main_group() || e.is_noble_gas()) return 8;
    return 18;
}

bool can_expand_octet(const Element& e) {
    return e.period >= 3 && (e.is_main_group() || e.is_noble_gas());
}

double electronegativity_of(const Element& e) {
    return value_or(e.electronegativity_pauling, e.is_noble_gas() ? 2.6 : 1.5);
}

int bond_int(BondOrder o) {
    return o == BondOrder::Aromatic ? 1 : static_cast<int>(o);
}

std::uint64_t mix_seed(std::uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

double unit_random(std::uint64_t seed) {
    return static_cast<double>(mix_seed(seed) >> 11) / 9007199254740992.0;
}

struct SmilesAtom {
    const Element* element = nullptr;
    int charge = 0;
    int isotope = 0;
    int hydrogens = -1;
    bool aromatic = false;
};

} // namespace

void Molecule::require_atom(std::size_t atom) const {
    if (atom >= m_atoms.size()) throw std::out_of_range("fizmo::chemistry: no atom with that index in the molecule");
}

std::size_t Molecule::add_atom(const Element& element, int formal_charge, int mass_number) {
    if (mass_number != 0 && !element.isotope(mass_number)) throw std::invalid_argument("fizmo::chemistry: that isotope is not known");
    m_atoms.push_back(MoleculeAtom{ &element, formal_charge, mass_number, 0.0, 0.0, 0.0 });
    return m_atoms.size() - 1;
}

std::size_t Molecule::add_atom(std::string_view symbol, int formal_charge, int mass_number) {
    return add_atom(PeriodicTable::element(symbol), formal_charge, mass_number);
}

std::size_t Molecule::add_bond(std::size_t a, std::size_t b, BondOrder order) {
    require_atom(a);
    require_atom(b);
    if (a == b) throw std::invalid_argument("fizmo::chemistry: an atom can't bond to itself");
    if (find_bond(a, b)) throw std::invalid_argument("fizmo::chemistry: those atoms are already bonded");
    m_bonds.push_back(MoleculeBond{ a, b, order });
    return m_bonds.size() - 1;
}

bool Molecule::remove_bond(std::size_t a, std::size_t b) {
    auto it = std::find_if(m_bonds.begin(), m_bonds.end(), [&](const MoleculeBond& x) { return (x.a == a && x.b == b) || (x.a == b && x.b == a); });
    if (it == m_bonds.end()) return false;
    m_bonds.erase(it);
    return true;
}

void Molecule::set_bond_order(std::size_t a, std::size_t b, BondOrder order) {
    for (MoleculeBond& x : m_bonds) {
        if ((x.a == a && x.b == b) || (x.a == b && x.b == a)) {
            x.order = order;
            return;
        }
    }
    throw std::invalid_argument("fizmo::chemistry: those atoms are not bonded");
}

void Molecule::set_formal_charge(std::size_t atom, int charge) {
    require_atom(atom);
    m_atoms[atom].formal_charge = charge;
}

void Molecule::set_position(std::size_t atom, double x, double y, double z) {
    require_atom(atom);
    m_atoms[atom].x = x;
    m_atoms[atom].y = y;
    m_atoms[atom].z = z;
}

const MoleculeBond* Molecule::find_bond(std::size_t a, std::size_t b) const noexcept {
    for (const MoleculeBond& x : m_bonds) {
        if ((x.a == a && x.b == b) || (x.a == b && x.b == a)) return &x;
    }
    return nullptr;
}

std::vector<std::size_t> Molecule::neighbors(std::size_t atom) const {
    require_atom(atom);
    std::vector<std::size_t> out;
    for (const MoleculeBond& x : m_bonds) {
        if (x.a == atom || x.b == atom) out.push_back(x.other(atom));
    }
    return out;
}

int Molecule::degree(std::size_t atom) const {
    return static_cast<int>(neighbors(atom).size());
}

double Molecule::bond_order_sum(std::size_t atom) const {
    require_atom(atom);
    double s = 0.0;
    for (const MoleculeBond& x : m_bonds) {
        if (x.a == atom || x.b == atom) s += numeric_bond_order(x.order);
    }
    return s;
}

int Molecule::lone_pairs(std::size_t atom) const {
    require_atom(atom);
    const MoleculeAtom& a = m_atoms[atom];
    const int remaining = a.element->valence_electrons() - a.formal_charge - static_cast<int>(std::lround(bond_order_sum(atom)));
    return remaining > 0 ? remaining / 2 : 0;
}

int Molecule::electrons_around(std::size_t atom) const {
    return 2 * lone_pairs(atom) + 2 * static_cast<int>(std::lround(bond_order_sum(atom)));
}

int Molecule::standard_valence(std::size_t atom) const {
    require_atom(atom);
    const MoleculeAtom& a = m_atoms[atom];
    const Element& e = *a.element;
    int v = 0;
    if (e.atomic_number == 1) v = 1;
    else if (e.group == 13) v = 3;
    else if (e.group == 14) v = 4;
    else if (e.group == 15) v = 3;
    else if (e.group == 16) v = 2;
    else if (e.group == 17) v = 1;
    else if (e.group == 18) v = 0;
    else v = e.most_common_oxidation_state() > 0 ? e.most_common_oxidation_state() : 0;
    if (e.group == 15 || e.group == 16 || e.group == 17) v += a.formal_charge;
    else if (e.group == 13) v -= a.formal_charge;
    else if (e.group == 14) v -= std::abs(a.formal_charge);
    return std::max(0, v);
}

int Molecule::missing_bonds(std::size_t atom) const {
    const int m = standard_valence(atom) - static_cast<int>(std::lround(bond_order_sum(atom)));
    return m > 0 ? m : 0;
}

bool Molecule::obeys_octet(std::size_t atom) const {
    const int target = m_atoms.at(atom).element->atomic_number <= 2 ? 2 : 8;
    return electrons_around(atom) == target;
}

bool Molecule::is_overbonded(std::size_t atom) const {
    return bond_order_sum(atom) > maximum_bonds(*m_atoms.at(atom).element) + 1e-9;
}

std::size_t Molecule::add_implicit_hydrogens() {
    const std::size_t heavy = m_atoms.size();
    std::size_t added = 0;
    const Element& h = PeriodicTable::element(1);
    for (std::size_t i = 0; i < heavy; ++i) {
        if (m_atoms[i].element->atomic_number == 1) continue;
        const int missing = missing_bonds(i);
        for (int k = 0; k < missing; ++k) {
            const std::size_t j = add_atom(h);
            add_bond(i, j);
            ++added;
        }
    }
    return added;
}

VseprResult Molecule::geometry_at(std::size_t atom) const {
    return vsepr(degree(atom), lone_pairs(atom));
}

Hybridization Molecule::hybridization_at(std::size_t atom) const {
    return geometry_at(atom).hybridization;
}

int Molecule::charge() const noexcept {
    int q = 0;
    for (const MoleculeAtom& a : m_atoms) q += a.formal_charge;
    return q;
}

Formula Molecule::formula() const {
    Formula f;
    for (const MoleculeAtom& a : m_atoms) f += Formula(*a.element, 1.0, a.mass_number);
    f.set_charge(charge());
    return f;
}

double Molecule::molar_mass() const {
    return formula().molar_mass();
}

std::vector<std::vector<std::size_t>> Molecule::fragments() const {
    std::vector<int> seen(m_atoms.size(), 0);
    std::vector<std::vector<std::size_t>> out;
    for (std::size_t start = 0; start < m_atoms.size(); ++start) {
        if (seen[start]) continue;
        std::vector<std::size_t> part;
        std::vector<std::size_t> stack{ start };
        seen[start] = 1;
        while (!stack.empty()) {
            const std::size_t a = stack.back();
            stack.pop_back();
            part.push_back(a);
            for (std::size_t n : neighbors(a)) {
                if (!seen[n]) {
                    seen[n] = 1;
                    stack.push_back(n);
                }
            }
        }
        std::sort(part.begin(), part.end());
        out.push_back(part);
    }
    return out;
}

bool Molecule::is_connected() const {
    return fragments().size() <= 1;
}

int Molecule::ring_count() const {
    return static_cast<int>(m_bonds.size()) - static_cast<int>(m_atoms.size()) + static_cast<int>(fragments().size());
}

bool Molecule::valid(std::string* reason) const {
    for (std::size_t i = 0; i < m_atoms.size(); ++i) {
        if (is_overbonded(i)) {
            if (reason) *reason = std::string(m_atoms[i].element->symbol) + " has too many bonds";
            return false;
        }
        const int remaining = m_atoms[i].element->valence_electrons() - m_atoms[i].formal_charge - static_cast<int>(std::lround(bond_order_sum(i)));
        if (remaining < 0) {
            if (reason) *reason = std::string(m_atoms[i].element->symbol) + " doesn't have enough electrons for its bonds";
            return false;
        }
    }
    if (reason) reason->clear();
    return true;
}

double Molecule::bond_energy(const MoleculeBond& bond) const {
    return chemistry::bond_energy(*m_atoms.at(bond.a).element, *m_atoms.at(bond.b).element, bond.order);
}

double Molecule::total_bond_energy() const {
    double total = 0.0;
    for (const MoleculeBond& b : m_bonds) total += bond_energy(b);
    return total;
}

double Molecule::estimated_enthalpy_of_formation() const {
    double atoms = 0.0;
    for (const MoleculeAtom& a : m_atoms) atoms += a.element->atomization_enthalpy;
    return atoms - total_bond_energy();
}

double Molecule::bond_length(const MoleculeBond& bond, BondLengthMethod method) const {
    return chemistry::bond_length(*m_atoms.at(bond.a).element, *m_atoms.at(bond.b).element, bond.order, method);
}

double Molecule::distance(std::size_t a, std::size_t b) const {
    require_atom(a);
    require_atom(b);
    const double dx = m_atoms[a].x - m_atoms[b].x;
    const double dy = m_atoms[a].y - m_atoms[b].y;
    const double dz = m_atoms[a].z - m_atoms[b].z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double Molecule::angle(std::size_t a, std::size_t center, std::size_t b) const {
    require_atom(a);
    require_atom(center);
    require_atom(b);
    const MoleculeAtom& p = m_atoms[a];
    const MoleculeAtom& c = m_atoms[center];
    const MoleculeAtom& q = m_atoms[b];
    const double ux = p.x - c.x, uy = p.y - c.y, uz = p.z - c.z;
    const double vx = q.x - c.x, vy = q.y - c.y, vz = q.z - c.z;
    const double dot = ux * vx + uy * vy + uz * vz;
    const double nu = std::sqrt(ux * ux + uy * uy + uz * uz);
    const double nv = std::sqrt(vx * vx + vy * vy + vz * vz);
    return std::acos(std::max(-1.0, std::min(1.0, dot / (nu * nv)))) * 180.0 / PI;
}

double Molecule::dipole_moment() const {
    double mx = 0.0, my = 0.0, mz = 0.0;
    for (const MoleculeBond& b : m_bonds) {
        const MoleculeAtom& p = m_atoms[b.a];
        const MoleculeAtom& q = m_atoms[b.b];
        const double xp = value_or(p.element->electronegativity_pauling, 0.0);
        const double xq = value_or(q.element->electronegativity_pauling, 0.0);
        const double d = distance(b.a, b.b);
        if (d <= 0.0) continue;
        const double ionic = percent_ionic_character(std::fabs(xp - xq)) / 100.0;
        const double sign = xq > xp ? 1.0 : -1.0;
        const double scale = sign * bond_dipole(ionic, d) / d;
        mx += scale * (q.x - p.x);
        my += scale * (q.y - p.y);
        mz += scale * (q.z - p.z);
    }
    return std::sqrt(mx * mx + my * my + mz * mz);
}

std::string Molecule::to_string() const {
    return formula().hill();
}

Molecule Molecule::from_smiles(std::string_view smiles) {
    Molecule m;
    std::vector<SmilesAtom> info;
    std::vector<std::size_t> stack;
    std::map<int, std::pair<std::size_t, int>> rings;
    std::size_t prev = static_cast<std::size_t>(-1);
    int pending = 0;
    const std::size_t none = static_cast<std::size_t>(-1);
    const auto fail = [&](const std::string& why) -> Molecule {
        throw std::invalid_argument("fizmo::chemistry: bad SMILES '" + std::string(smiles) + "': " + why);
    };
    const auto connect = [&](std::size_t a, std::size_t b, int order) {
        BondOrder o = BondOrder::Single;
        if (order == 2) o = BondOrder::Double;
        else if (order == 3) o = BondOrder::Triple;
        else if (order == 4) o = BondOrder::Aromatic;
        else if (order == 0 && info[a].aromatic && info[b].aromatic) o = BondOrder::Aromatic;
        m.add_bond(a, b, o);
    };
    const auto add = [&](SmilesAtom a) {
        const std::size_t idx = m.add_atom(*a.element, a.charge, 0);
        info.push_back(a);
        if (prev != none) connect(prev, idx, pending);
        pending = 0;
        prev = idx;
    };
    std::size_t i = 0;
    const std::size_t n = smiles.size();
    while (i < n) {
        const char c = smiles[i];
        if (c == ' ' || c == '\t') {
            ++i;
            continue;
        }
        if (c == '(') {
            if (prev == none) fail("a branch needs an atom before it");
            stack.push_back(prev);
            ++i;
            continue;
        }
        if (c == ')') {
            if (stack.empty()) fail("unmatched ')'");
            prev = stack.back();
            stack.pop_back();
            ++i;
            continue;
        }
        if (c == '-' || c == '/' || c == '\\') {
            pending = 1;
            ++i;
            continue;
        }
        if (c == '=') {
            pending = 2;
            ++i;
            continue;
        }
        if (c == '#' || c == '$') {
            pending = 3;
            ++i;
            continue;
        }
        if (c == ':') {
            pending = 4;
            ++i;
            continue;
        }
        if (c == '.') {
            prev = none;
            pending = 0;
            ++i;
            continue;
        }
        if (is_ascii_digit(c) || c == '%') {
            int label = 0;
            if (c == '%') {
                if (i + 2 >= n || !is_ascii_digit(smiles[i + 1]) || !is_ascii_digit(smiles[i + 2])) fail("'%' needs two digits");
                label = (smiles[i + 1] - '0') * 10 + (smiles[i + 2] - '0');
                i += 3;
            } else {
                label = c - '0';
                ++i;
            }
            if (prev == none) fail("a ring number needs an atom before it");
            auto it = rings.find(label);
            if (it == rings.end()) {
                rings[label] = { prev, pending };
            } else {
                const int order = pending != 0 ? pending : it->second.second;
                connect(it->second.first, prev, order);
                rings.erase(it);
            }
            pending = 0;
            continue;
        }
        SmilesAtom a;
        if (c == '[') {
            const std::size_t close = smiles.find(']', i);
            if (close == std::string_view::npos) fail("unclosed '['");
            std::string_view in = smiles.substr(i + 1, close - i - 1);
            std::size_t k = 0;
            while (k < in.size() && is_ascii_digit(in[k])) a.isotope = a.isotope * 10 + (in[k++] - '0');
            if (k >= in.size()) fail("empty bracket atom");
            if (is_ascii_lower(in[k])) {
                a.aromatic = true;
                std::string sym(1, ascii_upper(in[k]));
                if (k + 1 < in.size() && is_ascii_lower(in[k + 1]) && PeriodicTable::find_symbol(sym + in[k + 1])) {
                    sym += in[k + 1];
                    ++k;
                }
                ++k;
                a.element = PeriodicTable::find_symbol(sym);
            } else if (is_ascii_upper(in[k])) {
                std::string sym(1, in[k]);
                if (k + 1 < in.size() && is_ascii_lower(in[k + 1]) && PeriodicTable::find_symbol(sym + in[k + 1])) {
                    sym += in[k + 1];
                    ++k;
                }
                ++k;
                a.element = PeriodicTable::find_symbol(sym);
            }
            if (!a.element) fail("unknown element in '" + std::string(in) + "'");
            while (k < in.size() && in[k] == '@') ++k;
            a.hydrogens = 0;
            if (k < in.size() && in[k] == 'H') {
                ++k;
                int h = 0;
                bool digits = false;
                while (k < in.size() && is_ascii_digit(in[k])) {
                    h = h * 10 + (in[k++] - '0');
                    digits = true;
                }
                a.hydrogens = digits ? h : 1;
            }
            if (k < in.size() && (in[k] == '+' || in[k] == '-')) {
                const int sign = in[k] == '+' ? 1 : -1;
                ++k;
                int q = 1;
                if (k < in.size() && is_ascii_digit(in[k])) {
                    q = 0;
                    while (k < in.size() && is_ascii_digit(in[k])) q = q * 10 + (in[k++] - '0');
                } else {
                    while (k < in.size() && (in[k] == '+' || in[k] == '-')) {
                        ++q;
                        ++k;
                    }
                }
                a.charge = sign * q;
            }
            i = close + 1;
            add(a);
            continue;
        }
        if (c == '*') {
            fail("wildcard atoms aren't supported");
        }
        std::string sym;
        if (c == 'C' && i + 1 < n && smiles[i + 1] == 'l') {
            sym = "Cl";
            i += 2;
        } else if (c == 'B' && i + 1 < n && smiles[i + 1] == 'r') {
            sym = "Br";
            i += 2;
        } else if (c == 'B' || c == 'C' || c == 'N' || c == 'O' || c == 'P' || c == 'S' || c == 'F' || c == 'I') {
            sym = std::string(1, c);
            ++i;
        } else if (c == 'b' || c == 'c' || c == 'n' || c == 'o' || c == 'p' || c == 's') {
            sym = std::string(1, ascii_upper(c));
            a.aromatic = true;
            ++i;
        } else {
            fail(std::string("unexpected '") + c + "'");
        }
        a.element = PeriodicTable::find_symbol(sym);
        add(a);
    }
    if (!stack.empty()) fail("unclosed '('");
    if (!rings.empty()) fail("unclosed ring number");
    const std::size_t heavy = m.atom_count();
    for (std::size_t k = 0; k < heavy; ++k) {
        int h = info[k].hydrogens;
        if (h < 0) {
            const int z = info[k].element->atomic_number;
            std::vector<int> valences;
            switch (z) {
                case 5: valences = { 3 }; break;
                case 6: valences = { 4 }; break;
                case 7: valences = { 3, 5 }; break;
                case 8: valences = { 2 }; break;
                case 15: valences = { 3, 5 }; break;
                case 16: valences = { 2, 4, 6 }; break;
                default: valences = { 1 }; break;
            }
            int count = 0;
            int sum = 0;
            for (const MoleculeBond& b : m.m_bonds) {
                if (b.a != k && b.b != k) continue;
                ++count;
                sum += bond_int(b.order);
            }
            if (info[k].aromatic) {
                const bool lone_pair_donor = z == 8 || z == 16;
                h = valences.front() - sum - (lone_pair_donor ? 0 : 1);
            } else {
                h = 0;
                for (int v : valences) {
                    if (v >= sum) {
                        h = v - sum;
                        break;
                    }
                }
            }
            (void)count;
            if (h < 0) h = 0;
        }
        const Element& hydrogen = PeriodicTable::element(1);
        for (int j = 0; j < h; ++j) {
            const std::size_t idx = m.add_atom(hydrogen);
            m.add_bond(k, idx);
        }
    }
    return m;
}

std::size_t Molecule::remove_atom(std::size_t atom) {
    require_atom(atom);
    std::size_t removed = 0;
    std::vector<MoleculeBond> kept;
    for (const MoleculeBond& b : m_bonds) {
        if (b.a == atom || b.b == atom) {
            ++removed;
            continue;
        }
        MoleculeBond c = b;
        if (c.a > atom) --c.a;
        if (c.b > atom) --c.b;
        kept.push_back(c);
    }
    m_bonds = std::move(kept);
    m_atoms.erase(m_atoms.begin() + static_cast<std::ptrdiff_t>(atom));
    return removed;
}

std::vector<double> Molecule::partial_charges() const {
    std::vector<double> q(m_atoms.size(), 0.0);
    for (std::size_t i = 0; i < m_atoms.size(); ++i) q[i] = m_atoms[i].formal_charge;
    for (const MoleculeBond& b : m_bonds) {
        const double xa = electronegativity_of(*m_atoms[b.a].element);
        const double xb = electronegativity_of(*m_atoms[b.b].element);
        const double d = xb - xa;
        const double f = (1.0 - std::exp(-d * d / 4.0)) * 0.5 * std::sqrt(numeric_bond_order(b.order));
        if (d > 0) {
            q[b.a] += f;
            q[b.b] -= f;
        } else if (d < 0) {
            q[b.a] -= f;
            q[b.b] += f;
        }
    }
    return q;
}

LewisStructure Molecule::lewis_structure(int total_charge, bool keep_bond_orders) const {
    if (!keep_bond_orders) return best_lewis_structure(total_charge);
    LewisStructure out;
    const std::size_t n = m_atoms.size();
    out.charge = total_charge;
    out.atoms.assign(n, LewisAtom{});
    out.bond_orders.reserve(m_bonds.size());
    int valence = 0;
    for (const MoleculeAtom& a : m_atoms) valence += a.element->valence_electrons();
    valence -= total_charge;
    out.valence_electrons = valence;
    std::vector<int> bondsum(n, 0);
    int bonding = 0;
    for (const MoleculeBond& b : m_bonds) {
        const int o = bond_int(b.order);
        out.bond_orders.push_back(b.order);
        bondsum[b.a] += o;
        bondsum[b.b] += o;
        bonding += 2 * o;
    }
    double aromatic_extra = 0.0;
    for (const MoleculeBond& b : m_bonds) {
        if (b.order == BondOrder::Aromatic) aromatic_extra += 1.0;
    }
    out.bonding_electrons = bonding + static_cast<int>(std::lround(aromatic_extra));
    int remaining = valence - out.bonding_electrons;
    if (remaining < 0) {
        out.ok = false;
        out.notes.push_back("there are more bonding electrons (" + std::to_string(out.bonding_electrons) + ") than valence electrons (" + std::to_string(valence) + "); remove a bond or change the charge");
        out.score = 1e6;
        return out;
    }
    std::vector<double> aromatic_share(n, 0.0);
    for (const MoleculeBond& b : m_bonds) {
        if (b.order == BondOrder::Aromatic) {
            aromatic_share[b.a] += 0.5;
            aromatic_share[b.b] += 0.5;
        }
    }
    std::vector<std::size_t> order(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = i;
    std::vector<int> degree(n, 0);
    for (const MoleculeBond& b : m_bonds) {
        ++degree[b.a];
        ++degree[b.b];
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const bool ta = degree[a] <= 1;
        const bool tb = degree[b] <= 1;
        if (ta != tb) return ta;
        return electronegativity_of(*m_atoms[a].element) > electronegativity_of(*m_atoms[b].element);
    });
    std::vector<int> nonbonding(n, 0);
    for (std::size_t idx : order) {
        const Element& e = *m_atoms[idx].element;
        const int shared = 2 * bondsum[idx] + static_cast<int>(std::lround(2.0 * aromatic_share[idx]));
        int need = octet_target(e) - shared;
        if (need < 0) need = 0;
        int give = std::min(need, remaining);
        nonbonding[idx] += give;
        remaining -= give;
    }
    if (remaining > 0) {
        std::vector<std::size_t> centers;
        for (std::size_t i = 0; i < n; ++i) {
            if (can_expand_octet(*m_atoms[i].element)) centers.push_back(i);
        }
        std::sort(centers.begin(), centers.end(), [&](std::size_t a, std::size_t b) {
            if (degree[a] != degree[b]) return degree[a] > degree[b];
            return electronegativity_of(*m_atoms[a].element) < electronegativity_of(*m_atoms[b].element);
        });
        std::size_t k = 0;
        while (remaining > 0 && !centers.empty()) {
            const int give = std::min(2, remaining);
            nonbonding[centers[k % centers.size()]] += give;
            remaining -= give;
            ++k;
        }
        if (remaining > 0 && n > 0) {
            std::size_t best = order.back();
            for (std::size_t i = 0; i < n; ++i) {
                if (electronegativity_of(*m_atoms[i].element) > electronegativity_of(*m_atoms[best].element)) best = i;
            }
            nonbonding[best] += remaining;
            out.notes.push_back(std::string(m_atoms[best].element->symbol) + " has to hold extra electrons beyond its octet");
            remaining = 0;
        }
    }
    double score = 0.0;
    int lone_total = 0;
    int unpaired_total = 0;
    double max_en = 0.0;
    for (std::size_t i = 0; i < n; ++i) max_en = std::max(max_en, electronegativity_of(*m_atoms[i].element));
    for (std::size_t i = 0; i < n; ++i) {
        const Element& e = *m_atoms[i].element;
        LewisAtom& la = out.atoms[i];
        la.lone_pairs = nonbonding[i] / 2;
        la.unpaired = nonbonding[i] % 2;
        const double shared = 2.0 * bondsum[i] + 2.0 * aromatic_share[i];
        la.electrons = static_cast<int>(std::lround(shared)) + nonbonding[i];
        la.formal_charge = static_cast<int>(std::lround(e.valence_electrons() - nonbonding[i] - bondsum[i] - aromatic_share[i]));
        const int target = octet_target(e);
        la.octet = la.electrons == target;
        la.expanded_octet = la.electrons > target;
        la.incomplete_octet = la.electrons < target;
        lone_total += 2 * la.lone_pairs;
        unpaired_total += la.unpaired;
        score += 10.0 * std::abs(la.formal_charge);
        if (la.incomplete_octet) score += (e.atomic_number == 5 || e.atomic_number == 4 || e.atomic_number == 13) ? 3.0 * (target - la.electrons) / 2.0 : 6.0 * (target - la.electrons) / 2.0;
        if (la.expanded_octet) score += can_expand_octet(e) ? 0.5 * (la.electrons - target) : 100.0 * (la.electrons - target);
        if (la.formal_charge < 0) score += 2.0 * (max_en - electronegativity_of(e)) * -la.formal_charge;
        if (la.formal_charge > 0) score += 1.0 * electronegativity_of(e) * la.formal_charge / max_en;
        if (la.unpaired) score += 4.0;
    }
    out.lone_pair_electrons = lone_total;
    out.unpaired_electrons = unpaired_total;
    out.score = score;
    out.ok = true;
    for (std::size_t i = 0; i < n; ++i) {
        const LewisAtom& la = out.atoms[i];
        const char* sym = m_atoms[i].element->symbol;
        if (la.expanded_octet) out.notes.push_back(std::string(sym) + " has an expanded octet (" + std::to_string(la.electrons) + " electrons)");
        if (la.incomplete_octet && m_atoms[i].element->atomic_number != 1) out.notes.push_back(std::string(sym) + " has an incomplete octet (" + std::to_string(la.electrons) + " electrons)");
    }
    if (unpaired_total > 0) out.notes.push_back("odd number of electrons: the molecule is a radical with " + std::to_string(unpaired_total) + " unpaired electron" + (unpaired_total > 1 ? "s" : ""));
    return out;
}

LewisStructure Molecule::best_lewis_structure(int total_charge) const {
    ResonanceSet r = resonance_structures(total_charge, 1);
    if (r.structures.empty()) return lewis_structure(total_charge, true);
    return r.structures.front();
}

ResonanceSet Molecule::resonance_structures(int total_charge, std::size_t max_structures) const {
    ResonanceSet out;
    const std::size_t nb = m_bonds.size();
    const std::size_t na = m_atoms.size();
    if (na == 0) return out;
    int valence = 0;
    for (const MoleculeAtom& a : m_atoms) valence += a.element->valence_electrons();
    valence -= total_charge;
    std::vector<int> cap(na, 0);
    for (std::size_t i = 0; i < na; ++i) {
        const Element& e = *m_atoms[i].element;
        if (e.atomic_number <= 2) cap[i] = 1;
        else if (e.period == 2) cap[i] = e.group == 17 ? 1 : 4;
        else if (e.is_main_group() || e.is_noble_gas()) cap[i] = 7;
        else cap[i] = 9;
    }
    std::vector<int> maxo(nb, 3);
    for (std::size_t k = 0; k < nb; ++k) {
        const Element& a = *m_atoms[m_bonds[k].a].element;
        const Element& b = *m_atoms[m_bonds[k].b].element;
        if (a.atomic_number == 1 || b.atomic_number == 1 || a.atomic_number == 9 || b.atomic_number == 9) maxo[k] = 1;
    }
    std::vector<int> orders(nb, 1);
    std::vector<int> sum(na, 0);
    for (std::size_t k = 0; k < nb; ++k) {
        sum[m_bonds[k].a] += 1;
        sum[m_bonds[k].b] += 1;
    }
    for (std::size_t i = 0; i < na; ++i) {
        if (sum[i] > cap[i]) cap[i] = sum[i];
    }
    int base_bonding = 2 * static_cast<int>(nb);
    if (base_bonding > valence) {
        out.structures.push_back(lewis_structure(total_charge, true));
        return out;
    }
    double best = 1e18;
    std::vector<std::vector<int>> best_sets;
    Molecule work = *this;
    std::size_t visited = 0;
    const std::size_t node_limit = 400000;
    std::function<void(std::size_t, int)> dfs = [&](std::size_t k, int bonding) {
        if (++visited > node_limit) return;
        if (k == nb) {
            for (std::size_t j = 0; j < nb; ++j) work.m_bonds[j].order = static_cast<BondOrder>(orders[j]);
            const LewisStructure l = work.lewis_structure(total_charge, true);
            if (!l.ok) return;
            if (l.score < best - 1e-9) {
                best = l.score;
                best_sets.clear();
                best_sets.push_back(orders);
            } else if (std::fabs(l.score - best) <= 1e-9) {
                if (best_sets.size() < 4096) best_sets.push_back(orders);
            }
            return;
        }
        const std::size_t a = m_bonds[k].a;
        const std::size_t b = m_bonds[k].b;
        for (int o = 1; o <= maxo[k]; ++o) {
            const int extra = o - 1;
            if (sum[a] + extra > cap[a] || sum[b] + extra > cap[b]) break;
            if (bonding + 2 * extra > valence) break;
            orders[k] = o;
            sum[a] += extra;
            sum[b] += extra;
            dfs(k + 1, bonding + 2 * extra);
            sum[a] -= extra;
            sum[b] -= extra;
        }
        orders[k] = 1;
    };
    dfs(0, base_bonding);
    std::sort(best_sets.begin(), best_sets.end());
    best_sets.erase(std::unique(best_sets.begin(), best_sets.end()), best_sets.end());
    if (best_sets.empty()) {
        out.structures.push_back(lewis_structure(total_charge, true));
        return out;
    }
    out.average_bond_orders.assign(nb, 0.0);
    for (const std::vector<int>& set : best_sets) {
        for (std::size_t j = 0; j < nb; ++j) out.average_bond_orders[j] += set[j];
    }
    for (double& v : out.average_bond_orders) v /= static_cast<double>(best_sets.size());
    for (const std::vector<int>& set : best_sets) {
        if (out.structures.size() >= std::max<std::size_t>(1, max_structures)) break;
        for (std::size_t j = 0; j < nb; ++j) work.m_bonds[j].order = static_cast<BondOrder>(set[j]);
        out.structures.push_back(work.lewis_structure(total_charge, true));
    }
    if (best_sets.size() > out.structures.size()) out.structures.front().notes.push_back(std::to_string(best_sets.size()) + " equivalent structures in total");
    return out;
}

void Molecule::apply_lewis(const LewisStructure& lewis) {
    if (lewis.atoms.size() == m_atoms.size()) {
        for (std::size_t i = 0; i < m_atoms.size(); ++i) m_atoms[i].formal_charge = lewis.atoms[i].formal_charge;
    }
    if (lewis.bond_orders.size() == m_bonds.size()) {
        for (std::size_t k = 0; k < m_bonds.size(); ++k) m_bonds[k].order = lewis.bond_orders[k];
    }
}

bool Molecule::has_positions() const noexcept {
    for (const MoleculeAtom& a : m_atoms) {
        if (a.x != 0.0 || a.y != 0.0 || a.z != 0.0) return true;
    }
    return false;
}

void Molecule::center() {
    if (m_atoms.empty()) return;
    double cx = 0.0, cy = 0.0, cz = 0.0;
    for (const MoleculeAtom& a : m_atoms) {
        cx += a.x;
        cy += a.y;
        cz += a.z;
    }
    const double k = 1.0 / static_cast<double>(m_atoms.size());
    cx *= k;
    cy *= k;
    cz *= k;
    for (MoleculeAtom& a : m_atoms) {
        a.x -= cx;
        a.y -= cy;
        a.z -= cz;
    }
}

void Molecule::scale_positions(double factor) {
    for (MoleculeAtom& a : m_atoms) {
        a.x *= factor;
        a.y *= factor;
        a.z *= factor;
    }
}

void Molecule::layout_2d(double bond_length_pm) {
    const std::size_t n = m_atoms.size();
    if (n == 0) return;
    if (ring_count() > 0) {
        Molecule copy = *this;
        copy.layout_3d(800);
        double c[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
        for (const MoleculeAtom& a : copy.m_atoms) {
            const double v[3] = { a.x, a.y, a.z };
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) c[i][j] += v[i] * v[j];
            }
        }
        double vec[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        for (int sweep = 0; sweep < 50; ++sweep) {
            int p = 0, q = 1;
            double off = std::fabs(c[0][1]);
            if (std::fabs(c[0][2]) > off) {
                p = 0;
                q = 2;
                off = std::fabs(c[0][2]);
            }
            if (std::fabs(c[1][2]) > off) {
                p = 1;
                q = 2;
                off = std::fabs(c[1][2]);
            }
            if (off < 1e-12) break;
            const double theta = 0.5 * std::atan2(2.0 * c[p][q], c[q][q] - c[p][p]);
            const double cs = std::cos(theta), sn = std::sin(theta);
            for (int k = 0; k < 3; ++k) {
                const double ckp = c[k][p], ckq = c[k][q];
                c[k][p] = cs * ckp - sn * ckq;
                c[k][q] = sn * ckp + cs * ckq;
            }
            for (int k = 0; k < 3; ++k) {
                const double cpk = c[p][k], cqk = c[q][k];
                c[p][k] = cs * cpk - sn * cqk;
                c[q][k] = sn * cpk + cs * cqk;
            }
            for (int k = 0; k < 3; ++k) {
                const double vkp = vec[k][p], vkq = vec[k][q];
                vec[k][p] = cs * vkp - sn * vkq;
                vec[k][q] = sn * vkp + cs * vkq;
            }
        }
        int order[3] = { 0, 1, 2 };
        std::sort(order, order + 3, [&](int x, int y) { return c[x][x] > c[y][y]; });
        const double k = bond_length_pm / 140.0;
        for (std::size_t i = 0; i < n; ++i) {
            const MoleculeAtom& a = copy.m_atoms[i];
            const double v[3] = { a.x, a.y, a.z };
            double u = 0.0, w = 0.0;
            for (int d = 0; d < 3; ++d) {
                u += v[d] * vec[d][order[0]];
                w += v[d] * vec[d][order[1]];
            }
            m_atoms[i].x = u * k;
            m_atoms[i].y = w * k;
            m_atoms[i].z = 0.0;
        }
        return;
    }
    std::vector<std::vector<std::size_t>> adj(n);
    for (const MoleculeBond& b : m_bonds) {
        adj[b.a].push_back(b.b);
        adj[b.b].push_back(b.a);
    }
    std::vector<int> placed(n, 0);
    std::vector<double> px(n, 0.0), py(n, 0.0);
    double offset_x = 0.0;
    for (const std::vector<std::size_t>& frag : fragments()) {
        std::size_t root = frag.front();
        for (std::size_t a : frag) {
            if (adj[a].size() > adj[root].size()) root = a;
        }
        std::vector<std::size_t> queue{ root };
        placed[root] = 1;
        px[root] = offset_x;
        py[root] = 0.0;
        std::vector<double> heading(n, 0.0);
        for (std::size_t qi = 0; qi < queue.size(); ++qi) {
            const std::size_t a = queue[qi];
            std::vector<std::size_t> kids;
            for (std::size_t b : adj[a]) {
                if (!placed[b]) kids.push_back(b);
            }
            std::sort(kids.begin(), kids.end(), [&](std::size_t x, std::size_t y) { return m_atoms[x].element->atomic_number == 1 && m_atoms[y].element->atomic_number != 1 ? false : (m_atoms[x].element->atomic_number != 1 && m_atoms[y].element->atomic_number == 1); });
            const std::size_t total = adj[a].size();
            for (std::size_t k = 0; k < kids.size(); ++k) {
                const std::size_t b = kids[k];
                double angle;
                if (a == root) {
                    angle = 2.0 * PI * static_cast<double>(k) / static_cast<double>(std::max<std::size_t>(1, kids.size()));
                } else if (total == 2) {
                    const double turn = (qi % 2 == 0) ? 1.0 : -1.0;
                    angle = heading[a] + turn * PI / 3.0;
                } else {
                    angle = heading[a] + PI + 2.0 * PI * static_cast<double>(k + 1) / static_cast<double>(total);
                }
                const double len = (m_atoms[b].element->atomic_number == 1 || m_atoms[a].element->atomic_number == 1) ? 0.78 * bond_length_pm : bond_length_pm;
                px[b] = px[a] + len * std::cos(angle);
                py[b] = py[a] + len * std::sin(angle);
                heading[b] = angle;
                placed[b] = 1;
                queue.push_back(b);
            }
        }
        double max_x = offset_x;
        for (std::size_t a : frag) max_x = std::max(max_x, px[a]);
        offset_x = max_x + 2.5 * bond_length_pm;
    }
    std::vector<int> sp(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        int doubles = 0;
        bool triple = false;
        for (const MoleculeBond& b : m_bonds) {
            if (b.a != i && b.b != i) continue;
            if (b.order == BondOrder::Triple) triple = true;
            if (b.order == BondOrder::Double) ++doubles;
        }
        sp[i] = (triple || doubles >= 2) && adj[i].size() == 2 ? 1 : 0;
    }
    std::vector<double> fx(n), fy(n);
    double step = 0.08 * bond_length_pm;
    for (int it = 0; it < 500; ++it) {
        std::fill(fx.begin(), fx.end(), 0.0);
        std::fill(fy.begin(), fy.end(), 0.0);
        for (const MoleculeBond& b : m_bonds) {
            const double len = (m_atoms[b.a].element->atomic_number == 1 || m_atoms[b.b].element->atomic_number == 1) ? 0.78 * bond_length_pm : bond_length_pm;
            const double dx = px[b.b] - px[b.a], dy = py[b.b] - py[b.a];
            const double d = std::max(1e-6, std::sqrt(dx * dx + dy * dy));
            const double f = 3.0 * (d - len) / len;
            fx[b.a] += f * dx / d;
            fy[b.a] += f * dy / d;
            fx[b.b] -= f * dx / d;
            fy[b.b] -= f * dy / d;
        }
        for (std::size_t c = 0; c < n; ++c) {
            const std::size_t deg = adj[c].size();
            if (deg < 2) continue;
            const double ideal = sp[c] ? PI : (deg == 2 ? 2.0 * PI / 3.0 : 2.0 * PI / static_cast<double>(deg));
            for (std::size_t x = 0; x < deg; ++x) {
                for (std::size_t y = x + 1; y < deg; ++y) {
                    const std::size_t a = adj[c][x], b = adj[c][y];
                    const double la = std::hypot(px[a] - px[c], py[a] - py[c]);
                    const double lb = std::hypot(px[b] - px[c], py[b] - py[c]);
                    double theta = ideal;
                    if (deg > 2 && !sp[c]) theta = 2.0 * PI / static_cast<double>(deg);
                    const double target = std::sqrt(std::max(0.0, la * la + lb * lb - 2.0 * la * lb * std::cos(theta)));
                    const double dx = px[b] - px[a], dy = py[b] - py[a];
                    const double d = std::max(1e-6, std::sqrt(dx * dx + dy * dy));
                    if (deg > 3 && d > target) continue;
                    const double f = 0.5 * (d - target) / bond_length_pm;
                    fx[a] += f * dx / d;
                    fy[a] += f * dy / d;
                    fx[b] -= f * dx / d;
                    fy[b] -= f * dy / d;
                }
            }
        }
        for (std::size_t a = 0; a < n; ++a) {
            for (std::size_t b = a + 1; b < n; ++b) {
                const double dx = px[b] - px[a], dy = py[b] - py[a];
                double d2 = dx * dx + dy * dy;
                const double reach = 1.8 * bond_length_pm;
                if (d2 > reach * reach) continue;
                if (d2 < 1e-6) {
                    px[b] += 0.01 * bond_length_pm * (unit_random(a * 131 + b) - 0.5);
                    py[b] += 0.01 * bond_length_pm * (unit_random(a * 71 + b * 3) - 0.5);
                    continue;
                }
                bool bonded_pair = false;
                for (std::size_t nb2 : adj[a]) bonded_pair = bonded_pair || nb2 == b;
                if (bonded_pair) continue;
                const double d = std::sqrt(d2);
                const double f = 0.2 * (reach - d) / reach;
                fx[a] -= f * dx / d;
                fy[a] -= f * dy / d;
                fx[b] += f * dx / d;
                fy[b] += f * dy / d;
            }
        }
        for (std::size_t a = 0; a < n; ++a) {
            double mx = fx[a] * step, my = fy[a] * step;
            const double mag = std::hypot(mx, my);
            const double cap_move = 0.2 * bond_length_pm;
            if (mag > cap_move) {
                mx *= cap_move / mag;
                my *= cap_move / mag;
            }
            px[a] += mx;
            py[a] += my;
        }
        if (it > 300) step *= 0.99;
    }
    for (std::size_t i = 0; i < n; ++i) {
        m_atoms[i].x = px[i];
        m_atoms[i].y = py[i];
        m_atoms[i].z = 0.0;
    }
}

void Molecule::layout_3d(int iterations) {
    const std::size_t n = m_atoms.size();
    if (n == 0) return;
    std::vector<std::vector<std::size_t>> adj(n);
    for (const MoleculeBond& b : m_bonds) {
        adj[b.a].push_back(b.b);
        adj[b.b].push_back(b.a);
    }
    struct P {
        double x, y, z;
    };
    const auto norm = [](P v) {
        const double d = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return d > 1e-12 ? P{ v.x / d, v.y / d, v.z / d } : P{ 1, 0, 0 };
    };
    const auto cross = [](P a, P b) { return P{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
    const auto dot = [](P a, P b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    const auto rotate = [&](P v, P axis, double angle) {
        const P k = norm(axis);
        const double c = std::cos(angle), s2 = std::sin(angle);
        const P kxv = cross(k, v);
        const double kv = dot(k, v);
        return P{ v.x * c + kxv.x * s2 + k.x * kv * (1 - c), v.y * c + kxv.y * s2 + k.y * kv * (1 - c), v.z * c + kxv.z * s2 + k.z * kv * (1 - c) };
    };
    const auto template_for = [&](int steric) {
        std::vector<P> t;
        const double r3 = std::sqrt(3.0) / 2.0;
        switch (steric) {
            case 0: break;
            case 1: t = { { 1, 0, 0 } }; break;
            case 2: t = { { 1, 0, 0 }, { -1, 0, 0 } }; break;
            case 3: t = { { 1, 0, 0 }, { -0.5, r3, 0 }, { -0.5, -r3, 0 } }; break;
            case 4: t = { norm({ 1, 1, 1 }), norm({ 1, -1, -1 }), norm({ -1, 1, -1 }), norm({ -1, -1, 1 }) }; break;
            case 5: t = { { 1, 0, 0 }, { -0.5, r3, 0 }, { -0.5, -r3, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }; break;
            case 6: t = { { 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } }; break;
            case 7: {
                t = { { 0, 0, 1 }, { 0, 0, -1 } };
                for (int k = 0; k < 5; ++k) t.push_back({ std::cos(2 * PI * k / 5.0), std::sin(2 * PI * k / 5.0), 0 });
                break;
            }
            default: {
                for (int k = 0; k < steric; ++k) {
                    const double y = 1 - 2.0 * (k + 0.5) / steric;
                    const double rr = std::sqrt(std::max(0.0, 1 - y * y));
                    const double phi = k * PI * (3 - std::sqrt(5.0));
                    t.push_back({ std::cos(phi) * rr, y, std::sin(phi) * rr });
                }
                break;
            }
        }
        return t;
    };
    std::vector<int> lps(n, 0);
    for (std::size_t i = 0; i < n; ++i) lps[i] = adj[i].empty() ? 0 : std::max(0, lone_pairs(i));
    std::vector<P> pos(n, P{ 0, 0, 0 });
    std::vector<int> placed(n, 0);
    std::vector<std::vector<P>> lp_dirs(n);
    std::vector<double> bond_len(m_bonds.size(), 1.4);
    for (std::size_t k = 0; k < m_bonds.size(); ++k) {
        const double len = bond_length(m_bonds[k]);
        bond_len[k] = (len == len && len > 0.0) ? len / 100.0 : 1.4;
    }
    const auto length_between = [&](std::size_t a, std::size_t b) {
        for (std::size_t k = 0; k < m_bonds.size(); ++k) {
            if ((m_bonds[k].a == a && m_bonds[k].b == b) || (m_bonds[k].a == b && m_bonds[k].b == a)) return bond_len[k];
        }
        return 1.4;
    };
    double frag_offset = 0.0;
    for (const std::vector<std::size_t>& frag : fragments()) {
        std::size_t root = frag.front();
        for (std::size_t a : frag) {
            if (adj[a].size() + lps[a] > adj[root].size() + lps[root]) root = a;
        }
        pos[root] = { frag_offset, 0, 0 };
        placed[root] = 1;
        std::vector<std::pair<std::size_t, std::size_t>> queue{ { root, static_cast<std::size_t>(-1) } };
        for (std::size_t qi = 0; qi < queue.size(); ++qi) {
            const std::size_t c = queue[qi].first;
            const std::size_t parent = queue[qi].second;
            const int steric = static_cast<int>(adj[c].size()) + lps[c];
            std::vector<P> t = template_for(steric);
            if (t.empty()) continue;
            std::vector<std::size_t> lp_slots;
            std::vector<std::size_t> bond_slots;
            for (int k = 0; k < steric; ++k) {
                if (k < lps[c] && steric >= 5) lp_slots.push_back(static_cast<std::size_t>(k));
                else bond_slots.push_back(static_cast<std::size_t>(k));
            }
            if (steric < 5) {
                lp_slots.clear();
                bond_slots.clear();
                for (int k = 0; k < steric; ++k) {
                    if (k >= static_cast<int>(adj[c].size())) lp_slots.push_back(static_cast<std::size_t>(k));
                    else bond_slots.push_back(static_cast<std::size_t>(k));
                }
            }
            std::vector<P> world(t.size());
            if (parent == static_cast<std::size_t>(-1)) {
                for (std::size_t k = 0; k < t.size(); ++k) world[k] = t[k];
            } else {
                const P toward = norm(P{ pos[parent].x - pos[c].x, pos[parent].y - pos[c].y, pos[parent].z - pos[c].z });
                const P from = t[bond_slots.front()];
                P axis = cross(from, toward);
                const double cosang = std::max(-1.0, std::min(1.0, dot(from, toward)));
                double ang = std::acos(cosang);
                if (std::sqrt(dot(axis, axis)) < 1e-9) {
                    axis = std::fabs(from.x) < 0.9 ? cross(from, P{ 1, 0, 0 }) : cross(from, P{ 0, 1, 0 });
                    if (cosang > 0) ang = 0.0;
                }
                std::vector<P> aligned(t.size());
                for (std::size_t k = 0; k < t.size(); ++k) aligned[k] = ang == 0.0 ? t[k] : rotate(t[k], axis, ang);
                double best_score = -1e18;
                std::vector<P> best = aligned;
                for (int step = 0; step < 24; ++step) {
                    const double phi = 2 * PI * step / 24.0;
                    std::vector<P> cand(t.size());
                    for (std::size_t k = 0; k < t.size(); ++k) cand[k] = rotate(aligned[k], toward, phi);
                    double score = 0.0;
                    for (std::size_t k = 1; k < bond_slots.size() + lp_slots.size(); ++k) {
                        const std::size_t slot = k < bond_slots.size() ? bond_slots[k] : lp_slots[k - bond_slots.size()];
                        const P q{ pos[c].x + 1.3 * cand[slot].x, pos[c].y + 1.3 * cand[slot].y, pos[c].z + 1.3 * cand[slot].z };
                        for (std::size_t o = 0; o < n; ++o) {
                            if (!placed[o] || o == c || o == parent) continue;
                            const double dx = q.x - pos[o].x, dy = q.y - pos[o].y, dz = q.z - pos[o].z;
                            score -= 1.0 / std::max(0.05, dx * dx + dy * dy + dz * dz);
                        }
                    }
                    if (score > best_score + 1e-12) {
                        best_score = score;
                        best = cand;
                    }
                }
                world = best;
            }
            std::size_t next_slot = parent == static_cast<std::size_t>(-1) ? 0 : 1;
            for (std::size_t nb2 : adj[c]) {
                if (nb2 == parent) continue;
                if (next_slot >= bond_slots.size()) break;
                const P d = world[bond_slots[next_slot++]];
                if (placed[nb2]) continue;
                const double len = length_between(c, nb2);
                pos[nb2] = { pos[c].x + len * d.x, pos[c].y + len * d.y, pos[c].z + len * d.z };
                placed[nb2] = 1;
                queue.push_back({ nb2, c });
            }
            for (std::size_t slot : lp_slots) lp_dirs[c].push_back(world[slot]);
        }
        double max_x = frag_offset;
        for (std::size_t a : frag) max_x = std::max(max_x, pos[a].x);
        frag_offset = max_x + 4.0;
    }
    const bool rings = ring_count() > 0;
    std::vector<std::size_t> lp_center;
    for (std::size_t i = 0; i < n; ++i) {
        for (int k = 0; k < lps[i]; ++k) lp_center.push_back(i);
    }
    const std::size_t total = n + lp_center.size();
    {
        std::vector<std::size_t> used(n, 0);
        for (std::size_t k = 0; k < lp_center.size(); ++k) {
            const std::size_t c = lp_center[k];
            P d = used[c] < lp_dirs[c].size() ? lp_dirs[c][used[c]] : norm(P{ unit_random(k * 7 + 1) - 0.5, unit_random(k * 11 + 2) - 0.5, unit_random(k * 13 + 3) - 0.5 });
            ++used[c];
            pos.push_back({ pos[c].x + 0.6 * d.x, pos[c].y + 0.6 * d.y, pos[c].z + 0.6 * d.z });
        }
    }
    if (!rings) iterations = std::min(iterations, 150);
    std::vector<std::vector<std::size_t>> domains(n);
    for (std::size_t i = 0; i < n; ++i) domains[i] = adj[i];
    for (std::size_t k = 0; k < lp_center.size(); ++k) domains[lp_center[k]].push_back(n + k);
    std::vector<double> rest(m_bonds.size());
    for (std::size_t k = 0; k < m_bonds.size(); ++k) {
        const double len = bond_length(m_bonds[k]);
        rest[k] = (len == len && len > 0.0) ? len / 100.0 : 1.4;
    }
    std::vector<char> close_pair;
    const auto pair_index = [&](std::size_t a, std::size_t b) { return a * n + b; };
    close_pair.assign(n * n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        close_pair[pair_index(i, i)] = 1;
        for (std::size_t j : adj[i]) {
            close_pair[pair_index(i, j)] = 1;
            for (std::size_t k : adj[j]) close_pair[pair_index(i, k)] = 1;
        }
    }
    std::vector<P> grad(total);
    const auto energy_and_gradient = [&](std::vector<P>& g) {
        double e = 0.0;
        for (P& p : g) p = { 0, 0, 0 };
        for (std::size_t k = 0; k < m_bonds.size(); ++k) {
            const std::size_t a = m_bonds[k].a, b = m_bonds[k].b;
            const double dx = pos[b].x - pos[a].x, dy = pos[b].y - pos[a].y, dz = pos[b].z - pos[a].z;
            const double d = std::max(1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
            const double diff = d - rest[k];
            e += 40.0 * diff * diff;
            const double f = 80.0 * diff / d;
            g[a].x -= f * dx; g[a].y -= f * dy; g[a].z -= f * dz;
            g[b].x += f * dx; g[b].y += f * dy; g[b].z += f * dz;
        }
        for (std::size_t k = 0; k < lp_center.size(); ++k) {
            const std::size_t a = lp_center[k], b = n + k;
            const double dx = pos[b].x - pos[a].x, dy = pos[b].y - pos[a].y, dz = pos[b].z - pos[a].z;
            const double d = std::max(1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
            const double diff = d - 0.6;
            e += 40.0 * diff * diff;
            const double f = 80.0 * diff / d;
            g[a].x -= f * dx; g[a].y -= f * dy; g[a].z -= f * dz;
            g[b].x += f * dx; g[b].y += f * dy; g[b].z += f * dz;
        }
        for (std::size_t c = 0; c < n; ++c) {
            const std::vector<std::size_t>& dom = domains[c];
            for (std::size_t x = 0; x < dom.size(); ++x) {
                for (std::size_t y = x + 1; y < dom.size(); ++y) {
                    const std::size_t a = dom[x], b = dom[y];
                    double ux = pos[a].x - pos[c].x, uy = pos[a].y - pos[c].y, uz = pos[a].z - pos[c].z;
                    double vx = pos[b].x - pos[c].x, vy = pos[b].y - pos[c].y, vz = pos[b].z - pos[c].z;
                    const double lu = std::max(1e-6, std::sqrt(ux * ux + uy * uy + uz * uz));
                    const double lv = std::max(1e-6, std::sqrt(vx * vx + vy * vy + vz * vz));
                    ux /= lu; uy /= lu; uz /= lu;
                    vx /= lv; vy /= lv; vz /= lv;
                    const double wx = ux - vx, wy = uy - vy, wz = uz - vz;
                    const double d = std::max(1e-3, std::sqrt(wx * wx + wy * wy + wz * wz));
                    const double w = (a >= n ? 1.25 : 1.0) * (b >= n ? 1.25 : 1.0) * 3.0;
                    e += w / d;
                    const double dEd = -w / (d * d);
                    const double gx = dEd * wx / d, gy = dEd * wy / d, gz = dEd * wz / d;
                    const double du = gx * ux + gy * uy + gz * uz;
                    const double pax = (gx - du * ux) / lu, pay = (gy - du * uy) / lu, paz = (gz - du * uz) / lu;
                    const double dv = gx * vx + gy * vy + gz * vz;
                    const double pbx = -(gx - dv * vx) / lv, pby = -(gy - dv * vy) / lv, pbz = -(gz - dv * vz) / lv;
                    g[a].x += pax; g[a].y += pay; g[a].z += paz;
                    g[b].x += pbx; g[b].y += pby; g[b].z += pbz;
                    g[c].x -= pax + pbx; g[c].y -= pay + pby; g[c].z -= paz + pbz;
                }
            }
        }
        for (std::size_t a = 0; a < n; ++a) {
            for (std::size_t b = a + 1; b < n; ++b) {
                if (close_pair[pair_index(a, b)]) continue;
                const double dx = pos[b].x - pos[a].x, dy = pos[b].y - pos[a].y, dz = pos[b].z - pos[a].z;
                const double d2 = std::max(1e-4, dx * dx + dy * dy + dz * dz);
                if (d2 > 16.0) continue;
                const double w = 0.4;
                e += w / d2 - w / 16.0;
                const double f = -2.0 * w / (d2 * d2);
                g[a].x -= f * dx; g[a].y -= f * dy; g[a].z -= f * dz;
                g[b].x += f * dx; g[b].y += f * dy; g[b].z += f * dz;
            }
        }
        for (const MoleculeBond& bond : m_bonds) {
            if (bond.order != BondOrder::Double && bond.order != BondOrder::Aromatic) continue;
            const std::size_t a = bond.a, b = bond.b;
            for (std::size_t x : adj[a]) {
                if (x == b) continue;
                for (std::size_t y : adj[b]) {
                    if (y == a || y == x) continue;
                    const double b1x = pos[a].x - pos[x].x, b1y = pos[a].y - pos[x].y, b1z = pos[a].z - pos[x].z;
                    const double b2x = pos[b].x - pos[a].x, b2y = pos[b].y - pos[a].y, b2z = pos[b].z - pos[a].z;
                    const double b3x = pos[y].x - pos[b].x, b3y = pos[y].y - pos[b].y, b3z = pos[y].z - pos[b].z;
                    const double n1x = b1y * b2z - b1z * b2y, n1y = b1z * b2x - b1x * b2z, n1z = b1x * b2y - b1y * b2x;
                    const double n2x = b2y * b3z - b2z * b3y, n2y = b2z * b3x - b2x * b3z, n2z = b2x * b3y - b2y * b3x;
                    const double l1 = n1x * n1x + n1y * n1y + n1z * n1z;
                    const double l2 = n2x * n2x + n2y * n2y + n2z * n2z;
                    if (l1 < 1e-8 || l2 < 1e-8) continue;
                    const double l2b = std::sqrt(b2x * b2x + b2y * b2y + b2z * b2z);
                    const double s_vol = (b1x * n2x + b1y * n2y + b1z * n2z) * l2b;
                    const double kplane = 0.8;
                    const double ratio = s_vol * s_vol / (l1 * l2);
                    e += kplane * ratio;
                    const double eps = 1e-4;
                    const std::size_t ids[4] = { x, a, b, y };
                    for (std::size_t id : ids) {
                        for (int axis = 0; axis < 3; ++axis) {
                            double& coord = axis == 0 ? pos[id].x : (axis == 1 ? pos[id].y : pos[id].z);
                            const double saved = coord;
                            coord = saved + eps;
                            const auto value = [&]() {
                                const double c1x = pos[a].x - pos[x].x, c1y = pos[a].y - pos[x].y, c1z = pos[a].z - pos[x].z;
                                const double c2x = pos[b].x - pos[a].x, c2y = pos[b].y - pos[a].y, c2z = pos[b].z - pos[a].z;
                                const double c3x = pos[y].x - pos[b].x, c3y = pos[y].y - pos[b].y, c3z = pos[y].z - pos[b].z;
                                const double m1x = c1y * c2z - c1z * c2y, m1y = c1z * c2x - c1x * c2z, m1z = c1x * c2y - c1y * c2x;
                                const double m2x = c2y * c3z - c2z * c3y, m2y = c2z * c3x - c2x * c3z, m2z = c2x * c3y - c2y * c3x;
                                const double q1 = m1x * m1x + m1y * m1y + m1z * m1z;
                                const double q2 = m2x * m2x + m2y * m2y + m2z * m2z;
                                if (q1 < 1e-10 || q2 < 1e-10) return 0.0;
                                const double lb = std::sqrt(c2x * c2x + c2y * c2y + c2z * c2z);
                                const double sv = (c1x * m2x + c1y * m2y + c1z * m2z) * lb;
                                return kplane * sv * sv / (q1 * q2);
                            };
                            const double ep = value();
                            coord = saved - eps;
                            const double em = value();
                            coord = saved;
                            const double gcomp = (ep - em) / (2.0 * eps);
                            if (axis == 0) g[id].x += gcomp;
                            else if (axis == 1) g[id].y += gcomp;
                            else g[id].z += gcomp;
                        }
                    }
                }
            }
        }
        return e;
    };
    std::vector<P> trial_grad(total);
    const auto relax = [&](int iters) {
        double step = 0.01;
        double energy = energy_and_gradient(grad);
        for (int it = 0; it < iters; ++it) {
            const std::vector<P> saved = pos;
            for (std::size_t i = 0; i < total; ++i) {
                double mx = -step * grad[i].x, my = -step * grad[i].y, mz = -step * grad[i].z;
                const double mag = std::sqrt(mx * mx + my * my + mz * mz);
                if (mag > 0.15) {
                    mx *= 0.15 / mag;
                    my *= 0.15 / mag;
                    mz *= 0.15 / mag;
                }
                pos[i].x += mx;
                pos[i].y += my;
                pos[i].z += mz;
            }
            const double e2 = energy_and_gradient(trial_grad);
            if (e2 <= energy) {
                energy = e2;
                grad.swap(trial_grad);
                step = std::min(step * 1.15, 0.2);
            } else {
                pos = saved;
                step *= 0.5;
                if (step < 1e-8) break;
            }
        }
        return energy;
    };
    if (rings) {
        const std::vector<P> start = pos;
        std::vector<P> best_pos = pos;
        double best_e = 1e300;
        const double radius = 1.1 * std::cbrt(static_cast<double>(n)) * 1.3;
        for (int seed = 0; seed < 5; ++seed) {
            if (seed == 0) {
                pos = start;
            } else {
                for (std::size_t i = 0; i < n; ++i) {
                    pos[i] = { radius * (2.0 * unit_random(seed * 7919ULL + i * 3ULL) - 1.0), radius * (2.0 * unit_random(seed * 104729ULL + i * 5ULL + 1) - 1.0), radius * (2.0 * unit_random(seed * 1299709ULL + i * 7ULL + 2) - 1.0) };
                }
                for (std::size_t k = 0; k < lp_center.size(); ++k) {
                    const std::size_t c = lp_center[k];
                    const P d = norm(P{ unit_random(seed * 31ULL + k * 11ULL) - 0.5, unit_random(seed * 37ULL + k * 13ULL) - 0.5, unit_random(seed * 41ULL + k * 17ULL) - 0.5 });
                    pos[n + k] = { pos[c].x + 0.6 * d.x, pos[c].y + 0.6 * d.y, pos[c].z + 0.6 * d.z };
                }
            }
            const double e = relax(iterations);
            if (e < best_e) {
                best_e = e;
                best_pos = pos;
            }
        }
        pos = best_pos;
    } else {
        relax(iterations);
    }
    for (std::size_t i = 0; i < n; ++i) {
        m_atoms[i].x = pos[i].x * 100.0;
        m_atoms[i].y = pos[i].y * 100.0;
        m_atoms[i].z = pos[i].z * 100.0;
    }
    center();
}

double reaction_enthalpy_from_bonds(const std::vector<std::pair<Molecule, double>>& reactants, const std::vector<std::pair<Molecule, double>>& products) {
    double total = 0.0;
    for (const auto& r : reactants) total += r.second * r.first.total_bond_energy();
    for (const auto& p : products) total -= p.second * p.first.total_bond_energy();
    return total;
}

} // namespace chemistry
} // namespace fizmo
