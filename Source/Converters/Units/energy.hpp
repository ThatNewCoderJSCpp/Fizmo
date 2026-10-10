#ifndef FIZMO_UNITS_ENERGY_HPP
#define FIZMO_UNITS_ENERGY_HPP

#include "force.hpp"
#include "distance.hpp"
#include "time.hpp"

namespace fizmo {
namespace units {

namespace power {
    constexpr PowerUnit watt(1.0L);
    FIZMO_SI_PREFIXED(watt, watt);  
    constexpr PowerUnit horsepower         (745.69987158227L);
    constexpr PowerUnit metric_horsepower  (735.49875L);
    constexpr PowerUnit electric_horsepower(746.0L);
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

    constexpr EnergyUnit erg(1e-7L);
    FIZMO_SI_PREFIXED(erg, erg);

    constexpr EnergyUnit calorie(4.1868L);
    FIZMO_SI_PREFIXED(calorie, calorie);
    
    constexpr EnergyUnit food_calorie(4184.0L);

    constexpr EnergyUnit calorie_thermochem(4.184L);
    FIZMO_SI_PREFIXED(calorie_thermochem, calorie_thermochem);

    constexpr EnergyUnit btu  (1055.05585262L);
    constexpr EnergyUnit therm(1.05505585262e8L);

    constexpr EnergyUnit electronvolt(constants::ELECTRON_VOLT<long double>);
    FIZMO_SI_PREFIXED(electronvolt, electronvolt);

    constexpr EnergyUnit foot_pound_force = force::pound_force * distance::foot;   
    constexpr EnergyUnit foot_poundal     = force::poundal     * distance::foot;
    constexpr EnergyUnit inch_pound_force = force::pound_force * distance::inch;
} // namespace energy

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_ENERGY_HPP