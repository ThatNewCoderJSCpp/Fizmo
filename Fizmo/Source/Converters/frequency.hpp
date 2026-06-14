#ifndef FREQUENCY_CONVERSIONS_HPP
#define FREQUENCY_CONVERSIONS_HPP

namespace fizmo {
namespace converters {
namespace frequency {

// Quecto- prefix (10^-30)
constexpr long double quectohertz_to_si(const long double qhz) noexcept { return qhz * 1e-30; }
constexpr long double si_to_quectohertz(const long double hz) noexcept { return hz * 1e30; }

// Ronto- prefix (10^-27)
constexpr long double rontohertz_to_si(const long double rhz) noexcept { return rhz * 1e-27; }
constexpr long double si_to_rontohertz(const long double hz) noexcept { return hz * 1e27; }

// Yocto- prefix (10^-24)
constexpr long double yoctohertz_to_si(const long double yhz) noexcept { return yhz * 1e-24; }
constexpr long double si_to_yoctohertz(const long double hz) noexcept { return hz * 1e24; }

// Zepto- prefix (10^-21)
constexpr long double zeptohertz_to_si(const long double zhz) noexcept { return zhz * 1e-21; }
constexpr long double si_to_zeptohertz(const long double hz) noexcept { return hz * 1e21; }

// Atto- prefix (10^-18)
constexpr long double attohertz_to_si(const long double ahz) noexcept { return ahz * 1e-18; }
constexpr long double si_to_attohertz(const long double hz) noexcept { return hz * 1e18; }

// Femto- prefix (10^-15)
constexpr long double femtohertz_to_si(const long double fhz) noexcept { return fhz * 1e-15; }
constexpr long double si_to_femtohertz(const long double hz) noexcept { return hz * 1e15; }

// Pico- prefix (10^-12)
constexpr long double picohertz_to_si(const long double phz) noexcept { return phz * 1e-12; }
constexpr long double si_to_picohertz(const long double hz) noexcept { return hz * 1e12; }

// Nano- prefix (10^-9)
constexpr long double nanohertz_to_si(const long double nhz) noexcept { return nhz * 1e-9; }
constexpr long double si_to_nanohertz(const long double hz) noexcept { return hz * 1e9; }

// Micro- prefix (10^-6)
constexpr long double microhertz_to_si(const long double uhz) noexcept { return uhz * 1e-6; }
constexpr long double si_to_microhertz(const long double hz) noexcept { return hz * 1e6; }

// Milli- prefix (10^-3)
constexpr long double millihertz_to_si(const long double mhz) noexcept { return mhz * 1e-3; }
constexpr long double si_to_millihertz(const long double hz) noexcept { return hz * 1e3; }

// Centi- prefix (10^-2)
constexpr long double centihertz_to_si(const long double chz) noexcept { return chz * 1e-2; }
constexpr long double si_to_centihertz(const long double hz) noexcept { return hz * 1e2; }

// Deci- prefix (10^-1)
constexpr long double decihertz_to_si(const long double dhz) noexcept { return dhz * 1e-1; }
constexpr long double si_to_decihertz(const long double hz) noexcept { return hz * 1e1; }

// Deca- prefix (10^1)
constexpr long double decahertz_to_si(const long double dahz) noexcept { return dahz * 1e1; }
constexpr long double si_to_decahertz(const long double hz) noexcept { return hz * 1e-1; }

// Hecto- prefix (10^2)
constexpr long double hectohertz_to_si(const long double hhz) noexcept { return hhz * 1e2; }
constexpr long double si_to_hectohertz(const long double hz) noexcept { return hz * 1e-2; }

// Kilo- prefix (10^3)
constexpr long double kilohertz_to_si(const long double khz) noexcept { return khz * 1e3; }
constexpr long double si_to_kilohertz(const long double hz) noexcept { return hz * 1e-3; }

// Mega- prefix (10^6)
constexpr long double megahertz_to_si(const long double Mhz) noexcept { return Mhz * 1e6; }
constexpr long double si_to_megahertz(const long double hz) noexcept { return hz * 1e-6; }

// Giga- prefix (10^9)
constexpr long double gigahertz_to_si(const long double Ghz) noexcept { return Ghz * 1e9; }
constexpr long double si_to_gigahertz(const long double hz) noexcept { return hz * 1e-9; }

// Tera- prefix (10^12)
constexpr long double terahertz_to_si(const long double Thz) noexcept { return Thz * 1e12; }
constexpr long double si_to_terahertz(const long double hz) noexcept { return hz * 1e-12; }

// Peta- prefix (10^15)
constexpr long double petahertz_to_si(const long double Phz) noexcept { return Phz * 1e15; }
constexpr long double si_to_petahertz(const long double hz) noexcept { return hz * 1e-15; }

// Exa- prefix (10^18)
constexpr long double exahertz_to_si(const long double Ehz) noexcept { return Ehz * 1e18; }
constexpr long double si_to_exahertz(const long double hz) noexcept { return hz * 1e-18; }

// Zetta- prefix (10^21)
constexpr long double zettahertz_to_si(const long double Zhz) noexcept { return Zhz * 1e21; }
constexpr long double si_to_zettahertz(const long double hz) noexcept { return hz * 1e-21; }

// Yotta- prefix (10^24)
constexpr long double yottahertz_to_si(const long double Yhz) noexcept { return Yhz * 1e24; }
constexpr long double si_to_yottahertz(const long double hz) noexcept { return hz * 1e-24; }

// Ronna- prefix (10^27)
constexpr long double ronnahertz_to_si(const long double Rhz) noexcept { return Rhz * 1e27; }
constexpr long double si_to_ronnahertz(const long double hz) noexcept { return hz * 1e-27; }

// Quetta- prefix (10^30)
constexpr long double quettahertz_to_si(const long double Qhz) noexcept { return Qhz * 1e30; }
constexpr long double si_to_quettahertz(const long double hz) noexcept { return hz * 1e-30; }

constexpr long double period_minute_to_si(const long double min) noexcept { 
    if (std::abs(min) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (min * 60.0); 
}

constexpr long double period_hour_to_si(const long double hr) noexcept { 
    if (std::abs(hr) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hr * 3600.0); 
}

constexpr long double period_day_to_si(const long double d) noexcept { 
    if (std::abs(d) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (d * 86400.0); 
}

constexpr long double period_week_to_si(const long double w) noexcept { 
    if (std::abs(w) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (w * 604800.0); 
}

constexpr long double period_year_to_si(const long double yr) noexcept { 
    if (std::abs(yr) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (yr * 31557600.0); 
}

constexpr long double period_decade_to_si(const long double dec) noexcept { 
    if (std::abs(dec) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (dec * 315576000.0); 
}

constexpr long double period_century_to_si(const long double cen) noexcept { 
    if (std::abs(cen) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (cen * 3155760000.0); 
}

constexpr long double period_millennium_to_si(const long double mil) noexcept { 
    if (std::abs(mil) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (mil * 31557600000.0); 
}

constexpr long double si_to_period_minute(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 60.0); 
}

constexpr long double si_to_period_hour(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 3600.0); 
}

constexpr long double si_to_period_day(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 86400.0); 
}

constexpr long double si_to_period_week(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 604800.0); 
}

constexpr long double si_to_period_year(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 31557600.0); 
}

constexpr long double si_to_period_decade(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 315576000.0); 
}

constexpr long double si_to_period_century(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 3155760000.0); 
}

constexpr long double si_to_period_millennium(const long double hz) noexcept { 
    if (std::abs(hz) <= fizmo::constants::middle_epsilon()) { return fizmo::constants::quiet_nan(); }
    return 1.0 / (hz * 31557600000.0); 
}

} // namespace frequency
} // namespace converters
} // namespace fizmo

#endif // FREQUENCY_CONVERSIONS_HPP