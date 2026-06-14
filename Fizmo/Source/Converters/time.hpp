#ifndef TIME_CONVERSIONS_HPP
#define TIME_CONVERSIONS_HPP

namespace fizmo {
namespace converters {
namespace time {

// Quecto- prefix (10^-30)
constexpr long double quectoseconds_to_si(const long double qs) noexcept { return qs * 1e-30; }
constexpr long double si_to_quectoseconds(const long double s) noexcept { return s * 1e30; }

// Ronto- prefix (10^-27)
constexpr long double rontoseconds_to_si(const long double rs) noexcept { return rs * 1e-27; }
constexpr long double si_to_rontoseconds(const long double s) noexcept { return s * 1e27; }

// Yocto- prefix (10^-24)
constexpr long double yoctoseconds_to_si(const long double ys) noexcept { return ys * 1e-24; }
constexpr long double si_to_yoctoseconds(const long double s) noexcept { return s * 1e24; }

// Zepto- prefix (10^-21)
constexpr long double zeptoseconds_to_si(const long double zs) noexcept { return zs * 1e-21; }
constexpr long double si_to_zeptoseconds(const long double s) noexcept { return s * 1e21; }

// Atto- prefix (10^-18)
constexpr long double attoseconds_to_si(const long double as) noexcept { return as * 1e-18; }
constexpr long double si_to_attoseconds(const long double s) noexcept { return s * 1e18; }

// Femto- prefix (10^-15)
constexpr long double femtoseconds_to_si(const long double fs) noexcept { return fs * 1e-15; }
constexpr long double si_to_femtoseconds(const long double s) noexcept { return s * 1e15; }

// Pico- prefix (10^-12)
constexpr long double picoseconds_to_si(const long double ps) noexcept { return ps * 1e-12; }
constexpr long double si_to_picoseconds(const long double s) noexcept { return s * 1e12; }

// Nano- prefix (10^-9)
constexpr long double nanoseconds_to_si(const long double ns) noexcept { return ns * 1e-9; }
constexpr long double si_to_nanoseconds(const long double s) noexcept { return s * 1e9; }

// Micro- prefix (10^-6)
constexpr long double microseconds_to_si(const long double us) noexcept { return us * 1e-6; }
constexpr long double si_to_microseconds(const long double s) noexcept { return s * 1e6; }

// Milli- prefix (10^-3)
constexpr long double milliseconds_to_si(const long double ms) noexcept { return ms * 1e-3; }
constexpr long double si_to_milliseconds(const long double s) noexcept { return s * 1e3; }

// Centi- prefix (10^-2)
constexpr long double centiseconds_to_si(const long double cs) noexcept { return cs * 1e-2; }
constexpr long double si_to_centiseconds(const long double s) noexcept { return s * 1e2; }

// Deci- prefix (10^-1)
constexpr long double deciseconds_to_si(const long double ds) noexcept { return ds * 1e-1; }
constexpr long double si_to_deciseconds(const long double s) noexcept { return s * 1e1; }

// Deca- prefix (10^1)
constexpr long double decaseconds_to_si(const long double das) noexcept { return das * 1e1; }
constexpr long double si_to_decaseconds(const long double s) noexcept { return s * 1e-1; }

// Hecto- prefix (10^2)
constexpr long double hectoseconds_to_si(const long double hs) noexcept { return hs * 1e2; }
constexpr long double si_to_hectoseconds(const long double s) noexcept { return s * 1e-2; }

// Kilo- prefix (10^3)
constexpr long double kiloseconds_to_si(const long double ks) noexcept { return ks * 1e3; }
constexpr long double si_to_kiloseconds(const long double s) noexcept { return s * 1e-3; }

// Mega- prefix (10^6)
constexpr long double megaseconds_to_si(const long double Ms) noexcept { return Ms * 1e6; }
constexpr long double si_to_megaseconds(const long double s) noexcept { return s * 1e-6; }

// Giga- prefix (10^9)
constexpr long double gigaseconds_to_si(const long double Gs) noexcept { return Gs * 1e9; }
constexpr long double si_to_gigaseconds(const long double s) noexcept { return s * 1e-9; }

// Tera- prefix (10^12)
constexpr long double teraseconds_to_si(const long double Ts) noexcept { return Ts * 1e12; }
constexpr long double si_to_teraseconds(const long double s) noexcept { return s * 1e-12; }

// Peta- prefix (10^15)
constexpr long double petaseconds_to_si(const long double Ps) noexcept { return Ps * 1e15; }
constexpr long double si_to_petaseconds(const long double s) noexcept { return s * 1e-15; }

// Exa- prefix (10^18)
constexpr long double exaseconds_to_si(const long double Es) noexcept { return Es * 1e18; }
constexpr long double si_to_exaseconds(const long double s) noexcept { return s * 1e-18; }

// Zetta- prefix (10^21)
constexpr long double zettaseconds_to_si(const long double Zs) noexcept { return Zs * 1e21; }
constexpr long double si_to_zettaseconds(const long double s) noexcept { return s * 1e-21; }

// Yotta- prefix (10^24)
constexpr long double yottaseconds_to_si(const long double Ys) noexcept { return Ys * 1e24; }
constexpr long double si_to_yottaseconds(const long double s) noexcept { return s * 1e-24; }

// Ronna- prefix (10^27)
constexpr long double ronnaseconds_to_si(const long double Rs) noexcept { return Rs * 1e27; }
constexpr long double si_to_ronnaseconds(const long double s) noexcept { return s * 1e-27; }

// Quetta- prefix (10^30)
constexpr long double quettaseconds_to_si(const long double Qs) noexcept { return Qs * 1e30; }
constexpr long double si_to_quettaseconds(const long double s) noexcept { return s * 1e-30; }

// Common time units
constexpr long double minutes_to_si(const long double minutes) noexcept { return minutes * 60.0; }
constexpr long double si_to_minutes(const long double s) noexcept { return s / 60.0; }

constexpr long double hours_to_si(const long double hours) noexcept { return hours * 3600.0; }
constexpr long double si_to_hours(const long double s) noexcept { return s / 3600.0; }

constexpr long double days_to_si(const long double days) noexcept { return days * 86400.0; }
constexpr long double si_to_days(const long double s) noexcept { return s / 86400.0; }

constexpr long double weeks_to_si(const long double weeks) noexcept { return weeks * 604800.0; }
constexpr long double si_to_weeks(const long double s) noexcept { return s / 604800.0; }

constexpr long double years_to_si(const long double years) noexcept { return years * 31557600.0; } // Based on 365.25 days
constexpr long double si_to_years(const long double s) noexcept { return s / 31557600.0; }

constexpr long double decades_to_si(const long double decades) noexcept { return decades * 315576000.0; }
constexpr long double si_to_decades(const long double s) noexcept { return s / 315576000.0; }

constexpr long double centuries_to_si(const long double centuries) noexcept { return centuries * 3155760000.0; }
constexpr long double si_to_centuries(const long double s) noexcept { return s / 3155760000.0; }

constexpr long double millennia_to_si(const long double millennia) noexcept { return millennia * 31557600000.0; }
constexpr long double si_to_millennia(const long double s) noexcept { return s / 31557600000.0; }

} // namespace time
} // namespace converters
} // namespace fizmo

#endif // TIME_CONVERSIONS_HPP