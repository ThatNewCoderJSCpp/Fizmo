#ifndef FIZMO_CHEMISTRY_PHASE_DIAGRAM_HPP
#define FIZMO_CHEMISTRY_PHASE_DIAGRAM_HPP

#include "crystal.hpp"
#include "molecule.hpp"
#include "substance.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct JobackGroupCount {
    std::string group;
    int count = 0;
};

struct JobackEstimate {
    bool ok = false;
    std::vector<JobackGroupCount> groups;
    std::vector<std::string> unmatched;
    int atoms = 0;
    double boiling_point = NO_VALUE;
    double melting_point = NO_VALUE;
    double critical_temperature = NO_VALUE;
    double critical_pressure = NO_VALUE;
    double critical_volume = NO_VALUE;
    double acentric_factor = NO_VALUE;
    double enthalpy_of_formation = NO_VALUE;
    double gibbs_of_formation = NO_VALUE;
    double vaporization_enthalpy = NO_VALUE;
    double fusion_enthalpy = NO_VALUE;
    double cp_a = NO_VALUE;
    double cp_b = NO_VALUE;
    double cp_c = NO_VALUE;
    double cp_d = NO_VALUE;

    double ideal_gas_cp(double t) const noexcept { return cp_a + t * (cp_b + t * (cp_c + t * cp_d)); }
};

JobackEstimate joback(const Molecule& molecule);
double edmister_acentric_factor(double tb, double tc, double pc) noexcept;

struct PhaseProperties {
    std::string name;
    std::string formula;
    std::string source;
    double molar_mass = NO_VALUE;
    double critical_temperature = NO_VALUE;
    double critical_pressure = NO_VALUE;
    double critical_volume = NO_VALUE;
    double acentric_factor = NO_VALUE;
    double boiling_point = NO_VALUE;
    double melting_point = NO_VALUE;
    double triple_temperature = NO_VALUE;
    double triple_pressure = NO_VALUE;
    double vaporization_enthalpy = NO_VALUE;
    double fusion_enthalpy = NO_VALUE;
    double liquid_density = NO_VALUE;
    double solid_density = NO_VALUE;
    double solid_bulk_modulus = 8e9;
    const Substance* substance = nullptr;
    std::vector<std::string> estimated;

    bool usable() const noexcept { return critical_temperature == critical_temperature && critical_pressure == critical_pressure && triple_temperature == triple_temperature; }
    double fusion_volume_change() const noexcept;
};

PhaseProperties phase_properties(const Substance& substance);
PhaseProperties phase_properties(const Element& element);
PhaseProperties phase_properties(const Molecule& molecule, std::string_view name = std::string_view());
PhaseProperties phase_properties(const IonicCrystal& crystal);
PhaseProperties phase_properties(const MetallicCrystal& crystal);
PhaseProperties find_phase_properties(std::string_view name_or_formula);
void complete_phase_properties(PhaseProperties& props);

struct PhasePoint {
    double t = NO_VALUE;
    double p = NO_VALUE;
};

enum class PvtRegion : std::uint8_t {
    Solid,
    Liquid,
    Gas,
    Supercritical,
    LiquidVapor,
    SolidLiquid,
    SolidVapor
};

const char* to_string(PvtRegion r) noexcept;

struct PhaseDiagram {
    bool ok = false;
    PhaseProperties properties;
    std::vector<PhasePoint> sublimation;
    std::vector<PhasePoint> vaporization;
    std::vector<PhasePoint> fusion;
    PhasePoint triple;
    PhasePoint critical;
    PhasePoint normal_boiling;
    PhasePoint normal_melting;
    double t_min = 0.0;
    double t_max = 0.0;
    double p_min = 0.0;
    double p_max = 0.0;
    std::vector<std::string> notes;

    double vapor_pressure(double t) const;
    double sublimation_pressure(double t) const;
    double melting_pressure(double t) const;
    double melting_temperature(double p) const;
    double boiling_temperature(double p) const;
    double sublimation_temperature(double p) const;
    Phase phase_at(double t, double p) const;
    std::string describe(double t, double p) const;
};

PhaseDiagram phase_diagram(const PhaseProperties& properties, int points = 200);

struct PvtSurface {
    bool ok = false;
    int nt = 0;
    int nv = 0;
    std::vector<double> temperatures;
    std::vector<double> specific_volumes;
    std::vector<double> pressures;
    std::vector<PvtRegion> regions;
    std::vector<double> dome_t;
    std::vector<double> dome_liquid_v;
    std::vector<double> dome_vapor_v;
    std::vector<double> dome_p;
    double triple_t = NO_VALUE;
    double triple_p = NO_VALUE;
    double triple_solid_v = NO_VALUE;
    double triple_liquid_v = NO_VALUE;
    double triple_vapor_v = NO_VALUE;
    double critical_t = NO_VALUE;
    double critical_p = NO_VALUE;
    double critical_v = NO_VALUE;

    double pressure(int it, int iv) const { return pressures[static_cast<std::size_t>(it) * nv + iv]; }
    PvtRegion region(int it, int iv) const { return regions[static_cast<std::size_t>(it) * nv + iv]; }
};

PvtSurface pvt_surface(const PhaseDiagram& diagram, int nt = 48, int nv = 64);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_PHASE_DIAGRAM_HPP
