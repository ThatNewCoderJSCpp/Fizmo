#ifndef FIZMO_UNITS_ENERGY_HPP
#define FIZMO_UNITS_ENERGY_HPP

#include "force.hpp"
#include "distance.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {

namespace power {
    constexpr PowerUnit watt(exact("1.0"));
    FIZMO_SI_PREFIXED(watt, watt);  
    constexpr PowerUnit horsepower         (ratio(550) * exact_constants::pound * exact_constants::standard_gravity * exact_constants::foot);
    constexpr PowerUnit metric_horsepower  (exact("735.49875"));
    constexpr PowerUnit electric_horsepower(exact("746.0"));
} // namespace power

namespace energy {
    constexpr EnergyUnit joule = force::newton * distance::meter;         
    FIZMO_SI_PREFIXED(joule, joule);                

    constexpr EnergyUnit watt_second = power::watt * time::second;     
    FIZMO_SI_PREFIXED(watt_second, watt_second);

    constexpr EnergyUnit watt_minute = power::watt * time::minute;     
    FIZMO_SI_PREFIXED(watt_minute, watt_minute);

    constexpr EnergyUnit watt_hour = power::watt * time::hour;     
    FIZMO_SI_PREFIXED(watt_hour, watt_hour);

    constexpr EnergyUnit erg(exact("1e-7"));
    FIZMO_SI_PREFIXED(erg, erg);

    constexpr EnergyUnit calorie(exact("4.1868"));
    FIZMO_SI_PREFIXED(calorie, calorie);
    
    constexpr EnergyUnit food_calorie(exact("4184.0"));

    constexpr EnergyUnit calorie_thermochem(exact("4.184"));
    FIZMO_SI_PREFIXED(calorie_thermochem, calorie_thermochem);

    constexpr EnergyUnit btu  (exact("1055.05585262"));
    constexpr EnergyUnit therm(exact("1.05505585262e8"));

    constexpr EnergyUnit electronvolt(exact_constants::elementary_charge);
    FIZMO_SI_PREFIXED(electronvolt, electronvolt);

    constexpr EnergyUnit foot_pound_force = force::pound_force * distance::foot;   
    constexpr EnergyUnit foot_poundal     = force::poundal     * distance::foot;
    constexpr EnergyUnit inch_pound_force = force::pound_force * distance::inch;
} // namespace energy

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ENERGY_HPP