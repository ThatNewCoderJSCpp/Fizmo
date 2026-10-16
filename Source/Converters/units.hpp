#ifndef FIZMO_CORE_UNIT_HPP
#define FIZMO_CORE_UNIT_HPP

#include <cstdint>
#include <limits>
#include <type_traits>
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


namespace kind {

struct any {
    static constexpr bool generic = true;
    static constexpr bool affine  = false;
    using parent   = any;
    using interval = any;
};

struct none {
    static constexpr bool generic = false;
    static constexpr bool affine  = false;
    using parent   = none;
    using interval = none;
};

} // namespace kind

template <int L_, int M_, int T_, int I_, int K_, int N_, int J_, int A_>
struct kind_base {
    static constexpr int  L = L_;
    static constexpr int  M = M_;
    static constexpr int  T = T_;
    static constexpr int  I = I_;
    static constexpr int  K = K_;
    static constexpr int  N = N_;
    static constexpr int  J = J_;
    static constexpr int  A = A_;
    static constexpr bool generic = false;
    static constexpr bool affine  = false;

    static constexpr Dimension dimension() noexcept { return Dimension{L_,M_,T_,I_,K_,N_,J_,A_}; }
};

#define FIZMO_UNIT_KIND(NAME, DIM)                                                                          \
    struct NAME : ::fizmo::units::kind_base<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A> { \
        using parent   = NAME;                                                                              \
        using interval = NAME;                                                                              \
    }

#define FIZMO_UNIT_SUBKIND(NAME, PARENT)                                                                            \
    struct NAME : ::fizmo::units::kind_base<PARENT::L,PARENT::M,PARENT::T,PARENT::I,PARENT::K,PARENT::N,PARENT::J,PARENT::A> { \
        using parent   = PARENT;                                                                                    \
        using interval = NAME;                                                                                      \
    }

#define FIZMO_UNIT_POINT_KIND(NAME, PARENT, INTERVAL)                                                               \
    struct NAME : ::fizmo::units::kind_base<PARENT::L,PARENT::M,PARENT::T,PARENT::I,PARENT::K,PARENT::N,PARENT::J,PARENT::A> { \
        static constexpr bool affine = true;                                                                        \
        using parent   = PARENT;                                                                                    \
        using interval = INTERVAL;                                                                                  \
    }

namespace detail {

template <class Q, int L,int M,int T,int I,int K,int N,int J,int A>
struct kind_matches : std::integral_constant<bool, Q::L == L && Q::M == M && Q::T == T && Q::I == I && Q::K == K && Q::N == N && Q::J == J && Q::A == A> {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct kind_matches<kind::any, L,M,T,I,K,N,J,A> : std::true_type {};

template <bool C, class X, class Y>
using cond_t = typename std::conditional<C, X, Y>::type;

template <class X, class Y>
struct kinds_compatible : std::integral_constant<bool,
    std::is_same<X, Y>::value || X::generic || Y::generic ||
    std::is_same<typename X::parent, Y>::value || std::is_same<X, typename Y::parent>::value> {};

template <class From, class To>
struct implicit_kind : std::integral_constant<bool,
    std::is_same<From, To>::value || From::generic ||
    (To::generic && !From::affine) ||
    (std::is_same<To, typename From::parent>::value && !From::affine)> {};

template <class X, class Y>
using common_kind_t =
    cond_t<std::is_same<X, Y>::value, X,
    cond_t<X::generic, Y,
    cond_t<Y::generic, X,
    cond_t<std::is_same<typename X::parent, Y>::value, Y,
    cond_t<std::is_same<X, typename Y::parent>::value, X, kind::none>>>>>;

template <class P, class D>
struct offsets_point : std::integral_constant<bool,
    D::generic || std::is_same<D, typename P::interval>::value || std::is_same<D, typename P::parent>::value> {};

template <class X, class Y>
using add_kind_t =
    cond_t<X::affine && Y::affine, kind::none,
    cond_t<X::affine, cond_t<offsets_point<X, Y>::value, X, kind::none>,
    cond_t<Y::affine, cond_t<offsets_point<Y, X>::value, Y, kind::none>,
    common_kind_t<X, Y>>>>;

template <class X, class Y>
using sub_kind_t =
    cond_t<X::affine && Y::affine, cond_t<std::is_same<X, Y>::value, typename X::interval, kind::none>,
    cond_t<X::affine, cond_t<offsets_point<X, Y>::value, X, kind::none>,
    cond_t<Y::affine, kind::none,
    common_kind_t<X, Y>>>>;

template <class Q, int P>
struct pow_kind { using type = cond_t<P == 1, Q, kind::any>; };

template <class Q, int P>
using pow_kind_t = typename pow_kind<Q, P>::type;

} // namespace detail

template <
    int L, // Length
    int M, // Mass
    int T, // Time
    int I, // Electric Current
    int K, // Thermodynamic Temperature
    int N, // Amount of Substance
    int J, // Luminous Intensity
    int A, // Plane Angle
    class Q = kind::any
>
struct Unit {
    static_assert(detail::kind_matches<Q, L,M,T,I,K,N,J,A>::value, "fizmo::units::Unit: the kind's dimension doesn't match the unit's dimension");

    using kind_type = Q;

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

    template <class Q2, typename std::enable_if<!std::is_same<Q2, Q>::value && detail::implicit_kind<Q2, Q>::value, int>::type = 0>
    constexpr Unit(const Unit<L,M,T,I,K,N,J,A,Q2>& o) noexcept
        : scale(o.scale), offset(o.offset), exact_scale(o.exact_scale), exact_offset(o.exact_offset) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return x * scale + offset; }
    constexpr long double from_base(long double b) const noexcept { return (b - offset) / scale; }

    constexpr bool has_offset() const noexcept { return !exact_offset.is_zero; }

    constexpr Unit<L,M,T,I,K,N,J,A> as_generic() const noexcept {
        Unit<L,M,T,I,K,N,J,A> r;
        r.scale        = scale;
        r.offset       = offset;
        r.exact_scale  = exact_scale;
        r.exact_offset = exact_offset;
        return r;
    }

    template <int P>
    constexpr Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P, detail::pow_kind_t<Q, P>> pow() const noexcept {
        static_assert(!Q::affine || P == 1, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be raised to a power; use the *_interval unit or kelvin");
        return Unit<L*P, M*P, T*P, I*P, K*P, N*P, J*P, A*P, detail::pow_kind_t<Q, P>>(exact_scale.power(P));
    }
};

template <class Q2, int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q2> kind_cast(const Unit<L,M,T,I,K,N,J,A,Q>& u) noexcept {
    Unit<L,M,T,I,K,N,J,A,Q2> r;
    r.scale        = u.scale;
    r.offset       = u.offset;
    r.exact_scale  = u.exact_scale;
    r.exact_offset = u.exact_offset;
    return r;
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1, class Q1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class Q2
>
constexpr Unit<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>
operator*(Unit<L1,M1,T1,I1,K1,N1,J1,A1,Q1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2,Q2> b) noexcept {
    static_assert(!Q1::affine && !Q2::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be multiplied; use the *_interval unit or kelvin");
    return Unit<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>(a.exact_scale * b.exact_scale);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1, class Q1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class Q2
>
constexpr Unit<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>
operator/(Unit<L1,M1,T1,I1,K1,N1,J1,A1,Q1> a, Unit<L2,M2,T2,I2,K2,N2,J2,A2,Q2> b) noexcept {
    static_assert(!Q1::affine && !Q2::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be divided; use the *_interval unit or kelvin");
    return Unit<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>(a.exact_scale / b.exact_scale);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator*(const ExactScale& k, Unit<L,M,T,I,K,N,J,A,Q> u) noexcept {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use the *_interval unit or kelvin");
    return Unit<L,M,T,I,K,N,J,A,Q>(u.exact_scale * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator*(Unit<L,M,T,I,K,N,J,A,Q> u, const ExactScale& k) noexcept {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use the *_interval unit or kelvin");
    return Unit<L,M,T,I,K,N,J,A,Q>(u.exact_scale * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator/(Unit<L,M,T,I,K,N,J,A,Q> u, const ExactScale& k) noexcept {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be scaled; use the *_interval unit or kelvin");
    return Unit<L,M,T,I,K,N,J,A,Q>(u.exact_scale / k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<-L,-M,-T,-I,-K,-N,-J,-A> operator/(const ExactScale& k, Unit<L,M,T,I,K,N,J,A,Q> u) noexcept {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't be divided; use the *_interval unit or kelvin");
    return Unit<-L,-M,-T,-I,-K,-N,-J,-A>(k / u.exact_scale);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator*(long double k, Unit<L,M,T,I,K,N,J,A,Q> u) noexcept { return u * ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator*(Unit<L,M,T,I,K,N,J,A,Q> u, long double k) noexcept { return u * ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> operator/(Unit<L,M,T,I,K,N,J,A,Q> u, long double k) noexcept { return u / ExactScale::from_long_double(k); }

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<-L,-M,-T,-I,-K,-N,-J,-A> operator/(long double k, Unit<L,M,T,I,K,N,J,A,Q> u) noexcept { return ExactScale::from_long_double(k) / u; }

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> prefixed(const ExactScale& factor, Unit<L,M,T,I,K,N,J,A,Q> base) noexcept {
    static_assert(!Q::affine, "fizmo::units: a temperature point (celsius, fahrenheit...) can't take a prefix; use the *_interval unit or kelvin");
    return Unit<L,M,T,I,K,N,J,A,Q>(base.exact_scale * factor);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr Unit<L,M,T,I,K,N,J,A,Q> prefixed(long double factor, Unit<L,M,T,I,K,N,J,A,Q> base) noexcept {
    return prefixed(ExactScale::from_long_double(factor), base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q = kind::any>
struct ReciprocalUnit {
    static_assert(detail::kind_matches<Q, L,M,T,I,K,N,J,A>::value, "fizmo::units::ReciprocalUnit: the kind's dimension doesn't match the unit's dimension");

    using kind_type = Q;

    long double scale;
    ExactScale  exact_scale;

    constexpr ReciprocalUnit(const ExactScale& k) noexcept : scale(k.value()), exact_scale(k) {}

    template <class Q2, typename std::enable_if<!std::is_same<Q2, Q>::value && detail::implicit_kind<Q2, Q>::value, int>::type = 0>
    constexpr ReciprocalUnit(const ReciprocalUnit<L,M,T,I,K,N,J,A,Q2>& o) noexcept : scale(o.scale), exact_scale(o.exact_scale) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return (x == 0.0L) ? std::numeric_limits<long double>::quiet_NaN() : 1.0L / (x * scale); }
    constexpr long double from_base(long double b) const noexcept { return (b == 0.0L) ? std::numeric_limits<long double>::quiet_NaN() : 1.0L / (b * scale); }
};

template <class Q2, int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr ReciprocalUnit<L,M,T,I,K,N,J,A,Q2> kind_cast(const ReciprocalUnit<L,M,T,I,K,N,J,A,Q>& u) noexcept {
    return ReciprocalUnit<L,M,T,I,K,N,J,A,Q2>(u.exact_scale);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q = kind::any>
struct NonlinearUnit {
    static_assert(detail::kind_matches<Q, L,M,T,I,K,N,J,A>::value, "fizmo::units::NonlinearUnit: the kind's dimension doesn't match the unit's dimension");

    using kind_type = Q;

    long double (*fwd)(long double);
    long double (*inv)(long double);

    constexpr NonlinearUnit(long double (*f)(long double), long double (*g)(long double)) noexcept : fwd(f), inv(g) {}

    template <class Q2, typename std::enable_if<!std::is_same<Q2, Q>::value && detail::implicit_kind<Q2, Q>::value, int>::type = 0>
    constexpr NonlinearUnit(const NonlinearUnit<L,M,T,I,K,N,J,A,Q2>& o) noexcept : fwd(o.fwd), inv(o.inv) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }

    constexpr long double to_base(long double x)  const noexcept { return fwd(x); }
    constexpr long double from_base(long double b) const noexcept { return inv(b); }
};

template <class Q2, int L,int M,int T,int I,int K,int N,int J,int A, class Q>
constexpr NonlinearUnit<L,M,T,I,K,N,J,A,Q2> kind_cast(const NonlinearUnit<L,M,T,I,K,N,J,A,Q>& u) noexcept {
    return NonlinearUnit<L,M,T,I,K,N,J,A,Q2>(u.fwd, u.inv);
}

template <class Q>
using KindUnit = Unit<Q::L,Q::M,Q::T,Q::I,Q::K,Q::N,Q::J,Q::A,Q>;

template <class Q>
using KindReciprocalUnit = ReciprocalUnit<Q::L,Q::M,Q::T,Q::I,Q::K,Q::N,Q::J,Q::A,Q>;

template <class Q>
using KindNonlinearUnit = NonlinearUnit<Q::L,Q::M,Q::T,Q::I,Q::K,Q::N,Q::J,Q::A,Q>;

#define FIZMO_UNIT_OF(DIM) \
    ::fizmo::units::Unit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

#define FIZMO_RECIPROCAL_OF(DIM) \
    ::fizmo::units::ReciprocalUnit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

#define FIZMO_NONLINEAR_OF(DIM) \
    ::fizmo::units::NonlinearUnit<(DIM).L,(DIM).M,(DIM).T,(DIM).I,(DIM).K,(DIM).N,(DIM).J,(DIM).A>

#define FIZMO_UNIT_OF_KIND(KIND)       ::fizmo::units::KindUnit<KIND>
#define FIZMO_RECIPROCAL_OF_KIND(KIND) ::fizmo::units::KindReciprocalUnit<KIND>
#define FIZMO_NONLINEAR_OF_KIND(KIND)  ::fizmo::units::KindNonlinearUnit<KIND>

namespace kind {

FIZMO_UNIT_KIND(dimensionless,    DIMENSION_DIMENSIONLESS);
FIZMO_UNIT_KIND(length,           DIMENSION_LENGTH);
FIZMO_UNIT_KIND(mass,             DIMENSION_MASS);
FIZMO_UNIT_KIND(time,             DIMENSION_TIME);
FIZMO_UNIT_KIND(current,          DIMENSION_CURRENT);
FIZMO_UNIT_KIND(temperature,      DIMENSION_TEMPERATURE);
FIZMO_UNIT_SUBKIND(temperature_interval, temperature);
FIZMO_UNIT_POINT_KIND(temperature_point, temperature, temperature_interval);
FIZMO_UNIT_KIND(amount,           DIMENSION_AMOUNT);
FIZMO_UNIT_KIND(luminous,         DIMENSION_LUMINOUS);
FIZMO_UNIT_KIND(angle,            DIMENSION_ANGLE);

FIZMO_UNIT_KIND(frequency,        DIMENSION_FREQUENCY);
FIZMO_UNIT_KIND(activity,         DIMENSION_ACTIVITY);
FIZMO_UNIT_KIND(velocity,         DIMENSION_VELOCITY);
FIZMO_UNIT_KIND(acceleration,     DIMENSION_ACCELERATION);
FIZMO_UNIT_KIND(angular_velocity, DIMENSION_ANGULAR_VELOCITY);
FIZMO_UNIT_KIND(area,             DIMENSION_AREA);
FIZMO_UNIT_KIND(volume,           DIMENSION_VOLUME);
FIZMO_UNIT_KIND(density_2d,       DIMENSION_DENSITY_2D);
FIZMO_UNIT_KIND(density_3d,       DIMENSION_DENSITY_3D);
FIZMO_UNIT_KIND(momentum,         DIMENSION_MOMENTUM);
FIZMO_UNIT_SUBKIND(impulse,       momentum);
FIZMO_UNIT_KIND(force,            DIMENSION_FORCE);
FIZMO_UNIT_KIND(energy,           DIMENSION_ENERGY);
FIZMO_UNIT_KIND(torque,           DIMENSION_TORQUE);
FIZMO_UNIT_KIND(power,            DIMENSION_POWER);
FIZMO_UNIT_KIND(pressure,         DIMENSION_PRESSURE);
FIZMO_UNIT_KIND(absorbed_dose,    DIMENSION_ABSORBED_DOSE);
FIZMO_UNIT_KIND(equivalent_dose,  DIMENSION_EQUIVALENT_DOSE);
FIZMO_UNIT_KIND(charge,           DIMENSION_CHARGE);
FIZMO_UNIT_KIND(voltage,          DIMENSION_VOLTAGE);
FIZMO_UNIT_KIND(resistance,       DIMENSION_RESISTANCE);
FIZMO_UNIT_KIND(conductance,      DIMENSION_CONDUCTANCE);
FIZMO_UNIT_KIND(capacitance,      DIMENSION_CAPACITANCE);
FIZMO_UNIT_KIND(inductance,       DIMENSION_INDUCTANCE);
FIZMO_UNIT_KIND(magnetic_flux,    DIMENSION_MAGNETIC_FLUX);
FIZMO_UNIT_KIND(mass_flow_rate,   DIMENSION_MASS_FLOW_RATE);
FIZMO_UNIT_KIND(volume_flow_rate, DIMENSION_VOLUME_FLOW_RATE);
FIZMO_UNIT_KIND(area_flow_rate,   DIMENSION_AREA_FLOW_RATE);
FIZMO_UNIT_KIND(molar_flow_rate,  DIMENSION_MOLAR_FLOW_RATE);
FIZMO_UNIT_KIND(molarity_volume,  DIMENSION_MOLARITY_VOLUME);
FIZMO_UNIT_KIND(molarity_mass,    DIMENSION_MOLARITY_MASS);
FIZMO_UNIT_KIND(specific_area,    DIMENSION_SPECIFIC_AREA);
FIZMO_UNIT_KIND(specific_volume,  DIMENSION_SPECIFIC_VOLUME);

} // namespace kind

namespace detail {

template <> struct pow_kind<kind::length, 2> { using type = kind::area; };
template <> struct pow_kind<kind::length, 3> { using type = kind::volume; };

} // namespace detail

using DimensionlessUnit       = KindUnit<kind::dimensionless>;
using LengthUnit              = KindUnit<kind::length>;
using MassUnit                = KindUnit<kind::mass>;
using TimeUnit                = KindUnit<kind::time>;
using CurrentUnit             = KindUnit<kind::current>;
using TemperatureUnit         = KindUnit<kind::temperature>;
using TemperatureIntervalUnit = KindUnit<kind::temperature_interval>;
using TemperaturePointUnit    = KindUnit<kind::temperature_point>;
using AmountUnit              = KindUnit<kind::amount>;
using LuminousUnit            = KindUnit<kind::luminous>;
using AngleUnit               = KindUnit<kind::angle>;

using FrequencyUnit           = KindUnit<kind::frequency>;
using ActivityUnit            = KindUnit<kind::activity>;
using VelocityUnit            = KindUnit<kind::velocity>;
using AccelerationUnit        = KindUnit<kind::acceleration>;
using AngularVelocityUnit     = KindUnit<kind::angular_velocity>;
using AreaUnit                = KindUnit<kind::area>;
using VolumeUnit              = KindUnit<kind::volume>;
using Density2DUnit           = KindUnit<kind::density_2d>;
using Density3DUnit           = KindUnit<kind::density_3d>;
using MomentumUnit            = KindUnit<kind::momentum>;
using ForceUnit               = KindUnit<kind::force>;
using ImpulseUnit             = KindUnit<kind::impulse>;
using EnergyUnit              = KindUnit<kind::energy>;
using PowerUnit               = KindUnit<kind::power>;
using PressureUnit            = KindUnit<kind::pressure>;
using AbsorbedDoseUnit        = KindUnit<kind::absorbed_dose>;
using EquivalentDoseUnit      = KindUnit<kind::equivalent_dose>;
using ChargeUnit              = KindUnit<kind::charge>;
using VoltageUnit             = KindUnit<kind::voltage>;
using ResistanceUnit          = KindUnit<kind::resistance>;
using ConductanceUnit         = KindUnit<kind::conductance>;
using CapacitanceUnit         = KindUnit<kind::capacitance>;
using InductanceUnit          = KindUnit<kind::inductance>;
using MagneticFluxUnit        = KindUnit<kind::magnetic_flux>;
using TorqueUnit              = KindUnit<kind::torque>;
using MassFlowRateUnit        = KindUnit<kind::mass_flow_rate>;
using VolumeFlowRateUnit      = KindUnit<kind::volume_flow_rate>;
using AreaFlowRateUnit        = KindUnit<kind::area_flow_rate>;
using MolarFlowRateUnit       = KindUnit<kind::molar_flow_rate>;
using MolarityMassUnit        = KindUnit<kind::molarity_mass>;
using MolarityVolumeUnit      = KindUnit<kind::molarity_volume>;
using SpecificAreaUnit        = KindUnit<kind::specific_area>;
using SpecificVolumeUnit      = KindUnit<kind::specific_volume>;

template <class X, class Y>
constexpr bool same_kind(X, Y) noexcept {
    return X::dimension() == Y::dimension() && detail::kinds_compatible<typename X::kind_type, typename Y::kind_type>::value;
}

} // namespace units

template <typename T> struct is_fizmo_unit : std::false_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
struct is_fizmo_unit<units::Unit<L,M,T,I,K,N,J,A,Q>> : std::true_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
struct is_fizmo_unit<units::ReciprocalUnit<L,M,T,I,K,N,J,A,Q>> : std::true_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A, class Q>
struct is_fizmo_unit<units::NonlinearUnit<L,M,T,I,K,N,J,A,Q>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_unit_v = is_fizmo_unit<T>::value;

} // namespace fizmo

#endif // FIZMO_CORE_UNIT_HPP
