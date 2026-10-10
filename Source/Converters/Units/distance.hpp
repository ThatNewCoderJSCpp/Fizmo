#ifndef FIZMO_UNITS_DISTANCE_HPP
#define FIZMO_UNITS_DISTANCE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace distance {

constexpr LengthUnit meter(1.0L);
FIZMO_SI_PREFIXED(meter, meter);  

constexpr LengthUnit light_time(long double seconds) noexcept {
    return LengthUnit(constants::LIGHT_SPEED<long double> * seconds);
}

constexpr LengthUnit angstrom (1e-10L);
constexpr LengthUnit micron   (1e-6L);
constexpr LengthUnit bohr     (constants::BOHR_RADIUS<long double>);
constexpr LengthUnit planck   (constants::PLANCK_LENGTH<long double>);

constexpr LengthUnit thou      (2.54e-5L);
constexpr LengthUnit barleycorn(0.0254L / 3.0L);
constexpr LengthUnit inch      (0.0254L);
constexpr LengthUnit hand      (0.1016L);
constexpr LengthUnit foot      (0.3048L);
constexpr LengthUnit yard      (0.9144L);
constexpr LengthUnit fathom    (1.8288L);
constexpr LengthUnit rod       (5.0292L);
constexpr LengthUnit chain     (20.1168L);
constexpr LengthUnit furlong   (201.168L);
constexpr LengthUnit mile      (1609.344L);
constexpr LengthUnit league    (4828.032L);
constexpr LengthUnit link      (0.201168L);
constexpr LengthUnit ell       (1.143L);

constexpr LengthUnit nautical_mile      (1852.0L);
constexpr LengthUnit international_cable(185.2L);
constexpr LengthUnit us_cable           (219.456L);
constexpr LengthUnit shackle            (27.432L);

constexpr LengthUnit point(0.0254L / 72.0L);
constexpr LengthUnit pica (0.0254L / 6.0L);
constexpr LengthUnit ligne(0.002256L);

constexpr LengthUnit smoot(1.7018L);

constexpr LengthUnit au(constants::ASTRONOMICAL_UNIT<long double>);

constexpr LengthUnit light_second = light_time(1.0L);
constexpr LengthUnit light_minute = light_time(60.0L);
constexpr LengthUnit light_hour   = light_time(3600.0L);
constexpr LengthUnit light_day    = light_time(86400.0L);
constexpr LengthUnit light_week   = light_time(604800.0L);
constexpr LengthUnit light_year   = light_time(31557600.0L); // 365.25 days

constexpr LengthUnit parsec(constants::PARSEC<long double>);
FIZMO_SI_PREFIXED(parsec, parsec); 

} // namespace distance
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_DISTANCE_HPP