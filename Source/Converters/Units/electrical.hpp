#ifndef FIZMO_UNITS_ELECTRICAL_HPP
#define FIZMO_UNITS_ELECTRICAL_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {

namespace current {
    constexpr CurrentUnit ampere(1.0L);
    FIZMO_SI_PREFIXED(ampere, ampere);
    constexpr CurrentUnit abampere  (10.0L);
    constexpr CurrentUnit biot      (10.0L);
    constexpr CurrentUnit statampere(constants::LIGHT_SPEED<long double> * 1e-10L); 
} // namespace current

namespace charge {
    constexpr ChargeUnit coulomb(1.0L);
    FIZMO_SI_PREFIXED(coulomb, coulomb);

    constexpr ChargeUnit ampere_second(1.0L);
    FIZMO_SI_PREFIXED(ampere_second, ampere_second);

    constexpr ChargeUnit ampere_minute(60.0L);
    FIZMO_SI_PREFIXED(ampere_minute, ampere_minute);

    constexpr ChargeUnit ampere_hour(3600.0L);
    FIZMO_SI_PREFIXED(ampere_hour, ampere_hour);
    
    constexpr ChargeUnit faraday    (constants::FARADAY_CONSTANT<long double>);
    constexpr ChargeUnit elementary (constants::ELEMENTARY_CHARGE<long double>);
    constexpr ChargeUnit abcoulomb  (10.0L);
    constexpr ChargeUnit statcoulomb(constants::LIGHT_SPEED<long double> * 1e-10L);
} // namespace charge

namespace voltage {
    constexpr VoltageUnit volt(1.0L);
    FIZMO_SI_PREFIXED(volt, volt);
    constexpr VoltageUnit abvolt (1e-8L);
    constexpr VoltageUnit statvolt(constants::LIGHT_SPEED<long double> * 1e-6L); 
} // namespace voltage

namespace resistance {
    constexpr ResistanceUnit ohm(1.0L);
    FIZMO_SI_PREFIXED(ohm, ohm);
    constexpr ResistanceUnit abohm (1e-9L);
    constexpr ResistanceUnit statohm(8.987551787e11L);
} // namespace resistance

namespace capacitance {
    constexpr CapacitanceUnit farad = charge::coulomb / voltage::volt;            
    FIZMO_SI_PREFIXED(farad, farad);                  
} // namespace capacitance

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ELECTRICAL_HPP