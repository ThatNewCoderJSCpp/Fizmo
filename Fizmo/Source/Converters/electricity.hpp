#ifndef ELECTRICAL_CONVERSIONS_HPP
#define ELECTRICAL_CONVERSIONS_HPP

#include "../Basic/constants.hpp"
#include <cmath>

namespace fizmo {
namespace converters {
namespace electrical {

namespace charge {
    // Quecto- prefix (10^-30)
    constexpr long double quectocoulomb_to_si(const long double qc) noexcept { return qc * 1e-30; }
    constexpr long double si_to_quectocoulomb(const long double c) noexcept { return c * 1e30; }

    // Ronto- prefix (10^-27)
    constexpr long double rontocoulomb_to_si(const long double rc) noexcept { return rc * 1e-27; }
    constexpr long double si_to_rontocoulomb(const long double c) noexcept { return c * 1e27; }

    // Yocto- prefix (10^-24)
    constexpr long double yoctocoulomb_to_si(const long double yc) noexcept { return yc * 1e-24; }
    constexpr long double si_to_yoctocoulomb(const long double c) noexcept { return c * 1e24; }

    // Zepto- prefix (10^-21)
    constexpr long double zeptocoulomb_to_si(const long double zc) noexcept { return zc * 1e-21; }
    constexpr long double si_to_zeptocoulomb(const long double c) noexcept { return c * 1e21; }

    // Atto- prefix (10^-18)
    constexpr long double attocoulomb_to_si(const long double ac) noexcept { return ac * 1e-18; }
    constexpr long double si_to_attocoulomb(const long double c) noexcept { return c * 1e18; }

    // Femto- prefix (10^-15)
    constexpr long double femtocoulomb_to_si(const long double fc) noexcept { return fc * 1e-15; }
    constexpr long double si_to_femtocoulomb(const long double c) noexcept { return c * 1e15; }

    // Pico- prefix (10^-12)
    constexpr long double picocoulomb_to_si(const long double pc) noexcept { return pc * 1e-12; }
    constexpr long double si_to_picocoulomb(const long double c) noexcept { return c * 1e12; }

    // Nano- prefix (10^-9)
    constexpr long double nanocoulomb_to_si(const long double nc) noexcept { return nc * 1e-9; }
    constexpr long double si_to_nanocoulomb(const long double c) noexcept { return c * 1e9; }

    // Micro- prefix (10^-6)
    constexpr long double microcoulomb_to_si(const long double uc) noexcept { return uc * 1e-6; }
    constexpr long double si_to_microcoulomb(const long double c) noexcept { return c * 1e6; }

    // Milli- prefix (10^-3)
    constexpr long double millicoulomb_to_si(const long double mc) noexcept { return mc * 1e-3; }
    constexpr long double si_to_millicoulomb(const long double c) noexcept { return c * 1e3; }

    // Centi- prefix (10^-2)
    constexpr long double centicoulomb_to_si(const long double cc) noexcept { return cc * 1e-2; }
    constexpr long double si_to_centicoulomb(const long double c) noexcept { return c * 1e2; }

    // Deci- prefix (10^-1)
    constexpr long double decicoulomb_to_si(const long double dc) noexcept { return dc * 1e-1; }
    constexpr long double si_to_decicoulomb(const long double c) noexcept { return c * 1e1; }

    // Deca- prefix (10^1)
    constexpr long double decacoulomb_to_si(const long double dac) noexcept { return dac * 1e1; }
    constexpr long double si_to_decacoulomb(const long double c) noexcept { return c * 1e-1; }

    // Hecto- prefix (10^2)
    constexpr long double hectocoulomb_to_si(const long double hc) noexcept { return hc * 1e2; }
    constexpr long double si_to_hectocoulomb(const long double c) noexcept { return c * 1e-2; }

    // Kilo- prefix (10^3)
    constexpr long double kilocoulomb_to_si(const long double kc) noexcept { return kc * 1e3; }
    constexpr long double si_to_kilocoulomb(const long double c) noexcept { return c * 1e-3; }

    // Mega- prefix (10^6)
    constexpr long double megacoulomb_to_si(const long double Mc) noexcept { return Mc * 1e6; }
    constexpr long double si_to_megacoulomb(const long double c) noexcept { return c * 1e-6; }

    // Giga- prefix (10^9)
    constexpr long double gigacoulomb_to_si(const long double Gc) noexcept { return Gc * 1e9; }
    constexpr long double si_to_gigacoulomb(const long double c) noexcept { return c * 1e-9; }

    // Tera- prefix (10^12)
    constexpr long double teracoulomb_to_si(const long double Tc) noexcept { return Tc * 1e12; }
    constexpr long double si_to_teracoulomb(const long double c) noexcept { return c * 1e-12; }

    // Peta- prefix (10^15)
    constexpr long double petacoulomb_to_si(const long double Pc) noexcept { return Pc * 1e15; }
    constexpr long double si_to_petacoulomb(const long double c) noexcept { return c * 1e-15; }

    // Exa- prefix (10^18)
    constexpr long double exacoulomb_to_si(const long double Ec) noexcept { return Ec * 1e18; }
    constexpr long double si_to_exacoulomb(const long double c) noexcept { return c * 1e-18; }

    // Zetta- prefix (10^21)
    constexpr long double zettacoulomb_to_si(const long double Zc) noexcept { return Zc * 1e21; }
    constexpr long double si_to_zettacoulomb(const long double c) noexcept { return c * 1e-21; }

    // Yotta- prefix (10^24)
    constexpr long double yottacoulomb_to_si(const long double Yc) noexcept { return Yc * 1e24; }
    constexpr long double si_to_yottacoulomb(const long double c) noexcept { return c * 1e-24; }

    // Ronna- prefix (10^27)
    constexpr long double ronnacoulomb_to_si(const long double Rc) noexcept { return Rc * 1e27; }
    constexpr long double si_to_ronnacoulomb(const long double c) noexcept { return c * 1e-27; }

    // Quetta- prefix (10^30)
    constexpr long double quettacoulomb_to_si(const long double Qc) noexcept { return Qc * 1e30; }
    constexpr long double si_to_quettacoulomb(const long double c) noexcept { return c * 1e-30; }

    // Special units
    constexpr long double ampere_hour_to_si(const long double ah) noexcept { return ah * 3600.0; }
    constexpr long double si_to_ampere_hour(const long double c) noexcept { return c / 3600.0; }

    constexpr long double milliampere_hour_to_si(const long double mah) noexcept { return mah * 3.6; }
    constexpr long double si_to_milliampere_hour(const long double c) noexcept { return c / 3.6; }

    constexpr long double faraday_to_si(const long double f) noexcept { return f * fizmo::constants::faraday(); }
    constexpr long double si_to_faraday(const long double c) noexcept { return c / fizmo::constants::faraday(); }

    constexpr long double elementary_charge_to_si(const long double e) noexcept { return e * fizmo::constants::elementary_charge(); }
    constexpr long double si_to_elementary_charge(const long double c) noexcept { return c / fizmo::constants::elementary_charge(); }

    constexpr long double statcoulomb_to_si(const long double sc) noexcept { return sc * fizmo::constants::esu_charge(); }
    constexpr long double si_to_statcoulomb(const long double c) noexcept { return c / fizmo::constants::esu_charge(); }

    constexpr long double abcoulomb_to_si(const long double abc) noexcept { return abc * 10.0; }
    constexpr long double si_to_abcoulomb(const long double c) noexcept { return c * 0.1; }

    constexpr long double planck_coulomb_to_si(const long double pc) noexcept { return pc * fizmo::constants::planck_charge(); }
    constexpr long double si_to_planck_coulomb(const long double c) noexcept { return c / fizmo::constants::planck_charge(); }
}

namespace current {
    // Quecto- prefix (10^-30)
    constexpr long double quectoampere_to_si(const long double qa) noexcept { return qa * 1e-30; }
    constexpr long double si_to_quectoampere(const long double a) noexcept { return a * 1e30; }

    // Ronto- prefix (10^-27)
    constexpr long double rontoampere_to_si(const long double ra) noexcept { return ra * 1e-27; }
    constexpr long double si_to_rontoampere(const long double a) noexcept { return a * 1e27; }

    // Yocto- prefix (10^-24)
    constexpr long double yoctoampere_to_si(const long double ya) noexcept { return ya * 1e-24; }
    constexpr long double si_to_yoctoampere(const long double a) noexcept { return a * 1e24; }

    // Zepto- prefix (10^-21)
    constexpr long double zeptoampere_to_si(const long double za) noexcept { return za * 1e-21; }
    constexpr long double si_to_zeptoampere(const long double a) noexcept { return a * 1e21; }

    // Atto- prefix (10^-18)
    constexpr long double attoampere_to_si(const long double aa) noexcept { return aa * 1e-18; }
    constexpr long double si_to_attoampere(const long double a) noexcept { return a * 1e18; }

    // Femto- prefix (10^-15)
    constexpr long double femtoampere_to_si(const long double fa) noexcept { return fa * 1e-15; }
    constexpr long double si_to_femtoampere(const long double a) noexcept { return a * 1e15; }

    // Pico- prefix (10^-12)
    constexpr long double picoampere_to_si(const long double pa) noexcept { return pa * 1e-12; }
    constexpr long double si_to_picoampere(const long double a) noexcept { return a * 1e12; }

    // Nano- prefix (10^-9)
    constexpr long double nanoampere_to_si(const long double na) noexcept { return na * 1e-9; }
    constexpr long double si_to_nanoampere(const long double a) noexcept { return a * 1e9; }

    // Micro- prefix (10^-6)
    constexpr long double microampere_to_si(const long double ua) noexcept { return ua * 1e-6; }
    constexpr long double si_to_microampere(const long double a) noexcept { return a * 1e6; }

    // Milli- prefix (10^-3)
    constexpr long double milliampere_to_si(const long double ma) noexcept { return ma * 1e-3; }
    constexpr long double si_to_milliampere(const long double a) noexcept { return a * 1e3; }

    // Centi- prefix (10^-2)
    constexpr long double centiampere_to_si(const long double ca) noexcept { return ca * 1e-2; }
    constexpr long double si_to_centiampere(const long double a) noexcept { return a * 1e2; }

    // Deci- prefix (10^-1)
    constexpr long double deciampere_to_si(const long double da) noexcept { return da * 1e-1; }
    constexpr long double si_to_deciampere(const long double a) noexcept { return a * 1e1; }

    // Deca- prefix (10^1)
    constexpr long double decaampere_to_si(const long double daa) noexcept { return daa * 1e1; }
    constexpr long double si_to_decaampere(const long double a) noexcept { return a * 1e-1; }

    // Hecto- prefix (10^2)
    constexpr long double hectoampere_to_si(const long double ha) noexcept { return ha * 1e2; }
    constexpr long double si_to_hectoampere(const long double a) noexcept { return a * 1e-2; }

    // Kilo- prefix (10^3)
    constexpr long double kiloampere_to_si(const long double ka) noexcept { return ka * 1e3; }
    constexpr long double si_to_kiloampere(const long double a) noexcept { return a * 1e-3; }

    // Mega- prefix (10^6)
    constexpr long double megaampere_to_si(const long double Ma) noexcept { return Ma * 1e6; }
    constexpr long double si_to_megaampere(const long double a) noexcept { return a * 1e-6; }

    // Giga- prefix (10^9)
    constexpr long double gigaampere_to_si(const long double Ga) noexcept { return Ga * 1e9; }
    constexpr long double si_to_gigaampere(const long double a) noexcept { return a * 1e-9; }

    // Tera- prefix (10^12)
    constexpr long double teraampere_to_si(const long double Ta) noexcept { return Ta * 1e12; }
    constexpr long double si_to_teraampere(const long double a) noexcept { return a * 1e-12; }

    // Peta- prefix (10^15)
    constexpr long double petaampere_to_si(const long double Pa) noexcept { return Pa * 1e15; }
    constexpr long double si_to_petaampere(const long double a) noexcept { return a * 1e-15; }

    // Exa- prefix (10^18)
    constexpr long double exaampere_to_si(const long double Ea) noexcept { return Ea * 1e18; }
    constexpr long double si_to_exaampere(const long double a) noexcept { return a * 1e-18; }

    // Zetta- prefix (10^21)
    constexpr long double zettaampere_to_si(const long double Za) noexcept { return Za * 1e21; }
    constexpr long double si_to_zettaampere(const long double a) noexcept { return a * 1e-21; }

    // Yotta- prefix (10^24)
    constexpr long double yottaampere_to_si(const long double Ya) noexcept { return Ya * 1e24; }
    constexpr long double si_to_yottaampere(const long double a) noexcept { return a * 1e-24; }

    // Ronna- prefix (10^27)
    constexpr long double ronnaampere_to_si(const long double Ra) noexcept { return Ra * 1e27; }
    constexpr long double si_to_ronnaampere(const long double a) noexcept { return a * 1e-27; }

    // Quetta- prefix (10^30)
    constexpr long double quettaampere_to_si(const long double Qa) noexcept { return Qa * 1e30; }
    constexpr long double si_to_quettaampere(const long double a) noexcept { return a * 1e-30; }

    // Special units
    constexpr long double statampere_to_si(const long double sa) noexcept { return sa * fizmo::constants::esu_current(); }
    constexpr long double si_to_statampere(const long double a) noexcept { return a / fizmo::constants::esu_current(); }

    constexpr long double abampere_to_si(const long double aba) noexcept { return aba * 10.0; }
    constexpr long double si_to_abampere(const long double a) noexcept { return a * 0.1; }

    constexpr long double biot_to_si(const long double b) noexcept { return b * 10.0; }
    constexpr long double si_to_biot(const long double a) noexcept { return a * 0.1; }

    constexpr long double planck_ampere_to_si(const long double pa) noexcept { return pa * fizmo::constants::planck_current(); }
    constexpr long double si_to_planck_ampere(const long double a) noexcept { return a / fizmo::constants::planck_current(); }
}

namespace voltage {
    // Quecto- prefix (10^-30)
    constexpr long double quectovolt_to_si(const long double qv) noexcept { return qv * 1e-30; }
    constexpr long double si_to_quectovolt(const long double v) noexcept { return v * 1e30; }

    // Ronto- prefix (10^-27)
    constexpr long double rontovolt_to_si(const long double rv) noexcept { return rv * 1e-27; }
    constexpr long double si_to_rontovolt(const long double v) noexcept { return v * 1e27; }

    // Yocto- prefix (10^-24)
    constexpr long double yoctovolt_to_si(const long double yv) noexcept { return yv * 1e-24; }
    constexpr long double si_to_yoctovolt(const long double v) noexcept { return v * 1e24; }

    // Zepto- prefix (10^-21)
    constexpr long double zeptovolt_to_si(const long double zv) noexcept { return zv * 1e-21; }
    constexpr long double si_to_zeptovolt(const long double v) noexcept { return v * 1e21; }

    // Atto- prefix (10^-18)
    constexpr long double attovolt_to_si(const long double av) noexcept { return av * 1e-18; }
    constexpr long double si_to_attovolt(const long double v) noexcept { return v * 1e18; }

    // Femto- prefix (10^-15)
    constexpr long double femtovolt_to_si(const long double fv) noexcept { return fv * 1e-15; }
    constexpr long double si_to_femtovolt(const long double v) noexcept { return v * 1e15; }

    // Pico- prefix (10^-12)
    constexpr long double picovolt_to_si(const long double pv) noexcept { return pv * 1e-12; }
    constexpr long double si_to_picovolt(const long double v) noexcept { return v * 1e12; }

    // Nano- prefix (10^-9)
    constexpr long double nanovolt_to_si(const long double nv) noexcept { return nv * 1e-9; }
    constexpr long double si_to_nanovolt(const long double v) noexcept { return v * 1e9; }

    // Micro- prefix (10^-6)
    constexpr long double microvolt_to_si(const long double uv) noexcept { return uv * 1e-6; }
    constexpr long double si_to_microvolt(const long double v) noexcept { return v * 1e6; }

    // Milli- prefix (10^-3)
    constexpr long double millivolt_to_si(const long double mv) noexcept { return mv * 1e-3; }
    constexpr long double si_to_millivolt(const long double v) noexcept { return v * 1e3; }

    // Centi- prefix (10^-2)
    constexpr long double centivolt_to_si(const long double cv) noexcept { return cv * 1e-2; }
    constexpr long double si_to_centivolt(const long double v) noexcept { return v * 1e2; }

    // Deci- prefix (10^-1)
    constexpr long double decivolt_to_si(const long double dv) noexcept { return dv * 1e-1; }
    constexpr long double si_to_decivolt(const long double v) noexcept { return v * 1e1; }

    // Deca- prefix (10^1)
    constexpr long double decavolt_to_si(const long double dav) noexcept { return dav * 1e1; }
    constexpr long double si_to_decavolt(const long double v) noexcept { return v * 1e-1; }

    // Hecto- prefix (10^2)
    constexpr long double hectovolt_to_si(const long double hv) noexcept { return hv * 1e2; }
    constexpr long double si_to_hectovolt(const long double v) noexcept { return v * 1e-2; }

    // Kilo- prefix (10^3)
    constexpr long double kilovolt_to_si(const long double kv) noexcept { return kv * 1e3; }
    constexpr long double si_to_kilovolt(const long double v) noexcept { return v * 1e-3; }

    // Mega- prefix (10^6)
    constexpr long double megavolt_to_si(const long double Mv) noexcept { return Mv * 1e6; }
    constexpr long double si_to_megavolt(const long double v) noexcept { return v * 1e-6; }

    // Giga- prefix (10^9)
    constexpr long double gigavolt_to_si(const long double Gv) noexcept { return Gv * 1e9; }
    constexpr long double si_to_gigavolt(const long double v) noexcept { return v * 1e-9; }

    // Tera- prefix (10^12)
    constexpr long double teravolt_to_si(const long double Tv) noexcept { return Tv * 1e12; }
    constexpr long double si_to_teravolt(const long double v) noexcept { return v * 1e-12; }

    // Peta- prefix (10^15)
    constexpr long double petavolt_to_si(const long double Pv) noexcept { return Pv * 1e15; }
    constexpr long double si_to_petavolt(const long double v) noexcept { return v * 1e-15; }

    // Exa- prefix (10^18)
    constexpr long double exavolt_to_si(const long double Ev) noexcept { return Ev * 1e18; }
    constexpr long double si_to_exavolt(const long double v) noexcept { return v * 1e-18; }

    // Zetta- prefix (10^21)
    constexpr long double zettavolt_to_si(const long double Zv) noexcept { return Zv * 1e21; }
    constexpr long double si_to_zettavolt(const long double v) noexcept { return v * 1e-21; }

    // Yotta- prefix (10^24)
    constexpr long double yottavolt_to_si(const long double Yv) noexcept { return Yv * 1e24; }
    constexpr long double si_to_yottavolt(const long double v) noexcept { return v * 1e-24; }

    // Ronna- prefix (10^27)
    constexpr long double ronnavolt_to_si(const long double Rv) noexcept { return Rv * 1e27; }
    constexpr long double si_to_ronnavolt(const long double v) noexcept { return v * 1e-27; }

    // Quetta- prefix (10^30)
    constexpr long double quettavolt_to_si(const long double Qv) noexcept { return Qv * 1e30; }
    constexpr long double si_to_quettavolt(const long double v) noexcept { return v * 1e-30; }

    // Special units
    constexpr long double statvolt_to_si(const long double sv) noexcept { return sv / fizmo::constants::esu_voltage(); }
    constexpr long double si_to_statvolt(const long double v) noexcept { return v * fizmo::constants::esu_voltage(); }

    constexpr long double abvolt_to_si(const long double abv) noexcept { return abv * 1e-8; }
    constexpr long double si_to_abvolt(const long double v) noexcept { return v * 1e8; }

    constexpr long double planck_volt_to_si(const long double pv) noexcept { return pv * fizmo::constants::planck_voltage(); }
    constexpr long double si_to_planck_volt(const long double v) noexcept { return v / fizmo::constants::planck_voltage(); }
}

namespace resistance {
    // Quecto- prefix (10^-30)
    constexpr long double quectoohm_to_si(const long double qohm) noexcept { return qohm * 1e-30; }
    constexpr long double si_to_quectoohm(const long double ohm) noexcept { return ohm * 1e30; }

    // Ronto- prefix (10^-27)
    constexpr long double rontoohm_to_si(const long double rohm) noexcept { return rohm * 1e-27; }
    constexpr long double si_to_rontoohm(const long double ohm) noexcept { return ohm * 1e27; }

    // Yocto- prefix (10^-24)
    constexpr long double yoctoohm_to_si(const long double yohm) noexcept { return yohm * 1e-24; }
    constexpr long double si_to_yoctoohm(const long double ohm) noexcept { return ohm * 1e24; }

    // Zepto- prefix (10^-21)
    constexpr long double zeptoohm_to_si(const long double zohm) noexcept { return zohm * 1e-21; }
    constexpr long double si_to_zeptoohm(const long double ohm) noexcept { return ohm * 1e21; }

    // Atto- prefix (10^-18)
    constexpr long double attoohm_to_si(const long double aohm) noexcept { return aohm * 1e-18; }
    constexpr long double si_to_attoohm(const long double ohm) noexcept { return ohm * 1e18; }

    // Femto- prefix (10^-15)
    constexpr long double femtoohm_to_si(const long double fohm) noexcept { return fohm * 1e-15; }
    constexpr long double si_to_femtoohm(const long double ohm) noexcept { return ohm * 1e15; }

    // Pico- prefix (10^-12)
    constexpr long double picoohm_to_si(const long double pohm) noexcept { return pohm * 1e-12; }
    constexpr long double si_to_picoohm(const long double ohm) noexcept { return ohm * 1e12; }

    // Nano- prefix (10^-9)
    constexpr long double nanoohm_to_si(const long double nohm) noexcept { return nohm * 1e-9; }
    constexpr long double si_to_nanoohm(const long double ohm) noexcept { return ohm * 1e9; }

    // Micro- prefix (10^-6)
    constexpr long double microohm_to_si(const long double uohm) noexcept { return uohm * 1e-6; }
    constexpr long double si_to_microohm(const long double ohm) noexcept { return ohm * 1e6; }

    // Milli- prefix (10^-3)
    constexpr long double milliohm_to_si(const long double mohm) noexcept { return mohm * 1e-3; }
    constexpr long double si_to_milliohm(const long double ohm) noexcept { return ohm * 1e3; }

    // Centi- prefix (10^-2)
    constexpr long double centiohm_to_si(const long double cohm) noexcept { return cohm * 1e-2; }
    constexpr long double si_to_centiohm(const long double ohm) noexcept { return ohm * 1e2; }

    // Deci- prefix (10^-1)
    constexpr long double deciohm_to_si(const long double dohm) noexcept { return dohm * 1e-1; }
    constexpr long double si_to_deciohm(const long double ohm) noexcept { return ohm * 1e1; }

    // Deca- prefix (10^1)
    constexpr long double decaohm_to_si(const long double daohm) noexcept { return daohm * 1e1; }
    constexpr long double si_to_decaohm(const long double ohm) noexcept { return ohm * 1e-1; }

    // Hecto- prefix (10^2)
    constexpr long double hectoohm_to_si(const long double hohm) noexcept { return hohm * 1e2; }
    constexpr long double si_to_hectoohm(const long double ohm) noexcept { return ohm * 1e-2; }

    // Kilo- prefix (10^3)
    constexpr long double kiloohm_to_si(const long double kohm) noexcept { return kohm * 1e3; }
    constexpr long double si_to_kiloohm(const long double ohm) noexcept { return ohm * 1e-3; }

    // Mega- prefix (10^6)
    constexpr long double megaohm_to_si(const long double Mohm) noexcept { return Mohm * 1e6; }
    constexpr long double si_to_megaohm(const long double ohm) noexcept { return ohm * 1e-6; }

    // Giga- prefix (10^9)
    constexpr long double gigaohm_to_si(const long double Gohm) noexcept { return Gohm * 1e9; }
    constexpr long double si_to_gigaohm(const long double ohm) noexcept { return ohm * 1e-9; }

    // Tera- prefix (10^12)
    constexpr long double teraohm_to_si(const long double Tohm) noexcept { return Tohm * 1e12; }
    constexpr long double si_to_teraohm(const long double ohm) noexcept { return ohm * 1e-12; }

    // Peta- prefix (10^15)
    constexpr long double petaohm_to_si(const long double Pohm) noexcept { return Pohm * 1e15; }
    constexpr long double si_to_petaohm(const long double ohm) noexcept { return ohm * 1e-15; }

    // Exa- prefix (10^18)
    constexpr long double exaohm_to_si(const long double Eohm) noexcept { return Eohm * 1e18; }
    constexpr long double si_to_exaohm(const long double ohm) noexcept { return ohm * 1e-18; }

    // Zetta- prefix (10^21)
    constexpr long double zettaohm_to_si(const long double Zohm) noexcept { return Zohm * 1e21; }
    constexpr long double si_to_zettaohm(const long double ohm) noexcept { return ohm * 1e-21; }

    // Yotta- prefix (10^24)
    constexpr long double yottaohm_to_si(const long double Yohm) noexcept { return Yohm * 1e24; }
    constexpr long double si_to_yottaohm(const long double ohm) noexcept { return ohm * 1e-24; }

    // Ronna- prefix (10^27)
    constexpr long double ronnaohm_to_si(const long double Rohm) noexcept { return Rohm * 1e27; }
    constexpr long double si_to_ronnaohm(const long double ohm) noexcept { return ohm * 1e-27; }

    // Quetta- prefix (10^30)
    constexpr long double quettaohm_to_si(const long double Qohm) noexcept { return Qohm * 1e30; }
    constexpr long double si_to_quettaohm(const long double ohm) noexcept { return ohm * 1e-30; }

    // Special units
    constexpr long double statohm_to_si(const long double sohm) noexcept { return sohm / fizmo::constants::esu_resistance(); }
    constexpr long double si_to_statohm(const long double ohm) noexcept { return ohm * fizmo::constants::esu_resistance(); }

    constexpr long double abohm_to_si(const long double abohm) noexcept { return abohm * 1e-9; }
    constexpr long double si_to_abohm(const long double ohm) noexcept { return ohm * 1e9; }

    constexpr long double planck_ohm_to_si(const long double po) noexcept { return po * fizmo::constants::planck_impedance(); }
    constexpr long double si_to_planck_ohm(const long double ohm) noexcept { return ohm / fizmo::constants::planck_impedance(); }
}

} // namespace electrical
} // namespace converters
} // namespace fizmo

#endif // ELECTRICAL_CONVERSIONS_HPP