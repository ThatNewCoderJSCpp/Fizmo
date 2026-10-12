#ifndef FIZMO_CORE_PREFIXES_HPP
#define FIZMO_CORE_PREFIXES_HPP

#include "exact_constants.hpp"

namespace fizmo {
namespace units {
namespace prefix {

constexpr long double quecto = 1e-30L; constexpr long double ronto = 1e-27L;
constexpr long double yocto  = 1e-24L; constexpr long double zepto = 1e-21L;
constexpr long double atto   = 1e-18L; constexpr long double femto = 1e-15L;
constexpr long double pico   = 1e-12L; constexpr long double nano  = 1e-9L;
constexpr long double micro  = 1e-6L;  constexpr long double milli = 1e-3L;
constexpr long double centi  = 1e-2L;  constexpr long double deci  = 1e-1L;
constexpr long double deca   = 1e1L;   constexpr long double hecto = 1e2L;
constexpr long double kilo   = 1e3L;   constexpr long double mega  = 1e6L;
constexpr long double giga   = 1e9L;   constexpr long double tera  = 1e12L;
constexpr long double peta   = 1e15L;  constexpr long double exa   = 1e18L;
constexpr long double zetta  = 1e21L;  constexpr long double yotta = 1e24L;
constexpr long double ronna  = 1e27L;  constexpr long double quetta= 1e30L;

} // namespace prefix

#define FIZMO_SI_PREFIXED(BASE, U)                                            \
    constexpr auto quecto##BASE = ::fizmo::units::prefixed(::fizmo::units::exact("1e-30"), U); \
    constexpr auto ronto##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-27"), U); \
    constexpr auto yocto##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-24"), U); \
    constexpr auto zepto##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-21"), U); \
    constexpr auto atto##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e-18"), U); \
    constexpr auto femto##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-15"), U); \
    constexpr auto pico##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e-12"), U); \
    constexpr auto nano##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e-9"), U); \
    constexpr auto micro##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-6"), U); \
    constexpr auto milli##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-3"), U); \
    constexpr auto centi##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e-2"), U); \
    constexpr auto deci##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e-1"), U); \
    constexpr auto deca##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e1"), U); \
    constexpr auto hecto##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e2"), U); \
    constexpr auto kilo##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e3"), U); \
    constexpr auto mega##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e6"), U); \
    constexpr auto giga##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e9"), U); \
    constexpr auto tera##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e12"), U); \
    constexpr auto peta##BASE   = ::fizmo::units::prefixed(::fizmo::units::exact("1e15"), U); \
    constexpr auto exa##BASE    = ::fizmo::units::prefixed(::fizmo::units::exact("1e18"), U); \
    constexpr auto zetta##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e21"), U); \
    constexpr auto yotta##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e24"), U); \
    constexpr auto ronna##BASE  = ::fizmo::units::prefixed(::fizmo::units::exact("1e27"), U); \
    constexpr auto quetta##BASE = ::fizmo::units::prefixed(::fizmo::units::exact("1e30"), U)

#define FIZMO_SI_PREFIXED_POWERED(PREFIX, BASE_NAME, BASE_UNIT, POWER)                 \
    constexpr auto PREFIX##_##BASE_NAME =                             \
        (BASE_UNIT).pow<POWER>();                                                       \
    constexpr auto PREFIX##_quecto##BASE_NAME =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-30"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_ronto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-27"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_yocto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-24"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_zepto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-21"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_atto##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-18"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_femto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-15"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_pico##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-12"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_nano##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-9"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_micro##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-6"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_milli##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-3"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_centi##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-2"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_deci##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e-1"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_deca##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e1"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_hecto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e2"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_kilo##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e3"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_mega##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e6"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_giga##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e9"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_tera##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e12"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_peta##BASE_NAME   =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e15"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_exa##BASE_NAME    =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e18"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_zetta##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e21"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_yotta##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e24"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_ronna##BASE_NAME  =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e27"), BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_quetta##BASE_NAME =                       \
        ::fizmo::units::prefixed(::fizmo::units::exact("1e30"), BASE_UNIT).pow<POWER>()

} // namespace units
} // namespace fizmo

#endif // FIZMO_CORE_PREFIXES_HPP