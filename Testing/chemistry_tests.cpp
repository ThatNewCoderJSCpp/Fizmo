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

int main() {
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
