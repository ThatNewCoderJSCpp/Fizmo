#ifndef FIZMO_UNITS_QUANTITY_HPP
#define FIZMO_UNITS_QUANTITY_HPP

#include "Units/include.hpp"
#include "convert.hpp"

namespace fizmo {
namespace units {

template <int L,int M,int T,int I,int K,int N,int J,int A, class V = long double, class Q = kind::any>
struct Quantity {
    static_assert(detail::kind_matches<Q, L,M,T,I,K,N,J,A>::value, "fizmo::units::Quantity: the kind's dimension doesn't match the quantity's dimension");

    using value_type = V;
    using kind_type  = Q;

    V base;

    constexpr explicit Quantity(V b = V(0)) : base(b) {}

    template <class Q2, typename std::enable_if<!std::is_same<Q2, Q>::value && detail::implicit_kind<Q2, Q>::value, int>::type = 0>
    constexpr Quantity(const Quantity<L,M,T,I,K,N,J,A,V,Q2>& o) : base(o.base) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr Quantity<L,M,T,I,K,N,J,A,V> as_generic() const { return Quantity<L,M,T,I,K,N,J,A,V>(base); }
};

template <class Q2, int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,V,Q2> kind_cast(const Quantity<L,M,T,I,K,N,J,A,V,Q>& q) {
    return Quantity<L,M,T,I,K,N,J,A,V,Q2>(q.base);
}

template <class U, class V = long double>
using QuantityOf = Quantity<U::dimension().L,U::dimension().M,U::dimension().T,U::dimension().I,U::dimension().K,U::dimension().N,U::dimension().J,U::dimension().A, V, typename U::kind_type>;

template <class Q, class V = long double>
using KindQuantity = Quantity<Q::L,Q::M,Q::T,Q::I,Q::K,Q::N,Q::J,Q::A, V, Q>;

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,long double,Q> quantity(long double value, Unit<L,M,T,I,K,N,J,A,Q> u) noexcept {
    return Quantity<L,M,T,I,K,N,J,A,long double,Q>(u.to_base(value));
}

template <class V, int L,int M,int T,int I,int K,int N,int J,int A, class Q, typename std::enable_if<detail::is_mp_value<V>::value, int>::type = 0>
Quantity<L,M,T,I,K,N,J,A, detail::value_result_t<V>, Q> quantity(const V& value, Unit<L,M,T,I,K,N,J,A,Q> u) {
    return Quantity<L,M,T,I,K,N,J,A, detail::value_result_t<V>, Q>(to_base(value, u));
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat, Q> quantity(const multiprecision::BigFloat& value, Unit<L,M,T,I,K,N,J,A,Q> u, const multiprecision::BigFloatContext& ctx) {
    return Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat, Q>(to_base(value, u, ctx));
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q1, class Q2>
constexpr long double in(Quantity<L,M,T,I,K,N,J,A,long double,Q1> q, Unit<L,M,T,I,K,N,J,A,Q2> u) noexcept {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "in: the quantity and the unit are different kinds (energy and torque, say); use kind_cast if this is intended");
    return u.from_base(q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2, typename std::enable_if<detail::is_mp_value<V>::value, int>::type = 0>
V in(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& q, Unit<L,M,T,I,K,N,J,A,Q2> u) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "in: the quantity and the unit are different kinds (energy and torque, say); use kind_cast if this is intended");
    return from_base(q.base, u);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q1, class Q2>
multiprecision::BigFloat in(const Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat, Q1>& q, Unit<L,M,T,I,K,N,J,A,Q2> u, const multiprecision::BigFloatContext& ctx) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "in: the quantity and the unit are different kinds (energy and torque, say); use kind_cast if this is intended");
    return from_base(q.base, u, ctx);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1, class Q1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class Q2, class V
>
constexpr Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2,V>
operator*(const Quantity<L1,M1,T1,I1,K1,N1,J1,A1,V,Q1>& a, const Quantity<L2,M2,T2,I2,K2,N2,J2,A2,V,Q2>& b) {
    static_assert(!Q1::affine && !Q2::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be multiplied; use a temperature interval or kelvin");
    return Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2,V>(a.base * b.base);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1, class Q1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class Q2, class V
>
constexpr Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2,V>
operator/(const Quantity<L1,M1,T1,I1,K1,N1,J1,A1,V,Q1>& a, const Quantity<L2,M2,T2,I2,K2,N2,J2,A2,V,Q2>& b) {
    static_assert(!Q1::affine && !Q2::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be divided; use a temperature interval or kelvin");
    return Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2,V>(a.base / b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr auto operator+(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    using R = detail::add_kind_t<Q1, Q2>;
    static_assert(!std::is_same<R, kind::none>::value, "fizmo::units: these quantities can't be added (different kinds such as energy and torque, or two temperature points)");
    return Quantity<L,M,T,I,K,N,J,A,V, detail::cond_t<std::is_same<R, kind::none>::value, kind::any, R>>(a.base + b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr auto operator-(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    using R = detail::sub_kind_t<Q1, Q2>;
    static_assert(!std::is_same<R, kind::none>::value, "fizmo::units: these quantities can't be subtracted (different kinds such as energy and torque, or an interval minus a temperature point)");
    return Quantity<L,M,T,I,K,N,J,A,V, detail::cond_t<std::is_same<R, kind::none>::value, kind::any, R>>(a.base - b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,V,Q> operator-(const Quantity<L,M,T,I,K,N,J,A,V,Q>& a) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be negated; use a temperature interval or kelvin");
    return Quantity<L,M,T,I,K,N,J,A,V,Q>(V(0) - a.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,V,Q> operator*(const Quantity<L,M,T,I,K,N,J,A,V,Q>& a, const typename Quantity<L,M,T,I,K,N,J,A,V,Q>::value_type& k) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use a temperature interval or kelvin");
    return Quantity<L,M,T,I,K,N,J,A,V,Q>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,V,Q> operator*(const typename Quantity<L,M,T,I,K,N,J,A,V,Q>::value_type& k, const Quantity<L,M,T,I,K,N,J,A,V,Q>& a) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use a temperature interval or kelvin");
    return Quantity<L,M,T,I,K,N,J,A,V,Q>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<L,M,T,I,K,N,J,A,V,Q> operator/(const Quantity<L,M,T,I,K,N,J,A,V,Q>& a, const typename Quantity<L,M,T,I,K,N,J,A,V,Q>::value_type& k) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use a temperature interval or kelvin");
    return Quantity<L,M,T,I,K,N,J,A,V,Q>(a.base / k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V> reciprocal(const Quantity<L,M,T,I,K,N,J,A,V,Q>& q) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be inverted; use a temperature interval or kelvin");
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V>(V(1) / q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V> operator/(const typename Quantity<L,M,T,I,K,N,J,A,V,Q>::value_type& k, const Quantity<L,M,T,I,K,N,J,A,V,Q>& q) {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be inverted; use a temperature interval or kelvin");
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V>(k / q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator==(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return a.base == b.base;
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator!=(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return !(a.base == b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator<(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return a.base < b.base;
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator>(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return b.base < a.base;
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator<=(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return !(b.base < a.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, class Q1, class Q2>
constexpr bool operator>=(const Quantity<L,M,T,I,K,N,J,A,V,Q1>& a, const Quantity<L,M,T,I,K,N,J,A,V,Q2>& b) {
    static_assert(detail::kinds_compatible<Q1, Q2>::value, "fizmo::units: these quantities are different kinds (energy and torque, say) and can't be compared");
    return !(a.base < b.base);
}

using DimensionlessQuantity       = QuantityOf<DimensionlessUnit>;
using LengthQuantity              = QuantityOf<LengthUnit>;
using MassQuantity                = QuantityOf<MassUnit>;
using TimeQuantity                = QuantityOf<TimeUnit>;
using CurrentQuantity             = QuantityOf<CurrentUnit>;
using TemperatureQuantity         = QuantityOf<TemperatureUnit>;
using TemperatureIntervalQuantity = QuantityOf<TemperatureIntervalUnit>;
using TemperaturePointQuantity    = QuantityOf<TemperaturePointUnit>;
using AmountQuantity              = QuantityOf<AmountUnit>;
using LuminousQuantity            = QuantityOf<LuminousUnit>;
using AngleQuantity               = QuantityOf<AngleUnit>;
using FrequencyQuantity           = QuantityOf<FrequencyUnit>;
using ActivityQuantity            = QuantityOf<ActivityUnit>;
using VelocityQuantity            = QuantityOf<VelocityUnit>;
using AccelerationQuantity        = QuantityOf<AccelerationUnit>;
using AngularVelocityQuantity     = QuantityOf<AngularVelocityUnit>;
using AreaQuantity                = QuantityOf<AreaUnit>;
using VolumeQuantity              = QuantityOf<VolumeUnit>;
using Density2DQuantity           = QuantityOf<Density2DUnit>;
using Density3DQuantity           = QuantityOf<Density3DUnit>;
using MomentumQuantity            = QuantityOf<MomentumUnit>;
using ForceQuantity               = QuantityOf<ForceUnit>;
using ImpulseQuantity             = QuantityOf<ImpulseUnit>;
using EnergyQuantity              = QuantityOf<EnergyUnit>;
using PowerQuantity               = QuantityOf<PowerUnit>;
using PressureQuantity            = QuantityOf<PressureUnit>;
using AbsorbedDoseQuantity        = QuantityOf<AbsorbedDoseUnit>;
using EquivalentDoseQuantity      = QuantityOf<EquivalentDoseUnit>;
using ChargeQuantity              = QuantityOf<ChargeUnit>;
using VoltageQuantity             = QuantityOf<VoltageUnit>;
using ResistanceQuantity          = QuantityOf<ResistanceUnit>;
using ConductanceQuantity         = QuantityOf<ConductanceUnit>;
using CapacitanceQuantity         = QuantityOf<CapacitanceUnit>;
using InductanceQuantity          = QuantityOf<InductanceUnit>;
using MagneticFluxQuantity        = QuantityOf<MagneticFluxUnit>;
using TorqueQuantity              = QuantityOf<TorqueUnit>;
using MassFlowRateQuantity        = QuantityOf<MassFlowRateUnit>;
using VolumeFlowRateQuantity      = QuantityOf<VolumeFlowRateUnit>;
using AreaFlowRateQuantity        = QuantityOf<AreaFlowRateUnit>;
using MolarFlowRateQuantity       = QuantityOf<MolarFlowRateUnit>;
using MolarityMassQuantity        = QuantityOf<MolarityMassUnit>;
using MolarityVolumeQuantity      = QuantityOf<MolarityVolumeUnit>;
using SpecificAreaQuantity        = QuantityOf<SpecificAreaUnit>;
using SpecificVolumeQuantity      = QuantityOf<SpecificVolumeUnit>;

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_QUANTITY_HPP
