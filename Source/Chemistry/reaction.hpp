#ifndef FIZMO_CHEMISTRY_REACTION_HPP
#define FIZMO_CHEMISTRY_REACTION_HPP

#include "formula.hpp"
#include "thermochemistry.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class ReactionType : std::uint8_t {
    Synthesis,
    Decomposition,
    SingleReplacement,
    DoubleReplacement,
    Combustion,
    AcidBase,
    Redox,
    Other
};

constexpr const char* to_string(ReactionType t) noexcept {
    switch (t) {
        case ReactionType::Synthesis: return "synthesis";
        case ReactionType::Decomposition: return "decomposition";
        case ReactionType::SingleReplacement: return "single replacement";
        case ReactionType::DoubleReplacement: return "double replacement";
        case ReactionType::Combustion: return "combustion";
        case ReactionType::AcidBase: return "acid-base";
        case ReactionType::Redox: return "redox";
        default: return "other";
    }
}

struct ReactionTerm {
    double coefficient = 1.0;
    Formula formula;

    Phase phase() const noexcept { return formula.phase(); }
    std::string to_string(bool unicode = false) const;
};

struct OxidationChange {
    const Element* element = nullptr;
    AverageOxidationState before;
    AverageOxidationState after;
    double atoms = 0.0;

    bool oxidized() const noexcept { return after.value() > before.value(); }
    bool reduced() const noexcept { return after.value() < before.value(); }
    double electrons() const noexcept { return (after.value() - before.value()) * atoms; }
};

class Reaction {
public:
    Reaction() = default;
    Reaction(const char* equation) : Reaction(parse(equation)) {}
    Reaction(const std::string& equation) : Reaction(parse(equation)) {}
    Reaction(std::vector<ReactionTerm> reactants, std::vector<ReactionTerm> products, bool reversible = false);

    static Reaction parse(std::string_view equation);

    const std::vector<ReactionTerm>& reactants() const noexcept { return m_reactants; }
    const std::vector<ReactionTerm>& products() const noexcept { return m_products; }
    std::vector<ReactionTerm>& reactants() noexcept { return m_reactants; }
    std::vector<ReactionTerm>& products() noexcept { return m_products; }
    bool reversible() const noexcept { return m_reversible; }
    void set_reversible(bool r) noexcept { m_reversible = r; }
    std::size_t species_count() const noexcept { return m_reactants.size() + m_products.size(); }

    bool is_balanced(double tolerance = 1e-9) const;
    std::vector<std::pair<const Element*, double>> element_imbalance() const;
    double charge_imbalance() const;
    Reaction balanced() const;
    void balance();
    bool balance_is_unique() const;

    Reaction reversed() const;
    Reaction scaled(double factor) const;
    Reaction operator+(const Reaction& o) const;
    Reaction operator-(const Reaction& o) const { return *this + o.reversed(); }
    Reaction operator*(double factor) const { return scaled(factor); }
    Reaction simplified() const;

    double delta_n_gas() const;
    double electrons_transferred() const;
    std::vector<OxidationChange> oxidation_changes() const;
    bool is_redox() const;
    ReactionType classify() const;

    bool has_thermo_data() const;
    std::vector<Species> species() const;
    double delta_h(double t = STANDARD_TEMPERATURE) const;
    double delta_s(double t = STANDARD_TEMPERATURE) const;
    double delta_g(double t = STANDARD_TEMPERATURE) const;
    double delta_cp(double t = STANDARD_TEMPERATURE) const;
    double equilibrium_constant(double t = STANDARD_TEMPERATURE) const;
    double kc(double t = STANDARD_TEMPERATURE) const;
    double spontaneity_temperature() const;
    double reaction_quotient(const std::vector<double>& activities) const;

    std::size_t limiting_reactant(const std::vector<double>& reactant_moles) const;
    double extent(const std::vector<double>& reactant_moles) const;
    std::vector<double> product_moles(const std::vector<double>& reactant_moles) const;
    std::vector<double> leftover_moles(const std::vector<double>& reactant_moles) const;
    double theoretical_yield(std::size_t product_index, const std::vector<double>& reactant_moles) const;
    double theoretical_yield_grams(std::size_t product_index, const std::vector<double>& reactant_grams) const;
    std::vector<double> grams_to_moles(const std::vector<double>& reactant_grams) const;
    double mass_balance() const;

    std::string to_string() const;
    std::string to_unicode_string() const;

private:
    std::vector<ReactionTerm> m_reactants;
    std::vector<ReactionTerm> m_products;
    bool m_reversible = false;
};

struct ReactionHeat {
    bool ok = false;
    double delta_h = NO_VALUE;
    std::vector<std::string> missing;
    std::vector<std::string> notes;
};

ReactionHeat reaction_heat(const Reaction& reaction, double t = STANDARD_TEMPERATURE);
std::string heat_description(double joules);

double percent_yield(double actual, double theoretical) noexcept;
double atom_economy(const Reaction& reaction, std::size_t desired_product) ;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_REACTION_HPP
