#ifndef FIZMO_CHEMISTRY_SUBSTANCE_HPP
#define FIZMO_CHEMISTRY_SUBSTANCE_HPP

#include "substance_types.hpp"
#include "Data/substance_data.hpp"
#include "formula.hpp"

#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

inline constexpr int SUBSTANCE_COUNT = static_cast<int>(sizeof(data::SUBSTANCES) / sizeof(data::SUBSTANCES[0]));

constexpr Span<HeatCapacityRange> Substance::heat_capacity_ranges(Phase p) const noexcept {
    if (p == Phase::Gas) return Span<HeatCapacityRange>(data::HEAT_CAPACITY_RANGES + cp_gas_offset, cp_gas_count);
    if (p == Phase::Liquid) return Span<HeatCapacityRange>(data::HEAT_CAPACITY_RANGES + cp_liquid_offset, cp_liquid_count);
    if (p == Phase::Solid) return Span<HeatCapacityRange>(data::HEAT_CAPACITY_RANGES + cp_solid_offset, cp_solid_count);
    return Span<HeatCapacityRange>();
}

constexpr Span<Substance> substances() noexcept {
    return Span<Substance>(data::SUBSTANCES, SUBSTANCE_COUNT);
}

constexpr const Substance* find_substance(std::string_view name_formula_or_cas) noexcept {
    const std::string_view key = trim(name_formula_or_cas);
    for (const Substance& s : substances()) {
        if (equals_ignore_case(key, s.name) || key == s.cas) return &s;
    }
    for (const Substance& s : substances()) {
        if (key == s.formula) return &s;
    }
    if (equals_ignore_case(key, "aluminum")) return find_substance("aluminium");
    if (equals_ignore_case(key, "aluminum oxide")) return find_substance("aluminium oxide");
    if (equals_ignore_case(key, "isopropyl alcohol") || equals_ignore_case(key, "2-propanol")) return find_substance("isopropanol");
    if (equals_ignore_case(key, "ethyl alcohol")) return find_substance("ethanol");
    if (equals_ignore_case(key, "methyl alcohol")) return find_substance("methanol");
    if (equals_ignore_case(key, "carbon")) return find_substance("graphite");
    if (equals_ignore_case(key, "butane")) return find_substance("n-butane");
    if (equals_ignore_case(key, "pentane")) return find_substance("n-pentane");
    if (equals_ignore_case(key, "hexane")) return find_substance("n-hexane");
    if (equals_ignore_case(key, "heptane")) return find_substance("n-heptane");
    if (equals_ignore_case(key, "octane")) return find_substance("n-octane");
    if (equals_ignore_case(key, "R-134a") || equals_ignore_case(key, "R134a")) return find_substance("1,1,1,2-tetrafluoroethane");
    if (equals_ignore_case(key, "R-12")) return find_substance("dichlorodifluoromethane");
    if (equals_ignore_case(key, "R-22")) return find_substance("chlorodifluoromethane");
    if (equals_ignore_case(key, "quartz")) return find_substance("silicon dioxide");
    if (equals_ignore_case(key, "ethene")) return find_substance("ethylene");
    if (equals_ignore_case(key, "ethyne")) return find_substance("acetylene");
    if (equals_ignore_case(key, "propene")) return find_substance("propylene");
    if (equals_ignore_case(key, "DMSO")) return find_substance("dimethyl sulfoxide");
    if (equals_ignore_case(key, "THF")) return find_substance("tetrahydrofuran");
    return nullptr;
}

constexpr const Substance& substance(std::string_view name_formula_or_cas) {
    const Substance* s = find_substance(name_formula_or_cas);
    return s ? *s : throw std::invalid_argument("fizmo::chemistry: unknown substance");
}

std::vector<const Substance*> find_substances(const Formula& formula);
Formula formula_of(const Substance& s);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_SUBSTANCE_HPP
