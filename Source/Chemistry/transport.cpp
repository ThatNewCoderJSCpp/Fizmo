#include "transport.hpp"

#include "periodic_table.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

struct ResistivityEntry {
    int z;
    double rho_e8;
    double alpha;
};

constexpr ResistivityEntry RESISTIVITY[] = {
    { 3, 9.28, 0.0045 }, { 4, 3.6, 0.025 }, { 11, 4.77, 0.0055 }, { 12, 4.39, 0.0041 }, { 13, 2.65, 0.0039 },
    { 19, 7.2, 0.0057 }, { 20, 3.36, 0.0041 }, { 21, 56.2, 0.0028 }, { 22, 42.0, 0.0038 }, { 23, 19.7, 0.0039 },
    { 24, 12.5, 0.003 }, { 25, 144.0, 0.0004 }, { 26, 9.71, 0.005 }, { 27, 6.24, 0.0066 }, { 28, 6.99, 0.006 },
    { 29, 1.68, 0.00404 }, { 30, 5.9, 0.0037 }, { 31, 14.0, 0.004 }, { 37, 12.8, 0.0048 }, { 38, 13.2, 0.0035 },
    { 39, 59.6, 0.0027 }, { 40, 42.1, 0.0044 }, { 41, 15.2, 0.0026 }, { 42, 5.34, 0.0046 }, { 43, 20.0, 0.0035 },
    { 44, 7.1, 0.0041 }, { 45, 4.3, 0.0044 }, { 46, 10.5, 0.0038 }, { 47, 1.59, 0.0038 }, { 48, 6.8, 0.0042 },
    { 49, 8.4, 0.0047 }, { 50, 10.9, 0.0045 }, { 51, 41.7, 0.0051 }, { 55, 20.5, 0.0048 }, { 56, 33.2, 0.0061 },
    { 57, 61.5, 0.0022 }, { 58, 74.4, 0.0009 }, { 64, 131.0, 0.0017 }, { 72, 33.1, 0.0044 }, { 73, 13.1, 0.0038 },
    { 74, 5.6, 0.0045 }, { 75, 19.3, 0.0045 }, { 76, 8.1, 0.0042 }, { 77, 4.7, 0.0041 }, { 78, 10.6, 0.00392 },
    { 79, 2.44, 0.0034 }, { 80, 96.1, 0.0009 }, { 81, 18.0, 0.0052 }, { 82, 22.0, 0.0039 }, { 83, 129.0, 0.0046 },
    { 90, 15.7, 0.0028 }, { 92, 28.0, 0.0028 }, { 94, 146.0, -0.0002 }, { 6, 1375.0, -0.0005 }, { 33, 33.3, 0.0039 },
};

struct GapEntry {
    int z;
    double gap;
};

constexpr GapEntry BAND_GAPS[] = {
    { 5, 1.5 }, { 6, 5.47 }, { 14, 1.12 }, { 32, 0.67 }, { 34, 1.74 }, { 52, 0.33 }, { 15, 0.3 }, { 16, 2.6 }, { 53, 1.3 },
};

constexpr double LORENZ = 2.44e-8;
constexpr double MU0 = 1.25663706212e-6;

bool finite(double v) {
    return v == v && std::isfinite(v);
}

double atoms_volume_m3(double molar_mass_g, double density_g_cm3) {
    if (!(molar_mass_g > 0) || !(density_g_cm3 > 0)) return NO_VALUE;
    return molar_mass_g / density_g_cm3 * 1e-6;
}

void fill_drude(TransportProperties& t, double n, double vf) {
    if (!(n > 0) || !(t.resistivity > 0)) return;
    t.carrier_density = n;
    t.relaxation_time = ELECTRON_MASS / (n * ELEMENTARY_CHARGE * ELEMENTARY_CHARGE * t.resistivity);
    t.mobility = ELEMENTARY_CHARGE * t.relaxation_time / ELECTRON_MASS;
    t.drift_velocity_per_volt_per_m = t.mobility;
    if (vf > 0) t.mean_free_path = vf * t.relaxation_time;
    t.hall_coefficient = -1.0 / (n * ELEMENTARY_CHARGE);
    t.plasma_frequency = std::sqrt(n * ELEMENTARY_CHARGE * ELEMENTARY_CHARGE / (VACUUM_PERMITTIVITY * ELECTRON_MASS));
    t.plasma_wavelength = 2.0 * PI * SPEED_OF_LIGHT / t.plasma_frequency;
}

double resistivity_at(double rho293, double alpha, double t, double debye) {
    if (!(rho293 > 0)) return NO_VALUE;
    const double a = finite(alpha) ? alpha : 0.004;
    const double tlin = finite(debye) ? debye / 3.0 : 100.0;
    double rho = 0.0;
    if (t >= tlin || a <= 0.0) {
        rho = rho293 * (1.0 + a * (t - 293.15));
    } else {
        const double at_lin = rho293 * (1.0 + a * (tlin - 293.15));
        rho = at_lin * std::pow(std::max(t, 0.0) / tlin, 5.0);
    }
    return std::max(rho, 0.01 * rho293);
}

} // namespace

const char* to_string(MagneticOrder m) noexcept {
    switch (m) {
        case MagneticOrder::Diamagnetic: return "diamagnetic";
        case MagneticOrder::Paramagnetic: return "paramagnetic";
        case MagneticOrder::Ferromagnetic: return "ferromagnetic";
        case MagneticOrder::Antiferromagnetic: return "antiferromagnetic";
        default: return "ferrimagnetic";
    }
}

const char* to_string(ConductorKind k) noexcept {
    switch (k) {
        case ConductorKind::Metal: return "metal (electrons carry current)";
        case ConductorKind::Semimetal: return "semimetal";
        case ConductorKind::Semiconductor: return "semiconductor";
        case ConductorKind::Insulator: return "insulator";
        default: return "ionic conductor (ions carry current)";
    }
}

double tabulated_resistivity(int z) noexcept {
    for (const ResistivityEntry& e : RESISTIVITY) {
        if (e.z == z) return e.rho_e8 * 1e-8;
    }
    return NO_VALUE;
}

double resistivity_temperature_coefficient(int z) noexcept {
    for (const ResistivityEntry& e : RESISTIVITY) {
        if (e.z == z) return e.alpha;
    }
    return NO_VALUE;
}

double semiconductor_band_gap(int z) noexcept {
    for (const GapEntry& e : BAND_GAPS) {
        if (e.z == z) return e.gap;
    }
    return NO_VALUE;
}

MagneticInfo magnetic_info(const Element& e, double t) {
    MagneticInfo m;
    struct Ordered {
        int z;
        MagneticOrder order;
        double tc;
        double mu;
    };
    static const Ordered ordered[] = {
        { 26, MagneticOrder::Ferromagnetic, 1043.0, 200.0 }, { 27, MagneticOrder::Ferromagnetic, 1388.0, 70.0 },
        { 28, MagneticOrder::Ferromagnetic, 627.0, 110.0 }, { 64, MagneticOrder::Ferromagnetic, 293.0, 50.0 },
        { 65, MagneticOrder::Ferromagnetic, 219.0, 20.0 }, { 66, MagneticOrder::Ferromagnetic, 85.0, 20.0 },
        { 24, MagneticOrder::Antiferromagnetic, 311.0, 1.0 }, { 25, MagneticOrder::Antiferromagnetic, 95.0, 1.0 },
        { 8, MagneticOrder::Antiferromagnetic, 24.0, 1.0 },
    };
    for (const Ordered& o : ordered) {
        if (o.z != e.atomic_number) continue;
        m.ordering_temperature = o.tc;
        if (t < o.tc) {
            m.order = o.order;
            m.relative_permeability = o.mu;
            m.note = o.order == MagneticOrder::Ferromagnetic ? "magnetic domains line up below the Curie temperature" : "neighboring spins point opposite ways below the N\xC3\xA9" "el temperature";
        } else {
            m.order = MagneticOrder::Paramagnetic;
            m.note = std::string("above its ") + (o.order == MagneticOrder::Ferromagnetic ? "Curie" : "N\xC3\xA9" "el") + " temperature the spins are disordered";
        }
        return m;
    }
    static const int diamagnetic[] = { 1, 2, 4, 6, 7, 10, 14, 15, 16, 17, 18, 29, 30, 31, 32, 33, 34, 35, 36, 47, 48, 49, 51, 52, 53, 54, 79, 80, 81, 82, 83, 86 };
    for (int z : diamagnetic) {
        if (z == e.atomic_number) {
            m.order = MagneticOrder::Diamagnetic;
            m.note = e.atomic_number == 83 ? "bismuth is one of the most strongly diamagnetic elements" : "all electrons are paired, so a magnet weakly repels it";
            return m;
        }
    }
    if (e.atomic_number == 8) {
        m.order = MagneticOrder::Paramagnetic;
        m.note = "O\xE2\x82\x82 has two unpaired electrons (molecular orbital theory), so liquid oxygen sticks to a magnet";
        return m;
    }
    m.order = MagneticOrder::Paramagnetic;
    m.note = e.is_metal() ? "Pauli paramagnetism from the conduction electrons, plus any unpaired d or f electrons" : "unpaired electrons make it weakly attracted to a magnet";
    return m;
}

double debye_temperature_estimate(double tm, double molar_mass_g, double molar_volume_m3) noexcept {
    if (!(tm > 0) || !(molar_mass_g > 0) || !(molar_volume_m3 > 0)) return NO_VALUE;
    const double v_cm3 = molar_volume_m3 * 1e6;
    return 137.0 * std::sqrt(tm / (molar_mass_g * std::pow(v_cm3, 2.0 / 3.0)));
}

double debye_heat_capacity(double t, double theta) noexcept {
    if (!(t > 0) || !(theta > 0)) return NO_VALUE;
    const double xmax = theta / t;
    if (xmax < 1e-3) return 3.0 * GAS_CONSTANT;
    const int n = 400;
    double sum = 0.0;
    for (int i = 1; i <= n; ++i) {
        const double x = xmax * (i - 0.5) / n;
        const double ex = std::exp(std::min(x, 700.0));
        sum += x * x * x * x * ex / ((ex - 1.0) * (ex - 1.0));
    }
    sum *= xmax / n;
    return 9.0 * GAS_CONSTANT * std::pow(t / theta, 3.0) * sum;
}

double skin_depth(double resistivity, double frequency, double mu_r) noexcept {
    if (!(resistivity > 0) || !(frequency > 0)) return NO_VALUE;
    return std::sqrt(2.0 * resistivity / (2.0 * PI * frequency * MU0 * std::max(1.0, mu_r)));
}

double wiedemann_franz_conductivity(double resistivity, double t) noexcept {
    if (!(resistivity > 0) || !(t > 0)) return NO_VALUE;
    return LORENZ * t / resistivity;
}

TransportProperties element_transport(const Element& e, double t, double frequency) {
    TransportProperties out;
    out.temperature = t;
    out.frequency = frequency;
    out.magnetic = magnetic_info(e, t);
    const double vm = atoms_volume_m3(e.atomic_weight, e.density);
    out.debye_temperature = debye_temperature_estimate(e.melting_point, e.atomic_weight, vm);
    out.dulong_petit = 3.0 * GAS_CONSTANT;
    out.lattice_heat_capacity = debye_heat_capacity(t, out.debye_temperature);
    const double rho293 = tabulated_resistivity(e.atomic_number);
    const double gap = semiconductor_band_gap(e.atomic_number);
    if (e.atomic_number == 6) {
        out.kind = ConductorKind::Semimetal;
        out.resistivity = rho293;
        out.resistivity_293 = rho293;
        out.band_gap = gap;
        out.thermal_conductivity = e.thermal_conductivity;
        out.notes.push_back("graphite conducts along its layers like a semimetal; diamond, the other form of carbon, is an insulator with a 5.5 eV band gap");
    } else if (e.is_metal() || (finite(rho293) && rho293 < 1e-5)) {
        out.kind = e.atomic_number == 83 || e.atomic_number == 51 || e.atomic_number == 33 ? ConductorKind::Semimetal : ConductorKind::Metal;
        out.resistivity_293 = finite(rho293) ? rho293 : (e.thermal_conductivity > 0 ? LORENZ * 293.15 / e.thermal_conductivity : NO_VALUE);
        out.temperature_coefficient = finite(resistivity_temperature_coefficient(e.atomic_number)) ? resistivity_temperature_coefficient(e.atomic_number) : 0.004;
        out.resistivity = resistivity_at(out.resistivity_293, out.temperature_coefficient, t, out.debye_temperature);
        const int valence = free_electron_valence(e);
        const double n = finite(vm) ? valence * AVOGADRO_CONSTANT / vm : NO_VALUE;
        double vf = NO_VALUE;
        if (n > 0) {
            const double kf = std::cbrt(3.0 * PI * PI * n);
            vf = REDUCED_PLANCK_CONSTANT * kf / ELECTRON_MASS;
            const double ef = REDUCED_PLANCK_CONSTANT * REDUCED_PLANCK_CONSTANT * kf * kf / (2.0 * ELECTRON_MASS);
            out.electronic_heat_capacity = PI * PI / 2.0 * valence * GAS_CONSTANT * BOLTZMANN_CONSTANT * t / ef;
        }
        fill_drude(out, n, vf);
        out.thermal_conductivity = e.thermal_conductivity > 0 && std::fabs(t - 298.15) < 30.0 ? e.thermal_conductivity : wiedemann_franz_conductivity(out.resistivity, t);
        out.skin_depth = skin_depth(out.resistivity, frequency, out.magnetic.relative_permeability);
        out.lorenz_number = LORENZ;
    } else if (finite(gap) && gap > 3.0) {
        out.kind = ConductorKind::Insulator;
        out.band_gap = gap;
        out.thermal_conductivity = e.thermal_conductivity;
        out.notes.push_back("the band gap is too wide for heat to free any electrons, so it insulates");
    } else if (finite(gap)) {
        out.kind = gap < 0.1 ? ConductorKind::Semimetal : ConductorKind::Semiconductor;
        out.band_gap = gap;
        const double nc = 2.5e25 * std::pow(t / 300.0, 1.5);
        const double ni = nc * std::exp(-gap * ELECTRONVOLT / (2.0 * BOLTZMANN_CONSTANT * t));
        out.carrier_density = 2.0 * ni;
        const double mu = 0.1;
        out.conductivity = ni * ELEMENTARY_CHARGE * mu * 2.0;
        out.resistivity = out.conductivity > 0 ? 1.0 / out.conductivity : NO_VALUE;
        out.thermal_conductivity = e.thermal_conductivity;
        out.notes.push_back("intrinsic carriers only: real samples are usually doped, which raises the conductivity a lot");
        out.notes.push_back("conductivity rises with temperature as electrons are kicked across the band gap, the opposite of a metal");
    } else {
        out.kind = ConductorKind::Insulator;
        out.thermal_conductivity = e.thermal_conductivity;
    }
    if (finite(out.resistivity) && !finite(out.conductivity)) out.conductivity = 1.0 / out.resistivity;
    out.molar_heat_capacity = e.molar_heat_capacity > 0 && std::fabs(t - 298.15) < 30.0 ? e.molar_heat_capacity : (finite(out.lattice_heat_capacity) ? out.lattice_heat_capacity + (finite(out.electronic_heat_capacity) ? out.electronic_heat_capacity : 0.0) : e.molar_heat_capacity);
    if (finite(out.molar_heat_capacity) && e.atomic_weight > 0) out.specific_heat = out.molar_heat_capacity / (e.atomic_weight / 1000.0);
    if (finite(out.thermal_conductivity) && finite(out.specific_heat) && e.density > 0) out.thermal_diffusivity = out.thermal_conductivity / (e.density * 1000.0 * out.specific_heat);
    if (e.standard_state == Phase::Gas) out.notes.push_back("a gas at room conditions: the solid-state numbers apply only when it's frozen");
    return out;
}

TransportProperties metal_transport(const MetallicCrystal& c, double t, double frequency) {
    TransportProperties out;
    out.temperature = t;
    out.frequency = frequency;
    out.kind = ConductorKind::Metal;
    if (!c.ok || c.components.empty()) return out;
    const double vm = atoms_volume_m3(c.molar_mass, c.density);
    out.debye_temperature = debye_temperature_estimate(c.melting_point, c.molar_mass, vm);
    out.dulong_petit = 3.0 * GAS_CONSTANT;
    out.lattice_heat_capacity = debye_heat_capacity(t, out.debye_temperature);
    if (c.fermi_temperature > 0 && finite(c.valence_electrons_per_atom)) out.electronic_heat_capacity = PI * PI / 2.0 * c.valence_electrons_per_atom * GAS_CONSTANT * t / c.fermi_temperature;
    double rho = 0.0, alpha = 0.0, weight = 0.0, mixing = 0.0;
    const AlloyComponent* host = &c.components.front();
    for (const AlloyComponent& a : c.components) {
        if (a.fraction > host->fraction) host = &a;
        if (!a.element->is_metal() && c.components.size() > 1) continue;
        double r = tabulated_resistivity(a.element->atomic_number);
        if (!finite(r)) r = a.element->thermal_conductivity > 0 ? LORENZ * 293.15 / a.element->thermal_conductivity : 5e-7;
        double al = resistivity_temperature_coefficient(a.element->atomic_number);
        if (!finite(al)) al = 0.004;
        rho += a.fraction * r;
        alpha += a.fraction * al;
        weight += a.fraction;
    }
    for (std::size_t i = 0; i < c.components.size(); ++i) {
        for (std::size_t j = i + 1; j < c.components.size(); ++j) mixing += c.components[i].fraction * c.components[j].fraction;
    }
    const double nordheim = c.type == AlloyType::Interstitial ? 2.5e-6 : (c.type == AlloyType::Intermetallic ? 4e-7 : 1.6e-7);
    out.resistivity_293 = rho / std::max(weight, 1e-12) + nordheim * mixing;
    out.temperature_coefficient = alpha / std::max(weight, 1e-12) * (rho / std::max(weight, 1e-12)) / out.resistivity_293;
    out.resistivity = resistivity_at(out.resistivity_293, out.temperature_coefficient, t, out.debye_temperature);
    out.conductivity = 1.0 / out.resistivity;
    fill_drude(out, c.electron_density, c.fermi_velocity);
    if (c.components.size() == 1 && host->element->thermal_conductivity > 0 && std::fabs(t - 298.15) < 30.0) out.thermal_conductivity = host->element->thermal_conductivity;
    else out.thermal_conductivity = wiedemann_franz_conductivity(out.resistivity, t);
    out.lorenz_number = LORENZ;
    out.magnetic = magnetic_info(*host->element, t);
    if (out.magnetic.order == MagneticOrder::Ferromagnetic && host->fraction < 0.5) {
        out.magnetic.order = MagneticOrder::Paramagnetic;
        out.magnetic.relative_permeability = 1.0;
        out.magnetic.note = "too little of the magnetic metal for its domains to stay connected";
    }
    out.skin_depth = skin_depth(out.resistivity, frequency, out.magnetic.relative_permeability);
    out.molar_heat_capacity = (finite(out.lattice_heat_capacity) ? out.lattice_heat_capacity : out.dulong_petit) + (finite(out.electronic_heat_capacity) ? out.electronic_heat_capacity : 0.0);
    if (c.molar_mass > 0) out.specific_heat = out.molar_heat_capacity / (c.molar_mass / 1000.0);
    if (finite(out.thermal_conductivity) && finite(out.specific_heat) && c.density > 0) out.thermal_diffusivity = out.thermal_conductivity / (c.density * 1000.0 * out.specific_heat);
    if (c.components.size() > 1) out.notes.push_back("mixing metals scatters electrons (Nordheim's rule), so alloys conduct worse than either pure metal");
    out.notes.push_back("thermal conductivity follows from the electrical one through the Wiedemann\xE2\x80\x93" "Franz law, \xCE\xBA = L\xCF\x83T");
    return out;
}

TransportProperties ionic_transport(const IonicCrystal& c, double t) {
    TransportProperties out;
    out.temperature = t;
    if (!c.ok) return out;
    const double atoms = std::max(2.0, Formula::parse(c.formula).atom_count());
    const double vm = atoms_volume_m3(c.molar_mass / atoms, c.density);
    out.debye_temperature = debye_temperature_estimate(c.melting_point, c.molar_mass / atoms, vm);
    out.dulong_petit = 3.0 * GAS_CONSTANT * atoms;
    const double per_atom = debye_heat_capacity(t, out.debye_temperature);
    out.lattice_heat_capacity = finite(per_atom) ? per_atom * atoms : NO_VALUE;
    out.molar_heat_capacity = finite(out.lattice_heat_capacity) ? out.lattice_heat_capacity : out.dulong_petit;
    if (c.molar_mass > 0) out.specific_heat = out.molar_heat_capacity / (c.molar_mass / 1000.0);
    const bool molten = c.melting_point > 0 && t >= c.melting_point;
    out.kind = molten ? ConductorKind::IonicConductor : ConductorKind::Insulator;
    if (molten) {
        out.conductivity = 300.0;
        out.resistivity = 1.0 / out.conductivity;
        out.notes.push_back("melted, the ions can move, so it conducts electricity (this is how aluminum and sodium are made)");
    } else {
        out.notes.push_back("as a solid the ions are locked in place and every electron is held tightly, so it doesn't conduct; melted or dissolved in water it does");
        const double eg = finite(c.electronegativity_difference) ? 1.5 + 2.3 * c.electronegativity_difference : NO_VALUE;
        out.band_gap = eg;
    }
    if (finite(c.lattice_energy())) out.thermal_conductivity = std::max(1.0, 8e-3 * c.lattice_energy() * std::sqrt(298.15 / std::max(t, 1.0)));
    if (finite(out.thermal_conductivity) && finite(out.specific_heat) && c.density > 0) out.thermal_diffusivity = out.thermal_conductivity / (c.density * 1000.0 * out.specific_heat);
    out.notes.push_back("heat moves only by lattice vibrations (phonons), so ionic solids conduct heat far less well than metals");
    out.magnetic.order = MagneticOrder::Diamagnetic;
    if (c.cation.element && c.cation.element->block == Block::D) {
        out.magnetic.order = MagneticOrder::Paramagnetic;
        out.magnetic.note = "transition-metal ions often keep unpaired d electrons";
    }
    return out;
}

} // namespace chemistry
} // namespace fizmo
