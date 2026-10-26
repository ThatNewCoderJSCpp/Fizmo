#ifndef FIZMO_CHEMISTRY_MOLECULE_HPP
#define FIZMO_CHEMISTRY_MOLECULE_HPP

#include "bonds.hpp"
#include "formula.hpp"
#include "molecular_geometry.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct MoleculeAtom {
    const Element* element = nullptr;
    int formal_charge = 0;
    int mass_number = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct MoleculeBond {
    std::size_t a = 0;
    std::size_t b = 0;
    BondOrder order = BondOrder::Single;

    std::size_t other(std::size_t atom) const noexcept { return atom == a ? b : a; }
};

struct LewisAtom {
    int lone_pairs = 0;
    int formal_charge = 0;
    int electrons = 0;
    int unpaired = 0;
    bool octet = true;
    bool expanded_octet = false;
    bool incomplete_octet = false;
};

struct LewisStructure {
    bool ok = false;
    std::vector<LewisAtom> atoms;
    std::vector<BondOrder> bond_orders;
    int valence_electrons = 0;
    int bonding_electrons = 0;
    int lone_pair_electrons = 0;
    int unpaired_electrons = 0;
    int charge = 0;
    double score = 0.0;
    std::vector<std::string> notes;
};

struct ResonanceSet {
    std::vector<LewisStructure> structures;
    std::vector<double> average_bond_orders;

    std::size_t count() const noexcept { return structures.size(); }
    bool has_resonance() const noexcept { return structures.size() > 1; }
};

class Molecule {
public:
    Molecule() = default;

    static Molecule from_smiles(std::string_view smiles);

    std::size_t add_atom(const Element& element, int formal_charge = 0, int mass_number = 0);
    std::size_t add_atom(std::string_view symbol, int formal_charge = 0, int mass_number = 0);
    std::size_t add_bond(std::size_t a, std::size_t b, BondOrder order = BondOrder::Single);
    bool remove_bond(std::size_t a, std::size_t b);
    void set_bond_order(std::size_t a, std::size_t b, BondOrder order);
    void set_formal_charge(std::size_t atom, int charge);
    void set_position(std::size_t atom, double x_pm, double y_pm, double z_pm);
    std::size_t add_implicit_hydrogens();

    const std::vector<MoleculeAtom>& atoms() const noexcept { return m_atoms; }
    const std::vector<MoleculeBond>& bonds() const noexcept { return m_bonds; }
    std::size_t atom_count() const noexcept { return m_atoms.size(); }
    std::size_t bond_count() const noexcept { return m_bonds.size(); }
    const MoleculeBond* find_bond(std::size_t a, std::size_t b) const noexcept;

    std::vector<std::size_t> neighbors(std::size_t atom) const;
    int degree(std::size_t atom) const;
    double bond_order_sum(std::size_t atom) const;
    int lone_pairs(std::size_t atom) const;
    int electrons_around(std::size_t atom) const;
    int standard_valence(std::size_t atom) const;
    int missing_bonds(std::size_t atom) const;
    bool obeys_octet(std::size_t atom) const;
    bool is_overbonded(std::size_t atom) const;
    VseprResult geometry_at(std::size_t atom) const;
    Hybridization hybridization_at(std::size_t atom) const;

    int charge() const noexcept;
    Formula formula() const;
    double molar_mass() const;
    bool is_connected() const;
    std::vector<std::vector<std::size_t>> fragments() const;
    int ring_count() const;
    bool valid(std::string* reason = nullptr) const;

    double bond_energy(const MoleculeBond& bond) const;
    double total_bond_energy() const;
    double estimated_enthalpy_of_formation() const;
    double bond_length(const MoleculeBond& bond, BondLengthMethod method = BondLengthMethod::Table) const;
    double distance(std::size_t a, std::size_t b) const;
    double angle(std::size_t a, std::size_t center, std::size_t b) const;
    double dipole_moment() const;
    std::vector<double> partial_charges() const;
    std::string to_string() const;

    LewisStructure lewis_structure(int total_charge, bool keep_bond_orders = true) const;
    LewisStructure best_lewis_structure(int total_charge) const;
    ResonanceSet resonance_structures(int total_charge, std::size_t max_structures = 24) const;
    void apply_lewis(const LewisStructure& lewis);
    int total_formal_charge() const noexcept { return charge(); }

    void layout_2d(double bond_length_pm = 140.0);
    void layout_3d(int iterations = 600);
    void center();
    void scale_positions(double factor);
    bool has_positions() const noexcept;
    std::size_t remove_atom(std::size_t atom);

private:
    void require_atom(std::size_t atom) const;
    std::vector<MoleculeAtom> m_atoms;
    std::vector<MoleculeBond> m_bonds;
};

double reaction_enthalpy_from_bonds(const std::vector<std::pair<Molecule, double>>& reactants, const std::vector<std::pair<Molecule, double>>& products);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_MOLECULE_HPP
