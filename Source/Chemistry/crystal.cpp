#include "crystal.hpp"

#include "bonds.hpp"
#include "substance.hpp"
#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace fizmo {
namespace chemistry {

namespace {

constexpr double SQRT2 = 1.4142135623730951;
constexpr double SQRT3 = 1.7320508075688772;
constexpr double O2_SECOND_ELECTRON_AFFINITY = -798.0;

struct StructureInfo {
    CrystalStructure s;
    const char* name;
    const char* short_name;
};

const StructureInfo STRUCTURES[] = {
    { CrystalStructure::SimpleCubic, "simple cubic", "SC" },
    { CrystalStructure::BodyCenteredCubic, "body-centered cubic", "BCC" },
    { CrystalStructure::FaceCenteredCubic, "face-centered cubic", "FCC" },
    { CrystalStructure::HexagonalClosePacked, "hexagonal close-packed", "HCP" },
    { CrystalStructure::Diamond, "diamond cubic", "diamond" },
    { CrystalStructure::RockSalt, "rock salt", "NaCl" },
    { CrystalStructure::CesiumChloride, "cesium chloride", "CsCl" },
    { CrystalStructure::ZincBlende, "zinc blende", "ZnS (cubic)" },
    { CrystalStructure::Wurtzite, "wurtzite", "ZnS (hexagonal)" },
    { CrystalStructure::Fluorite, "fluorite", "CaF2" },
    { CrystalStructure::Antifluorite, "antifluorite", "Na2O" },
    { CrystalStructure::Rutile, "rutile", "TiO2" },
    { CrystalStructure::Corundum, "corundum", "Al2O3" },
    { CrystalStructure::NickelArsenide, "nickel arsenide", "NiAs" },
    { CrystalStructure::CadmiumIodide, "cadmium iodide", "CdI2" },
    { CrystalStructure::Cuprite, "cuprite", "Cu2O" },
    { CrystalStructure::BetaCristobalite, "beta-cristobalite", "SiO2" },
    { CrystalStructure::RheniumTrioxide, "rhenium trioxide", "ReO3" },
    { CrystalStructure::AntiRheniumTrioxide, "anti-rhenium trioxide", "Cu3N" },
    { CrystalStructure::AntiBixbyite, "anti-bixbyite", "Mg3N2" },
    { CrystalStructure::Perovskite, "perovskite", "CaTiO3" },
    { CrystalStructure::Unknown, "unknown", "?" },
};

struct ThermoRadius {
    const char* formula;
    int charge;
    double radius;
};

const ThermoRadius THERMO_RADII[] = {
    { "NH4", 1, 151.0 }, { "H3O", 1, 100.0 }, { "PH4", 1, 157.0 }, { "N(CH3)4", 1, 234.0 }, { "CH3NH3", 1, 180.0 }, { "Hg2", 2, 119.0 },
    { "OH", -1, 133.0 }, { "CN", -1, 191.0 }, { "NO3", -1, 179.0 }, { "NO2", -1, 192.0 }, { "ClO4", -1, 240.0 }, { "ClO3", -1, 171.0 },
    { "BrO3", -1, 154.0 }, { "IO3", -1, 122.0 }, { "IO4", -1, 231.0 }, { "MnO4", -1, 240.0 }, { "SCN", -1, 213.0 }, { "N3", -1, 195.0 },
    { "HCO3", -1, 156.0 }, { "HSO4", -1, 221.0 }, { "HS", -1, 207.0 }, { "BF4", -1, 232.0 }, { "CH3COO", -1, 162.0 }, { "HCOO", -1, 158.0 },
    { "ClO", -1, 151.0 }, { "ClO2", -1, 165.0 }, { "BH4", -1, 193.0 }, { "AlH4", -1, 226.0 }, { "PF6", -1, 242.0 }, { "H2PO4", -1, 213.0 },
    { "OCN", -1, 203.0 }, { "O2", -1, 158.0 }, { "O3", -1, 177.0 }, { "AuCl4", -1, 288.0 },
    { "CO3", -2, 178.0 }, { "SO4", -2, 258.0 }, { "SO3", -2, 200.0 }, { "CrO4", -2, 256.0 }, { "MoO4", -2, 254.0 }, { "WO4", -2, 257.0 },
    { "SeO4", -2, 249.0 }, { "C2O4", -2, 202.0 }, { "O2", -2, 173.0 }, { "S2O3", -2, 251.0 }, { "SiF6", -2, 248.0 }, { "HPO4", -2, 200.0 },
    { "Cr2O7", -2, 292.0 }, { "PtCl6", -2, 299.0 }, { "SiO3", -2, 201.0 }, { "S2", -2, 192.0 }, { "C2", -2, 200.0 }, { "TiO3", -2, 205.0 },
    { "PO4", -3, 238.0 }, { "AsO4", -3, 248.0 }, { "BO3", -3, 191.0 }, { "Fe(CN)6", -3, 439.0 }, { "Fe(CN)6", -4, 442.0 }, { "SiO4", -4, 259.0 },
};

struct StructureOverride {
    const char* formula;
    CrystalStructure structure;
};

const StructureOverride OVERRIDES[] = {
    { "CsCl", CrystalStructure::CesiumChloride }, { "CsBr", CrystalStructure::CesiumChloride }, { "CsI", CrystalStructure::CesiumChloride },
    { "TlCl", CrystalStructure::CesiumChloride }, { "TlBr", CrystalStructure::CesiumChloride }, { "NH4Cl", CrystalStructure::CesiumChloride },
    { "ZnS", CrystalStructure::ZincBlende }, { "CuCl", CrystalStructure::ZincBlende }, { "CuBr", CrystalStructure::ZincBlende },
    { "CuI", CrystalStructure::ZincBlende }, { "AgI", CrystalStructure::ZincBlende }, { "BeS", CrystalStructure::ZincBlende },
    { "GaAs", CrystalStructure::ZincBlende }, { "GaP", CrystalStructure::ZincBlende }, { "InSb", CrystalStructure::ZincBlende },
    { "AlP", CrystalStructure::ZincBlende }, { "CdTe", CrystalStructure::ZincBlende }, { "HgS", CrystalStructure::ZincBlende },
    { "ZnSe", CrystalStructure::ZincBlende }, { "ZnTe", CrystalStructure::ZincBlende },
    { "ZnO", CrystalStructure::Wurtzite }, { "BeO", CrystalStructure::Wurtzite }, { "AlN", CrystalStructure::Wurtzite },
    { "GaN", CrystalStructure::Wurtzite }, { "CdS", CrystalStructure::Wurtzite }, { "CdSe", CrystalStructure::Wurtzite }, { "InN", CrystalStructure::Wurtzite },
    { "CaF2", CrystalStructure::Fluorite }, { "SrF2", CrystalStructure::Fluorite }, { "BaF2", CrystalStructure::Fluorite },
    { "CdF2", CrystalStructure::Fluorite }, { "PbF2", CrystalStructure::Fluorite }, { "ThO2", CrystalStructure::Fluorite },
    { "UO2", CrystalStructure::Fluorite }, { "ZrO2", CrystalStructure::Fluorite }, { "CeO2", CrystalStructure::Fluorite }, { "HgF2", CrystalStructure::Fluorite },
    { "SrCl2", CrystalStructure::Fluorite }, { "BaCl2", CrystalStructure::Fluorite },
    { "TiO2", CrystalStructure::Rutile }, { "SnO2", CrystalStructure::Rutile }, { "MnO2", CrystalStructure::Rutile }, { "PbO2", CrystalStructure::Rutile },
    { "MgF2", CrystalStructure::Rutile }, { "ZnF2", CrystalStructure::Rutile }, { "FeF2", CrystalStructure::Rutile }, { "NiF2", CrystalStructure::Rutile },
    { "CoF2", CrystalStructure::Rutile }, { "CrO2", CrystalStructure::Rutile }, { "VO2", CrystalStructure::Rutile }, { "RuO2", CrystalStructure::Rutile },
    { "GeO2", CrystalStructure::Rutile }, { "IrO2", CrystalStructure::Rutile }, { "MnF2", CrystalStructure::Rutile },
    { "Al2O3", CrystalStructure::Corundum }, { "Fe2O3", CrystalStructure::Corundum }, { "Cr2O3", CrystalStructure::Corundum },
    { "Ti2O3", CrystalStructure::Corundum }, { "V2O3", CrystalStructure::Corundum }, { "Ga2O3", CrystalStructure::Corundum },
    { "Li2O", CrystalStructure::Antifluorite }, { "Na2O", CrystalStructure::Antifluorite }, { "K2O", CrystalStructure::Antifluorite },
    { "Rb2O", CrystalStructure::Antifluorite }, { "Li2S", CrystalStructure::Antifluorite }, { "Na2S", CrystalStructure::Antifluorite },
    { "K2S", CrystalStructure::Antifluorite }, { "Na2Se", CrystalStructure::Antifluorite }, { "Mg2Si", CrystalStructure::Antifluorite },
    { "Cu2O", CrystalStructure::Cuprite }, { "Ag2O", CrystalStructure::Cuprite },
    { "NiAs", CrystalStructure::NickelArsenide }, { "FeS", CrystalStructure::NickelArsenide }, { "CoS", CrystalStructure::NickelArsenide },
    { "NiS", CrystalStructure::NickelArsenide }, { "CrSb", CrystalStructure::NickelArsenide }, { "MnTe", CrystalStructure::NickelArsenide },
    { "FeSe", CrystalStructure::NickelArsenide }, { "CoTe", CrystalStructure::NickelArsenide },
    { "CdI2", CrystalStructure::CadmiumIodide }, { "PbI2", CrystalStructure::CadmiumIodide }, { "MgI2", CrystalStructure::CadmiumIodide },
    { "CaI2", CrystalStructure::CadmiumIodide }, { "FeBr2", CrystalStructure::CadmiumIodide }, { "TiS2", CrystalStructure::CadmiumIodide },
    { "Mg(OH)2", CrystalStructure::CadmiumIodide }, { "Ca(OH)2", CrystalStructure::CadmiumIodide }, { "SnS2", CrystalStructure::CadmiumIodide },
    { "SiO2", CrystalStructure::BetaCristobalite }, { "BeF2", CrystalStructure::BetaCristobalite },
    { "AlF3", CrystalStructure::RheniumTrioxide }, { "ScF3", CrystalStructure::RheniumTrioxide }, { "WO3", CrystalStructure::RheniumTrioxide },
    { "Cu3N", CrystalStructure::AntiRheniumTrioxide }, { "Li3N", CrystalStructure::AntiRheniumTrioxide },
    { "Mg3N2", CrystalStructure::AntiBixbyite }, { "Ca3N2", CrystalStructure::AntiBixbyite }, { "Be3N2", CrystalStructure::AntiBixbyite },
    { "Zn3P2", CrystalStructure::AntiBixbyite }, { "Ca3P2", CrystalStructure::AntiBixbyite },
};

struct AxialRatio {
    int z;
    double ratio;
};

const AxialRatio HCP_RATIOS[] = {
    { 4, 1.568 }, { 12, 1.624 }, { 21, 1.594 }, { 22, 1.587 }, { 27, 1.623 }, { 30, 1.856 }, { 39, 1.571 }, { 40, 1.593 },
    { 43, 1.604 }, { 44, 1.582 }, { 48, 1.886 }, { 64, 1.590 }, { 72, 1.581 }, { 75, 1.615 }, { 76, 1.579 }, { 81, 1.598 },
};

struct AlloyName {
    int host;
    int solute;
    double min_fraction;
    double max_fraction;
    const char* name;
    const char* note;
};

const AlloyName ALLOYS[] = {
    { 29, 30, 0.05, 0.45, "brass", "copper-zinc; harder than copper, used for fittings and instruments" },
    { 29, 50, 0.02, 0.35, "bronze", "copper-tin; the alloy of the Bronze Age" },
    { 29, 28, 0.02, 0.45, "cupronickel", "copper-nickel; used in coins" },
    { 28, 29, 0.25, 0.45, "Monel", "nickel-copper; resists seawater" },
    { 29, 13, 0.03, 0.15, "aluminium bronze", "copper-aluminium; strong and corrosion resistant" },
    { 29, 4, 0.005, 0.1, "beryllium copper", "springy, non-sparking tools" },
    { 26, 6, 0.0002, 0.095, "steel", "iron with a little carbon dissolved between the iron atoms" },
    { 26, 6, 0.095, 0.25, "cast iron", "more than about 2 percent carbon by mass; brittle but easy to cast" },
    { 26, 24, 0.10, 0.35, "stainless steel", "at least about 10.5 percent chromium forms a protective oxide film" },
    { 26, 28, 0.30, 0.40, "Invar", "iron-nickel that barely expands when heated" },
    { 26, 25, 0.08, 0.16, "manganese steel", "Hadfield steel; work-hardens under impact" },
    { 26, 14, 0.01, 0.07, "electrical steel", "silicon steel for transformer cores" },
    { 26, 74, 0.05, 0.25, "tungsten steel", "high-speed tool steel" },
    { 79, 47, 0.20, 0.80, "electrum", "natural gold-silver alloy" },
    { 79, 29, 0.10, 0.45, "rose gold", "gold-copper" },
    { 79, 28, 0.05, 0.30, "white gold", "gold whitened with nickel" },
    { 79, 46, 0.05, 0.30, "white gold", "gold whitened with palladium" },
    { 47, 29, 0.05, 0.15, "sterling silver", "92.5 percent silver by mass" },
    { 50, 82, 0.20, 0.55, "solder", "tin-lead; the classic electronics solder (63/37 is the eutectic)" },
    { 82, 50, 0.20, 0.50, "solder", "lead-tin" },
    { 50, 51, 0.03, 0.10, "pewter", "mostly tin, hardened with antimony and copper" },
    { 82, 51, 0.03, 0.20, "type metal", "hard lead for printing type and bullets" },
    { 28, 24, 0.10, 0.30, "nichrome", "nickel-chromium heating wire" },
    { 28, 22, 0.40, 0.55, "Nitinol", "nickel-titanium shape-memory alloy" },
    { 22, 28, 0.45, 0.60, "Nitinol", "nickel-titanium shape-memory alloy" },
    { 13, 29, 0.01, 0.06, "duralumin", "age-hardened aluminium for aircraft" },
    { 13, 12, 0.03, 0.55, "magnalium", "aluminium-magnesium" },
    { 13, 14, 0.04, 0.25, "silumin", "aluminium-silicon casting alloy" },
    { 13, 30, 0.02, 0.10, "7000-series aluminium", "zinc-strengthened aircraft aluminium" },
    { 22, 13, 0.05, 0.12, "titanium alloy (Ti-6Al-4V type)", "the most common titanium alloy" },
    { 27, 24, 0.20, 0.35, "Stellite", "cobalt-chromium; very wear resistant" },
    { 31, 49, 0.10, 0.30, "galinstan / EGaIn", "gallium alloys that are liquid at room temperature" },
    { 11, 19, 0.20, 0.80, "NaK", "sodium-potassium, liquid at room temperature" },
    { 19, 11, 0.20, 0.80, "NaK", "sodium-potassium, liquid at room temperature" },
    { 30, 13, 0.03, 0.10, "Zamak", "zinc die-casting alloy" },
    { 12, 13, 0.03, 0.12, "AZ magnesium alloy", "magnesium-aluminium-zinc" },
    { 78, 77, 0.05, 0.30, "platinum-iridium", "used for the old standard kilogram" },
    { 83, 82, 0.20, 0.50, "Wood's metal / Rose's metal family", "low-melting bismuth alloys" },
};

const char* coordination_shape_for(int cn) {
    switch (cn) {
        case 2: return "linear";
        case 3: return "trigonal planar";
        case 4: return "tetrahedral";
        case 6: return "octahedral";
        case 8: return "cubic";
        case 12: return "cuboctahedral";
        default: return "";
    }
}

double shannon_radius(const Element& e, int charge, int cn) {
    if (const IonicRadius* r = e.ionic_radius(charge, cn)) return r->ionic_radius;
    for (int alt : { 6, 4, 8, 5, 7, 9, 12, 3, 2 }) {
        if (const IonicRadius* r = e.ionic_radius(charge, alt)) return r->ionic_radius;
    }
    return NO_VALUE;
}

double ion_radius(const IonDescriptor& ion, int cn) {
    if (ion.polyatomic) return thermochemical_radius(ion);
    if (!ion.element) return NO_VALUE;
    double r = shannon_radius(*ion.element, ion.charge, cn);
    if (r == r) return r;
    if (ion.charge > 0) return value_or(ion.element->covalent_radius, 120.0) * 0.7;
    return value_or(ion.element->van_der_waals_radius, 180.0) * 0.85;
}

double structure_a_from_distance(CrystalStructure s, double d) {
    switch (s) {
        case CrystalStructure::RockSalt: return 2.0 * d;
        case CrystalStructure::CesiumChloride: return 2.0 * d / SQRT3;
        case CrystalStructure::ZincBlende: return 4.0 * d / SQRT3;
        case CrystalStructure::Wurtzite: return d / 0.6124;
        case CrystalStructure::Fluorite:
        case CrystalStructure::Antifluorite:
        case CrystalStructure::Cuprite:
        case CrystalStructure::AntiBixbyite: return 4.0 * d / SQRT3;
        case CrystalStructure::Rutile: return 2.33 * d;
        case CrystalStructure::Corundum: return 2.49 * d;
        case CrystalStructure::NickelArsenide: return 1.49 * d;
        case CrystalStructure::CadmiumIodide: return 1.42 * d;
        case CrystalStructure::BetaCristobalite: return 8.0 * d / SQRT3;
        case CrystalStructure::RheniumTrioxide:
        case CrystalStructure::AntiRheniumTrioxide: return 2.0 * d;
        default: return 2.0 * d;
    }
}

std::array<int, 2> coordination_of(CrystalStructure s) {
    switch (s) {
        case CrystalStructure::RockSalt: return { { 6, 6 } };
        case CrystalStructure::CesiumChloride: return { { 8, 8 } };
        case CrystalStructure::ZincBlende:
        case CrystalStructure::Wurtzite: return { { 4, 4 } };
        case CrystalStructure::Fluorite: return { { 8, 4 } };
        case CrystalStructure::Antifluorite: return { { 4, 8 } };
        case CrystalStructure::Rutile: return { { 6, 3 } };
        case CrystalStructure::Corundum: return { { 6, 4 } };
        case CrystalStructure::NickelArsenide: return { { 6, 6 } };
        case CrystalStructure::CadmiumIodide: return { { 6, 3 } };
        case CrystalStructure::Cuprite: return { { 2, 4 } };
        case CrystalStructure::BetaCristobalite: return { { 4, 2 } };
        case CrystalStructure::RheniumTrioxide: return { { 6, 2 } };
        case CrystalStructure::AntiRheniumTrioxide: return { { 2, 6 } };
        case CrystalStructure::AntiBixbyite: return { { 4, 6 } };
        case CrystalStructure::SimpleCubic: return { { 6, 0 } };
        case CrystalStructure::BodyCenteredCubic: return { { 8, 0 } };
        case CrystalStructure::FaceCenteredCubic:
        case CrystalStructure::HexagonalClosePacked: return { { 12, 0 } };
        case CrystalStructure::Diamond: return { { 4, 0 } };
        default: return { { 0, 0 } };
    }
}

void fcc(std::vector<LatticeSite>& out, int species, double ox = 0, double oy = 0, double oz = 0) {
    const double p[4][3] = { { 0, 0, 0 }, { 0.5, 0.5, 0 }, { 0.5, 0, 0.5 }, { 0, 0.5, 0.5 } };
    for (const auto& q : p) {
        double x = std::fmod(q[0] + ox, 1.0), y = std::fmod(q[1] + oy, 1.0), z = std::fmod(q[2] + oz, 1.0);
        if (x < 0) x += 1;
        if (y < 0) y += 1;
        if (z < 0) z += 1;
        out.push_back({ x, y, z, species });
    }
}

double wrap(double v) {
    v = std::fmod(v, 1.0);
    if (v < -1e-9) v += 1.0;
    if (v > 1.0 - 1e-9) v -= 1.0;
    if (std::fabs(v) < 1e-9) v = 0.0;
    return v;
}

std::string clean_formula(const std::string& f) {
    std::string out;
    for (char c : f) {
        if (c != ' ') out += c;
    }
    return out;
}

double metallic_atom_radius(const Element& e) {
    const double a = e.lattice_constant;
    if (a == a) {
        switch (crystal_structure_named(e.crystal_structure)) {
            case CrystalStructure::FaceCenteredCubic: return a * SQRT2 / 4.0;
            case CrystalStructure::BodyCenteredCubic: return a * SQRT3 / 4.0;
            case CrystalStructure::HexagonalClosePacked: return a / 2.0;
            case CrystalStructure::Diamond: return a * SQRT3 / 8.0;
            case CrystalStructure::SimpleCubic: return a / 2.0;
            default: break;
        }
    }
    if (e.metallic_radius == e.metallic_radius) return e.metallic_radius * 1.07;
    return value_or(e.atomic_radius, value_or(e.covalent_radius, 140.0));
}

} // namespace

const char* to_string(CrystalStructure s) noexcept {
    for (const StructureInfo& i : STRUCTURES) {
        if (i.s == s) return i.name;
    }
    return "unknown";
}

const char* short_name(CrystalStructure s) noexcept {
    for (const StructureInfo& i : STRUCTURES) {
        if (i.s == s) return i.short_name;
    }
    return "?";
}

CrystalStructure crystal_structure_named(std::string_view name) noexcept {
    const std::string_view n = trim(name);
    if (equals_ignore_case(n, "FCC") || equals_ignore_case(n, "CCP")) return CrystalStructure::FaceCenteredCubic;
    if (equals_ignore_case(n, "BCC")) return CrystalStructure::BodyCenteredCubic;
    if (equals_ignore_case(n, "HEX") || equals_ignore_case(n, "HCP")) return CrystalStructure::HexagonalClosePacked;
    if (equals_ignore_case(n, "DIA")) return CrystalStructure::Diamond;
    if (equals_ignore_case(n, "SC")) return CrystalStructure::SimpleCubic;
    for (const StructureInfo& i : STRUCTURES) {
        if (equals_ignore_case(n, i.name) || equals_ignore_case(n, i.short_name)) return i.s;
    }
    return CrystalStructure::Unknown;
}

std::array<double, 3> UnitCell::to_cartesian(double fx, double fy, double fz) const noexcept {
    if (hexagonal()) return { { a * (fx - 0.5 * fy), a * fy * SQRT3 / 2.0, c * fz } };
    return { { a * fx, b * fy, c * fz } };
}

double UnitCell::volume() const noexcept {
    if (hexagonal()) return a * a * c * SQRT3 / 2.0;
    return a * b * c;
}

UnitCell unit_cell(CrystalStructure structure, double a_pm, double c_pm) {
    UnitCell cell;
    cell.structure = structure;
    cell.a = cell.b = a_pm;
    cell.c = a_pm;
    cell.coordination = coordination_of(structure);
    cell.coordination_shape = { { coordination_shape_for(cell.coordination[0]), coordination_shape_for(cell.coordination[1]) } };
    auto& s = cell.sites;
    const auto hexa = [&](double ratio) {
        cell.gamma = 120.0;
        cell.c = c_pm == c_pm ? c_pm : ratio * a_pm;
    };
    switch (structure) {
        case CrystalStructure::SimpleCubic:
            s.push_back({ 0, 0, 0, 0 });
            cell.packing_fraction = PI / 6.0;
            break;
        case CrystalStructure::BodyCenteredCubic:
            s.push_back({ 0, 0, 0, 0 });
            s.push_back({ 0.5, 0.5, 0.5, 0 });
            cell.formula_units = 2;
            cell.packing_fraction = PI * SQRT3 / 8.0;
            break;
        case CrystalStructure::FaceCenteredCubic:
            fcc(s, 0);
            cell.formula_units = 4;
            cell.packing_fraction = PI / (3.0 * SQRT2);
            break;
        case CrystalStructure::HexagonalClosePacked:
            hexa(1.633);
            s.push_back({ 1.0 / 3.0, 2.0 / 3.0, 0.25, 0 });
            s.push_back({ 2.0 / 3.0, 1.0 / 3.0, 0.75, 0 });
            cell.formula_units = 2;
            cell.packing_fraction = PI / (3.0 * SQRT2);
            break;
        case CrystalStructure::Diamond:
            fcc(s, 0);
            fcc(s, 0, 0.25, 0.25, 0.25);
            cell.formula_units = 8;
            cell.packing_fraction = PI * SQRT3 / 16.0;
            break;
        case CrystalStructure::RockSalt:
            fcc(s, 0);
            fcc(s, 1, 0.5, 0, 0);
            cell.formula_units = 4;
            cell.species_count = 2;
            break;
        case CrystalStructure::CesiumChloride:
            s.push_back({ 0.5, 0.5, 0.5, 0 });
            s.push_back({ 0, 0, 0, 1 });
            cell.species_count = 2;
            break;
        case CrystalStructure::ZincBlende:
            fcc(s, 1);
            fcc(s, 0, 0.25, 0.25, 0.25);
            cell.formula_units = 4;
            cell.species_count = 2;
            break;
        case CrystalStructure::Wurtzite:
            hexa(1.633);
            s.push_back({ 1.0 / 3.0, 2.0 / 3.0, 0.0, 1 });
            s.push_back({ 2.0 / 3.0, 1.0 / 3.0, 0.5, 1 });
            s.push_back({ 1.0 / 3.0, 2.0 / 3.0, 0.375, 0 });
            s.push_back({ 2.0 / 3.0, 1.0 / 3.0, 0.875, 0 });
            cell.formula_units = 2;
            cell.species_count = 2;
            break;
        case CrystalStructure::Fluorite:
        case CrystalStructure::Antifluorite: {
            const int big = structure == CrystalStructure::Fluorite ? 0 : 1;
            const int minor_species = 1 - big;
            fcc(s, big);
            for (double x : { 0.25, 0.75 }) {
                for (double y : { 0.25, 0.75 }) {
                    for (double z : { 0.25, 0.75 }) s.push_back({ x, y, z, minor_species });
                }
            }
            cell.formula_units = 4;
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::AntiBixbyite: {
            fcc(s, 1);
            int k = 0;
            for (double x : { 0.25, 0.75 }) {
                for (double y : { 0.25, 0.75 }) {
                    for (double z : { 0.25, 0.75 }) {
                        if (k != 0 && k != 7) s.push_back({ x, y, z, 0 });
                        ++k;
                    }
                }
            }
            cell.formula_units = 2;
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::Rutile: {
            cell.c = c_pm == c_pm ? c_pm : 0.644 * a_pm;
            const double u = 0.305;
            s.push_back({ 0, 0, 0, 0 });
            s.push_back({ 0.5, 0.5, 0.5, 0 });
            s.push_back({ u, u, 0, 1 });
            s.push_back({ 1 - u, 1 - u, 0, 1 });
            s.push_back({ 0.5 + u, 0.5 - u, 0.5, 1 });
            s.push_back({ 0.5 - u, 0.5 + u, 0.5, 1 });
            cell.formula_units = 2;
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::Corundum: {
            hexa(2.73);
            const double zc = 0.3523, xo = 0.3064;
            const double centering[3][3] = { { 0, 0, 0 }, { 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 }, { 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0 } };
            const double cats[4][3] = { { 0, 0, zc }, { 0, 0, 0.5 - zc }, { 0, 0, -zc }, { 0, 0, zc + 0.5 } };
            const double ans[6][3] = { { xo, 0, 0.25 }, { 0, xo, 0.25 }, { -xo, -xo, 0.25 }, { -xo, 0, 0.75 }, { 0, -xo, 0.75 }, { xo, xo, 0.75 } };
            for (const auto& t : centering) {
                for (const auto& p : cats) s.push_back({ wrap(p[0] + t[0]), wrap(p[1] + t[1]), wrap(p[2] + t[2]), 0 });
                for (const auto& p : ans) s.push_back({ wrap(p[0] + t[0]), wrap(p[1] + t[1]), wrap(p[2] + t[2]), 1 });
            }
            cell.formula_units = 6;
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::NickelArsenide:
            hexa(1.39);
            s.push_back({ 0, 0, 0, 0 });
            s.push_back({ 0, 0, 0.5, 0 });
            s.push_back({ 1.0 / 3.0, 2.0 / 3.0, 0.25, 1 });
            s.push_back({ 2.0 / 3.0, 1.0 / 3.0, 0.75, 1 });
            cell.formula_units = 2;
            cell.species_count = 2;
            break;
        case CrystalStructure::CadmiumIodide:
            hexa(1.61);
            s.push_back({ 0, 0, 0, 0 });
            s.push_back({ 1.0 / 3.0, 2.0 / 3.0, 0.25, 1 });
            s.push_back({ 2.0 / 3.0, 1.0 / 3.0, 0.75, 1 });
            cell.species_count = 2;
            break;
        case CrystalStructure::Cuprite:
            s.push_back({ 0, 0, 0, 1 });
            s.push_back({ 0.5, 0.5, 0.5, 1 });
            s.push_back({ 0.25, 0.25, 0.25, 0 });
            s.push_back({ 0.75, 0.75, 0.25, 0 });
            s.push_back({ 0.75, 0.25, 0.75, 0 });
            s.push_back({ 0.25, 0.75, 0.75, 0 });
            cell.formula_units = 2;
            cell.species_count = 2;
            break;
        case CrystalStructure::BetaCristobalite: {
            std::vector<LatticeSite> si;
            fcc(si, 0);
            fcc(si, 0, 0.25, 0.25, 0.25);
            for (const LatticeSite& p : si) s.push_back(p);
            for (std::size_t i = 0; i < 4; ++i) {
                const LatticeSite& a = si[i];
                const double offs[4][3] = { { 0.25, 0.25, 0.25 }, { 0.25, -0.25, -0.25 }, { -0.25, 0.25, -0.25 }, { -0.25, -0.25, 0.25 } };
                for (const auto& o : offs) s.push_back({ wrap(a.x + o[0] / 2.0), wrap(a.y + o[1] / 2.0), wrap(a.z + o[2] / 2.0), 1 });
            }
            cell.formula_units = 8;
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::RheniumTrioxide:
        case CrystalStructure::AntiRheniumTrioxide: {
            const int center = structure == CrystalStructure::RheniumTrioxide ? 0 : 1;
            s.push_back({ 0, 0, 0, center });
            s.push_back({ 0.5, 0, 0, 1 - center });
            s.push_back({ 0, 0.5, 0, 1 - center });
            s.push_back({ 0, 0, 0.5, 1 - center });
            cell.species_count = 2;
            break;
        }
        case CrystalStructure::Perovskite:
            s.push_back({ 0, 0, 0, 0 });
            s.push_back({ 0.5, 0.5, 0.5, 2 });
            s.push_back({ 0.5, 0.5, 0, 1 });
            s.push_back({ 0.5, 0, 0.5, 1 });
            s.push_back({ 0, 0.5, 0.5, 1 });
            cell.species_count = 3;
            break;
        default:
            s.push_back({ 0, 0, 0, 0 });
            break;
    }
    return cell;
}

std::vector<CrystalAtom> build_crystal(const UnitCell& cell, int nx, int ny, int nz, bool include_boundary) {
    std::vector<CrystalAtom> out;
    nx = std::max(1, nx);
    ny = std::max(1, ny);
    nz = std::max(1, nz);
    const double eps = 1e-6;
    const int ex = include_boundary ? 1 : 0;
    for (int i = 0; i < nx + ex; ++i) {
        for (int j = 0; j < ny + ex; ++j) {
            for (int k = 0; k < nz + ex; ++k) {
                for (const LatticeSite& site : cell.sites) {
                    const double fx = site.x + i, fy = site.y + j, fz = site.z + k;
                    if (fx > nx + eps || fy > ny + eps || fz > nz + eps) continue;
                    const std::array<double, 3> p = cell.to_cartesian(fx, fy, fz);
                    out.push_back({ p[0], p[1], p[2], site.species, false });
                }
            }
        }
    }
    double cx = 0, cy = 0, cz = 0;
    const std::array<double, 3> far_corner = cell.to_cartesian(nx, ny, nz);
    const std::array<double, 3> fx = cell.to_cartesian(nx, 0, 0);
    const std::array<double, 3> fy = cell.to_cartesian(0, ny, 0);
    cx = 0.5 * (fx[0] + fy[0]);
    cy = 0.5 * (fx[1] + fy[1]);
    cz = 0.5 * far_corner[2];
    for (CrystalAtom& a : out) {
        a.x -= cx;
        a.y -= cy;
        a.z -= cz;
    }
    return out;
}

std::vector<std::pair<std::size_t, std::size_t>> nearest_neighbor_pairs(const std::vector<CrystalAtom>& atoms, double tolerance) {
    std::vector<std::pair<std::size_t, std::size_t>> out;
    if (atoms.size() < 2) return out;
    bool mixed = false;
    for (const CrystalAtom& a : atoms) mixed = mixed || a.species != atoms.front().species;
    double best = 1e300;
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        for (std::size_t j = i + 1; j < atoms.size(); ++j) {
            if (mixed && atoms[i].species == atoms[j].species) continue;
            const double dx = atoms[i].x - atoms[j].x, dy = atoms[i].y - atoms[j].y, dz = atoms[i].z - atoms[j].z;
            const double d = dx * dx + dy * dy + dz * dz;
            if (d > 1e-6 && d < best) best = d;
        }
    }
    const double limit = best * (1.0 + tolerance) * (1.0 + tolerance);
    for (std::size_t i = 0; i < atoms.size(); ++i) {
        for (std::size_t j = i + 1; j < atoms.size(); ++j) {
            if (mixed && atoms[i].species == atoms[j].species) continue;
            const double dx = atoms[i].x - atoms[j].x, dy = atoms[i].y - atoms[j].y, dz = atoms[i].z - atoms[j].z;
            const double d = dx * dx + dy * dy + dz * dz;
            if (d > 1e-6 && d <= limit) out.emplace_back(i, j);
        }
    }
    return out;
}

double thermochemical_radius(const IonDescriptor& ion) noexcept {
    for (const ThermoRadius& t : THERMO_RADII) {
        if (ion.formula == t.formula && ion.charge == t.charge) return t.radius;
    }
    if (ion.polyatomic) {
        try {
            const Formula f = Formula::parse(ion.formula);
            return 110.0 + 35.0 * std::cbrt(f.atom_count());
        } catch (...) {
            return 200.0;
        }
    }
    return NO_VALUE;
}

std::vector<int> likely_cation_charges(const Element& metal) {
    std::vector<int> out;
    const int common = metal.most_common_oxidation_state();
    if (common > 0) out.push_back(common);
    for (const OxidationStateEntry& s : metal.oxidation_states()) {
        if (s.state > 0 && s.common && std::find(out.begin(), out.end(), s.state) == out.end()) out.push_back(s.state);
    }
    for (const OxidationStateEntry& s : metal.oxidation_states()) {
        if (s.state > 0 && std::find(out.begin(), out.end(), s.state) == out.end()) out.push_back(s.state);
    }
    if (out.empty()) out.push_back(metal.group <= 2 ? std::max(1, static_cast<int>(metal.group)) : (metal.group >= 13 ? static_cast<int>(metal.group) - 10 : 2));
    return out;
}

int likely_anion_charge(const Element& nonmetal) {
    if (nonmetal.atomic_number == 1) return -1;
    if (nonmetal.group >= 13 && nonmetal.group <= 17) return static_cast<int>(nonmetal.group) - 18;
    int q = 0;
    for (const OxidationStateEntry& s : nonmetal.oxidation_states()) {
        if (s.state < q) q = s.state;
    }
    return q;
}

double IonicCrystal::lattice_energy() const noexcept {
    if (lattice_energy_born_haber == lattice_energy_born_haber) return lattice_energy_born_haber;
    if (lattice_energy_born_lande == lattice_energy_born_lande) return lattice_energy_born_lande;
    return lattice_energy_kapustinskii;
}

IonicCrystal ionic_crystal(const Element& metal, const Element& nonmetal) {
    const int qc = likely_cation_charges(metal).front();
    int qa = likely_anion_charge(nonmetal);
    if (qa == 0) qa = -1;
    return ionic_crystal(monatomic_ion(metal, qc), monatomic_ion(nonmetal, qa));
}

IonicCrystal ionic_crystal(const IonDescriptor& cation, const IonDescriptor& anion) {
    IonicCrystal r;
    r.cation = cation;
    r.anion = anion;
    if (cation.charge <= 0 || anion.charge >= 0) {
        r.notes.push_back("an ionic compound needs a positive and a negative ion");
        return r;
    }
    const IonicCombination combo = combine_ions(cation, anion);
    r.cation_count = combo.cation_count;
    r.anion_count = combo.anion_count;
    r.formula = combo.formula;
    r.name = combo.name;
    const int m = r.cation_count;
    const int x = r.anion_count;
    double rc = ion_radius(cation, 6);
    double ra = ion_radius(anion, 6);
    r.radius_ratio = std::min(rc, ra) / std::max(rc, ra);
    const double ratio = rc / ra;
    CrystalStructure s = CrystalStructure::Unknown;
    std::string reason;
    const std::string formula = clean_formula(r.formula);
    for (const StructureOverride& o : OVERRIDES) {
        if (formula == o.formula) {
            s = o.structure;
            reason = "the measured structure of " + formula;
            break;
        }
    }
    if (s == CrystalStructure::Unknown) {
        const double rr = r.radius_ratio;
        if (m == 1 && x == 1) {
            if (rr >= 0.732) {
                s = CrystalStructure::CesiumChloride;
                reason = "radius ratio " + format_number(rr, 3) + " is above 0.732, so each ion fits 8 neighbors (cubic holes)";
            } else if (rr >= 0.414) {
                s = CrystalStructure::RockSalt;
                reason = "radius ratio " + format_number(rr, 3) + " is between 0.414 and 0.732, so each ion fits 6 neighbors (octahedral holes)";
            } else {
                s = CrystalStructure::ZincBlende;
                reason = "radius ratio " + format_number(rr, 3) + " is below 0.414, so each ion only fits 4 neighbors (tetrahedral holes)";
            }
            if (s == CrystalStructure::CesiumChloride && (cation.charge > 1 || !cation.element || cation.element->group != 1)) {
                s = CrystalStructure::RockSalt;
                reason += "; the radius-ratio rule overestimates coordination here, and rock salt is far more common";
            }
        } else if (m == 1 && x == 2) {
            if (ratio >= 0.732) {
                s = CrystalStructure::Fluorite;
                reason = "a large cation (r+/r- = " + format_number(ratio, 3) + ") with twice as many anions gives 8:4 coordination";
            } else if (ratio >= 0.414) {
                s = CrystalStructure::Rutile;
                reason = "r+/r- = " + format_number(ratio, 3) + " favors 6:3 coordination for MX2";
            } else {
                s = CrystalStructure::BetaCristobalite;
                reason = "a small cation (r+/r- = " + format_number(ratio, 3) + ") gives 4:2 coordination for MX2";
            }
        } else if (m == 2 && x == 1) {
            s = CrystalStructure::Antifluorite;
            reason = "M2X compounds usually take the antifluorite structure (the cations fill all tetrahedral holes)";
        } else if (m == 2 && x == 3) {
            s = CrystalStructure::Corundum;
            reason = "M2X3 compounds of small cations take the corundum structure (cations in 2/3 of the octahedral holes)";
        } else if (m == 3 && x == 2) {
            s = CrystalStructure::AntiBixbyite;
            reason = "M3X2 compounds take the anti-bixbyite structure, a fluorite-like arrangement with some sites empty";
        } else if (m == 1 && x == 3) {
            s = CrystalStructure::RheniumTrioxide;
            reason = "MX3 is shown as the rhenium trioxide arrangement (corner-sharing octahedra)";
        } else if (m == 3 && x == 1) {
            s = CrystalStructure::AntiRheniumTrioxide;
            reason = "M3X is shown as the anti-rhenium trioxide arrangement";
        } else {
            s = CrystalStructure::RockSalt;
            reason = "no simple structure fits " + std::to_string(m) + ":" + std::to_string(x) + "; showing a rock salt arrangement as a stand-in";
            r.notes.push_back("the real structure is more complex than any of the simple types here");
        }
    }
    r.structure = s;
    r.structure_reason = reason;
    const std::array<int, 2> cn = coordination_of(s);
    r.cation_coordination = cn[0];
    r.anion_coordination = cn[1];
    rc = ion_radius(cation, cn[0] > 0 ? cn[0] : 6);
    ra = ion_radius(anion, cn[1] > 0 ? cn[1] : 6);
    r.cation_radius = rc;
    r.anion_radius = ra;
    r.nearest_distance = rc + ra;
    const double a = structure_a_from_distance(s, r.nearest_distance);
    r.cell = unit_cell(s, a);
    r.molar_mass = combo.composition().molar_mass();
    const double cell_volume_cm3 = r.cell.volume() * 1e-30;
    r.density = r.cell.formula_units * r.molar_mass / (AVOGADRO_CONSTANT * cell_volume_cm3);
    r.madelung_constant = madelung_constant(to_string(s));
    if (cation.element && anion.element) {
        r.born_exponent = born_exponent(*cation.element, cation.charge, *anion.element, anion.charge);
        const double xc = value_or(cation.element->electronegativity_pauling, NO_VALUE);
        const double xa = value_or(anion.element->electronegativity_pauling, NO_VALUE);
        r.electronegativity_difference = std::fabs(xa - xc);
        r.percent_ionic = percent_ionic_character(r.electronegativity_difference);
    } else {
        r.born_exponent = 9.0;
        r.notes.push_back("polyatomic ions are treated as spheres with thermochemical radii, so energies are approximate");
    }
    if (r.madelung_constant == r.madelung_constant) {
        r.lattice_energy_born_lande = born_lande_energy(r.madelung_constant, cation.charge, anion.charge, r.nearest_distance, r.born_exponent);
        r.lattice_energy_born_mayer = born_mayer_energy(r.madelung_constant, cation.charge, anion.charge, r.nearest_distance);
    }
    r.lattice_energy_kapustinskii = kapustinskii_energy(m + x, cation.charge, anion.charge, rc, ra);
    for (const Substance* sub : find_substances(combo.composition())) {
        const double h = sub->enthalpy_of_formation(Phase::Solid);
        if (h == h) {
            r.enthalpy_of_formation = h / 1000.0;
            r.melting_point = sub->melting_point;
            r.boiling_point = sub->boiling_point;
            break;
        }
    }
    if (cation.element && anion.element) {
        const double ie = cation.element->cumulative_ionization_energy(cation.charge) * EV_PER_PARTICLE_TO_KJ_PER_MOL;
        double ea = anion.element->electron_affinity_kj_per_mol();
        if (anion.charge == -2 && anion.element->atomic_number == 8) ea += O2_SECOND_ELECTRON_AFFINITY;
        else if (anion.charge != -1) ea = NO_VALUE;
        r.ionization_energy_total = ie * m;
        r.electron_affinity_total = ea * x;
        const double metal_atom = cation.element->atomization_enthalpy;
        const double nonmetal_atom = anion.element->atomization_enthalpy;
        if (r.enthalpy_of_formation == r.enthalpy_of_formation && ea == ea && metal_atom == metal_atom && nonmetal_atom == nonmetal_atom) {
            r.lattice_energy_born_haber = born_haber_lattice_energy(r.enthalpy_of_formation, m * metal_atom, m * ie, x * nonmetal_atom, x * ea);
        }
        const std::string cat = cation.text(true);
        const std::string an = anion.text(true);
        r.steps.push_back(std::string(cation.element->symbol) + " gives up " + std::to_string(cation.charge) + " electron" + (cation.charge > 1 ? "s" : "") + " to become " + cat + " (costs " + format_number(ie, 4) + " kJ/mol).");
        if (ea == ea) r.steps.push_back(std::string(anion.element->symbol) + " takes " + std::to_string(-anion.charge) + " electron" + (anion.charge < -1 ? "s" : "") + " to become " + an + " (" + (ea >= 0 ? "releases " + format_number(ea, 4) : "costs " + format_number(-ea, 4)) + " kJ/mol).");
        else r.steps.push_back(std::string(anion.element->symbol) + " takes " + std::to_string(-anion.charge) + " electrons to become " + an + ".");
        r.steps.push_back("Making the ions alone costs energy; the payoff is the lattice: " + std::to_string(m) + " " + cat + " for every " + std::to_string(x) + " " + an + " pack together and release about " + format_number(r.lattice_energy(), 4) + " kJ/mol.");
    } else {
        r.steps.push_back("The " + cation.name + " and " + anion.name + " ions attract and pack in a " + to_string(s) + " arrangement.");
    }
    r.steps.push_back("Each " + cation.text(true) + " touches " + std::to_string(r.cation_coordination) + " " + anion.text(true) + (r.anion_coordination ? " and each " + anion.text(true) + " touches " + std::to_string(r.anion_coordination) + " " + cation.text(true) : std::string()) + ".");
    r.solubility = solubility(cation, anion);
    r.ok = true;
    return r;
}

const char* to_string(AlloyType t) noexcept {
    switch (t) {
        case AlloyType::PureMetal: return "pure metal";
        case AlloyType::Substitutional: return "substitutional solid solution";
        case AlloyType::Interstitial: return "interstitial solid solution";
        case AlloyType::LimitedSolubility: return "limited solid solubility (likely two phases)";
        case AlloyType::Intermetallic: return "intermetallic compound tendency";
        case AlloyType::Amalgam: return "amalgam (mercury alloy)";
        default: return "unknown";
    }
}

int free_electron_valence(const Element& e) noexcept {
    const int g = e.group;
    if (g == 1 || g == 11) return 1;
    if (g == 2 || g == 12) return 2;
    if (g == 13) return 3;
    if (g == 14) return 4;
    if (g == 15) return 5;
    if (g >= 3 && g <= 10) return 2;
    if (e.block == Block::F) return 3;
    return 2;
}

double metallic_lattice_constant(CrystalStructure s, double radius_pm) noexcept {
    switch (s) {
        case CrystalStructure::SimpleCubic: return 2.0 * radius_pm;
        case CrystalStructure::BodyCenteredCubic: return 4.0 * radius_pm / SQRT3;
        case CrystalStructure::FaceCenteredCubic: return 2.0 * SQRT2 * radius_pm;
        case CrystalStructure::HexagonalClosePacked: return 2.0 * radius_pm;
        case CrystalStructure::Diamond: return 8.0 * radius_pm / SQRT3;
        default: return 2.0 * SQRT2 * radius_pm;
    }
}

MetallicCrystal metallic_crystal(const Element& metal) {
    return metallic_crystal(std::vector<AlloyComponent>{ AlloyComponent{ &metal, 1.0 } });
}

MetallicCrystal metallic_crystal(const std::vector<AlloyComponent>& input) {
    MetallicCrystal r;
    std::map<int, double> merged;
    for (const AlloyComponent& c : input) {
        if (c.element && c.fraction > 0.0) merged[c.element->atomic_number] += c.fraction;
    }
    if (merged.empty()) {
        r.notes.push_back("pick at least one metal");
        return r;
    }
    double total = 0.0;
    for (const auto& [z, f] : merged) total += f;
    for (const auto& [z, f] : merged) r.components.push_back({ &PeriodicTable::element(z), f / total });
    std::stable_sort(r.components.begin(), r.components.end(), [](const AlloyComponent& a, const AlloyComponent& b) { return a.fraction > b.fraction; });
    const Element& host = *r.components.front().element;
    const auto radius_of = [](const Element& e) { return metallic_atom_radius(e); };
    CrystalStructure s = crystal_structure_named(host.crystal_structure);
    if (s == CrystalStructure::Unknown) {
        const std::string real = host.crystal_structure;
        if (real == "CUB") {
            s = host.atomic_number == 84 ? CrystalStructure::SimpleCubic : CrystalStructure::BodyCenteredCubic;
            if (host.atomic_number != 84) r.structure_note = std::string(host.name) + " has a complex cubic structure; shown as body-centered cubic";
        } else {
            s = CrystalStructure::FaceCenteredCubic;
            if (!real.empty()) r.structure_note = std::string(host.name) + " is really " + (real == "ORC" ? "orthorhombic" : real == "TET" ? "tetragonal" : real == "RHL" ? "rhombohedral" : real == "MCL" ? "monoclinic" : real) + "; shown as the nearest close-packed arrangement";
            else r.structure_note = "the crystal structure of " + std::string(host.name) + " isn't known; shown as face-centered cubic";
        }
    }
    r.structure = s;
    double host_a = host.lattice_constant;
    const bool table_a = host_a == host_a && (crystal_structure_named(host.crystal_structure) == s);
    if (!table_a) {
        host_a = metallic_lattice_constant(s, radius_of(host));
        if (host.density == host.density && host.density > 0.0) {
            const UnitCell probe = unit_cell(s, 1.0);
            const double cell_cm3 = probe.formula_units * host.atomic_weight / (AVOGADRO_CONSTANT * host.density);
            host_a = std::cbrt(cell_cm3 / (probe.volume() * 1e-30)) ;
        }
    }
    double host_c = NO_VALUE;
    if (s == CrystalStructure::HexagonalClosePacked) {
        double ratio = 1.633;
        for (const AxialRatio& ar : HCP_RATIOS) {
            if (ar.z == host.atomic_number) ratio = ar.ratio;
        }
        host_c = ratio * host_a;
    }
    const bool hg = merged.count(80) > 0 && r.components.size() > 1;
    double interstitial_fraction = 0.0;
    std::vector<int> interstitial;
    const double host_r = radius_of(host);
    for (std::size_t i = 1; i < r.components.size(); ++i) {
        const Element& e = *r.components[i].element;
        const double rr = value_or(e.covalent_radius, radius_of(e));
        if ((e.atomic_number == 1 || e.atomic_number == 5 || e.atomic_number == 6 || e.atomic_number == 7 || e.atomic_number == 8) && rr / host_r < 0.63) {
            interstitial.push_back(static_cast<int>(i));
            interstitial_fraction += r.components[i].fraction;
        }
    }
    bool all_pass = true;
    double max_size = 0.0;
    double max_en = 0.0;
    for (std::size_t i = 1; i < r.components.size(); ++i) {
        if (std::find(interstitial.begin(), interstitial.end(), static_cast<int>(i)) != interstitial.end()) continue;
        const Element& e = *r.components[i].element;
        const double size = std::fabs(radius_of(e) - host_r) / host_r;
        const double den = std::fabs(value_or(e.electronegativity_pauling, 1.8) - value_or(host.electronegativity_pauling, 1.8));
        const bool same_structure = crystal_structure_named(e.crystal_structure) == s;
        const int dv = std::abs(free_electron_valence(e) - free_electron_valence(host));
        max_size = std::max(max_size, size);
        max_en = std::max(max_en, den);
        const std::string tag = std::string(e.symbol) + " in " + host.symbol + ": ";
        r.checks.push_back({ "atomic size within 15%", tag + format_number(size * 100.0, 3) + "% difference", size <= 0.15 });
        r.checks.push_back({ "same crystal structure", tag + (e.crystal_structure[0] ? std::string(e.crystal_structure) : std::string("?")) + " vs " + (host.crystal_structure[0] ? host.crystal_structure : "?"), same_structure });
        r.checks.push_back({ "similar electronegativity", tag + "difference " + format_number(den, 3), den <= 0.4 });
        r.checks.push_back({ "similar valence", tag + std::to_string(free_electron_valence(e)) + " vs " + std::to_string(free_electron_valence(host)), dv <= 1 });
        all_pass = all_pass && size <= 0.15 && den <= 0.4;
        if (size <= 0.15 && den <= 0.4 && (!same_structure || dv > 1)) r.notes.push_back(std::string(e.name) + " dissolves in " + host.name + " only up to a limit: complete mixing needs the same crystal structure and valence");
    }
    for (int idx : interstitial) {
        const Element& e = *r.components[idx].element;
        const double rr = value_or(e.covalent_radius, radius_of(e));
        r.checks.push_back({ "small enough to fit between atoms", std::string(e.symbol) + " radius / " + host.symbol + " radius = " + format_number(rr / host_r, 3) + " (small enough to sit in the holes)", true });
    }
    r.size_mismatch = max_size;
    r.electronegativity_spread = max_en;
    if (r.components.size() == 1) r.type = AlloyType::PureMetal;
    else if (hg) r.type = AlloyType::Amalgam;
    else if (!interstitial.empty() && interstitial.size() + 1 == r.components.size()) r.type = AlloyType::Interstitial;
    else if (all_pass) r.type = AlloyType::Substitutional;
    else if (max_en > 0.7 || (max_en > 0.4 && max_size > 0.15)) r.type = AlloyType::Intermetallic;
    else r.type = AlloyType::LimitedSolubility;
    double a = 0.0, c = 0.0, radius = 0.0, molar = 0.0, valence = 0.0, tm = 0.0, tb = 0.0, cohesive = 0.0;
    double sub_total = 0.0;
    for (std::size_t i = 0; i < r.components.size(); ++i) {
        const Element& e = *r.components[i].element;
        const double f = r.components[i].fraction;
        molar += f * e.atomic_weight;
        valence += f * free_electron_valence(e);
        tm += f * value_or(e.melting_point, NO_VALUE);
        tb += f * value_or(e.boiling_point, NO_VALUE);
        cohesive += f * value_or(e.atomization_enthalpy, NO_VALUE);
        if (std::find(interstitial.begin(), interstitial.end(), static_cast<int>(i)) != interstitial.end()) continue;
        sub_total += f;
        const double ai = i == 0 ? host_a : metallic_lattice_constant(s, radius_of(e)) * (table_a ? host_a / metallic_lattice_constant(s, host_r) : 1.0);
        a += f * ai;
        radius += f * radius_of(e);
    }
    a /= sub_total;
    radius /= sub_total;
    a *= 1.0 + 0.25 * interstitial_fraction;
    if (host_c == host_c) c = host_c * a / host_a;
    r.lattice_constant = a;
    r.lattice_c = host_c == host_c ? c : NO_VALUE;
    r.metallic_radius = radius;
    r.molar_mass = molar;
    r.valence_electrons_per_atom = valence;
    r.melting_point = tm;
    r.boiling_point = tb;
    r.cohesive_energy = cohesive;
    r.cell = unit_cell(s, a, r.lattice_c);
    r.coordination = r.cell.coordination[0];
    r.packing_fraction = r.cell.packing_fraction;
    const double atoms_per_cell = r.cell.formula_units / std::max(1e-9, sub_total);
    const double volume_m3 = r.cell.volume() * 1e-36;
    r.density = atoms_per_cell * molar / AVOGADRO_CONSTANT / (volume_m3 * 1e6);
    r.electron_density = atoms_per_cell * valence / volume_m3;
    r.fermi_energy = REDUCED_PLANCK_CONSTANT * REDUCED_PLANCK_CONSTANT / (2.0 * ELECTRON_MASS) * std::pow(3.0 * PI * PI * r.electron_density, 2.0 / 3.0) / ELECTRONVOLT;
    r.fermi_temperature = r.fermi_energy * ELECTRONVOLT / BOLTZMANN_CONSTANT;
    r.fermi_velocity = std::sqrt(2.0 * r.fermi_energy * ELECTRONVOLT / ELECTRON_MASS);
    std::string formula;
    for (const AlloyComponent& comp : r.components) {
        formula += comp.element->symbol;
        if (r.components.size() > 1) formula += format_number(comp.fraction * 100.0, 3);
    }
    r.formula = formula;
    if (r.components.size() == 1) {
        r.name = std::string(host.name);
        for (char& ch : r.name) ch = ascii_lower(ch);
    } else {
        double mass_total = 0.0;
        for (const AlloyComponent& comp : r.components) mass_total += comp.fraction * comp.element->atomic_weight;
        for (const AlloyName& an : ALLOYS) {
            if (an.host != host.atomic_number) continue;
            for (std::size_t i = 1; i < r.components.size(); ++i) {
                if (r.components[i].element->atomic_number != an.solute) continue;
                const double wt = r.components[i].fraction * r.components[i].element->atomic_weight / mass_total;
                if (wt >= an.min_fraction && wt <= an.max_fraction) {
                    if (r.name.empty()) r.name = an.name;
                    else r.other_names.push_back(an.name);
                    r.notes.push_back(std::string(an.name) + ": " + an.note);
                }
            }
        }
        if (hg && r.name.empty()) r.name = "amalgam";
        if (r.name.empty()) {
            std::string n;
            for (std::size_t i = 0; i < r.components.size(); ++i) {
                std::string en = r.components[i].element->name;
                for (char& ch : en) ch = ascii_lower(ch);
                n += (i ? "-" : "") + en;
            }
            r.name = n + " alloy";
        }
        r.notes.push_back("melting point is a weighted average; real alloys often melt lower, especially near a eutectic composition");
    }
    if (!r.structure_note.empty()) r.notes.push_back(r.structure_note);
    r.ok = true;
    return r;
}

std::vector<CrystalAtom> build_metal(const MetallicCrystal& crystal, int nx, int ny, int nz, unsigned seed) {
    std::vector<CrystalAtom> atoms = build_crystal(crystal.cell, nx, ny, nz, true);
    if (crystal.components.size() <= 1) return atoms;
    std::vector<std::size_t> substitutional;
    std::vector<std::size_t> interstitial;
    double sub_total = 0.0;
    const Element& host = *crystal.components.front().element;
    const double host_r = metallic_atom_radius(host);
    for (std::size_t i = 0; i < crystal.components.size(); ++i) {
        const Element& e = *crystal.components[i].element;
        const double rr = value_or(e.covalent_radius, 100.0);
        const bool fits_between = i > 0 && (e.atomic_number == 1 || e.atomic_number == 5 || e.atomic_number == 6 || e.atomic_number == 7 || e.atomic_number == 8) && rr / host_r < 0.63;
        if (fits_between) interstitial.push_back(i);
        else {
            substitutional.push_back(i);
            sub_total += crystal.components[i].fraction;
        }
    }
    std::uint64_t state = seed * 2654435761ULL + 12345ULL;
    const auto next = [&]() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(state >> 11) / 9007199254740992.0;
    };
    std::vector<std::size_t> order(atoms.size());
    std::iota(order.begin(), order.end(), 0);
    for (std::size_t i = order.size(); i > 1; --i) std::swap(order[i - 1], order[static_cast<std::size_t>(next() * static_cast<double>(i)) % i]);
    std::size_t pos = 0;
    for (std::size_t k = 0; k < substitutional.size(); ++k) {
        const std::size_t comp = substitutional[k];
        std::size_t count = static_cast<std::size_t>(std::lround(crystal.components[comp].fraction / sub_total * static_cast<double>(atoms.size())));
        if (k + 1 == substitutional.size()) count = atoms.size() - pos;
        for (std::size_t j = 0; j < count && pos < atoms.size(); ++j) atoms[order[pos++]].species = static_cast<int>(comp);
    }
    if (!interstitial.empty()) {
        std::vector<CrystalAtom> holes;
        const UnitCell& cell = crystal.cell;
        std::vector<std::array<double, 3>> frac;
        switch (cell.structure) {
            case CrystalStructure::BodyCenteredCubic: frac = { { { 0.5, 0.5, 0 } }, { { 0.5, 0, 0.5 } }, { { 0, 0.5, 0.5 } }, { { 0.5, 0, 0 } }, { { 0, 0.5, 0 } }, { { 0, 0, 0.5 } } }; break;
            case CrystalStructure::HexagonalClosePacked: frac = { { { 0, 0, 0 } }, { { 0, 0, 0.5 } } }; break;
            default: frac = { { { 0.5, 0.5, 0.5 } }, { { 0.5, 0, 0 } }, { { 0, 0.5, 0 } }, { { 0, 0, 0.5 } } }; break;
        }
        for (int i = 0; i < nx; ++i) {
            for (int j = 0; j < ny; ++j) {
                for (int k = 0; k < nz; ++k) {
                    for (const auto& f : frac) {
                        const std::array<double, 3> p = cell.to_cartesian(f[0] + i, f[1] + j, f[2] + k);
                        holes.push_back({ p[0], p[1], p[2], 0, true });
                    }
                }
            }
        }
        const std::array<double, 3> fx = cell.to_cartesian(nx, 0, 0);
        const std::array<double, 3> fy = cell.to_cartesian(0, ny, 0);
        const std::array<double, 3> far_corner = cell.to_cartesian(nx, ny, nz);
        const double cx = 0.5 * (fx[0] + fy[0]), cy = 0.5 * (fx[1] + fy[1]), cz = 0.5 * far_corner[2];
        for (CrystalAtom& h : holes) {
            h.x -= cx;
            h.y -= cy;
            h.z -= cz;
        }
        for (std::size_t i = holes.size(); i > 1; --i) std::swap(holes[i - 1], holes[static_cast<std::size_t>(next() * static_cast<double>(i)) % i]);
        std::size_t used = 0;
        const double host_atoms = static_cast<double>(atoms.size());
        for (std::size_t comp : interstitial) {
            const std::size_t want = static_cast<std::size_t>(std::lround(crystal.components[comp].fraction / sub_total * host_atoms));
            for (std::size_t j = 0; j < std::max<std::size_t>(want, 1) && used < holes.size(); ++j) {
                CrystalAtom h = holes[used++];
                h.species = static_cast<int>(comp);
                atoms.push_back(h);
            }
        }
    }
    return atoms;
}

} // namespace chemistry
} // namespace fizmo
