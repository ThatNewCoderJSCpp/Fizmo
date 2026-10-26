#ifndef FIZMO_CHEMISTRY_ATOM_HPP
#define FIZMO_CHEMISTRY_ATOM_HPP

#include "periodic_table.hpp"
#include "oxidation_state.hpp"
#include "electron_configuration.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace fizmo {
namespace chemistry {

class Atom {
public:
    static constexpr int MIN_CHARGE = -8;

    constexpr Atom(const Element& element, int charge = 0, int mass_number = 0)
        : m_element(&element),
          m_mass_number(checked_mass_number(element, mass_number)),
          m_charge(checked_charge(element, charge)),
          m_oxidation(OxidationState::from_charge(charge)) {}

    constexpr explicit Atom(int atomic_number, int charge = 0, int mass_number = 0)
        : Atom(PeriodicTable::element(atomic_number), charge, mass_number) {}

    constexpr Atom(const Isotope& isotope, int charge = 0)
        : Atom(isotope.element(), charge, isotope.mass_number) {}

    static Atom parse(std::string_view text);

    constexpr const Element& element() const noexcept { return *m_element; }
    constexpr int atomic_number() const noexcept { return m_element->atomic_number; }
    constexpr const char* symbol() const noexcept { return m_element->symbol; }
    constexpr int mass_number() const noexcept { return m_mass_number != 0 ? m_mass_number : m_element->reference_mass_number; }
    constexpr bool is_specific_isotope() const noexcept { return m_mass_number != 0; }
    constexpr const Isotope* isotope() const noexcept { return m_mass_number != 0 ? m_element->isotope(m_mass_number) : nullptr; }

    constexpr int charge() const noexcept { return m_charge; }
    constexpr void set_charge(int charge, bool update_oxidation_state = true) {
        m_charge = checked_charge(*m_element, charge);
        if (update_oxidation_state) m_oxidation = OxidationState::from_charge(charge);
    }
    constexpr OxidationState oxidation_state() const noexcept { return m_oxidation; }
    constexpr void set_oxidation_state(OxidationState state) noexcept { m_oxidation = state; }
    constexpr void set_oxidation_state(int state) { m_oxidation = OxidationState(state); }
    constexpr bool oxidation_state_is_usual() const noexcept {
        return !m_oxidation.known() || m_element->has_oxidation_state(m_oxidation.value_or(0));
    }

    constexpr Atom ionized(int extra_charge) const {
        Atom a = *this;
        a.set_charge(m_charge + extra_charge);
        return a;
    }
    constexpr Atom with_mass_number(int mass_number) const { return Atom(*m_element, m_charge, mass_number); }
    constexpr Atom neutral() const { return Atom(*m_element, 0, m_mass_number); }

    constexpr int protons() const noexcept { return m_element->atomic_number; }
    constexpr int electrons() const noexcept { return m_element->atomic_number - m_charge; }
    constexpr int neutrons() const noexcept { return mass_number() - m_element->atomic_number; }
    constexpr bool is_neutral() const noexcept { return m_charge == 0; }
    constexpr bool is_cation() const noexcept { return m_charge > 0; }
    constexpr bool is_anion() const noexcept { return m_charge < 0; }
    constexpr bool is_ion() const noexcept { return m_charge != 0; }

    constexpr double mass() const noexcept {
        const Isotope* i = isotope();
        const double neutral_mass = i ? i->mass : m_element->atomic_weight;
        return neutral_mass - m_charge * ELECTRON_MASS_U;
    }
    constexpr double mass_kg() const noexcept { return mass() * ATOMIC_MASS_CONSTANT; }
    constexpr double molar_mass() const noexcept { return mass(); }
    constexpr double charge_coulombs() const noexcept { return m_charge * ELEMENTARY_CHARGE; }
    constexpr double mass_to_charge() const noexcept { return m_charge != 0 ? mass() / (m_charge < 0 ? -m_charge : m_charge) : NO_VALUE; }

    constexpr double next_ionization_energy() const noexcept {
        if (m_charge >= 0) return m_element->ionization_energy(m_charge + 1);
        return m_charge == -1 ? m_element->electron_affinity : NO_VALUE;
    }
    constexpr double formation_energy() const noexcept { return m_element->cumulative_ionization_energy(m_charge); }

    constexpr const IonicRadius* ionic_radius_entry(int coordination = 6) const noexcept {
        return m_charge != 0 ? m_element->ionic_radius(m_charge, coordination) : nullptr;
    }
    constexpr double ionic_radius(int coordination = 6) const noexcept {
        const IonicRadius* r = ionic_radius_entry(coordination);
        return r ? r->ionic_radius : NO_VALUE;
    }
    constexpr double radius(int coordination = 6) const noexcept {
        return m_charge != 0 ? ionic_radius(coordination) : m_element->covalent_radius;
    }

    ElectronConfiguration electron_configuration() const { return ElectronConfiguration::of_ion(*m_element, m_charge); }
    int unpaired_electrons() const { return electron_configuration().unpaired_electrons(); }
    bool is_isoelectronic_with(const Atom& other) const { return electrons() == other.electrons(); }

    std::string to_string() const;
    std::string to_unicode_string() const;
    std::string stock_name() const;

    constexpr bool operator==(const Atom& o) const noexcept {
        return m_element == o.m_element && m_mass_number == o.m_mass_number && m_charge == o.m_charge && m_oxidation == o.m_oxidation;
    }
    constexpr bool operator!=(const Atom& o) const noexcept { return !(*this == o); }

private:
    static constexpr int checked_mass_number(const Element& e, int mass_number) {
        return mass_number == 0 ? 0 : (e.isotope(mass_number) ? mass_number : throw std::invalid_argument("fizmo::chemistry: that isotope is not known"));
    }
    static constexpr int checked_charge(const Element& e, int charge) {
        return (charge >= MIN_CHARGE && charge <= e.atomic_number) ? charge : throw std::out_of_range("fizmo::chemistry: charge must be between -8 and the atomic number");
    }

    const Element* m_element;
    int m_mass_number;
    int m_charge;
    OxidationState m_oxidation;
};

using Ion = Atom;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ATOM_HPP
