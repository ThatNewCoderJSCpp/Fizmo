#ifndef FIZMO_UNITS_FLOW_HPP
#define FIZMO_UNITS_FLOW_HPP

#include "amount.hpp"
#include "mass.hpp"
#include "volume.hpp"
#include "time.hpp"
#include "area.hpp"

namespace fizmo {
namespace units {

namespace mass_flow {
    constexpr MassFlowRateUnit gram_per_second = mass::gram / time::second;
    FIZMO_SI_PREFIXED(gram_per_second, gram_per_second);

    constexpr MassFlowRateUnit gram_per_minute = mass::gram / time::minute;
    FIZMO_SI_PREFIXED(gram_per_minute, gram_per_minute);
    
    constexpr MassFlowRateUnit gram_per_hour = mass::gram / time::hour;
    FIZMO_SI_PREFIXED(gram_per_hour, gram_per_hour);

    constexpr MassFlowRateUnit pound_per_second = mass::pound / time::second;
    constexpr MassFlowRateUnit pound_per_minute = mass::pound / time::minute;
    constexpr MassFlowRateUnit pound_per_hour   = mass::pound / time::hour;
} // namespace mass_flow

namespace volume_flow {
    constexpr VolumeFlowRateUnit cubic_meter_per_second = volume::cubic_meter / time::second;
    constexpr VolumeFlowRateUnit cubic_meter_per_minute = volume::cubic_meter / time::minute;
    constexpr VolumeFlowRateUnit cubic_meter_per_hour   = volume::cubic_meter / time::hour;

    constexpr VolumeFlowRateUnit liter_per_second = volume::liter / time::second;
    FIZMO_SI_PREFIXED(liter_per_second, liter_per_second);

    constexpr VolumeFlowRateUnit liter_per_minute = volume::liter / time::minute;
    FIZMO_SI_PREFIXED(liter_per_minute, liter_per_minute);

    constexpr VolumeFlowRateUnit liter_per_hour = volume::liter / time::hour;
    FIZMO_SI_PREFIXED(liter_per_hour, liter_per_hour);

    constexpr VolumeFlowRateUnit us_gallon_per_second   = volume::us_gallon   / time::second;
    constexpr VolumeFlowRateUnit us_gallon_per_minute   = volume::us_gallon   / time::minute;
    constexpr VolumeFlowRateUnit us_gallon_per_hour     = volume::us_gallon   / time::hour;
    constexpr VolumeFlowRateUnit cubic_foot_per_second  = volume::cubic_foot  / time::second;
    constexpr VolumeFlowRateUnit cubic_foot_per_minute  = volume::cubic_foot  / time::minute;
    constexpr VolumeFlowRateUnit cubic_foot_per_hour    = volume::cubic_foot  / time::hour;
    constexpr VolumeFlowRateUnit cubic_inch_per_second  = volume::cubic_inch  / time::second;
    constexpr VolumeFlowRateUnit cubic_inch_per_minute  = volume::cubic_inch  / time::minute;
    constexpr VolumeFlowRateUnit cubic_inch_per_hour    = volume::cubic_inch  / time::hour;
} // namespace volume_flow

namespace area_flow {
    constexpr AreaFlowRateUnit square_meter_per_second = area::square_meter / time::second;
    constexpr AreaFlowRateUnit square_meter_per_minute = area::square_meter / time::minute;
    constexpr AreaFlowRateUnit square_meter_per_hour   = area::square_meter / time::hour;
    
    constexpr AreaFlowRateUnit square_foot_per_second  = area::square_foot  / time::second;
    constexpr AreaFlowRateUnit square_foot_per_minute  = area::square_foot  / time::minute;
    constexpr AreaFlowRateUnit square_foot_per_hour    = area::square_foot  / time::hour;
    constexpr AreaFlowRateUnit square_inch_per_second  = area::square_inch  / time::second;
    constexpr AreaFlowRateUnit square_inch_per_minute  = area::square_inch  / time::minute;
    constexpr AreaFlowRateUnit square_inch_per_hour    = area::square_inch  / time::hour;
} // namespace area_flow

namespace molar_flow {
    constexpr MolarFlowRateUnit mole_per_second = amount::mole / time::second;
    FIZMO_SI_PREFIXED(mole_per_second, mole_per_second);

    constexpr MolarFlowRateUnit mole_per_minute = amount::mole / time::minute;
    FIZMO_SI_PREFIXED(mole_per_minute, mole_per_minute);

    constexpr MolarFlowRateUnit mole_per_hour = amount::mole / time::hour;
    FIZMO_SI_PREFIXED(mole_per_hour, mole_per_hour);
} // namespace molar_flow

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_FLOW_HPP