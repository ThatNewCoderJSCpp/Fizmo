#ifndef FIZMO_CORE_UNIT_HPP
#define FIZMO_CORE_UNIT_HPP

#include <limits>
#include "dimension.hpp"

namespace fizmo {
namespace units {

namespace detail {
constexpr long double ld_pow(long double base, int n) noexcept {
    return n == 0 ? 1.0L : n > 0  ? base * ld_pow(base, n - 1) : 1.0L / ld_pow(base, -n);
}
} // namespace detail

template <
    int L, // Length
    int M, // Mass
    int T, // Time
    int I, // Electric Current 
    int K, // Thermodynamic Temperature
    int N, // Amount of Substance
    int J, // Luminous Intensity
    int A  // Plane Angle
>
struct Unit {
    long double scale;
    long double offset;

    constexpr Unit(long double s = 1.0L, long double o = 0.0L) noexcept : scale(s), offset(o) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return x * scale + offset; }
    constexpr long double from_base(long double b) const noexcept { return (b - offset) / scale; }

    template <int P>
    constexpr Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P> pow() const noexcept {
        return Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P>(detail::ld_pow(scale, P), 0.0L);
    }
};

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>
constexpr Unit<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>
operator*(Unit<L1,M1,T1,I1,K1,N1,J1,A1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return {a.scale * b.scale, 0.0L};
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>
constexpr Unit<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>
operator/(Unit<L1,M1,T1,I1,K1,N1,J1,A1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return {a.scale / b.scale, 0.0L};
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(long double k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return {u.scale * k, 0.0L}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(Unit<L,M,T,I,K,N,J,A> u, long double k) noexcept { return {u.scale * k, 0.0L}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator/(Unit<L,M,T,I,K,N,J,A> u, long double k) noexcept { return {u.scale / k, 0.0L}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<-L,-M,-T,-I,-K,-N,-J,-A> operator/(long double k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return {k / u.scale, 0.0L}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> prefixed(long double factor, Unit<L,M,T,I,K,N,J,A> base) noexcept {
    return {base.scale * factor, 0.0L};
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct NonlinearUnit {
    long double (*fwd)(long double);
    long double (*inv)(long double);

    constexpr NonlinearUnit(long double (*f)(long double), long double (*g)(long double)) noexcept : fwd(f), inv(g) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return fwd(x); }
    constexpr long double from_base(long double b) const noexcept { return inv(b); }
};

#define FIZMO_UNIT_OF(DIM) \
    ::fizmo::units::Unit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

#define FIZMO_NONLINEAR_OF(DIM) \
    ::fizmo::units::NonlinearUnit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

using DimensionlessUnit   = FIZMO_UNIT_OF(DIMENSION_DIMENSIONLESS);
using LengthUnit          = FIZMO_UNIT_OF(DIMENSION_LENGTH);
using MassUnit            = FIZMO_UNIT_OF(DIMENSION_MASS);
using TimeUnit            = FIZMO_UNIT_OF(DIMENSION_TIME);
using CurrentUnit         = FIZMO_UNIT_OF(DIMENSION_CURRENT);
using TemperatureUnit     = FIZMO_UNIT_OF(DIMENSION_TEMPERATURE);
using AmountUnit          = FIZMO_UNIT_OF(DIMENSION_AMOUNT);
using LuminousUnit        = FIZMO_UNIT_OF(DIMENSION_LUMINOUS);
using AngleUnit           = FIZMO_UNIT_OF(DIMENSION_ANGLE);

using FrequencyUnit       = FIZMO_UNIT_OF(DIMENSION_FREQUENCY);
using VelocityUnit        = FIZMO_UNIT_OF(DIMENSION_VELOCITY);
using AccelerationUnit    = FIZMO_UNIT_OF(DIMENSION_ACCELERATION);
using AngularVelocityUnit = FIZMO_UNIT_OF(DIMENSION_ANGULAR_VELOCITY);
using AreaUnit            = FIZMO_UNIT_OF(DIMENSION_AREA);
using VolumeUnit          = FIZMO_UNIT_OF(DIMENSION_VOLUME);
using Density2DUnit       = FIZMO_UNIT_OF(DIMENSION_DENSITY_2D);
using Density3DUnit       = FIZMO_UNIT_OF(DIMENSION_DENSITY_3D);
using MomentumUnit        = FIZMO_UNIT_OF(DIMENSION_MOMENTUM);
using ForceUnit           = FIZMO_UNIT_OF(DIMENSION_FORCE);
using ImpulseUnit         = FIZMO_UNIT_OF(DIMENSION_IMPULSE);
using EnergyUnit          = FIZMO_UNIT_OF(DIMENSION_ENERGY);
using PowerUnit           = FIZMO_UNIT_OF(DIMENSION_POWER);
using PressureUnit        = FIZMO_UNIT_OF(DIMENSION_PRESSURE);
using AbsorbedDoseUnit    = FIZMO_UNIT_OF(DIMENSION_ABSORBED_DOSE);
using ChargeUnit          = FIZMO_UNIT_OF(DIMENSION_CHARGE);
using VoltageUnit         = FIZMO_UNIT_OF(DIMENSION_VOLTAGE);
using ResistanceUnit      = FIZMO_UNIT_OF(DIMENSION_RESISTANCE);
using ConductanceUnit     = FIZMO_UNIT_OF(DIMENSION_CONDUCTANCE);
using CapacitanceUnit     = FIZMO_UNIT_OF(DIMENSION_CAPACITANCE);
using InductanceUnit      = FIZMO_UNIT_OF(DIMENSION_INDUCTANCE);
using MagneticFluxUnit    = FIZMO_UNIT_OF(DIMENSION_MAGNETIC_FLUX);
using TorqueUnit          = FIZMO_UNIT_OF(DIMENSION_TORQUE);
using MassFlowRateUnit    = FIZMO_UNIT_OF(DIMENSION_MASS_FLOW_RATE);
using VolumeFlowRateUnit  = FIZMO_UNIT_OF(DIMENSION_VOLUME_FLOW_RATE);
using AreaFlowRateUnit    = FIZMO_UNIT_OF(DIMENSION_AREA_FLOW_RATE);
using MolarFlowRateUnit   = FIZMO_UNIT_OF(DIMENSION_MOLAR_FLOW_RATE);
using MolarityMassUnit    = FIZMO_UNIT_OF(DIMENSION_MOLARITY_MASS);
using MolarityVolumeUnit  = FIZMO_UNIT_OF(DIMENSION_MOLARITY_VOLUME);
using SpecificAreaUnit    = FIZMO_UNIT_OF(DIMENSION_SPECIFIC_AREA);
using SpecificVolumeUnit  = FIZMO_UNIT_OF(DIMENSION_SPECIFIC_VOLUME);

} // namespace units

template <typename T> struct is_fizmo_unit : std::false_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A> struct is_fizmo_unit<units::Unit<L, M, T, I, K, N, J, A>> : std::true_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_unit<units::NonlinearUnit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_unit_v = is_fizmo_unit<T>::value;

} // namespace fizmo

#endif // FIZMO_CORE_UNIT_HPP