#include "materials.hpp"

#include "periodic_table.hpp"
#include "transport.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

constexpr double MU0 = 1.25663706212e-6;
constexpr double BOHR_MAGNETON = 9.2740100783e-24;
constexpr double RICHARDSON = 1.20173e6;
constexpr double BOHR3_TO_A3 = 0.148184711;

bool finite(double v) {
    return v == v && std::isfinite(v);
}

bool same(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

struct DielectricEntry {
    const char* name;
    const char* formula;
    double eps;
    double n;
    double strength;
};

constexpr DielectricEntry DIELECTRICS[] = {
    { "water", "H2O", 78.36, 1.333, 6.5e7 },
    { "heavy water", "D2O", 78.06, 1.328, NO_VALUE },
    { "hydrogen peroxide", "H2O2", 74.6, 1.407, NO_VALUE },
    { "methanol", "CH3OH", 32.7, 1.329, NO_VALUE },
    { "ethanol", "C2H5OH", 24.5, 1.361, NO_VALUE },
    { "1-propanol", "C3H7OH", 20.1, 1.385, NO_VALUE },
    { "isopropanol", "C3H8O", 18.3, 1.377, NO_VALUE },
    { "acetone", "C3H6O", 20.7, 1.359, NO_VALUE },
    { "acetic acid", "CH3COOH", 6.2, 1.372, NO_VALUE },
    { "formic acid", "HCOOH", 58.0, 1.371, NO_VALUE },
    { "glycerol", "C3H8O3", 42.5, 1.474, NO_VALUE },
    { "ethylene glycol", "C2H6O2", 37.7, 1.432, NO_VALUE },
    { "dimethyl sulfoxide", "C2H6OS", 46.7, 1.479, NO_VALUE },
    { "acetonitrile", "CH3CN", 37.5, 1.344, NO_VALUE },
    { "dimethylformamide", "C3H7NO", 36.7, 1.430, NO_VALUE },
    { "nitrobenzene", "C6H5NO2", 34.8, 1.551, NO_VALUE },
    { "benzene", "C6H6", 2.27, 1.501, 1.6e7 },
    { "toluene", "C7H8", 2.38, 1.497, NO_VALUE },
    { "n-hexane", "C6H14", 1.89, 1.375, NO_VALUE },
    { "n-pentane", "C5H12", 1.84, 1.358, NO_VALUE },
    { "n-octane", "C8H18", 1.95, 1.398, NO_VALUE },
    { "cyclohexane", "C6H12", 2.02, 1.426, NO_VALUE },
    { "carbon tetrachloride", "CCl4", 2.24, 1.460, NO_VALUE },
    { "chloroform", "CHCl3", 4.81, 1.446, NO_VALUE },
    { "dichloromethane", "CH2Cl2", 8.93, 1.424, NO_VALUE },
    { "diethyl ether", "C4H10O", 4.33, 1.353, NO_VALUE },
    { "tetrahydrofuran", "C4H8O", 7.6, 1.407, NO_VALUE },
    { "pyridine", "C5H5N", 12.4, 1.510, NO_VALUE },
    { "carbon disulfide", "CS2", 2.6, 1.628, NO_VALUE },
    { "hydrogen fluoride", "HF", 84.0, 1.157, NO_VALUE },
    { "hydrogen cyanide", "HCN", 115.0, 1.261, NO_VALUE },
    { "sulfuric acid", "H2SO4", 100.0, 1.427, NO_VALUE },
    { "ammonia", "NH3", 16.9, 1.325, NO_VALUE },
    { "bromine", "Br2", 3.1, 1.661, NO_VALUE },
    { "sodium chloride", "NaCl", 5.9, 1.544, 3.0e7 },
    { "potassium chloride", "KCl", 4.68, 1.490, NO_VALUE },
    { "potassium bromide", "KBr", 4.9, 1.559, NO_VALUE },
    { "potassium iodide", "KI", 4.94, 1.677, NO_VALUE },
    { "lithium fluoride", "LiF", 9.0, 1.392, NO_VALUE },
    { "lithium chloride", "LiCl", 11.9, 1.662, NO_VALUE },
    { "sodium fluoride", "NaF", 5.1, 1.326, NO_VALUE },
    { "sodium bromide", "NaBr", 6.0, 1.641, NO_VALUE },
    { "cesium chloride", "CsCl", 7.2, 1.640, NO_VALUE },
    { "silver chloride", "AgCl", 12.3, 2.071, NO_VALUE },
    { "silver bromide", "AgBr", 13.1, 2.253, NO_VALUE },
    { "magnesium oxide", "MgO", 9.8, 1.736, NO_VALUE },
    { "calcium oxide", "CaO", 11.8, 1.837, NO_VALUE },
    { "calcium fluoride", "CaF2", 6.8, 1.434, NO_VALUE },
    { "aluminium oxide", "Al2O3", 9.3, 1.768, 1.35e7 },
    { "silicon dioxide", "SiO2", 4.5, 1.544, 2.5e7 },
    { "titanium dioxide", "TiO2", 86.0, 2.61, NO_VALUE },
    { "zinc oxide", "ZnO", 8.5, 2.00, NO_VALUE },
    { "zinc sulfide", "ZnS", 8.3, 2.37, NO_VALUE },
    { "lead sulfide", "PbS", 175.0, 4.3, NO_VALUE },
    { "barium titanate", "BaTiO3", 1500.0, 2.40, NO_VALUE },
    { "calcium carbonate", "CaCO3", 8.5, 1.60, NO_VALUE },
    { "gallium arsenide", "GaAs", 12.9, 3.30, 4.0e7 },
    { "silicon carbide", "SiC", 9.7, 2.65, 3.0e8 },
    { "silicon", "Si", 11.7, 3.42, 3.0e7 },
    { "germanium", "Ge", 16.2, 4.0, 1.0e7 },
    { "diamond", "C", 5.7, 2.417, 1.0e9 },
    { "sulfur", "S", 3.6, 2.04, NO_VALUE },
    { "selenium", "Se", 6.6, 2.5, NO_VALUE },
    { "air", "", 1.000536, 1.000293, 3.0e6 },
    { "nitrogen", "N2", 1.000548, 1.000298, 3.3e6 },
    { "oxygen", "O2", 1.000494, 1.000271, NO_VALUE },
    { "hydrogen", "H2", 1.000254, 1.000132, 1.7e6 },
    { "helium", "He", 1.000065, 1.000036, 4.5e5 },
    { "neon", "Ne", 1.000127, 1.000067, NO_VALUE },
    { "argon", "Ar", 1.000517, 1.000281, NO_VALUE },
    { "krypton", "Kr", 1.00078, 1.000427, NO_VALUE },
    { "xenon", "Xe", 1.00124, 1.000702, NO_VALUE },
    { "chlorine", "Cl2", 1.0013, 1.000773, NO_VALUE },
    { "carbon dioxide", "CO2", 1.000922, 1.00045, NO_VALUE },
    { "sulfur hexafluoride", "SF6", 1.002, 1.00078, 8.9e6 },
    { "methane", "CH4", 1.00081, 1.000444, NO_VALUE },
};

const DielectricEntry* find_dielectric(std::string_view key) noexcept {
    if (key.empty()) return nullptr;
    for (const DielectricEntry& e : DIELECTRICS) {
        if (same(key, e.name) || (*e.formula && key == e.formula)) return &e;
    }
    return nullptr;
}

struct MechEntry {
    int z;
    double e;
    double k;
    double g;
    double alpha;
    double mohs;
};

constexpr MechEntry MECHANICS[] = {
    { 3, 4.9, 11.0, 4.2, 46.0, 0.6 }, { 4, 287.0, 130.0, 132.0, 11.3, 5.5 }, { 6, 1050.0, 443.0, 478.0, 1.0, 10.0 },
    { 11, 10.0, 6.3, 3.3, 71.0, 0.5 }, { 12, 45.0, 45.0, 17.0, 24.8, 2.5 }, { 13, 70.0, 76.0, 26.0, 23.1, 2.75 },
    { 14, 130.0, 100.0, 51.0, 2.6, 6.5 }, { 19, 3.53, 3.1, 1.3, 83.3, 0.4 }, { 20, 20.0, 17.0, 7.4, 22.3, 1.75 },
    { 22, 116.0, 110.0, 44.0, 8.6, 6.0 }, { 23, 128.0, 160.0, 47.0, 8.4, 7.0 }, { 24, 279.0, 160.0, 115.0, 4.9, 8.5 },
    { 25, 198.0, 120.0, NO_VALUE, 21.7, 6.0 }, { 26, 211.0, 170.0, 82.0, 11.8, 4.0 }, { 27, 209.0, 180.0, 75.0, 13.0, 5.0 },
    { 28, 200.0, 180.0, 76.0, 13.4, 4.0 }, { 29, 130.0, 140.0, 48.0, 16.5, 3.0 }, { 30, 108.0, 70.0, 43.0, 30.2, 2.5 },
    { 32, 103.0, 75.0, 41.0, 6.0, 6.0 }, { 40, 68.0, 91.1, 33.0, 5.7, 5.0 }, { 41, 105.0, 170.0, 38.0, 7.3, 6.0 },
    { 42, 329.0, 230.0, 126.0, 4.8, 5.5 }, { 44, 447.0, 220.0, 173.0, 6.4, 6.5 }, { 45, 275.0, 380.0, 150.0, 8.2, 6.0 },
    { 46, 121.0, 180.0, 44.0, 11.8, 4.75 }, { 47, 83.0, 100.0, 30.0, 18.9, 2.5 }, { 48, 50.0, 42.0, 19.0, 30.8, 2.0 },
    { 49, 11.0, NO_VALUE, NO_VALUE, 32.1, 1.2 }, { 50, 50.0, 58.0, 18.0, 22.0, 1.5 }, { 51, 55.0, 42.0, 20.0, 11.0, 3.0 },
    { 56, 13.0, 9.6, 4.9, 20.6, 1.25 }, { 72, 78.0, 110.0, 30.0, 5.9, 5.5 }, { 73, 186.0, 200.0, 69.0, 6.3, 6.5 },
    { 74, 411.0, 310.0, 161.0, 4.5, 7.5 }, { 75, 463.0, 370.0, 178.0, 6.2, 7.0 }, { 76, NO_VALUE, 462.0, 222.0, 5.1, 7.0 },
    { 77, 528.0, 320.0, 210.0, 6.4, 6.5 }, { 78, 168.0, 230.0, 61.0, 8.8, 3.5 }, { 79, 78.0, 180.0, 27.0, 14.2, 2.5 },
    { 80, NO_VALUE, 25.0, NO_VALUE, 60.4, NO_VALUE }, { 81, 8.0, 43.0, 2.8, 29.9, 1.2 }, { 82, 16.0, 46.0, 5.6, 28.9, 1.5 },
    { 83, 32.0, 31.0, 12.0, 13.4, 2.25 }, { 92, 208.0, 100.0, 111.0, 13.9, 6.0 }, { 55, 1.7, 1.6, NO_VALUE, 97.0, 0.2 },
    { 37, 2.4, 2.5, NO_VALUE, 90.0, 0.3 }, { 38, NO_VALUE, NO_VALUE, 6.1, 22.5, 1.5 }, { 31, 9.8, NO_VALUE, NO_VALUE, 18.0, 1.5 },
};

struct WorkEntry {
    int z;
    double w;
};

constexpr WorkEntry WORK_FUNCTIONS[] = {
    { 3, 2.9 }, { 11, 2.36 }, { 19, 2.29 }, { 37, 2.26 }, { 55, 2.14 }, { 4, 4.98 }, { 12, 3.66 }, { 20, 2.87 }, { 38, 2.59 },
    { 56, 2.52 }, { 13, 4.28 }, { 29, 4.65 }, { 47, 4.26 }, { 79, 5.1 }, { 26, 4.5 }, { 28, 5.15 }, { 78, 5.65 }, { 74, 4.55 },
    { 30, 4.33 }, { 82, 4.25 }, { 50, 4.42 }, { 22, 4.33 }, { 24, 4.5 }, { 27, 5.0 }, { 42, 4.6 }, { 46, 5.12 }, { 73, 4.25 },
    { 48, 4.22 }, { 80, 4.49 }, { 14, 4.85 }, { 32, 5.0 }, { 6, 5.0 }, { 83, 4.22 }, { 49, 4.12 }, { 31, 4.2 }, { 25, 4.1 },
    { 40, 4.05 }, { 41, 4.3 }, { 77, 5.27 }, { 76, 5.93 }, { 75, 4.72 }, { 45, 4.98 }, { 44, 4.71 }, { 72, 3.9 }, { 51, 4.55 },
    { 81, 3.84 }, { 92, 3.63 }, { 90, 3.4 }, { 34, 5.9 }, { 52, 4.95 }, { 23, 4.3 }, { 39, 3.1 }, { 21, 3.5 }, { 57, 3.5 },
};

struct SusEntry {
    int z;
    double chi;
};

constexpr SusEntry SUSCEPTIBILITY[] = {
    { 1, -3.99 }, { 2, -1.88 }, { 3, 14.2 }, { 4, -9.0 }, { 5, -6.7 }, { 6, -6.0 }, { 7, -12.0 }, { 8, 3449.0 }, { 10, -6.96 },
    { 11, 16.0 }, { 12, 13.1 }, { 13, 16.5 }, { 14, -3.9 }, { 15, -26.6 }, { 16, -15.5 }, { 17, -40.5 }, { 18, -19.32 },
    { 19, 20.8 }, { 20, 40.0 }, { 22, 151.0 }, { 23, 255.0 }, { 24, 167.0 }, { 25, 511.0 }, { 29, -5.46 }, { 30, -9.15 },
    { 32, -76.84 }, { 35, -56.4 }, { 36, -28.8 }, { 42, 72.0 }, { 46, 567.4 }, { 47, -19.5 }, { 48, -19.7 }, { 50, 3.1 },
    { 53, -90.0 }, { 54, -45.5 }, { 74, 53.0 }, { 78, 201.9 }, { 79, -28.0 }, { 80, -33.4 }, { 82, -23.0 }, { 83, -280.1 },
};

struct IonPol {
    int z;
    int charge;
    double a;
};

constexpr IonPol ION_POLARIZABILITY[] = {
    { 3, 1, 0.03 }, { 11, 1, 0.41 }, { 19, 1, 1.33 }, { 37, 1, 1.98 }, { 55, 1, 3.34 }, { 4, 2, 0.008 }, { 12, 2, 0.094 },
    { 20, 2, 1.1 }, { 38, 2, 1.6 }, { 56, 2, 2.5 }, { 13, 3, 0.05 }, { 30, 2, 0.8 }, { 47, 1, 1.7 }, { 29, 1, 1.6 },
    { 26, 2, 0.6 }, { 26, 3, 0.3 }, { 82, 2, 3.6 }, { 9, -1, 0.64 }, { 17, -1, 2.96 }, { 35, -1, 4.16 }, { 53, -1, 6.43 },
    { 8, -2, 2.4 }, { 16, -2, 5.9 }, { 34, -2, 7.0 }, { 7, -3, 3.5 }, { 1, -1, 3.0 },
};

double miller_ahp(int z) noexcept {
    switch (z) {
        case 1: return 0.387;
        case 6: return 1.064;
        case 7: return 1.09;
        case 8: return 0.637;
        case 9: return 0.296;
        case 15: return 3.0;
        case 16: return 3.0;
        case 17: return 2.315;
        case 35: return 3.013;
        case 53: return 5.415;
        default: return NO_VALUE;
    }
}

double molar_volume_m3(double molar_mass_g, double density_kg_m3) noexcept {
    if (!(molar_mass_g > 0) || !(density_kg_m3 > 0)) return NO_VALUE;
    return molar_mass_g / 1000.0 / density_kg_m3;
}

void finish_mechanics(MechanicalProperties& m, double density_kg_m3, double molar_volume, double cv) {
    if (finite(m.youngs_modulus) && finite(m.bulk_modulus) && !finite(m.shear_modulus) && 9.0 * m.bulk_modulus > m.youngs_modulus) m.shear_modulus = 3.0 * m.bulk_modulus * m.youngs_modulus / (9.0 * m.bulk_modulus - m.youngs_modulus);
    if (finite(m.shear_modulus) && finite(m.bulk_modulus) && !finite(m.youngs_modulus)) m.youngs_modulus = 9.0 * m.bulk_modulus * m.shear_modulus / (3.0 * m.bulk_modulus + m.shear_modulus);
    if (finite(m.youngs_modulus) && finite(m.bulk_modulus) && m.bulk_modulus > 0) m.poisson_ratio = (3.0 * m.bulk_modulus - m.youngs_modulus) / (6.0 * m.bulk_modulus);
    if (finite(m.bulk_modulus) && m.bulk_modulus > 0) m.compressibility = 1.0 / (m.bulk_modulus * 1e9);
    if (density_kg_m3 > 0) {
        if (finite(m.bulk_modulus)) {
            const double g = finite(m.shear_modulus) ? m.shear_modulus : 0.0;
            m.longitudinal_sound_speed = std::sqrt((m.bulk_modulus + 4.0 * g / 3.0) * 1e9 / density_kg_m3);
            m.acoustic_impedance = density_kg_m3 * m.longitudinal_sound_speed;
        }
        if (finite(m.shear_modulus)) m.transverse_sound_speed = std::sqrt(m.shear_modulus * 1e9 / density_kg_m3);
        if (finite(m.youngs_modulus)) m.bar_sound_speed = std::sqrt(m.youngs_modulus * 1e9 / density_kg_m3);
    }
    if (finite(m.linear_expansion)) m.volume_expansion = 3.0 * m.linear_expansion;
    if (finite(m.linear_expansion) && finite(m.bulk_modulus) && molar_volume > 0 && cv > 0) m.gruneisen = m.volume_expansion * m.bulk_modulus * 1e9 * molar_volume / cv;
}

} // namespace

double tabulated_relative_permittivity(std::string_view key) noexcept {
    const DielectricEntry* e = find_dielectric(key);
    return e ? e->eps : NO_VALUE;
}

double tabulated_refractive_index(std::string_view key) noexcept {
    const DielectricEntry* e = find_dielectric(key);
    return e ? e->n : NO_VALUE;
}

double tabulated_dielectric_strength(std::string_view key) noexcept {
    const DielectricEntry* e = find_dielectric(key);
    return e ? e->strength : NO_VALUE;
}

double work_function(int z) noexcept {
    for (const WorkEntry& w : WORK_FUNCTIONS) {
        if (w.z == z) return w.w;
    }
    return NO_VALUE;
}

double tabulated_molar_susceptibility_cgs(int z) noexcept {
    for (const SusEntry& s : SUSCEPTIBILITY) {
        if (s.z == z) return s.chi * 1e-6;
    }
    return NO_VALUE;
}

double ion_polarizability_volume(const Element& e, int charge) noexcept {
    for (const IonPol& p : ION_POLARIZABILITY) {
        if (p.z == e.atomic_number && p.charge == charge) return p.a;
    }
    if (!finite(e.polarizability)) return NO_VALUE;
    const double atom = e.polarizability * BOHR3_TO_A3;
    if (charge > 0) return atom * std::pow(0.3, static_cast<double>(charge));
    if (charge < 0) return atom * (1.0 + 0.5 * static_cast<double>(-charge));
    return atom;
}

double molecular_polarizability_volume(const Formula& f) {
    double total = 0.0;
    for (const auto& c : f.composition()) {
        double a = miller_ahp(c.first->atomic_number);
        if (!finite(a)) a = finite(c.first->polarizability) ? 0.75 * c.first->polarizability * BOHR3_TO_A3 : NO_VALUE;
        if (!finite(a)) return NO_VALUE;
        total += a * c.second;
    }
    return total > 0 ? total : NO_VALUE;
}

double polarizability_volume_to_si(double a3) noexcept {
    return finite(a3) ? 4.0 * PI * VACUUM_PERMITTIVITY * a3 * 1e-30 : NO_VALUE;
}

double spin_only_moment(int unpaired) noexcept {
    return std::sqrt(static_cast<double>(unpaired) * (unpaired + 2.0));
}

double curie_molar_susceptibility(double mu_eff, double t) noexcept {
    if (!(t > 0) || !finite(mu_eff)) return NO_VALUE;
    return MU0 * AVOGADRO_CONSTANT * mu_eff * mu_eff * BOHR_MAGNETON * BOHR_MAGNETON / (3.0 * BOLTZMANN_CONSTANT * t);
}

double clausius_mossotti_permittivity(double n, double alpha) noexcept {
    if (!(n > 0) || !(alpha > 0)) return NO_VALUE;
    const double x = n * alpha / (3.0 * VACUUM_PERMITTIVITY);
    if (x >= 0.95) return NO_VALUE;
    return (1.0 + 2.0 * x) / (1.0 - x);
}

double onsager_permittivity(double n, double alpha, double mu, double t) noexcept {
    const double n2 = clausius_mossotti_permittivity(n, alpha);
    if (!finite(n2) || !(t > 0)) return NO_VALUE;
    if (!(mu > 0)) return n2;
    const double y = n * mu * mu / (9.0 * VACUUM_PERMITTIVITY * BOLTZMANN_CONSTANT * t);
    const double b = n2 + (n2 + 2.0) * (n2 + 2.0) * y;
    return (b + std::sqrt(b * b + 8.0 * n2 * n2)) / 4.0;
}

double debye_gas_permittivity(double n, double alpha, double mu, double t) noexcept {
    if (!(n > 0) || !(t > 0)) return NO_VALUE;
    const double a = finite(alpha) ? alpha : 0.0;
    const double orient = mu > 0 ? mu * mu / (3.0 * BOLTZMANN_CONSTANT * t) : 0.0;
    return 1.0 + n / VACUUM_PERMITTIVITY * (a + orient);
}

void fill_optics(DielectricProperties& d) {
    if (finite(d.relative_permittivity)) {
        d.susceptibility = d.relative_permittivity - 1.0;
        d.absolute_permittivity = d.relative_permittivity * VACUUM_PERMITTIVITY;
    }
    if (!finite(d.refractive_index) && finite(d.high_frequency_permittivity) && d.high_frequency_permittivity > 0) {
        d.refractive_index = std::sqrt(d.high_frequency_permittivity);
        d.refractive_index_estimated = true;
    }
    if (!finite(d.high_frequency_permittivity) && finite(d.refractive_index)) d.high_frequency_permittivity = d.refractive_index * d.refractive_index;
    if (finite(d.refractive_index) && d.refractive_index > 0) {
        const double n = d.refractive_index;
        d.speed_of_light = SPEED_OF_LIGHT / n;
        d.reflectance = (n - 1.0) * (n - 1.0) / ((n + 1.0) * (n + 1.0));
        d.brewster_angle = std::atan(n) * 180.0 / PI;
        if (n > 1.0) d.critical_angle = std::asin(1.0 / n) * 180.0 / PI;
    }
    if (finite(d.polarizability_volume)) {
        d.polarizability = polarizability_volume_to_si(d.polarizability_volume);
        d.molar_refraction = 4.0 * PI / 3.0 * AVOGADRO_CONSTANT * d.polarizability_volume * 1e-30;
    }
}

MechanicalProperties element_mechanics(const Element& e, double t) {
    MechanicalProperties m;
    for (const MechEntry& x : MECHANICS) {
        if (x.z != e.atomic_number) continue;
        m.youngs_modulus = x.e;
        m.bulk_modulus = x.k;
        m.shear_modulus = x.g;
        m.linear_expansion = finite(x.alpha) ? x.alpha * 1e-6 : NO_VALUE;
        m.mohs_hardness = x.mohs;
        break;
    }
    const double rho = e.density > 0 && e.standard_state != Phase::Gas ? e.density * 1000.0 : NO_VALUE;
    const double vm = molar_volume_m3(e.atomic_weight, rho);
    if (!finite(m.bulk_modulus) && e.is_metal() && finite(vm) && finite(e.atomization_enthalpy)) {
        m.bulk_modulus = 2.5 * e.atomization_enthalpy * 1000.0 / vm * 1e-9;
        m.estimated = true;
        m.notes.push_back("bulk modulus estimated from the cohesive energy per volume");
    }
    if (!finite(m.linear_expansion) && finite(m.bulk_modulus) && finite(vm)) {
        m.linear_expansion = 2.0 * 3.0 * GAS_CONSTANT / (3.0 * m.bulk_modulus * 1e9 * vm);
        m.estimated = true;
        m.notes.push_back("thermal expansion estimated from the Gr\xC3\xBCneisen relation (\xCE\xB3 \xE2\x89\x88 2)");
    }
    const double cv = finite(e.molar_heat_capacity) ? e.molar_heat_capacity : 3.0 * GAS_CONSTANT;
    finish_mechanics(m, rho, vm, cv);
    if (e.standard_state == Phase::Liquid) m.notes.push_back("a liquid: it has no shear stiffness, so only the bulk modulus and sound speed mean anything");
    (void)t;
    return m;
}

MechanicalProperties metal_mechanics(const MetallicCrystal& c, double t) {
    MechanicalProperties m;
    if (!c.ok || c.components.empty()) return m;
    double e = 0.0, k = 0.0, g = 0.0, a = 0.0, h = 0.0, we = 0.0, wk = 0.0, wg = 0.0, wa = 0.0, wh = 0.0;
    for (const AlloyComponent& comp : c.components) {
        const MechanicalProperties p = element_mechanics(*comp.element, t);
        if (finite(p.youngs_modulus)) { e += comp.fraction * p.youngs_modulus; we += comp.fraction; }
        if (finite(p.bulk_modulus)) { k += comp.fraction * p.bulk_modulus; wk += comp.fraction; }
        if (finite(p.shear_modulus)) { g += comp.fraction * p.shear_modulus; wg += comp.fraction; }
        if (finite(p.linear_expansion)) { a += comp.fraction * p.linear_expansion; wa += comp.fraction; }
        if (finite(p.mohs_hardness)) { h += comp.fraction * p.mohs_hardness; wh += comp.fraction; }
        if (p.estimated) m.estimated = true;
    }
    if (we > 0.5) m.youngs_modulus = e / we;
    if (wk > 0.5) m.bulk_modulus = k / wk;
    if (wg > 0.5) m.shear_modulus = g / wg;
    if (wa > 0.5) m.linear_expansion = a / wa;
    if (wh > 0.5) m.mohs_hardness = h / wh;
    if (c.components.size() > 1) {
        m.estimated = true;
        m.notes.push_back("stiffness and expansion mixed by atom fraction; alloys are usually harder than this because mixed atoms pin dislocations (solid-solution strengthening)");
        if (finite(m.mohs_hardness)) m.mohs_hardness = std::min(10.0, m.mohs_hardness + 0.5);
    }
    finish_mechanics(m, c.density * 1000.0, molar_volume_m3(c.molar_mass, c.density * 1000.0), 3.0 * GAS_CONSTANT);
    return m;
}

MechanicalProperties ionic_mechanics(const IonicCrystal& c, double t) {
    MechanicalProperties m;
    if (!c.ok) return m;
    const double rho = c.density * 1000.0;
    const double vm = molar_volume_m3(c.molar_mass, rho);
    const double u = c.lattice_energy();
    const double nb = finite(c.born_exponent) ? c.born_exponent : 8.0;
    if (finite(u) && finite(vm)) {
        m.bulk_modulus = (nb - 1.0) * std::fabs(u) * 1000.0 / (9.0 * vm) * 1e-9;
        m.youngs_modulus = 1.5 * m.bulk_modulus;
        m.shear_modulus = 0.6 * m.bulk_modulus;
        m.estimated = true;
        m.notes.push_back("bulk modulus from the Born\xE2\x80\x93Land\xC3\xA9 lattice energy, K = (n \xE2\x88\x92 1)U/9V; Young's and shear moduli assume Poisson's ratio 0.25");
        double atoms = 2.0;
        try {
            atoms = std::max(2.0, Formula::parse(c.formula).atom_count());
        } catch (...) {
        }
        const double cv = 3.0 * GAS_CONSTANT * atoms;
        m.linear_expansion = 1.6 * cv / (3.0 * m.bulk_modulus * 1e9 * vm);
        m.mohs_hardness = std::max(1.0, std::min(9.0, 1.0 + 0.95 * std::log(1.0 + m.bulk_modulus / 5.0)));
    }
    finish_mechanics(m, rho, vm, 0.0);
    m.notes.push_back("ionic crystals are hard but brittle: shifting a layer by one ion lines up like charges, which pushes the layers apart, so they shatter instead of bending");
    (void)t;
    return m;
}

ElectronGasProperties electron_gas(double n, double t, double w) {
    ElectronGasProperties g;
    g.work_function = w;
    if (finite(w) && w > 0) {
        g.photoelectric_threshold = PLANCK_CONSTANT * SPEED_OF_LIGHT / (w * ELECTRONVOLT);
        if (t > 0) g.richardson_current = RICHARDSON * t * t * std::exp(std::max(-700.0, -w * ELECTRONVOLT / (BOLTZMANN_CONSTANT * t)));
    }
    if (!(n > 0)) return g;
    g.fermi_wavevector = std::cbrt(3.0 * PI * PI * n);
    const double ef = REDUCED_PLANCK_CONSTANT * REDUCED_PLANCK_CONSTANT * g.fermi_wavevector * g.fermi_wavevector / (2.0 * ELECTRON_MASS);
    g.fermi_energy = ef / ELECTRONVOLT;
    g.fermi_temperature = ef / BOLTZMANN_CONSTANT;
    g.fermi_velocity = REDUCED_PLANCK_CONSTANT * g.fermi_wavevector / ELECTRON_MASS;
    g.density_of_states = 1.5 * n / ef;
    g.seebeck_coefficient = -PI * PI * BOLTZMANN_CONSTANT * BOLTZMANN_CONSTANT * t / (2.0 * ELEMENTARY_CHARGE * ef);
    g.thomas_fermi_length = std::sqrt(VACUUM_PERMITTIVITY / (ELEMENTARY_CHARGE * ELEMENTARY_CHARGE * g.density_of_states));
    g.pauli_susceptibility = MU0 * BOHR_MAGNETON * BOHR_MAGNETON * g.density_of_states;
    return g;
}

ElectronGasProperties element_electron_gas(const Element& e, double t) {
    if (!e.is_metal() && e.atomic_number != 33 && e.atomic_number != 51) return electron_gas(NO_VALUE, t, work_function(e.atomic_number));
    const double vm = molar_volume_m3(e.atomic_weight, e.density * 1000.0);
    const double n = finite(vm) ? free_electron_valence(e) * AVOGADRO_CONSTANT / vm : NO_VALUE;
    return electron_gas(n, t, work_function(e.atomic_number));
}

ElectronGasProperties metal_electron_gas(const MetallicCrystal& c, double t) {
    double w = 0.0, ww = 0.0;
    for (const AlloyComponent& a : c.components) {
        const double x = work_function(a.element->atomic_number);
        if (finite(x)) {
            w += a.fraction * x;
            ww += a.fraction;
        }
    }
    return electron_gas(c.electron_density, t, ww > 0.5 ? w / ww : NO_VALUE);
}

DielectricProperties element_dielectric(const Element& e, double t, double p) {
    DielectricProperties d;
    const char* sym = e.symbol;
    std::string key = sym;
    if (e.is_diatomic()) key += "2";
    const bool gas = e.standard_state == Phase::Gas;
    if (e.is_metal() || e.atomic_number == 33 || e.atomic_number == 51) {
        d.conductor = true;
        const TransportProperties tr = element_transport(e, t, 60.0);
        if (finite(tr.plasma_frequency) && finite(tr.relaxation_time)) {
            const double w = 2.0 * PI * SPEED_OF_LIGHT / 550e-9;
            const double wt = w * tr.relaxation_time;
            const double e1 = 1.0 - tr.plasma_frequency * tr.plasma_frequency * tr.relaxation_time * tr.relaxation_time / (1.0 + wt * wt);
            const double e2 = tr.plasma_frequency * tr.plasma_frequency * tr.relaxation_time / (w * (1.0 + wt * wt));
            const double mag = std::sqrt(e1 * e1 + e2 * e2);
            const double nn = std::sqrt(std::max(0.0, (mag + e1) / 2.0));
            const double kk = std::sqrt(std::max(0.0, (mag - e1) / 2.0));
            d.optical_permittivity = e1;
            d.reflectance = ((nn - 1.0) * (nn - 1.0) + kk * kk) / ((nn + 1.0) * (nn + 1.0) + kk * kk);
            d.optical_penetration_depth = kk > 0 ? 550e-9 / (4.0 * PI * kk) : NO_VALUE;
        }
        d.method = "free-electron (Drude) metal";
        d.notes.push_back("a conductor: free electrons cancel any steady field inside, so its static permittivity is effectively infinite");
        d.notes.push_back("below the plasma frequency the permittivity is negative, which is why metals reflect visible light");
        return d;
    }
    double alpha_a3 = finite(e.polarizability) ? e.polarizability * BOHR3_TO_A3 : NO_VALUE;
    if (e.is_diatomic() && finite(alpha_a3)) alpha_a3 *= 2.0;
    d.polarizability_volume = alpha_a3;
    const double alpha = polarizability_volume_to_si(alpha_a3);
    const DielectricEntry* tab = find_dielectric(key);
    if (!tab && e.atomic_number == 6) tab = find_dielectric("diamond");
    if (gas) {
        const double n = p / (BOLTZMANN_CONSTANT * t);
        const double est = debye_gas_permittivity(n, alpha, 0.0, t);
        if (tab && std::fabs(t - 298.15) < 30.0 && std::fabs(p - 101325.0) < 5000.0) {
            d.relative_permittivity = tab->eps;
            d.refractive_index = tab->n;
            d.method = "measured gas value at room conditions";
        } else {
            d.relative_permittivity = est;
            d.high_frequency_permittivity = est;
            d.permittivity_estimated = true;
            d.method = "ideal gas: \xCE\xB5 = 1 + N\xCE\xB1/\xCE\xB5\xE2\x82\x80";
        }
        d.dielectric_strength = tab ? tab->strength : NO_VALUE;
    } else if (tab) {
        d.relative_permittivity = tab->eps;
        d.refractive_index = tab->n;
        d.dielectric_strength = tab->strength;
        d.method = e.atomic_number == 6 ? "measured (diamond; graphite conducts)" : "measured";
    } else {
        const double vm = molar_volume_m3(e.atomic_weight, e.density * 1000.0);
        const double per = e.is_diatomic() ? 0.5 : 1.0;
        const double n = finite(vm) ? per * AVOGADRO_CONSTANT / vm : NO_VALUE;
        d.relative_permittivity = clausius_mossotti_permittivity(n, alpha);
        d.high_frequency_permittivity = d.relative_permittivity;
        d.permittivity_estimated = true;
        d.method = "Clausius\xE2\x80\x93Mossotti from the atomic polarizability";
    }
    fill_optics(d);
    return d;
}

DielectricProperties metal_dielectric(const MetallicCrystal& c, double wavelength) {
    DielectricProperties d;
    d.conductor = true;
    if (!c.ok) return d;
    const TransportProperties tr = metal_transport(c, 298.15, 60.0);
    if (finite(tr.plasma_frequency) && finite(tr.relaxation_time) && wavelength > 0) {
        const double w = 2.0 * PI * SPEED_OF_LIGHT / wavelength;
        const double wt = w * tr.relaxation_time;
        const double e1 = 1.0 - tr.plasma_frequency * tr.plasma_frequency * tr.relaxation_time * tr.relaxation_time / (1.0 + wt * wt);
        const double e2 = tr.plasma_frequency * tr.plasma_frequency * tr.relaxation_time / (w * (1.0 + wt * wt));
        const double mag = std::sqrt(e1 * e1 + e2 * e2);
        const double nn = std::sqrt(std::max(0.0, (mag + e1) / 2.0));
        const double kk = std::sqrt(std::max(0.0, (mag - e1) / 2.0));
        d.optical_permittivity = e1;
        d.reflectance = ((nn - 1.0) * (nn - 1.0) + kk * kk) / ((nn + 1.0) * (nn + 1.0) + kk * kk);
        d.optical_penetration_depth = kk > 0 ? wavelength / (4.0 * PI * kk) : NO_VALUE;
    }
    d.method = "free-electron (Drude) metal";
    d.notes.push_back("a conductor: its static permittivity is effectively infinite, and visible light is mostly reflected");
    d.notes.push_back("copper, gold and brass look colored because bound d electrons absorb blue light, which the free-electron model leaves out");
    return d;
}

DielectricProperties ionic_dielectric(const IonicCrystal& c) {
    DielectricProperties d;
    if (!c.ok) return d;
    const DielectricEntry* tab = find_dielectric(c.formula);
    if (!tab) tab = find_dielectric(c.name);
    double a3 = 0.0;
    bool have = true;
    const auto ion_alpha = [&](const IonDescriptor& ion) {
        if (ion.element && !ion.polyatomic) return ion_polarizability_volume(*ion.element, ion.charge);
        try {
            return molecular_polarizability_volume(ion.composition());
        } catch (...) {
            return NO_VALUE;
        }
    };
    const double ac = ion_alpha(c.cation), aa = ion_alpha(c.anion);
    if (finite(ac) && finite(aa)) a3 = c.cation_count * ac + c.anion_count * aa;
    else have = false;
    if (have) d.polarizability_volume = a3;
    const double n = c.molar_mass > 0 && c.density > 0 ? c.density * 1000.0 / (c.molar_mass / 1000.0) * AVOGADRO_CONSTANT : NO_VALUE;
    if (tab) {
        d.relative_permittivity = tab->eps;
        d.refractive_index = tab->n;
        d.dielectric_strength = tab->strength;
        d.method = "measured";
    } else if (have) {
        d.high_frequency_permittivity = clausius_mossotti_permittivity(n, polarizability_volume_to_si(a3));
        if (finite(d.high_frequency_permittivity)) d.relative_permittivity = 2.6 * d.high_frequency_permittivity;
        d.permittivity_estimated = true;
        d.method = "Clausius\xE2\x80\x93Mossotti from ion polarizabilities, with the ions' own displacement added";
    }
    if (!finite(d.dielectric_strength)) {
        d.dielectric_strength = 2e7;
        d.notes.push_back("dielectric strength is a typical value for ionic crystals; real samples fail sooner at cracks and impurities");
    }
    fill_optics(d);
    d.notes.push_back("the static permittivity is higher than n\xC2\xB2 because in a slow field the ions themselves shift, not just their electron clouds");
    return d;
}

DielectricProperties molecular_dielectric(const std::string& name, const Formula& f, double dipole_debye, Phase phase, double density, double t, double p) {
    DielectricProperties d;
    const DielectricEntry* tab = find_dielectric(name);
    if (!tab) tab = find_dielectric(f.source());
    if (!tab) tab = find_dielectric(f.to_string());
    d.polarizability_volume = molecular_polarizability_volume(f);
    const double alpha = polarizability_volume_to_si(d.polarizability_volume);
    const double mu = finite(dipole_debye) ? dipole_debye * DEBYE : 0.0;
    const bool tab_is_gas = tab && tab->eps < 1.01;
    if (phase == Phase::Gas) {
        const double n = p / (BOLTZMANN_CONSTANT * t);
        if (tab && tab_is_gas && std::fabs(t - 298.15) < 30.0 && std::fabs(p - 101325.0) < 5000.0) {
            d.relative_permittivity = tab->eps;
            d.refractive_index = tab->n;
            d.method = "measured gas value";
        } else {
            d.relative_permittivity = debye_gas_permittivity(n, alpha, mu, t);
            d.high_frequency_permittivity = debye_gas_permittivity(n, alpha, 0.0, t);
            d.permittivity_estimated = true;
            d.method = mu > 0 ? "Debye equation for a polar gas" : "ideal gas: \xCE\xB5 = 1 + N\xCE\xB1/\xCE\xB5\xE2\x82\x80";
        }
        if (tab) d.dielectric_strength = tab->strength;
    } else if (tab && !tab_is_gas) {
        d.relative_permittivity = tab->eps;
        d.refractive_index = tab->n;
        d.dielectric_strength = tab->strength;
        d.method = "measured at 25 \xC2\xB0" "C";
    } else {
        const double n = density > 0 && f.molar_mass() > 0 ? density / (f.molar_mass() / 1000.0) * AVOGADRO_CONSTANT : NO_VALUE;
        d.relative_permittivity = onsager_permittivity(n, alpha, mu, t);
        d.high_frequency_permittivity = clausius_mossotti_permittivity(n, alpha);
        d.permittivity_estimated = true;
        d.method = mu > 0 ? "Onsager equation (polar liquid)" : "Clausius\xE2\x80\x93Mossotti (nonpolar)";
        if (mu > 0) d.notes.push_back("hydrogen-bonded liquids line their dipoles up with each other, so the real value can be two or three times this estimate");
    }
    fill_optics(d);
    if (finite(d.relative_permittivity) && d.relative_permittivity > 15.0 && phase == Phase::Liquid) d.notes.push_back("a high permittivity weakens the pull between dissolved ions by the same factor, which is why salts dissolve in it");
    return d;
}

SusceptibilityInfo element_susceptibility(const Element& e, double t) {
    SusceptibilityInfo s;
    const double rho = e.density > 0 ? e.density * 1000.0 : NO_VALUE;
    double vm = e.standard_state == Phase::Gas ? BOLTZMANN_CONSTANT * t / 101325.0 * AVOGADRO_CONSTANT : molar_volume_m3(e.atomic_weight, rho);
    if (e.is_diatomic() && e.standard_state == Phase::Gas) vm *= 0.5;
    const bool free_atom = e.standard_state == Phase::Gas && !e.is_diatomic();
    const bool local = free_atom || e.atomic_number == 8 || (e.is_metal() && (e.block == Block::D || e.block == Block::F));
    s.unpaired_electrons = e.atomic_number == 8 ? 2 : (local ? e.unpaired_electrons() : 0);
    const double tab = tabulated_molar_susceptibility_cgs(e.atomic_number);
    if (finite(tab) && tab < 0.0) s.unpaired_electrons = 0;
    if (finite(tab)) {
        s.molar_susceptibility_cgs = tab;
        s.method = "measured (CRC, room temperature)";
        if (e.atomic_number == 8 && std::fabs(t - 298.15) > 30.0) {
            s.molar_susceptibility_cgs = tab * 298.15 / t;
            s.method = "measured, scaled by Curie's law (1/T)";
        }
    } else if (e.is_metal()) {
        const ElectronGasProperties g = element_electron_gas(e, t);
        if (finite(g.pauli_susceptibility) && finite(vm)) {
            s.molar_susceptibility_cgs = g.pauli_susceptibility * vm / (4.0 * PI * 1e-6);
            s.estimated = true;
            s.method = "Pauli paramagnetism of the conduction electrons";
        }
        if (s.unpaired_electrons > 0 && (e.block == Block::D || e.block == Block::F)) {
            const double cur = curie_molar_susceptibility(spin_only_moment(s.unpaired_electrons), t);
            s.molar_susceptibility_cgs = (finite(s.molar_susceptibility_cgs) ? s.molar_susceptibility_cgs : 0.0) + cur / (4.0 * PI * 1e-6) * 0.1;
            s.method = "Pauli plus partly localized d/f moments (rough)";
        }
    } else if (s.unpaired_electrons > 0 && e.standard_state == Phase::Gas && !e.is_diatomic()) {
        s.molar_susceptibility_cgs = curie_molar_susceptibility(spin_only_moment(s.unpaired_electrons), t) / (4.0 * PI * 1e-6);
        s.estimated = true;
        s.method = "Curie law for free atoms";
    } else {
        s.molar_susceptibility_cgs = -0.6e-6 * e.atomic_number;
        s.estimated = true;
        s.method = "Larmor diamagnetism, roughly \xE2\x88\x92" "0.6\xC3\x97" "10\xE2\x81\xBB\xE2\x81\xB6 Z cm\xC2\xB3/mol";
    }
    if (s.unpaired_electrons > 0) {
        s.effective_moment = spin_only_moment(s.unpaired_electrons);
        s.curie_constant = curie_molar_susceptibility(s.effective_moment, 1.0);
    }
    if (finite(s.molar_susceptibility_cgs)) {
        s.molar_susceptibility = 4.0 * PI * 1e-6 * s.molar_susceptibility_cgs;
        if (finite(vm) && vm > 0) s.volume_susceptibility = s.molar_susceptibility / vm;
        if (e.atomic_weight > 0) s.mass_susceptibility = s.molar_susceptibility / (e.atomic_weight / 1000.0);
    }
    const MagneticInfo mag = magnetic_info(e, t);
    if (mag.order == MagneticOrder::Ferromagnetic) {
        s.volume_susceptibility = mag.relative_permeability - 1.0;
        if (finite(vm)) s.molar_susceptibility = s.volume_susceptibility * vm;
        s.molar_susceptibility_cgs = finite(s.molar_susceptibility) ? s.molar_susceptibility / (4.0 * PI * 1e-6) : NO_VALUE;
        if (e.atomic_weight > 0 && finite(s.molar_susceptibility)) s.mass_susceptibility = s.molar_susceptibility / (e.atomic_weight / 1000.0);
        s.estimated = true;
        s.method = "ferromagnetic: \xCF\x87 = \xCE\xBC\xE1\xB5\xA3 \xE2\x88\x92 1 for a typical soft sample in a weak field (it depends on the field and the sample's history)";
    }
    return s;
}

SusceptibilityInfo ionic_susceptibility(const IonicCrystal& c, double t) {
    SusceptibilityInfo s;
    if (!c.ok) return s;
    int unpaired = 0;
    if (c.cation.element && !c.cation.polyatomic && (c.cation.element->block == Block::D || c.cation.element->block == Block::F)) {
        const int valence = c.cation.element->block == Block::D ? c.cation.element->group : 0;
        if (c.cation.element->block == Block::D) {
            const int d = std::max(0, std::min(10, valence - c.cation.charge));
            unpaired = d <= 5 ? d : 10 - d;
        } else {
            const int f = std::max(0, std::min(14, c.cation.element->atomic_number - 57 + 3 - c.cation.charge));
            unpaired = f <= 7 ? f : 14 - f;
        }
    }
    s.unpaired_electrons = unpaired * c.cation_count;
    double electrons = 0.0;
    try {
        for (const auto& comp : Formula::parse(c.formula).composition()) electrons += comp.first->atomic_number * comp.second;
    } catch (...) {
    }
    const double dia = -0.4e-6 * electrons;
    if (unpaired > 0) {
        s.effective_moment = spin_only_moment(unpaired);
        const double para = c.cation_count * curie_molar_susceptibility(s.effective_moment, t) / (4.0 * PI * 1e-6);
        s.molar_susceptibility_cgs = para + dia;
        s.curie_constant = c.cation_count * curie_molar_susceptibility(s.effective_moment, 1.0);
        s.method = "Curie law, spin-only moment of the high-spin cation";
    } else {
        s.molar_susceptibility_cgs = dia;
        s.method = "diamagnetic: every electron paired";
    }
    s.estimated = true;
    s.molar_susceptibility = 4.0 * PI * 1e-6 * s.molar_susceptibility_cgs;
    const double vm = molar_volume_m3(c.molar_mass, c.density * 1000.0);
    if (finite(vm)) s.volume_susceptibility = s.molar_susceptibility / vm;
    if (c.molar_mass > 0) s.mass_susceptibility = s.molar_susceptibility / (c.molar_mass / 1000.0);
    return s;
}

SusceptibilityInfo metal_susceptibility(const MetallicCrystal& c, double t) {
    SusceptibilityInfo s;
    if (!c.ok) return s;
    double chi = 0.0, w = 0.0;
    for (const AlloyComponent& a : c.components) {
        const SusceptibilityInfo e = element_susceptibility(*a.element, t);
        if (finite(e.molar_susceptibility_cgs)) {
            chi += a.fraction * e.molar_susceptibility_cgs;
            w += a.fraction;
        }
        if (e.estimated) s.estimated = true;
    }
    if (w > 0.5) s.molar_susceptibility_cgs = chi / w;
    s.method = c.components.size() > 1 ? "atom-fraction average of the elements" : "as the element";
    if (finite(s.molar_susceptibility_cgs)) {
        s.molar_susceptibility = 4.0 * PI * 1e-6 * s.molar_susceptibility_cgs;
        const double vm = molar_volume_m3(c.molar_mass, c.density * 1000.0);
        if (finite(vm)) s.volume_susceptibility = s.molar_susceptibility / vm;
        if (c.molar_mass > 0) s.mass_susceptibility = s.molar_susceptibility / (c.molar_mass / 1000.0);
    }
    return s;
}

} // namespace chemistry
} // namespace fizmo
