#ifndef MASS_CONVERSIONS_HPP
#define MASS_CONVERSIONS_HPP

#include "../Basic/constants.hpp"

namespace fizmo {
namespace converters {
namespace mass {

// Metric prefixes (base: kilogram in SI, but gram conversions provided)
constexpr long double quectogram_to_si(const long double qg) noexcept { return qg * 1e-33; }
constexpr long double si_to_quectogram(const long double kg) noexcept { return kg * 1e33; }

constexpr long double rontogram_to_si(const long double rg) noexcept { return rg * 1e-30; }
constexpr long double si_to_rontogram(const long double kg) noexcept { return kg * 1e30; }

constexpr long double yoctogram_to_si(const long double yg) noexcept { return yg * 1e-27; }
constexpr long double si_to_yoctogram(const long double kg) noexcept { return kg * 1e27; }

constexpr long double zeptogram_to_si(const long double zg) noexcept { return zg * 1e-24; }
constexpr long double si_to_zeptogram(const long double kg) noexcept { return kg * 1e24; }

constexpr long double attogram_to_si(const long double ag) noexcept { return ag * 1e-21; }
constexpr long double si_to_attogram(const long double kg) noexcept { return kg * 1e21; }

constexpr long double femtogram_to_si(const long double fg) noexcept { return fg * 1e-18; }
constexpr long double si_to_femtogram(const long double kg) noexcept { return kg * 1e18; }

constexpr long double picogram_to_si(const long double pg) noexcept { return pg * 1e-15; }
constexpr long double si_to_picogram(const long double kg) noexcept { return kg * 1e15; }

constexpr long double nanogram_to_si(const long double ng) noexcept { return ng * 1e-12; }
constexpr long double si_to_nanogram(const long double kg) noexcept { return kg * 1e12; }

constexpr long double microgram_to_si(const long double ug) noexcept { return ug * 1e-9; }
constexpr long double si_to_microgram(const long double kg) noexcept { return kg * 1e9; }

constexpr long double milligram_to_si(const long double mg) noexcept { return mg * 1e-6; }
constexpr long double si_to_milligram(const long double kg) noexcept { return kg * 1e6; }

constexpr long double centigram_to_si(const long double cg) noexcept { return cg * 1e-5; }
constexpr long double si_to_centigram(const long double kg) noexcept { return kg * 1e5; }

constexpr long double decigram_to_si(const long double dg) noexcept { return dg * 1e-4; }
constexpr long double si_to_decigram(const long double kg) noexcept { return kg * 1e4; }

constexpr long double gram_to_si(const long double g) noexcept { return g * 1e-3; }
constexpr long double si_to_gram(const long double kg) noexcept { return kg * 1e3; }

constexpr long double decagram_to_si(const long double dag) noexcept { return dag * 1e-2; }
constexpr long double si_to_decagram(const long double kg) noexcept { return kg * 1e2; }

constexpr long double hectogram_to_si(const long double hg) noexcept { return hg * 1e-1; }
constexpr long double si_to_hectogram(const long double kg) noexcept { return kg * 1e1; }

constexpr long double megagram_to_si(const long double Mg) noexcept { return Mg * 1e3; }
constexpr long double si_to_megagram(const long double kg) noexcept { return kg * 1e-3; }

constexpr long double gigagram_to_si(const long double Gg) noexcept { return Gg * 1e6; }
constexpr long double si_to_gigagram(const long double kg) noexcept { return kg * 1e-6; }

constexpr long double teragram_to_si(const long double Tg) noexcept { return Tg * 1e9; }
constexpr long double si_to_teragram(const long double kg) noexcept { return kg * 1e-9; }

constexpr long double petagram_to_si(const long double Pg) noexcept { return Pg * 1e12; }
constexpr long double si_to_petagram(const long double kg) noexcept { return kg * 1e-12; }

constexpr long double exagram_to_si(const long double Eg) noexcept { return Eg * 1e15; }
constexpr long double si_to_exagram(const long double kg) noexcept { return kg * 1e-15; }

constexpr long double zettagram_to_si(const long double Zg) noexcept { return Zg * 1e18; }
constexpr long double si_to_zettagram(const long double kg) noexcept { return kg * 1e-18; }

constexpr long double yottagram_to_si(const long double Yg) noexcept { return Yg * 1e21; }
constexpr long double si_to_yottagram(const long double kg) noexcept { return kg * 1e-21; }

constexpr long double ronnagram_to_si(const long double Rg) noexcept { return Rg * 1e24; }
constexpr long double si_to_ronnagram(const long double kg) noexcept { return kg * 1e-24; }

constexpr long double quettagram_to_si(const long double Qg) noexcept { return Qg * 1e27; }
constexpr long double si_to_quettagram(const long double kg) noexcept { return kg * 1e-27; }

// Metric ton / tonne (1000 kg)
constexpr long double metric_ton_to_si(const long double t) noexcept { return t * 1000.0; }
constexpr long double si_to_metric_ton(const long double kg) noexcept { return kg / 1000.0; }

constexpr long double tonne_to_si(const long double t) noexcept { return t * 1000.0; }
constexpr long double si_to_tonne(const long double kg) noexcept { return kg / 1000.0; }

// Quintal (100 kg)
constexpr long double quintal_to_si(const long double q) noexcept { return q * 100.0; }
constexpr long double si_to_quintal(const long double kg) noexcept { return kg / 100.0; }

// Imperial/Avoirdupois units
constexpr long double grain_to_si(const long double gr) noexcept { return gr * 6.479891e-5; }
constexpr long double si_to_grain(const long double kg) noexcept { return kg / 6.479891e-5; }

constexpr long double dram_to_si(const long double dr) noexcept { return dr * 1.7718451953125e-3; }
constexpr long double si_to_dram(const long double kg) noexcept { return kg / 1.7718451953125e-3; }

constexpr long double ounce_to_si(const long double oz) noexcept { return oz * 0.028349523125; }
constexpr long double si_to_ounce(const long double kg) noexcept { return kg / 0.028349523125; }

constexpr long double pound_to_si(const long double lb) noexcept { return lb * 0.45359237; }
constexpr long double si_to_pound(const long double kg) noexcept { return kg / 0.45359237; }

constexpr long double stone_to_si(const long double st) noexcept { return st * 6.35029318; }
constexpr long double si_to_stone(const long double kg) noexcept { return kg / 6.35029318; }

// Quarter
constexpr long double quarter_us_to_si(const long double qtr) noexcept { return qtr * 11.33980925; }
constexpr long double si_to_quarter_us(const long double kg) noexcept { return kg / 11.33980925; }

constexpr long double quarter_uk_to_si(const long double qtr) noexcept { return qtr * 12.70058636; }
constexpr long double si_to_quarter_uk(const long double kg) noexcept { return kg / 12.70058636; }

// Hundredweight
constexpr long double hundredweight_us_to_si(const long double cwt) noexcept { return cwt * 45.359237; }
constexpr long double si_to_hundredweight_us(const long double kg) noexcept { return kg / 45.359237; }

constexpr long double hundredweight_uk_to_si(const long double cwt) noexcept { return cwt * 50.80234544; }
constexpr long double si_to_hundredweight_uk(const long double kg) noexcept { return kg / 50.80234544; }

// Cental (same as US hundredweight)
constexpr long double cental_to_si(const long double cental) noexcept { return cental * 45.359237; }
constexpr long double si_to_cental(const long double kg) noexcept { return kg / 45.359237; }

// Ton
constexpr long double ton_us_to_si(const long double ton) noexcept { return ton * 907.18474; }
constexpr long double si_to_ton_us(const long double kg) noexcept { return kg / 907.18474; }

constexpr long double ton_uk_to_si(const long double ton) noexcept { return ton * 1016.0469088; }
constexpr long double si_to_ton_uk(const long double kg) noexcept { return kg / 1016.0469088; }

// Troy weight system
constexpr long double troy_grain_to_si(const long double gr) noexcept { return gr * 6.479891e-5; }
constexpr long double si_to_troy_grain(const long double kg) noexcept { return kg / 6.479891e-5; }

constexpr long double pennyweight_to_si(const long double dwt) noexcept { return dwt * 1.55517384e-3; }
constexpr long double si_to_pennyweight(const long double kg) noexcept { return kg / 1.55517384e-3; }

constexpr long double troy_ounce_to_si(const long double oz_t) noexcept { return oz_t * 0.0311034768; }
constexpr long double si_to_troy_ounce(const long double kg) noexcept { return kg / 0.0311034768; }

constexpr long double troy_pound_to_si(const long double lb_t) noexcept { return lb_t * 0.3732417216; }
constexpr long double si_to_troy_pound(const long double kg) noexcept { return kg / 0.3732417216; }

constexpr long double scruple_to_si(const long double s_ap) noexcept { return s_ap * 1.2959782e-3; }
constexpr long double si_to_scruple(const long double kg) noexcept { return kg / 1.2959782e-3; }

constexpr long double drachm_to_si(const long double dr_ap) noexcept { return dr_ap * 3.8879346e-3; }
constexpr long double si_to_drachm(const long double kg) noexcept { return kg / 3.8879346e-3; }

constexpr long double apothecaries_ounce_to_si(const long double oz_ap) noexcept { return oz_ap * 0.0311034768; }
constexpr long double si_to_apothecaries_ounce(const long double kg) noexcept { return kg / 0.0311034768; }

constexpr long double apothecaries_pound_to_si(const long double lb_ap) noexcept { return lb_ap * 0.3732417216; }
constexpr long double si_to_apothecaries_pound(const long double kg) noexcept { return kg / 0.3732417216; }

constexpr long double amu_chemistry_to_si(const long double amu) noexcept { return amu * 1.660468e-27L; }
constexpr long double si_to_amu_chemistry(const long double kg) noexcept { return kg / 1.660468e-27L; }

constexpr long double amu_physics_to_si(const long double amu) noexcept { return amu * 1.6600113e-27L; }
constexpr long double si_to_amu_physics(const long double kg) noexcept { return kg / 1.6600113e-27L; }

constexpr long double dalton_to_si(const long double da) noexcept { return da * 1.66053906660e-27; }
constexpr long double si_to_dalton(const long double kg) noexcept { return kg / 1.66053906660e-27; }

constexpr long double unified_mass_unit_to_si(const long double u) noexcept { return u * 1.66053906660e-27; }
constexpr long double si_to_unified_mass_unit(const long double kg) noexcept { return kg / 1.66053906660e-27; }

constexpr long double planck_mass_to_si(const long double mp) noexcept { return mp * fizmo::constants::planck_mass(); }
constexpr long double si_to_planck_mass(const long double kg) noexcept { return kg / fizmo::constants::planck_mass(); }

// Engineering units
constexpr long double slug_to_si(const long double slug) noexcept { return slug * 14.593902937; }
constexpr long double si_to_slug(const long double kg) noexcept { return kg / 14.593902937; }

// Precious metals and gemstones
constexpr long double carat_to_si(const long double ct) noexcept { return ct * 2e-4; }
constexpr long double si_to_carat(const long double kg) noexcept { return kg / 2e-4; }

constexpr long double point_to_si(const long double pt) noexcept { return pt * 2e-6; }
constexpr long double si_to_point(const long double kg) noexcept { return kg / 2e-6; }

constexpr long double pearl_grain_to_si(const long double gr) noexcept { return gr * 5e-5; }
constexpr long double si_to_pearl_grain(const long double kg) noexcept { return kg / 5e-5; }

// Force-related (kip = 1000 lbf, but as mass = 1000 lb)
constexpr long double kip_to_si(const long double kip) noexcept { return kip * 453.59237; }
constexpr long double si_to_kip(const long double kg) noexcept { return kg / 453.59237; }

// Gamma (geophysics, 1 microgram)
constexpr long double gamma_to_si(const long double gamma) noexcept { return gamma * 1e-9; }
constexpr long double si_to_gamma(const long double kg) noexcept { return kg * 1e9; }

} // namespace mass
} // namespace converters
} // namespace fizmo

#endif // MASS_CONVERSIONS_HPP