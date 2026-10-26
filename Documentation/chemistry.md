# Chemistry

`fizmo::chemistry` is the periodic table, isotopes, ions, formulas, reactions, thermochemistry, equations of state and the rest of the physical chemistry Sapling builds on. It is the `chemistry` component (`fizmo::chemistry` in CMake), which only needs `fizmo::core`.

```cpp
#define FIZMO
#define FIZMO_CHEMISTRY
#include <fizmo/includes.hpp>

namespace chem = fizmo::chemistry;
```

`ALL_FIZMO` turns it on along with everything else. `FIZMO_CHEM` works too.

Everything is SI unless the name says otherwise: kelvin, pascal, m³/mol, J/mol, J/(mol·K). The exceptions are the ones chemists expect: molar masses and atomic weights in g/mol (u), radii and bond lengths in picometres, ionization energies and electron affinities in electronvolts, bond energies in kJ/mol, dipoles in debye. Missing values are `NO_VALUE` (a quiet NaN); test with `has_value(x)`.

## Elements (`periodic_table.hpp`, `element_types.hpp`)

All 118 elements are `constexpr`, so lookups work at compile time:

```cpp
static_assert(chem::element("Fe").group == 8);
constexpr const chem::Element& gold = chem::element(79);
```

| | |
|---|---|
| `element(z)`, `element("Fe")`, `element("iron")` | throw on an unknown element; `find_element(...)` returns `nullptr` instead. Names are case-insensitive and accept `aluminium`, `caesium`, `sulphur` |
| `PeriodicTable::elements()`, `in_period`, `in_group`, `in_block`, `in_category`, `with_classification`, `in_phase`, `at_position(period, group)` | |
| `PeriodicTable::table_row(e)`, `table_column(e)` | where to draw it on an 18-column table; the f-block goes in rows 9 and 10 |

An `Element` has its atomic number, symbol, name, period, group, block, `category` (alkali metal, alkaline earth metal, lanthanide, actinide, transition metal, post-transition metal, metalloid, reactive nonmetal, halogen, noble gas), `classification` (metal, metalloid, nonmetal), standard state at 298.15 K and 1 bar, and:

- **Atomic weight.** `atomic_weight` (g/mol), `atomic_weight_uncertainty`, and `atomic_weight_min` / `atomic_weight_max`. These are the CIAAW 2024 standard atomic weights. For the 14 elements that CIAAW gives as an interval (H, Li, B, C, N, O, Mg, Si, S, Cl, Ar, Br, Tl, Pb), `atomic_weight` is the conventional value (1.008, 12.011, …), the uncertainty is half the interval, and the min/max are the interval. `molar_mass()` returns the same number in g/mol; `exact_molar_mass()` multiplies by the 2022 CODATA molar mass constant (1.000 000 001 05 g/mol), which only matters past the ninth significant figure. Thallium's conventional value, 204.38, sits just below its interval [204.382, 204.385]; that is how CIAAW publishes it (the conventional value is rounded), so don't assert `atomic_weight_min <= atomic_weight` for Tl.
- **Stability.** `is_stable` is true when the element has at least one stable nuclide, so Tc, Pm and everything from Bi onward are unstable. `has_standard_atomic_weight` is true when CIAAW gives one. When it doesn't, the atomic weight is the atomic mass of the longest-lived isotope (Tc-97, Pm-145, Po-209, …, Og-294). Bi, Th, Pa and U are unstable but keep their natural atomic weights, because those are what you want for real samples. `reference_mass_number` is that longest-lived (or most abundant) isotope; `neutrons()` uses it. For the superheavy elements, half-lives known only from one or two events (uncertainty over 70 %) don't count when picking the longest-lived isotope.
- **Electrons.** `electron_configuration` (`"[Ar] 3d6 4s2"`, the real ground state, so Cr, Cu, Pd, Gd, … are right), `subshells()`, `ground_term` (from NIST, `5D4` for iron), `valence_electrons()`, `unpaired_electrons()`, `full_electron_configuration()`, `shell_configuration()` (`"2, 8, 14, 2"`).
- **Ionization.** `ionization_energy(n)` is the nth ionization energy in eV, for every n NIST lists (all of them up to Z = 103, most of them to 110). `cumulative_ionization_energy(charge)`, `ionization_energy_kj_per_mol(n)`, `ionization_energy_uncertainty(n)`.
- **Electron affinity.** `electron_affinity` in eV with its uncertainty and `electron_affinity_quality` (measured, estimated or calculated). Negative values mean the anion isn't bound (`forms_stable_anion()` is false); those are estimates. Values come from the current experimental compilation (Hotop–Lineberger and later measurements), which revises several of the older CRC numbers (Hf, the lanthanides, Po, At).
- **Electronegativity.** `electronegativity_pauling` (CRC, plus Kr 3.00 and Rn 2.2), `electronegativity_allen`, `mulliken_electronegativity()`.
- **Oxidation states.** `oxidation_states()` with a `common` flag on each, `has_oxidation_state`, `is_common_oxidation_state`, `highest_oxidation_state`, `lowest_oxidation_state`, `most_common_oxidation_state`.
- **Sizes.** `covalent_radius` (Pyykkö single bond), `covalent_radius_double`, `covalent_radius_triple`, `covalent_radius_cordero`, `van_der_waals_radius` (Alvarez, then Bondi), `metallic_radius`, `atomic_radius`, `polarizability` (bohr³), `ionic_radii()` and `ionic_radius(charge, coordination)` (Shannon).
- **Bulk properties.** Gas-phase enthalpy of formation of the atom (`atomization_enthalpy`, kJ/mol), melting and boiling points, triple and critical points, density, heats of fusion and vaporization, heat capacities, thermal conductivity, crystal structure, lattice constant, crustal and seawater abundance, CPK color, discovery year. `properties_predicted` is set for Z ≥ 104, whose chemistry is still mostly predicted.

The group convention follows IUPAC 2021: group 3 is Sc, Y, Lu, Lr. La–Yb and Ac–No are f-block with `group == 0`. Group 12 counts as transition metals; At and Ts are halogens classified as metalloids.

## Isotopes (`isotope_types.hpp`)

All 3,557 nuclides from NUBASE2020 / AME2020 are in `PeriodicTable::isotopes()`, with each element's in `element.isotopes()`.

| | |
|---|---|
| `isotope(z, a)`, `isotope("C-14")`, `"14C"`, `"carbon-14"`, `"U 235"`, `"^235U"`, `"D"`, `"tritium"` | `find_isotope(...)` returns `nullptr` instead of throwing |
| `mass`, `mass_uncertainty` | atomic mass in u |
| `abundance` | natural abundance as a fraction, `is_natural()` |
| `is_stable`, `half_life`, `half_life_uncertainty` | seconds; stable nuclides have `INFINITE_TIME` |
| `spin()`, `parity`, `g_factor`, `magnetic_moment()`, `gyromagnetic_ratio()`, `quadrupole_moment` (barn), `is_nmr_active()` | |
| `binding_energy()`, `binding_energy_per_nucleon()`, `mass_excess()` | MeV |
| `decay_constant()`, `mean_lifetime()`, `specific_activity()` (Bq/g), `remaining_fraction(t)`, `activity(moles)` | |
| `decays()`, `primary_decay()` | `DecayBranch`es with the mode, percent and whether it's a sub-branch (β-delayed emission, or e⁺/EC inside a β⁺ total) |
| `daughter(parent, branch)`, `decay_q_value(parent, branch)` | |

NUBASE's branching intensities for very small branches (U-238's spontaneous fission is 5.44×10⁻⁵ %) lost their exponents in the database the data came from. For the 47 nuclides where that happened, the dominant branch is kept, the small ones are taken from ICRP-107 where it has them, and otherwise their percent is left unknown rather than wrong.

`nuclear.hpp` adds `decay_chain(start)` (follows the primary decays down to a stable nuclide), `bateman(...)` and `chain_activities(...)` for the amounts and activities along a chain, the semi-empirical mass formula, Coulomb barriers, Q values of nuclear reactions, curie/becquerel and radiometric dating.

## Ions and oxidation states (`atom.hpp`, `oxidation_state.hpp`)

`OxidationState` holds -8 to +8 and throws outside that range. It has `roman()` (`"III"`), `stock_notation("iron")`, `oxidized(n)` and `reduced(n)`. `AverageOxidationState` is a fraction for mixed-valence compounds (+8/3 for Fe in Fe₃O₄).

`Atom` (also called `Ion`) is an element with a charge (-8 up to the atomic number), an optional mass number, and an oxidation state that starts out equal to the charge and can be changed separately:

```cpp
chem::Atom fe = chem::Atom::parse("Fe3+");
fe.electrons();                           // 23
fe.electron_configuration().to_noble_gas_string();   // "[Ar] 3d5"
fe.electron_configuration().hund_term().to_string(); // "6S5/2"
fe.ionic_radius(6);                       // 64.5 pm
fe.stock_name();                          // "iron(III)"
```

`Atom::parse` takes `Fe3+`, `Fe+3`, `Fe^{3+}`, `Fe++`, `Cl-`, `56Fe`, `U-235`, `D`. `ElectronConfiguration` removes electrons from the highest shell first (4s before 3d), so ion configurations come out right, and has Hund's-rule term symbols, Slater's-rule effective nuclear charge, parsing, aufbau filling and unicode output.

## Formulas (`formula.hpp`)

```cpp
chem::Formula f("CuSO4·5H2O");
f.molar_mass();          // 249.677
f.count("O");            // 9
f.hill();                // "CuH10O9S"
chem::assign_oxidation_states(chem::Formula("KMnO4"));   // K +1, Mn +7, O -2
```

Formulas handle nested `()`, `[]` and `{}`, hydrates and adducts with `·`, `*` or `.5H2O`, isotope labels (`D2O`, `[13C]O2`, `^13CO2`), a trailing phase (`(s)`, `(l)`, `(g)`, `(aq)`) and a charge. A charge can always be written explicitly as `SO4^2-` or `SO4 2-`. Without the `^`, the trailing digits follow the usual plain-text habits: one atom with one digit is a charge (`Fe3+`, `O2-`), two digits before the sign split into a count and a charge (`SO42-`, `Hg22+`, `Fe(CN)63-`), digits after `]` are a charge (`[Fe(CN)6]4-`), and otherwise a single digit is a count (`NH4+`, `NO3-`). Write azide as `N3^-` and superoxide as `O2^-`. `e-` is an electron.

A formula gives molar mass and its uncertainty, monoisotopic and nominal mass, mass fractions, empirical formula, Hill notation, unicode text (`CuSO₄`), degree of unsaturation, and electron, proton and valence counts.

`assign_oxidation_states` follows the usual rules (F -1, group 1 and 2 metals, H +1 or -1 in hydrides, O -2 unless that's impossible, as in peroxides and superoxides). Whatever is left is solved by charge balance, choosing among each element's known oxidation states when more than one element is left (pyrite comes out Fe +2, S -1).

## Substances and thermochemistry (`substance.hpp`, `thermochemistry.hpp`)

136 substances (common gases, solvents, fuels, acids, salts, oxides and elements in their reference states) are `constexpr`:

```cpp
const chem::Substance& co2 = chem::substance("carbon dioxide");   // or "CO2" or "124-38-9"
co2.critical_temperature;      // 304.1282 K
co2.enthalpy(chem::Phase::Gas, 1000.0);
co2.vapor_pressure(250.0);
```

Each has its formula, CAS number, molar mass and standard state. It also carries critical temperature, pressure, volume and compressibility, acentric factor, normal boiling and melting points, triple point, and dipole moment. Thermochemical data covers ΔHf°, S° and ΔGf° per phase, enthalpies of vaporization and fusion, Lennard-Jones parameters, Antoine coefficients (log₁₀ Pa, K) and heat-capacity polynomials per phase.

A few notes on the data. Lennard-Jones σ and ε/k come only from Poling, Prausnitz & O'Connell Appendix B; substances not in that table have `NO_VALUE` rather than an estimate. The ΔHvap of formic and acetic acid refers to the dimerized vapor, so it's lower than the monomer value. For substances whose standard state is gas (propane, isobutane, dimethyl ether, chloromethane, …), the liquid-phase ΔHf°/ΔGf° are for the hypothetical liquid at 1 bar and 298.15 K, not a real state.

Formation enthalpies are the Active Thermochemical Tables values where they exist (methane is −74.53 kJ/mol rather than the older −74.87). Heat capacities are NIST Shomate fits, Poling's ideal-gas polynomials where the Shomate fit doesn't reach 298 K, and constant 298 K values otherwise. `enthalpy(phase, T)` and `entropy(phase, T)` integrate Cp from 298.15 K; `gibbs_energy` is H − TS on that same basis, which is what reaction thermochemistry needs. There are also `enthalpy_of_vaporization(T)` (Watson), `boiling_temperature(p)`, `liquid_density(T)` (Rackett) and `phase_at(T, p)`.

`Species` wraps a substance in a particular phase, or holds your own ΔHf°, S° and Cp for something that isn't in the table.

Free functions cover ΔG = ΔH − TΔS, K from ΔG and back, van 't Hoff, Kirchhoff, Clausius–Clapeyron in all three directions, Trouton's rule, calorimetry, Kp ↔ Kc, ideal mixing and adiabatic flame temperature.

## Reactions (`reaction.hpp`)

```cpp
chem::Reaction r = chem::Reaction("MnO4- + Fe2+ + H+ -> Mn2+ + Fe3+ + H2O").balanced();
// MnO4^- + 5 Fe^2+ + 8 H^+ -> Mn^2+ + 5 Fe^3+ + 4 H2O
chem::Reaction("CH4(g) + 2O2(g) -> CO2(g) + 2H2O(l)").delta_h();   // -890.6 kJ
```

Arrows are `->`, `=>`, `=`, `→`, and `<=>`, `<->`, `⇌` for equilibria. Balancing is exact rational arithmetic on element and charge conservation, so ionic equations and half-reactions with `e-` work; `balance_is_unique()` says whether the answer is the only one. Reactions can be reversed, scaled, added and subtracted (Hess's law, cancelling species that appear on both sides).

With thermochemical data for every species there are `delta_h(T)`, `delta_s(T)`, `delta_g(T)`, `delta_cp(T)`, `equilibrium_constant(T)`, `kc(T)` and `spontaneity_temperature()`. Stoichiometry covers the limiting reactant, extent, product and leftover amounts, theoretical yield in moles or grams, percent yield and atom economy. `oxidation_changes()`, `is_redox()` and `electrons_transferred()` describe the redox, and `classify()` names it (synthesis, decomposition, single or double replacement, combustion, acid–base, redox).

## Equations of state (`equation_of_state.hpp`)

Everything derives from `EquationOfState`, which only needs `pressure(T, v)`. From that the base class finds every mechanically stable volume root and picks the stable one by Gibbs energy (or the liquid or vapor root you ask for). It computes compressibility, fugacity and its coefficient, and the residual Helmholtz energy, internal energy, enthalpy, entropy, Gibbs energy, Cv and Cp. Also derived are isothermal compressibility, thermal expansion, bulk modulus, the Joule–Thomson coefficient, speed of sound and heat-capacity ratio (given the ideal-gas Cp). It finds spinodals, saturation pressure and temperature (equal fugacity), the critical point, second and third virial coefficients, and the Boyle and inversion temperatures. Models override whatever they have in closed form.

| Model | Built from |
|---|---|
| `IdealGas` | |
| `VanDerWaals(a, b)` | `from_critical(Tc, Pc)`, `from_substance`; `a_in_liter_atm()`, `b_in_liters()` |
| `Berthelot`, `Clausius` | critical constants (Clausius also needs Vc) |
| `RedlichKwong`, `SoaveRedlichKwong`, `PengRobinson`, `PengRobinsonStryjekVera`, `PatelTeja` | critical constants and acentric factor |
| `CubicMixture` | cubic components of one family, mole fractions and kij, with van der Waals one-fluid mixing and per-component fugacity coefficients |
| `Dieterici`, `Wohl` | critical constants |
| `VirialEquation` | your own B(T), C(T), D(T) in density or pressure form, or Tsonopoulos / Pitzer–Abbott B with Orbey–Vera C |
| `LeeKesler` | critical constants and acentric factor; its fugacity and departure functions are the Lee–Kesler ones, interpolated at fixed Tr and Pr |
| `BeattieBridgeman`, `BenedictWebbRubin` | fitted constants, with `preset("nitrogen")` and the like for the common gases |
| `CarnahanStarling`, `CarnahanStarlingVanDerWaals` | hard-sphere diameter or critical constants |
| `NobleAbel`, `StiffenedGas` | covolume; γ, p∞ and Cv |
| `Tait`, `Murnaghan`, `BirchMurnaghan`, `Vinet` | reference volume, bulk modulus and its pressure derivative, for liquids and solids |

The cubic equations all use the generic form P = RT/(v − b) − a(T)/((v + εb)(v + σb)), so their roots, fugacities and departure functions are analytic. `make_equation_of_state(EosKind, substance)` builds any of them by name. There are also helpers for the ideal gas law, Kay's pseudo-critical rule, Rackett liquid volumes, and Wilson and Lee–Kesler vapor pressures.

## Bonds and shapes (`bonds.hpp`, `molecular_geometry.hpp`)

`Bond(a, b, order)` gives:
- `energy()`: average bond enthalpies (Huheey), else Pauling's estimate from electronegativity.
- `length(method)`: measured typical lengths, Pyykkö radii, Schomaker–Stevenson or Cordero.
- `electronegativity_difference()`, `ionic_character()` and `dipole_moment()`.
- `character()`: nonpolar, polar, ionic or metallic.
- `reduced_mass()`.

There are also bond order ↔ length (Pauling) and ΔH from bonds broken and formed. Force constants come from Gordy's rule or a measured wavenumber, along with vibrational frequencies. `MorsePotential` and `LennardJonesPotential` are there for simulations. Lattice energies come from Born–Landé, Born–Mayer, Kapustinskii (with Shannon radii) and the Born–Haber cycle, with Madelung constants for the common structures.

`Molecule` is a graph of atoms (with formal charges, optional isotopes and positions in pm) and bonds, for building compounds:

```cpp
chem::Molecule water;
const auto o = water.add_atom("O");
water.add_implicit_hydrogens();          // fills O up to its usual valence: H2O
water.geometry_at(o).molecular_geometry; // bent
water.lone_pairs(o);                     // 2
water.estimated_enthalpy_of_formation(); // kJ/mol, gas
```

It reports neighbors, degree, bond-order sums, lone pairs, the electrons around each atom (and whether that's an octet), and the usual valence and how many bonds an atom is missing. It also gives per-atom VSEPR geometry and hybridization, the formula and molar mass, fragments and ring count, and validity (no atom over-bonded or short of electrons). On the energy side there are bond energies, total bond energy, and ΔHf estimated as the atoms' gas-phase enthalpies of formation (`Element::atomization_enthalpy`, ATcT) minus the bond energies. With positions set, distances, angles and a bond-dipole estimate of the dipole moment are available too. `reaction_enthalpy_from_bonds` takes molecules with coefficients.

`vsepr(central, ligands, charge)` counts electrons with the ligand-valence rule, so ions work (NO₃⁻ is trigonal planar, I₃⁻ is linear). It gives the steric number, lone pairs, AXE notation, electron and molecular geometry, hybridization, ideal angles and whether the shape alone makes it polar. `formal_charge` is there too.

## Kinetics, electrochemistry, acids and bases, solutions, gases, spectroscopy

- **`kinetics.hpp`:**
  - rate constants: Arrhenius (and modified), activation energy from two rates, Eyring, collision theory, Q10
  - integrated rate laws: zeroth, first, second, nth and mixed second order; their half-lives; consecutive and reversible first-order reactions
  - Michaelis–Menten with competitive inhibition
  - `ReactionNetwork`: mass-action elementary steps, giving the derivative vector you can hand to `fizmo::numerics` ODE solvers, plus a built-in RK4 `simulate`
- **`electrochemistry.hpp`:** 72 standard reduction potentials (CRC electrochemical series) looked up by couple (`"Cu2+/Cu"`). `GalvanicCell::from_couples` gives E°, n, ΔG°, K and the Nernst potential. There are also the Nernst equation, ΔG ↔ E ↔ K, Faraday electrolysis, pH-dependent and concentration-cell potentials, Tafel and Butler–Volmer.
- **`acid_base.hpp`:**
  - Ka tables for 47 acids and Kb tables for 9 bases
  - pH, pOH, Ka ↔ pKa, Kw(T)
  - exact strong, weak and polyprotic pH by charge balance (`equilibrium_ph` takes any mix of acids, bases, strong acid and strong base)
  - Henderson–Hasselbalch, buffer capacity, species fractions, titration curves
- **`solutions.hpp`:**
  - concentration conversions, dilution
  - colligative properties, with Kb and Kf computed from a solvent's properties
  - Raoult's law and bubble and dew points; Henry's law with its temperature dependence
  - ionic strength; Debye–Hückel (limiting and extended) and Davies activity coefficients
  - Ksp ↔ solubility
- **`gas_kinetics.hpp`:**
  - molecular speeds and Maxwell–Boltzmann distributions
  - mean free path, collision frequency, effusion and Graham's law
  - partial pressures, the barometric formula
  - Chapman–Enskog viscosity and diffusion with Neufeld collision integrals; Eucken thermal conductivity (from a substance's Lennard-Jones data)
- **`spectroscopy.hpp`:** photon energy, wavelength and momentum, de Broglie wavelength, hydrogen-like levels and radii with the reduced-mass correction, Rydberg lines, Beer–Lambert, blackbody radiation, rigid-rotor and anharmonic-oscillator levels, Boltzmann populations.

`constants.hpp` has the CODATA 2022 constants the module uses and unit conversions (eV ↔ kJ/mol, atm, bar, torr, psi, °C, °F, litres, Å, bohr, debye).

## Lewis structures, resonance and layouts (`molecule.hpp`)

`Molecule::from_smiles("[O-]C(=O)[O-]")` reads SMILES, with charges, brackets, rings, branches and aromatic atoms, and adds the implicit hydrogens. On any molecule:
- `lewis_structure(charge)` keeps the drawn bond orders and hands out the rest of the valence electrons as lone pairs. Each atom gets its lone pairs, formal charge, electron count, unpaired electrons and octet status (expanded or incomplete), and the structure gets notes for anything odd: too many bonding electrons, radicals, broken octets.
- `best_lewis_structure(charge)` searches the bond orders for the structure with the smallest formal charges; `apply_lewis` writes it back.
- `resonance_structures(charge, max)` lists every best-scoring structure and the average bond order of each bond (1.33 for nitrate, 1.5 for benzene and sulfate).
- `partial_charges()` gives electronegativity-equalized partial charges.
- `layout_2d(bond_pm)` draws the molecule flat for a Lewis picture; `layout_3d()` builds a VSEPR shape (templates for up to seven electron domains, then a force field with lone-pair sites and random restarts for rings). `center`, `scale_positions`, `has_positions` and `remove_atom` round it out.

`Formula::display_string(unicode)` writes a formula the way it was typed, groups and hydrates included, with subscripts and superscript charges when `unicode` is true (`Fe₂(SO₄)₃`, `CuSO₄·5H₂O`, `SO₄²⁻`).

## Units (`units.hpp`)

Every quantity the chemistry module reads or prints can go through `units.hpp`. `Dimension` names 26 kinds of quantity (temperature, pressure, mass, volume, amount, molar energy, length, concentration, density, specific volume, dipole moment and so on), and each has a table of units with aliases (`"C"` is °C, `"A"` is Å, `"mmHg"`, `"psi"`, `"kcal/mol"`, `"cm³/g"`):

```cpp
chem::convert(25, "C", "F");                         // 77
chem::to_si(1, "atm", chem::Dimension::Pressure);    // 101325
chem::Quantity q = chem::Quantity::parse("250 mL");  // q.si() is 2.5e-4 m³
chem::format_quantity(373.15, chem::Dimension::Temperature, "°C"); // "100°C"
chem::unit_symbols(chem::Dimension::Pressure);       // Pa, kPa, MPa, ..., psi
```

`convert` works out the dimension from the two units, trying temperature before the others so `"C"` means Celsius rather than coulombs; pass a `Dimension` to be explicit.

## Names, ions and solubility (`nomenclature.hpp`, `solubility.hpp`)

`polyatomic_ions()` has 157 polyatomic ions (oxyanions of every common element, hydrogen salts, ammonium and hydronium, mercury(I) Hg₂²⁺, organic anions, transition-metal complexes), each with its other names, its acid and the acid's formula, a group and a SMILES string. `common_compounds()` holds the compounds with names that rules can't make (water, ammonia, glucose, ethanol and the rest). They're searchable with `find_polyatomic_ion`, `search_polyatomic_ions` and `find_common_compounds`.

`name_compound("Fe2(SO4)3")` names ionic compounds (Stock numerals only when the metal has more than one charge, classical names as alternatives), double salts, hydrates, binary and oxy acids, binary molecular compounds with prefixes, simple organics and carbohydrates, and explains the rule it used. `formula_from_name("iron(III) sulfate")` goes the other way. `parse_ion` reads `"SO42-"`, `"Fe3+"`, `"Hg2 2+"`, `"iron(III)"` or `"sulfate"`, and `combine_ions` gives the neutral formula and name.

`solubility(cation, anion)` and `solubility_of(formula)` apply the usual solubility rules and say which rule decided. `is_strong_acid`, `is_weak_acid`, `is_strong_base`, `is_weak_base` and `is_strong_electrolyte` classify formulas.

## Crystals: ionic and metallic (`crystal.hpp`)

`unit_cell(structure, a, c)` returns the sites of 21 structures (simple, body- and face-centered cubic, hcp, diamond, rock salt, CsCl, zinc blende, wurtzite, fluorite and antifluorite, rutile, corundum, NiAs, CdI₂, cuprite, β-cristobalite, ReO₃ and anti-ReO₃, anti-bixbyite, perovskite), and `build_crystal` repeats one into a block of atoms; `nearest_neighbor_pairs` finds the bonds.

`ionic_crystal(metal, nonmetal)` (or two `IonDescriptor`s, polyatomic ones included) works out the formula and name, the likely structure from the radius ratio, coordination numbers, nearest distance and cell, then the lattice energy four ways (Born–Landé, Born–Mayer, Kapustinskii and the Born–Haber cycle), the Madelung constant and Born exponent, density, ionic character, ΔHf, melting and boiling points, solubility and a step-by-step Born–Haber explanation.

`metallic_crystal({{Cu, 0.7}, {Zn, 0.3}})` decides between a pure metal, a substitutional or interstitial solid solution, limited solubility, an intermetallic and an amalgam by checking the Hume-Rothery rules. It names known alloys (brass, bronze, steel, solder...), and gives the structure, lattice constant, metallic radius, packing, density, valence electrons per atom, free-electron density, Fermi energy, temperature and velocity, melting point and cohesive energy. `build_metal` places the atoms, putting small atoms in interstitial holes.

## Orbitals and electron clouds (`orbitals.hpp`)

Hydrogen-like and Slater-type orbitals with Slater's-rules effective nuclear charges:
- `radial_wavefunction`, `radial_probability`, `real_spherical_harmonic`, `wavefunction` and `probability_density` for any n, l, m.
- `radial_distribution(element, n, l, r_max)` gives a `RadialCurve` with P(r), the cumulative probability, the most probable and mean radius, the 90% radius, node positions and the orbital energy. `atom_radial_distributions` gives one per occupied subshell (weighted by electrons, ions included) and `total_radial_density` the whole atom.
- `sample_orbital` and `sample_atom` draw random electron positions from |ψ|² for point-cloud pictures; `orbital_slice` and `atom_slice` give density (or signed ψ) on a plane through the nucleus.
- `orbital_name(2, 1, 0)` is `"2p_z"`; `orbital_shape_name`, node counts and `effective_nuclear_charge` are there too.

## Phase diagrams (`phase_diagram.hpp`)

`find_phase_properties("CO2")` gathers critical constants, triple point, normal melting and boiling points, enthalpies of fusion and vaporization, densities and the acentric factor. Anything missing is estimated and listed in `estimated`: the Joback group-contribution method for molecules (`joback(molecule)`), corresponding states, or element, ionic-crystal and metal data (`phase_properties` takes a `Substance`, `Element`, `Molecule`, `IonicCrystal` or `MetallicCrystal`).

`phase_diagram(props)` builds the sublimation, vaporization (Lee–Kesler anchored at the critical point) and fusion (Clausius–Clapeyron with the real volume change, so water's line leans left) curves. It marks the triple, critical and normal points and answers `phase_at(T, P)` and the boiling, melting and sublimation temperature at any pressure. `pvt_surface(diagram)` samples P over T and specific volume with Peng–Robinson, labels each point solid, liquid, gas, supercritical or two-phase, and returns the liquid–vapor dome and the triple line for a 3D picture.

## Predicting products (`prediction.hpp`)

`predict_products("Zn + CuSO4")` recognizes combustion, synthesis, decomposition, single replacement (with the metal and halogen activity series), double replacement (precipitates, gases, water), neutralization, metal and nonmetal oxides with water, weak acid and base ionization (as equilibria) and esterification. It returns the balanced reaction, its type, an explanation, notes, and the complete and net ionic equations with spectators. `ionic_equations(reaction)` does the ionic equations for any reaction.

## Solvers (`solvers.hpp`)

`chemistry_equations()` lists over 60 equations from gases, stoichiometry, solutions, acids and bases, thermochemistry, kinetics, electrochemistry and light. Each has its variables with dimensions and default units, and `solve_equation(eq, values, unknown)` solves for whichever one is missing (a log-spaced scan followed by Brent's method, so nonlinear ones like van der Waals work).

`solve_stoichiometry(reaction, knowns, T, P)` takes amounts of any species, as mass, moles, particles, a gas volume or a solution volume with molarity. It finds the limiting reactant and the extent of reaction, and gives every species' amount used, left and made, with the steps.

`bond_dissociation(a, b, order, T, P)` turns a bond's enthalpy into ΔH, ΔS (translational, rotational and electronic), ΔG, K and the fraction broken at any temperature and pressure, plus the temperature where half are broken. `percent_composition`, `empirical_formula_from_masses` and `molecular_formula` cover composition problems.

## Where the data comes from

The tables in `Source/Chemistry/Data` are generated by `tools/chemistry/generate_elements.py` and `tools/chemistry/generate_substances.py` (Python 3 with `mendeleev`, `chemicals` and `radioactivedecay` from pip). The generators also carry the hand-checked values listed below. `reference_data.hpp` (bonds, potentials, acids, Madelung constants) is written by hand.

| Data | Source |
|---|---|
| Atomic weights | CIAAW Standard Atomic Weights 2024 (Gd, Lu, Zr revised in 2024) |
| Nuclide masses, half-lives, spins, decay modes, abundances | AME2020 and NUBASE2020 (Kondev et al. 2021), IUPAC isotopic compositions |
| Small decay branches | ICRP-107 |
| Ionization energies, ground terms, configurations | NIST Atomic Spectra Database |
| Electron affinities | Hotop & Lineberger and later measurements, as compiled on the Wikipedia data page |
| Electronegativity, radii, bulk properties | CRC Handbook; Pyykkö 2009; Cordero 2008; Alvarez 2013; Bondi 1964 |
| Ionic radii | Shannon 1976 |
| Critical constants, formation enthalpies, entropies, Cp | `chemicals` package: IUPAC/DIPPR critical data, ATcT, NIST WebBook Shomate fits, Poling et al., CRC |
| Bond energies | Huheey's average bond energies |
| Reduction potentials | CRC electrochemical series (Vanýsek) |
| Acid constants | LibreTexts reference table E1 |
| Beattie–Bridgeman, Benedict–Webb–Rubin constants | Çengel & Boles, *Thermodynamics*, Table 3-4 |
| Group contributions | Joback & Reid 1987 |
| Solubility rules, activity series, ion names | standard general-chemistry tables (IUPAC Red Book 2005 for names) |
