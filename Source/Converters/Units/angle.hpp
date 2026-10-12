#ifndef FIZMO_UNITS_ANGLE_HPP
#define FIZMO_UNITS_ANGLE_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace angle {

constexpr AngleUnit radian   (exact("1.0"));
constexpr AngleUnit degree   (ExactScale::pi() / ratio(180));
constexpr AngleUnit gradian  (ExactScale::pi() / ratio(200));
constexpr AngleUnit arcminute(ExactScale::pi() / ratio(10800));
constexpr AngleUnit arcsecond(ExactScale::pi() / ratio(648000));
constexpr AngleUnit turn     (ExactScale::pi() * ratio(2));
constexpr AngleUnit revolution = turn;

} // namespace angle
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ANGLE_HPP