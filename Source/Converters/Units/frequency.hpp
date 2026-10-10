#ifndef FIZMO_UNITS_FREQUENCY_HPP
#define FIZMO_UNITS_FREQUENCY_HPP

#include "../prefixes.hpp"

namespace fizmo {
namespace units {
namespace frequency {

constexpr FrequencyUnit hertz(1.0L);
FIZMO_SI_PREFIXED(hertz, hertz);   

constexpr FrequencyUnit rpm(1.0L / 60.0L);   
constexpr FrequencyUnit bpm = rpm;                           

namespace detail {
    constexpr long double period_to_hz(long double p, long double k) noexcept {
        return (p == 0.0L) ? constants::QUIET_NAN<long double> : 1.0L / (p * k);
    }
    constexpr long double minute_fwd(long double p) noexcept { return period_to_hz(p, 60.0L); }
    constexpr long double minute_inv(long double f) noexcept { return period_to_hz(f, 60.0L); }
    constexpr long double hour_fwd  (long double p) noexcept { return period_to_hz(p, 3600.0L); }
    constexpr long double hour_inv  (long double f) noexcept { return period_to_hz(f, 3600.0L); }
    constexpr long double day_fwd   (long double p) noexcept { return period_to_hz(p, 86400.0L); }
    constexpr long double day_inv   (long double f) noexcept { return period_to_hz(f, 86400.0L); }
}

constexpr FIZMO_NONLINEAR_OF(DIMENSION_FREQUENCY)
    period_minute(detail::minute_fwd, detail::minute_inv);

constexpr FIZMO_NONLINEAR_OF(DIMENSION_FREQUENCY)
    period_hour(detail::hour_fwd, detail::hour_fwd);

constexpr FIZMO_NONLINEAR_OF(DIMENSION_FREQUENCY)
    period_day(detail::day_fwd, detail::day_inv);

} // namespace frequency
} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_FREQUENCY_HPP