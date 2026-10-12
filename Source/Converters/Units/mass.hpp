#ifndef FIZMO_UNITS_MASS_HPP
#define FIZMO_UNITS_MASS_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace mass {

constexpr MassUnit gram(exact("1e-3"));
FIZMO_SI_PREFIXED(gram, gram);    

constexpr MassUnit tonne     (exact("1000.0")); 
constexpr MassUnit metric_ton(exact("1000.0"));
constexpr MassUnit quintal   (exact("100.0"));

constexpr MassUnit grain           (exact("6.479891e-5"));
constexpr MassUnit dram            (exact("1.7718451953125e-3"));
constexpr MassUnit ounce           (exact("0.028349523125"));
constexpr MassUnit pound           (exact("0.45359237"));
constexpr MassUnit stone           (exact("6.35029318"));
constexpr MassUnit quarter_us      (exact("11.33980925"));
constexpr MassUnit quarter_uk      (exact("12.70058636"));
constexpr MassUnit hundredweight_us(exact("45.359237"));
constexpr MassUnit hundredweight_uk(exact("50.80234544"));
constexpr MassUnit ton_us          (exact("907.18474"));    // short ton
constexpr MassUnit ton_uk          (exact("1016.0469088")); // long ton

constexpr MassUnit pennyweight (exact("1.55517384e-3"));
constexpr MassUnit troy_ounce  (exact("0.0311034768"));
constexpr MassUnit troy_pound  (exact("0.3732417216"));
constexpr MassUnit scruple     (exact("1.2959782e-3"));
constexpr MassUnit drachm      (exact("3.8879346e-3"));

constexpr MassUnit dalton     (exact_constants::dalton);
constexpr MassUnit planck_mass(exact_constants::planck_mass);
constexpr MassUnit slug       (exact_constants::pound * exact_constants::standard_gravity / exact_constants::foot);
constexpr MassUnit carat      (exact("2e-4"));
constexpr MassUnit pearl_grain(exact("5e-5"));

} // namespace mass
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_MASS_HPP