#include "molecular_geometry.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double TETRAHEDRAL_ANGLE = 109.47122063449069;

} // namespace

Geometry electron_geometry(int steric_number) noexcept {
    switch (steric_number) {
        case 0: return Geometry::Single;
        case 1:
        case 2: return Geometry::Linear;
        case 3: return Geometry::TrigonalPlanar;
        case 4: return Geometry::Tetrahedral;
        case 5: return Geometry::TrigonalBipyramidal;
        case 6: return Geometry::Octahedral;
        case 7: return Geometry::PentagonalBipyramidal;
        default: return Geometry::Unknown;
    }
}

Geometry molecular_geometry(int bonded_atoms, int lone_pairs) noexcept {
    if (bonded_atoms <= 0) return Geometry::Single;
    if (bonded_atoms == 1) return Geometry::Linear;
    switch (bonded_atoms + lone_pairs) {
        case 2: return Geometry::Linear;
        case 3: return lone_pairs == 0 ? Geometry::TrigonalPlanar : Geometry::Bent;
        case 4:
            if (lone_pairs == 0) return Geometry::Tetrahedral;
            return lone_pairs == 1 ? Geometry::TrigonalPyramidal : Geometry::Bent;
        case 5:
            if (lone_pairs == 0) return Geometry::TrigonalBipyramidal;
            if (lone_pairs == 1) return Geometry::Seesaw;
            return lone_pairs == 2 ? Geometry::TShaped : Geometry::Linear;
        case 6:
            if (lone_pairs == 0) return Geometry::Octahedral;
            if (lone_pairs == 1) return Geometry::SquarePyramidal;
            if (lone_pairs == 2) return Geometry::SquarePlanar;
            return lone_pairs == 3 ? Geometry::TShaped : Geometry::Linear;
        case 7:
            if (lone_pairs == 0) return Geometry::PentagonalBipyramidal;
            if (lone_pairs == 1) return Geometry::PentagonalPyramidal;
            if (lone_pairs == 2) return Geometry::PentagonalPlanar;
            return Geometry::Unknown;
        default: return Geometry::Unknown;
    }
}

Hybridization hybridization(int steric_number) noexcept {
    switch (steric_number) {
        case 1: return Hybridization::S;
        case 2: return Hybridization::SP;
        case 3: return Hybridization::SP2;
        case 4: return Hybridization::SP3;
        case 5: return Hybridization::SP3D;
        case 6: return Hybridization::SP3D2;
        case 7: return Hybridization::SP3D3;
        default: return Hybridization::Unknown;
    }
}

std::vector<double> ideal_bond_angles(int bonded_atoms, int lone_pairs) {
    switch (molecular_geometry(bonded_atoms, lone_pairs)) {
        case Geometry::Linear: return bonded_atoms >= 2 ? std::vector<double>{ 180.0 } : std::vector<double>{};
        case Geometry::Bent: return { bonded_atoms + lone_pairs == 3 ? 120.0 : TETRAHEDRAL_ANGLE };
        case Geometry::TrigonalPlanar: return { 120.0 };
        case Geometry::TrigonalPyramidal: return { TETRAHEDRAL_ANGLE };
        case Geometry::Tetrahedral: return { TETRAHEDRAL_ANGLE };
        case Geometry::TShaped: return { 90.0, 180.0 };
        case Geometry::Seesaw: return { 90.0, 120.0, 180.0 };
        case Geometry::SquarePlanar: return { 90.0, 180.0 };
        case Geometry::TrigonalBipyramidal: return { 90.0, 120.0, 180.0 };
        case Geometry::SquarePyramidal: return { 90.0, 180.0 };
        case Geometry::Octahedral: return { 90.0, 180.0 };
        case Geometry::PentagonalPlanar: return { 72.0, 144.0 };
        case Geometry::PentagonalPyramidal: return { 72.0, 90.0, 144.0 };
        case Geometry::PentagonalBipyramidal: return { 72.0, 90.0, 144.0, 180.0 };
        default: return {};
    }
}

bool is_shape_symmetric(Geometry g) noexcept {
    switch (g) {
        case Geometry::Single:
        case Geometry::Linear:
        case Geometry::TrigonalPlanar:
        case Geometry::Tetrahedral:
        case Geometry::SquarePlanar:
        case Geometry::TrigonalBipyramidal:
        case Geometry::Octahedral:
        case Geometry::PentagonalPlanar:
        case Geometry::PentagonalBipyramidal:
            return true;
        default:
            return false;
    }
}

VseprResult vsepr(int bonded_atoms, int lone_pairs) {
    if (bonded_atoms < 0 || lone_pairs < 0) throw std::invalid_argument("fizmo::chemistry: counts must not be negative");
    VseprResult r;
    r.bonded_atoms = bonded_atoms;
    r.lone_pairs = lone_pairs;
    r.steric_number = bonded_atoms + lone_pairs;
    r.electron_geometry = electron_geometry(r.steric_number);
    r.molecular_geometry = molecular_geometry(bonded_atoms, lone_pairs);
    r.hybridization = hybridization(r.steric_number);
    r.ideal_angles = ideal_bond_angles(bonded_atoms, lone_pairs);
    r.polar_by_shape = !is_shape_symmetric(r.molecular_geometry);
    r.axe_notation = "AX" + (bonded_atoms != 1 ? std::to_string(bonded_atoms) : std::string()) +
                     (lone_pairs > 0 ? "E" + (lone_pairs != 1 ? std::to_string(lone_pairs) : std::string()) : std::string());
    return r;
}

int ligand_electron_contribution(const Element& ligand) noexcept {
    if (ligand.atomic_number == 1) return 1;
    if (ligand.group == 17) return 1;
    if (ligand.group == 16) return 0;
    if (ligand.group == 15) return -1;
    if (ligand.group == 14) return -2;
    return 1;
}

VseprResult vsepr(const Element& central, const std::vector<const Element*>& ligands, int charge) {
    int electrons = central.valence_electrons() - charge;
    bool mixed_ligands = false;
    for (const Element* l : ligands) {
        if (!l) throw std::invalid_argument("fizmo::chemistry: null ligand");
        electrons += ligand_electron_contribution(*l);
        if (l != ligands.front()) mixed_ligands = true;
    }
    if (electrons < 0 || electrons % 2 != 0) electrons += 1;
    const int steric = electrons / 2;
    int lone = steric - static_cast<int>(ligands.size());
    if (lone < 0) lone = 0;
    VseprResult r = vsepr(static_cast<int>(ligands.size()), lone);
    if (mixed_ligands) r.polar_by_shape = true;
    return r;
}

VseprResult vsepr(std::string_view central, const std::vector<std::string_view>& ligands, int charge) {
    std::vector<const Element*> l;
    for (std::string_view s : ligands) l.push_back(&PeriodicTable::element(s));
    return vsepr(PeriodicTable::element(central), l, charge);
}

int formal_charge(int valence_electrons, int nonbonding_electrons, int bonding_electrons) noexcept {
    return valence_electrons - nonbonding_electrons - bonding_electrons / 2;
}

int formal_charge(const Element& atom, int lone_pair_electrons, double bond_order_sum) noexcept {
    return atom.valence_electrons() - lone_pair_electrons - static_cast<int>(std::lround(bond_order_sum));
}

int lone_pairs_on_central(const Element& central, double bond_order_sum, int formal_charge_on_central) noexcept {
    const int remaining = central.valence_electrons() - formal_charge_on_central - static_cast<int>(std::lround(bond_order_sum));
    return remaining > 0 ? remaining / 2 : 0;
}

} // namespace chemistry
} // namespace fizmo
