#ifndef FIZMO_CHEMISTRY_CRYSTAL_HPP
#define FIZMO_CHEMISTRY_CRYSTAL_HPP

#include "nomenclature.hpp"
#include "solubility.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class CrystalStructure : std::uint8_t {
    SimpleCubic,
    BodyCenteredCubic,
    FaceCenteredCubic,
    HexagonalClosePacked,
    Diamond,
    RockSalt,
    CesiumChloride,
    ZincBlende,
    Wurtzite,
    Fluorite,
    Antifluorite,
    Rutile,
    Corundum,
    NickelArsenide,
    CadmiumIodide,
    Cuprite,
    BetaCristobalite,
    RheniumTrioxide,
    AntiRheniumTrioxide,
    AntiBixbyite,
    Perovskite,
    Unknown
};

const char* to_string(CrystalStructure s) noexcept;
const char* short_name(CrystalStructure s) noexcept;
CrystalStructure crystal_structure_named(std::string_view name) noexcept;

struct LatticeSite {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int species = 0;
};

struct UnitCell {
    CrystalStructure structure = CrystalStructure::Unknown;
    double a = 1.0;
    double b = 1.0;
    double c = 1.0;
    double gamma = 90.0;
    std::vector<LatticeSite> sites;
    int formula_units = 1;
    int species_count = 1;
    std::array<int, 2> coordination{ { 0, 0 } };
    std::array<const char*, 2> coordination_shape{ { "", "" } };
    double packing_fraction = NO_VALUE;

    std::array<double, 3> to_cartesian(double fx, double fy, double fz) const noexcept;
    double volume() const noexcept;
    bool hexagonal() const noexcept { return gamma > 100.0; }
};

UnitCell unit_cell(CrystalStructure structure, double a_pm, double c_pm = NO_VALUE);

struct CrystalAtom {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int species = 0;
    bool interstitial = false;
};

std::vector<CrystalAtom> build_crystal(const UnitCell& cell, int nx, int ny, int nz, bool include_boundary = true);
std::vector<std::pair<std::size_t, std::size_t>> nearest_neighbor_pairs(const std::vector<CrystalAtom>& atoms, double tolerance = 0.08);

double thermochemical_radius(const IonDescriptor& ion) noexcept;

struct IonicCrystal {
    bool ok = false;
    IonDescriptor cation;
    IonDescriptor anion;
    int cation_count = 0;
    int anion_count = 0;
    std::string formula;
    std::string name;
    CrystalStructure structure = CrystalStructure::Unknown;
    std::string structure_reason;
    double cation_radius = NO_VALUE;
    double anion_radius = NO_VALUE;
    double radius_ratio = NO_VALUE;
    int cation_coordination = 0;
    int anion_coordination = 0;
    double nearest_distance = NO_VALUE;
    double madelung_constant = NO_VALUE;
    double born_exponent = NO_VALUE;
    double lattice_energy_born_lande = NO_VALUE;
    double lattice_energy_born_mayer = NO_VALUE;
    double lattice_energy_kapustinskii = NO_VALUE;
    double lattice_energy_born_haber = NO_VALUE;
    double molar_mass = NO_VALUE;
    double density = NO_VALUE;
    double electronegativity_difference = NO_VALUE;
    double percent_ionic = NO_VALUE;
    double enthalpy_of_formation = NO_VALUE;
    double melting_point = NO_VALUE;
    double boiling_point = NO_VALUE;
    double ionization_energy_total = NO_VALUE;
    double electron_affinity_total = NO_VALUE;
    SolubilityResult solubility;
    UnitCell cell;
    std::vector<std::string> steps;
    std::vector<std::string> notes;

    double lattice_energy() const noexcept;
};

IonicCrystal ionic_crystal(const IonDescriptor& cation, const IonDescriptor& anion);
IonicCrystal ionic_crystal(const Element& metal, const Element& nonmetal);
std::vector<int> likely_cation_charges(const Element& metal);
int likely_anion_charge(const Element& nonmetal);

struct AlloyComponent {
    const Element* element = nullptr;
    double fraction = 0.0;
};

enum class AlloyType : std::uint8_t {
    PureMetal,
    Substitutional,
    Interstitial,
    LimitedSolubility,
    Intermetallic,
    Amalgam
};

const char* to_string(AlloyType t) noexcept;

struct HumeRotheryCheck {
    std::string rule;
    std::string detail;
    bool passes = false;
};

struct MetallicCrystal {
    bool ok = false;
    std::vector<AlloyComponent> components;
    AlloyType type = AlloyType::PureMetal;
    CrystalStructure structure = CrystalStructure::Unknown;
    std::string structure_note;
    std::string name;
    std::vector<std::string> other_names;
    std::string formula;
    double lattice_constant = NO_VALUE;
    double lattice_c = NO_VALUE;
    double metallic_radius = NO_VALUE;
    double molar_mass = NO_VALUE;
    double density = NO_VALUE;
    double packing_fraction = NO_VALUE;
    int coordination = 0;
    double valence_electrons_per_atom = NO_VALUE;
    double electron_density = NO_VALUE;
    double fermi_energy = NO_VALUE;
    double fermi_temperature = NO_VALUE;
    double fermi_velocity = NO_VALUE;
    double melting_point = NO_VALUE;
    double boiling_point = NO_VALUE;
    double cohesive_energy = NO_VALUE;
    double electronegativity_spread = NO_VALUE;
    double size_mismatch = NO_VALUE;
    std::vector<HumeRotheryCheck> checks;
    std::vector<std::string> notes;
    UnitCell cell;
};

MetallicCrystal metallic_crystal(const std::vector<AlloyComponent>& components);
MetallicCrystal metallic_crystal(const Element& metal);
std::vector<CrystalAtom> build_metal(const MetallicCrystal& crystal, int nx, int ny, int nz, unsigned seed = 1);
int free_electron_valence(const Element& e) noexcept;
double metallic_lattice_constant(CrystalStructure s, double radius_pm) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_CRYSTAL_HPP
