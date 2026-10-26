#ifndef FIZMO_CHEMISTRY_SOLUBILITY_HPP
#define FIZMO_CHEMISTRY_SOLUBILITY_HPP

#include "nomenclature.hpp"

#include <string>
#include <string_view>

namespace fizmo {
namespace chemistry {

enum class Solubility : std::uint8_t {
    Soluble,
    SlightlySoluble,
    Insoluble,
    ReactsWithWater,
    Unknown
};

constexpr const char* to_string(Solubility s) noexcept {
    switch (s) {
        case Solubility::Soluble: return "soluble";
        case Solubility::SlightlySoluble: return "slightly soluble";
        case Solubility::Insoluble: return "insoluble";
        case Solubility::ReactsWithWater: return "reacts with water";
        default: return "unknown";
    }
}

struct SolubilityResult {
    Solubility solubility = Solubility::Unknown;
    std::string rule;
};

SolubilityResult solubility(const IonDescriptor& cation, const IonDescriptor& anion);
SolubilityResult solubility_of(std::string_view formula);
bool is_soluble(std::string_view formula);
bool is_strong_acid(std::string_view formula);
bool is_strong_base(std::string_view formula);
bool is_weak_acid(std::string_view formula);
bool is_weak_base(std::string_view formula);
bool is_strong_electrolyte(std::string_view formula);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SOLUBILITY_HPP
