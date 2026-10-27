#include <algorithm>
#include "../Source/Chemistry/chemistry.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace fizmo::chemistry;

static_assert(ELEMENT_COUNT == 118, "every element is in the table");
static_assert(element(1).atomic_number == 1 && element(118).atomic_number == 118, "elements are in order");
static_assert(element("Fe").atomic_number == 26, "lookup by symbol");
static_assert(element("iron").atomic_number == 26, "lookup by name");
static_assert(element("aluminium").atomic_number == 13, "lookup by alternate name");
static_assert(element(26).group == 8 && element(26).period == 4 && element(26).block == Block::D, "position of iron");
static_assert(element(71).group == 3 && element(57).group == 0, "lutetium is in group 3, lanthanum is f-block");
static_assert(element("Cl").category == Category::Halogen, "chlorine is a halogen");
static_assert(element("Si").classification == Classification::Metalloid, "silicon is a metalloid");
static_assert(element("He").standard_state == Phase::Gas && element("Br").standard_state == Phase::Liquid && element("Hg").standard_state == Phase::Liquid, "standard states");
static_assert(!element("Tc").is_stable && element("Tc").reference_mass_number == 97, "technetium uses its longest-lived isotope");
static_assert(element("U").has_standard_atomic_weight && !element("U").is_stable, "uranium keeps its natural atomic weight");
static_assert(element("C").isotope(14) != nullptr && element("C").isotope(14)->neutrons() == 8, "carbon-14");
static_assert(isotope("U-235").mass_number == 235 && isotope("235U").atomic_number == 92, "isotope notations");
static_assert(element("O").valence_electrons() == 6 && element("Fe").unpaired_electrons() == 4, "valence electrons");
static_assert(OxidationState(8).value() == 8 && !OxidationState::valid(9) && !OxidationState::valid(-9), "oxidation states run from -8 to +8");
static_assert(Atom(element("Fe"), 3).electrons() == 23, "ions keep track of electrons");
static_assert(Atom(element("O"), -2).oxidation_state() == -2, "monatomic ions take their charge as oxidation state");
static_assert(substance("water").critical_temperature > 647.0, "substance data is constexpr");

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}

bool rel(double a, double b, double tol) {
    return std::fabs(a - b) <= tol * std::fabs(b);
}

std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

void test_elements() {
    check(near(element("H").atomic_weight, 1.008, 1e-12), "conventional atomic weight of hydrogen");
    check(near(element("C").atomic_weight_min, 12.0096, 1e-12) && near(element("C").atomic_weight_max, 12.0116, 1e-12), "carbon atomic weight interval");
    check(near(element("Na").atomic_weight, 22.98976928, 1e-12), "sodium atomic weight");
    check(near(element("Zr").atomic_weight, 91.222, 1e-12), "zirconium uses the 2024 value");
    check(near(element("Tc").atomic_weight, 96.90636, 1e-4), "technetium uses the mass of Tc-97");
    check(near(element("H").ionization_energy(1), 13.598434599702, 1e-9), "hydrogen ionization energy");
    check(element("Fe").ionization_count == 26 && near(element("Fe").ionization_energy(3), 30.651, 1e-3), "every iron ionization energy");
    check(near(element("Cl").electron_affinity, 3.612725, 1e-6), "chlorine electron affinity");
    check(!element("N").forms_stable_anion(), "nitrogen has no stable anion");
    check(near(element("F").electronegativity_pauling, 3.98, 1e-9), "fluorine electronegativity");
    check(element("Fe").ground_term.to_string() == "5D4", "iron ground term");
    check(element("Cu").full_electron_configuration() == "1s2 2s2 2p6 3s2 3p6 3d10 4s1", "copper configuration");
    check(element("Fe").shell_configuration() == "2, 8, 14, 2", "iron shells");
    check(element("Mn").has_oxidation_state(7) && element("Mn").highest_oxidation_state() == 7, "manganese oxidation states");
    check(element("Na").ionic_radius(1, 6) && near(element("Na").ionic_radius(1, 6)->ionic_radius, 102.0, 1e-9), "Shannon radius of Na+");
    check(element("Sn").stable_isotope_count() == 10, "tin has ten stable isotopes");
    check(PeriodicTable::in_group(18).size() == 7, "seven noble gases");
    check(PeriodicTable::find_isotope("tritium") && near(PeriodicTable::find_isotope("tritium")->half_life / NUBASE_YEAR, 12.32, 0.01), "tritium half-life");
}

void test_isotopes() {
    const Isotope& fe56 = isotope("Fe-56");
    check(near(fe56.binding_energy_per_nucleon(), 8.790, 2e-3), "binding energy of iron-56: " + num(fe56.binding_energy_per_nucleon()));
    const Isotope& u238 = isotope("U-238");
    check(near(u238.half_life / NUBASE_YEAR / 1e9, 4.463, 1e-3), "uranium-238 half-life");
    check(u238.primary_decay()->mode == DecayMode::Alpha, "uranium-238 decays by alpha");
    check(near(decay_q_value(u238, *u238.primary_decay()), 4.27, 0.01), "uranium-238 alpha Q value");
    const Isotope& c14 = isotope("C-14");
    check(near(decay_q_value(c14, c14.decays()[0]) * 1000.0, 156.476, 0.01), "carbon-14 beta Q value");
    check(near(u238.specific_activity(), 12445.0, 15.0), "specific activity of uranium-238: " + num(u238.specific_activity()));
    const std::vector<const Isotope*> chain = decay_chain(u238);
    check(chain.back()->atomic_number == 82 && chain.back()->mass_number == 206, "uranium-238 decays down to lead-206");
    check(near(semi_empirical_binding_energy(26, 56) / 56.0, 8.79, 0.1), "semi-empirical binding energy");
    const std::vector<double> n = bateman({ 1.0, 0.5 }, 1.0, 1.0);
    check(near(n[0], std::exp(-1.0), 1e-12) && near(n[1], 2.0 * (std::exp(-0.5) - std::exp(-1.0)), 1e-12), "Bateman equations");
}

void test_ions_and_configurations() {
    check(Atom::parse("Fe3+").electron_configuration().to_noble_gas_string() == "[Ar] 3d5", "Fe3+ loses the 4s electrons first");
    check(Atom::parse("Cu+").electron_configuration().to_noble_gas_string() == "[Ar] 3d10", "Cu+ configuration");
    check(Atom::parse("O2-").electron_configuration().to_noble_gas_string() == "[Ne]", "oxide is neon-like");
    check(Atom::parse("Cr3+").electron_configuration().hund_term().to_string() == "4F3/2", "Cr3+ ground term");
    check(Atom::parse("Fe3+").stock_name() == "iron(III)" && Atom::parse("Cl-").stock_name() == "chloride", "ion names");
    check(Atom::parse("U-235").mass_number() == 235 && Atom::parse("D").mass_number() == 2, "isotopes as atoms");
    check(near(Atom::parse("Fe2+").formation_energy(), element("Fe").ionization_energy(1) + element("Fe").ionization_energy(2), 1e-12), "ionization to Fe2+");
    check(OxidationState(-3).roman() == "-III" && OxidationState(6).to_string() == "+6", "oxidation state text");
    bool threw = false;
    try {
        OxidationState(9);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "oxidation state +9 is rejected");
}

void test_formulas() {
    check(near(Formula("H2O").molar_mass(), 18.015, 1e-9), "molar mass of water");
    check(near(Formula("CuSO4·5H2O").molar_mass(), 249.677, 1e-6), "hydrate molar mass");
    check(Formula("Ca(OH)2").count("H") == 2.0 && Formula("K4[Fe(CN)6]").count("N") == 6.0, "nested groups");
    check(Formula("SO42-").charge() == -2 && Formula("NH4+").charge() == 1 && Formula("[Fe(CN)6]4-").charge() == -4 && Formula("Hg22+").count("Hg") == 2.0, "charges");
    check(Formula("C2H5OH").hill() == "C2H6O", "Hill notation");
    check(Formula("C6H12O6").empirical().to_string() == "CH2O", "empirical formula");
    check(near(Formula("C6H6").degree_of_unsaturation(), 4.0, 1e-12), "degree of unsaturation");
    check(oxidation_state_of(Formula("KMnO4"), element("Mn")) == AverageOxidationState(7), "Mn in permanganate");
    check(oxidation_state_of(Formula("Fe3O4"), element("Fe")) == AverageOxidationState(8, 3), "Fe in magnetite");
    check(oxidation_state_of(Formula("H2O2"), element("O")) == AverageOxidationState(-1), "O in peroxide");
    check(oxidation_state_of(Formula("NaH"), element("H")) == AverageOxidationState(-1), "H in a hydride");
    check(oxidation_state_of(Formula("Cr2O72-"), element("Cr")) == AverageOxidationState(6), "Cr in dichromate");
    check(oxidation_state_of(Formula("FeS2"), element("Fe")) == AverageOxidationState(2), "Fe in pyrite");
}

void test_reactions() {
    Reaction r = Reaction("C3H8 + O2 -> CO2 + H2O").balanced();
    check(r.to_string() == "C3H8 + 5 O2 -> 3 CO2 + 4 H2O", "propane combustion: " + r.to_string());
    check(r.classify() == ReactionType::Combustion, "classified as combustion");
    Reaction big = Reaction("K4Fe(CN)6 + KMnO4 + H2SO4 -> KHSO4 + Fe2(SO4)3 + MnSO4 + HNO3 + CO2 + H2O").balanced();
    check(big.reactants()[1].coefficient == 122.0 && big.products()[3].coefficient == 60.0 && big.products()[5].coefficient == 188.0 && big.is_balanced(), "large redox balance");
    Reaction ionic = Reaction("MnO4- + Fe2+ + H+ -> Mn2+ + Fe3+ + H2O").balanced();
    check(ionic.reactants()[1].coefficient == 5.0 && near(ionic.charge_imbalance(), 0.0, 1e-12), "ionic balance with charge");
    check(near(ionic.electrons_transferred(), 5.0, 1e-12), "five electrons move");
    Reaction methane("CH4(g) + 2O2(g) -> CO2(g) + 2H2O(l)");
    check(near(methane.delta_h() / 1000.0, -890.6, 0.5), "methane combustion enthalpy: " + num(methane.delta_h()));
    check(near(methane.delta_g() / 1000.0, -818.0, 1.0), "methane combustion Gibbs energy");
    Reaction shift("CO(g) + H2O(g) <=> CO2(g) + H2(g)");
    check(near(shift.equilibrium_constant(1000.0), 1.44, 0.05), "water-gas shift at 1000 K: " + num(shift.equilibrium_constant(1000.0)));
    Reaction hess = Reaction("C(s) + O2(g) -> CO2(g)") - Reaction("CO(g) + 0.5O2(g) -> CO2(g)");
    check(near(hess.delta_h() / 1000.0, -110.5, 0.2), "Hess's law");
    Reaction water("2H2 + O2 -> 2H2O");
    check(water.limiting_reactant({ 2.0, 3.0 }) == 0 && near(water.theoretical_yield(0, { 2.0, 3.0 }), 2.0, 1e-12), "limiting reactant");
}

void test_thermo() {
    const Substance& co2 = substance("CO2");
    check(near(co2.enthalpy_change(Phase::Gas, 298.15, 1000.0), 33397.0, 20.0), "CO2 enthalpy to 1000 K");
    check(near(co2.entropy(Phase::Gas, 1000.0), 269.30, 0.05), "CO2 entropy at 1000 K");
    check(near(substance("water").vapor_pressure(373.15), 101325.0, 400.0), "water boils at 100 C");
    check(near(substance("water").gibbs_of_formation_liquid / 1000.0, -237.14, 0.1), "Gibbs energy of formation of water");
    check(near(van_t_hoff(1.0, 300.0, 310.0, 50000.0), std::exp(-50000.0 / GAS_CONSTANT * (1.0 / 310.0 - 1.0 / 300.0)), 1e-12), "van 't Hoff");
}

void test_equations_of_state() {
    const Substance& methane = substance("methane");
    PengRobinson pr = PengRobinson::from_substance(methane);
    check(near(pr.saturation_pressure(150.0) / 1e6, 1.0414, 0.01), "Peng-Robinson vapor pressure of methane: " + num(pr.saturation_pressure(150.0)));
    check(near(pr.saturation_temperature(ATMOSPHERE), 111.67, 0.2), "Peng-Robinson boiling point of methane");
    check(near(pr.EquationOfState::critical_point().compressibility(), 0.30740, 1e-4), "Peng-Robinson critical compressibility");
    const double v = pr.molar_volume(200.0, 5e6);
    check(rel(pr.EquationOfState::residual_helmholtz(200.0, v), pr.residual_helmholtz(200.0, v), 1e-7), "numeric and analytic residual Helmholtz energy agree");
    check(near(PengRobinson::from_substance(substance("propane")).saturation_pressure(300.0) / 1e6, 0.9976, 0.005), "Peng-Robinson vapor pressure of propane");
    VanDerWaals vdw = VanDerWaals::from_substance(substance("CO2"));
    const CriticalConstants numeric = vdw.EquationOfState::critical_point();
    check(rel(numeric.temperature, vdw.critical_point().temperature, 1e-8) && rel(numeric.volume, 3.0 * vdw.b(), 1e-5), "numeric critical point of van der Waals");
    check(near(vdw.compressibility_at_critical(), 0.375, 1e-12), "van der Waals critical compressibility");
    check(rel(vdw.boyle_temperature(), vdw.boyle_temperature_exact(), 1e-8), "van der Waals Boyle temperature");
    IdealGas ideal;
    check(near(ideal.molar_volume(273.15, 101325.0) * 1000.0, 22.414, 1e-3), "ideal molar volume");
    for (EosKind kind : equation_of_state_kinds()) {
        if (kind == EosKind::BeattieBridgeman || kind == EosKind::BenedictWebbRubin) continue;
        const std::unique_ptr<EquationOfState> eos = make_equation_of_state(kind, substance("nitrogen"));
        const double z = eos->compressibility(300.0, 1e5);
        check(near(z, 1.0, 0.01), std::string(to_string(kind)) + " is nearly ideal at 1 bar: " + num(z));
    }
    check(near(BenedictWebbRubin::preset("methane").saturation_pressure(150.0) / 1e6, 1.04, 0.02), "Benedict-Webb-Rubin vapor pressure of methane");
    check(near(BeattieBridgeman::preset("argon").second_virial(300.0) * 1e6, -15.5, 1.5), "Beattie-Bridgeman second virial of argon");
    check(near(LeeKesler::from_substance(methane).saturation_pressure(150.0) / 1e6, 1.04, 0.02), "Lee-Kesler vapor pressure of methane");
    check(has_value(Wohl::from_substance(methane).saturation_pressure(150.0)), "Wohl has a two-phase region");
    check(has_value(Dieterici::from_substance(methane).saturation_pressure(150.0)), "Dieterici has a two-phase region");
    auto mix = CubicMixture({ std::make_shared<PengRobinson>(PengRobinson::from_substance(methane)), std::make_shared<PengRobinson>(PengRobinson::from_substance(substance("ethane"))) }, { 0.7, 0.3 });
    const std::vector<double> lnphi = mix.ln_fugacity_coefficients(250.0, 3e6, Root::Vapor);
    check(near(0.7 * lnphi[0] + 0.3 * lnphi[1], mix.ln_fugacity_coefficient(250.0, 3e6, Root::Vapor), 1e-10), "mixture fugacities are consistent");
    Tait tait = Tait::from_bulk_modulus(1.807e-5, 2.2e9, 7.0);
    check(rel(tait.pressure(298.15, tait.molar_volume(298.15, 5e7)), 5e7, 1e-9), "Tait round trip");
}

void test_bonds_and_shapes() {
    check(near(Bond("H", "Cl").length(), 127.5, 1e-9) && near(Bond("C", "C", BondOrder::Triple).energy(), 835.0, 1e-9), "bond tables");
    check(near(Bond("C", "H").length(BondLengthMethod::Pyykko), 107.0, 1e-9), "Pyykko bond length");
    check(near(percent_ionic_character(0.0), 0.0, 1e-12) && Bond("Na", "Cl").character() == BondCharacter::Ionic, "ionic character");
    check(near(kapustinskii_energy(element("Mg"), 2, 1, element("O"), -2, 1), 3795.0, 50.0), "Kapustinskii lattice energy of MgO");
    const VseprResult water = vsepr("O", { "H", "H" });
    check(water.molecular_geometry == Geometry::Bent && water.hybridization == Hybridization::SP3, "water is bent");
    check(vsepr("Xe", { "F", "F", "F", "F" }).molecular_geometry == Geometry::SquarePlanar, "XeF4 is square planar");
    check(vsepr("N", { "O", "O", "O" }, -1).molecular_geometry == Geometry::TrigonalPlanar, "nitrate is trigonal planar");
    check(vsepr("I", { "I", "I" }, -1).molecular_geometry == Geometry::Linear, "triiodide is linear");
}

void test_kinetics_and_solutions() {
    check(near(first_order_half_life(LN2), 1.0, 1e-12), "first-order half-life");
    check(rel(activation_energy(1.0, 300.0, arrhenius(1.0, 50000.0, 310.0) / arrhenius(1.0, 50000.0, 300.0), 310.0), 50000.0, 1e-9), "activation energy from two rates");
    ReactionNetwork net;
    net.add({ { 0, 1.0 } }, { { 1, 1.0 } }, 2.0);
    const std::vector<double> c = net.simulate({ 1.0, 0.0 }, 1.0, 1000);
    check(near(c[0], std::exp(-2.0), 1e-9), "mass action network");
    check(near(GalvanicCell::from_couples("Zn2+/Zn", "Cu2+/Cu").standard_potential(), 1.1037, 1e-4), "Daniell cell");
    check(near(weak_acid_ph(0.1, ka_from_pka(4.756)), 2.88, 0.01), "pH of 0.1 M acetic acid");
    check(near(strong_acid_ph(0.01), 2.0, 1e-3), "pH of 0.01 M HCl");
    check(near(henderson_hasselbalch(4.756, 0.1, 0.1), 4.756, 1e-12), "buffer pH");
    const std::vector<double> f = species_fractions(7.21, { 2.16, 7.21, 12.32 });
    check(near(f[1], f[2], 1e-3), "phosphate fractions cross at pKa2");
    check(near(freezing_point_depression(1.0, WATER_CRYOSCOPIC_CONSTANT, 2.0), 3.72, 1e-12), "freezing point depression");
    check(near(rms_speed(300.0, 28.014), 516.8, 0.5), "rms speed of nitrogen");
    check(near(gas_viscosity(substance("nitrogen"), 300.0) * 1e6, 17.9, 0.6), "viscosity of nitrogen: " + num(gas_viscosity(substance("nitrogen"), 300.0)));
    check(near(rydberg_wavelength(1, 2, 3, isotope("H-1").mass - ELECTRON_MASS_U) * 1e9, 656.47, 0.05), "Balmer alpha in vacuum");
}

void test_molecules() {
    Molecule methane;
    const std::size_t c = methane.add_atom("C");
    check(methane.add_implicit_hydrogens() == 4 && methane.formula().hill() == "CH4", "implicit hydrogens on carbon");
    check(methane.geometry_at(c).molecular_geometry == Geometry::Tetrahedral && methane.obeys_octet(c), "methane is tetrahedral");
    const double hf = methane.estimated_enthalpy_of_formation();
    check(hf < -40.0 && hf > -90.0, "bond-energy estimate of methane's enthalpy of formation: " + num(hf));
    Molecule co2;
    const std::size_t cc = co2.add_atom("C");
    co2.add_bond(cc, co2.add_atom("O"), BondOrder::Double);
    co2.add_bond(cc, co2.add_atom("O"), BondOrder::Double);
    check(co2.geometry_at(cc).molecular_geometry == Geometry::Linear && co2.missing_bonds(cc) == 0 && co2.valid(), "carbon dioxide is linear");
    Molecule ammonium;
    const std::size_t n = ammonium.add_atom("N", 1);
    ammonium.add_implicit_hydrogens();
    check(ammonium.degree(n) == 4 && ammonium.charge() == 1 && ammonium.formula().charge() == 1, "ammonium");
    Molecule borohydride;
    const std::size_t b = borohydride.add_atom("B", -1);
    borohydride.add_implicit_hydrogens();
    check(borohydride.degree(b) == 4 && borohydride.geometry_at(b).molecular_geometry == Geometry::Tetrahedral, "borohydride");
    Molecule benzene;
    std::vector<std::size_t> ring;
    for (int i = 0; i < 6; ++i) ring.push_back(benzene.add_atom("C"));
    for (int i = 0; i < 6; ++i) benzene.add_bond(ring[i], ring[(i + 1) % 6], BondOrder::Aromatic);
    benzene.add_implicit_hydrogens();
    check(benzene.formula().hill() == "C6H6" && benzene.ring_count() == 1 && benzene.hybridization_at(0) == Hybridization::SP2, "benzene");
    Molecule water;
    const std::size_t o = water.add_atom("O");
    const std::size_t h1 = water.add_atom("H");
    const std::size_t h2 = water.add_atom("H");
    water.add_bond(o, h1);
    water.add_bond(o, h2);
    water.set_position(h1, 75.7, 58.6, 0.0);
    water.set_position(h2, -75.7, 58.6, 0.0);
    check(water.lone_pairs(o) == 2 && near(water.angle(h1, o, h2), 104.5, 0.2), "water geometry");
    check(near(water.dipole_moment(), 1.85, 0.4), "water dipole estimate: " + num(water.dipole_moment()));
    std::string why;
    Molecule bad;
    const std::size_t cb = bad.add_atom("C");
    for (int i = 0; i < 5; ++i) bad.add_bond(cb, bad.add_atom("H"));
    check(!bad.valid(&why) && !why.empty(), "five bonds to carbon is invalid");
}

} // namespace

void test_units_and_names() {
    check(near(convert(25.0, "C", "F"), 77.0, 1e-9), "25 C is 77 F");
    check(near(convert(1.0, "atm", "kPa"), 101.325, 1e-9), "1 atm in kPa");
    check(near(Quantity::parse("750 mmHg").in("torr"), 750.0 * MMHG / TORR, 1e-9), "mmHg to torr");
    check(near(convert(1.0, "kcal/mol", "kJ/mol"), 4.184, 1e-12), "kcal to kJ");
    check(name_compound("Fe2O3").name == "iron(III) oxide", "Fe2O3 name");
    check(name_compound("CuSO4*5H2O").name == "copper(II) sulfate pentahydrate", "hydrate name");
    check(name_compound("(NH4)2SO4").name == "ammonium sulfate", "ammonium sulfate");
    check(name_compound("Pb(NO3)2").name == "lead(II) nitrate", "metal before nitryl");
    check(name_compound("H2SO4").name == "sulfuric acid", "oxyacid");
    check(name_compound("HCl(aq)").name == "hydrochloric acid", "binary acid in water");
    check(name_compound("N2O5").name == "dinitrogen pentoxide", "covalent prefixes");
    check(name_compound("KAl(SO4)2").name == "potassium aluminum sulfate", "double salt: " + name_compound("KAl(SO4)2").name);
    check(name_compound("C3H8").name == "propane", "alkane");
    check(name_compound("CH3COOC2H5").name == "ethyl acetate", "ester");
    check(name_compound("C6H12O6").name == "glucose", "carbohydrate");
    check(formula_from_name("aluminum sulfate").formula == "Al2(SO4)3", "name to formula");
    check(formula_from_name("ferric chloride").formula == "FeCl3", "classical name");
    check(formula_from_name("phosphorus pentachloride").formula == "PCl5", "covalent name to formula");
    check(formula_from_name("propanoic acid").formula == "C2H5COOH", "organic acid name");
    check(parse_ion("SO42-").has_value() && parse_ion("SO42-")->name == "sulfate", "SO42- is sulfate");
    check(parse_ion("Hg22+").has_value() && parse_ion("Hg22+")->name == "mercury(I)", "mercury(I)");
    int bad = 0;
    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
        const Molecule m = Molecule::from_smiles(p.smiles);
        if (!m.formula().same_composition(ion_formula(p)) || m.charge() != p.charge) ++bad;
    }
    check(bad == 0, "every polyatomic ion's structure matches its formula (" + std::to_string(bad) + " wrong)");
    check(polyatomic_ions().size() > 150, "big ion list");
}

void test_structures() {
    Molecule water = Molecule::from_smiles("O");
    water.apply_lewis(water.lewis_structure(0));
    water.layout_3d();
    check(near(water.angle(1, 0, 2), 104.5, 3.0), "water is bent: " + num(water.angle(1, 0, 2)));
    Molecule sf6 = Molecule::from_smiles("FS(F)(F)(F)(F)F");
    sf6.layout_3d();
    check(near(sf6.angle(0, 1, 2), 180.0, 2.0) || near(sf6.angle(0, 1, 2), 90.0, 2.0), "SF6 is octahedral");
    Molecule nitrate = Molecule::from_smiles("[O-][N+](=O)[O-]");
    for (const MoleculeBond& b : nitrate.bonds()) nitrate.set_bond_order(b.a, b.b, BondOrder::Single);
    const ResonanceSet res = nitrate.resonance_structures(-1);
    check(res.count() == 3 && near(res.average_bond_orders[0], 4.0 / 3.0, 1e-9), "nitrate has three resonance structures");
    const IonicCrystal nacl = ionic_crystal(element("Na"), element("Cl"));
    check(nacl.structure == CrystalStructure::RockSalt && near(nacl.cell.a, 564.0, 10.0), "NaCl is rock salt with a = 564 pm");
    check(rel(nacl.lattice_energy_born_haber, 787.0, 0.02), "NaCl Born-Haber lattice energy: " + num(nacl.lattice_energy_born_haber));
    check(rel(nacl.density, 2.165, 0.03), "NaCl density");
    check(ionic_crystal(element("Ca"), element("F")).structure == CrystalStructure::Fluorite, "CaF2 is fluorite");
    const MetallicCrystal cu = metallic_crystal(element("Cu"));
    check(cu.structure == CrystalStructure::FaceCenteredCubic && rel(cu.density, 8.96, 0.01) && rel(cu.fermi_energy, 7.0, 0.03), "copper FCC, density and Fermi energy");
    const MetallicCrystal steel = metallic_crystal({ { &element("Fe"), 0.96 }, { &element("C"), 0.04 } });
    check(steel.type == AlloyType::Interstitial && steel.name == "steel", "steel is interstitial");
    check(metallic_crystal({ { &element("Cu"), 0.7 }, { &element("Zn"), 0.3 } }).name == "brass", "brass");
    check(solubility_of("AgCl").solubility == Solubility::Insoluble && solubility_of("KNO3").solubility == Solubility::Soluble, "solubility rules");
}

void test_orbitals_and_phases() {
    const RadialCurve h1s = radial_distribution(1, 0, 1.0, 500.0, 2000);
    check(near(h1s.most_probable_pm, BOHR_RADIUS / PICOMETER, 0.5), "hydrogen 1s peaks at the Bohr radius");
    check(near(h1s.cumulative.back(), 1.0, 1e-3), "1s is normalized");
    const RadialCurve h2s = radial_distribution(2, 0, 1.0, 1500.0);
    check(h2s.node_radii_pm.size() == 1 && near(h2s.node_radii_pm[0], 2.0 * BOHR_RADIUS / PICOMETER, 0.5), "2s node at 2 a0");
    check(sample_orbital(3, 2, 0, 1.0, 500).size() == 500, "orbital sampling");
    const PhaseDiagram water = phase_diagram(find_phase_properties("water"));
    check(water.ok && water.phase_at(298.15, ATMOSPHERE) == Phase::Liquid && water.phase_at(263.15, ATMOSPHERE) == Phase::Solid && water.phase_at(400.0, ATMOSPHERE) == Phase::Gas, "water phases");
    check(near(water.boiling_temperature(ATMOSPHERE), 373.12, 0.2), "water boils at 373 K");
    check(water.melting_pressure(273.0) > water.properties.triple_pressure + 1e6, "water's melting line leans left");
    const PhaseDiagram co2 = phase_diagram(find_phase_properties("carbon dioxide"));
    check(co2.phase_at(194.0, ATMOSPHERE) == Phase::Solid && co2.phase_at(200.0, ATMOSPHERE) == Phase::Gas, "dry ice sublimes at 1 atm");
    check(pvt_surface(water).ok, "water PvT surface");
    const JobackEstimate j = joback(Molecule::from_smiles("CCO"));
    check(j.ok && near(j.boiling_point, 337.3, 1.0), "Joback ethanol boiling point");
}

void test_prediction_and_solvers() {
    check(predict_products("CH4 + O2").equation(false) == "CH4(g) + 2 O2(g) -> CO2(g) + 2 H2O(l)", "methane combustion: " + predict_products("CH4 + O2").equation(false));
    check(predict_products("Cu + HCl").no_reaction, "copper doesn't react with HCl");
    check(predict_products("Zn + CuSO4").equation(false) == "Zn(s) + CuSO4(aq) -> ZnSO4(aq) + Cu(s)", "zinc displaces copper");
    const PredictedReaction ag = predict_products("AgNO3(aq) + NaCl(aq)");
    check(ag.ok && ag.net_ionic.find("AgCl(s)") != std::string::npos && ag.spectators.size() == 2, "silver chloride precipitate");
    check(predict_products("N2 + H2").reaction.reversible(), "Haber process is reversible");
    check(predict_products("KNO3 + NaCl").no_reaction, "no precipitate, no reaction");
    const EquationSolution v = solve_equation(*find_equation("ideal_gas"), { ATMOSPHERE, NO_VALUE, 1.0, 273.15 });
    check(v.ok && near(v.value * 1000.0, 22.414, 1e-3), "molar volume at STP");
    const EquationSolution ph = solve_equation(*find_equation("weak_acid"), { 1.8e-5, 100.0, NO_VALUE });
    check(ph.ok && rel(ph.value / 1000.0, 1.333e-3, 0.01), "0.1 M acetic acid");
    const StoichiometryResult st = solve_stoichiometry(Reaction::parse("N2(g) + H2(g) -> NH3(g)"), { { 0, AmountKind::Mass, 0.028, NO_VALUE }, { 1, AmountKind::Mass, 0.010, NO_VALUE } });
    check(st.ok && st.limiting == 0 && rel(st.species[2].final_moles, 1.999, 1e-3), "limiting reactant");
    const BondThermo h2 = bond_dissociation(element("H"), element("H"), BondOrder::Single, 298.15, STANDARD_PRESSURE);
    check(rel(h2.entropy, 98.7, 0.03), "H2 dissociation entropy: " + num(h2.entropy));
    check(empirical_formula_from_masses({ { &element("C"), 40.0 }, { &element("H"), 6.71 }, { &element("O"), 53.3 } }).to_string() == "CH2O", "empirical formula");
}

void test_transport_and_estimates() {
    const TransportProperties cu = element_transport(element("Cu"));
    check(cu.kind == ConductorKind::Metal && near(cu.resistivity, 1.71e-8, 0.03e-8), "copper's resistivity at 25 C");
    check(cu.thermal_conductivity > 380.0 && cu.debye_temperature > 280.0 && cu.debye_temperature < 380.0, "copper conducts heat and has a sensible Debye temperature");
    check(cu.skin_depth > 0.007 && cu.skin_depth < 0.01, "copper's skin depth at 60 Hz is about 8.5 mm");
    check(element_transport(element("Cu"), 20.0).resistivity < cu.resistivity / 20.0, "metals conduct much better when cold");
    check(element_transport(element("Fe")).magnetic.order == MagneticOrder::Ferromagnetic && element_transport(element("Fe"), 1200.0).magnetic.order == MagneticOrder::Paramagnetic, "iron loses ferromagnetism above its Curie point");
    const TransportProperties si = element_transport(element("Si"));
    check(si.kind == ConductorKind::Semiconductor && near(si.band_gap, 1.12, 1e-9) && element_transport(element("Si"), 500.0).conductivity > si.conductivity * 100.0, "silicon is a semiconductor that conducts better when hot");
    const TransportProperties brass = metal_transport(metallic_crystal({ { &element("Cu"), 0.7 }, { &element("Zn"), 0.3 } }));
    check(brass.resistivity > 4e-8 && brass.resistivity < 9e-8, "brass conducts worse than copper or zinc");
    const TransportProperties salt = ionic_transport(ionic_crystal(element("Na"), element("Cl")));
    check(salt.kind == ConductorKind::Insulator && ionic_transport(ionic_crystal(element("Na"), element("Cl")), 1200.0).kind == ConductorKind::IonicConductor, "salt conducts only when molten");
    check(near(debye_heat_capacity(5000.0, 300.0), 3.0 * GAS_CONSTANT, 0.05), "Debye heat capacity approaches 3R");
    const PhaseProperties peroxide = find_phase_properties("H2O2");
    check(peroxide.vaporization_enthalpy > 40000.0 && peroxide.vaporization_enthalpy < 60000.0, "hydrogen peroxide gets an estimated enthalpy of vaporization");
    check(near(bond_energy(element("Xe"), element("F")), 130.0, 1e-9), "Xe-F has a bond energy");
}

void test_materials() {
    const MechanicalProperties cu = element_mechanics(element("Cu"));
    check(near(cu.youngs_modulus, 130.0, 1e-9) && cu.poisson_ratio > 0.3 && cu.poisson_ratio < 0.38, "copper's elastic constants");
    check(cu.longitudinal_sound_speed > 4000.0 && cu.longitudinal_sound_speed < 5500.0, "sound in copper travels about 4.7 km/s");
    const DielectricProperties si = element_dielectric(element("Si"));
    check(near(si.relative_permittivity, 11.7, 1e-9) && near(si.refractive_index, 3.42, 1e-9) && si.reflectance > 0.25, "silicon's permittivity and refractive index");
    const DielectricProperties ar = element_dielectric(element("Ar"), 500.0, 101325.0);
    check(ar.permittivity_estimated && ar.relative_permittivity > 1.0001 && ar.relative_permittivity < 1.0005, "a hot gas's permittivity from its polarizability");
    check(element_dielectric(element("Cu")).conductor && element_dielectric(element("Ag")).reflectance > 0.9, "metals reflect visible light");
    const IonicCrystal kcl = ionic_crystal(element("K"), element("Cl"));
    check(near(ionic_dielectric(kcl).relative_permittivity, 4.68, 1e-9), "KCl's permittivity is tabulated");
    const DielectricProperties feo = ionic_dielectric(ionic_crystal(element("Fe"), element("O")));
    check(feo.permittivity_estimated && feo.relative_permittivity > 8.0 && feo.relative_permittivity < 25.0, "an estimated permittivity for FeO");
    const MechanicalProperties salt = ionic_mechanics(ionic_crystal(element("Na"), element("Cl")));
    check(salt.bulk_modulus > 18.0 && salt.bulk_modulus < 30.0 && salt.linear_expansion > 3e-5 && salt.linear_expansion < 5.5e-5, "salt's bulk modulus and expansion from its lattice energy");
    const DielectricProperties water = molecular_dielectric("water", Formula::parse("H2O"), 1.85, Phase::Liquid, 997.0);
    check(near(water.relative_permittivity, 78.36, 1e-9) && near(water.polarizability_volume, 1.411, 0.01), "water's permittivity and polarizability");
    const DielectricProperties steam = molecular_dielectric("steam", Formula::parse("H2O"), 1.85, Phase::Gas, 0.0, 373.15, 101325.0);
    check(steam.relative_permittivity > 1.004 && steam.relative_permittivity < 1.008, "steam's permittivity from the Debye equation");
    check(onsager_permittivity(1e28, polarizability_volume_to_si(6.0), 2.0 * DEBYE, 298.15) > clausius_mossotti_permittivity(1e28, polarizability_volume_to_si(6.0)), "dipoles raise the permittivity");
    const ElectronGasProperties na = element_electron_gas(element("Na"));
    check(near(na.fermi_energy, 3.24, 0.15) && na.seebeck_coefficient < 0.0 && near(na.work_function, 2.36, 1e-9), "sodium's Fermi energy and work function");
    check(near(na.photoelectric_threshold, 525e-9, 5e-9), "sodium's photoelectric threshold is green light");
    check(element_susceptibility(element("Bi")).volume_susceptibility < -1e-4, "bismuth is strongly diamagnetic");
    check(element_susceptibility(element("Al")).volume_susceptibility > 0.0, "aluminum is paramagnetic");
    check(element_susceptibility(element("Fe")).volume_susceptibility > 10.0, "iron is ferromagnetic");
    const SusceptibilityInfo mn = ionic_susceptibility(ionic_crystal(element("Mn"), element("Cl")));
    check(mn.unpaired_electrons == 5 && near(mn.effective_moment, 5.92, 0.01), "Mn2+ has five unpaired electrons");
    check(near(spin_only_moment(1), 1.732, 1e-3), "spin-only moment");
    const EquationSolution skin = solve_equation(*find_equation("skin_depth"), { NO_VALUE, 1.68e-8, 60.0, 1.0 });
    check(skin.ok && near(skin.value * 1000.0, 8.42, 0.05), "copper's skin depth at 60 Hz");
    const EquationSolution pkb = solve_equation(*find_equation("pka_pkb"), { 4.76, NO_VALUE, 14.0 });
    check(pkb.ok && near(pkb.value, 9.24, 1e-6), "pKa + pKb = pKw");
    const EquationSolution cap = solve_equation(*find_equation("capacitor"), { NO_VALUE, 1.0, 1.0, 1e-3 });
    check(cap.ok && near(cap.value, 0.8854, 1e-3), "a 1 cm\xC2\xB2, 1 mm air capacitor is 0.885 pF");
}

void test_general_titration() {
    const AcidBaseSpecies* acetic = find_acid_base_species("acetic acid", 1);
    const AcidBaseSpecies* naoh = find_acid_base_species("NaOH", -1);
    const AcidBaseSpecies* hcl = find_acid_base_species("hydrochloric acid", 1);
    const AcidBaseSpecies* nh3 = find_acid_base_species("ammonia", -1);
    const AcidBaseSpecies* carbonate = find_acid_base_species("carbonate", -1);
    const AcidBaseSpecies* acetate = find_acid_base_species("acetate", -1);
    const AcidBaseSpecies* ammonium = find_acid_base_species("ammonium", 1);
    check(acetic && naoh && hcl && nh3 && carbonate && acetate && ammonium, "acids and bases are catalogued");
    if (!(acetic && naoh && hcl && nh3 && carbonate && acetate && ammonium)) return;
    check(hcl->strong && !acetic->strong && naoh->strong && !nh3->strong && carbonate->equivalents == 2, "strengths and equivalents");
    check(near(solution_ph(*acetate, 0.1), 8.88, 0.03) && near(solution_ph(*ammonium, 0.1), 5.13, 0.03), "salts of weak acids and bases");
    const TitrationResult ws = titrate(*acetic, 0.1, 0.025, *naoh, 0.1);
    check(ws.ok && near(ws.equivalence_volumes[0], 0.025, 1e-12) && near(titration_point_ph(*acetic, 0.1, 0.025, *naoh, 0.1, 0.025), 8.72, 0.03), "weak acid with strong base");
    check(near(titration_point_ph(*nh3, 0.1, 0.025, *hcl, 0.1, 0.025), 5.28, 0.03), "weak base with strong acid");
    check(near(titration_point_ph(*hcl, 0.1, 0.025, *naoh, 0.1, 0.025), 7.0, 0.01), "strong with strong is neutral");
    check(near(titration_point_ph(*acetic, 0.1, 0.025, *nh3, 0.1, 0.025), 7.0, 0.05), "acetic acid with ammonia is about neutral");
    const TitrationResult carb = titrate(*carbonate, 0.1, 0.025, *hcl, 0.1);
    check(carb.ok && carb.equivalence_volumes.size() == 2 && near(carb.equivalence_volumes[1], 0.05, 1e-12) && near(carb.start_ph, 11.66, 0.05), "carbonate takes two protons");
    check(!titrate(*hcl, 0.1, 0.025, *acetic, 0.1).ok, "an acid can't titrate an acid");
}

void test_organic_naming() {
    const char* smiles[][2] = {
        { "CC(C)C", "2-methylpropane" }, { "CCC(CC)C(C)CC", "3-ethyl-4-methylhexane" }, { "CC(C)CC(C)(C)C", "2,2,4-trimethylpentane" },
        { "CCCC(C(C)C)CCC", "4-isopropylheptane" }, { "C#CCC=C", "pent-1-en-4-yne" }, { "C=CC(O)C", "but-3-en-2-ol" },
        { "OCCO", "ethane-1,2-diol" }, { "CC(C)CC(=O)O", "3-methylbutanoic acid" }, { "CC1CCCCC1C", "1,2-dimethylcyclohexane" },
        { "Cc1ccc(O)cc1", "4-methylphenol" }, { "CCOCC", "ethoxyethane" }, { "CC(Cl)C(Br)C", "2-bromo-3-chlorobutane" }
    };
    for (const auto& t : smiles) {
        const std::string got = iupac_name(Molecule::from_smiles(t[0]));
        check(got == t[1], std::string("IUPAC name of ") + t[0] + ": " + got);
    }
    const char* condensed[][2] = {
        { "CH3CH(CH3)CH2CH3", "2-methylbutane" }, { "(CH3)3CCl", "2-chloro-2-methylpropane" }, { "CH3(CH2)4CH3", "hexane" },
        { "CH3CH=CHCH3", "but-2-ene" }, { "CH3COCH3", "propan-2-one" }, { "CH3CH2C(CH3)2CH(CH2CH3)CH2CH3", "4-ethyl-3,3-dimethylhexane" }
    };
    for (const auto& t : condensed) {
        const std::string got = iupac_name_from_condensed(t[0]);
        check(got == t[1], std::string("condensed ") + t[0] + ": " + got);
    }
    check(name_compound("CH3CH(CH3)CH2CH3").name == "2-methylbutane", "name_compound names condensed structures");
    const CompoundName acetic = name_compound("CH3COOH");
    check(std::find(acetic.other_names.begin(), acetic.other_names.end(), "ethanoic acid") != acetic.other_names.end() || acetic.name == "ethanoic acid", "a common name also lists the IUPAC name");
}

void test_intermolecular() {
    const IntermolecularForces water = intermolecular_forces(Molecule::from_smiles("O"), 1.85);
    check(water.ok && water.hydrogen_bonds_per_molecule == 2 && water.strongest == "hydrogen bonding", "water makes two hydrogen bonds per molecule");
    check(rel(intermolecular_forces(Molecule::from_smiles("CCCCCC"), 0.0).polarizability, 11.9, 0.03), "hexane's polarizability");
    const IntermolecularForces acetone = intermolecular_forces(Molecule::from_smiles("CC(=O)C"), 2.88);
    check(acetone.hydrogen_bonds_per_molecule == 0 && acetone.hydrogen_acceptor_pairs == 2 && acetone.strongest == "dipole\xE2\x80\x93" "dipole", "acetone accepts but can't donate hydrogen bonds");
    check(intermolecular_forces(Molecule::from_smiles("CCCCCCCCO"), 1.7).water_solubility.find("slightly") != std::string::npos, "octanol is only slightly soluble");
}

void test_stoichiometry_roles() {
    const Reaction r = Reaction::parse("Na2CO3 + CuCl -> Cu2CO3 + NaCl");
    StoichiometryResult st = solve_stoichiometry(r, { { 0, AmountKind::Moles, 5.0, NO_VALUE }, { 2, AmountKind::Moles, 5.0, NO_VALUE } });
    check(st.ok && rel(st.species[1].start_moles, 10.0, 1e-9) && st.species[1].assumed && rel(st.species[3].final_moles, 10.0, 1e-9), "a product amount counts as made and fills in the rest");
    check(!st.assumptions.empty(), "the missing reactant is reported as assumed");
    st = solve_stoichiometry(r, { { 2, AmountKind::Moles, 5.0, NO_VALUE } });
    check(st.ok && rel(st.species[0].start_moles, 5.0, 1e-9) && rel(st.species[3].final_moles, 10.0, 1e-9), "a product alone works backward to every reactant");
    st = solve_stoichiometry(r, { { 0, AmountKind::Moles, 5.0, NO_VALUE }, { 1, AmountKind::Moles, 4.0, NO_VALUE } });
    check(st.ok && st.limiting == 1 && rel(st.species[0].final_moles, 3.0, 1e-9) && st.assumed_complete, "two starting amounts find the limiting reactant");
    st = solve_stoichiometry(r, { { 0, AmountKind::Moles, 5.0, NO_VALUE }, { 1, AmountKind::Moles, 10.0, NO_VALUE }, { 2, AmountKind::Moles, 4.0, NO_VALUE } });
    check(st.ok && rel(st.percent_yield, 80.0, 1e-9), "percent yield");
    st = solve_stoichiometry(r, { { 0, AmountKind::Moles, 5.0, NO_VALUE }, { 2, AmountKind::Moles, 7.0, NO_VALUE } });
    check(!st.ok && st.message.find("isn't enough") != std::string::npos, "more product than the reactant allows is an error");
    st = solve_stoichiometry(r, { { 2, AmountKind::Moles, 5.0, NO_VALUE }, { 3, AmountKind::Moles, 7.0, NO_VALUE } });
    check(!st.ok && st.message.find("don't fit") != std::string::npos, "inconsistent amounts are an error");
    st = solve_stoichiometry(r, { { 1, AmountKind::Moles, 2.0, NO_VALUE, AmountRole::Left } });
    check(!st.ok, "a leftover amount alone can't be solved");
    st = solve_stoichiometry(r, { { 1, AmountKind::Moles, 2.0, NO_VALUE, AmountRole::Left }, { 3, AmountKind::Moles, 6.0, NO_VALUE } });
    check(st.ok && rel(st.species[1].start_moles, 8.0, 1e-9), "leftover plus product gives the starting amount");
}

void test_gas_stoichiometry() {
    check(near(water_vapor_pressure(298.15), 3169.0, 40.0), "water's vapor pressure at 25 C");
    check(gas_behavior(Formula::parse("H2"), 298.15).can_be_gas && gas_behavior(Formula::parse("O2"), 298.15).can_be_gas && !gas_behavior(Formula::parse("Zn"), 298.15).can_be_gas && !gas_behavior(Formula::parse("ZnCl2"), 298.15).can_be_gas, "which species are gases without (g)");
    const VanDerWaalsConstants co2 = gas_behavior(Formula::parse("CO2"), 300.0).vdw;
    check(co2.ok() && near(gas_pressure(1.0, 1e-3, 300.0, GasModel::VanDerWaals, co2) / ATMOSPHERE, 22.1, 0.3) && near(gas_pressure(1.0, 1e-3, 300.0) / ATMOSPHERE, 24.6, 0.1), "van der Waals versus ideal for CO2");
    check(near(gas_moles(gas_pressure(0.7, 2e-3, 350.0, GasModel::VanDerWaals, co2), 2e-3, 350.0, GasModel::VanDerWaals, co2), 0.7, 1e-6), "van der Waals moles invert the pressure");
    GasVessel open;
    open.over_water = true;
    const GasStoichiometryResult zn = solve_gas_stoichiometry(Reaction::parse("Zn + HCl -> ZnCl2 + H2"), { GasKnown{ 0, GasAmountKind::Mass, 0.5e-3 } }, open);
    check(zn.ok && zn.after.size() == 1 && near(zn.volume_after * 1000.0, 0.1932, 0.002), "hydrogen collected over water");
    GasVessel box;
    box.mode = GasVesselMode::Rigid;
    box.volume = 0.010;
    const GasStoichiometryResult burn = solve_gas_stoichiometry(Reaction::parse("CH4 + O2 -> CO2 + H2O"), { GasKnown{ 0, GasAmountKind::PartialPressure, ATMOSPHERE }, GasKnown{ 1, GasAmountKind::PartialPressure, 2.0 * ATMOSPHERE } }, box);
    check(burn.ok && near(burn.pressure_before / ATMOSPHERE, 3.0, 1e-6) && near(burn.pressure_after / ATMOSPHERE, 1.031, 0.003), "methane burned in a closed box, water condensing");
    bool condensed = false;
    for (const GasSpeciesState& g : burn.after) condensed = condensed || g.condensed_moles > 0.5;
    check(condensed, "most of the water condenses");
    GasVessel hot = box;
    hot.temperature = 500.0;
    const GasStoichiometryResult steam = solve_gas_stoichiometry(Reaction::parse("CH4 + O2 -> CO2 + H2O"), { GasKnown{ 0, GasAmountKind::PartialPressure, ATMOSPHERE, NO_VALUE, 298.15 }, GasKnown{ 1, GasAmountKind::PartialPressure, 2.0 * ATMOSPHERE, NO_VALUE, 298.15 } }, hot);
    check(steam.ok && near(steam.moles_after, steam.moles_before, 1e-9), "at 500 K the water stays as steam, so the moles of gas don't change");
}

void test_emission() {
    const Srgb8 red = wavelength_color(650.0), green = wavelength_color(530.0), blue = wavelength_color(450.0);
    check(red.r > red.g && red.r > red.b && green.g > green.r && green.g > green.b && blue.b > blue.r && blue.b > blue.g, "wavelengths have the right colors");
    const Srgb8 sodium = spectrum_color(emission_spectrum({ { 11, 1.0 } }));
    check(sodium.r > 200 && sodium.g > 100 && sodium.b < 90, "sodium light is yellow-orange");
    check(emission_lines_of(1, 380.0, 780.0).size() >= 4 && near(strongest_emission_line(11).wavelength_nm, 588.995, 1e-9), "line lists");
    const Spectrum mix = emission_spectrum({ { 11, 30.0 }, { 3, 70.0 } });
    const SpectrumFit two = fit_emission_spectrum(mix, { 3, 11, 19, 20 });
    check(two.ok && two.percentages.size() == 2 && near(two.percentages[0].second, 70.0, 0.5) && two.percentages[0].first == 3, "lithium and sodium are recovered from their mixed light");
    const SpectrumFit all = fit_emission_spectrum(emission_spectrum({ { 11, 20.0 }, { 38, 50.0 }, { 29, 30.0 } }));
    bool found = all.ok && all.percentages.size() >= 3;
    double sr = 0.0;
    for (const auto& p : all.percentages) if (p.first == 38) sr = p.second;
    check(found && near(sr, 50.0, 2.0), "a three-element mixture is found among every element");
    const Srgb8 purple = linear_to_srgb8(LinearRgb{ 0.5 * srgb8_to_linear(flame_color(38)).r + 0.5 * srgb8_to_linear(flame_color(55)).r, 0.5 * srgb8_to_linear(flame_color(38)).g + 0.5 * srgb8_to_linear(flame_color(55)).g, 0.5 * srgb8_to_linear(flame_color(38)).b + 0.5 * srgb8_to_linear(flame_color(55)).b }, false);
    const ColorFit cf = fit_color_mixture(purple, { 38, 55, 29 }, true);
    check(cf.ok && cf.distance < 6.0, "a flame color is matched by mixing flame colors");
    Srgb8 h;
    check(parse_hex_color("#ff8800", h) && h.r == 255 && h.g == 136 && to_hex(h) == "#ff8800", "hex colors");
}

int main() {
    test_transport_and_estimates();
    test_materials();
    test_general_titration();
    test_gas_stoichiometry();
    test_emission();
    test_stoichiometry_roles();
    test_organic_naming();
    test_intermolecular();
    test_elements();
    test_isotopes();
    test_ions_and_configurations();
    test_formulas();
    test_reactions();
    test_thermo();
    test_equations_of_state();
    test_bonds_and_shapes();
    test_kinetics_and_solutions();
    test_molecules();
    test_units_and_names();
    test_structures();
    test_orbitals_and_phases();
    test_prediction_and_solvers();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
