#ifndef UNIT_CONVERTER_FUNCTIONS_HPP
#define UNIT_CONVERTER_FUNCTIONS_HPP

#include "unit_management.hpp"

namespace fizmo {
namespace units {

constexpr long double convert_electrical_charge(const long double value, const ElectricalChargeUnit from, const ElectricalChargeUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case ElectricalChargeUnit::COULOMB:           si_value = value; break;
        case ElectricalChargeUnit::QUECTOCOULOMB:     si_value = fizmo::converters::electrical::charge::quectocoulomb_to_si(value); break;
        case ElectricalChargeUnit::RONTOCOULOMB:      si_value = fizmo::converters::electrical::charge::rontocoulomb_to_si(value); break;
        case ElectricalChargeUnit::YOCTOCOULOMB:      si_value = fizmo::converters::electrical::charge::yoctocoulomb_to_si(value); break;
        case ElectricalChargeUnit::ZEPTOCOULOMB:      si_value = fizmo::converters::electrical::charge::zeptocoulomb_to_si(value); break;
        case ElectricalChargeUnit::ATTOCOULOMB:       si_value = fizmo::converters::electrical::charge::attocoulomb_to_si(value); break;
        case ElectricalChargeUnit::FEMTOCOULOMB:      si_value = fizmo::converters::electrical::charge::femtocoulomb_to_si(value); break;
        case ElectricalChargeUnit::PICOCOULOMB:       si_value = fizmo::converters::electrical::charge::picocoulomb_to_si(value); break;
        case ElectricalChargeUnit::NANOCOULOMB:       si_value = fizmo::converters::electrical::charge::nanocoulomb_to_si(value); break;
        case ElectricalChargeUnit::MICROCOULOMB:      si_value = fizmo::converters::electrical::charge::microcoulomb_to_si(value); break;
        case ElectricalChargeUnit::MILLICOULOMB:      si_value = fizmo::converters::electrical::charge::millicoulomb_to_si(value); break;
        case ElectricalChargeUnit::CENTICOULOMB:      si_value = fizmo::converters::electrical::charge::centicoulomb_to_si(value); break;
        case ElectricalChargeUnit::DECICOULOMB:       si_value = fizmo::converters::electrical::charge::decicoulomb_to_si(value); break;
        case ElectricalChargeUnit::DECACOULOMB:       si_value = fizmo::converters::electrical::charge::decacoulomb_to_si(value); break;
        case ElectricalChargeUnit::HECTOCOULOMB:      si_value = fizmo::converters::electrical::charge::hectocoulomb_to_si(value); break;
        case ElectricalChargeUnit::KILOCOULOMB:       si_value = fizmo::converters::electrical::charge::kilocoulomb_to_si(value); break;
        case ElectricalChargeUnit::MEGACOULOMB:       si_value = fizmo::converters::electrical::charge::megacoulomb_to_si(value); break;
        case ElectricalChargeUnit::GIGACOULOMB:       si_value = fizmo::converters::electrical::charge::gigacoulomb_to_si(value); break;
        case ElectricalChargeUnit::TERACOULOMB:       si_value = fizmo::converters::electrical::charge::teracoulomb_to_si(value); break;
        case ElectricalChargeUnit::PETACOULOMB:       si_value = fizmo::converters::electrical::charge::petacoulomb_to_si(value); break;
        case ElectricalChargeUnit::EXACOULOMB:        si_value = fizmo::converters::electrical::charge::exacoulomb_to_si(value); break;
        case ElectricalChargeUnit::ZETTACOULOMB:      si_value = fizmo::converters::electrical::charge::zettacoulomb_to_si(value); break;
        case ElectricalChargeUnit::YOTTACOULOMB:      si_value = fizmo::converters::electrical::charge::yottacoulomb_to_si(value); break;
        case ElectricalChargeUnit::RONNACOULOMB:      si_value = fizmo::converters::electrical::charge::ronnacoulomb_to_si(value); break;
        case ElectricalChargeUnit::QUETTACOULOMB:     si_value = fizmo::converters::electrical::charge::quettacoulomb_to_si(value); break;
        case ElectricalChargeUnit::AMPERE_HOUR:       si_value = fizmo::converters::electrical::charge::ampere_hour_to_si(value); break;
        case ElectricalChargeUnit::MILLIAMPERE_HOUR:  si_value = fizmo::converters::electrical::charge::milliampere_hour_to_si(value); break;
        case ElectricalChargeUnit::FARADAY:           si_value = fizmo::converters::electrical::charge::faraday_to_si(value); break;
        case ElectricalChargeUnit::ELEMENTARY_CHARGE: si_value = fizmo::converters::electrical::charge::elementary_charge_to_si(value); break;
        case ElectricalChargeUnit::STATCOULOMB:       si_value = fizmo::converters::electrical::charge::statcoulomb_to_si(value); break;
        case ElectricalChargeUnit::ABCOULOMB:         si_value = fizmo::converters::electrical::charge::abcoulomb_to_si(value); break;
        case ElectricalChargeUnit::PLANCK_COULOMB:    si_value = fizmo::converters::electrical::charge::planck_coulomb_to_si(value); break;
    }

    switch (to) {
        case ElectricalChargeUnit::COULOMB:           return si_value;
        case ElectricalChargeUnit::QUECTOCOULOMB:     return fizmo::converters::electrical::charge::si_to_quectocoulomb(si_value);
        case ElectricalChargeUnit::RONTOCOULOMB:      return fizmo::converters::electrical::charge::si_to_rontocoulomb(si_value);
        case ElectricalChargeUnit::YOCTOCOULOMB:      return fizmo::converters::electrical::charge::si_to_yoctocoulomb(si_value);
        case ElectricalChargeUnit::ZEPTOCOULOMB:      return fizmo::converters::electrical::charge::si_to_zeptocoulomb(si_value);
        case ElectricalChargeUnit::ATTOCOULOMB:       return fizmo::converters::electrical::charge::si_to_attocoulomb(si_value);
        case ElectricalChargeUnit::FEMTOCOULOMB:      return fizmo::converters::electrical::charge::si_to_femtocoulomb(si_value);
        case ElectricalChargeUnit::PICOCOULOMB:       return fizmo::converters::electrical::charge::si_to_picocoulomb(si_value);
        case ElectricalChargeUnit::NANOCOULOMB:       return fizmo::converters::electrical::charge::si_to_nanocoulomb(si_value);
        case ElectricalChargeUnit::MICROCOULOMB:      return fizmo::converters::electrical::charge::si_to_microcoulomb(si_value);
        case ElectricalChargeUnit::MILLICOULOMB:      return fizmo::converters::electrical::charge::si_to_millicoulomb(si_value);
        case ElectricalChargeUnit::CENTICOULOMB:      return fizmo::converters::electrical::charge::si_to_centicoulomb(si_value);
        case ElectricalChargeUnit::DECICOULOMB:       return fizmo::converters::electrical::charge::si_to_decicoulomb(si_value);
        case ElectricalChargeUnit::DECACOULOMB:       return fizmo::converters::electrical::charge::si_to_decacoulomb(si_value);
        case ElectricalChargeUnit::HECTOCOULOMB:      return fizmo::converters::electrical::charge::si_to_hectocoulomb(si_value);
        case ElectricalChargeUnit::KILOCOULOMB:       return fizmo::converters::electrical::charge::si_to_kilocoulomb(si_value);
        case ElectricalChargeUnit::MEGACOULOMB:       return fizmo::converters::electrical::charge::si_to_megacoulomb(si_value);
        case ElectricalChargeUnit::GIGACOULOMB:       return fizmo::converters::electrical::charge::si_to_gigacoulomb(si_value);
        case ElectricalChargeUnit::TERACOULOMB:       return fizmo::converters::electrical::charge::si_to_teracoulomb(si_value);
        case ElectricalChargeUnit::PETACOULOMB:       return fizmo::converters::electrical::charge::si_to_petacoulomb(si_value);
        case ElectricalChargeUnit::EXACOULOMB:        return fizmo::converters::electrical::charge::si_to_exacoulomb(si_value);
        case ElectricalChargeUnit::ZETTACOULOMB:      return fizmo::converters::electrical::charge::si_to_zettacoulomb(si_value);
        case ElectricalChargeUnit::YOTTACOULOMB:      return fizmo::converters::electrical::charge::si_to_yottacoulomb(si_value);
        case ElectricalChargeUnit::RONNACOULOMB:      return fizmo::converters::electrical::charge::si_to_ronnacoulomb(si_value);
        case ElectricalChargeUnit::QUETTACOULOMB:     return fizmo::converters::electrical::charge::si_to_quettacoulomb(si_value);
        case ElectricalChargeUnit::AMPERE_HOUR:       return fizmo::converters::electrical::charge::si_to_ampere_hour(si_value);
        case ElectricalChargeUnit::MILLIAMPERE_HOUR:  return fizmo::converters::electrical::charge::si_to_milliampere_hour(si_value);
        case ElectricalChargeUnit::FARADAY:           return fizmo::converters::electrical::charge::si_to_faraday(si_value);
        case ElectricalChargeUnit::ELEMENTARY_CHARGE: return fizmo::converters::electrical::charge::si_to_elementary_charge(si_value);
        case ElectricalChargeUnit::STATCOULOMB:       return fizmo::converters::electrical::charge::si_to_statcoulomb(si_value);
        case ElectricalChargeUnit::ABCOULOMB:         return fizmo::converters::electrical::charge::si_to_abcoulomb(si_value);
        case ElectricalChargeUnit::PLANCK_COULOMB:    return fizmo::converters::electrical::charge::si_to_planck_coulomb(si_value);
    }
    return value; 
}

constexpr long double convert_current(const long double value, const ElectricalCurrentUnit from, const ElectricalCurrentUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case ElectricalCurrentUnit::AMPERE:        si_value = value; break;
        case ElectricalCurrentUnit::QUECTOAMPERE:  si_value = fizmo::converters::electrical::current::quectoampere_to_si(value); break;
        case ElectricalCurrentUnit::RONTOAMPERE:   si_value = fizmo::converters::electrical::current::rontoampere_to_si(value); break;
        case ElectricalCurrentUnit::YOCTOAMPERE:   si_value = fizmo::converters::electrical::current::yoctoampere_to_si(value); break;
        case ElectricalCurrentUnit::ZEPTOAMPERE:   si_value = fizmo::converters::electrical::current::zeptoampere_to_si(value); break;
        case ElectricalCurrentUnit::ATTOAMPERE:    si_value = fizmo::converters::electrical::current::attoampere_to_si(value); break;
        case ElectricalCurrentUnit::FEMTOAMPERE:   si_value = fizmo::converters::electrical::current::femtoampere_to_si(value); break;
        case ElectricalCurrentUnit::PICOAMPERE:    si_value = fizmo::converters::electrical::current::picoampere_to_si(value); break;
        case ElectricalCurrentUnit::NANOAMPERE:    si_value = fizmo::converters::electrical::current::nanoampere_to_si(value); break;
        case ElectricalCurrentUnit::MICROAMPERE:   si_value = fizmo::converters::electrical::current::microampere_to_si(value); break;
        case ElectricalCurrentUnit::MILLIAMPERE:   si_value = fizmo::converters::electrical::current::milliampere_to_si(value); break;
        case ElectricalCurrentUnit::CENTIAMPERE:   si_value = fizmo::converters::electrical::current::centiampere_to_si(value); break;
        case ElectricalCurrentUnit::DECIAMPERE:    si_value = fizmo::converters::electrical::current::deciampere_to_si(value); break;
        case ElectricalCurrentUnit::DECAAMPERE:    si_value = fizmo::converters::electrical::current::decaampere_to_si(value); break;
        case ElectricalCurrentUnit::HECTOAMPERE:   si_value = fizmo::converters::electrical::current::hectoampere_to_si(value); break;
        case ElectricalCurrentUnit::KILOAMPERE:    si_value = fizmo::converters::electrical::current::kiloampere_to_si(value); break;
        case ElectricalCurrentUnit::MEGAAMPERE:    si_value = fizmo::converters::electrical::current::megaampere_to_si(value); break;
        case ElectricalCurrentUnit::GIGAAMPERE:    si_value = fizmo::converters::electrical::current::gigaampere_to_si(value); break;
        case ElectricalCurrentUnit::TERAAMPERE:    si_value = fizmo::converters::electrical::current::teraampere_to_si(value); break;
        case ElectricalCurrentUnit::PETAAMPERE:    si_value = fizmo::converters::electrical::current::petaampere_to_si(value); break;
        case ElectricalCurrentUnit::EXAAMPERE:     si_value = fizmo::converters::electrical::current::exaampere_to_si(value); break;
        case ElectricalCurrentUnit::ZETTAAMPERE:   si_value = fizmo::converters::electrical::current::zettaampere_to_si(value); break;
        case ElectricalCurrentUnit::YOTTAAMPERE:   si_value = fizmo::converters::electrical::current::yottaampere_to_si(value); break;
        case ElectricalCurrentUnit::RONNAAMPERE:   si_value = fizmo::converters::electrical::current::ronnaampere_to_si(value); break;
        case ElectricalCurrentUnit::QUETTAAMPERE:  si_value = fizmo::converters::electrical::current::quettaampere_to_si(value); break;
        case ElectricalCurrentUnit::STATAMPERE:    si_value = fizmo::converters::electrical::current::statampere_to_si(value); break;
        case ElectricalCurrentUnit::ABAMPERE:      si_value = fizmo::converters::electrical::current::abampere_to_si(value); break;
        case ElectricalCurrentUnit::BIOT:          si_value = fizmo::converters::electrical::current::biot_to_si(value); break;
        case ElectricalCurrentUnit::PLANCK_AMPERE: si_value = fizmo::converters::electrical::current::planck_ampere_to_si(value); break;
    }

    switch (to) {
        case ElectricalCurrentUnit::AMPERE:        return si_value;
        case ElectricalCurrentUnit::QUECTOAMPERE:  return fizmo::converters::electrical::current::si_to_quectoampere(si_value);
        case ElectricalCurrentUnit::RONTOAMPERE:   return fizmo::converters::electrical::current::si_to_rontoampere(si_value);
        case ElectricalCurrentUnit::YOCTOAMPERE:   return fizmo::converters::electrical::current::si_to_yoctoampere(si_value);
        case ElectricalCurrentUnit::ZEPTOAMPERE:   return fizmo::converters::electrical::current::si_to_zeptoampere(si_value);
        case ElectricalCurrentUnit::ATTOAMPERE:    return fizmo::converters::electrical::current::si_to_attoampere(si_value);
        case ElectricalCurrentUnit::FEMTOAMPERE:   return fizmo::converters::electrical::current::si_to_femtoampere(si_value);
        case ElectricalCurrentUnit::PICOAMPERE:    return fizmo::converters::electrical::current::si_to_picoampere(si_value);
        case ElectricalCurrentUnit::NANOAMPERE:    return fizmo::converters::electrical::current::si_to_nanoampere(si_value);
        case ElectricalCurrentUnit::MICROAMPERE:   return fizmo::converters::electrical::current::si_to_microampere(si_value);
        case ElectricalCurrentUnit::MILLIAMPERE:   return fizmo::converters::electrical::current::si_to_milliampere(si_value);
        case ElectricalCurrentUnit::CENTIAMPERE:   return fizmo::converters::electrical::current::si_to_centiampere(si_value);
        case ElectricalCurrentUnit::DECIAMPERE:    return fizmo::converters::electrical::current::si_to_deciampere(si_value);
        case ElectricalCurrentUnit::DECAAMPERE:    return fizmo::converters::electrical::current::si_to_decaampere(si_value);
        case ElectricalCurrentUnit::HECTOAMPERE:   return fizmo::converters::electrical::current::si_to_hectoampere(si_value);
        case ElectricalCurrentUnit::KILOAMPERE:    return fizmo::converters::electrical::current::si_to_kiloampere(si_value);
        case ElectricalCurrentUnit::MEGAAMPERE:    return fizmo::converters::electrical::current::si_to_megaampere(si_value);
        case ElectricalCurrentUnit::GIGAAMPERE:    return fizmo::converters::electrical::current::si_to_gigaampere(si_value);
        case ElectricalCurrentUnit::TERAAMPERE:    return fizmo::converters::electrical::current::si_to_teraampere(si_value);
        case ElectricalCurrentUnit::PETAAMPERE:    return fizmo::converters::electrical::current::si_to_petaampere(si_value);
        case ElectricalCurrentUnit::EXAAMPERE:     return fizmo::converters::electrical::current::si_to_exaampere(si_value);
        case ElectricalCurrentUnit::ZETTAAMPERE:   return fizmo::converters::electrical::current::si_to_zettaampere(si_value);
        case ElectricalCurrentUnit::YOTTAAMPERE:   return fizmo::converters::electrical::current::si_to_yottaampere(si_value);
        case ElectricalCurrentUnit::RONNAAMPERE:   return fizmo::converters::electrical::current::si_to_ronnaampere(si_value);
        case ElectricalCurrentUnit::QUETTAAMPERE:  return fizmo::converters::electrical::current::si_to_quettaampere(si_value);
        case ElectricalCurrentUnit::STATAMPERE:    return fizmo::converters::electrical::current::si_to_statampere(si_value);
        case ElectricalCurrentUnit::ABAMPERE:      return fizmo::converters::electrical::current::si_to_abampere(si_value);
        case ElectricalCurrentUnit::BIOT:          return fizmo::converters::electrical::current::si_to_biot(si_value);
        case ElectricalCurrentUnit::PLANCK_AMPERE: return fizmo::converters::electrical::current::si_to_planck_ampere(si_value);
    }
    return value;
}

constexpr long double convert_voltage(const long double value, const VoltageUnit from, const VoltageUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case VoltageUnit::VOLT:        si_value = value; break;
        case VoltageUnit::QUECTOVOLT:  si_value = fizmo::converters::electrical::voltage::quectovolt_to_si(value); break;
        case VoltageUnit::RONTOVOLT:   si_value = fizmo::converters::electrical::voltage::rontovolt_to_si(value); break;
        case VoltageUnit::YOCTOVOLT:   si_value = fizmo::converters::electrical::voltage::yoctovolt_to_si(value); break;
        case VoltageUnit::ZEPTOVOLT:   si_value = fizmo::converters::electrical::voltage::zeptovolt_to_si(value); break;
        case VoltageUnit::ATTOVOLT:    si_value = fizmo::converters::electrical::voltage::attovolt_to_si(value); break;
        case VoltageUnit::FEMTOVOLT:   si_value = fizmo::converters::electrical::voltage::femtovolt_to_si(value); break;
        case VoltageUnit::PICOVOLT:    si_value = fizmo::converters::electrical::voltage::picovolt_to_si(value); break;
        case VoltageUnit::NANOVOLT:    si_value = fizmo::converters::electrical::voltage::nanovolt_to_si(value); break;
        case VoltageUnit::MICROVOLT:   si_value = fizmo::converters::electrical::voltage::microvolt_to_si(value); break;
        case VoltageUnit::MILLIVOLT:   si_value = fizmo::converters::electrical::voltage::millivolt_to_si(value); break;
        case VoltageUnit::CENTIVOLT:   si_value = fizmo::converters::electrical::voltage::centivolt_to_si(value); break;
        case VoltageUnit::DECIVOLT:    si_value = fizmo::converters::electrical::voltage::decivolt_to_si(value); break;
        case VoltageUnit::DECAVOLT:    si_value = fizmo::converters::electrical::voltage::decavolt_to_si(value); break;
        case VoltageUnit::HECTOVOLT:   si_value = fizmo::converters::electrical::voltage::hectovolt_to_si(value); break;
        case VoltageUnit::KILOVOLT:    si_value = fizmo::converters::electrical::voltage::kilovolt_to_si(value); break;
        case VoltageUnit::MEGAVOLT:    si_value = fizmo::converters::electrical::voltage::megavolt_to_si(value); break;
        case VoltageUnit::GIGAVOLT:    si_value = fizmo::converters::electrical::voltage::gigavolt_to_si(value); break;
        case VoltageUnit::TERAVOLT:    si_value = fizmo::converters::electrical::voltage::teravolt_to_si(value); break;
        case VoltageUnit::PETAVOLT:    si_value = fizmo::converters::electrical::voltage::petavolt_to_si(value); break;
        case VoltageUnit::EXAVOLT:     si_value = fizmo::converters::electrical::voltage::exavolt_to_si(value); break;
        case VoltageUnit::ZETTAVOLT:   si_value = fizmo::converters::electrical::voltage::zettavolt_to_si(value); break;
        case VoltageUnit::YOTTAVOLT:   si_value = fizmo::converters::electrical::voltage::yottavolt_to_si(value); break;
        case VoltageUnit::RONNAVOLT:   si_value = fizmo::converters::electrical::voltage::ronnavolt_to_si(value); break;
        case VoltageUnit::QUETTAVOLT:  si_value = fizmo::converters::electrical::voltage::quettavolt_to_si(value); break;
        case VoltageUnit::STATVOLT:    si_value = fizmo::converters::electrical::voltage::statvolt_to_si(value); break;
        case VoltageUnit::ABVOLT:      si_value = fizmo::converters::electrical::voltage::abvolt_to_si(value); break;
        case VoltageUnit::PLANCK_VOLT: si_value = fizmo::converters::electrical::voltage::planck_volt_to_si(value); break;
    }

    switch (to) {
        case VoltageUnit::VOLT:        return si_value;
        case VoltageUnit::QUECTOVOLT:  return fizmo::converters::electrical::voltage::si_to_quectovolt(si_value);
        case VoltageUnit::RONTOVOLT:   return fizmo::converters::electrical::voltage::si_to_rontovolt(si_value);
        case VoltageUnit::YOCTOVOLT:   return fizmo::converters::electrical::voltage::si_to_yoctovolt(si_value);
        case VoltageUnit::ZEPTOVOLT:   return fizmo::converters::electrical::voltage::si_to_zeptovolt(si_value);
        case VoltageUnit::ATTOVOLT:    return fizmo::converters::electrical::voltage::si_to_attovolt(si_value);
        case VoltageUnit::FEMTOVOLT:   return fizmo::converters::electrical::voltage::si_to_femtovolt(si_value);
        case VoltageUnit::PICOVOLT:    return fizmo::converters::electrical::voltage::si_to_picovolt(si_value);
        case VoltageUnit::NANOVOLT:    return fizmo::converters::electrical::voltage::si_to_nanovolt(si_value);
        case VoltageUnit::MICROVOLT:   return fizmo::converters::electrical::voltage::si_to_microvolt(si_value);
        case VoltageUnit::MILLIVOLT:   return fizmo::converters::electrical::voltage::si_to_millivolt(si_value);
        case VoltageUnit::CENTIVOLT:   return fizmo::converters::electrical::voltage::si_to_centivolt(si_value);
        case VoltageUnit::DECIVOLT:    return fizmo::converters::electrical::voltage::si_to_decivolt(si_value);
        case VoltageUnit::DECAVOLT:    return fizmo::converters::electrical::voltage::si_to_decavolt(si_value);
        case VoltageUnit::HECTOVOLT:   return fizmo::converters::electrical::voltage::si_to_hectovolt(si_value);
        case VoltageUnit::KILOVOLT:    return fizmo::converters::electrical::voltage::si_to_kilovolt(si_value);
        case VoltageUnit::MEGAVOLT:    return fizmo::converters::electrical::voltage::si_to_megavolt(si_value);
        case VoltageUnit::GIGAVOLT:    return fizmo::converters::electrical::voltage::si_to_gigavolt(si_value);
        case VoltageUnit::TERAVOLT:    return fizmo::converters::electrical::voltage::si_to_teravolt(si_value);
        case VoltageUnit::PETAVOLT:    return fizmo::converters::electrical::voltage::si_to_petavolt(si_value);
        case VoltageUnit::EXAVOLT:     return fizmo::converters::electrical::voltage::si_to_exavolt(si_value);
        case VoltageUnit::ZETTAVOLT:   return fizmo::converters::electrical::voltage::si_to_zettavolt(si_value);
        case VoltageUnit::YOTTAVOLT:   return fizmo::converters::electrical::voltage::si_to_yottavolt(si_value);
        case VoltageUnit::RONNAVOLT:   return fizmo::converters::electrical::voltage::si_to_ronnavolt(si_value);
        case VoltageUnit::QUETTAVOLT:  return fizmo::converters::electrical::voltage::si_to_quettavolt(si_value);
        case VoltageUnit::STATVOLT:    return fizmo::converters::electrical::voltage::si_to_statvolt(si_value);
        case VoltageUnit::ABVOLT:      return fizmo::converters::electrical::voltage::si_to_abvolt(si_value);
        case VoltageUnit::PLANCK_VOLT: return fizmo::converters::electrical::voltage::si_to_planck_volt(si_value);
    }
    return value;
}

constexpr long double convert_resistance(const long double value, const ResistanceUnit from, const ResistanceUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case ResistanceUnit::OHM:        si_value = value; break;
        case ResistanceUnit::QUECTOOHM:  si_value = fizmo::converters::electrical::resistance::quectoohm_to_si(value); break;
        case ResistanceUnit::RONTOOHM:   si_value = fizmo::converters::electrical::resistance::rontoohm_to_si(value); break;
        case ResistanceUnit::YOCTOOHM:   si_value = fizmo::converters::electrical::resistance::yoctoohm_to_si(value); break;
        case ResistanceUnit::ZEPTOOHM:   si_value = fizmo::converters::electrical::resistance::zeptoohm_to_si(value); break;
        case ResistanceUnit::ATTOOHM:    si_value = fizmo::converters::electrical::resistance::attoohm_to_si(value); break;
        case ResistanceUnit::FEMTOOHM:   si_value = fizmo::converters::electrical::resistance::femtoohm_to_si(value); break;
        case ResistanceUnit::PICOOHM:    si_value = fizmo::converters::electrical::resistance::picoohm_to_si(value); break;
        case ResistanceUnit::NANOOHM:    si_value = fizmo::converters::electrical::resistance::nanoohm_to_si(value); break;
        case ResistanceUnit::MICROOHM:   si_value = fizmo::converters::electrical::resistance::microohm_to_si(value); break;
        case ResistanceUnit::MILLIOHM:   si_value = fizmo::converters::electrical::resistance::milliohm_to_si(value); break;
        case ResistanceUnit::CENTIOHM:   si_value = fizmo::converters::electrical::resistance::centiohm_to_si(value); break;
        case ResistanceUnit::DECIOHM:    si_value = fizmo::converters::electrical::resistance::deciohm_to_si(value); break;
        case ResistanceUnit::DECAOHM:    si_value = fizmo::converters::electrical::resistance::decaohm_to_si(value); break;
        case ResistanceUnit::HECTOOHM:   si_value = fizmo::converters::electrical::resistance::hectoohm_to_si(value); break;
        case ResistanceUnit::KILOOHM:    si_value = fizmo::converters::electrical::resistance::kiloohm_to_si(value); break;
        case ResistanceUnit::MEGAOHM:    si_value = fizmo::converters::electrical::resistance::megaohm_to_si(value); break;
        case ResistanceUnit::GIGAOHM:    si_value = fizmo::converters::electrical::resistance::gigaohm_to_si(value); break;
        case ResistanceUnit::TERAOHM:    si_value = fizmo::converters::electrical::resistance::teraohm_to_si(value); break;
        case ResistanceUnit::PETAOHM:    si_value = fizmo::converters::electrical::resistance::petaohm_to_si(value); break;
        case ResistanceUnit::EXAOHM:     si_value = fizmo::converters::electrical::resistance::exaohm_to_si(value); break;
        case ResistanceUnit::ZETTAOHM:   si_value = fizmo::converters::electrical::resistance::zettaohm_to_si(value); break;
        case ResistanceUnit::YOTTAOHM:   si_value = fizmo::converters::electrical::resistance::yottaohm_to_si(value); break;
        case ResistanceUnit::RONNAOHM:   si_value = fizmo::converters::electrical::resistance::ronnaohm_to_si(value); break;
        case ResistanceUnit::QUETTAOHM:  si_value = fizmo::converters::electrical::resistance::quettaohm_to_si(value); break;
        case ResistanceUnit::STATOHM:    si_value = fizmo::converters::electrical::resistance::statohm_to_si(value); break;
        case ResistanceUnit::ABOHM:      si_value = fizmo::converters::electrical::resistance::abohm_to_si(value); break;
        case ResistanceUnit::PLANCK_OHM: si_value = fizmo::converters::electrical::resistance::planck_ohm_to_si(value); break;
    }

    switch (to) {
        case ResistanceUnit::OHM:        return si_value;
        case ResistanceUnit::QUECTOOHM:  return fizmo::converters::electrical::resistance::si_to_quectoohm(si_value);
        case ResistanceUnit::RONTOOHM:   return fizmo::converters::electrical::resistance::si_to_rontoohm(si_value);
        case ResistanceUnit::YOCTOOHM:   return fizmo::converters::electrical::resistance::si_to_yoctoohm(si_value);
        case ResistanceUnit::ZEPTOOHM:   return fizmo::converters::electrical::resistance::si_to_zeptoohm(si_value);
        case ResistanceUnit::ATTOOHM:    return fizmo::converters::electrical::resistance::si_to_attoohm(si_value);
        case ResistanceUnit::FEMTOOHM:   return fizmo::converters::electrical::resistance::si_to_femtoohm(si_value);
        case ResistanceUnit::PICOOHM:    return fizmo::converters::electrical::resistance::si_to_picoohm(si_value);
        case ResistanceUnit::NANOOHM:    return fizmo::converters::electrical::resistance::si_to_nanoohm(si_value); 
        case ResistanceUnit::MICROOHM:   return fizmo::converters::electrical::resistance::si_to_microohm(si_value); 
        case ResistanceUnit::MILLIOHM:   return fizmo::converters::electrical::resistance::si_to_milliohm(si_value); 
        case ResistanceUnit::CENTIOHM:   return fizmo::converters::electrical::resistance::si_to_centiohm(si_value);
        case ResistanceUnit::DECIOHM:    return fizmo::converters::electrical::resistance::si_to_deciohm(si_value);
        case ResistanceUnit::DECAOHM:    return fizmo::converters::electrical::resistance::si_to_decaohm(si_value);
        case ResistanceUnit::HECTOOHM:   return fizmo::converters::electrical::resistance::si_to_hectoohm(si_value);
        case ResistanceUnit::KILOOHM:    return fizmo::converters::electrical::resistance::si_to_kiloohm(si_value); 
        case ResistanceUnit::MEGAOHM:    return fizmo::converters::electrical::resistance::si_to_megaohm(si_value); 
        case ResistanceUnit::GIGAOHM:    return fizmo::converters::electrical::resistance::si_to_gigaohm(si_value); 
        case ResistanceUnit::TERAOHM:    return fizmo::converters::electrical::resistance::si_to_teraohm(si_value);
        case ResistanceUnit::PETAOHM:    return fizmo::converters::electrical::resistance::si_to_petaohm(si_value);
        case ResistanceUnit::EXAOHM:     return fizmo::converters::electrical::resistance::si_to_exaohm(si_value);
        case ResistanceUnit::ZETTAOHM:   return fizmo::converters::electrical::resistance::si_to_zettaohm(si_value);
        case ResistanceUnit::YOTTAOHM:   return fizmo::converters::electrical::resistance::si_to_yottaohm(si_value);
        case ResistanceUnit::RONNAOHM:   return fizmo::converters::electrical::resistance::si_to_ronnaohm(si_value);
        case ResistanceUnit::QUETTAOHM:  return fizmo::converters::electrical::resistance::si_to_quettaohm(si_value);
        case ResistanceUnit::STATOHM:    return fizmo::converters::electrical::resistance::si_to_statohm(si_value); 
        case ResistanceUnit::ABOHM:      return fizmo::converters::electrical::resistance::si_to_abohm(si_value);
        case ResistanceUnit::PLANCK_OHM: return fizmo::converters::electrical::resistance::si_to_planck_ohm(si_value);
    }
    return value;
}

constexpr long double convert_frequency(const long double value, const FrequencyUnit from, const FrequencyUnit to) noexcept {
    // First convert to SI unit (Hertz)
    long double si_value = 0;
    switch (from) {
        case FrequencyUnit::HERTZ:          si_value = value; break;
        case FrequencyUnit::QUECTOHERTZ:    si_value = fizmo::converters::frequency::quectohertz_to_si(value); break;
        case FrequencyUnit::RONTOHERTZ:     si_value = fizmo::converters::frequency::rontohertz_to_si(value); break;
        case FrequencyUnit::YOCTOHERTZ:     si_value = fizmo::converters::frequency::yoctohertz_to_si(value); break;
        case FrequencyUnit::ZEPTOHERTZ:     si_value = fizmo::converters::frequency::zeptohertz_to_si(value); break;
        case FrequencyUnit::ATTOHERTZ:      si_value = fizmo::converters::frequency::attohertz_to_si(value); break;
        case FrequencyUnit::FEMTOHERTZ:     si_value = fizmo::converters::frequency::femtohertz_to_si(value); break;
        case FrequencyUnit::PICOHERTZ:      si_value = fizmo::converters::frequency::picohertz_to_si(value); break;
        case FrequencyUnit::NANOHERTZ:      si_value = fizmo::converters::frequency::nanohertz_to_si(value); break;
        case FrequencyUnit::MICROHERTZ:     si_value = fizmo::converters::frequency::microhertz_to_si(value); break;
        case FrequencyUnit::MILLIHERTZ:     si_value = fizmo::converters::frequency::millihertz_to_si(value); break;
        case FrequencyUnit::CENTIHERTZ:     si_value = fizmo::converters::frequency::centihertz_to_si(value); break;
        case FrequencyUnit::DECIHERTZ:      si_value = fizmo::converters::frequency::decihertz_to_si(value); break;
        case FrequencyUnit::DECAHERTZ:      si_value = fizmo::converters::frequency::decahertz_to_si(value); break;
        case FrequencyUnit::HECTOHERTZ:     si_value = fizmo::converters::frequency::hectohertz_to_si(value); break;
        case FrequencyUnit::KILOHERTZ:      si_value = fizmo::converters::frequency::kilohertz_to_si(value); break;
        case FrequencyUnit::MEGAHERTZ:      si_value = fizmo::converters::frequency::megahertz_to_si(value); break;
        case FrequencyUnit::GIGAHERTZ:      si_value = fizmo::converters::frequency::gigahertz_to_si(value); break;
        case FrequencyUnit::TERAHERTZ:      si_value = fizmo::converters::frequency::terahertz_to_si(value); break;
        case FrequencyUnit::PETAHERTZ:      si_value = fizmo::converters::frequency::petahertz_to_si(value); break;
        case FrequencyUnit::EXAHERTZ:       si_value = fizmo::converters::frequency::exahertz_to_si(value); break;
        case FrequencyUnit::ZETTAHERTZ:     si_value = fizmo::converters::frequency::zettahertz_to_si(value); break;
        case FrequencyUnit::YOTTAHERTZ:     si_value = fizmo::converters::frequency::yottahertz_to_si(value); break;
        case FrequencyUnit::RONNAHERTZ:     si_value = fizmo::converters::frequency::ronnahertz_to_si(value); break;
        case FrequencyUnit::QUETTAHERTZ:    si_value = fizmo::converters::frequency::quettahertz_to_si(value); break;
        case FrequencyUnit::PERIOD_MINUTE:         si_value = fizmo::converters::frequency::period_minute_to_si(value); break;
        case FrequencyUnit::PERIOD_HOUR:           si_value = fizmo::converters::frequency::period_hour_to_si(value); break;
        case FrequencyUnit::PERIOD_DAY:            si_value = fizmo::converters::frequency::period_day_to_si(value); break;
        case FrequencyUnit::PERIOD_WEEK:           si_value = fizmo::converters::frequency::period_week_to_si(value); break;
        case FrequencyUnit::PERIOD_YEAR:           si_value = fizmo::converters::frequency::period_year_to_si(value); break;
        case FrequencyUnit::PERIOD_DECADE:         si_value = fizmo::converters::frequency::period_decade_to_si(value); break;
        case FrequencyUnit::PERIOD_CENTURY:        si_value = fizmo::converters::frequency::period_century_to_si(value); break;
        case FrequencyUnit::PERIOD_MILLENNIUM:     si_value = fizmo::converters::frequency::period_millennium_to_si(value); break;
    }
    
    switch (to) {
        case FrequencyUnit::HERTZ:          return si_value;
        case FrequencyUnit::QUECTOHERTZ:    return fizmo::converters::frequency::si_to_quectohertz(si_value);
        case FrequencyUnit::RONTOHERTZ:     return fizmo::converters::frequency::si_to_rontohertz(si_value);
        case FrequencyUnit::YOCTOHERTZ:     return fizmo::converters::frequency::si_to_yoctohertz(si_value);
        case FrequencyUnit::ZEPTOHERTZ:     return fizmo::converters::frequency::si_to_zeptohertz(si_value);
        case FrequencyUnit::ATTOHERTZ:      return fizmo::converters::frequency::si_to_attohertz(si_value);
        case FrequencyUnit::FEMTOHERTZ:     return fizmo::converters::frequency::si_to_femtohertz(si_value);
        case FrequencyUnit::PICOHERTZ:      return fizmo::converters::frequency::si_to_picohertz(si_value);
        case FrequencyUnit::NANOHERTZ:      return fizmo::converters::frequency::si_to_nanohertz(si_value);
        case FrequencyUnit::MICROHERTZ:     return fizmo::converters::frequency::si_to_microhertz(si_value);
        case FrequencyUnit::MILLIHERTZ:     return fizmo::converters::frequency::si_to_millihertz(si_value);
        case FrequencyUnit::CENTIHERTZ:     return fizmo::converters::frequency::si_to_centihertz(si_value);
        case FrequencyUnit::DECIHERTZ:      return fizmo::converters::frequency::si_to_decihertz(si_value);
        case FrequencyUnit::DECAHERTZ:      return fizmo::converters::frequency::si_to_decahertz(si_value);
        case FrequencyUnit::HECTOHERTZ:     return fizmo::converters::frequency::si_to_hectohertz(si_value);
        case FrequencyUnit::KILOHERTZ:      return fizmo::converters::frequency::si_to_kilohertz(si_value);
        case FrequencyUnit::MEGAHERTZ:      return fizmo::converters::frequency::si_to_megahertz(si_value);
        case FrequencyUnit::GIGAHERTZ:      return fizmo::converters::frequency::si_to_gigahertz(si_value);
        case FrequencyUnit::TERAHERTZ:      return fizmo::converters::frequency::si_to_terahertz(si_value);
        case FrequencyUnit::PETAHERTZ:      return fizmo::converters::frequency::si_to_petahertz(si_value);
        case FrequencyUnit::EXAHERTZ:       return fizmo::converters::frequency::si_to_exahertz(si_value);
        case FrequencyUnit::ZETTAHERTZ:     return fizmo::converters::frequency::si_to_zettahertz(si_value);
        case FrequencyUnit::YOTTAHERTZ:     return fizmo::converters::frequency::si_to_yottahertz(si_value);
        case FrequencyUnit::RONNAHERTZ:     return fizmo::converters::frequency::si_to_ronnahertz(si_value);
        case FrequencyUnit::QUETTAHERTZ:    return fizmo::converters::frequency::si_to_quettahertz(si_value);
        case FrequencyUnit::PERIOD_MINUTE:         return fizmo::converters::frequency::si_to_period_minute(si_value);
        case FrequencyUnit::PERIOD_HOUR:           return fizmo::converters::frequency::si_to_period_hour(si_value);
        case FrequencyUnit::PERIOD_DAY:            return fizmo::converters::frequency::si_to_period_day(si_value);
        case FrequencyUnit::PERIOD_WEEK:           return fizmo::converters::frequency::si_to_period_week(si_value);
        case FrequencyUnit::PERIOD_YEAR:           return fizmo::converters::frequency::si_to_period_year(si_value);
        case FrequencyUnit::PERIOD_DECADE:         return fizmo::converters::frequency::si_to_period_decade(si_value);
        case FrequencyUnit::PERIOD_CENTURY:        return fizmo::converters::frequency::si_to_period_century(si_value);
        case FrequencyUnit::PERIOD_MILLENNIUM:     return fizmo::converters::frequency::si_to_period_millennium(si_value);
    }

    return value;
}

// Conversion function for radiation
constexpr long double convert_radiation(const long double value, const RadiationUnit from, const RadiationUnit to) noexcept {
    // First convert to SI unit (Gray for absorbed dose, Becquerel for activity)
    long double si_value = 0;
    switch (from) {
        case RadiationUnit::GRAY:      si_value = value; break;
        case RadiationUnit::ROENTGEN:  si_value = fizmo::converters::radiation::roentgen_to_si(value); break;
        case RadiationUnit::RAD:       si_value = fizmo::converters::radiation::rad_to_si(value); break;
        case RadiationUnit::SIEVERT:   si_value = fizmo::converters::radiation::sievert_to_si(value); break;
        case RadiationUnit::REM:       si_value = fizmo::converters::radiation::rem_to_si(value); break;
        case RadiationUnit::BECQUEREL: si_value = value; break;
        case RadiationUnit::CURIE:     si_value = fizmo::converters::radiation::curie_to_si(value); break;
    }

    // Then convert from SI to target unit
    switch (to) {
        case RadiationUnit::GRAY:      return si_value;
        case RadiationUnit::ROENTGEN:  return fizmo::converters::radiation::si_to_roentgen(si_value);
        case RadiationUnit::RAD:       return fizmo::converters::radiation::si_to_rad(si_value);
        case RadiationUnit::SIEVERT:   return fizmo::converters::radiation::si_to_sievert(si_value);
        case RadiationUnit::REM:       return fizmo::converters::radiation::si_to_rem(si_value);
        case RadiationUnit::BECQUEREL: return si_value;
        case RadiationUnit::CURIE:     return fizmo::converters::radiation::si_to_curie(si_value);
    }
    return value; 
}

constexpr long double convert_distance(const long double value, const DistanceUnit from, const DistanceUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case DistanceUnit::METER:                 si_value = value; break;
        case DistanceUnit::QUECTOMETER:           si_value = fizmo::converters::distance::quectometers_to_si(value); break;
        case DistanceUnit::RONTOMETER:            si_value = fizmo::converters::distance::rontometers_to_si(value); break;
        case DistanceUnit::YOCTOMETER:            si_value = fizmo::converters::distance::yoctometers_to_si(value); break;
        case DistanceUnit::ZEPTOMETER:            si_value = fizmo::converters::distance::zeptometers_to_si(value); break;
        case DistanceUnit::ATTOMETER:             si_value = fizmo::converters::distance::attometers_to_si(value); break;
        case DistanceUnit::FEMTOMETER:            si_value = fizmo::converters::distance::femtometers_to_si(value); break;
        case DistanceUnit::PICOMETER:             si_value = fizmo::converters::distance::picometers_to_si(value); break;
        case DistanceUnit::NANOMETER:             si_value = fizmo::converters::distance::nanometers_to_si(value); break;
        case DistanceUnit::MICROMETER:            si_value = fizmo::converters::distance::micrometers_to_si(value); break;
        case DistanceUnit::MILLIMETER:            si_value = fizmo::converters::distance::millimeters_to_si(value); break;
        case DistanceUnit::CENTIMETER:            si_value = fizmo::converters::distance::centimeters_to_si(value); break;
        case DistanceUnit::DECIMETER:             si_value = fizmo::converters::distance::decimeters_to_si(value); break;
        case DistanceUnit::DECAMETER:             si_value = fizmo::converters::distance::decameters_to_si(value); break;
        case DistanceUnit::HECTOMETER:            si_value = fizmo::converters::distance::hectometers_to_si(value); break;
        case DistanceUnit::KILOMETER:             si_value = fizmo::converters::distance::kilometers_to_si(value); break;
        case DistanceUnit::MEGAMETER:             si_value = fizmo::converters::distance::megameters_to_si(value); break;
        case DistanceUnit::GIGAMETER:             si_value = fizmo::converters::distance::gigameters_to_si(value); break;
        case DistanceUnit::TERAMETER:             si_value = fizmo::converters::distance::terameters_to_si(value); break;
        case DistanceUnit::PETAMETER:             si_value = fizmo::converters::distance::petameters_to_si(value); break;
        case DistanceUnit::EXAMETER:              si_value = fizmo::converters::distance::exameters_to_si(value); break;
        case DistanceUnit::ZETTAMETER:            si_value = fizmo::converters::distance::zettameters_to_si(value); break;
        case DistanceUnit::YOTTAMETER:            si_value = fizmo::converters::distance::yottameters_to_si(value); break;
        case DistanceUnit::RONNAMETER:            si_value = fizmo::converters::distance::ronnameters_to_si(value); break;
        case DistanceUnit::QUETTAMETER:           si_value = fizmo::converters::distance::quettameters_to_si(value); break;
        case DistanceUnit::ANGSTROM:              si_value = fizmo::converters::distance::angstroms_to_si(value); break;
        case DistanceUnit::BOHR:                  si_value = fizmo::converters::distance::bohr_to_si(value); break;
        case DistanceUnit::PLANCK:                si_value = fizmo::converters::distance::planck_to_si(value); break;
        case DistanceUnit::INCH:                  si_value = fizmo::converters::distance::inches_to_si(value); break;
        case DistanceUnit::FOOT:                  si_value = fizmo::converters::distance::feet_to_si(value); break;
        case DistanceUnit::YARD:                  si_value = fizmo::converters::distance::yards_to_si(value); break;
        case DistanceUnit::MILE:                  si_value = fizmo::converters::distance::miles_to_si(value); break;
        case DistanceUnit::NAUTICAL_MILE:         si_value = fizmo::converters::distance::nautical_miles_to_si(value); break;
        case DistanceUnit::LEAGUE:                si_value = fizmo::converters::distance::leagues_to_si(value); break;
        case DistanceUnit::ROD:                   si_value = fizmo::converters::distance::rods_to_si(value); break;
        case DistanceUnit::CHAIN:                 si_value = fizmo::converters::distance::chains_to_si(value); break;
        case DistanceUnit::FURLONG:               si_value = fizmo::converters::distance::furlongs_to_si(value); break;
        case DistanceUnit::HAND:                  si_value = fizmo::converters::distance::hands_to_si(value); break;
        case DistanceUnit::SMOOT:                 si_value = fizmo::converters::distance::smoots_to_si(value); break;
        case DistanceUnit::AU:                    si_value = fizmo::converters::distance::au_to_si(value); break;
        case DistanceUnit::LIGHT_PLANCK_TIME:     si_value = fizmo::converters::distance::light_planck_time_to_si(value); break;
        case DistanceUnit::LIGHT_QUECTOSECOND:    si_value = fizmo::converters::distance::light_quectoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_RONTOSECOND:     si_value = fizmo::converters::distance::light_rontoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_YOCTOSECOND:     si_value = fizmo::converters::distance::light_yoctoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_ZEPTOSECOND:     si_value = fizmo::converters::distance::light_zeptoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_ATTOSECOND:      si_value = fizmo::converters::distance::light_attoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_FEMTOSECOND:     si_value = fizmo::converters::distance::light_femtoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_PICOSECOND:      si_value = fizmo::converters::distance::light_picoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_NANOSECOND:      si_value = fizmo::converters::distance::light_nanoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_MICROSECOND:     si_value = fizmo::converters::distance::light_microseconds_to_si(value); break;
        case DistanceUnit::LIGHT_MILLISECOND:     si_value = fizmo::converters::distance::light_milliseconds_to_si(value); break;
        case DistanceUnit::LIGHT_CENTISECOND:     si_value = fizmo::converters::distance::light_centiseconds_to_si(value); break;
        case DistanceUnit::LIGHT_DECISECOND:      si_value = fizmo::converters::distance::light_deciseconds_to_si(value); break;
        case DistanceUnit::LIGHT_SECOND:          si_value = fizmo::converters::distance::light_seconds_to_si(value); break;
        case DistanceUnit::LIGHT_DECASECOND:      si_value = fizmo::converters::distance::light_decaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_HECTOSECOND:     si_value = fizmo::converters::distance::light_hectoseconds_to_si(value); break;
        case DistanceUnit::LIGHT_KILOSECOND:      si_value = fizmo::converters::distance::light_kiloseconds_to_si(value); break;
        case DistanceUnit::LIGHT_MINUTE:          si_value = fizmo::converters::distance::light_minutes_to_si(value); break;
        case DistanceUnit::LIGHT_MEGASECOND:      si_value = fizmo::converters::distance::light_megaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_HOUR:            si_value = fizmo::converters::distance::light_hours_to_si(value); break;
        case DistanceUnit::LIGHT_GIGASECOND:      si_value = fizmo::converters::distance::light_gigaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_DAY:             si_value = fizmo::converters::distance::light_days_to_si(value); break;
        case DistanceUnit::LIGHT_WEEK:            si_value = fizmo::converters::distance::light_weeks_to_si(value); break;
        case DistanceUnit::LIGHT_TERASECOND:      si_value = fizmo::converters::distance::light_teraseconds_to_si(value); break;
        case DistanceUnit::LIGHT_PETASECOND:      si_value = fizmo::converters::distance::light_petaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_YEAR:            si_value = fizmo::converters::distance::light_years_to_si(value); break;
        case DistanceUnit::LIGHT_EXASECOND:       si_value = fizmo::converters::distance::light_exaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_ZETTASECOND:     si_value = fizmo::converters::distance::light_zettaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_YOTTASECOND:     si_value = fizmo::converters::distance::light_yottaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_RONNASECOND:     si_value = fizmo::converters::distance::light_ronnaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_QUETTASECOND:    si_value = fizmo::converters::distance::light_quettaseconds_to_si(value); break;
        case DistanceUnit::LIGHT_DECADE:          si_value = fizmo::converters::distance::light_decades_to_si(value); break;
        case DistanceUnit::LIGHT_CENTURY:         si_value = fizmo::converters::distance::light_centuries_to_si(value); break;
        case DistanceUnit::LIGHT_MILLENNIUM:      si_value = fizmo::converters::distance::light_millennia_to_si(value); break;
        case DistanceUnit::QUECTOPARSEC:          si_value = fizmo::converters::distance::quectoparsec_to_si(value); break;
        case DistanceUnit::RONTOPARSEC:           si_value = fizmo::converters::distance::rontoparsec_to_si(value); break;
        case DistanceUnit::YOCTOPARSEC:           si_value = fizmo::converters::distance::yoctoparsec_to_si(value); break;
        case DistanceUnit::ZEPTOPARSEC:           si_value = fizmo::converters::distance::zeptoparsec_to_si(value); break;
        case DistanceUnit::ATTOPARSEC:            si_value = fizmo::converters::distance::attoparsec_to_si(value); break;
        case DistanceUnit::FEMTOPARSEC:           si_value = fizmo::converters::distance::femtoparsec_to_si(value); break;
        case DistanceUnit::PICOPARSEC:            si_value = fizmo::converters::distance::picoparsec_to_si(value); break;
        case DistanceUnit::NANOPARSEC:            si_value = fizmo::converters::distance::nanoparsec_to_si(value); break;
        case DistanceUnit::MICROPARSEC:           si_value = fizmo::converters::distance::microparsec_to_si(value); break;
        case DistanceUnit::MILLIPARSEC:           si_value = fizmo::converters::distance::milliparsec_to_si(value); break;
        case DistanceUnit::CENTIPARSEC:           si_value = fizmo::converters::distance::centiparsec_to_si(value); break;
        case DistanceUnit::DECIPARSEC:            si_value = fizmo::converters::distance::deciparsec_to_si(value); break;
        case DistanceUnit::PARSEC:                si_value = fizmo::converters::distance::parsecs_to_si(value); break;
        case DistanceUnit::DECAPARSEC:            si_value = fizmo::converters::distance::decaparsec_to_si(value); break;
        case DistanceUnit::HECTOPARSEC:           si_value = fizmo::converters::distance::hectoparsec_to_si(value); break;
        case DistanceUnit::KILOPARSEC:            si_value = fizmo::converters::distance::kiloparsec_to_si(value); break;
        case DistanceUnit::MEGAPARSEC:            si_value = fizmo::converters::distance::megaparsec_to_si(value); break;
        case DistanceUnit::GIGAPARSEC:            si_value = fizmo::converters::distance::gigaparsec_to_si(value); break;
        case DistanceUnit::TERAPARSEC:            si_value = fizmo::converters::distance::teraparsec_to_si(value); break;
        case DistanceUnit::PETAPARSEC:            si_value = fizmo::converters::distance::petaparsec_to_si(value); break;
        case DistanceUnit::EXAPARSEC:             si_value = fizmo::converters::distance::exaparsec_to_si(value); break;
        case DistanceUnit::ZETTAPARSEC:           si_value = fizmo::converters::distance::zettaparsec_to_si(value); break;
        case DistanceUnit::YOTTAPARSEC:           si_value = fizmo::converters::distance::yottaparsec_to_si(value); break;
        case DistanceUnit::RONNAPARSEC:           si_value = fizmo::converters::distance::ronnaparsec_to_si(value); break;
        case DistanceUnit::QUETTAPARSEC:          si_value = fizmo::converters::distance::quettaparsec_to_si(value); break;
    }

    switch (to) {
        case DistanceUnit::METER:                 return si_value;
        case DistanceUnit::QUECTOMETER:           return fizmo::converters::distance::si_to_quectometers(si_value);
        case DistanceUnit::RONTOMETER:            return fizmo::converters::distance::si_to_rontometers(si_value);
        case DistanceUnit::YOCTOMETER:            return fizmo::converters::distance::si_to_yoctometers(si_value);
        case DistanceUnit::ZEPTOMETER:            return fizmo::converters::distance::si_to_zeptometers(si_value);
        case DistanceUnit::ATTOMETER:             return fizmo::converters::distance::si_to_attometers(si_value);
        case DistanceUnit::FEMTOMETER:            return fizmo::converters::distance::si_to_femtometers(si_value);
        case DistanceUnit::PICOMETER:             return fizmo::converters::distance::si_to_picometers(si_value);
        case DistanceUnit::NANOMETER:             return fizmo::converters::distance::si_to_nanometers(si_value);
        case DistanceUnit::MICROMETER:            return fizmo::converters::distance::si_to_micrometers(si_value);
        case DistanceUnit::MILLIMETER:            return fizmo::converters::distance::si_to_millimeters(si_value);
        case DistanceUnit::CENTIMETER:            return fizmo::converters::distance::si_to_centimeters(si_value);
        case DistanceUnit::DECIMETER:             return fizmo::converters::distance::si_to_decimeters(si_value);
        case DistanceUnit::DECAMETER:             return fizmo::converters::distance::si_to_decameters(si_value);
        case DistanceUnit::HECTOMETER:            return fizmo::converters::distance::si_to_hectometers(si_value);
        case DistanceUnit::KILOMETER:             return fizmo::converters::distance::si_to_kilometers(si_value);
        case DistanceUnit::MEGAMETER:             return fizmo::converters::distance::si_to_megameters(si_value);
        case DistanceUnit::GIGAMETER:             return fizmo::converters::distance::si_to_gigameters(si_value);
        case DistanceUnit::TERAMETER:             return fizmo::converters::distance::si_to_terameters(si_value);
        case DistanceUnit::PETAMETER:             return fizmo::converters::distance::si_to_petameters(si_value);
        case DistanceUnit::EXAMETER:              return fizmo::converters::distance::si_to_exameters(si_value);
        case DistanceUnit::ZETTAMETER:            return fizmo::converters::distance::si_to_zettameters(si_value);
        case DistanceUnit::YOTTAMETER:            return fizmo::converters::distance::si_to_yottameters(si_value);
        case DistanceUnit::RONNAMETER:            return fizmo::converters::distance::si_to_ronnameters(si_value);
        case DistanceUnit::QUETTAMETER:           return fizmo::converters::distance::si_to_quettameters(si_value);
        case DistanceUnit::ANGSTROM:              return fizmo::converters::distance::si_to_angstroms(si_value);
        case DistanceUnit::BOHR:                  return fizmo::converters::distance::si_to_bohr(si_value);
        case DistanceUnit::PLANCK:                return fizmo::converters::distance::si_to_planck(si_value);
        case DistanceUnit::INCH:                  return fizmo::converters::distance::si_to_inches(si_value);
        case DistanceUnit::FOOT:                  return fizmo::converters::distance::si_to_feet(si_value);
        case DistanceUnit::YARD:                  return fizmo::converters::distance::si_to_yards(si_value);
        case DistanceUnit::MILE:                  return fizmo::converters::distance::si_to_miles(si_value);
        case DistanceUnit::NAUTICAL_MILE:         return fizmo::converters::distance::si_to_nautical_miles(si_value);
        case DistanceUnit::LEAGUE:                return fizmo::converters::distance::si_to_leagues(si_value);
        case DistanceUnit::ROD:                   return fizmo::converters::distance::si_to_rods(si_value);
        case DistanceUnit::CHAIN:                 return fizmo::converters::distance::si_to_chains(si_value);
        case DistanceUnit::FURLONG:               return fizmo::converters::distance::si_to_furlongs(si_value);
        case DistanceUnit::HAND:                  return fizmo::converters::distance::si_to_hands(si_value);
        case DistanceUnit::SMOOT:                 return fizmo::converters::distance::si_to_smoots(si_value);
        case DistanceUnit::AU:                    return fizmo::converters::distance::si_to_au(si_value);
        case DistanceUnit::LIGHT_PLANCK_TIME:     return fizmo::converters::distance::si_to_light_planck_time(si_value);
        case DistanceUnit::LIGHT_QUECTOSECOND:    return fizmo::converters::distance::si_to_light_quectoseconds(si_value);
        case DistanceUnit::LIGHT_RONTOSECOND:     return fizmo::converters::distance::si_to_light_rontoseconds(si_value);
        case DistanceUnit::LIGHT_YOCTOSECOND:     return fizmo::converters::distance::si_to_light_yoctoseconds(si_value);
        case DistanceUnit::LIGHT_ZEPTOSECOND:     return fizmo::converters::distance::si_to_light_zeptoseconds(si_value);
        case DistanceUnit::LIGHT_ATTOSECOND:      return fizmo::converters::distance::si_to_light_attoseconds(si_value);
        case DistanceUnit::LIGHT_FEMTOSECOND:     return fizmo::converters::distance::si_to_light_femtoseconds(si_value);
        case DistanceUnit::LIGHT_PICOSECOND:      return fizmo::converters::distance::si_to_light_picoseconds(si_value);
        case DistanceUnit::LIGHT_NANOSECOND:      return fizmo::converters::distance::si_to_light_nanoseconds(si_value);
        case DistanceUnit::LIGHT_MICROSECOND:     return fizmo::converters::distance::si_to_light_microseconds(si_value);
        case DistanceUnit::LIGHT_MILLISECOND:     return fizmo::converters::distance::si_to_light_milliseconds(si_value);
        case DistanceUnit::LIGHT_CENTISECOND:     return fizmo::converters::distance::si_to_light_centiseconds(si_value);
        case DistanceUnit::LIGHT_DECISECOND:      return fizmo::converters::distance::si_to_light_deciseconds(si_value);
        case DistanceUnit::LIGHT_SECOND:          return fizmo::converters::distance::si_to_light_seconds(si_value);
        case DistanceUnit::LIGHT_DECASECOND:      return fizmo::converters::distance::si_to_light_decaseconds(si_value);
        case DistanceUnit::LIGHT_HECTOSECOND:     return fizmo::converters::distance::si_to_light_hectoseconds(si_value);
        case DistanceUnit::LIGHT_KILOSECOND:      return fizmo::converters::distance::si_to_light_kiloseconds(si_value);
        case DistanceUnit::LIGHT_MINUTE:          return fizmo::converters::distance::si_to_light_minutes(si_value);
        case DistanceUnit::LIGHT_MEGASECOND:      return fizmo::converters::distance::si_to_light_megaseconds(si_value);
        case DistanceUnit::LIGHT_HOUR:            return fizmo::converters::distance::si_to_light_hours(si_value);
        case DistanceUnit::LIGHT_GIGASECOND:      return fizmo::converters::distance::si_to_light_gigaseconds(si_value);
        case DistanceUnit::LIGHT_DAY:             return fizmo::converters::distance::si_to_light_days(si_value);
        case DistanceUnit::LIGHT_WEEK:            return fizmo::converters::distance::si_to_light_weeks(si_value);
        case DistanceUnit::LIGHT_TERASECOND:      return fizmo::converters::distance::si_to_light_teraseconds(si_value);
        case DistanceUnit::LIGHT_PETASECOND:      return fizmo::converters::distance::si_to_light_petaseconds(si_value);
        case DistanceUnit::LIGHT_YEAR:            return fizmo::converters::distance::si_to_light_years(si_value);
        case DistanceUnit::LIGHT_EXASECOND:       return fizmo::converters::distance::si_to_light_exaseconds(si_value);
        case DistanceUnit::LIGHT_ZETTASECOND:     return fizmo::converters::distance::si_to_light_zettaseconds(si_value);
        case DistanceUnit::LIGHT_YOTTASECOND:     return fizmo::converters::distance::si_to_light_yottaseconds(si_value);
        case DistanceUnit::LIGHT_RONNASECOND:     return fizmo::converters::distance::si_to_light_ronnaseconds(si_value);
        case DistanceUnit::LIGHT_QUETTASECOND:    return fizmo::converters::distance::si_to_light_quettaseconds(si_value);
        case DistanceUnit::LIGHT_DECADE:          return fizmo::converters::distance::si_to_light_decades(si_value); 
        case DistanceUnit::LIGHT_CENTURY:         return fizmo::converters::distance::si_to_light_centuries(si_value); 
        case DistanceUnit::LIGHT_MILLENNIUM:      return fizmo::converters::distance::si_to_light_millennia(si_value); 
        case DistanceUnit::QUECTOPARSEC:          return fizmo::converters::distance::si_to_quectoparsec(si_value);
        case DistanceUnit::RONTOPARSEC:           return fizmo::converters::distance::si_to_rontoparsec(si_value);
        case DistanceUnit::YOCTOPARSEC:           return fizmo::converters::distance::si_to_yoctoparsec(si_value);
        case DistanceUnit::ZEPTOPARSEC:           return fizmo::converters::distance::si_to_zeptoparsec(si_value);
        case DistanceUnit::ATTOPARSEC:            return fizmo::converters::distance::si_to_attoparsec(si_value);
        case DistanceUnit::FEMTOPARSEC:           return fizmo::converters::distance::si_to_femtoparsec(si_value);
        case DistanceUnit::PICOPARSEC:            return fizmo::converters::distance::si_to_picoparsec(si_value);
        case DistanceUnit::NANOPARSEC:            return fizmo::converters::distance::si_to_nanoparsec(si_value);
        case DistanceUnit::MICROPARSEC:           return fizmo::converters::distance::si_to_microparsec(si_value);
        case DistanceUnit::MILLIPARSEC:           return fizmo::converters::distance::si_to_milliparsec(si_value);
        case DistanceUnit::CENTIPARSEC:           return fizmo::converters::distance::si_to_centiparsec(si_value);
        case DistanceUnit::DECIPARSEC:            return fizmo::converters::distance::si_to_deciparsec(si_value);
        case DistanceUnit::PARSEC:                return fizmo::converters::distance::si_to_parsecs(si_value);
        case DistanceUnit::DECAPARSEC:            return fizmo::converters::distance::si_to_decaparsec(si_value);
        case DistanceUnit::HECTOPARSEC:           return fizmo::converters::distance::si_to_hectoparsec(si_value);
        case DistanceUnit::KILOPARSEC:            return fizmo::converters::distance::si_to_kiloparsec(si_value);
        case DistanceUnit::MEGAPARSEC:            return fizmo::converters::distance::si_to_megaparsec(si_value);
        case DistanceUnit::GIGAPARSEC:            return fizmo::converters::distance::si_to_gigaparsec(si_value);
        case DistanceUnit::TERAPARSEC:            return fizmo::converters::distance::si_to_teraparsec(si_value);
        case DistanceUnit::PETAPARSEC:            return fizmo::converters::distance::si_to_petaparsec(si_value);
        case DistanceUnit::EXAPARSEC:             return fizmo::converters::distance::si_to_exaparsec(si_value);
        case DistanceUnit::ZETTAPARSEC:           return fizmo::converters::distance::si_to_zettaparsec(si_value);
        case DistanceUnit::YOTTAPARSEC:           return fizmo::converters::distance::si_to_yottaparsec(si_value);
        case DistanceUnit::RONNAPARSEC:           return fizmo::converters::distance::si_to_ronnaparsec(si_value);
        case DistanceUnit::QUETTAPARSEC:          return fizmo::converters::distance::si_to_quettaparsec(si_value);
    }
    return value; 
}

constexpr long double convert_temperature(const long double value, const TemperatureUnit from, const TemperatureUnit to) noexcept {
    // First convert to SI unit (Kelvin)
    long double si_value = 0;
    switch (from) {
        case TemperatureUnit::KELVIN:      si_value = value; break;
        case TemperatureUnit::CELSIUS:     si_value = fizmo::converters::temperature::celsius_to_si(value); break;
        case TemperatureUnit::FAHRENHEIT:  si_value = fizmo::converters::temperature::fahrenheit_to_si(value); break;
        case TemperatureUnit::RANKINE:     si_value = fizmo::converters::temperature::rankine_to_si(value); break;
        case TemperatureUnit::REAUMUR:     si_value = fizmo::converters::temperature::reaumur_to_si(value); break;
        case TemperatureUnit::ROMER:       si_value = fizmo::converters::temperature::romer_to_si(value); break;
        case TemperatureUnit::DELISLE:     si_value = fizmo::converters::temperature::delisle_to_si(value); break;
        case TemperatureUnit::NEWTON:      si_value = fizmo::converters::temperature::newton_to_si(value); break;
    }

    // Then convert from SI to target unit
    switch (to) {
        case TemperatureUnit::KELVIN:      return si_value;
        case TemperatureUnit::CELSIUS:     return fizmo::converters::temperature::si_to_celsius(si_value);
        case TemperatureUnit::FAHRENHEIT:  return fizmo::converters::temperature::si_to_fahrenheit(si_value);
        case TemperatureUnit::RANKINE:     return fizmo::converters::temperature::si_to_rankine(si_value);
        case TemperatureUnit::REAUMUR:     return fizmo::converters::temperature::si_to_reaumur(si_value);
        case TemperatureUnit::ROMER:       return fizmo::converters::temperature::si_to_romer(si_value);
        case TemperatureUnit::DELISLE:     return fizmo::converters::temperature::si_to_delisle(si_value);
        case TemperatureUnit::NEWTON:      return fizmo::converters::temperature::si_to_newton(si_value);
    }
    return value; 
}

constexpr long double convert_time(const long double value, const TimeUnit from, const TimeUnit to) noexcept {
    long double si_value = 0;
    switch (from) {
        case TimeUnit::SECOND:        si_value = value; break;
        case TimeUnit::QUECTOSECOND:  si_value = fizmo::converters::time::quectoseconds_to_si(value); break;
        case TimeUnit::RONTOSECOND:   si_value = fizmo::converters::time::rontoseconds_to_si(value); break;
        case TimeUnit::YOCTOSECOND:   si_value = fizmo::converters::time::yoctoseconds_to_si(value); break;
        case TimeUnit::ZEPTOSECOND:   si_value = fizmo::converters::time::zeptoseconds_to_si(value); break;
        case TimeUnit::ATTOSECOND:    si_value = fizmo::converters::time::attoseconds_to_si(value); break;
        case TimeUnit::FEMTOSECOND:   si_value = fizmo::converters::time::femtoseconds_to_si(value); break;
        case TimeUnit::PICOSECOND:    si_value = fizmo::converters::time::picoseconds_to_si(value); break;
        case TimeUnit::NANOSECOND:    si_value = fizmo::converters::time::nanoseconds_to_si(value); break;
        case TimeUnit::MICROSECOND:   si_value = fizmo::converters::time::microseconds_to_si(value); break;
        case TimeUnit::MILLISECOND:   si_value = fizmo::converters::time::milliseconds_to_si(value); break;
        case TimeUnit::CENTISECOND:   si_value = fizmo::converters::time::centiseconds_to_si(value); break;
        case TimeUnit::DECISECOND:    si_value = fizmo::converters::time::deciseconds_to_si(value); break;
        case TimeUnit::DECASECOND:    si_value = fizmo::converters::time::decaseconds_to_si(value); break;
        case TimeUnit::HECTOSECOND:   si_value = fizmo::converters::time::hectoseconds_to_si(value); break;
        case TimeUnit::KILOSECOND:    si_value = fizmo::converters::time::kiloseconds_to_si(value); break;
        case TimeUnit::MEGASECOND:    si_value = fizmo::converters::time::megaseconds_to_si(value); break;
        case TimeUnit::GIGASECOND:    si_value = fizmo::converters::time::gigaseconds_to_si(value); break;
        case TimeUnit::TERASECOND:    si_value = fizmo::converters::time::teraseconds_to_si(value); break;
        case TimeUnit::PETASECOND:    si_value = fizmo::converters::time::petaseconds_to_si(value); break;
        case TimeUnit::EXASECOND:     si_value = fizmo::converters::time::exaseconds_to_si(value); break;
        case TimeUnit::ZETTASECOND:   si_value = fizmo::converters::time::zettaseconds_to_si(value); break;
        case TimeUnit::YOTTASECOND:   si_value = fizmo::converters::time::yottaseconds_to_si(value); break;
        case TimeUnit::RONNASECOND:   si_value = fizmo::converters::time::ronnaseconds_to_si(value); break;
        case TimeUnit::QUETTASECOND:  si_value = fizmo::converters::time::quettaseconds_to_si(value); break;
        case TimeUnit::MINUTE:        si_value = fizmo::converters::time::minutes_to_si(value); break;
        case TimeUnit::HOUR:          si_value = fizmo::converters::time::hours_to_si(value); break;
        case TimeUnit::DAY:           si_value = fizmo::converters::time::days_to_si(value); break;
        case TimeUnit::WEEK:          si_value = fizmo::converters::time::weeks_to_si(value); break;
        case TimeUnit::YEAR:          si_value = fizmo::converters::time::years_to_si(value); break;
        case TimeUnit::DECADE:        si_value = fizmo::converters::time::decades_to_si(value); break;
        case TimeUnit::CENTURY:       si_value = fizmo::converters::time::centuries_to_si(value); break;
        case TimeUnit::MILLENNIUM:    si_value = fizmo::converters::time::millennia_to_si(value); break;
    }

    switch (to) {
        case TimeUnit::SECOND:        return si_value;
        case TimeUnit::QUECTOSECOND:  return fizmo::converters::time::si_to_quectoseconds(si_value);
        case TimeUnit::RONTOSECOND:   return fizmo::converters::time::si_to_rontoseconds(si_value);
        case TimeUnit::YOCTOSECOND:   return fizmo::converters::time::si_to_yoctoseconds(si_value);
        case TimeUnit::ZEPTOSECOND:   return fizmo::converters::time::si_to_zeptoseconds(si_value);
        case TimeUnit::ATTOSECOND:    return fizmo::converters::time::si_to_attoseconds(si_value);
        case TimeUnit::FEMTOSECOND:   return fizmo::converters::time::si_to_femtoseconds(si_value);
        case TimeUnit::PICOSECOND:    return fizmo::converters::time::si_to_picoseconds(si_value);
        case TimeUnit::NANOSECOND:    return fizmo::converters::time::si_to_nanoseconds(si_value);
        case TimeUnit::MICROSECOND:   return fizmo::converters::time::si_to_microseconds(si_value);
        case TimeUnit::MILLISECOND:   return fizmo::converters::time::si_to_milliseconds(si_value);
        case TimeUnit::CENTISECOND:   return fizmo::converters::time::si_to_centiseconds(si_value);
        case TimeUnit::DECISECOND:    return fizmo::converters::time::si_to_deciseconds(si_value);
        case TimeUnit::DECASECOND:    return fizmo::converters::time::si_to_decaseconds(si_value);
        case TimeUnit::HECTOSECOND:   return fizmo::converters::time::si_to_hectoseconds(si_value);
        case TimeUnit::KILOSECOND:    return fizmo::converters::time::si_to_kiloseconds(si_value);
        case TimeUnit::MEGASECOND:    return fizmo::converters::time::si_to_megaseconds(si_value);
        case TimeUnit::GIGASECOND:    return fizmo::converters::time::si_to_gigaseconds(si_value);
        case TimeUnit::TERASECOND:    return fizmo::converters::time::si_to_teraseconds(si_value);
        case TimeUnit::PETASECOND:    return fizmo::converters::time::si_to_petaseconds(si_value);
        case TimeUnit::EXASECOND:     return fizmo::converters::time::si_to_exaseconds(si_value);
        case TimeUnit::ZETTASECOND:   return fizmo::converters::time::si_to_zettaseconds(si_value);
        case TimeUnit::YOTTASECOND:   return fizmo::converters::time::si_to_yottaseconds(si_value);
        case TimeUnit::RONNASECOND:   return fizmo::converters::time::si_to_ronnaseconds(si_value);
        case TimeUnit::QUETTASECOND:  return fizmo::converters::time::si_to_quettaseconds(si_value);
        case TimeUnit::MINUTE:        return fizmo::converters::time::si_to_minutes(si_value);
        case TimeUnit::HOUR:          return fizmo::converters::time::si_to_hours(si_value);
        case TimeUnit::DAY:           return fizmo::converters::time::si_to_days(si_value);
        case TimeUnit::WEEK:          return fizmo::converters::time::si_to_weeks(si_value);
        case TimeUnit::YEAR:          return fizmo::converters::time::si_to_years(si_value);
        case TimeUnit::DECADE:        return fizmo::converters::time::si_to_decades(si_value);
        case TimeUnit::CENTURY:       return fizmo::converters::time::si_to_centuries(si_value);
        case TimeUnit::MILLENNIUM:    return fizmo::converters::time::si_to_millennia(si_value);
    }
    return value; 
}

constexpr long double convert_weight(const long double value, const WeightUnit from, const WeightUnit to) noexcept {
    // First convert to SI unit (Kilogram)
    long double si_value = 0;
    switch (from) {
        case WeightUnit::KILOGRAM:            si_value = value; break;
        case WeightUnit::QUECTOGRAM:          si_value = fizmo::converters::mass::quectogram_to_si(value); break;
        case WeightUnit::RONTOGRAM:           si_value = fizmo::converters::mass::rontogram_to_si(value); break;
        case WeightUnit::YOCTOGRAM:           si_value = fizmo::converters::mass::yoctogram_to_si(value); break;
        case WeightUnit::ZEPTOGRAM:           si_value = fizmo::converters::mass::zeptogram_to_si(value); break;
        case WeightUnit::ATTOGRAM:            si_value = fizmo::converters::mass::attogram_to_si(value); break;
        case WeightUnit::FEMTOGRAM:           si_value = fizmo::converters::mass::femtogram_to_si(value); break;
        case WeightUnit::PICOGRAM:            si_value = fizmo::converters::mass::picogram_to_si(value); break;
        case WeightUnit::NANOGRAM:            si_value = fizmo::converters::mass::nanogram_to_si(value); break;
        case WeightUnit::MICROGRAM:           si_value = fizmo::converters::mass::microgram_to_si(value); break;
        case WeightUnit::MILLIGRAM:           si_value = fizmo::converters::mass::milligram_to_si(value); break;
        case WeightUnit::CENTIGRAM:           si_value = fizmo::converters::mass::centigram_to_si(value); break;
        case WeightUnit::DECIGRAM:            si_value = fizmo::converters::mass::decigram_to_si(value); break;
        case WeightUnit::GRAM:                si_value = fizmo::converters::mass::gram_to_si(value); break;
        case WeightUnit::DECAGRAM:            si_value = fizmo::converters::mass::decagram_to_si(value); break;
        case WeightUnit::HECTOGRAM:           si_value = fizmo::converters::mass::hectogram_to_si(value); break;
        case WeightUnit::MEGAGRAM:            si_value = fizmo::converters::mass::megagram_to_si(value); break;
        case WeightUnit::GIGAGRAM:            si_value = fizmo::converters::mass::gigagram_to_si(value); break;
        case WeightUnit::TERAGRAM:            si_value = fizmo::converters::mass::teragram_to_si(value); break;
        case WeightUnit::PETAGRAM:            si_value = fizmo::converters::mass::petagram_to_si(value); break;
        case WeightUnit::EXAGRAM:             si_value = fizmo::converters::mass::exagram_to_si(value); break;
        case WeightUnit::ZETTAGRAM:           si_value = fizmo::converters::mass::zettagram_to_si(value); break;
        case WeightUnit::YOTTAGRAM:           si_value = fizmo::converters::mass::yottagram_to_si(value); break;
        case WeightUnit::RONNAGRAM:           si_value = fizmo::converters::mass::ronnagram_to_si(value); break;
        case WeightUnit::QUETTAGRAM:          si_value = fizmo::converters::mass::quettagram_to_si(value); break;
        case WeightUnit::METRIC_TON:          si_value = fizmo::converters::mass::metric_ton_to_si(value); break;
        case WeightUnit::TONNE:               si_value = fizmo::converters::mass::tonne_to_si(value); break;
        case WeightUnit::QUINTAL:             si_value = fizmo::converters::mass::quintal_to_si(value); break;
        case WeightUnit::GRAIN:               si_value = fizmo::converters::mass::grain_to_si(value); break;
        case WeightUnit::DRAM:                si_value = fizmo::converters::mass::dram_to_si(value); break;
        case WeightUnit::OUNCE:               si_value = fizmo::converters::mass::ounce_to_si(value); break;
        case WeightUnit::POUND:               si_value = fizmo::converters::mass::pound_to_si(value); break;
        case WeightUnit::STONE:               si_value = fizmo::converters::mass::stone_to_si(value); break;
        case WeightUnit::QUARTER_US:          si_value = fizmo::converters::mass::quarter_us_to_si(value); break;
        case WeightUnit::QUARTER_UK:          si_value = fizmo::converters::mass::quarter_uk_to_si(value); break;
        case WeightUnit::HUNDREDWEIGHT_US:    si_value = fizmo::converters::mass::hundredweight_us_to_si(value); break;
        case WeightUnit::HUNDREDWEIGHT_UK:    si_value = fizmo::converters::mass::hundredweight_uk_to_si(value); break;
        case WeightUnit::CENTAL:              si_value = fizmo::converters::mass::cental_to_si(value); break;
        case WeightUnit::TON_US:              si_value = fizmo::converters::mass::ton_us_to_si(value); break;
        case WeightUnit::TON_UK:              si_value = fizmo::converters::mass::ton_uk_to_si(value); break;
        case WeightUnit::TROY_GRAIN:          si_value = fizmo::converters::mass::troy_grain_to_si(value); break;
        case WeightUnit::PENNYWEIGHT:         si_value = fizmo::converters::mass::pennyweight_to_si(value); break;
        case WeightUnit::TROY_OUNCE:          si_value = fizmo::converters::mass::troy_ounce_to_si(value); break;
        case WeightUnit::TROY_POUND:          si_value = fizmo::converters::mass::troy_pound_to_si(value); break;
        case WeightUnit::SCRUPLE:             si_value = fizmo::converters::mass::scruple_to_si(value); break;
        case WeightUnit::DRACHM:              si_value = fizmo::converters::mass::drachm_to_si(value); break;
        case WeightUnit::APOTHECARIES_OUNCE:  si_value = fizmo::converters::mass::apothecaries_ounce_to_si(value); break;
        case WeightUnit::APOTHECARIES_POUND:  si_value = fizmo::converters::mass::apothecaries_pound_to_si(value); break;
        case WeightUnit::AMU_CHEMISTRY:       si_value = fizmo::converters::mass::amu_chemistry_to_si(value); break;
        case WeightUnit::AMU_PHYSICS:         si_value = fizmo::converters::mass::amu_physics_to_si(value); break;
        case WeightUnit::DALTON:              si_value = fizmo::converters::mass::dalton_to_si(value); break;
        case WeightUnit::UMU:                 si_value = fizmo::converters::mass::unified_mass_unit_to_si(value); break;
        case WeightUnit::PLANCK_MASS:         si_value = fizmo::converters::mass::planck_mass_to_si(value); break;
        case WeightUnit::SLUG:                si_value = fizmo::converters::mass::slug_to_si(value); break;
        case WeightUnit::CARAT:               si_value = fizmo::converters::mass::carat_to_si(value); break;
        case WeightUnit::POINT:               si_value = fizmo::converters::mass::point_to_si(value); break;
        case WeightUnit::PEARL_GRAIN:         si_value = fizmo::converters::mass::pearl_grain_to_si(value); break;
        case WeightUnit::KIP:                 si_value = fizmo::converters::mass::kip_to_si(value); break;
        case WeightUnit::GAMMA:               si_value = fizmo::converters::mass::gamma_to_si(value); break;
    }

    // Then convert from SI to target unit
    switch (to) {
        case WeightUnit::KILOGRAM:            return si_value;
        case WeightUnit::QUECTOGRAM:          return fizmo::converters::mass::si_to_quectogram(si_value);
        case WeightUnit::RONTOGRAM:           return fizmo::converters::mass::si_to_rontogram(si_value);
        case WeightUnit::YOCTOGRAM:           return fizmo::converters::mass::si_to_yoctogram(si_value);
        case WeightUnit::ZEPTOGRAM:           return fizmo::converters::mass::si_to_zeptogram(si_value);
        case WeightUnit::ATTOGRAM:            return fizmo::converters::mass::si_to_attogram(si_value);
        case WeightUnit::FEMTOGRAM:           return fizmo::converters::mass::si_to_femtogram(si_value);
        case WeightUnit::PICOGRAM:            return fizmo::converters::mass::si_to_picogram(si_value);
        case WeightUnit::NANOGRAM:            return fizmo::converters::mass::si_to_nanogram(si_value);
        case WeightUnit::MICROGRAM:           return fizmo::converters::mass::si_to_microgram(si_value);
        case WeightUnit::MILLIGRAM:           return fizmo::converters::mass::si_to_milligram(si_value);
        case WeightUnit::CENTIGRAM:           return fizmo::converters::mass::si_to_centigram(si_value);
        case WeightUnit::DECIGRAM:            return fizmo::converters::mass::si_to_decigram(si_value);
        case WeightUnit::GRAM:                return fizmo::converters::mass::si_to_gram(si_value);
        case WeightUnit::DECAGRAM:            return fizmo::converters::mass::si_to_decagram(si_value);
        case WeightUnit::HECTOGRAM:           return fizmo::converters::mass::si_to_hectogram(si_value);
        case WeightUnit::MEGAGRAM:            return fizmo::converters::mass::si_to_megagram(si_value);
        case WeightUnit::GIGAGRAM:            return fizmo::converters::mass::si_to_gigagram(si_value);
        case WeightUnit::TERAGRAM:            return fizmo::converters::mass::si_to_teragram(si_value);
        case WeightUnit::PETAGRAM:            return fizmo::converters::mass::si_to_petagram(si_value);
        case WeightUnit::EXAGRAM:             return fizmo::converters::mass::si_to_exagram(si_value);
        case WeightUnit::ZETTAGRAM:           return fizmo::converters::mass::si_to_zettagram(si_value);
        case WeightUnit::YOTTAGRAM:           return fizmo::converters::mass::si_to_yottagram(si_value);
        case WeightUnit::RONNAGRAM:           return fizmo::converters::mass::si_to_ronnagram(si_value);
        case WeightUnit::QUETTAGRAM:          return fizmo::converters::mass::si_to_quettagram(si_value);
        case WeightUnit::METRIC_TON:          return fizmo::converters::mass::si_to_metric_ton(si_value);
        case WeightUnit::TONNE:               return fizmo::converters::mass::si_to_tonne(si_value);
        case WeightUnit::QUINTAL:             return fizmo::converters::mass::si_to_quintal(si_value);
        case WeightUnit::GRAIN:               return fizmo::converters::mass::si_to_grain(si_value);
        case WeightUnit::DRAM:                return fizmo::converters::mass::si_to_dram(si_value);
        case WeightUnit::OUNCE:               return fizmo::converters::mass::si_to_ounce(si_value);
        case WeightUnit::POUND:               return fizmo::converters::mass::si_to_pound(si_value);
        case WeightUnit::STONE:               return fizmo::converters::mass::si_to_stone(si_value);
        case WeightUnit::QUARTER_US:          return fizmo::converters::mass::si_to_quarter_us(si_value);
        case WeightUnit::QUARTER_UK:          return fizmo::converters::mass::si_to_quarter_uk(si_value);
        case WeightUnit::HUNDREDWEIGHT_US:    return fizmo::converters::mass::si_to_hundredweight_us(si_value);
        case WeightUnit::HUNDREDWEIGHT_UK:    return fizmo::converters::mass::si_to_hundredweight_uk(si_value);
        case WeightUnit::CENTAL:              return fizmo::converters::mass::si_to_cental(si_value);
        case WeightUnit::TON_US:              return fizmo::converters::mass::si_to_ton_us(si_value);
        case WeightUnit::TON_UK:              return fizmo::converters::mass::si_to_ton_uk(si_value);
        case WeightUnit::TROY_GRAIN:          return fizmo::converters::mass::si_to_troy_grain(si_value);
        case WeightUnit::PENNYWEIGHT:         return fizmo::converters::mass::si_to_pennyweight(si_value);
        case WeightUnit::TROY_OUNCE:          return fizmo::converters::mass::si_to_troy_ounce(si_value);
        case WeightUnit::TROY_POUND:          return fizmo::converters::mass::si_to_troy_pound(si_value);
        case WeightUnit::SCRUPLE:             return fizmo::converters::mass::si_to_scruple(si_value);
        case WeightUnit::DRACHM:              return fizmo::converters::mass::si_to_drachm(si_value);
        case WeightUnit::APOTHECARIES_OUNCE:  return fizmo::converters::mass::si_to_apothecaries_ounce(si_value);
        case WeightUnit::APOTHECARIES_POUND:  return fizmo::converters::mass::si_to_apothecaries_pound(si_value);
        case WeightUnit::AMU_CHEMISTRY:       return fizmo::converters::mass::si_to_amu_chemistry(si_value);
        case WeightUnit::AMU_PHYSICS:         return fizmo::converters::mass::si_to_amu_physics(si_value);
        case WeightUnit::DALTON:              return fizmo::converters::mass::si_to_dalton(si_value);
        case WeightUnit::UMU:                 return fizmo::converters::mass::si_to_unified_mass_unit(si_value);
        case WeightUnit::PLANCK_MASS:         return fizmo::converters::mass::si_to_planck_mass(si_value);
        case WeightUnit::SLUG:                return fizmo::converters::mass::si_to_slug(si_value);
        case WeightUnit::CARAT:               return fizmo::converters::mass::si_to_carat(si_value);
        case WeightUnit::POINT:               return fizmo::converters::mass::si_to_point(si_value);
        case WeightUnit::PEARL_GRAIN:         return fizmo::converters::mass::si_to_pearl_grain(si_value);
        case WeightUnit::KIP:                 return fizmo::converters::mass::si_to_kip(si_value);
        case WeightUnit::GAMMA:               return fizmo::converters::mass::si_to_gamma(si_value);
    }
    return value; 
}

constexpr long double convert_velocity(const long double value, const VelocityUnit from, const VelocityUnit to) noexcept {
    long double distance_in_meters = convert_distance(1.0L, from.distance, DistanceUnit::METER);
    long double time_in_seconds = convert_time(1.0L, from.time, TimeUnit::SECOND);
    long double si_value = value * distance_in_meters / time_in_seconds * from.special_conversion_factor;
    long double target_distance = convert_distance(1.0L, DistanceUnit::METER, to.distance);
    long double target_time = convert_time(1.0L, TimeUnit::SECOND, to.time);
    return si_value * target_distance / (target_time * to.special_conversion_factor);
}

constexpr long double calculate_mach_1(
    const long double temp, 
    const TemperatureUnit temp_unit, 
    const long double gas_constant = 287.05, 
    const long double abiadic_index = 1.4
) noexcept {
    long double T = convert_temperature(temp, temp_unit, TemperatureUnit::KELVIN);
    T = fizmo::max_constexpr(T, fizmo::constants::TYPE_EPSILON<long double>);
    const long double gam = fizmo::max_constexpr(abiadic_index, fizmo::constants::TYPE_EPSILON<long double>);
    const long double G = fizmo::max_constexpr(gas_constant, fizmo::constants::TYPE_EPSILON<long double>);
    return fizmo::math::sqrt_constexpr(T * gam * G);
}

constexpr VelocityUnit VelocityUnit::mach(
    const long double temp, 
    const TemperatureUnit temp_unit, 
    const long double gas_constant, 
    const long double abiadic_index
) noexcept { 
    return Special(calculate_mach_1(temp, temp_unit, gas_constant, abiadic_index)); 
}

constexpr long double convert_acceleration(const long double value, const AccelerationUnit from, const AccelerationUnit to) noexcept {
    long double velocity_factor = convert_velocity(1.0L, from.velocity, VelocityUnit::SI());
    long double time_in_seconds = convert_time(1.0L, from.time, TimeUnit::SECOND);
    long double si_value = value * velocity_factor / time_in_seconds * from.special_conversion_factor;
    long double target_velocity_factor = convert_velocity(1.0L, VelocityUnit::SI(), to.velocity);
    long double target_time = convert_time(1.0L, TimeUnit::SECOND, to.time);
    return si_value * target_velocity_factor / (target_time * to.special_conversion_factor);
}

constexpr long double convert_angular_velocity(const long double value, const AngularVelocityUnit from, const AngularVelocityUnit to) noexcept {
    long double angle_in_radians = (from.angle == AngleUnit::DEGREES) ? value * fizmo::constants::PI_180<long double> : value;
    long double time_in_seconds = convert_time(1.0L, from.time, TimeUnit::SECOND);
    long double si_value = angle_in_radians / time_in_seconds * from.special_conversion_factor;
    long double target_angle = (to.angle == AngleUnit::DEGREES) ? si_value * fizmo::constants::RECIPROCAL_PI_180<long double> : si_value;
    long double target_time = convert_time(1.0L, TimeUnit::SECOND, to.time);
    return target_angle * target_time / to.special_conversion_factor;
}

constexpr long double convert_area(const long double value, const AreaUnit from, const AreaUnit to) noexcept {
    long double length_in_meters = convert_distance(1.0L, from.length, DistanceUnit::METER);
    long double width_in_meters = convert_distance(1.0L, from.width, DistanceUnit::METER);
    long double si_value = value * length_in_meters * width_in_meters * from.special_conversion_factor;
    long double target_length = convert_distance(1.0L, DistanceUnit::METER, to.length);
    long double target_width = convert_distance(1.0L, DistanceUnit::METER, to.width);
    return si_value * target_length * target_width / to.special_conversion_factor;
}

constexpr long double convert_density_2d(const long double value, const DensityUnit_2D from, const DensityUnit_2D to) noexcept {
    long double mass_in_kg = convert_weight(1.0L, from.mass, WeightUnit::KILOGRAM);
    long double area_in_m2 = convert_area(1.0L, from.area, AreaUnit::SI());
    long double si_value = value * mass_in_kg / area_in_m2 * from.special_conversion_factor;
    long double target_mass = convert_weight(1.0L, WeightUnit::KILOGRAM, to.mass);
    long double target_area = convert_area(1.0L, AreaUnit::SI(), to.area);
    return si_value * target_mass / (target_area * to.special_conversion_factor);
}

constexpr long double convert_volume(const long double value, const VolumeUnit from, const VolumeUnit to) noexcept {
    long double length_in_meters = convert_distance(1.0L, from.length, DistanceUnit::METER);
    long double width_in_meters = convert_distance(1.0L, from.width, DistanceUnit::METER);
    long double height_in_meters = convert_distance(1.0L, from.height, DistanceUnit::METER);
    long double si_value = value * length_in_meters * width_in_meters * height_in_meters * from.special_conversion_factor;
    long double target_length = convert_distance(1.0L, DistanceUnit::METER, to.length);
    long double target_width = convert_distance(1.0L, DistanceUnit::METER, to.width);
    long double target_height = convert_distance(1.0L, DistanceUnit::METER, to.height);
    return si_value * target_length * target_width * target_height / to.special_conversion_factor;
}

constexpr long double convert_density_3d(const long double value, const DensityUnit_3D from, const DensityUnit_3D to) noexcept {
    long double mass_in_kg = convert_weight(1.0L, from.mass, WeightUnit::KILOGRAM);
    long double volume_in_m3 = convert_volume(1.0L, from.volume, VolumeUnit::SI());
    long double si_value = value * mass_in_kg / volume_in_m3 * from.special_conversion_factor;
    long double target_mass = convert_weight(1.0L, WeightUnit::KILOGRAM, to.mass);
    long double target_volume = convert_volume(1.0L, VolumeUnit::SI(), to.volume);
    return si_value * target_mass / (target_volume * to.special_conversion_factor);
}

constexpr long double convert_momentum(const long double value, const MomentumUnit from, const MomentumUnit to) noexcept {
    long double mass_in_kg = convert_weight(1.0L, from.mass, WeightUnit::KILOGRAM);
    long double velocity_in_mps = convert_velocity(1.0L, from.velocity, VelocityUnit::SI());
    long double si_value = value * mass_in_kg * velocity_in_mps * from.special_conversion_factor;
    long double target_mass = convert_weight(1.0L, WeightUnit::KILOGRAM, to.mass);
    long double target_velocity = convert_velocity(1.0L, VelocityUnit::SI(), to.velocity);
    return si_value * target_mass * target_velocity / to.special_conversion_factor;
}

constexpr long double convert_force(const long double value, const ForceUnit from, const ForceUnit to) noexcept {
    long double mass_in_kg = convert_weight(1.0L, from.mass, WeightUnit::KILOGRAM);
    long double accel_in_si = convert_acceleration(1.0L, from.acceleration, AccelerationUnit::SI());
    long double si_value = value * mass_in_kg * accel_in_si * from.special_conversion_factor;
    long double target_mass = convert_weight(1.0L, WeightUnit::KILOGRAM, to.mass);
    long double target_accel = convert_acceleration(1.0L, AccelerationUnit::SI(), to.acceleration);
    return si_value * target_mass * target_accel / to.special_conversion_factor;
}

constexpr long double convert_power(const long double value, const PowerUnit from, const PowerUnit to) noexcept {
    long double si_value = 0;
    
    if (from.composition == PowerUnit::Composition::ELECTRICAL) {
        long double voltage_in_volts = convert_voltage(1.0L, from.voltage, VoltageUnit::VOLT);
        long double current_in_amperes = convert_current(1.0L, from.current, ElectricalCurrentUnit::AMPERE);
        si_value = value * voltage_in_volts * current_in_amperes * from.special_conversion_factor;
    } else { 
        long double force_in_newtons = convert_force(1.0L, from.force, ForceUnit::SI());
        long double velocity_in_mps = convert_velocity(1.0L, from.velocity, VelocityUnit::SI());
        si_value = value * force_in_newtons * velocity_in_mps * from.special_conversion_factor;
    }
    
    if (to.composition == PowerUnit::Composition::ELECTRICAL) {
        long double target_voltage = convert_voltage(1.0L, VoltageUnit::VOLT, to.voltage);
        long double target_current = convert_current(1.0L, ElectricalCurrentUnit::AMPERE, to.current);
        return si_value * target_voltage * target_current / to.special_conversion_factor;
    } else {
        long double target_force = convert_force(1.0L, ForceUnit::SI(), to.force);
        long double target_velocity = convert_velocity(1.0L, VelocityUnit::SI(), to.velocity);
        return si_value * target_force * target_velocity / to.special_conversion_factor;
    }
}

constexpr long double convert_energy(const long double value, const EnergyUnit from, const EnergyUnit to) noexcept {
    long double si_value = 0.0L;

    if (from.representation == EnergyUnit::Representation::POWER_TIME) {
        long double power_in_watts = convert_power(1.0L, from.power, PowerUnit::SI_electrical());
        long double time_in_seconds = convert_time(1.0L, from.time, TimeUnit::SECOND);
        si_value = value * power_in_watts * time_in_seconds * from.special_conversion_factor;
    } else { 
        long double force_in_newtons = convert_force(1.0L, from.force, ForceUnit::SI());
        long double distance_in_meters = convert_distance(1.0L, from.distance, DistanceUnit::METER);
        si_value = value * force_in_newtons * distance_in_meters * from.special_conversion_factor;
    }
    
    if (to.representation == EnergyUnit::Representation::POWER_TIME) {
        long double target_power = convert_power(1.0L, PowerUnit::SI_electrical(), to.power);
        long double target_time = convert_time(1.0L, TimeUnit::SECOND, to.time);
        return si_value * target_power * target_time / to.special_conversion_factor;
    } else { 
        long double target_force = convert_force(1.0L, ForceUnit::SI(), to.force);
        long double target_distance = convert_distance(1.0L, DistanceUnit::METER, to.distance);
        return si_value * target_force * target_distance / to.special_conversion_factor;
    }
}

constexpr long double convert_capacitance(const long double value, const CapacitanceUnit from, const CapacitanceUnit to) noexcept {
    long double charge_in_coulombs = convert_electrical_charge(1.0L, from.charge, ElectricalChargeUnit::COULOMB);
    long double voltage_in_volts = convert_voltage(1.0L, from.voltage, VoltageUnit::VOLT);
    long double si_value = value * charge_in_coulombs / voltage_in_volts * from.special_conversion_factor;
    long double target_charge = convert_electrical_charge(1.0L, ElectricalChargeUnit::COULOMB, to.charge);
    long double target_voltage = convert_voltage(1.0L, VoltageUnit::VOLT, to.voltage);
    return si_value * target_charge / (target_voltage * to.special_conversion_factor);
}

constexpr long double convert_torque(const long double value, const TorqueUnit from, const TorqueUnit to) noexcept {
    long double force_in_newtons = convert_force(1.0L, from.force, ForceUnit::SI());
    long double distance_in_meters = convert_distance(1.0L, from.distance, DistanceUnit::METER);
    long double si_value = value * force_in_newtons * distance_in_meters * from.special_conversion_factor;
    long double target_force = convert_force(1.0L, ForceUnit::SI(), to.force);
    long double target_distance = convert_distance(1.0L, DistanceUnit::METER, to.distance);
    return si_value * target_force * target_distance / to.special_conversion_factor;
}

constexpr long double convert_impulse(const long double value, const ImpulseUnit from, const ImpulseUnit to) noexcept {
    long double force_in_newtons = convert_force(1.0L, from.force, ForceUnit::SI());
    long double time_in_seconds = convert_time(1.0L, from.time, TimeUnit::SECOND);
    long double si_value = value * force_in_newtons * time_in_seconds * from.special_conversion_factor;
    long double target_force = convert_force(1.0L, ForceUnit::SI(), to.force);
    long double target_time = convert_time(1.0L, TimeUnit::SECOND, to.time);
    return si_value * target_force * target_time / to.special_conversion_factor;
}

constexpr long double convert_pressure(const long double value, const PressureUnit from, const PressureUnit to) noexcept {
    long double force_in_newtons = convert_force(1.0L, from.force, ForceUnit::SI());
    long double area_in_meters = convert_area(1.0L, from.area, AreaUnit::SI());
    long double si_value = value * force_in_newtons / area_in_meters * from.special_conversion_factor;
    long double target_force = convert_force(1.0L, ForceUnit::SI(), to.force);
    long double target_area = convert_area(1.0L, AreaUnit::SI(), to.area);
    return si_value * target_force / (target_area * to.special_conversion_factor);
}

constexpr long double convert_specific_heat_capacity(
    const long double value,
    const SpecificHeatCapacityUnit from,
    const SpecificHeatCapacityUnit to
) noexcept {
    long double e_si = convert_energy(1.0L, from.energy, EnergyUnit::joule());
    long double m_si = convert_weight(1.0L, from.mass, WeightUnit::KILOGRAM);
    long double t_si = convert_temperature(1.0L, from.temperature, TemperatureUnit::KELVIN);
    long double si_value = value * (e_si / (m_si * t_si)) * from.special_conversion_factor;
    long double e_t = convert_energy(1.0L, EnergyUnit::joule(), to.energy);
    long double m_t = convert_weight(1.0L, WeightUnit::KILOGRAM, to.mass);
    long double t_t = convert_temperature(1.0L, TemperatureUnit::KELVIN, to.temperature);
    return si_value * (e_t / (m_t * t_t)) / to.special_conversion_factor;
}

constexpr long double convert_volumetric_heat_capacity(
    const long double value,
    const VolumetricHeatCapacityUnit from,
    const VolumetricHeatCapacityUnit to
) noexcept {
    long double e_si = convert_energy(1.0L, from.energy, EnergyUnit::joule());
    long double v_si = convert_volume(1.0L, from.volume, VolumeUnit::SI());
    long double t_si = convert_temperature(1.0L, from.temperature, TemperatureUnit::KELVIN);
    long double si_value = value * (e_si / (v_si * t_si)) * from.special_conversion_factor;
    long double e_t = convert_energy(1.0L, EnergyUnit::joule(), to.energy);
    long double v_t = convert_volume(1.0L, VolumeUnit::SI(), to.volume);
    long double t_t = convert_temperature(1.0L, TemperatureUnit::KELVIN, to.temperature);
    return si_value * (e_t / (v_t * t_t)) / to.special_conversion_factor;
}

constexpr long double convert_dosage(
    const long double value,
    const DosageUnit from,
    const DosageUnit to
) noexcept {
    long double si_value = 0.0L;

    if (from.type == DosageUnit::Type::VOLUME_PER_WEIGHT) {
        long double vol_si = convert_volume(1.0L, from.volume, VolumeUnit::SI());
        long double w_si   = convert_weight(1.0L, from.weight_denominator, WeightUnit::KILOGRAM);
        si_value = value * (vol_si / w_si) * from.special_conversion_factor;
    } else {
        long double w_num_si = convert_weight(1.0L, from.weight_numerator, WeightUnit::KILOGRAM);
        long double w_den_si = convert_weight(1.0L, from.weight_denominator, WeightUnit::KILOGRAM);
        si_value = value * (w_num_si / w_den_si) * from.special_conversion_factor;
    }

    if (to.type == DosageUnit::Type::VOLUME_PER_WEIGHT) {
        long double vol_t = convert_volume(1.0L, VolumeUnit::SI(), to.volume);
        long double w_t   = convert_weight(1.0L, WeightUnit::KILOGRAM, to.weight_denominator);
        return si_value * (vol_t / w_t) / to.special_conversion_factor;
    } else {
        long double w_num_t = convert_weight(1.0L, WeightUnit::KILOGRAM, to.weight_numerator);
        long double w_den_t = convert_weight(1.0L, WeightUnit::KILOGRAM, to.weight_denominator);
        return si_value * (w_num_t / w_den_t) / to.special_conversion_factor;
    }
}

constexpr long double VelocityUnit::conversion_factor(const VelocityUnit to) const noexcept { return convert_velocity(1.0L, *this, to); }
constexpr long double VelocityUnit::conversion_factor(const VelocityUnit from, const VelocityUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double AccelerationUnit::conversion_factor(const AccelerationUnit to) const noexcept { return convert_acceleration(1.0L, *this, to); }
constexpr long double AccelerationUnit::conversion_factor(const AccelerationUnit from, const AccelerationUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double AngularVelocityUnit::conversion_factor(const AngularVelocityUnit to) const noexcept { return convert_angular_velocity(1.0L, *this, to); }
constexpr long double AngularVelocityUnit::conversion_factor(const AngularVelocityUnit from, const AngularVelocityUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double AreaUnit::conversion_factor(const AreaUnit to) const noexcept { return convert_area(1.0L, *this, to); }
constexpr long double AreaUnit::conversion_factor(const AreaUnit from, const AreaUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double DensityUnit_2D::conversion_factor(const DensityUnit_2D to) const noexcept { return convert_density_2d(1.0L, *this, to); }
constexpr long double DensityUnit_2D::conversion_factor(const DensityUnit_2D from, const DensityUnit_2D to) noexcept { return from.conversion_factor(to); }

constexpr long double VolumeUnit::conversion_factor(const VolumeUnit to) const noexcept { return convert_volume(1.0L, *this, to); }
constexpr long double VolumeUnit::conversion_factor(const VolumeUnit from, const VolumeUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double DensityUnit_3D::conversion_factor(const DensityUnit_3D to) const noexcept { return convert_density_3d(1.0L, *this, to); }
constexpr long double DensityUnit_3D::conversion_factor(const DensityUnit_3D from, const DensityUnit_3D to) noexcept { return from.conversion_factor(to); }

constexpr long double MomentumUnit::conversion_factor(const MomentumUnit to) const noexcept { return convert_momentum(1.0L, *this, to); }
constexpr long double MomentumUnit::conversion_factor(const MomentumUnit from, const MomentumUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double PowerUnit::conversion_factor(const PowerUnit to) const noexcept { return convert_power(1.0L, *this, to); }
constexpr long double PowerUnit::conversion_factor(const PowerUnit from, const PowerUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double EnergyUnit::conversion_factor(const EnergyUnit to) const noexcept { return convert_energy(1.0L, *this, to); }
constexpr long double EnergyUnit::conversion_factor(const EnergyUnit from, const EnergyUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double CapacitanceUnit::conversion_factor(const CapacitanceUnit to) const noexcept { return convert_capacitance(1.0L, *this, to); }
constexpr long double CapacitanceUnit::conversion_factor(const CapacitanceUnit from, const CapacitanceUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double ForceUnit::conversion_factor(const ForceUnit to) const noexcept { return convert_force(1.0L, *this, to); }
constexpr long double ForceUnit::conversion_factor(const ForceUnit from, const ForceUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double TorqueUnit::conversion_factor(const TorqueUnit to) const noexcept { return convert_torque(1.0L, *this, to); }
constexpr long double TorqueUnit::conversion_factor(const TorqueUnit from, const TorqueUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double ImpulseUnit::conversion_factor(const ImpulseUnit to) const noexcept { return convert_impulse(1.0L, *this, to); }
constexpr long double ImpulseUnit::conversion_factor(const ImpulseUnit from, const ImpulseUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double PressureUnit::conversion_factor(const PressureUnit to) const noexcept { return convert_pressure(1.0L, *this, to); }
constexpr long double PressureUnit::conversion_factor(const PressureUnit from, const PressureUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double SpecificHeatCapacityUnit::conversion_factor(const SpecificHeatCapacityUnit to) const noexcept { return convert_specific_heat_capacity(1.0L, *this, to); }
constexpr long double SpecificHeatCapacityUnit::conversion_factor(const SpecificHeatCapacityUnit from, const SpecificHeatCapacityUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double VolumetricHeatCapacityUnit::conversion_factor(const VolumetricHeatCapacityUnit to) const noexcept { return convert_volumetric_heat_capacity(1.0L, *this, to); }
constexpr long double VolumetricHeatCapacityUnit::conversion_factor(const VolumetricHeatCapacityUnit from, const VolumetricHeatCapacityUnit to) noexcept { return from.conversion_factor(to); }

constexpr long double DosageUnit::conversion_factor(const DosageUnit to) const noexcept { return convert_dosage(1.0L, *this, to); }
constexpr long double DosageUnit::conversion_factor(const DosageUnit from, const DosageUnit to) noexcept { return from.conversion_factor(to); }

template <typename Unit, typename = typename std::enable_if<is_fizmo_unit_v<Unit>>::type>
constexpr long double convert_unit(
    const long double value,
    const Unit from,
    const Unit to
) noexcept;

template <typename Unit, typename = typename std::enable_if<is_fizmo_unit_v<Unit>>::type>
constexpr long double convert_to_si(
    const long double value,
    const Unit from
) noexcept;

template <typename Unit, typename = typename std::enable_if<is_fizmo_unit_v<Unit>>::type>
constexpr long double convert_from_si(
    const long double value,
    const Unit to
) noexcept;

} // namespace units
} // namespace fizmo

#endif // UNIT_CONVERTER_FUNCTIONS_HPP