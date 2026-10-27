#include "intermolecular.hpp"
#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace fizmo {
namespace chemistry {

namespace {

std::string symbol_at(const Molecule& m, std::size_t i) {
    return m.atoms()[i].element->symbol;
}

int bonded_heavy_or_h(const Molecule& m, std::size_t i, const char* symbol) {
    int n = 0;
    for (std::size_t j : m.neighbors(i)) n += symbol_at(m, j) == symbol;
    return n;
}

int implicit_hydrogens(const Molecule& m, std::size_t i) {
    return std::max(0, m.missing_bonds(i));
}

}

double atomic_polarizability(const Molecule& m, std::size_t i) {
    const std::string s = symbol_at(m, i);
    double max_order = 1.0;
    int doubles = 0;
    for (const MoleculeBond& b : m.bonds()) {
        if (b.a != i && b.b != i) continue;
        const double o = numeric_bond_order(b.order);
        max_order = std::max(max_order, o);
        doubles += b.order == BondOrder::Double || b.order == BondOrder::Aromatic;
    }
    const bool triple = max_order >= 3.0 || doubles >= 2;
    const bool planar = !triple && max_order > 1.0;
    if (s == "H") return 0.387;
    if (s == "C") return triple ? 1.283 : planar ? 1.352 : 1.064;
    if (s == "N") return triple ? 0.956 : planar ? 1.030 : 0.964;
    if (s == "O") return planar ? 0.569 : 0.637;
    if (s == "F") return 0.296;
    if (s == "Cl") return 2.315;
    if (s == "Br") return 3.013;
    if (s == "I") return 5.415;
    if (s == "S") return planar ? 3.729 : 3.000;
    if (s == "P") return 2.630;
    if (s == "Si") return 3.600;
    if (s == "B") return 1.300;
    if (s == "Se") return 3.800;
    const Element& e = *m.atoms()[i].element;
    return e.polarizability == e.polarizability && e.polarizability > 0.0 ? e.polarizability : 1.5;
}

double molecular_polarizability(const Molecule& m) {
    double alpha = 0.0;
    for (std::size_t i = 0; i < m.atom_count(); ++i) {
        alpha += atomic_polarizability(m, i);
        if (symbol_at(m, i) != "H") alpha += 0.387 * implicit_hydrogens(m, i);
    }
    return alpha;
}

IntermolecularForces intermolecular_forces(const Molecule& m, double dipole, int charge) {
    IntermolecularForces out;
    if (m.atom_count() == 0) {
        out.message = "no atoms";
        return out;
    }
    out.charge = charge;
    out.dipole = dipole == dipole ? std::fabs(dipole) : 0.0;
    out.polarizability = molecular_polarizability(m);
    int polar_atoms = 0;
    for (std::size_t i = 0; i < m.atom_count(); ++i) {
        const Element& e = *m.atoms()[i].element;
        out.electrons += e.atomic_number;
        const std::string s = e.symbol;
        if (s == "C") ++out.carbons;
        if (s == "N" || s == "O" || s == "F") {
            ++polar_atoms;
            out.hydrogen_donors += bonded_heavy_or_h(m, i, "H") + implicit_hydrogens(m, i);
            int pairs = 0;
            try {
                pairs = m.lone_pairs(i);
            } catch (...) {
                pairs = s == "N" ? 1 : s == "O" ? 2 : 3;
            }
            out.hydrogen_acceptor_pairs += std::max(0, pairs);
        }
    }
    out.electrons -= charge;
    if (out.hydrogen_donors > 0 && out.hydrogen_acceptor_pairs > 0) out.hydrogen_bonds_per_molecule = std::min(out.hydrogen_donors, out.hydrogen_acceptor_pairs);
    const double alpha = out.polarizability;

    IntermolecularForce london;
    london.name = "London dispersion";
    london.present = true;
    london.strength = alpha / (alpha + 12.0);
    london.typical_energy = "about 0.5 to 40 kJ/mol, growing with size";
    london.detail = "every molecule has them: its " + std::to_string(out.electrons) + " electrons form brief, shifting dipoles. Polarizability about " + format_number(alpha, 3) + " \xC3\x85\xC2\xB3; bigger, longer and flatter molecules have more";
    out.forces.push_back(london);

    IntermolecularForce dd;
    dd.name = "dipole\xE2\x80\x93" "dipole";
    dd.present = out.dipole > 0.3 && charge == 0;
    dd.strength = dd.present ? out.dipole * out.dipole / (out.dipole * out.dipole + 3.0) : 0.0;
    dd.typical_energy = "about 2 to 15 kJ/mol";
    dd.detail = dd.present ? "the molecule is polar (" + format_number(out.dipole, 3) + " D), so the + end of one lines up with the \xE2\x88\x92 end of the next"
                           : (out.dipole > 0.0 ? "only " + format_number(out.dipole, 2) + " D: too small to matter much" : "nonpolar: the bond dipoles cancel or there are none");
    out.forces.push_back(dd);

    IntermolecularForce hb;
    hb.name = "hydrogen bonding";
    hb.present = out.hydrogen_bonds_per_molecule > 0;
    hb.strength = std::min(1.0, 0.45 * out.hydrogen_bonds_per_molecule);
    hb.typical_energy = "about 10 to 40 kJ/mol per bond";
    if (hb.present) {
        hb.detail = std::to_string(out.hydrogen_donors) + " H on N, O or F to give and " + std::to_string(out.hydrogen_acceptor_pairs) + " lone pair" + (out.hydrogen_acceptor_pairs == 1 ? "" : "s") + " on N, O or F to accept, so about " + std::to_string(out.hydrogen_bonds_per_molecule) + " hydrogen bond" + (out.hydrogen_bonds_per_molecule == 1 ? "" : "s") + " per molecule in the pure liquid";
    } else if (out.hydrogen_acceptor_pairs > 0 && charge == 0) {
        hb.detail = "no H on N, O or F, so it can't hydrogen-bond with itself, but its " + std::to_string(out.hydrogen_acceptor_pairs) + " lone pair" + (out.hydrogen_acceptor_pairs == 1 ? "" : "s") + " on N, O or F can accept hydrogen bonds from water or alcohols";
    } else if (out.hydrogen_donors > 0) {
        hb.detail = "has H on N, O or F but no lone pairs to accept with";
    } else {
        hb.detail = "no H on N, O or F and no N, O or F lone pairs";
    }
    out.forces.push_back(hb);

    IntermolecularForce ion;
    ion.name = "ion\xE2\x80\x93" "dipole";
    ion.present = charge != 0;
    ion.strength = ion.present ? 1.0 : 0.0;
    ion.typical_energy = "about 40 to 600 kJ/mol (hydration)";
    ion.detail = ion.present ? std::string("an ion: water molecules crowd around it, ") + (charge > 0 ? "their O ends" : "their H ends") + " pointing in" : "not an ion";
    out.forces.push_back(ion);

    if (charge != 0) out.strongest = "ion\xE2\x80\x93" "dipole";
    else if (hb.present && alpha < 18.0) out.strongest = "hydrogen bonding";
    else if (hb.present) out.strongest = "London dispersion (this molecule is big enough that its dispersion forces add up to more than its hydrogen bonds)";
    else if (dd.present && alpha < 10.0) out.strongest = "dipole\xE2\x80\x93" "dipole";
    else out.strongest = dd.present ? "London dispersion (with some dipole\xE2\x80\x93" "dipole help)" : "London dispersion";

    if (charge != 0) out.boiling = "ions don't boil on their own: in a solid they're held by ionic bonds, in water by ion\xE2\x80\x93" "dipole forces";
    else if (hb.present) out.boiling = "boils far higher than a nonpolar molecule of the same size, because hydrogen bonds must break";
    else if (dd.present) out.boiling = "boils a little higher than a nonpolar molecule of the same size";
    else out.boiling = "boiling point rises with size (more electrons, more dispersion); small nonpolar molecules are gases at room temperature";

    const int polar_groups = std::max(1, polar_atoms);
    if (charge != 0) out.water_solubility = "dissolves: water's dipoles surround the ion";
    else if ((hb.present || out.hydrogen_acceptor_pairs > 0) && out.carbons <= 4 * std::max(1, polar_groups)) out.water_solubility = "soluble: it can hydrogen-bond with water";
    else if (hb.present || out.hydrogen_acceptor_pairs > 0) out.water_solubility = "only slightly soluble: its nonpolar carbon part is too big for its polar group";
    else if (dd.present) out.water_solubility = "slightly soluble: polar, but it can't hydrogen-bond with water";
    else out.water_solubility = "insoluble: water would have to break its hydrogen bonds and get nothing back (\"like dissolves like\")";

    if (charge != 0 || hb.present) out.solvent_partners = "mixes with water, alcohols and other polar solvents";
    else if (dd.present) out.solvent_partners = "mixes with polar aprotic solvents (acetone, dichloromethane) and many organic liquids";
    else out.solvent_partners = "mixes with nonpolar solvents like hexane, oils and fats";
    out.ok = true;
    return out;
}

} // namespace chemistry
} // namespace fizmo
