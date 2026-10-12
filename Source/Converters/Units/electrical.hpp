#ifndef FIZMO_UNITS_ELECTRICAL_HPP
#define FIZMO_UNITS_ELECTRICAL_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {

namespace current {
    constexpr CurrentUnit ampere(exact("1.0"));
    FIZMO_SI_PREFIXED(ampere, ampere);
    constexpr CurrentUnit abampere  (exact("10.0"));
    constexpr CurrentUnit biot      (exact("10.0"));
    constexpr CurrentUnit statampere(ratio(1, 10) / exact_constants::speed_of_light);
} // namespace current

namespace charge {
    constexpr ChargeUnit coulomb(exact("1.0"));
    FIZMO_SI_PREFIXED(coulomb, coulomb);

    constexpr ChargeUnit ampere_second(exact("1.0"));
    FIZMO_SI_PREFIXED(ampere_second, ampere_second);

    constexpr ChargeUnit ampere_minute(exact("60.0"));
    FIZMO_SI_PREFIXED(ampere_minute, ampere_minute);

    constexpr ChargeUnit ampere_hour(exact("3600.0"));
    FIZMO_SI_PREFIXED(ampere_hour, ampere_hour);
    
    constexpr ChargeUnit faraday    (exact_constants::faraday);
    constexpr ChargeUnit elementary (exact_constants::elementary_charge);
    constexpr ChargeUnit abcoulomb  (exact("10.0"));
    constexpr ChargeUnit statcoulomb(ratio(1, 10) / exact_constants::speed_of_light);
} // namespace charge

namespace voltage {
    constexpr VoltageUnit volt(exact("1.0"));
    FIZMO_SI_PREFIXED(volt, volt);
    constexpr VoltageUnit abvolt (exact("1e-8"));
    constexpr VoltageUnit statvolt(exact_constants::speed_of_light * exact("1e-6"));
} // namespace voltage

namespace resistance {
    constexpr ResistanceUnit ohm(exact("1.0"));
    FIZMO_SI_PREFIXED(ohm, ohm);
    constexpr ResistanceUnit abohm (exact("1e-9"));
    constexpr ResistanceUnit statohm(exact_constants::speed_of_light.power(2) * exact("1e-9"));
} // namespace resistance

namespace capacitance {
    constexpr CapacitanceUnit farad = charge::coulomb / voltage::volt;            
    FIZMO_SI_PREFIXED(farad, farad);                  
} // namespace capacitance

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ELECTRICAL_HPP