#ifndef DISPATCH_UNIT_CONVERTERS_HPP
#define DISPATCH_UNIT_CONVERTERS_HPP

#include "unit_converters.hpp"

namespace fizmo {
namespace units {

#define FIZMO_DEFINE_UNIT_DISPATCH(UnitType, ConvertFunc, SIUnit)               \
    template <>                                                                 \
    constexpr long double convert_unit<UnitType>(                               \
        const long double value,                                                \
        const UnitType from,                                                    \
        const UnitType to                                                       \
    ) noexcept {                                                                \
        return ConvertFunc(value, from, to);                                    \
    }

#define FIZMO_DEFINE_TO_SI_DISPATCH(UnitType, ConvertFunc, SIUnit)             \
    template <>                                                                 \
    constexpr long double convert_to_si<UnitType>(                              \
        const long double value,                                                \
        const UnitType from                                                     \
    ) noexcept {                                                                \
        return ConvertFunc(value, from, SIUnit);                                \
    }

#define FIZMO_DEFINE_FROM_SI_DISPATCH(UnitType, ConvertFunc, SIUnit)           \
    template <>                                                                 \
    constexpr long double convert_from_si<UnitType>(                            \
        const long double value,                                                \
        const UnitType to                                                       \
    ) noexcept {                                                                \
        return ConvertFunc(value, SIUnit, to);                                  \
    }

#define FIZMO_DEFINE_ALL_DISPATCH(UnitType, ConvertFunc, SIUnit)               \
    FIZMO_DEFINE_UNIT_DISPATCH(UnitType, ConvertFunc, SIUnit);                 \
    FIZMO_DEFINE_TO_SI_DISPATCH(UnitType, ConvertFunc, SIUnit);                \
    FIZMO_DEFINE_FROM_SI_DISPATCH(UnitType, ConvertFunc, SIUnit)

FIZMO_DEFINE_ALL_DISPATCH(ElectricalChargeUnit,        convert_electrical_charge,         ElectricalChargeUnit::COULOMB);
FIZMO_DEFINE_ALL_DISPATCH(ElectricalCurrentUnit,       convert_current,                   ElectricalCurrentUnit::AMPERE);
FIZMO_DEFINE_ALL_DISPATCH(VoltageUnit,                 convert_voltage,                   VoltageUnit::VOLT);
FIZMO_DEFINE_ALL_DISPATCH(ResistanceUnit,              convert_resistance,                ResistanceUnit::OHM);
FIZMO_DEFINE_ALL_DISPATCH(FrequencyUnit,               convert_frequency,                 FrequencyUnit::HERTZ);
FIZMO_DEFINE_ALL_DISPATCH(RadiationUnit,               convert_radiation,                 RadiationUnit::GRAY);
FIZMO_DEFINE_ALL_DISPATCH(DistanceUnit,                convert_distance,                  DistanceUnit::METER);
FIZMO_DEFINE_ALL_DISPATCH(TemperatureUnit,             convert_temperature,               TemperatureUnit::KELVIN);
FIZMO_DEFINE_ALL_DISPATCH(TimeUnit,                    convert_time,                      TimeUnit::SECOND);
FIZMO_DEFINE_ALL_DISPATCH(WeightUnit,                  convert_weight,                    WeightUnit::KILOGRAM);

FIZMO_DEFINE_ALL_DISPATCH(VelocityUnit,                convert_velocity,                  VelocityUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(AccelerationUnit,            convert_acceleration,              AccelerationUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(AngularVelocityUnit,         convert_angular_velocity,          AngularVelocityUnit::SI());

FIZMO_DEFINE_ALL_DISPATCH(AreaUnit,                    convert_area,                      AreaUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(VolumeUnit,                  convert_volume,                    VolumeUnit::SI());

FIZMO_DEFINE_ALL_DISPATCH(DensityUnit_2D,              convert_density_2d,                DensityUnit_2D::SI());
FIZMO_DEFINE_ALL_DISPATCH(DensityUnit_3D,              convert_density_3d,                DensityUnit_3D::SI());

FIZMO_DEFINE_ALL_DISPATCH(MomentumUnit,                convert_momentum,                  MomentumUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(ForceUnit,                   convert_force,                     ForceUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(PowerUnit,                   convert_power,                     PowerUnit::SI_electrical());
FIZMO_DEFINE_ALL_DISPATCH(EnergyUnit,                  convert_energy,                    EnergyUnit::SI_electrical());

FIZMO_DEFINE_ALL_DISPATCH(CapacitanceUnit,             convert_capacitance,               CapacitanceUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(TorqueUnit,                  convert_torque,                    TorqueUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(ImpulseUnit,                 convert_impulse,                   ImpulseUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(PressureUnit,                convert_pressure,                  PressureUnit::SI());

FIZMO_DEFINE_ALL_DISPATCH(SpecificHeatCapacityUnit,    convert_specific_heat_capacity,    SpecificHeatCapacityUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(VolumetricHeatCapacityUnit,  convert_volumetric_heat_capacity,  VolumetricHeatCapacityUnit::SI());
FIZMO_DEFINE_ALL_DISPATCH(DosageUnit,                  convert_dosage,                    DosageUnit::SI_volume_per_weight());


// ============================================================
// CLEANUP
// ============================================================
#undef FIZMO_DEFINE_UNIT_DISPATCH
#undef FIZMO_DEFINE_TO_SI_DISPATCH
#undef FIZMO_DEFINE_FROM_SI_DISPATCH
#undef FIZMO_DEFINE_ALL_DISPATCH

} // namespace units
} // namespace fizmo

#endif // DISPATCH_UNIT_CONVERTERS_HPP