#ifndef RADIATION_CONVERSIONS_HPP
#define RADIATION_CONVERSIONS_HPP

namespace fizmo {
namespace converters {
namespace radiation {

constexpr long double roentgen_to_si(const long double roentgen) noexcept { return roentgen * 0.00869; }
constexpr long double rad_to_si(const long double rad) noexcept { return rad * 0.01; }
constexpr long double sievert_to_si(const long double sievert) noexcept { return sievert; }
constexpr long double rem_to_si(const long double rem) noexcept { return rem * 0.01; }

constexpr long double si_to_roentgen(const long double gray) noexcept { return gray / 0.00869; }
constexpr long double si_to_rad(const long double gray) noexcept { return gray * 100; }
constexpr long double si_to_sievert(const long double gray) noexcept { return gray; }
constexpr long double si_to_rem(const long double gray) noexcept { return gray * 100; }

constexpr long double curie_to_si(const long double curie) noexcept { return curie * 3.7e10; }
constexpr long double si_to_curie(const long double becquerel) noexcept { return becquerel * 2.7027e-11; }

} // namespace radiation
} // namespace converters
} // namespace fizmo

#endif // RADIATION_CONVERSIONS_HPP