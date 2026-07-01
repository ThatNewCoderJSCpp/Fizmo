#ifndef FIZMO_UNITS_AMOUNT_HPP
#define FIZMO_UNITS_AMOUNT_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace amount {

constexpr AmountUnit mole(1.0L);
FIZMO_SI_PREFIXED(mole, mole);   

} // namespace amount
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_AMOUNT_HPP