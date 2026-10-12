#ifndef FIZMO_UNITS_TIME_HPP
#define FIZMO_UNITS_TIME_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace time {

constexpr TimeUnit second(exact("1.0"));
FIZMO_SI_PREFIXED(second, second);  

constexpr TimeUnit minute     (exact("60.0"));
constexpr TimeUnit hour       (exact("3600.0"));
constexpr TimeUnit day        (exact("86400.0"));
constexpr TimeUnit week       (exact("604800.0"));
constexpr TimeUnit fortnight  (exact("1209600.0"));
constexpr TimeUnit year       (exact("31557600.0"));
constexpr TimeUnit decade     (exact("315576000"));
constexpr TimeUnit century    (exact("3155760000"));
constexpr TimeUnit millennium (exact("31557600000"));
constexpr TimeUnit planck_time(exact_constants::planck_time);

} // namespace time
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_TIME_HPP