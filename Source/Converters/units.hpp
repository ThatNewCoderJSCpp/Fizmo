#ifndef FIZMO_CORE_UNIT_HPP
#define FIZMO_CORE_UNIT_HPP

#include <cstdint>
#include <limits>
#include "dimension.hpp"

namespace fizmo {
namespace units {

namespace detail {

constexpr long double ld_pow(long double base, int n) noexcept {
    return n == 0 ? 1.0L : n > 0  ? base * ld_pow(base, n - 1) : 1.0L / ld_pow(base, -n);
}

constexpr long double ld_pi = 3.141592653589793238462643383279502884L;

constexpr int exact_capacity = 12;

} // namespace detail

struct ExactScale {
    long long   mant[detail::exact_capacity];
    int         pow[detail::exact_capacity];
    int         count;
    int         exp10;
    int         pi_pow;
    bool        negative;
    bool        is_zero;
    bool        binary;
    long double binary_value;

    constexpr ExactScale() noexcept
        : mant{}, pow{}, count(0), exp10(0), pi_pow(0), negative(false), is_zero(false), binary(false), binary_value(1.0L) {}

    static constexpr ExactScale one() noexcept { return ExactScale(); }

    static constexpr ExactScale zero() noexcept {
        ExactScale r;
        r.is_zero      = true;
        r.binary_value = 0.0L;
        return r;
    }

    static constexpr ExactScale from_long_double(long double v) noexcept {
        if (v == 0.0L) return zero();
        if (v == 1.0L) return one();
        ExactScale r;
        r.binary       = true;
        r.binary_value = v;
        return r;
    }

    static constexpr ExactScale pi(int k = 1) noexcept {
        ExactScale r;
        r.pi_pow = k;
        return r;
    }

    constexpr ExactScale with_factor(long long m, int p) const noexcept {
        if (p == 0 || is_zero) return *this;
        if (m == 0) return (p > 0) ? zero() : *this;
        ExactScale r = *this;
        if (r.binary) { r.binary_value *= detail::ld_pow(static_cast<long double>(m), p); return r; }
        if (m < 0) { m = -m; if (p % 2 != 0) r.negative = !r.negative; }
        while (m % 10 == 0) { m /= 10; r.exp10 += p; }
        if (m == 1) return r;

        for (int i = 0; i < r.count; ++i) {
            if (r.mant[i] == m) {
                r.pow[i] += p;

                if (r.pow[i] == 0) {
                    for (int j = i + 1; j < r.count; ++j) { r.mant[j - 1] = r.mant[j]; r.pow[j - 1] = r.pow[j]; }
                    --r.count;
                }

                return r;
            }
        }

        if (r.count == detail::exact_capacity) return from_long_double(value() * detail::ld_pow(static_cast<long double>(m), p));
        r.mant[r.count] = m;
        r.pow[r.count]  = p;
        ++r.count;
        return r;
    }

    constexpr ExactScale power(int p) const noexcept {
        if (is_zero) return (p > 0) ? zero() : *this;
        if (binary)  return from_long_double(detail::ld_pow(binary_value, p));
        ExactScale r;
        r.exp10    = exp10 * p;
        r.pi_pow   = pi_pow * p;
        r.negative = negative && (p % 2 != 0);
        r.count    = count;
        for (int i = 0; i < count; ++i) { r.mant[i] = mant[i]; r.pow[i] = pow[i] * p; }
        return r;
    }

    constexpr ExactScale times(const ExactScale& o, int sign = 1) const noexcept {
        if (is_zero || (o.is_zero && sign > 0)) return zero();
        if (binary || o.binary) {
            const long double a = value();
            const long double b = o.value();
            return from_long_double(sign > 0 ? a * b : a / b);
        }
        ExactScale r = *this;
        r.exp10  += sign * o.exp10;
        r.pi_pow += sign * o.pi_pow;
        if (o.negative) r.negative = !r.negative;
        for (int i = 0; i < o.count; ++i) r = r.with_factor(o.mant[i], sign * o.pow[i]);
        return r;
    }

    constexpr ExactScale operator*(const ExactScale& o) const noexcept { return times(o, 1); }
    constexpr ExactScale operator/(const ExactScale& o) const noexcept { return times(o, -1); }
    constexpr ExactScale operator-() const noexcept {
        ExactScale r = *this;
        if (r.binary) r.binary_value = -r.binary_value;
        else          r.negative = !r.negative;
        return r;
    }

    constexpr long double value() const noexcept {
        if (is_zero) return 0.0L;
        if (binary)  return binary_value;
        long double num = 1.0L, den = 1.0L;

        for (int i = 0; i < count; ++i) {
            if (pow[i] > 0) num *= detail::ld_pow(static_cast<long double>(mant[i]), pow[i]);
            else            den *= detail::ld_pow(static_cast<long double>(mant[i]), -pow[i]);
        }

        if (exp10 > 0) num *= detail::ld_pow(10.0L, exp10);
        else           den *= detail::ld_pow(10.0L, -exp10);
        long double v = num / den;
        if (pi_pow != 0) v *= detail::ld_pow(detail::ld_pi, pi_pow);
        return negative ? -v : v;
    }
};

constexpr ExactScale exact(const char* s) noexcept {
    bool          neg = false;
    unsigned long long m = 0;
    int           e   = 0;
    int           dig = 0;
    bool          dot = false;
    bool          lossy = false;
    std::size_t   i   = 0;
    if (s[i] == '-') { neg = true; ++i; }
    else if (s[i] == '+') { ++i; }

    for (; s[i] != '\0' && s[i] != 'e' && s[i] != 'E'; ++i) {
        if (s[i] == '.') { dot = true; continue; }
        if (dig >= 18) { lossy = true; if (!dot) ++e; continue; }
        m = m * 10u + static_cast<unsigned long long>(s[i] - '0');
        if (m != 0) ++dig;
        if (dot) --e;
    }

    if (s[i] == 'e' || s[i] == 'E') {
        ++i;
        bool en = false;
        if (s[i] == '-') { en = true; ++i; }
        else if (s[i] == '+') { ++i; }
        int x = 0;
        for (; s[i] != '\0'; ++i) x = x * 10 + (s[i] - '0');
        e += en ? -x : x;
    }

    if (m == 0) return ExactScale::zero();
    ExactScale r;
    r.exp10 = e;
    r = r.with_factor(static_cast<long long>(m), 1);
    if (neg) r = -r;
    if (lossy) r = ExactScale::from_long_double(r.value());
    return r;
}

constexpr ExactScale ratio(long long num, long long den = 1) noexcept {
    return ExactScale().with_factor(num, 1).with_factor(den, -1);
}

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
    ExactScale  exact_scale;
    ExactScale  exact_offset;

    constexpr Unit(long double s = 1.0L, long double o = 0.0L) noexcept
        : scale(s), offset(o), exact_scale(ExactScale::from_long_double(s)), exact_offset(ExactScale::from_long_double(o)) {}

    constexpr Unit(const ExactScale& s) noexcept
        : scale(s.value()), offset(0.0L), exact_scale(s), exact_offset(ExactScale::zero()) {}

    constexpr Unit(const ExactScale& s, const ExactScale& o) noexcept
        : scale(s.value()), offset(o.value()), exact_scale(s), exact_offset(o) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return x * scale + offset; }
    constexpr long double from_base(long double b) const noexcept { return (b - offset) / scale; }

    constexpr bool has_offset() const noexcept { return !exact_offset.is_zero; }

    template <int P>
    constexpr Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P> pow() const noexcept {
        return Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P>(exact_scale.power(P));
    }
};

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>
constexpr Unit<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>
operator*(Unit<L1,M1,T1,I1,K1,N1,J1,A1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return Unit<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>(a.exact_scale * b.exact_scale);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>
constexpr Unit<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>
operator/(Unit<L1,M1,T1,I1,K1,N1,J1,A1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return Unit<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>(a.exact_scale / b.exact_scale);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(const ExactScale& k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return Unit<L,M,T,I,K,N,J,A>(u.exact_scale * k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(Unit<L,M,T,I,K,N,J,A> u, const ExactScale& k) noexcept { return Unit<L,M,T,I,K,N,J,A>(u.exact_scale * k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator/(Unit<L,M,T,I,K,N,J,A> u, const ExactScale& k) noexcept { return Unit<L,M,T,I,K,N,J,A>(u.exact_scale / k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<-L,-M,-T,-I,-K,-N,-J,-A> operator/(const ExactScale& k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return Unit<-L,-M,-T,-I,-K,-N,-J,-A>(k / u.exact_scale); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(long double k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return u * ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator*(Unit<L,M,T,I,K,N,J,A> u, long double k) noexcept { return u * ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> operator/(Unit<L,M,T,I,K,N,J,A> u, long double k) noexcept { return u / ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<-L,-M,-T,-I,-K,-N,-J,-A> operator/(long double k, Unit<L,M,T,I,K,N,J,A> u) noexcept { return ExactScale::from_long_double(k) / u; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> prefixed(const ExactScale& factor, Unit<L,M,T,I,K,N,J,A> base) noexcept {
    return Unit<L,M,T,I,K,N,J,A>(base.exact_scale * factor);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Unit<L,M,T,I,K,N,J,A> prefixed(long double factor, Unit<L,M,T,I,K,N,J,A> base) noexcept {
    return prefixed(ExactScale::from_long_double(factor), base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct ReciprocalUnit {
    long double scale;
    ExactScale  exact_scale;

    constexpr ReciprocalUnit(const ExactScale& k) noexcept : scale(k.value()), exact_scale(k) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return (x == 0.0L) ? std::numeric_limits<long double>::quiet_NaN() : 1.0L / (x * scale); }
    constexpr long double from_base(long double b) const noexcept { return (b == 0.0L) ? std::numeric_limits<long double>::quiet_NaN() : 1.0L / (b * scale); }
};

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

#define FIZMO_RECIPROCAL_OF(DIM) \
    ::fizmo::units::ReciprocalUnit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

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

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_unit<units::Unit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_unit<units::ReciprocalUnit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_unit<units::NonlinearUnit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_unit_v = is_fizmo_unit<T>::value;

} // namespace fizmo

#endif // FIZMO_CORE_UNIT_HPP
