#ifndef DISTANCE_CONVERSIONS_HPP
#define DISTANCE_CONVERSIONS_HPP

#include "../Basic/constants.hpp"

namespace fizmo {
namespace converters {
namespace distance {

// Quetta- prefix (10^30)
constexpr long double quettameters_to_si(const long double qm) noexcept { return qm * 1e30; }
constexpr long double si_to_quettameters(const long double m) noexcept { return m * 1e-30; }

// Ronna- prefix (10^27)
constexpr long double ronnameters_to_si(const long double Rm) noexcept { return Rm * 1e27; }
constexpr long double si_to_ronnameters(const long double m) noexcept { return m * 1e-27; }

// Yotta- prefix (10^24)
constexpr long double yottameters_to_si(const long double Ym) noexcept { return Ym * 1e24; }
constexpr long double si_to_yottameters(const long double m) noexcept { return m * 1e-24; }

// Zetta- prefix (10^21)
constexpr long double zettameters_to_si(const long double Zm) noexcept { return Zm * 1e21; }
constexpr long double si_to_zettameters(const long double m) noexcept { return m * 1e-21; }

// Exa- prefix (10^18)
constexpr long double exameters_to_si(const long double Em) noexcept { return Em * 1e18; }
constexpr long double si_to_exameters(const long double m) noexcept { return m * 1e-18; }

// Peta- prefix (10^15)
constexpr long double petameters_to_si(const long double Pm) noexcept { return Pm * 1e15; }
constexpr long double si_to_petameters(const long double m) noexcept { return m * 1e-15; }

// Tera- prefix (10^12)
constexpr long double terameters_to_si(const long double Tm) noexcept { return Tm * 1e12; }
constexpr long double si_to_terameters(const long double m) noexcept { return m * 1e-12; }

// Giga- prefix (10^9)
constexpr long double gigameters_to_si(const long double Gm) noexcept { return Gm * 1e9; }
constexpr long double si_to_gigameters(const long double m) noexcept { return m * 1e-9; }

// Mega- prefix (10^6)
constexpr long double megameters_to_si(const long double Mm) noexcept { return Mm * 1e6; }
constexpr long double si_to_megameters(const long double m) noexcept { return m * 1e-6; }

// Kilo- prefix (10^3)
constexpr long double kilometers_to_si(const long double km) noexcept { return km * 1e3; }
constexpr long double si_to_kilometers(const long double m) noexcept { return m * 1e-3; }

// Hecto- prefix (10^2)
constexpr long double hectometers_to_si(const long double hm) noexcept { return hm * 1e2; }
constexpr long double si_to_hectometers(const long double m) noexcept { return m * 1e-2; }

// Deca- prefix (10^1)
constexpr long double decameters_to_si(const long double dam) noexcept { return dam * 1e1; }
constexpr long double si_to_decameters(const long double m) noexcept { return m * 1e-1; }

// Deci- prefix (10^-1)
constexpr long double decimeters_to_si(const long double dm) noexcept { return dm * 1e-1; }
constexpr long double si_to_decimeters(const long double m) noexcept { return m * 1e1; }

// Centi- prefix (10^-2)
constexpr long double centimeters_to_si(const long double cm) noexcept { return cm * 1e-2; }
constexpr long double si_to_centimeters(const long double m) noexcept { return m * 1e2; }

// Milli- prefix (10^-3)
constexpr long double millimeters_to_si(const long double mm) noexcept { return mm * 1e-3; }
constexpr long double si_to_millimeters(const long double m) noexcept { return m * 1e3; }

// Micro- prefix (10^-6)
constexpr long double micrometers_to_si(const long double um) noexcept { return um * 1e-6; }
constexpr long double si_to_micrometers(const long double m) noexcept { return m * 1e6; }

// Nano- prefix (10^-9)
constexpr long double nanometers_to_si(const long double nm) noexcept { return nm * 1e-9; }
constexpr long double si_to_nanometers(const long double m) noexcept { return m * 1e9; }

// Pico- prefix (10^-12)
constexpr long double picometers_to_si(const long double pm) noexcept { return pm * 1e-12; }
constexpr long double si_to_picometers(const long double m) noexcept { return m * 1e12; }

// Femto- prefix (10^-15)
constexpr long double femtometers_to_si(const long double fm) noexcept { return fm * 1e-15; }
constexpr long double si_to_femtometers(const long double m) noexcept { return m * 1e15; }

// Atto- prefix (10^-18)
constexpr long double attometers_to_si(const long double am) noexcept { return am * 1e-18; }
constexpr long double si_to_attometers(const long double m) noexcept { return m * 1e18; }

// Zepto- prefix (10^-21)
constexpr long double zeptometers_to_si(const long double zm) noexcept { return zm * 1e-21; }
constexpr long double si_to_zeptometers(const long double m) noexcept { return m * 1e21; }

// Yocto- prefix (10^-24)
constexpr long double yoctometers_to_si(const long double ym) noexcept { return ym * 1e-24; }
constexpr long double si_to_yoctometers(const long double m) noexcept { return m * 1e24; }

// Ronto- prefix (10^-27)
constexpr long double rontometers_to_si(const long double rm) noexcept { return rm * 1e-27; }
constexpr long double si_to_rontometers(const long double m) noexcept { return m * 1e27; }

// Quecto- prefix (10^-30)
constexpr long double quectometers_to_si(const long double qm) noexcept { return qm * 1e-30; }
constexpr long double si_to_quectometers(const long double m) noexcept { return m * 1e30; }

constexpr long double angstroms_to_si(const long double angstrom) noexcept { return angstrom * 1e-10; }
constexpr long double si_to_angstroms(const long double m) noexcept { return m * 1e10; }

// Another name for micrometer
constexpr long double microns_to_si(const long double micron) noexcept { return micron * 1e-6; }
constexpr long double si_to_microns(const long double m) noexcept { return m * 1e6; }

constexpr long double planck_to_si(const long double planck) noexcept { return planck * fizmo::constants::planck_length(); }
constexpr long double si_to_planck(const long double m) noexcept { return m / fizmo::constants::planck_length(); }

constexpr long double bohr_to_si(const long double bohr) noexcept { return bohr * fizmo::constants::bohr_radius(); }
constexpr long double si_to_bohr(const long double m) noexcept { return m / fizmo::constants::bohr_radius(); }

// Thou (mil) - 1/1000 of an inch
constexpr long double thou_to_si(const long double thou) noexcept { return thou * 2.54e-5; }
constexpr long double si_to_thou(const long double m) noexcept { return m / 2.54e-5; }

// Barleycorn - 1/3 of an inch
constexpr long double barleycorn_to_si(const long double bc) noexcept { return bc * 0.0254 / 3.0; }
constexpr long double si_to_barleycorn(const long double m) noexcept { return m / (0.0254 / 3.0); }

// Inch
constexpr long double inches_to_si(const long double inches) noexcept { return inches * 0.0254; }
constexpr long double si_to_inches(const long double m) noexcept { return m / 0.0254; }

// Hand (4 inches, used for horse height)
constexpr long double hands_to_si(const long double hands) noexcept { return hands * 0.1016; }
constexpr long double si_to_hands(const long double m) noexcept { return m / 0.1016; }

// Foot
constexpr long double feet_to_si(const long double feet) noexcept { return feet * 0.3048; }
constexpr long double si_to_feet(const long double m) noexcept { return m / 0.3048; }

// Yard
constexpr long double yards_to_si(const long double yards) noexcept { return yards * 0.9144; }
constexpr long double si_to_yards(const long double m) noexcept { return m / 0.9144; }

// Fathom (6 feet, used for depth)
constexpr long double fathoms_to_si(const long double fathoms) noexcept { return fathoms * 1.8288; }
constexpr long double si_to_fathoms(const long double m) noexcept { return m / 1.8288; }

// Rod/Pole/Perch (16.5 feet)
constexpr long double rods_to_si(const long double rods) noexcept { return rods * 5.0292; }
constexpr long double si_to_rods(const long double m) noexcept { return m / 5.0292; }

// Chain (66 feet, 4 rods)
constexpr long double chains_to_si(const long double chains) noexcept { return chains * 20.1168; }
constexpr long double si_to_chains(const long double m) noexcept { return m / 20.1168; }

// Furlong (660 feet, 10 chains, 1/8 mile)
constexpr long double furlongs_to_si(const long double furlongs) noexcept { return furlongs * 201.168; }
constexpr long double si_to_furlongs(const long double m) noexcept { return m / 201.168; }

// Mile (5280 feet)
constexpr long double miles_to_si(const long double miles) noexcept { return miles * 1609.344; }
constexpr long double si_to_miles(const long double m) noexcept { return m / 1609.344; }

// League (3 miles)
constexpr long double leagues_to_si(const long double leagues) noexcept { return leagues * 4828.032; }
constexpr long double si_to_leagues(const long double m) noexcept { return m / 4828.032; }

// Shackle (15 fathoms = 90 feet, used for anchor chain)
constexpr long double shackles_to_si(const long double shackles) noexcept { return shackles * 27.432; }
constexpr long double si_to_shackles(const long double m) noexcept { return m / 27.432; }

constexpr long double international_cables_to_si(const long double cables) noexcept { return cables * 185.2; }
constexpr long double si_to_international_cables(const long double m) noexcept { return m / 185.2; }

constexpr long double us_cables_to_si(const long double cables) noexcept { return cables * 219.456; }
constexpr long double si_to_us_cables(const long double m) noexcept { return m / 219.456; }

constexpr long double nautical_miles_to_si(const long double nm) noexcept { return nm * 1852.0; }
constexpr long double si_to_nautical_miles(const long double m) noexcept { return m / 1852.0; }

// Astronomical Unit (AU) - average Earth-Sun distance
constexpr long double au_to_si(const long double au) noexcept { return au * 1.495978707e11; }
constexpr long double si_to_au(const long double m) noexcept { return m / 1.495978707e11; }

// Light-Planck-time
constexpr long double light_planck_time_to_si(const long double lpt) noexcept { return lpt * fizmo::constants::light_speed() * fizmo::constants::planck_time(); }
constexpr long double si_to_light_planck_time(const long double m) noexcept { return m / (fizmo::constants::light_speed() * fizmo::constants::planck_time()); }

// Light-quectosecond
constexpr long double light_quectoseconds_to_si(const long double lqs) noexcept { return lqs * fizmo::constants::light_speed() * 1e-30; }
constexpr long double si_to_light_quectoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-30); }

// Light-rontosecond
constexpr long double light_rontoseconds_to_si(const long double lrs) noexcept { return lrs * fizmo::constants::light_speed() * 1e-27; }
constexpr long double si_to_light_rontoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-27); }

// Light-yoctosecond
constexpr long double light_yoctoseconds_to_si(const long double lys) noexcept { return lys * fizmo::constants::light_speed() * 1e-24; }
constexpr long double si_to_light_yoctoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-24); }

// Light-zeptosecond
constexpr long double light_zeptoseconds_to_si(const long double lzs) noexcept { return lzs * fizmo::constants::light_speed() * 1e-21; }
constexpr long double si_to_light_zeptoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-21); }

// Light-attosecond
constexpr long double light_attoseconds_to_si(const long double las) noexcept { return las * fizmo::constants::light_speed() * 1e-18; }
constexpr long double si_to_light_attoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-18); }

// Light-femtosecond
constexpr long double light_femtoseconds_to_si(const long double lfs) noexcept { return lfs * fizmo::constants::light_speed() * 1e-15; }
constexpr long double si_to_light_femtoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-15); }

// Light-picosecond
constexpr long double light_picoseconds_to_si(const long double lps) noexcept { return lps * fizmo::constants::light_speed() * 1e-12; }
constexpr long double si_to_light_picoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-12); }

// Light-nanosecond
constexpr long double light_nanoseconds_to_si(const long double lns) noexcept { return lns * fizmo::constants::light_speed() * 1e-9; }
constexpr long double si_to_light_nanoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-9); }

// Light-microsecond
constexpr long double light_microseconds_to_si(const long double lus) noexcept { return lus * fizmo::constants::light_speed() * 1e-6; }
constexpr long double si_to_light_microseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-6); }

// Light-millisecond
constexpr long double light_milliseconds_to_si(const long double lms) noexcept { return lms * fizmo::constants::light_speed() * 1e-3; }
constexpr long double si_to_light_milliseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-3); }

// Light-centisecond
constexpr long double light_centiseconds_to_si(const long double lcs) noexcept { return lcs * fizmo::constants::light_speed() * 1e-2; }
constexpr long double si_to_light_centiseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-2); }

// Light-decisecond
constexpr long double light_deciseconds_to_si(const long double lds) noexcept { return lds * fizmo::constants::light_speed() * 1e-1; }
constexpr long double si_to_light_deciseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e-1); }

// Light-second
constexpr long double light_seconds_to_si(const long double ls) noexcept { return ls * fizmo::constants::light_speed(); }
constexpr long double si_to_light_seconds(const long double m) noexcept { return m / fizmo::constants::light_speed(); }

// Light-decasecond
constexpr long double light_decaseconds_to_si(const long double ldas) noexcept { return ldas * fizmo::constants::light_speed() * 1e1; }
constexpr long double si_to_light_decaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e1); }

// Light-hectosecond
constexpr long double light_hectoseconds_to_si(const long double lhs) noexcept { return lhs * fizmo::constants::light_speed() * 1e2; }
constexpr long double si_to_light_hectoseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e2); }

// Light-kilosecond
constexpr long double light_kiloseconds_to_si(const long double lks) noexcept { return lks * fizmo::constants::light_speed() * 1e3; }
constexpr long double si_to_light_kiloseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e3); }

// Light-minute
constexpr long double light_minutes_to_si(const long double lm) noexcept { return lm * fizmo::constants::light_speed() * 60.0; }
constexpr long double si_to_light_minutes(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 60.0); }

// Light-megasecond
constexpr long double light_megaseconds_to_si(const long double lMs) noexcept { return lMs * fizmo::constants::light_speed() * 1e6; }
constexpr long double si_to_light_megaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e6); }

// Light-hour
constexpr long double light_hours_to_si(const long double lh) noexcept { return lh * fizmo::constants::light_speed() * 3600.0; }
constexpr long double si_to_light_hours(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 3600.0); }

// Light-gigasecond
constexpr long double light_gigaseconds_to_si(const long double lGs) noexcept { return lGs * fizmo::constants::light_speed() * 1e9; }
constexpr long double si_to_light_gigaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e9); }

// Light-day
constexpr long double light_days_to_si(const long double ld) noexcept { return ld * fizmo::constants::light_speed() * 86400.0; }
constexpr long double si_to_light_days(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 86400.0); }

// Light-week
constexpr long double light_weeks_to_si(const long double lw) noexcept { return lw * fizmo::constants::light_speed() * 604800.0; }
constexpr long double si_to_light_weeks(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 604800.0); }

// Light-terasecond
constexpr long double light_teraseconds_to_si(const long double lTs) noexcept { return lTs * fizmo::constants::light_speed() * 1e12; }
constexpr long double si_to_light_teraseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e12); }

// Light-petasecond
constexpr long double light_petaseconds_to_si(const long double lPs) noexcept { return lPs * fizmo::constants::light_speed() * 1e15; }
constexpr long double si_to_light_petaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e15); }

// Light-year
constexpr long double light_years_to_si(const long double ly) noexcept { return ly * fizmo::constants::light_speed() * 31557600.0; }
constexpr long double si_to_light_years(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 31557600.0); }

// Light-exasecond
constexpr long double light_exaseconds_to_si(const long double lEs) noexcept { return lEs * fizmo::constants::light_speed() * 1e18; }
constexpr long double si_to_light_exaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e18); }

// Light-zettasecond
constexpr long double light_zettaseconds_to_si(const long double lZs) noexcept { return lZs * fizmo::constants::light_speed() * 1e21; }
constexpr long double si_to_light_zettaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e21); }

// Light-yottasecond
constexpr long double light_yottaseconds_to_si(const long double lYs) noexcept { return lYs * fizmo::constants::light_speed() * 1e24; }
constexpr long double si_to_light_yottaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e24); }

// Light-ronnasecond
constexpr long double light_ronnaseconds_to_si(const long double lRs) noexcept { return lRs * fizmo::constants::light_speed() * 1e27; }
constexpr long double si_to_light_ronnaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e27); }

// Light-quettasecond
constexpr long double light_quettaseconds_to_si(const long double lQs) noexcept { return lQs * fizmo::constants::light_speed() * 1e30; }
constexpr long double si_to_light_quettaseconds(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 1e30); }

// Light-decade
constexpr long double light_decades_to_si(const long double ly) noexcept { return ly * fizmo::constants::light_speed() * 31557600.0; }
constexpr long double si_to_light_decades(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 315576000.0); }

// Light-century
constexpr long double light_centuries_to_si(const long double ly) noexcept { return ly * fizmo::constants::light_speed() * 31557600.0; }
constexpr long double si_to_light_centuries(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 3155760000.0); }

// Light-millennium
constexpr long double light_millennia_to_si(const long double ly) noexcept { return ly * fizmo::constants::light_speed() * 31557600.0; }
constexpr long double si_to_light_millennia(const long double m) noexcept { return m / (fizmo::constants::light_speed() * 31557600000.0); }

// Quectoparsec
constexpr long double quectoparsec_to_si(const long double qpc) noexcept { return qpc * fizmo::constants::parsec() * 1e-30; }
constexpr long double si_to_quectoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-30); }

// Rontoparsec
constexpr long double rontoparsec_to_si(const long double rpc) noexcept { return rpc * fizmo::constants::parsec() * 1e-27; }
constexpr long double si_to_rontoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-27); }

// Yoctoparsec
constexpr long double yoctoparsec_to_si(const long double ypc) noexcept { return ypc * fizmo::constants::parsec() * 1e-24; }
constexpr long double si_to_yoctoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-24); }

// Zeptoparsec
constexpr long double zeptoparsec_to_si(const long double zpc) noexcept { return zpc * fizmo::constants::parsec() * 1e-21; }
constexpr long double si_to_zeptoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-21); }

// Attoparsec
constexpr long double attoparsec_to_si(const long double apc) noexcept { return apc * fizmo::constants::parsec() * 1e-18; }
constexpr long double si_to_attoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-18); }

// Femtoparsec
constexpr long double femtoparsec_to_si(const long double fpc) noexcept { return fpc * fizmo::constants::parsec() * 1e-15; }
constexpr long double si_to_femtoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-15); }

// Picoparsec
constexpr long double picoparsec_to_si(const long double ppc) noexcept { return ppc * fizmo::constants::parsec() * 1e-12; }
constexpr long double si_to_picoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-12); }

// Nanoparsec
constexpr long double nanoparsec_to_si(const long double npc) noexcept { return npc * fizmo::constants::parsec() * 1e-9; }
constexpr long double si_to_nanoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-9); }

// Microparsec
constexpr long double microparsec_to_si(const long double upc) noexcept { return upc * fizmo::constants::parsec() * 1e-6; }
constexpr long double si_to_microparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-6); }

// Milliparsec
constexpr long double milliparsec_to_si(const long double mpc) noexcept { return mpc * fizmo::constants::parsec() * 1e-3; }
constexpr long double si_to_milliparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-3); }

// Centiparsec
constexpr long double centiparsec_to_si(const long double cpc) noexcept { return cpc * fizmo::constants::parsec() * 1e-2; }
constexpr long double si_to_centiparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-2); }

// Deciparsec
constexpr long double deciparsec_to_si(const long double dpc) noexcept { return dpc * fizmo::constants::parsec() * 1e-1; }
constexpr long double si_to_deciparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e-1); }

// Parsec (parallax arcsecond)
constexpr long double parsecs_to_si(const long double parsec) noexcept { return parsec * fizmo::constants::parsec(); }
constexpr long double si_to_parsecs(const long double m) noexcept { return m / fizmo::constants::parsec(); }

// Decaparsec
constexpr long double decaparsec_to_si(const long double dapc) noexcept { return dapc * fizmo::constants::parsec() * 1e1; }
constexpr long double si_to_decaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e1); }

// Hectoparsec
constexpr long double hectoparsec_to_si(const long double hpc) noexcept { return hpc * fizmo::constants::parsec() * 1e2; }
constexpr long double si_to_hectoparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e2); }

// Kiloparsec
constexpr long double kiloparsec_to_si(const long double kpc) noexcept { return kpc * fizmo::constants::parsec() * 1e3; }
constexpr long double si_to_kiloparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e3); }

// Megaparsec
constexpr long double megaparsec_to_si(const long double Mpc) noexcept { return Mpc * fizmo::constants::parsec() * 1e6; }
constexpr long double si_to_megaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e6); }

// Gigaparsec
constexpr long double gigaparsec_to_si(const long double Gpc) noexcept { return Gpc * fizmo::constants::parsec() * 1e9; }
constexpr long double si_to_gigaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e9); }

// Teraparsec
constexpr long double teraparsec_to_si(const long double Tpc) noexcept { return Tpc * fizmo::constants::parsec() * 1e12; }
constexpr long double si_to_teraparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e12); }

// Petaparsec
constexpr long double petaparsec_to_si(const long double Ppc) noexcept { return Ppc * fizmo::constants::parsec() * 1e15; }
constexpr long double si_to_petaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e15); }

// Exaparsec
constexpr long double exaparsec_to_si(const long double Epc) noexcept { return Epc * fizmo::constants::parsec() * 1e18; }
constexpr long double si_to_exaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e18); }

// Zettaparsec
constexpr long double zettaparsec_to_si(const long double Zpc) noexcept { return Zpc * fizmo::constants::parsec() * 1e21; }
constexpr long double si_to_zettaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e21); }

// Yottaparsec
constexpr long double yottaparsec_to_si(const long double Ypc) noexcept { return Ypc * fizmo::constants::parsec() * 1e24; }
constexpr long double si_to_yottaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e24); }

// Ronnaparsec
constexpr long double ronnaparsec_to_si(const long double Rpc) noexcept { return Rpc * fizmo::constants::parsec() * 1e27; }
constexpr long double si_to_ronnaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e27); }

// Quettaparsec
constexpr long double quettaparsec_to_si(const long double Qpc) noexcept { return Qpc * fizmo::constants::parsec() * 1e30; }
constexpr long double si_to_quettaparsec(const long double m) noexcept { return m / (fizmo::constants::parsec() * 1e30); }

// Smoot (5 feet 7 inches, humorous MIT unit)
constexpr long double smoots_to_si(const long double smoots) noexcept { return smoots * 1.7018; }
constexpr long double si_to_smoots(const long double m) noexcept { return m / 1.7018; }

// Ell (45 inches, cloth measurement)
constexpr long double ells_to_si(const long double ells) noexcept { return ells * 1.143; }
constexpr long double si_to_ells(const long double m) noexcept { return m / 1.143; }

// Link (Gunter's chain, 7.92 inches)
constexpr long double links_to_si(const long double links) noexcept { return links * 0.201168; }
constexpr long double si_to_links(const long double m) noexcept { return m / 0.201168; }

// Point (1/72 inch, typography)
constexpr long double points_to_si(const long double points) noexcept { return points * 0.000352777778; }
constexpr long double si_to_points(const long double m) noexcept { return m / 0.000352777778; }

// Pica (1/6 inch, typography)
constexpr long double picas_to_si(const long double picas) noexcept { return picas * 0.00423333333; }
constexpr long double si_to_picas(const long double m) noexcept { return m / 0.00423333333; }

// Ligne (1/12 French inch, used for buttons)
constexpr long double lignes_to_si(const long double lignes) noexcept { return lignes * 0.002256; }
constexpr long double si_to_lignes(const long double m) noexcept { return m / 0.002256; }

} // namespace distance
} // namespace converters
} // namespace fizmo

#endif // DISTANCE_CONVERSIONS_HPP