#include "phase_diagram.hpp"

#include "equation_of_state.hpp"
#include "nomenclature.hpp"
#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace fizmo {
namespace chemistry {

namespace {

struct JobackGroup {
    const char* name;
    double tc;
    double pc;
    double vc;
    double tb;
    double tm;
    double hf;
    double gf;
    double hfus;
    double hvap;
};

const JobackGroup JOBACK[] = {
    { "-CH3", 0.0141, -0.0012, 65, 23.58, -5.10, -76.45, -43.96, 0.908, 2.373 },
    { "-CH2-", 0.0189, 0.0000, 56, 22.88, 11.27, -20.64, 8.42, 2.590, 2.226 },
    { ">CH-", 0.0164, 0.0020, 41, 21.74, 12.64, 29.89, 58.36, 0.749, 1.691 },
    { ">C<", 0.0067, 0.0043, 27, 18.25, 46.43, 82.23, 116.02, -1.460, 0.636 },
    { "=CH2", 0.0113, -0.0028, 56, 18.18, -4.32, -9.630, 3.77, -0.473, 1.724 },
    { "=CH-", 0.0129, -0.0006, 46, 24.96, 8.73, 37.97, 48.53, 2.691, 2.205 },
    { "=C<", 0.0117, 0.0011, 38, 24.14, 11.14, 83.99, 92.36, 3.063, 2.138 },
    { "=C=", 0.0026, 0.0028, 36, 26.15, 17.78, 142.14, 136.70, 4.720, 2.661 },
    { "#CH", 0.0027, -0.0008, 46, 9.20, -11.18, 79.30, 77.71, 2.322, 1.155 },
    { "#C-", 0.0020, 0.0016, 37, 27.38, 64.32, 115.51, 109.82, 4.151, 3.302 },
    { "ring -CH2-", 0.0100, 0.0025, 48, 27.15, 7.75, -26.80, -3.68, 0.490, 2.398 },
    { "ring >CH-", 0.0122, 0.0004, 38, 21.78, 19.88, 8.67, 40.99, 3.243, 1.942 },
    { "ring >C<", 0.0042, 0.0061, 27, 21.32, 60.15, 79.72, 87.88, -1.373, 0.644 },
    { "ring =CH-", 0.0082, 0.0011, 41, 26.73, 8.13, 2.09, 11.30, 1.101, 2.544 },
    { "ring =C<", 0.0143, 0.0008, 32, 31.01, 37.02, 46.43, 54.05, 2.394, 3.059 },
    { "-F", 0.0111, -0.0057, 27, -0.03, -15.78, -251.92, -247.19, 1.398, -0.670 },
    { "-Cl", 0.0105, -0.0049, 58, 38.13, 13.55, -71.55, -64.31, 2.515, 4.532 },
    { "-Br", 0.0133, 0.0057, 71, 66.86, 43.43, -29.48, -38.06, 3.603, 6.582 },
    { "-I", 0.0068, -0.0034, 97, 93.84, 41.69, 21.06, 5.74, 2.724, 9.520 },
    { "-OH (alcohol)", 0.0741, 0.0112, 28, 92.88, 44.45, -208.04, -189.20, 2.406, 16.826 },
    { "-OH (phenol)", 0.0240, 0.0184, -25, 76.34, 82.83, -221.65, -197.37, 4.490, 12.499 },
    { "-O-", 0.0168, 0.0015, 18, 22.42, 22.23, -132.22, -105.00, 1.188, 2.410 },
    { "ring -O-", 0.0098, 0.0048, 13, 31.22, 23.05, -138.16, -98.22, 5.879, 4.682 },
    { ">C=O", 0.0380, 0.0031, 62, 76.75, 61.20, -133.22, -120.50, 4.189, 8.972 },
    { "ring >C=O", 0.0284, 0.0028, 55, 94.97, 75.97, -164.50, -126.27, 0.0, 6.645 },
    { "O=CH- (aldehyde)", 0.0379, 0.0030, 82, 72.24, 36.90, -162.03, -143.48, 3.197, 9.093 },
    { "-COOH (acid)", 0.0791, 0.0077, 89, 169.09, 155.50, -426.72, -387.87, 11.051, 19.537 },
    { "-COO- (ester)", 0.0481, 0.0005, 82, 81.10, 53.60, -337.92, -301.95, 6.959, 9.633 },
    { "=O (other)", 0.0143, 0.0101, 36, -10.50, 2.08, -247.61, -250.83, 3.624, 5.909 },
    { "-NH2", 0.0243, 0.0109, 38, 73.23, 66.89, -22.02, 14.07, 3.515, 10.788 },
    { ">NH", 0.0295, 0.0077, 35, 50.17, 52.66, 53.47, 89.39, 5.099, 6.436 },
    { "ring >NH", 0.0130, 0.0114, 29, 52.82, 101.51, 31.65, 75.61, 7.490, 6.930 },
    { ">N-", 0.0169, 0.0074, 9, 11.74, 48.84, 123.34, 163.16, 4.703, 1.896 },
    { "-N=", 0.0255, -0.0099, 0, 74.60, 0.0, 23.61, 0.0, 0.0, 3.335 },
    { "ring -N=", 0.0085, 0.0076, 34, 57.55, 68.40, 55.52, 79.93, 3.649, 6.528 },
    { "=NH", 0.0, 0.0, 0, 83.08, 68.91, 93.70, 119.66, 0.0, 5.009 },
    { "-CN", 0.0496, -0.0101, 91, 125.66, 59.89, 88.43, 89.22, 2.414, 12.851 },
    { "-NO2", 0.0437, 0.0064, 91, 152.54, 127.24, -66.57, -16.83, 9.679, 16.738 },
    { "-SH", 0.0031, 0.0084, 63, 63.56, 20.09, -17.33, -22.99, 2.360, 6.884 },
    { "-S-", 0.0119, 0.0049, 54, 68.78, 34.40, 41.87, 33.12, 4.130, 6.817 },
    { "ring -S-", 0.0019, 0.0051, 38, 52.10, 79.93, 39.10, 27.76, 1.557, 5.984 },
};

const JobackGroup* joback_group(const std::string& name) {
    for (const JobackGroup& g : JOBACK) {
        if (name == g.name) return &g;
    }
    return nullptr;
}


struct VaporCurve {
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    bool three_point = false;
    CriticalConstants crit;
    double correction = 0.0;
    double tt = NO_VALUE;
    double tc = NO_VALUE;

    double ln_p(double t) const {
        if (three_point) return a + b / t + c * std::log(t);
        double lp = std::log(lee_kesler_vapor_pressure(crit, t));
        if (tt == tt && correction != 0.0) lp += correction * (1.0 / t - 1.0 / tc) / (1.0 / tt - 1.0 / tc);
        return lp;
    }
    double dlnp_dinv_t(double t) const {
        if (three_point) return b - c * t;
        const double h = 1e-4 * t;
        const double l1 = ln_p(t - h), l2 = ln_p(t + h);
        return (l2 - l1) / (1.0 / (t + h) - 1.0 / (t - h));
    }
};

VaporCurve vapor_curve(const PhaseProperties& p) {
    VaporCurve v;
    v.crit = CriticalConstants{ p.critical_temperature, p.critical_pressure, p.critical_volume, value_or(p.acentric_factor, 0.0) };
    v.tt = p.triple_temperature;
    v.tc = p.critical_temperature;
    const double tt = p.triple_temperature, pt = p.triple_pressure, tb = p.boiling_point, tc = p.critical_temperature, pc = p.critical_pressure;
    if (tt == tt && pt == pt && pt > 0 && tb == tb && tc == tc && pc == pc && tt < tb - 1.0 && tb < tc - 1.0) {
        const double x[3] = { tt, tb, tc };
        const double y[3] = { std::log(pt), std::log(ATMOSPHERE), std::log(pc) };
        double m[3][4];
        for (int i = 0; i < 3; ++i) {
            m[i][0] = 1.0;
            m[i][1] = 1.0 / x[i];
            m[i][2] = std::log(x[i]);
            m[i][3] = y[i];
        }
        for (int col = 0; col < 3; ++col) {
            int piv = col;
            for (int r = col + 1; r < 3; ++r) {
                if (std::fabs(m[r][col]) > std::fabs(m[piv][col])) piv = r;
            }
            for (int k = 0; k < 4; ++k) std::swap(m[col][k], m[piv][k]);
            for (int r = 0; r < 3; ++r) {
                if (r == col) continue;
                const double f = m[r][col] / m[col][col];
                for (int k = 0; k < 4; ++k) m[r][k] -= f * m[col][k];
            }
        }
        v.a = m[0][3] / m[0][0];
        v.b = m[1][3] / m[1][1];
        v.c = m[2][3] / m[2][2];
        bool monotonic = true;
        double prev = -1e300;
        for (int i = 0; i <= 50; ++i) {
            const double t = tt + (tc - tt) * i / 50.0;
            const double lp = v.a + v.b / t + v.c * std::log(t);
            if (lp < prev) monotonic = false;
            prev = lp;
        }
        v.three_point = monotonic;
    }
    if (!v.three_point && tt == tt && pt == pt && pt > 0) v.correction = std::log(pt) - std::log(lee_kesler_vapor_pressure(v.crit, tt));
    return v;
}

void mark(PhaseProperties& p, const char* what) {
    if (std::find(p.estimated.begin(), p.estimated.end(), what) == p.estimated.end()) p.estimated.emplace_back(what);
}

std::vector<char> ring_bonds(const Molecule& m) {
    std::vector<char> in(m.bond_count(), 0);
    const auto& bonds = m.bonds();
    for (std::size_t k = 0; k < bonds.size(); ++k) {
        const std::size_t a = bonds[k].a, b = bonds[k].b;
        std::vector<char> seen(m.atom_count(), 0);
        std::vector<std::size_t> stack{ a };
        seen[a] = 1;
        bool found = false;
        while (!stack.empty() && !found) {
            const std::size_t x = stack.back();
            stack.pop_back();
            for (std::size_t j = 0; j < bonds.size(); ++j) {
                if (j == k) continue;
                if (bonds[j].a != x && bonds[j].b != x) continue;
                const std::size_t y = bonds[j].other(x);
                if (y == b) {
                    found = true;
                    break;
                }
                if (!seen[y]) {
                    seen[y] = 1;
                    stack.push_back(y);
                }
            }
        }
        in[k] = found ? 1 : 0;
    }
    return in;
}

} // namespace

const char* to_string(PvtRegion r) noexcept {
    switch (r) {
        case PvtRegion::Solid: return "solid";
        case PvtRegion::Liquid: return "liquid";
        case PvtRegion::Gas: return "gas";
        case PvtRegion::Supercritical: return "supercritical fluid";
        case PvtRegion::LiquidVapor: return "liquid + vapor";
        case PvtRegion::SolidLiquid: return "solid + liquid";
        default: return "solid + vapor";
    }
}

double edmister_acentric_factor(double tb, double tc, double pc) noexcept {
    const double theta = tb / tc;
    return 3.0 / 7.0 * theta / (1.0 - theta) * std::log10(pc / ATMOSPHERE) - 1.0;
}

JobackEstimate joback(const Molecule& m) {
    JobackEstimate out;
    const auto& atoms = m.atoms();
    const auto& bonds = m.bonds();
    const std::size_t n = atoms.size();
    out.atoms = static_cast<int>(n);
    if (n == 0) return out;
    const std::vector<char> ring = ring_bonds(m);
    std::vector<char> atom_ring(n, 0), used(n, 0);
    for (std::size_t k = 0; k < bonds.size(); ++k) {
        if (ring[k]) atom_ring[bonds[k].a] = atom_ring[bonds[k].b] = 1;
    }
    std::map<std::string, int> counts;
    const auto z_of = [&](std::size_t i) { return atoms[i].element->atomic_number; };
    const auto hcount = [&](std::size_t i) {
        int h = 0;
        for (std::size_t j : m.neighbors(i)) h += z_of(j) == 1;
        return h;
    };
    const auto order_between = [&](std::size_t a, std::size_t b) {
        const MoleculeBond* bond = m.find_bond(a, b);
        return bond ? bond->order : BondOrder::Single;
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (z_of(i) != 7 || used[i]) continue;
        std::vector<std::size_t> oxy;
        for (std::size_t j : m.neighbors(i)) {
            if (z_of(j) == 8 && m.degree(j) == 1) oxy.push_back(j);
        }
        if (oxy.size() == 2) {
            ++counts["-NO2"];
            used[i] = used[oxy[0]] = used[oxy[1]] = 1;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (z_of(i) != 6 || used[i]) continue;
        for (std::size_t j : m.neighbors(i)) {
            if (z_of(j) == 7 && order_between(i, j) == BondOrder::Triple) {
                ++counts["-CN"];
                used[i] = used[j] = 1;
            }
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (z_of(i) != 6 || used[i]) continue;
        std::size_t carbonyl_o = n;
        for (std::size_t j : m.neighbors(i)) {
            if (z_of(j) == 8 && order_between(i, j) == BondOrder::Double && m.degree(j) == 1) carbonyl_o = j;
        }
        if (carbonyl_o == n) continue;
        std::size_t hydroxyl = n, ester = n;
        for (std::size_t j : m.neighbors(i)) {
            if (j == carbonyl_o || z_of(j) != 8 || used[j]) continue;
            if (hcount(j) == 1 && m.degree(j) == 2) hydroxyl = j;
            else if (hcount(j) == 0 && m.degree(j) == 2) ester = j;
        }
        if (hydroxyl != n) {
            ++counts["-COOH (acid)"];
            used[i] = used[carbonyl_o] = used[hydroxyl] = 1;
            for (std::size_t j : m.neighbors(hydroxyl)) {
                if (z_of(j) == 1) used[j] = 1;
            }
        } else if (ester != n) {
            ++counts["-COO- (ester)"];
            used[i] = used[carbonyl_o] = used[ester] = 1;
        } else if (hcount(i) == 1) {
            ++counts["O=CH- (aldehyde)"];
            used[i] = used[carbonyl_o] = 1;
        } else {
            ++counts[atom_ring[i] ? "ring >C=O" : ">C=O"];
            used[i] = used[carbonyl_o] = 1;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (used[i] || z_of(i) == 1) continue;
        const int z = z_of(i);
        const int h = hcount(i);
        int single = 0, dbl = 0, trip = 0, arom = 0;
        for (std::size_t j : m.neighbors(i)) {
            if (z_of(j) == 1) continue;
            switch (order_between(i, j)) {
                case BondOrder::Double: ++dbl; break;
                case BondOrder::Triple: ++trip; break;
                case BondOrder::Aromatic: ++arom; break;
                default: ++single; break;
            }
        }
        std::string g;
        const bool r = atom_ring[i] != 0;
        if (z == 6) {
            if (trip) g = h == 1 ? "#CH" : "#C-";
            else if (dbl == 2) g = "=C=";
            else if (dbl == 1 || arom > 0) g = r ? (h >= 1 ? "ring =CH-" : "ring =C<") : (h == 2 ? "=CH2" : (h == 1 ? "=CH-" : "=C<"));
            else if (r) g = h == 2 ? "ring -CH2-" : (h == 1 ? "ring >CH-" : "ring >C<");
            else g = h == 3 ? "-CH3" : (h == 2 ? "-CH2-" : (h == 1 ? ">CH-" : (h == 0 ? ">C<" : "")));
        } else if (z == 8) {
            if (h == 1 && single + arom == 1) {
                bool phenol = false;
                for (std::size_t j : m.neighbors(i)) {
                    if (z_of(j) == 1) continue;
                    for (std::size_t k : m.neighbors(j)) {
                        if (order_between(j, k) == BondOrder::Aromatic) phenol = true;
                    }
                }
                g = phenol ? "-OH (phenol)" : "-OH (alcohol)";
            } else if (h == 0 && single == 2) {
                g = r ? "ring -O-" : "-O-";
            } else if (dbl == 1 && h == 0) {
                g = "=O (other)";
            }
        } else if (z == 7) {
            if (h == 2 && single == 1) g = "-NH2";
            else if (h == 1 && single == 2) g = r ? "ring >NH" : ">NH";
            else if (h == 0 && single == 3) g = ">N-";
            else if (dbl == 1 || arom > 0) g = (r || arom > 0) ? "ring -N=" : (h == 1 ? "=NH" : "-N=");
        } else if (z == 16) {
            if (h == 1 && single == 1) g = "-SH";
            else if (h == 0 && single == 2) g = r ? "ring -S-" : "-S-";
        } else if (z == 9 && single == 1) g = "-F";
        else if (z == 17 && single == 1) g = "-Cl";
        else if (z == 35 && single == 1) g = "-Br";
        else if (z == 53 && single == 1) g = "-I";
        if (g.empty()) {
            out.unmatched.push_back(std::string(atoms[i].element->symbol) + " (atom " + std::to_string(i + 1) + ")");
            continue;
        }
        ++counts[g];
    }
    double stc = 0, spc = 0, svc = 0, stb = 0, stm = 0, shf = 0, sgf = 0, shfus = 0, shvap = 0;
    for (const auto& [name, count] : counts) {
        const JobackGroup* g = joback_group(name);
        if (!g) continue;
        out.groups.push_back({ name, count });
        stc += count * g->tc;
        spc += count * g->pc;
        svc += count * g->vc;
        stb += count * g->tb;
        stm += count * g->tm;
        shf += count * g->hf;
        sgf += count * g->gf;
        shfus += count * g->hfus;
        shvap += count * g->hvap;
    }
    if (out.groups.empty()) return out;
    out.boiling_point = 198.0 + stb;
    out.melting_point = 122.5 + stm;
    out.critical_temperature = out.boiling_point / (0.584 + 0.965 * stc - stc * stc);
    out.critical_pressure = 1e5 / std::pow(0.113 + 0.0032 * out.atoms - spc, 2.0);
    out.critical_volume = (17.5 + svc) * 1e-6;
    out.enthalpy_of_formation = (68.29 + shf) * 1000.0;
    out.gibbs_of_formation = (53.88 + sgf) * 1000.0;
    out.vaporization_enthalpy = (15.30 + shvap) * 1000.0;
    out.fusion_enthalpy = (-0.88 + shfus) * 1000.0;
    out.acentric_factor = edmister_acentric_factor(out.boiling_point, out.critical_temperature, out.critical_pressure);
    out.ok = out.unmatched.empty();
    return out;
}

double PhaseProperties::fusion_volume_change() const noexcept {
    if (!(liquid_density > 0) || !(solid_density > 0) || !(molar_mass > 0)) return NO_VALUE;
    return molar_mass / 1000.0 * (1.0 / liquid_density - 1.0 / solid_density);
}

void complete_phase_properties(PhaseProperties& p) {
    const bool metal_like = p.source.find("metal") != std::string::npos;
    const bool ionic_like = p.source.find("ionic") != std::string::npos;
    if (!(p.boiling_point > 0) && p.critical_temperature > 0) {
        p.boiling_point = p.critical_temperature / 1.5;
        mark(p, "boiling point");
    }
    if (!(p.critical_temperature > 0) && p.boiling_point > 0) {
        p.critical_temperature = (metal_like ? 2.0 : (ionic_like ? 1.6 : 1.5)) * p.boiling_point;
        mark(p, "critical temperature");
    }
    if (!(p.melting_point > 0) && p.triple_temperature > 0) p.melting_point = p.triple_temperature;
    if (!(p.melting_point > 0) && p.boiling_point > 0) {
        p.melting_point = 0.6 * p.boiling_point;
        mark(p, "melting point");
    }
    if (!(p.liquid_density > 0) && p.solid_density > 0) {
        p.liquid_density = p.solid_density / 1.08;
        mark(p, "liquid density");
    }
    if (!(p.critical_volume > 0)) {
        if (p.liquid_density > 0 && p.molar_mass > 0) p.critical_volume = 2.7 * (p.molar_mass / 1000.0) / p.liquid_density;
        else p.critical_volume = 2.0e-4;
        mark(p, "critical volume");
    }
    if (!(p.critical_pressure > 0) && p.critical_temperature > 0) {
        p.critical_pressure = 0.27 * GAS_CONSTANT * p.critical_temperature / p.critical_volume;
        mark(p, "critical pressure");
    }
    if (!(p.acentric_factor == p.acentric_factor) && p.boiling_point > 0 && p.critical_temperature > 0 && p.critical_pressure > 0) {
        p.acentric_factor = edmister_acentric_factor(p.boiling_point, p.critical_temperature, p.critical_pressure);
        if (!(p.acentric_factor > -0.5 && p.acentric_factor < 2.0)) p.acentric_factor = 0.2;
        mark(p, "acentric factor");
    }
    if (!(p.triple_temperature > 0)) {
        p.triple_temperature = p.melting_point;
        mark(p, "triple point temperature");
    }
    if (p.triple_temperature >= p.critical_temperature) p.triple_temperature = 0.6 * p.critical_temperature;
    if (!(p.liquid_density > 0) && p.critical_temperature > 0) {
        const CriticalConstants c{ p.critical_temperature, p.critical_pressure, p.critical_volume, value_or(p.acentric_factor, 0.0) };
        p.liquid_density = (p.molar_mass / 1000.0) / rackett_volume(c, p.triple_temperature);
        mark(p, "liquid density");
    }
    if (!(p.solid_density > 0)) {
        p.solid_density = p.liquid_density * (metal_like ? 1.04 : 1.10);
        mark(p, "solid density");
    }
    if (!(p.triple_pressure > 0) && p.critical_temperature > 0) {
        const CriticalConstants c{ p.critical_temperature, p.critical_pressure, p.critical_volume, value_or(p.acentric_factor, 0.0) };
        p.triple_pressure = lee_kesler_vapor_pressure(c, p.triple_temperature);
        mark(p, "triple point pressure");
    }
    if (!(p.fusion_enthalpy > 0) && p.melting_point > 0) {
        p.fusion_enthalpy = (metal_like ? 9.6 : (ionic_like ? 26.0 : 45.0)) * p.melting_point;
        mark(p, "enthalpy of fusion");
    }
    if (metal_like) p.solid_bulk_modulus = 1e11;
    else if (ionic_like) p.solid_bulk_modulus = 3e10;
}

PhaseProperties phase_properties(const Substance& s) {
    PhaseProperties p;
    p.name = s.name;
    p.formula = s.formula;
    p.source = "substance data";
    p.substance = &s;
    p.molar_mass = s.molar_mass;
    p.critical_temperature = s.critical_temperature;
    p.critical_pressure = s.critical_pressure;
    p.critical_volume = s.critical_volume;
    p.acentric_factor = s.acentric_factor;
    p.boiling_point = s.boiling_point;
    p.melting_point = s.melting_point;
    p.triple_temperature = s.triple_point_temperature;
    p.triple_pressure = s.triple_point_pressure;
    p.vaporization_enthalpy = s.vaporization_enthalpy;
    p.fusion_enthalpy = s.fusion_enthalpy;
    if (s.has_critical_point()) {
        const double v = s.rackett_liquid_volume(value_or(s.triple_point_temperature, value_or(s.melting_point, 0.6 * s.critical_temperature)));
        if (v > 0) p.liquid_density = s.molar_mass / 1000.0 / v;
    }
    const std::string f = s.formula;
    if (f == "H2O") {
        p.liquid_density = 999.8;
        p.solid_density = 916.7;
    } else if (f == "Ga") {
        p.liquid_density = 6095.0;
        p.solid_density = 5910.0;
    } else if (f == "Bi") {
        p.liquid_density = 10050.0;
        p.solid_density = 9780.0;
    } else if (f == "Si") {
        p.liquid_density = 2570.0;
        p.solid_density = 2330.0;
    } else if (f == "Ge") {
        p.liquid_density = 5600.0;
        p.solid_density = 5323.0;
    }
    if (s.standard_state == Phase::Solid && !s.has_critical_point()) {
        p.source = "substance data (non-molecular solid)";
        p.liquid_density = NO_VALUE;
        if (const Element* e = PeriodicTable::find_symbol(f)) {
            if (e->density == e->density) p.solid_density = e->density * 1000.0;
            if (e->is_metal()) p.source = "substance data (metal)";
        } else {
            const CompoundName nm = name_compound(f);
            if (nm.ok && (nm.kind == CompoundKind::Ionic || nm.kind == CompoundKind::Base) && nm.ions.size() >= 2) {
                const IonicCrystal ic = ionic_crystal(nm.ions.front(), nm.ions.back());
                if (ic.density > 0) p.solid_density = ic.density * 1000.0;
                p.source = "substance data (ionic solid)";
            } else {
                p.solid_density = 1500.0;
                mark(p, "solid density");
            }
        }
        if (f == "Si") p.liquid_density = 2570.0;
        if (f == "Ge") p.liquid_density = 5600.0;
        if (f == "Ga") p.liquid_density = 6095.0;
        if (f == "Bi") p.liquid_density = 10050.0;
    }
    complete_phase_properties(p);
    return p;
}

PhaseProperties phase_properties(const Element& e) {
    PhaseProperties p;
    p.name = e.name;
    p.formula = e.symbol;
    p.molar_mass = e.atomic_weight;
    p.source = e.is_metal() ? "element data (metal)" : "element data";
    p.critical_temperature = e.critical_temperature;
    p.critical_pressure = e.critical_pressure;
    p.boiling_point = e.boiling_point;
    p.melting_point = e.melting_point;
    p.triple_temperature = e.triple_point_temperature;
    p.triple_pressure = e.triple_point_pressure;
    if (e.fusion_heat == e.fusion_heat) p.fusion_enthalpy = e.fusion_heat * 1000.0;
    if (e.vaporization_heat == e.vaporization_heat) p.vaporization_enthalpy = e.vaporization_heat * 1000.0;
    if (e.density == e.density) {
        if (e.standard_state == Phase::Solid) p.solid_density = e.density * 1000.0;
        else if (e.standard_state == Phase::Liquid) p.liquid_density = e.density * 1000.0;
    }
    if (e.atomic_number == 31) {
        p.liquid_density = 6095.0;
        p.solid_density = 5910.0;
    } else if (e.atomic_number == 83) {
        p.liquid_density = 10050.0;
        p.solid_density = 9780.0;
    } else if (e.atomic_number == 14) {
        p.liquid_density = 2570.0;
    } else if (e.atomic_number == 32) {
        p.liquid_density = 5600.0;
    }
    if (e.is_diatomic()) {
        p.formula = std::string(e.symbol) + "2";
        p.molar_mass *= 2.0;
        if (const Substance* s = find_substance(p.formula)) {
            if (s->has_critical_point()) return phase_properties(*s);
        }
    }
    if (const Substance* s = find_substance(e.symbol)) {
        if (s->has_critical_point()) {
            PhaseProperties q = phase_properties(*s);
            q.name = e.name;
            return q;
        }
    }
    complete_phase_properties(p);
    return p;
}

PhaseProperties phase_properties(const Molecule& molecule, std::string_view name) {
    const Formula f = molecule.formula();
    const std::vector<const Substance*> matches = find_substances(f);
    if (matches.size() == 1 && matches.front()->has_critical_point()) {
        PhaseProperties p = phase_properties(*matches.front());
        if (!name.empty()) p.name = std::string(name);
        return p;
    }
    PhaseProperties p;
    p.formula = f.hill();
    p.name = name.empty() ? name_compound(f.hill()).name : std::string(name);
    if (p.name.empty()) p.name = p.formula;
    p.molar_mass = f.molar_mass();
    const JobackEstimate j = joback(molecule);
    p.source = "Joback group contribution";
    if (j.groups.empty()) {
        p.source = "rough estimate";
        p.boiling_point = 100.0 + 25.0 * std::sqrt(p.molar_mass);
        mark(p, "boiling point");
    } else {
        p.boiling_point = j.boiling_point;
        p.melting_point = j.melting_point;
        p.critical_temperature = j.critical_temperature;
        p.critical_pressure = j.critical_pressure;
        p.critical_volume = j.critical_volume;
        p.acentric_factor = j.acentric_factor;
        p.vaporization_enthalpy = j.vaporization_enthalpy;
        if (j.fusion_enthalpy > 0) p.fusion_enthalpy = j.fusion_enthalpy;
        for (const char* what : { "boiling point", "melting point", "critical temperature", "critical pressure", "critical volume", "acentric factor" }) mark(p, what);
        if (!j.unmatched.empty()) p.source += " (some atoms had no group)";
    }
    if (matches.size() > 1) p.estimated.push_back("several known substances share this formula; built structure used instead");
    complete_phase_properties(p);
    return p;
}

PhaseProperties phase_properties(const IonicCrystal& crystal) {
    PhaseProperties p;
    p.name = crystal.name;
    p.formula = crystal.formula;
    p.molar_mass = crystal.molar_mass;
    p.source = "ionic estimate";
    p.solid_density = crystal.density * 1000.0;
    p.melting_point = crystal.melting_point;
    p.boiling_point = crystal.boiling_point;
    if (const Substance* s = find_substance(crystal.formula)) {
        if (s->has_critical_point()) return phase_properties(*s);
        if (!(p.melting_point > 0)) p.melting_point = s->melting_point;
        if (!(p.boiling_point > 0)) p.boiling_point = s->boiling_point;
        if (s->fusion_enthalpy == s->fusion_enthalpy) p.fusion_enthalpy = s->fusion_enthalpy;
    }
    if (!(p.melting_point > 0)) {
        const double u = crystal.lattice_energy();
        p.melting_point = u > 0 ? 1074.0 * std::pow(u / 787.0, 0.6) : 1000.0;
        mark(p, "melting point");
    }
    if (!(p.boiling_point > 0)) {
        p.boiling_point = 1.6 * p.melting_point;
        mark(p, "boiling point");
    }
    complete_phase_properties(p);
    return p;
}

PhaseProperties phase_properties(const MetallicCrystal& crystal) {
    if (crystal.components.size() == 1 && crystal.components.front().element) return phase_properties(*crystal.components.front().element);
    PhaseProperties p;
    p.name = crystal.name;
    p.formula = crystal.formula;
    p.molar_mass = crystal.molar_mass;
    p.source = "metal alloy estimate";
    p.melting_point = crystal.melting_point;
    p.boiling_point = crystal.boiling_point;
    p.solid_density = crystal.density * 1000.0;
    double tc = 0.0, fus = 0.0;
    bool have_tc = true;
    for (const AlloyComponent& c : crystal.components) {
        const double t = c.element->critical_temperature;
        if (t == t) tc += c.fraction * t;
        else have_tc = false;
        fus += c.fraction * value_or(c.element->fusion_heat, 9.6e-3 * value_or(c.element->melting_point, 1000.0)) * 1000.0;
    }
    if (have_tc) p.critical_temperature = tc;
    p.fusion_enthalpy = fus;
    mark(p, "melting point (weighted average)");
    complete_phase_properties(p);
    return p;
}

PhaseProperties find_phase_properties(std::string_view name_or_formula) {
    const std::string_view text = trim(name_or_formula);
    if (const Substance* s = find_substance(text)) return phase_properties(*s);
    if (const Element* e = PeriodicTable::find(text)) return phase_properties(*e);
    const CompoundName byname = formula_from_name(text);
    if (byname.ok) {
        if (const Substance* s = find_substance(byname.formula)) return phase_properties(*s);
        for (const CommonCompoundEntry* cc : find_common_compounds(byname.name)) {
            if (*cc->smiles) {
                try {
                    PhaseProperties p = phase_properties(Molecule::from_smiles(cc->smiles), cc->name);
                    return p;
                } catch (...) {
                }
            }
        }
    }
    try {
        const Formula f = Formula::parse(text);
        const std::vector<const Substance*> matches = find_substances(f);
        if (!matches.empty()) return phase_properties(*matches.front());
        for (const CommonCompoundEntry* cc : find_common_compounds(text)) {
            if (*cc->smiles) return phase_properties(Molecule::from_smiles(cc->smiles), cc->name);
        }
        const CompoundName nm = name_compound(text);
        if (nm.ok && (nm.kind == CompoundKind::Ionic || nm.kind == CompoundKind::Base) && nm.ions.size() >= 2) {
            return phase_properties(ionic_crystal(nm.ions.front(), nm.ions.back()));
        }
    } catch (...) {
    }
    try {
        return phase_properties(Molecule::from_smiles(text));
    } catch (...) {
    }
    PhaseProperties p;
    p.name = std::string(text);
    return p;
}

double PhaseDiagram::vapor_pressure(double t) const {
    const VaporCurve v = vapor_curve(properties);
    return std::exp(v.ln_p(t));
}

double PhaseDiagram::sublimation_pressure(double t) const {
    const VaporCurve v = vapor_curve(properties);
    const double tt = properties.triple_temperature;
    const double hvap = -GAS_CONSTANT * v.dlnp_dinv_t(tt);
    const double hsub = std::max(hvap, 1.0) + std::max(0.0, value_or(properties.fusion_enthalpy, 0.0));
    return properties.triple_pressure * std::exp(-hsub / GAS_CONSTANT * (1.0 / t - 1.0 / tt));
}

double PhaseDiagram::melting_pressure(double t) const {
    const double dv = properties.fusion_volume_change();
    const double tt = properties.triple_temperature;
    const double h = value_or(properties.fusion_enthalpy, NO_VALUE);
    if (!(h > 0) || !(dv == dv) || dv == 0.0) return NO_VALUE;
    return properties.triple_pressure + h / dv * std::log(t / tt);
}

double PhaseDiagram::melting_temperature(double p) const {
    const double dv = properties.fusion_volume_change();
    const double h = value_or(properties.fusion_enthalpy, NO_VALUE);
    if (!(h > 0) || !(dv == dv) || dv == 0.0) return properties.triple_temperature;
    return properties.triple_temperature * std::exp((p - properties.triple_pressure) * dv / h);
}

double PhaseDiagram::boiling_temperature(double p) const {
    double lo = properties.triple_temperature, hi = properties.critical_temperature;
    if (p <= vapor_pressure(lo)) return lo;
    if (p >= properties.critical_pressure) return hi;
    for (int i = 0; i < 100; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (vapor_pressure(mid) < p) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

double PhaseDiagram::sublimation_temperature(double p) const {
    const VaporCurve v = vapor_curve(properties);
    const double tt = properties.triple_temperature;
    const double hvap = -GAS_CONSTANT * v.dlnp_dinv_t(tt);
    const double hsub = std::max(hvap, 1.0) + std::max(0.0, value_or(properties.fusion_enthalpy, 0.0));
    return 1.0 / (1.0 / tt - GAS_CONSTANT / hsub * std::log(p / properties.triple_pressure));
}

Phase PhaseDiagram::phase_at(double t, double p) const {
    if (!ok) return Phase::Unknown;
    const double tc = properties.critical_temperature, pc = properties.critical_pressure;
    if (p < properties.triple_pressure) return t < sublimation_temperature(p) ? Phase::Solid : Phase::Gas;
    const double tm = melting_temperature(p);
    if (t < tm) return Phase::Solid;
    if (t >= tc && p >= pc) return Phase::Supercritical;
    if (p >= pc) return Phase::Liquid;
    if (t >= tc) return Phase::Gas;
    return t < boiling_temperature(p) ? Phase::Liquid : Phase::Gas;
}

std::string PhaseDiagram::describe(double t, double p) const {
    const Phase ph = phase_at(t, p);
    std::string out = to_string(ph);
    if (ph == Phase::Liquid && p < properties.critical_pressure) out += "; boils at " + format_number(boiling_temperature(p), 4) + " K at this pressure";
    if (ph == Phase::Solid) {
        if (p >= properties.triple_pressure) out += "; melts at " + format_number(melting_temperature(p), 4) + " K at this pressure";
        else out += "; sublimes at " + format_number(sublimation_temperature(p), 4) + " K at this pressure";
    }
    if (ph == Phase::Gas && t < properties.critical_temperature && t > properties.triple_temperature) out += "; condenses above " + format_number(vapor_pressure(t), 4) + " Pa at this temperature";
    return out;
}

PhaseDiagram phase_diagram(const PhaseProperties& props, int points) {
    PhaseDiagram d;
    d.properties = props;
    if (!props.usable()) {
        d.notes.push_back("not enough data for a phase diagram");
        return d;
    }
    d.ok = true;
    const double tt = props.triple_temperature, tc = props.critical_temperature;
    d.triple = { tt, props.triple_pressure };
    d.critical = { tc, props.critical_pressure };
    d.t_min = std::max(1.0, 0.7 * tt);
    d.t_max = 1.25 * tc;
    d.p_max = 30.0 * props.critical_pressure;
    points = std::max(20, points);
    for (int i = 0; i <= points; ++i) {
        const double t = tt + (tc - tt) * i / points;
        d.vaporization.push_back({ t, d.vapor_pressure(t) });
    }
    for (int i = 0; i <= points; ++i) {
        const double t = d.t_min + (tt - d.t_min) * i / points;
        d.sublimation.push_back({ t, d.sublimation_pressure(t) });
    }
    d.p_min = std::max(1e-6, std::min(d.sublimation.front().p, props.triple_pressure * 1e-3));
    const double dv = props.fusion_volume_change();
    if (dv == dv && dv != 0.0 && props.fusion_enthalpy > 0) {
        const double t_end = d.melting_temperature(d.p_max);
        for (int i = 0; i <= points; ++i) {
            const double t = tt + (t_end - tt) * i / points;
            const double p = d.melting_pressure(t);
            if (p >= props.triple_pressure && p <= d.p_max * 1.0001) d.fusion.push_back({ t, p });
        }
        if (dv < 0) d.notes.push_back("the solid is less dense than the liquid, so the melting line leans left (more pressure melts it)");
    }
    if (props.triple_pressure < ATMOSPHERE && ATMOSPHERE < props.critical_pressure) d.normal_boiling = { d.boiling_temperature(ATMOSPHERE), ATMOSPHERE };
    if (props.triple_pressure < ATMOSPHERE) d.normal_melting = { d.melting_temperature(ATMOSPHERE), ATMOSPHERE };
    else d.notes.push_back("the triple point is above 1 atm, so at normal pressure it sublimes instead of melting");
    if (!props.estimated.empty()) {
        std::string list;
        for (const std::string& e : props.estimated) list += (list.empty() ? "" : ", ") + e;
        d.notes.push_back("estimated: " + list);
    }
    return d;
}

PvtSurface pvt_surface(const PhaseDiagram& d, int nt, int nv) {
    PvtSurface s;
    if (!d.ok) return s;
    const PhaseProperties& p = d.properties;
    const double m = p.molar_mass / 1000.0;
    const double tt = p.triple_temperature, tc = p.critical_temperature, pc = p.critical_pressure;
    const PengRobinson pr(tc, pc, value_or(p.acentric_factor, 0.0));
    const double dv_frac = p.liquid_density > 0 && p.solid_density > 0 ? 1.0 - p.liquid_density / p.solid_density : 0.08;
    const double K = p.solid_bulk_modulus;
    const auto liquid_volume = [&](double t) {
        const auto sat = pr.saturation_volumes(t);
        if (sat.first > 0 && sat.first == sat.first) return sat.first;
        return pr.minimum_volume(t) * 1.3;
    };
    const double v_l_tt = liquid_volume(tt);
    const double v_s_tt = v_l_tt * (1.0 - dv_frac);
    const double p_low = std::max(d.p_min, p.triple_pressure * 0.02);
    const double t0 = std::max(1.0, 0.85 * tt), t1 = 1.3 * tc;
    const double v_min = std::min(v_s_tt, v_l_tt) * 0.94;
    const double v_max = GAS_CONSTANT * t1 / p_low;
    s.nt = std::max(8, nt);
    s.nv = std::max(8, nv);
    s.temperatures.resize(s.nt);
    s.specific_volumes.resize(s.nv);
    s.pressures.resize(static_cast<std::size_t>(s.nt) * s.nv);
    s.regions.resize(s.pressures.size());
    for (int i = 0; i < s.nt; ++i) s.temperatures[i] = t0 + (t1 - t0) * i / (s.nt - 1);
    for (int j = 0; j < s.nv; ++j) s.specific_volumes[j] = std::exp(std::log(v_min) + (std::log(v_max) - std::log(v_min)) * j / (s.nv - 1)) / m;
    for (int i = 0; i < s.nt; ++i) {
        const double t = s.temperatures[i];
        double v_l = NO_VALUE, v_v = NO_VALUE, psat = NO_VALUE;
        if (t >= tt && t < tc) {
            psat = d.vapor_pressure(t);
            const auto sat = pr.saturation_volumes(t);
            v_l = sat.first > 0 ? sat.first : liquid_volume(t);
            v_v = sat.second > 0 ? sat.second : GAS_CONSTANT * t / psat;
            if (!(v_v > v_l)) v_v = GAS_CONSTANT * t / psat;
        }
        const double pfus = t >= tt ? d.melting_pressure(t) : NO_VALUE;
        double v_lf = NO_VALUE, v_sf = NO_VALUE;
        if (pfus == pfus && pfus > 0) {
            v_lf = pr.molar_volume(t, pfus, Root::Liquid);
            if (!(v_lf > 0)) v_lf = v_l_tt;
            v_sf = v_lf * (1.0 - dv_frac);
        }
        for (int j = 0; j < s.nv; ++j) {
            const double v = s.specific_volumes[j] * m;
            double pres = NO_VALUE;
            PvtRegion region = PvtRegion::Gas;
            if (t < tt) {
                const double psub = d.sublimation_pressure(t);
                const double v_s = v_s_tt * (1.0 + 1e-4 * (t - tt));
                const double v_g = GAS_CONSTANT * t / psub;
                if (v < v_s) {
                    pres = psub + K * (v_s - v) / v_s;
                    region = PvtRegion::Solid;
                } else if (v < v_g) {
                    pres = psub;
                    region = PvtRegion::SolidVapor;
                } else {
                    pres = GAS_CONSTANT * t / v;
                    region = PvtRegion::Gas;
                }
            } else {
                bool done = false;
                if (v_lf == v_lf) {
                    const double lo = std::min(v_sf, v_lf), hi = std::max(v_sf, v_lf);
                    if (v < lo) {
                        pres = pfus + K * (lo - v) / lo;
                        region = PvtRegion::Solid;
                        done = true;
                    } else if (v <= hi) {
                        pres = pfus;
                        region = PvtRegion::SolidLiquid;
                        done = true;
                    }
                }
                if (!done) {
                    if (t < tc && v > v_l && v < v_v) {
                        pres = psat;
                        region = PvtRegion::LiquidVapor;
                    } else {
                        const double vmin = pr.minimum_volume(t);
                        pres = v > vmin * 1.0001 ? pr.pressure(t, v) : NO_VALUE;
                        if (!(pres > 0)) pres = GAS_CONSTANT * t / v;
                        if (t < tc) {
                            region = v <= v_l ? PvtRegion::Liquid : PvtRegion::Gas;
                            if (region == PvtRegion::Liquid) pres = std::max(pres, psat);
                            else pres = std::min(pres, psat);
                        } else {
                            region = pres >= pc ? PvtRegion::Supercritical : PvtRegion::Gas;
                        }
                        if (pfus == pfus && pfus > 0 && pres > pfus) pres = pfus;
                    }
                }
            }
            const std::size_t idx = static_cast<std::size_t>(i) * s.nv + j;
            s.pressures[idx] = std::max(pres, d.p_min);
            s.regions[idx] = region;
        }
    }
    for (int i = 0; i <= 60; ++i) {
        const double t = tt + (tc - tt) * i / 60.0 * 0.999;
        const auto sat = pr.saturation_volumes(t);
        const double psat = d.vapor_pressure(t);
        const double vl = sat.first > 0 ? sat.first : liquid_volume(t);
        double vv = sat.second > 0 ? sat.second : GAS_CONSTANT * t / psat;
        if (!(vv > vl)) vv = GAS_CONSTANT * t / psat;
        s.dome_t.push_back(t);
        s.dome_liquid_v.push_back(vl / m);
        s.dome_vapor_v.push_back(vv / m);
        s.dome_p.push_back(psat);
    }
    s.triple_t = tt;
    s.triple_p = p.triple_pressure;
    s.triple_solid_v = v_s_tt / m;
    s.triple_liquid_v = v_l_tt / m;
    s.triple_vapor_v = GAS_CONSTANT * tt / p.triple_pressure / m;
    s.critical_t = tc;
    s.critical_p = pc;
    const CriticalConstants cc = pr.critical_point();
    s.critical_v = (cc.volume > 0 ? cc.volume : p.critical_volume) / m;
    s.ok = true;
    return s;
}

} // namespace chemistry
} // namespace fizmo
