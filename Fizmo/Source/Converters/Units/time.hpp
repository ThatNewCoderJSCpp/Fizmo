#ifndef FIZMO_UNITS_TIME_HPP
#define FIZMO_UNITS_TIME_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace time {

constexpr TimeUnit second(1.0L);
FIZMO_SI_PREFIXED(second, second);  

constexpr TimeUnit minute     (60.0L);
constexpr TimeUnit hour       (3600.0L);
constexpr TimeUnit day        (86400.0L);
constexpr TimeUnit week       (604800.0L);
constexpr TimeUnit fortnight  (1209600.0L);
constexpr TimeUnit year       (31557600.0L);
constexpr TimeUnit decade     (31557600.0L * 10.0L);
constexpr TimeUnit century    (31557600.0L * 100.0L);
constexpr TimeUnit millennium (31557600.0L * 1000.0L);
constexpr TimeUnit planck_time(constants::PLANCK_TIME<long double>);

} // namespace time
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_TIME_HPP