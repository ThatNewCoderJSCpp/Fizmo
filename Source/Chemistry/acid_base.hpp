#ifndef FIZMO_CHEMISTRY_ACID_BASE_HPP
#define FIZMO_CHEMISTRY_ACID_BASE_HPP

#include "common.hpp"
#include "constants.hpp"
#include "reference_types.hpp"
#include "Data/reference_data.hpp"

#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

constexpr Span<AcidConstantEntry> acid_constants() noexcept {
    return Span<AcidConstantEntry>(data::ACID_CONSTANTS, sizeof(data::ACID_CONSTANTS) / sizeof(data::ACID_CONSTANTS[0]));
}

constexpr Span<BaseConstantEntry> base_constants() noexcept {
    return Span<BaseConstantEntry>(data::BASE_CONSTANTS, sizeof(data::BASE_CONSTANTS) / sizeof(data::BASE_CONSTANTS[0]));
}

constexpr const AcidConstantEntry* find_acid(std::string_view name_or_formula) noexcept {
    for (const AcidConstantEntry& a : acid_constants()) {
        if (equals_ignore_case(trim(name_or_formula), a.name) || trim(name_or_formula) == a.formula) return &a;
    }
    return nullptr;
}

constexpr const BaseConstantEntry* find_base(std::string_view name_or_formula) noexcept {
    for (const BaseConstantEntry& b : base_constants()) {
        if (equals_ignore_case(trim(name_or_formula), b.name) || trim(name_or_formula) == b.formula) return &b;
    }
    return nullptr;
}

struct AcidBaseComponent {
    double concentration = 0.0;
    std::vector<double> pka;
    int charge_of_most_protonated = 0;
};

double ph(double hydronium) noexcept;
double hydronium(double ph_value) noexcept;
double poh(double hydroxide) noexcept;
double ph_from_poh(double poh_value, double pkw = WATER_PKW) noexcept;
double ka_from_pka(double pka) noexcept;
double pka_from_ka(double ka) noexcept;
double conjugate_pk(double pk, double pkw = WATER_PKW) noexcept;
double water_ion_product(double t) noexcept;
double strong_acid_ph(double concentration, double kw = WATER_ION_PRODUCT) noexcept;
double strong_base_ph(double concentration, double kw = WATER_ION_PRODUCT) noexcept;
double weak_acid_ph(double concentration, double ka, double kw = WATER_ION_PRODUCT) noexcept;
double weak_base_ph(double concentration, double kb, double kw = WATER_ION_PRODUCT) noexcept;
double henderson_hasselbalch(double pka, double base_concentration, double acid_concentration) noexcept;
double buffer_ratio(double target_ph, double pka) noexcept;
double buffer_capacity(double total_concentration, double ka, double hydronium_value, double kw = WATER_ION_PRODUCT) noexcept;
double percent_ionization(double concentration, double ka) noexcept;
std::vector<double> species_fractions(double ph_value, const std::vector<double>& pka);
double average_protons_bound(double ph_value, const std::vector<double>& pka);
double isoelectric_point(double pka_acid, double pka_base) noexcept;
double equilibrium_ph(const std::vector<AcidBaseComponent>& components, double strong_acid = 0.0, double strong_base = 0.0, double kw = WATER_ION_PRODUCT);
double titration_ph(double acid_moles, const std::vector<double>& pka, double titrant_moles, double total_volume, bool titrant_is_base = true, double kw = WATER_ION_PRODUCT);
std::vector<std::pair<double, double>> titration_curve(double acid_concentration, double acid_volume, const std::vector<double>& pka, double titrant_concentration, double max_volume, int points, bool titrant_is_base = true);
double equivalence_volume(double analyte_moles, double titrant_concentration, int protons = 1) noexcept;

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ACID_BASE_HPP
