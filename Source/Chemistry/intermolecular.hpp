#ifndef FIZMO_CHEMISTRY_INTERMOLECULAR_HPP
#define FIZMO_CHEMISTRY_INTERMOLECULAR_HPP

#include "molecule.hpp"

#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

struct IntermolecularForce {
    std::string name;
    bool present = false;
    double strength = 0.0;
    std::string typical_energy;
    std::string detail;
};

struct IntermolecularForces {
    bool ok = false;
    std::string message;
    double polarizability = NO_VALUE;
    int electrons = 0;
    double dipole = NO_VALUE;
    int hydrogen_donors = 0;
    int hydrogen_acceptor_pairs = 0;
    int hydrogen_bonds_per_molecule = 0;
    int charge = 0;
    int carbons = 0;
    std::vector<IntermolecularForce> forces;
    std::string strongest;
    std::string boiling;
    std::string water_solubility;
    std::string solvent_partners;
};

double atomic_polarizability(const Molecule& molecule, std::size_t atom);
double molecular_polarizability(const Molecule& molecule);
IntermolecularForces intermolecular_forces(const Molecule& molecule, double dipole_debye, int charge = 0);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_INTERMOLECULAR_HPP
