#ifndef FIZMO_CORE_PREFIXES_HPP
#define FIZMO_CORE_PREFIXES_HPP

#include "convert.hpp"

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
    constexpr auto quecto##BASE = ::fizmo::units::prefixed(1e-30L, U); \
    constexpr auto ronto##BASE  = ::fizmo::units::prefixed(1e-27L, U); \
    constexpr auto yocto##BASE  = ::fizmo::units::prefixed(1e-24L, U); \
    constexpr auto zepto##BASE  = ::fizmo::units::prefixed(1e-21L, U); \
    constexpr auto atto##BASE   = ::fizmo::units::prefixed(1e-18L, U); \
    constexpr auto femto##BASE  = ::fizmo::units::prefixed(1e-15L, U); \
    constexpr auto pico##BASE   = ::fizmo::units::prefixed(1e-12L, U); \
    constexpr auto nano##BASE   = ::fizmo::units::prefixed(1e-9L,  U); \
    constexpr auto micro##BASE  = ::fizmo::units::prefixed(1e-6L,  U); \
    constexpr auto milli##BASE  = ::fizmo::units::prefixed(1e-3L,  U); \
    constexpr auto centi##BASE  = ::fizmo::units::prefixed(1e-2L,  U); \
    constexpr auto deci##BASE   = ::fizmo::units::prefixed(1e-1L,  U); \
    constexpr auto deca##BASE   = ::fizmo::units::prefixed(1e1L,   U); \
    constexpr auto hecto##BASE  = ::fizmo::units::prefixed(1e2L,   U); \
    constexpr auto kilo##BASE   = ::fizmo::units::prefixed(1e3L,   U); \
    constexpr auto mega##BASE   = ::fizmo::units::prefixed(1e6L,   U); \
    constexpr auto giga##BASE   = ::fizmo::units::prefixed(1e9L,   U); \
    constexpr auto tera##BASE   = ::fizmo::units::prefixed(1e12L,  U); \
    constexpr auto peta##BASE   = ::fizmo::units::prefixed(1e15L,  U); \
    constexpr auto exa##BASE    = ::fizmo::units::prefixed(1e18L,  U); \
    constexpr auto zetta##BASE  = ::fizmo::units::prefixed(1e21L,  U); \
    constexpr auto yotta##BASE  = ::fizmo::units::prefixed(1e24L,  U); \
    constexpr auto ronna##BASE  = ::fizmo::units::prefixed(1e27L,  U); \
    constexpr auto quetta##BASE = ::fizmo::units::prefixed(1e30L,  U)

#define FIZMO_SI_PREFIXED_POWERED(PREFIX, BASE_NAME, BASE_UNIT, POWER)                 \
    constexpr auto PREFIX##_##BASE_NAME =                             \
        (BASE_UNIT).pow<POWER>();                                                       \
    constexpr auto PREFIX##_quecto##BASE_NAME =                       \
        ::fizmo::units::prefixed(1e-30L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_ronto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-27L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_yocto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-24L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_zepto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-21L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_atto##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e-18L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_femto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-15L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_pico##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e-12L, BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_nano##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e-9L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_micro##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-6L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_milli##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-3L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_centi##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e-2L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_deci##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e-1L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_deca##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e1L,   BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_hecto##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e2L,   BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_kilo##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e3L,   BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_mega##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e6L,   BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_giga##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e9L,   BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_tera##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e12L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_peta##BASE_NAME   =                       \
        ::fizmo::units::prefixed(1e15L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_exa##BASE_NAME    =                       \
        ::fizmo::units::prefixed(1e18L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_zetta##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e21L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_yotta##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e24L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_ronna##BASE_NAME  =                       \
        ::fizmo::units::prefixed(1e27L,  BASE_UNIT).pow<POWER>();                       \
    constexpr auto PREFIX##_quetta##BASE_NAME =                       \
        ::fizmo::units::prefixed(1e30L,  BASE_UNIT).pow<POWER>()

} // namespace units
} // namespace fizmo

#endif // FIZMO_CORE_PREFIXES_HPP