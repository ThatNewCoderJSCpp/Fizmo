#ifndef FIZMO_UNITS_MASS_HPP
#define FIZMO_UNITS_MASS_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace mass {

constexpr MassUnit gram(1e-3L);
FIZMO_SI_PREFIXED(gram, gram);    

constexpr MassUnit tonne     (1000.0L); 
constexpr MassUnit metric_ton(1000.0L);
constexpr MassUnit quintal   (100.0L);

constexpr MassUnit grain           (6.479891e-5L);
constexpr MassUnit dram            (1.7718451953125e-3L);
constexpr MassUnit ounce           (0.028349523125L);
constexpr MassUnit pound           (0.45359237L);
constexpr MassUnit stone           (6.35029318L);
constexpr MassUnit quarter_us      (11.33980925L);
constexpr MassUnit quarter_uk      (12.70058636L);
constexpr MassUnit hundredweight_us(45.359237L);
constexpr MassUnit hundredweight_uk(50.80234544L);
constexpr MassUnit ton_us          (907.18474L);    // short ton
constexpr MassUnit ton_uk          (1016.0469088L); // long ton

constexpr MassUnit pennyweight (1.55517384e-3L);
constexpr MassUnit troy_ounce  (0.0311034768L);
constexpr MassUnit troy_pound  (0.3732417216L);
constexpr MassUnit scruple     (1.2959782e-3L);
constexpr MassUnit drachm      (3.8879346e-3L);

constexpr MassUnit dalton     (constants::ATOMIC_MASS_UNIT<long double>);
constexpr MassUnit planck_mass(constants::PLANCK_MASS<long double>);
constexpr MassUnit slug       (14.593902937L);
constexpr MassUnit carat      (2e-4L);
constexpr MassUnit pearl_grain(5e-5L);

} // namespace mass
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_MASS_HPP