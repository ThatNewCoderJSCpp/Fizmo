#ifndef FIZMO_CHEMISTRY_FORMULA_HPP
#define FIZMO_CHEMISTRY_FORMULA_HPP

#include "periodic_table.hpp"
#include "oxidation_state.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fizmo {
namespace chemistry {

struct FormulaTerm {
    const Element* element = nullptr;
    int mass_number = 0;
    double count = 0.0;

    double mass() const noexcept;
    bool operator==(const FormulaTerm& o) const noexcept { return element == o.element && mass_number == o.mass_number && count == o.count; }
};

class Formula {
public:
    Formula() = default;
    Formula(const char* text) : Formula(parse(text)) {}
    Formula(const std::string& text) : Formula(parse(text)) {}
    explicit Formula(const Element& element, double count = 1.0, int mass_number = 0);

    static Formula parse(std::string_view text);
    static Formula electron();

    const std::vector<FormulaTerm>& terms() const noexcept { return m_terms; }
    int charge() const noexcept { return m_charge; }
    void set_charge(int charge) noexcept { m_charge = charge; }
    Phase phase() const noexcept { return m_phase; }
    void set_phase(Phase phase) noexcept { m_phase = phase; }
    const std::string& source() const noexcept { return m_source; }

    bool empty() const noexcept { return m_terms.empty(); }
    bool is_electron() const noexcept { return m_terms.empty() && m_charge == -1; }
    bool is_element() const noexcept;
    bool is_ion() const noexcept { return m_charge != 0; }
    bool is_integral() const noexcept;
    bool contains(int atomic_number) const noexcept { return count(atomic_number) > 0.0; }
    bool contains(const Element& e) const noexcept { return contains(e.atomic_number); }
    double count(int atomic_number) const noexcept;
    double count(const Element& e) const noexcept { return count(e.atomic_number); }
    double count(std::string_view symbol) const;
    double atom_count() const noexcept;
    int element_count() const noexcept;
    std::vector<std::pair<const Element*, double>> composition() const;
    std::vector<const Element*> elements() const;

    double molar_mass() const noexcept;
    double molar_mass_uncertainty() const noexcept;
    double monoisotopic_mass() const noexcept;
    int nominal_mass() const noexcept;
    double mass_fraction(int atomic_number) const noexcept;
    double mass_fraction(const Element& e) const noexcept { return mass_fraction(e.atomic_number); }
    double mass_percent(int atomic_number) const noexcept { return 100.0 * mass_fraction(atomic_number); }
    double moles(double grams) const noexcept { return grams / molar_mass(); }
    double grams(double moles) const noexcept { return moles * molar_mass(); }
    double molecules(double moles) const noexcept { return moles * AVOGADRO_CONSTANT; }

    double protons() const noexcept;
    double electrons() const noexcept { return protons() - m_charge; }
    double valence_electrons() const noexcept;
    double degree_of_unsaturation() const noexcept;
    bool is_organic() const noexcept;
    bool is_hydrocarbon() const noexcept;
    bool contains_metal() const noexcept;
    bool is_probably_ionic() const noexcept;
    bool is_diatomic() const noexcept;

    Formula empirical() const;
    Formula without_charge() const;
    Formula scaled(double factor) const;

    std::string to_string() const;
    std::string hill() const;
    std::string to_unicode_string() const;
    std::string display_string(bool unicode = false) const;
    std::string charge_string() const;

    Formula operator+(const Formula& o) const;
    Formula operator*(double factor) const { return scaled(factor); }
    Formula& operator+=(const Formula& o);
    bool operator==(const Formula& o) const noexcept;
    bool operator!=(const Formula& o) const noexcept { return !(*this == o); }
    bool same_composition(const Formula& o) const noexcept;

private:
    void add(const Element* e, int mass_number, double count);

    std::vector<FormulaTerm> m_terms;
    int m_charge = 0;
    Phase m_phase = Phase::Unknown;
    std::string m_source;
};

Formula operator*(double factor, const Formula& f);

struct OxidationAssignment {
    const Element* element = nullptr;
    AverageOxidationState state;
};

std::vector<OxidationAssignment> assign_oxidation_states(const Formula& formula);
AverageOxidationState oxidation_state_of(const Formula& formula, const Element& element);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_FORMULA_HPP
