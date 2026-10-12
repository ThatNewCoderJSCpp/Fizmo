#ifndef FIZMO_UNITS_DISTANCE_HPP
#define FIZMO_UNITS_DISTANCE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace distance {

constexpr LengthUnit meter(exact("1.0"));
FIZMO_SI_PREFIXED(meter, meter);  

constexpr LengthUnit light_time(long double seconds) noexcept {
    return LengthUnit(exact_constants::speed_of_light * ExactScale::from_long_double(seconds));
}

constexpr LengthUnit angstrom (exact("1e-10"));
constexpr LengthUnit micron   (exact("1e-6"));
constexpr LengthUnit bohr     (exact_constants::bohr_radius);
constexpr LengthUnit planck   (exact_constants::planck_length);

constexpr LengthUnit thou      (exact("2.54e-5"));
constexpr LengthUnit barleycorn(exact("0.0254") / ratio(3));
constexpr LengthUnit inch      (exact("0.0254"));
constexpr LengthUnit hand      (exact("0.1016"));
constexpr LengthUnit foot      (exact("0.3048"));
constexpr LengthUnit yard      (exact("0.9144"));
constexpr LengthUnit fathom    (exact("1.8288"));
constexpr LengthUnit rod       (exact("5.0292"));
constexpr LengthUnit chain     (exact("20.1168"));
constexpr LengthUnit furlong   (exact("201.168"));
constexpr LengthUnit mile      (exact("1609.344"));
constexpr LengthUnit league    (exact("4828.032"));
constexpr LengthUnit link      (exact("0.201168"));
constexpr LengthUnit ell       (exact("1.143"));

constexpr LengthUnit nautical_mile      (exact("1852.0"));
constexpr LengthUnit international_cable(exact("185.2"));
constexpr LengthUnit us_cable           (exact("219.456"));
constexpr LengthUnit shackle            (exact("27.432"));

constexpr LengthUnit point(exact("0.0254") / ratio(72));
constexpr LengthUnit pica (exact("0.0254") / ratio(6));
constexpr LengthUnit ligne(exact("0.002256"));

constexpr LengthUnit smoot(exact("1.7018"));

constexpr LengthUnit au(exact_constants::astronomical_unit);

constexpr LengthUnit light_second(exact_constants::speed_of_light);
constexpr LengthUnit light_minute(exact_constants::speed_of_light * ratio(60));
constexpr LengthUnit light_hour  (exact_constants::speed_of_light * ratio(3600));
constexpr LengthUnit light_day   (exact_constants::speed_of_light * ratio(86400));
constexpr LengthUnit light_week  (exact_constants::speed_of_light * ratio(604800));
constexpr LengthUnit light_year  (exact_constants::speed_of_light * exact_constants::julian_year); // 365.25 days

constexpr LengthUnit parsec(exact_constants::parsec);
FIZMO_SI_PREFIXED(parsec, parsec); 

} // namespace distance
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_DISTANCE_HPP