#ifndef UNIT_MANAGEMENT_HPP
#define UNIT_MANAGEMENT_HPP

#include "distance.hpp"
#include "electricity.hpp"
#include "frequency.hpp"
#include "radiation.hpp"
#include "temperature.hpp"
#include "thermal.hpp"
#include "time.hpp"
#include "weight.hpp"

namespace fizmo {
namespace units {

struct VelocityUnit;
struct AccelerationUnit;
struct AngularVelocityUnit;
struct AreaUnit;
struct VolumeUnit;
struct DensityUnit_2D;
struct DensityUnit_3D;
struct MomentumUnit;
struct ForceUnit;
struct PowerUnit;
struct EnergyUnit;
struct CapacitanceUnit;
struct TorqueUnit;
struct ImpulseUnit;
struct PressureUnit;

enum class ElectricalChargeUnit {
    COULOMB, QUECTOCOULOMB, RONTOCOULOMB, YOCTOCOULOMB, ZEPTOCOULOMB,
    ATTOCOULOMB, FEMTOCOULOMB, PICOCOULOMB, NANOCOULOMB, MICROCOULOMB,
    MILLICOULOMB, CENTICOULOMB, DECICOULOMB, DECACOULOMB, HECTOCOULOMB,
    KILOCOULOMB, MEGACOULOMB, GIGACOULOMB, TERACOULOMB, PETACOULOMB,
    EXACOULOMB, ZETTACOULOMB, YOTTACOULOMB, RONNACOULOMB, QUETTACOULOMB,
    AMPERE_HOUR, MILLIAMPERE_HOUR, FARADAY, ELEMENTARY_CHARGE,
    STATCOULOMB, ABCOULOMB, PLANCK_COULOMB
};

enum class ElectricalCurrentUnit {
    AMPERE, QUECTOAMPERE, RONTOAMPERE, YOCTOAMPERE, ZEPTOAMPERE,
    ATTOAMPERE, FEMTOAMPERE, PICOAMPERE, NANOAMPERE, MICROAMPERE,
    MILLIAMPERE, CENTIAMPERE, DECIAMPERE, DECAAMPERE, HECTOAMPERE,
    KILOAMPERE, MEGAAMPERE, GIGAAMPERE, TERAAMPERE, PETAAMPERE,
    EXAAMPERE, ZETTAAMPERE, YOTTAAMPERE, RONNAAMPERE, QUETTAAMPERE,
    STATAMPERE, ABAMPERE, BIOT, PLANCK_AMPERE
};

enum class VoltageUnit {
    VOLT, QUECTOVOLT, RONTOVOLT, YOCTOVOLT, ZEPTOVOLT, ATTOVOLT,
    FEMTOVOLT, PICOVOLT, NANOVOLT, MICROVOLT, MILLIVOLT, CENTIVOLT,
    DECIVOLT, DECAVOLT, HECTOVOLT, KILOVOLT, MEGAVOLT, GIGAVOLT,
    TERAVOLT, PETAVOLT, EXAVOLT, ZETTAVOLT, YOTTAVOLT, RONNAVOLT,
    QUETTAVOLT, STATVOLT, ABVOLT, PLANCK_VOLT
};

enum class ResistanceUnit {
    OHM, QUECTOOHM, RONTOOHM, YOCTOOHM, ZEPTOOHM, ATTOOHM, FEMTOOHM,
    PICOOHM, NANOOHM, MICROOHM, MILLIOHM, CENTIOHM, DECIOHM, DECAOHM,
    HECTOOHM, KILOOHM, MEGAOHM, GIGAOHM, TERAOHM, PETAOHM, EXAOHM,
    ZETTAOHM, YOTTAOHM, RONNAOHM, QUETTAOHM, STATOHM, ABOHM, PLANCK_OHM
};

enum class FrequencyUnit {
    HERTZ, QUECTOHERTZ, RONTOHERTZ, YOCTOHERTZ, ZEPTOHERTZ, ATTOHERTZ,
    FEMTOHERTZ, PICOHERTZ, NANOHERTZ, MICROHERTZ, MILLIHERTZ, CENTIHERTZ,
    DECIHERTZ, DECAHERTZ, HECTOHERTZ, KILOHERTZ, MEGAHERTZ, GIGAHERTZ,
    TERAHERTZ, PETAHERTZ, EXAHERTZ, ZETTAHERTZ, YOTTAHERTZ, RONNAHERTZ,
    QUETTAHERTZ, PERIOD_MINUTE, PERIOD_HOUR, PERIOD_DAY, PERIOD_WEEK,
    PERIOD_YEAR, PERIOD_DECADE, PERIOD_CENTURY, PERIOD_MILLENNIUM
};

enum class RadiationUnit {
    GRAY, ROENTGEN, RAD, SIEVERT, REM, BECQUEREL, CURIE
};

enum class DistanceUnit {
    METER, QUECTOMETER, RONTOMETER, YOCTOMETER, ZEPTOMETER, ATTOMETER,
    FEMTOMETER, PICOMETER, NANOMETER, MICROMETER, MILLIMETER, CENTIMETER,
    DECIMETER, DECAMETER, HECTOMETER, KILOMETER, MEGAMETER, GIGAMETER,
    TERAMETER, PETAMETER, EXAMETER, ZETTAMETER, YOTTAMETER, RONNAMETER,
    QUETTAMETER, ANGSTROM, BOHR, PLANCK, INCH, FOOT, YARD, MILE,
    NAUTICAL_MILE, LEAGUE, ROD, CHAIN, FURLONG, HAND, SMOOT, AU,
    LIGHT_PLANCK_TIME, LIGHT_QUECTOSECOND, LIGHT_RONTOSECOND,
    LIGHT_YOCTOSECOND, LIGHT_ZEPTOSECOND, LIGHT_ATTOSECOND,
    LIGHT_FEMTOSECOND, LIGHT_PICOSECOND, LIGHT_NANOSECOND,
    LIGHT_MICROSECOND, LIGHT_MILLISECOND, LIGHT_CENTISECOND,
    LIGHT_DECISECOND, LIGHT_SECOND, LIGHT_DECASECOND, LIGHT_HECTOSECOND,
    LIGHT_KILOSECOND, LIGHT_MINUTE, LIGHT_MEGASECOND, LIGHT_HOUR,
    LIGHT_GIGASECOND, LIGHT_DAY, LIGHT_WEEK, LIGHT_TERASECOND,
    LIGHT_PETASECOND, LIGHT_YEAR, LIGHT_EXASECOND, LIGHT_ZETTASECOND,
    LIGHT_YOTTASECOND, LIGHT_RONNASECOND, LIGHT_QUETTASECOND,
    LIGHT_DECADE, LIGHT_CENTURY, LIGHT_MILLENNIUM, QUECTOPARSEC,
    RONTOPARSEC, YOCTOPARSEC, ZEPTOPARSEC, ATTOPARSEC, FEMTOPARSEC,
    PICOPARSEC, NANOPARSEC, MICROPARSEC, MILLIPARSEC, CENTIPARSEC,
    DECIPARSEC, PARSEC, DECAPARSEC, HECTOPARSEC, KILOPARSEC, MEGAPARSEC,
    GIGAPARSEC, TERAPARSEC, PETAPARSEC, EXAPARSEC, ZETTAPARSEC,
    YOTTAPARSEC, RONNAPARSEC, QUETTAPARSEC
};

enum class TemperatureUnit {
    KELVIN, CELSIUS, FAHRENHEIT, RANKINE, REAUMUR, ROMER, DELISLE, NEWTON
};

enum class TimeUnit {
    SECOND, QUECTOSECOND, RONTOSECOND, YOCTOSECOND, ZEPTOSECOND,
    ATTOSECOND, FEMTOSECOND, PICOSECOND, NANOSECOND, MICROSECOND,
    MILLISECOND, CENTISECOND, DECISECOND, DECASECOND, HECTOSECOND,
    KILOSECOND, MEGASECOND, GIGASECOND, TERASECOND, PETASECOND,
    EXASECOND, ZETTASECOND, YOTTASECOND, RONNASECOND, QUETTASECOND,
    MINUTE, HOUR, DAY, WEEK, YEAR, DECADE, CENTURY, MILLENNIUM
};

enum class WeightUnit {
    KILOGRAM, QUECTOGRAM, RONTOGRAM, YOCTOGRAM, ZEPTOGRAM, ATTOGRAM,
    FEMTOGRAM, PICOGRAM, NANOGRAM, MICROGRAM, MILLIGRAM, CENTIGRAM,
    DECIGRAM, GRAM, DECAGRAM, HECTOGRAM, MEGAGRAM, GIGAGRAM, TERAGRAM,
    PETAGRAM, EXAGRAM, ZETTAGRAM, YOTTAGRAM, RONNAGRAM, QUETTAGRAM,
    METRIC_TON, TONNE, QUINTAL, GRAIN, DRAM, OUNCE, POUND, STONE,
    QUARTER_US, QUARTER_UK, HUNDREDWEIGHT_US, HUNDREDWEIGHT_UK, CENTAL,
    TON_US, TON_UK, TROY_GRAIN, PENNYWEIGHT, TROY_OUNCE, TROY_POUND,
    SCRUPLE, DRACHM, APOTHECARIES_OUNCE, APOTHECARIES_POUND, AMU_PHYSICS,
    AMU_CHEMISTRY, DALTON, UMU, PLANCK_MASS, SLUG, CARAT, POINT,
    PEARL_GRAIN, KIP, GAMMA
};

enum class AngleUnit {
    DEGREES,
    RADIANS
};

struct VelocityUnit {
    const DistanceUnit distance;
    const TimeUnit time;
    const long double special_conversion_factor;
    
    constexpr VelocityUnit(
        const DistanceUnit d_ = DistanceUnit::METER, 
        const TimeUnit t_ = TimeUnit::SECOND, 
        const long double special_factor = 1.0
    ) noexcept 
        : distance(d_), time(t_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr VelocityUnit SI() noexcept { 
        return VelocityUnit(DistanceUnit::METER, TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr VelocityUnit Special(const long double factor) noexcept { 
        return VelocityUnit(DistanceUnit::METER, TimeUnit::SECOND, factor); 
    }

    static constexpr VelocityUnit meters_per_second() noexcept {
        return VelocityUnit(DistanceUnit::METER, TimeUnit::SECOND);
    }

    static constexpr VelocityUnit feet_per_second() noexcept {
        return VelocityUnit(DistanceUnit::FOOT, TimeUnit::SECOND);
    }

    static constexpr VelocityUnit miles_per_hour() noexcept {
        return VelocityUnit(DistanceUnit::MILE, TimeUnit::HOUR);
    }

    static constexpr VelocityUnit kilometers_per_hour() noexcept {
        return VelocityUnit(DistanceUnit::KILOMETER, TimeUnit::HOUR);
    }
    
    static constexpr VelocityUnit knots() noexcept { 
        return VelocityUnit(DistanceUnit::NAUTICAL_MILE, TimeUnit::HOUR); 
    }
    
    static constexpr VelocityUnit mach(
        const long double temp = 298.15, 
        const TemperatureUnit temp_unit = TemperatureUnit::KELVIN, 
        const long double gas_constant = 287.05, 
        const long double abiadic_index = 1.4
    ) noexcept;
    
    static constexpr VelocityUnit light_speed() noexcept { 
        return VelocityUnit(DistanceUnit::LIGHT_SECOND, TimeUnit::SECOND); 
    }
    
    static constexpr VelocityUnit percent_light_speed() noexcept { 
        return Special(fizmo::constants::LIGHT_SPEED<long double> * 0.01); 
    }
    
    constexpr long double conversion_factor(const VelocityUnit to) const noexcept;
    static constexpr long double conversion_factor(const VelocityUnit from, const VelocityUnit to) noexcept;
};

struct AccelerationUnit {
    const VelocityUnit velocity;
    const TimeUnit time;
    const long double special_conversion_factor;
    
    constexpr AccelerationUnit(
        const DistanceUnit d_, 
        const TimeUnit t_1, 
        const TimeUnit t_2, 
        const long double special_factor = 1.0
    ) noexcept 
        : velocity(VelocityUnit(d_, t_1)), time(t_2), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}

    constexpr AccelerationUnit(
        const DistanceUnit d_, 
        const TimeUnit t = TimeUnit::SECOND, 
        const long double special_factor = 1.0
    ) noexcept 
        : velocity(VelocityUnit(d_, t)), time(t), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr AccelerationUnit(
        const VelocityUnit v_ = VelocityUnit::meters_per_second(), 
        const TimeUnit t_ = TimeUnit::SECOND, 
        const long double special_factor = 1.0
    ) noexcept 
        : velocity(v_), time(t_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr AccelerationUnit SI() noexcept { 
        return AccelerationUnit(VelocityUnit::SI(), TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr AccelerationUnit Special(const long double factor) noexcept { 
        return AccelerationUnit(VelocityUnit::SI(), TimeUnit::SECOND, factor); 
    }
    
    static constexpr AccelerationUnit g_force(const long double g_multiple = 1.0L) noexcept { 
        return Special(fizmo::constants::GRAVITY<long double> * fizmo::max_constexpr(g_multiple, fizmo::constants::TYPE_EPSILON<long double>)); 
    }
    
    static constexpr AccelerationUnit standard_gravity() noexcept { 
        return g_force(1.0L); 
    }
    
    constexpr long double conversion_factor(const AccelerationUnit to) const noexcept;
    static constexpr long double conversion_factor(const AccelerationUnit from, const AccelerationUnit to) noexcept;
};

struct AngularVelocityUnit {
    const AngleUnit angle;
    const TimeUnit time;
    const long double special_conversion_factor;
    
    constexpr AngularVelocityUnit(
        const AngleUnit a_ = AngleUnit::RADIANS, 
        const TimeUnit t_ = TimeUnit::SECOND, 
        const long double special_factor = 1.0
    ) noexcept 
        : angle(a_), time(t_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr AngularVelocityUnit SI() noexcept { 
        return AngularVelocityUnit(AngleUnit::RADIANS, TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr AngularVelocityUnit Special(const long double factor) noexcept { 
        return AngularVelocityUnit(AngleUnit::RADIANS, TimeUnit::SECOND, factor); 
    }
    
    constexpr long double conversion_factor(const AngularVelocityUnit to) const noexcept;
    static constexpr long double conversion_factor(const AngularVelocityUnit from, const AngularVelocityUnit to) noexcept;
};

struct AreaUnit {
    const DistanceUnit length;
    const DistanceUnit width;
    const long double special_conversion_factor;
    
    constexpr AreaUnit(
        const DistanceUnit l_, 
        const DistanceUnit w_, 
        const long double special_factor = 1.0
    ) noexcept 
        : length(l_), width(w_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr AreaUnit(
        const DistanceUnit size = DistanceUnit::METER, 
        const long double special_factor = 1.0
    ) noexcept 
        : length(size), width(size), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr AreaUnit SI() noexcept { 
        return AreaUnit(DistanceUnit::METER, 1.0); 
    }
    
    static constexpr AreaUnit Special(const long double factor) noexcept { 
        return AreaUnit(DistanceUnit::METER, factor); 
    }
    
    static constexpr AreaUnit hectare() noexcept { 
        return AreaUnit(DistanceUnit::HECTOMETER); 
    }
    
    static constexpr AreaUnit acre() noexcept { 
        return AreaUnit(DistanceUnit::CHAIN, DistanceUnit::FURLONG); 
    }
    
    static constexpr AreaUnit are() noexcept { 
        return AreaUnit(DistanceUnit::DECAMETER); 
    }
    
    static constexpr AreaUnit barn() noexcept { 
        return Special(1e-28L); 
    }
    
    constexpr long double conversion_factor(const AreaUnit to) const noexcept;
    static constexpr long double conversion_factor(const AreaUnit from, const AreaUnit to) noexcept;
};

struct VolumeUnit {
    const DistanceUnit length;
    const DistanceUnit width;
    const DistanceUnit height;
    const long double special_conversion_factor;
    
    constexpr VolumeUnit(
        const DistanceUnit l_, 
        const DistanceUnit w_, 
        const DistanceUnit h_, 
        const long double special_factor = 1.0
    ) noexcept 
        : length(l_), width(w_), height(h_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr VolumeUnit(
        const DistanceUnit size = DistanceUnit::METER, 
        const long double special_factor = 1.0
    ) noexcept 
        : length(size), width(size), height(size), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr VolumeUnit SI() noexcept { 
        return VolumeUnit(DistanceUnit::METER, 1.0); 
    }
    
    static constexpr VolumeUnit Special(const long double factor) noexcept { 
        return VolumeUnit(DistanceUnit::METER, factor); 
    }
    
    static constexpr VolumeUnit liter() noexcept { 
        return VolumeUnit(DistanceUnit::DECIMETER); 
    }
    
    static constexpr VolumeUnit milliliter() noexcept { 
        return VolumeUnit(DistanceUnit::CENTIMETER); 
    }

    static constexpr VolumeUnit us_fluid_ounce() noexcept { return Special(2.95735295625e-5L); }
    static constexpr VolumeUnit us_cup() noexcept { return Special(2.95735295625e-5L * 8.0L); }
    static constexpr VolumeUnit us_pint() noexcept { return Special(2.95735295625e-5L * 16.0L); }
    static constexpr VolumeUnit us_quart() noexcept { return Special(2.95735295625e-5L * 32.0L); }
    static constexpr VolumeUnit us_gallon() noexcept { return Special(3.785411784e-3L); }
    static constexpr VolumeUnit us_dry_cup() noexcept { return Special(2.365882365e-4L); }
    static constexpr VolumeUnit us_dry_pint() noexcept { return Special(5.506104713575e-4L); }
    static constexpr VolumeUnit us_dry_quart() noexcept { return Special(5.506104713575e-4L * 2.0L); }
    static constexpr VolumeUnit dry_gallon() noexcept { return Special(4.40488377086e-3); }
    static constexpr VolumeUnit us_peck() noexcept { return Special(5.506104713575e-4L * 16.0L); }
    static constexpr VolumeUnit us_bushel() noexcept { return Special(5.506104713575e-4L * 64.0L); }
    static constexpr VolumeUnit us_tablespoon() noexcept { return Special(2.95735295625e-5L * 0.5L); }
    static constexpr VolumeUnit us_teaspoon() noexcept { return Special(2.95735295625e-5L / 6.0L); }
    static constexpr VolumeUnit uk_fluid_ounce() noexcept { return Special(2.84130625e-5L); }
    static constexpr VolumeUnit uk_cup() noexcept { return Special(2.84130625e-5L * 10.0L); }
    static constexpr VolumeUnit uk_pint() noexcept { return Special(2.84130625e-5L * 20.0L); }
    static constexpr VolumeUnit uk_quart() noexcept { return Special(2.84130625e-5L * 40.0L); }
    static constexpr VolumeUnit uk_gallon() noexcept { return Special(4.54609e-3L); }
    static constexpr VolumeUnit uk_peck() noexcept { return Special(9.09218e-3L); }
    static constexpr VolumeUnit uk_bushel() noexcept { return Special(3.523907016688e-2L); }
    static constexpr VolumeUnit uk_tablespoon() noexcept { return Special(1.77581640625e-5L); }
    static constexpr VolumeUnit uk_teaspoon() noexcept { return Special(4.92892159375e-6L); }

    constexpr long double conversion_factor(const VolumeUnit to) const noexcept;
    static constexpr long double conversion_factor(const VolumeUnit from, const VolumeUnit to) noexcept;
};

struct DensityUnit_2D {
    const WeightUnit mass;
    const AreaUnit area;
    const long double special_conversion_factor;
    
    constexpr DensityUnit_2D(
        const WeightUnit m_ = WeightUnit::KILOGRAM, 
        const AreaUnit a_ = AreaUnit(), 
        const long double special_factor = 1.0
    ) noexcept 
        : mass(m_), area(a_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr DensityUnit_2D SI() noexcept { 
        return DensityUnit_2D(WeightUnit::KILOGRAM, AreaUnit::SI(), 1.0); 
    }
    
    static constexpr DensityUnit_2D Special(const long double factor) noexcept { 
        return DensityUnit_2D(WeightUnit::KILOGRAM, AreaUnit::SI(), factor); 
    }
    
    constexpr long double conversion_factor(const DensityUnit_2D to) const noexcept;
    static constexpr long double conversion_factor(const DensityUnit_2D from, const DensityUnit_2D to) noexcept;
};

struct DensityUnit_3D {
    const WeightUnit mass;
    const VolumeUnit volume;
    const long double special_conversion_factor;
    
    constexpr DensityUnit_3D(
        const WeightUnit m_ = WeightUnit::KILOGRAM, 
        const VolumeUnit v_ = VolumeUnit(), 
        const long double special_factor = 1.0
    ) noexcept 
        : mass(m_), volume(v_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr DensityUnit_3D SI() noexcept { 
        return DensityUnit_3D(WeightUnit::KILOGRAM, VolumeUnit::SI(), 1.0); 
    }
    
    static constexpr DensityUnit_3D Special(const long double factor) noexcept { 
        return DensityUnit_3D(WeightUnit::KILOGRAM, VolumeUnit::SI(), factor); 
    }
    
    constexpr long double conversion_factor(const DensityUnit_3D to) const noexcept;
    static constexpr long double conversion_factor(const DensityUnit_3D from, const DensityUnit_3D to) noexcept;
};

struct MomentumUnit {
    const WeightUnit mass;
    const VelocityUnit velocity;
    const long double special_conversion_factor;
    
    constexpr MomentumUnit(
        const WeightUnit m_ = WeightUnit::KILOGRAM, 
        const VelocityUnit v_ = VelocityUnit(), 
        const long double special_factor = 1.0
    ) noexcept 
        : mass(m_), velocity(v_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr MomentumUnit(
        const DistanceUnit d_, 
        const TimeUnit t_ = TimeUnit::SECOND, 
        const WeightUnit m_ = WeightUnit::KILOGRAM, 
        const long double special_factor = 1.0
    ) noexcept 
        : mass(m_), velocity(VelocityUnit(d_, t_)), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr MomentumUnit SI() noexcept { 
        return MomentumUnit(WeightUnit::KILOGRAM, VelocityUnit::SI(), 1.0); 
    }
    
    static constexpr MomentumUnit Special(const long double factor) noexcept { 
        return MomentumUnit(WeightUnit::KILOGRAM, VelocityUnit::SI(), factor); 
    }
    
    constexpr long double conversion_factor(const MomentumUnit to) const noexcept;
    static constexpr long double conversion_factor(const MomentumUnit from, const MomentumUnit to) noexcept;
};

struct ForceUnit {
    const WeightUnit mass;
    const AccelerationUnit acceleration;
    const long double special_conversion_factor;
    
    constexpr ForceUnit(
        const WeightUnit m_ = WeightUnit::KILOGRAM, 
        const AccelerationUnit a_ = AccelerationUnit::SI(), 
        const long double special_factor = 1.0
    ) noexcept 
        : mass(m_), acceleration(a_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr ForceUnit SI() noexcept { 
        return ForceUnit(WeightUnit::KILOGRAM, AccelerationUnit::SI(), 1.0); 
    }
    
    static constexpr ForceUnit Special(const long double factor) noexcept { 
        return ForceUnit(WeightUnit::KILOGRAM, AccelerationUnit::SI(), factor); 
    }

    static constexpr ForceUnit newton() noexcept {
        return SI();
    }

    static constexpr ForceUnit dyne() noexcept {
        return ForceUnit(WeightUnit::GRAM, AccelerationUnit(DistanceUnit::CENTIMETER));
    }

    static constexpr ForceUnit poundal() noexcept {
        return ForceUnit(WeightUnit::POUND, AccelerationUnit(DistanceUnit::FOOT));
    }

    static constexpr ForceUnit sthene() noexcept {
        return ForceUnit(WeightUnit::MEGAGRAM, AccelerationUnit::SI());
    }

    static constexpr ForceUnit pound_force() noexcept {
        return ForceUnit(WeightUnit::POUND, AccelerationUnit::standard_gravity());
    }

    static constexpr ForceUnit kilogram_force() noexcept {
        return ForceUnit(WeightUnit::KILOGRAM, AccelerationUnit::standard_gravity());
    }
    
    constexpr long double conversion_factor(const ForceUnit to) const noexcept;
    static constexpr long double conversion_factor(const ForceUnit from, const ForceUnit to) noexcept;
};

// PowerUnit: V * I (electrical) or F * v (mechanical)
struct PowerUnit {
    enum class Composition {
        ELECTRICAL,   // V * I
        MECHANICAL,   // F * v
    };

    const Composition composition;
    
    // Electrical: V * I
    const VoltageUnit voltage;
    const ElectricalCurrentUnit current;
    
    // Mechanical: F * v
    const ForceUnit force;
    const VelocityUnit velocity;
    
    const long double special_conversion_factor;
    
    constexpr PowerUnit(
        const VoltageUnit v_ = VoltageUnit::VOLT, 
        const ElectricalCurrentUnit i_ = ElectricalCurrentUnit::AMPERE, 
        const long double special_factor = 1.0
    ) noexcept 
        : composition(Composition::ELECTRICAL), voltage(v_), current(i_), 
          force(ForceUnit::SI()), velocity(VelocityUnit::SI()),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr PowerUnit(
        const ForceUnit f_, 
        const VelocityUnit v_, 
        const long double special_factor = 1.0
    ) noexcept 
        : composition(Composition::MECHANICAL), 
          voltage(VoltageUnit::VOLT), 
          current(ElectricalCurrentUnit::AMPERE),
          force(f_), velocity(v_),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}

    static constexpr PowerUnit SI_electrical() noexcept { 
        return PowerUnit(VoltageUnit::VOLT, ElectricalCurrentUnit::AMPERE, 1.0); 
    }
    
    static constexpr PowerUnit SI_mechanical() noexcept { 
        return PowerUnit(ForceUnit::SI(), VelocityUnit::SI(), 1.0); 
    }
    
    static constexpr PowerUnit SpecialElectrical(const long double factor) noexcept { 
        return PowerUnit(VoltageUnit::VOLT, ElectricalCurrentUnit::AMPERE, factor); 
    }
    
    static constexpr PowerUnit SpecialMechanical(const long double factor) noexcept { 
        return PowerUnit(ForceUnit::SI(), VelocityUnit::SI(), factor); 
    }

    static constexpr PowerUnit horsepower() noexcept { return SpecialMechanical(745.69987158227L); }
    static constexpr PowerUnit metric_horsepower() noexcept { return SpecialMechanical(735.49875L); }
    static constexpr PowerUnit electric_horsepower() noexcept { return SpecialMechanical(746.0L); }
    static constexpr PowerUnit boiler_horsepower() noexcept { return SpecialMechanical(9809.5L); }
    static constexpr PowerUnit water_horsepower() noexcept { return SpecialMechanical(745.69987158227L); }
    static constexpr PowerUnit brake_horsepower() noexcept { return SpecialMechanical(745.69987158227L); }
    static constexpr PowerUnit shaft_horsepower() noexcept { return SpecialMechanical(745.69987158227L); }
    static constexpr PowerUnit hydraulic_horsepower() noexcept { return SpecialMechanical(745.69987158227L); }

    static constexpr PowerUnit nanowatt() noexcept { return SpecialElectrical(1e-9L); }
    static constexpr PowerUnit microwatt() noexcept { return SpecialElectrical(1e-6L); }
    static constexpr PowerUnit milliwatt() noexcept { return SpecialElectrical(1e-3L); }
    static constexpr PowerUnit watt() noexcept { return SI_electrical(); }
    static constexpr PowerUnit kilowatt() noexcept { return SpecialElectrical(1e3L); }
    static constexpr PowerUnit megawatt() noexcept { return SpecialElectrical(1e6L); }
    static constexpr PowerUnit gigawatt() noexcept { return SpecialElectrical(1e9L); }
    static constexpr PowerUnit terawatt() noexcept { return SpecialElectrical(1e12L); }

    static constexpr PowerUnit nanowatt_mechanical() noexcept { return SpecialMechanical(1e-9L); }
    static constexpr PowerUnit microwatt_mechanical() noexcept { return SpecialMechanical(1e-6L); }
    static constexpr PowerUnit milliwatt_mechanical() noexcept { return SpecialMechanical(1e-3L); }
    static constexpr PowerUnit watt_mechanical() noexcept { return SI_mechanical(); }
    static constexpr PowerUnit kilowatt_mechanical() noexcept { return SpecialMechanical(1e3L); }
    static constexpr PowerUnit megawatt_mechanical() noexcept { return SpecialMechanical(1e6L); }
    static constexpr PowerUnit gigawatt_mechanical() noexcept { return SpecialMechanical(1e9L); }
    static constexpr PowerUnit terawatt_mechanical() noexcept { return SpecialMechanical(1e12L); }

    constexpr long double conversion_factor(const PowerUnit to) const noexcept;
    static constexpr long double conversion_factor(const PowerUnit from, const PowerUnit to) noexcept;
};

// EnergyUnit: P * t or F * d
struct EnergyUnit {
    enum class Representation {
        POWER_TIME,      // P * t
        FORCE_DISTANCE   // F * d (mechanical work)
    };

    const Representation representation;
    
    // Power * Time representation
    PowerUnit power;
    TimeUnit time;
    
    // Force * Distance representation (for mechanical work like foot-pounds)
    ForceUnit force;
    DistanceUnit distance;
    
    long double special_conversion_factor;
    
    constexpr EnergyUnit(
        const PowerUnit p_, 
        const TimeUnit t_, 
        const long double special_factor = 1.0
    ) noexcept 
        : representation(Representation::POWER_TIME),
          power(p_), time(t_), 
          force(ForceUnit::SI()), distance(DistanceUnit::METER),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    constexpr EnergyUnit(
        const ForceUnit f_, 
        const DistanceUnit d_, 
        const long double special_factor = 1.0
    ) noexcept 
        : representation(Representation::FORCE_DISTANCE),
          power(PowerUnit::SI_mechanical()), time(TimeUnit::SECOND),
          force(f_), distance(d_),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr EnergyUnit SI_electrical() noexcept { 
        return EnergyUnit(PowerUnit::SI_electrical(), TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr EnergyUnit SI_mechanical() noexcept { 
        return EnergyUnit(PowerUnit::SI_mechanical(), TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr EnergyUnit SpecialMechanical(const long double factor) noexcept { 
        return EnergyUnit(PowerUnit::SI_mechanical(), TimeUnit::SECOND, factor); 
    }
    
    static constexpr EnergyUnit SpecialElectrical(const long double factor) noexcept { 
        return EnergyUnit(PowerUnit::SI_electrical(), TimeUnit::SECOND, factor); 
    }
    
    static constexpr EnergyUnit nanojoule() noexcept { return SpecialElectrical(1e-9L); }
    static constexpr EnergyUnit microjoule() noexcept { return SpecialElectrical(1e-6L); }
    static constexpr EnergyUnit millijoule() noexcept { return SpecialElectrical(1e-3L); }
    static constexpr EnergyUnit joule() noexcept { return SI_electrical(); }
    static constexpr EnergyUnit kilojoule() noexcept { return SpecialElectrical(1e3L); }
    static constexpr EnergyUnit megajoule() noexcept { return SpecialElectrical(1e6L); }
    static constexpr EnergyUnit gigajoule() noexcept { return SpecialElectrical(1e9L); }
    static constexpr EnergyUnit terajoule() noexcept { return SpecialElectrical(1e12L); }

    static constexpr EnergyUnit erg() noexcept {
        return EnergyUnit(
            PowerUnit(ForceUnit::dyne(), VelocityUnit(DistanceUnit::CENTIMETER)),
            TimeUnit::SECOND
        );
    }

    static constexpr EnergyUnit foot_pound_force() noexcept {
        return EnergyUnit(ForceUnit::pound_force(), DistanceUnit::FOOT);
    }

    static constexpr EnergyUnit foot_poundal() noexcept {
        return EnergyUnit(ForceUnit::poundal(), DistanceUnit::FOOT);
    }

    static constexpr EnergyUnit inch_pound_force() noexcept {
        return EnergyUnit(ForceUnit::pound_force(), DistanceUnit::INCH);
    }

    static constexpr EnergyUnit electronvolt() noexcept { 
        return SpecialElectrical(fizmo::constants::ELECTRON_VOLT<long double>); 
    }
    
    static constexpr EnergyUnit kilo_electronvolt() noexcept { 
        return SpecialElectrical(fizmo::constants::ELECTRON_VOLT<long double> * 1e3L); 
    }
    
    static constexpr EnergyUnit mega_electronvolt() noexcept { 
        return SpecialElectrical(fizmo::constants::ELECTRON_VOLT<long double> * 1e6L); 
    }
    
    static constexpr EnergyUnit giga_electronvolt() noexcept { 
        return SpecialElectrical(fizmo::constants::ELECTRON_VOLT<long double> * 1e9L); 
    }

    static constexpr EnergyUnit calorie() noexcept { return SpecialMechanical(4.1868L); }
    static constexpr EnergyUnit kilocalorie() noexcept { return SpecialMechanical(4.1868L * 1e3); }
    static constexpr EnergyUnit megacalorie() noexcept { return SpecialMechanical(4.1868L * 1e6); }
    static constexpr EnergyUnit gigacalorie() noexcept { return SpecialMechanical(4.1868L * 1e9); }
    static constexpr EnergyUnit calorie_thermochemical() noexcept { return SpecialMechanical(4.184L); }

    static constexpr EnergyUnit btu() noexcept { return SpecialMechanical(1055.05585262L); }
    static constexpr EnergyUnit btu_thermochemical() noexcept { return SpecialMechanical(1054.35026444L); }
    static constexpr EnergyUnit therm() noexcept { return SpecialMechanical(1055.05585262L * 1e5L); }
    static constexpr EnergyUnit quad() noexcept { return SpecialMechanical(1055.05585262L * 1e15L); }

    static constexpr EnergyUnit kilowatt_hour() noexcept {
        return EnergyUnit(PowerUnit::kilowatt(), TimeUnit::HOUR);
    }

    static constexpr EnergyUnit watt_hour() noexcept {
        return EnergyUnit(PowerUnit::watt(), TimeUnit::HOUR);
    }

    constexpr long double conversion_factor(const EnergyUnit to) const noexcept;
    static constexpr long double conversion_factor(const EnergyUnit from, const EnergyUnit to) noexcept;
};

// TorqueUnit: F * d 
struct TorqueUnit {
    ForceUnit force;
    DistanceUnit distance;
    long double special_conversion_factor;
    
    constexpr TorqueUnit(
        const ForceUnit f_ = ForceUnit::SI(), 
        const DistanceUnit d_ = DistanceUnit::METER, 
        const long double special_factor = 1.0
    ) noexcept 
        : force(f_), distance(d_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr TorqueUnit SI() noexcept { 
        return TorqueUnit(ForceUnit::SI(), DistanceUnit::METER, 1.0); 
    }
    
    static constexpr TorqueUnit Special(const long double factor) noexcept { 
        return TorqueUnit(ForceUnit::SI(), DistanceUnit::METER, factor); 
    }

    static constexpr TorqueUnit pound_foot() noexcept {
        return TorqueUnit(ForceUnit::pound_force(), DistanceUnit::FOOT);
    }

    static constexpr TorqueUnit pound_inch() noexcept {
        return TorqueUnit(ForceUnit::pound_force(), DistanceUnit::INCH);
    }

    static constexpr TorqueUnit newton_meter() noexcept {
        return TorqueUnit(ForceUnit::SI(), DistanceUnit::METER);
    }
    
    constexpr long double conversion_factor(const TorqueUnit to) const noexcept;
    static constexpr long double conversion_factor(const TorqueUnit from, const TorqueUnit to) noexcept;
};

// CapacitanceUnit: Q / V
struct CapacitanceUnit {
    ElectricalChargeUnit charge;
    VoltageUnit voltage;
    long double special_conversion_factor;
    
    constexpr CapacitanceUnit(
        const ElectricalChargeUnit q_ = ElectricalChargeUnit::COULOMB, 
        const VoltageUnit v_ = VoltageUnit::VOLT, 
        const long double special_factor = 1.0
    ) noexcept 
        : charge(q_), voltage(v_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr CapacitanceUnit SI() noexcept { 
        return CapacitanceUnit(ElectricalChargeUnit::COULOMB, VoltageUnit::VOLT, 1.0); 
    }
    
    static constexpr CapacitanceUnit Special(const long double factor) noexcept { 
        return CapacitanceUnit(ElectricalChargeUnit::COULOMB, VoltageUnit::VOLT, factor); 
    }
    
    constexpr long double conversion_factor(const CapacitanceUnit to) const noexcept;
    static constexpr long double conversion_factor(const CapacitanceUnit from, const CapacitanceUnit to) noexcept;
};

// ImpulseUnit: F * t
struct ImpulseUnit {
    ForceUnit force;
    TimeUnit time;
    long double special_conversion_factor;
    
    constexpr ImpulseUnit(
        const ForceUnit f_ = ForceUnit::SI(), 
        const TimeUnit t_ = TimeUnit::SECOND, 
        const long double special_factor = 1.0
    ) noexcept 
        : force(f_), time(t_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr ImpulseUnit SI() noexcept { 
        return ImpulseUnit(ForceUnit::SI(), TimeUnit::SECOND, 1.0); 
    }
    
    static constexpr ImpulseUnit Special(const long double factor) noexcept { 
        return ImpulseUnit(ForceUnit::SI(), TimeUnit::SECOND, factor); 
    }
    
    constexpr long double conversion_factor(const ImpulseUnit to) const noexcept;
    static constexpr long double conversion_factor(const ImpulseUnit from, const ImpulseUnit to) noexcept;
};

// PressureUnit: F / A
struct PressureUnit {
    ForceUnit force;
    AreaUnit area;
    long double special_conversion_factor;
    
    constexpr PressureUnit(
        const ForceUnit f_ = ForceUnit::SI(), 
        const AreaUnit a_ = AreaUnit::SI(), 
        const long double special_factor = 1.0
    ) noexcept 
        : force(f_), area(a_), 
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}
    
    static constexpr PressureUnit SI() noexcept { 
        return PressureUnit(ForceUnit::SI(), AreaUnit::SI(), 1.0); 
    }
    
    static constexpr PressureUnit Special(const long double factor) noexcept { 
        return PressureUnit(ForceUnit::SI(), AreaUnit::SI(), factor); 
    }

    static constexpr PressureUnit pascals() noexcept { return SI(); }
    static constexpr PressureUnit kilopascal() noexcept { return Special(1e3L); }
    static constexpr PressureUnit megapascal() noexcept { return Special(1e6L); }
    static constexpr PressureUnit bar() noexcept { return Special(1e5L); }
    static constexpr PressureUnit millibar() noexcept { return Special(1e2L); }
    static constexpr PressureUnit atmosphere() noexcept { return Special(101325.0L); }
    static constexpr PressureUnit torr() noexcept { return Special(133.3223684210526315789473684210526L); }
    
    static constexpr PressureUnit psi() noexcept {
        return PressureUnit(ForceUnit::pound_force(), AreaUnit(DistanceUnit::INCH));
    }
    
    constexpr long double conversion_factor(const PressureUnit to) const noexcept;
    static constexpr long double conversion_factor(const PressureUnit from, const PressureUnit to) noexcept;
};

struct SpecificHeatCapacityUnit {
    EnergyUnit energy;
    WeightUnit mass;
    TemperatureUnit temperature;
    const long double special_conversion_factor;

    constexpr SpecificHeatCapacityUnit(
        const EnergyUnit e_ = EnergyUnit::joule(),
        const WeightUnit m_ = WeightUnit::KILOGRAM,
        const TemperatureUnit t_ = TemperatureUnit::KELVIN,
        const long double special_factor = 1.0L
    ) noexcept
        : energy(e_), mass(m_), temperature(t_),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}

    static constexpr SpecificHeatCapacityUnit SI() noexcept {
        return SpecificHeatCapacityUnit(EnergyUnit::joule(), WeightUnit::KILOGRAM, TemperatureUnit::KELVIN, 1.0L);
    }

    static constexpr SpecificHeatCapacityUnit cal_per_g_C() noexcept {
        return SpecificHeatCapacityUnit(EnergyUnit::calorie(), WeightUnit::GRAM, TemperatureUnit::CELSIUS, 1.0L);
    }

    static constexpr SpecificHeatCapacityUnit j_per_g_C() noexcept {
        return SpecificHeatCapacityUnit(EnergyUnit::joule(), WeightUnit::GRAM, TemperatureUnit::KELVIN, 1.0L);
    }

    static constexpr SpecificHeatCapacityUnit Special(const long double factor) noexcept {
        return SpecificHeatCapacityUnit(EnergyUnit::joule(), WeightUnit::KILOGRAM, TemperatureUnit::KELVIN, factor);
    }

    constexpr long double conversion_factor(const SpecificHeatCapacityUnit to) const noexcept;
    static constexpr long double conversion_factor(const SpecificHeatCapacityUnit from, const SpecificHeatCapacityUnit to) noexcept;
};

struct VolumetricHeatCapacityUnit {
    EnergyUnit energy;
    VolumeUnit volume;
    TemperatureUnit temperature;
    const long double special_conversion_factor;

    constexpr VolumetricHeatCapacityUnit(
        const EnergyUnit e_ = EnergyUnit::joule(),
        const VolumeUnit v_ = VolumeUnit::SI(),
        const TemperatureUnit t_ = TemperatureUnit::KELVIN,
        const long double special_factor = 1.0L
    ) noexcept
        : energy(e_), volume(v_), temperature(t_),
          special_conversion_factor(fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)) {}

    static constexpr VolumetricHeatCapacityUnit SI() noexcept {
        return VolumetricHeatCapacityUnit(EnergyUnit::joule(), VolumeUnit::SI(), TemperatureUnit::KELVIN, 1.0L);
    }

    static constexpr VolumetricHeatCapacityUnit cal_per_cm_C() noexcept {
        return VolumetricHeatCapacityUnit(EnergyUnit::calorie(), VolumeUnit(DistanceUnit::CENTIMETER), TemperatureUnit::CELSIUS, 1.0L);
    }

    static constexpr VolumetricHeatCapacityUnit Special(const long double factor) noexcept {
        return VolumetricHeatCapacityUnit(EnergyUnit::joule(), VolumeUnit::SI(), TemperatureUnit::KELVIN, factor);
    }

    constexpr long double conversion_factor(const VolumetricHeatCapacityUnit to) const noexcept;
    static constexpr long double conversion_factor(const VolumetricHeatCapacityUnit from, const VolumetricHeatCapacityUnit to) noexcept;
};

struct DosageUnit {
    enum class Type {
        VOLUME_PER_WEIGHT,   
        WEIGHT_PER_WEIGHT   
    };

    const Type type;
    const VolumeUnit volume;   
    const WeightUnit weight_numerator;   
    const WeightUnit weight_denominator;

    const long double special_conversion_factor;

    constexpr DosageUnit(
        const VolumeUnit v_,
        const WeightUnit w_den_,
        const long double special_factor = 1.0L
    ) noexcept
        : type(Type::VOLUME_PER_WEIGHT),
          volume(v_),
          weight_numerator(WeightUnit::KILOGRAM),
          weight_denominator(w_den_),
          special_conversion_factor(
              fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)
          ) {}

    constexpr DosageUnit(
        const WeightUnit w_num_,
        const WeightUnit w_den_,
        const long double special_factor = 1.0L
    ) noexcept
        : type(Type::WEIGHT_PER_WEIGHT),
          volume(VolumeUnit::SI()),
          weight_numerator(w_num_),
          weight_denominator(w_den_),
          special_conversion_factor(
              fizmo::max_constexpr(special_factor, fizmo::constants::TYPE_EPSILON<long double>)
          ) {}

    static constexpr DosageUnit SI_volume_per_weight() noexcept {
        return DosageUnit(VolumeUnit::SI(), WeightUnit::KILOGRAM, 1.0L);
    }

    static constexpr DosageUnit SI_weight_per_weight() noexcept {
        return DosageUnit(WeightUnit::KILOGRAM, WeightUnit::KILOGRAM, 1.0L);
    }

    static constexpr DosageUnit mL_per_kg() noexcept {
        return DosageUnit(VolumeUnit::milliliter(), WeightUnit::KILOGRAM);
    }

    static constexpr DosageUnit mL_per_g() noexcept {
        return DosageUnit(VolumeUnit::milliliter(), WeightUnit::GRAM);
    }

    static constexpr DosageUnit microgram_per_kg() noexcept {
        return DosageUnit(WeightUnit::MICROGRAM, WeightUnit::KILOGRAM);
    }

    static constexpr DosageUnit milligram_per_kg() noexcept {
        return DosageUnit(WeightUnit::MILLIGRAM, WeightUnit::KILOGRAM);
    }

    static constexpr DosageUnit g_per_kg() noexcept {
        return DosageUnit(WeightUnit::GRAM, WeightUnit::KILOGRAM);
    }

    constexpr long double conversion_factor(const DosageUnit to) const noexcept;
    static constexpr long double conversion_factor(const DosageUnit from, const DosageUnit to) noexcept;
};

} // namespace units

template <typename T>
struct is_fizmo_unit : std::false_type {};

template <>
struct is_fizmo_unit<units::AccelerationUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::AngleUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::AngularVelocityUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::AreaUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::CapacitanceUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::DensityUnit_2D> : std::true_type {};

template <>
struct is_fizmo_unit<units::DensityUnit_3D> : std::true_type {};

template <>
struct is_fizmo_unit<units::DistanceUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::ElectricalChargeUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::ElectricalCurrentUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::EnergyUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::ForceUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::FrequencyUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::ImpulseUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::MomentumUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::PowerUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::PressureUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::RadiationUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::ResistanceUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::TemperatureUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::TimeUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::TorqueUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::VelocityUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::VoltageUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::VolumeUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::WeightUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::SpecificHeatCapacityUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::VolumetricHeatCapacityUnit> : std::true_type {};

template <>
struct is_fizmo_unit<units::DosageUnit> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_unit_v = is_fizmo_unit<T>::value;

} // namespace fizmo

#endif // UNIT_MANAGEMENT_HPP