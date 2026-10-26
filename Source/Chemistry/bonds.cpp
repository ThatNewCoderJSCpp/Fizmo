#include "bonds.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double PAULING_EV = 96.485332;
const double SCHOMAKER_STEVENSON_PM = 9.0;
const double PAULING_ORDER_PM = 71.0;
const double KAPUSTINSKII_K = 1.2025e-4;
const double KAPUSTINSKII_D = 3.45e-11;
const double E_SQUARED_OVER_4PI_EPS0 = ELEMENTARY_CHARGE * ELEMENTARY_CHARGE * COULOMB_CONSTANT;

double chi_difference(const Element& a, const Element& b) {
    const double xa = a.electronegativity_pauling;
    const double xb = b.electronegativity_pauling;
    if (!has_value(xa) || !has_value(xb)) return NO_VALUE;
    return std::fabs(xa - xb);
}

int born_n_for_electrons(int electrons) {
    if (electrons <= 2) return 5;
    if (electrons <= 10) return 7;
    if (electrons <= 28) return 9;
    if (electrons <= 46) return 10;
    return 12;
}

} // namespace

Bond::Bond(std::string_view first, std::string_view second, BondOrder o) : a(&PeriodicTable::element(first)), b(&PeriodicTable::element(second)), order(o) {}

double Bond::energy() const {
    return bond_energy(*a, *b, order);
}

double Bond::length(BondLengthMethod method) const {
    return bond_length(*a, *b, order, method);
}

double Bond::electronegativity_difference() const noexcept {
    return chi_difference(*a, *b);
}

double Bond::ionic_character() const noexcept {
    const double d = electronegativity_difference();
    return has_value(d) ? percent_ionic_character(d) / 100.0 : NO_VALUE;
}

double Bond::dipole_moment() const {
    return bond_dipole(ionic_character(), length());
}

double Bond::reduced_mass() const noexcept {
    return chemistry::reduced_mass(a->atomic_weight, b->atomic_weight);
}

BondCharacter Bond::character() const noexcept {
    if (a->is_metal() && b->is_metal()) return BondCharacter::Metallic;
    const double d = electronegativity_difference();
    if (!has_value(d)) return BondCharacter::PolarCovalent;
    if (d < 0.4) return BondCharacter::NonpolarCovalent;
    if (d < 1.7) return BondCharacter::PolarCovalent;
    return BondCharacter::Ionic;
}

std::string Bond::to_string() const {
    const char* symbol = order == BondOrder::Double ? "=" : (order == BondOrder::Triple ? "#" : (order == BondOrder::Aromatic ? ":" : "-"));
    return std::string(a->symbol) + symbol + b->symbol;
}

double bond_energy(const Element& a, const Element& b, BondOrder order) {
    if (order == BondOrder::Aromatic) {
        const double single = bond_energy(a, b, BondOrder::Single);
        const double dbl = bond_energy(a, b, BondOrder::Double);
        return 0.5 * (single + dbl);
    }
    const int n = static_cast<int>(order);
    const double table = tabulated_bond_energy(a.atomic_number, b.atomic_number, n);
    if (has_value(table)) return table;
    return n == 1 ? pauling_bond_energy(a, b) : NO_VALUE;
}

double pauling_bond_energy(const Element& a, const Element& b) {
    const double daa = tabulated_bond_energy(a.atomic_number, a.atomic_number, 1);
    const double dbb = tabulated_bond_energy(b.atomic_number, b.atomic_number, 1);
    const double d = chi_difference(a, b);
    if (!has_value(daa) || !has_value(dbb) || !has_value(d)) return NO_VALUE;
    return 0.5 * (daa + dbb) + PAULING_EV * d * d;
}

double covalent_radius(const Element& e, BondOrder order) {
    switch (order) {
        case BondOrder::Double: return value_or(e.covalent_radius_double, e.covalent_radius);
        case BondOrder::Triple: return value_or(e.covalent_radius_triple, value_or(e.covalent_radius_double, e.covalent_radius));
        case BondOrder::Aromatic: return has_value(e.covalent_radius_double) ? 0.5 * (e.covalent_radius + e.covalent_radius_double) : e.covalent_radius;
        default: return e.covalent_radius;
    }
}

double bond_length(const Element& a, const Element& b, BondOrder order, BondLengthMethod method) {
    if (method == BondLengthMethod::Table) {
        if (order == BondOrder::Aromatic && a.atomic_number == 6 && b.atomic_number == 6) return 139.0;
        if (order != BondOrder::Aromatic) {
            const double t = tabulated_bond_length(a.atomic_number, b.atomic_number, static_cast<int>(order));
            if (has_value(t)) return t;
        }
        method = BondLengthMethod::Pyykko;
    }
    if (method == BondLengthMethod::Pyykko) return covalent_radius(a, order) + covalent_radius(b, order);
    if (method == BondLengthMethod::Cordero) return a.covalent_radius_cordero + b.covalent_radius_cordero;
    const double d = value_or(chi_difference(a, b), 0.0);
    const double base = order == BondOrder::Single ? a.covalent_radius_cordero + b.covalent_radius_cordero : covalent_radius(a, order) + covalent_radius(b, order);
    return base - SCHOMAKER_STEVENSON_PM * d;
}

double bond_order_from_length(double single_bond_length_pm, double length_pm) {
    return std::pow(10.0, (single_bond_length_pm - length_pm) / PAULING_ORDER_PM);
}

double length_from_bond_order(double single_bond_length_pm, double bond_order) {
    return single_bond_length_pm - PAULING_ORDER_PM * std::log10(bond_order);
}

double percent_ionic_character(double d) noexcept {
    return 100.0 * (1.0 - std::exp(-0.25 * d * d));
}

double hannay_smyth_ionic_character(double d) noexcept {
    return 16.0 * d + 3.5 * d * d;
}

double bond_dipole(double partial_charge, double length_pm) noexcept {
    return partial_charge * ELEMENTARY_CHARGE * length_pm * PICOMETER / DEBYE;
}

double reaction_enthalpy_from_bonds(const std::vector<std::pair<Bond, double>>& broken, const std::vector<std::pair<Bond, double>>& formed) {
    double total = 0.0;
    for (const auto& b : broken) total += b.second * b.first.energy();
    for (const auto& f : formed) total -= f.second * f.first.energy();
    return total;
}

double reduced_mass(double m1, double m2) noexcept {
    return m1 * m2 / (m1 + m2);
}

double van_der_waals_contact(const Element& a, const Element& b) noexcept {
    return a.van_der_waals_radius + b.van_der_waals_radius;
}

double vibrational_frequency(double force_constant, double reduced_mass_u) noexcept {
    return std::sqrt(force_constant / (reduced_mass_u * ATOMIC_MASS_CONSTANT)) / (2.0 * PI);
}

double force_constant_from_wavenumber(double wavenumber_per_cm, double reduced_mass_u) noexcept {
    const double omega = 2.0 * PI * SPEED_OF_LIGHT * wavenumber_per_cm * 100.0;
    return omega * omega * reduced_mass_u * ATOMIC_MASS_CONSTANT;
}

double gordy_force_constant(const Element& a, const Element& b, double length_pm, double bond_order) noexcept {
    const double d = length_pm / 100.0;
    const double x = a.electronegativity_pauling * b.electronegativity_pauling / (d * d);
    return 100.0 * (1.67 * bond_order * std::pow(x, 0.75) + 0.30);
}

MorsePotential MorsePotential::from_force_constant(double well_depth_j_per_mol, double force_constant_, double equilibrium_m) {
    MorsePotential m;
    m.well_depth = well_depth_j_per_mol / AVOGADRO_CONSTANT;
    m.equilibrium_distance = equilibrium_m;
    m.width = std::sqrt(force_constant_ / (2.0 * m.well_depth));
    return m;
}

MorsePotential MorsePotential::from_bond(const Bond& bond, double force_constant_) {
    const double d = bond.energy() * 1000.0;
    const double r = bond.length();
    const double k = has_value(force_constant_) ? force_constant_ : gordy_force_constant(*bond.a, *bond.b, r, numeric_bond_order(bond.order));
    return from_force_constant(d, k, r * PICOMETER);
}

double MorsePotential::energy(double r) const noexcept {
    const double e = 1.0 - std::exp(-width * (r - equilibrium_distance));
    return well_depth * e * e - well_depth;
}

double MorsePotential::force(double r) const noexcept {
    const double ex = std::exp(-width * (r - equilibrium_distance));
    return -2.0 * well_depth * width * (1.0 - ex) * ex;
}

double MorsePotential::force_constant() const noexcept {
    return 2.0 * well_depth * width * width;
}

double MorsePotential::vibrational_wavenumber(double reduced_mass_u) const noexcept {
    return std::sqrt(force_constant() / (reduced_mass_u * ATOMIC_MASS_CONSTANT)) / (2.0 * PI * SPEED_OF_LIGHT * 100.0);
}

double MorsePotential::vibrational_level(int v, double reduced_mass_u) const noexcept {
    const double quantum = PLANCK_CONSTANT * SPEED_OF_LIGHT * 100.0 * vibrational_wavenumber(reduced_mass_u);
    const double x = quantum * (v + 0.5);
    return x - x * x / (4.0 * well_depth);
}

int MorsePotential::bound_levels(double reduced_mass_u) const noexcept {
    const double mu = reduced_mass_u * ATOMIC_MASS_CONSTANT;
    const double lambda = std::sqrt(2.0 * mu * well_depth) / (width * REDUCED_PLANCK_CONSTANT);
    return static_cast<int>(std::floor(lambda - 0.5)) + 1;
}

double LennardJonesPotential::energy(double r) const noexcept {
    const double s6 = std::pow(sigma / r, 6.0);
    return 4.0 * epsilon * (s6 * s6 - s6);
}

double LennardJonesPotential::force(double r) const noexcept {
    const double s6 = std::pow(sigma / r, 6.0);
    return 24.0 * epsilon / r * (2.0 * s6 * s6 - s6);
}

double LennardJonesPotential::minimum_distance() const noexcept {
    return std::pow(2.0, 1.0 / 6.0) * sigma;
}

double LennardJonesPotential::mix_epsilon(const LennardJonesPotential& o) const noexcept {
    return std::sqrt(epsilon * o.epsilon);
}

double LennardJonesPotential::mix_sigma(const LennardJonesPotential& o) const noexcept {
    return 0.5 * (sigma + o.sigma);
}

double madelung_constant(std::string_view structure) noexcept {
    for (const MadelungEntry& m : madelung_table()) {
        if (equals_ignore_case(structure, m.structure) || equals_ignore_case(structure, m.example)) return m.constant;
    }
    return NO_VALUE;
}

double born_exponent(const Element& cation, int cation_charge, const Element& anion, int anion_charge) {
    const int nc = born_n_for_electrons(cation.atomic_number - cation_charge);
    const int na = born_n_for_electrons(anion.atomic_number - anion_charge);
    return 0.5 * (nc + na);
}

double born_lande_energy(double madelung, int cation_charge, int anion_charge, double distance_pm, double born_n) noexcept {
    const double r = distance_pm * PICOMETER;
    return AVOGADRO_CONSTANT * madelung * std::abs(cation_charge * anion_charge) * E_SQUARED_OVER_4PI_EPS0 / r * (1.0 - 1.0 / born_n) / 1000.0;
}

double born_mayer_energy(double madelung, int cation_charge, int anion_charge, double distance_pm, double rho_pm) noexcept {
    const double r = distance_pm * PICOMETER;
    return AVOGADRO_CONSTANT * madelung * std::abs(cation_charge * anion_charge) * E_SQUARED_OVER_4PI_EPS0 / r * (1.0 - rho_pm / distance_pm) / 1000.0;
}

double kapustinskii_energy(int ions_per_formula, int cation_charge, int anion_charge, double cation_radius_pm, double anion_radius_pm) noexcept {
    const double r = (cation_radius_pm + anion_radius_pm) * PICOMETER;
    return KAPUSTINSKII_K * ions_per_formula * std::abs(cation_charge * anion_charge) / r * (1.0 - KAPUSTINSKII_D / r) / 1000.0;
}

double kapustinskii_energy(const Element& cation, int cation_charge, int cation_count, const Element& anion, int anion_charge, int anion_count) {
    const IonicRadius* rc = cation.ionic_radius(cation_charge, 6);
    const IonicRadius* ra = anion.ionic_radius(anion_charge, 6);
    if (!rc || !ra) return NO_VALUE;
    return kapustinskii_energy(cation_count + anion_count, cation_charge, anion_charge, rc->ionic_radius, ra->ionic_radius);
}

double born_haber_lattice_energy(double formation_enthalpy, double metal_atomization, double ionization_energies, double nonmetal_atomization, double electron_affinities) noexcept {
    return -formation_enthalpy + metal_atomization + ionization_energies + nonmetal_atomization - electron_affinities;
}

} // namespace chemistry
} // namespace fizmo
