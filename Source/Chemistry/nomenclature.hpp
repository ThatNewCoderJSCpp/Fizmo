#ifndef FIZMO_CHEMISTRY_NOMENCLATURE_HPP
#define FIZMO_CHEMISTRY_NOMENCLATURE_HPP

#include "Data/ion_data.hpp"
#include "formula.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

constexpr Span<PolyatomicIonEntry> polyatomic_ions() noexcept {
    return Span<PolyatomicIonEntry>(data::POLYATOMIC_IONS, sizeof(data::POLYATOMIC_IONS) / sizeof(data::POLYATOMIC_IONS[0]));
}

constexpr Span<CommonCompoundEntry> common_compounds() noexcept {
    return Span<CommonCompoundEntry>(data::COMMON_COMPOUNDS, sizeof(data::COMMON_COMPOUNDS) / sizeof(data::COMMON_COMPOUNDS[0]));
}

const PolyatomicIonEntry* find_polyatomic_ion(std::string_view name_or_formula, int charge = 0);
std::vector<const PolyatomicIonEntry*> find_polyatomic_ions(const Formula& composition);
std::vector<const PolyatomicIonEntry*> polyatomic_ions_in(std::string_view category);
std::vector<std::string> polyatomic_ion_categories();
std::vector<const PolyatomicIonEntry*> search_polyatomic_ions(std::string_view text);
Formula ion_formula(const PolyatomicIonEntry& ion);
std::string ion_formula_text(const PolyatomicIonEntry& ion, bool unicode = false);
std::vector<const CommonCompoundEntry*> find_common_compounds(std::string_view name_or_formula);

enum class CompoundKind : std::uint8_t {
    Element,
    MonatomicIon,
    PolyatomicIon,
    Ionic,
    CovalentBinary,
    Covalent,
    Acid,
    Base,
    Organic,
    Carbohydrate,
    Unknown
};

constexpr const char* to_string(CompoundKind k) noexcept {
    switch (k) {
        case CompoundKind::Element: return "element";
        case CompoundKind::MonatomicIon: return "monatomic ion";
        case CompoundKind::PolyatomicIon: return "polyatomic ion";
        case CompoundKind::Ionic: return "ionic compound";
        case CompoundKind::CovalentBinary: return "binary molecular compound";
        case CompoundKind::Covalent: return "molecular compound";
        case CompoundKind::Acid: return "acid";
        case CompoundKind::Base: return "base";
        case CompoundKind::Organic: return "organic compound";
        case CompoundKind::Carbohydrate: return "carbohydrate";
        default: return "unknown";
    }
}

struct IonDescriptor {
    std::string formula;
    int charge = 0;
    std::string name;
    std::string classical_name;
    bool polyatomic = false;
    const Element* element = nullptr;
    const PolyatomicIonEntry* entry = nullptr;

    bool valid() const noexcept { return charge != 0 && !formula.empty(); }
    bool is_cation() const noexcept { return charge > 0; }
    bool is_anion() const noexcept { return charge < 0; }
    Formula composition() const;
    std::string text(bool unicode = false) const;
};

struct IonicCombination {
    IonDescriptor cation;
    IonDescriptor anion;
    int cation_count = 0;
    int anion_count = 0;
    std::string formula;
    std::string name;

    Formula composition() const { return Formula::parse(formula); }
};

struct CompoundName {
    bool ok = false;
    std::string name;
    std::vector<std::string> other_names;
    CompoundKind kind = CompoundKind::Unknown;
    std::string formula;
    std::string explanation;
    std::vector<IonDescriptor> ions;
    int hydrate_waters = 0;
};

IonDescriptor monatomic_ion(const Element& element, int charge);
IonDescriptor polyatomic_ion(const PolyatomicIonEntry& entry);
std::optional<IonDescriptor> parse_ion(std::string_view text);
std::vector<IonDescriptor> common_cations();
std::vector<IonDescriptor> common_anions();
IonicCombination combine_ions(const IonDescriptor& cation, const IonDescriptor& anion);

std::string monatomic_anion_name(const Element& element);
std::string monatomic_cation_name(const Element& element, int charge, bool force_numeral = false);
std::string classical_cation_name(const Element& element, int charge);
bool has_single_common_charge(const Element& element) noexcept;
std::string element_root(const Element& element);
std::string acid_name_for_anion(const IonDescriptor& anion, bool aqueous = true);
const char* numeric_prefix(int n) noexcept;
std::string prefixed(int n, const std::string& word, bool drop_mono);
std::string hydrate_suffix(int waters);

CompoundName name_compound(std::string_view formula_text, Phase phase = Phase::Unknown);
CompoundName name_of(const Formula& formula);
CompoundName formula_from_name(std::string_view name);
std::string organic_name(const Formula& formula, std::string_view source = std::string_view());
std::string alkane_stem(int carbons);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_NOMENCLATURE_HPP
