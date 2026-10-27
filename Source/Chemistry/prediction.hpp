#ifndef FIZMO_CHEMISTRY_PREDICTION_HPP
#define FIZMO_CHEMISTRY_PREDICTION_HPP

#include "reaction.hpp"
#include "solubility.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct PredictedReaction {
    bool ok = false;
    bool no_reaction = false;
    Reaction reaction;
    ReactionType type = ReactionType::Other;
    std::string kind;
    std::string explanation;
    std::vector<std::string> notes;
    std::string complete_ionic;
    std::string net_ionic;
    std::vector<std::string> spectators;

    std::string equation(bool unicode = true) const;
};

PredictedReaction predict_products(std::string_view reactants);
PredictedReaction predict_products(const std::vector<std::string>& reactants);
std::vector<std::string> split_reactants(std::string_view text);

int metal_activity_rank(const Element& e) noexcept;
int halogen_activity_rank(const Element& e) noexcept;
bool displaces(const Element& free_element, const Element& combined) noexcept;
std::vector<const Element*> metal_activity_series();

struct IonicEquation {
    std::string complete;
    std::string net;
    std::vector<std::string> spectators;
    bool no_net_reaction = false;
};

IonicEquation ionic_equations(const Reaction& reaction);

Phase likely_phase(const Formula& formula, double temperature, double pressure, bool in_solution);
bool reaction_in_solution(const Reaction& reaction);
std::vector<Phase> likely_phases(const Reaction& reaction, double temperature, double pressure);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_PREDICTION_HPP
