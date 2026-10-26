#ifndef FIZMO_CHEMISTRY_MOLECULAR_GEOMETRY_HPP
#define FIZMO_CHEMISTRY_MOLECULAR_GEOMETRY_HPP

#include "periodic_table.hpp"

#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class Geometry : std::uint8_t {
    Single,
    Linear,
    Bent,
    TrigonalPlanar,
    TrigonalPyramidal,
    TShaped,
    Tetrahedral,
    Seesaw,
    SquarePlanar,
    TrigonalBipyramidal,
    SquarePyramidal,
    Octahedral,
    PentagonalPlanar,
    PentagonalPyramidal,
    PentagonalBipyramidal,
    Unknown
};

enum class Hybridization : std::uint8_t {
    S,
    SP,
    SP2,
    SP3,
    SP3D,
    SP3D2,
    SP3D3,
    Unknown
};

constexpr const char* to_string(Geometry g) noexcept {
    switch (g) {
        case Geometry::Single: return "single atom";
        case Geometry::Linear: return "linear";
        case Geometry::Bent: return "bent";
        case Geometry::TrigonalPlanar: return "trigonal planar";
        case Geometry::TrigonalPyramidal: return "trigonal pyramidal";
        case Geometry::TShaped: return "T-shaped";
        case Geometry::Tetrahedral: return "tetrahedral";
        case Geometry::Seesaw: return "seesaw";
        case Geometry::SquarePlanar: return "square planar";
        case Geometry::TrigonalBipyramidal: return "trigonal bipyramidal";
        case Geometry::SquarePyramidal: return "square pyramidal";
        case Geometry::Octahedral: return "octahedral";
        case Geometry::PentagonalPlanar: return "pentagonal planar";
        case Geometry::PentagonalPyramidal: return "pentagonal pyramidal";
        case Geometry::PentagonalBipyramidal: return "pentagonal bipyramidal";
        default: return "unknown";
    }
}

constexpr const char* to_string(Hybridization h) noexcept {
    switch (h) {
        case Hybridization::S: return "s";
        case Hybridization::SP: return "sp";
        case Hybridization::SP2: return "sp2";
        case Hybridization::SP3: return "sp3";
        case Hybridization::SP3D: return "sp3d";
        case Hybridization::SP3D2: return "sp3d2";
        case Hybridization::SP3D3: return "sp3d3";
        default: return "unknown";
    }
}

struct VseprResult {
    int bonded_atoms = 0;
    int lone_pairs = 0;
    int steric_number = 0;
    Geometry electron_geometry = Geometry::Unknown;
    Geometry molecular_geometry = Geometry::Unknown;
    Hybridization hybridization = Hybridization::Unknown;
    std::vector<double> ideal_angles;
    bool polar_by_shape = false;
    std::string axe_notation;
};

Geometry electron_geometry(int steric_number) noexcept;
Geometry molecular_geometry(int bonded_atoms, int lone_pairs) noexcept;
Hybridization hybridization(int steric_number) noexcept;
std::vector<double> ideal_bond_angles(int bonded_atoms, int lone_pairs);
VseprResult vsepr(int bonded_atoms, int lone_pairs);
VseprResult vsepr(const Element& central, const std::vector<const Element*>& ligands, int charge = 0);
VseprResult vsepr(std::string_view central, const std::vector<std::string_view>& ligands, int charge = 0);
int ligand_electron_contribution(const Element& ligand) noexcept;
int formal_charge(int valence_electrons, int nonbonding_electrons, int bonding_electrons) noexcept;
int formal_charge(const Element& atom, int lone_pair_electrons, double bond_order_sum) noexcept;
int lone_pairs_on_central(const Element& central, double bond_order_sum, int formal_charge_on_central = 0) noexcept;
bool is_shape_symmetric(Geometry g) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_MOLECULAR_GEOMETRY_HPP
