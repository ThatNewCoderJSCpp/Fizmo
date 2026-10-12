#ifndef FIZMO_UNITS_AREA_HPP
#define FIZMO_UNITS_AREA_HPP

#include "distance.hpp"

namespace fizmo {
namespace units {
namespace area {

FIZMO_SI_PREFIXED_POWERED(square, meter, distance::meter, 2);

constexpr AreaUnit square_foot = distance::foot.pow<2>();
constexpr AreaUnit square_inch = distance::inch.pow<2>();
constexpr AreaUnit square_yard = distance::yard.pow<2>();
constexpr AreaUnit square_mile = distance::mile.pow<2>();

constexpr AreaUnit are    (exact("100.0"));
constexpr AreaUnit hectare(exact("1e4"));
constexpr AreaUnit barn   (exact("1e-28"));
constexpr AreaUnit acre  = distance::chain * distance::furlong;     

} // namespace area
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_AREA_HPP