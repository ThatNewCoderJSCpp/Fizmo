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

constexpr AreaUnit are    (100.0L);
constexpr AreaUnit hectare(1e4L);
constexpr AreaUnit barn   (1e-28L);
constexpr AreaUnit acre  = distance::chain * distance::furlong;     

} // namespace area
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_AREA_HPP