#ifndef FIZMO_CORE_CONVERT_HPP
#define FIZMO_CORE_CONVERT_HPP

#include "units.hpp"
#include "../Basic/constants.hpp"
#include "../Multiprecision/include.hpp"

#include <cmath>
#include <type_traits>

namespace fizmo {

template <typename T> struct is_fizmo_linear_unit : std::false_type {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_linear_unit<units::Unit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <typename T> struct is_fizmo_exact_unit : is_fizmo_linear_unit<T> {};

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct is_fizmo_exact_unit<units::ReciprocalUnit<L,M,T,I,K,N,J,A>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_exact_unit_v = is_fizmo_exact_unit<T>::value;

namespace units {
namespace detail {

using multiprecision::BigFloat;
using multiprecision::BigFloatContext;
using multiprecision::BigInt;
using multiprecision::RoundingMode;

template <class T>
struct is_bigfloat : std::is_same<typename std::decay<T>::type, BigFloat> {};

template <class T>
struct is_fixed_mp : std::integral_constant<bool, math::bdetail::is_fw<typename std::decay<T>::type>::value> {};

template <class T>
struct is_mp_value : std::integral_constant<bool, is_fixed_mp<T>::value || is_bigfloat<T>::value> {};

template <class T>
struct is_value : std::integral_constant<bool, std::is_arithmetic<T>::value || is_mp_value<T>::value> {};

template <class T, class = void>
struct value_result { using type = long double; };

template <class T>
struct value_result<T, typename std::enable_if<is_fixed_mp<T>::value>::type> {
    using type = typename math::bdetail::as_float<typename std::decay<T>::type>::type;
};

template <class T>
struct value_result<T, typename std::enable_if<is_bigfloat<T>::value>::type> { using type = BigFloat; };

template <class T>
using value_result_t = typename value_result<T>::type;

struct rat {
    BigInt n;
    BigInt d;
};

inline rat rat_of(long long v) { return rat{BigInt(v), BigInt(1)}; }

inline rat operator*(const rat& a, const rat& b) { return rat{a.n * b.n, a.d * b.d}; }
inline rat operator+(const rat& a, const rat& b) { return rat{a.n * b.d + b.n * a.d, a.d * b.d}; }

inline BigInt ipow(const BigInt& b, int p) {
    BigInt r(1);
    for (int i = 0; i < p; ++i) r = r * b;
    return r;
}

inline rat rat_of(const BigFloat& v) {
    if (v.is_zero()) return rat_of(0);
    BigInt             n(v.significand(), v.signbit());
    BigInt             d(1);
    const std::int64_t e = v.exponent();
    if (e >= 0) n.shift_left_mutable(static_cast<std::size_t>(e));
    else        d.shift_left_mutable(static_cast<std::size_t>(-e));
    return rat{n, d};
}

inline rat rat_of(const ExactScale& s, const rat& pi) {
    if (s.is_zero) return rat_of(0);
    if (s.binary)  return rat_of(BigFloat(s.binary_value));
    rat r = rat_of(1);

    for (int i = 0; i < s.count; ++i) {
        const BigInt m(s.mant[i]);
        if (s.pow[i] > 0) r.n = r.n * ipow(m, s.pow[i]);
        else              r.d = r.d * ipow(m, -s.pow[i]);
    }

    const BigInt ten(10);
    if (s.exp10 > 0) r.n = r.n * ipow(ten, s.exp10);
    else             r.d = r.d * ipow(ten, -s.exp10);
    if (s.pi_pow > 0) r = r * rat{ipow(pi.n, s.pi_pow), ipow(pi.d, s.pi_pow)};
    if (s.pi_pow < 0) r = r * rat{ipow(pi.d, -s.pi_pow), ipow(pi.n, -s.pi_pow)};
    if (s.negative) r.n = -r.n;
    return r;
}

struct map {
    ExactScale p;
    ExactScale q;
    ExactScale r;
    ExactScale s;
};

inline int pi_weight(const ExactScale& e) noexcept { return (e.is_zero || e.binary) ? 0 : (e.pi_pow < 0 ? -e.pi_pow : e.pi_pow); }
inline int pi_weight(const map& m) noexcept { return pi_weight(m.p) + pi_weight(m.q) + pi_weight(m.r) + pi_weight(m.s); }

template <int L,int M,int T,int I,int K,int N,int J,int A>
map to_base_map(const Unit<L,M,T,I,K,N,J,A>& u) { return map{u.exact_scale, u.exact_offset, ExactScale::zero(), ExactScale::one()}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
map from_base_map(const Unit<L,M,T,I,K,N,J,A>& u) { return map{ExactScale::one(), -u.exact_offset, ExactScale::zero(), u.exact_scale}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
map to_base_map(const ReciprocalUnit<L,M,T,I,K,N,J,A>& u) { return map{ExactScale::zero(), ExactScale::one(), u.exact_scale, ExactScale::zero()}; }

template <int L,int M,int T,int I,int K,int N,int J,int A>
map from_base_map(const ReciprocalUnit<L,M,T,I,K,N,J,A>& u) { return map{ExactScale::zero(), ExactScale::one(), u.exact_scale, ExactScale::zero()}; }

inline map identity_map() { return map{ExactScale::one(), ExactScale::zero(), ExactScale::zero(), ExactScale::one()}; }

struct rmap {
    rat p, q, r, s;
};

inline rmap realize(const map& m, const rat& pi) {
    return rmap{rat_of(m.p, pi), rat_of(m.q, pi), rat_of(m.r, pi), rat_of(m.s, pi)};
}

inline rmap compose(const rmap& a, const rmap& b) {
    return rmap{a.p * b.p + a.q * b.r, a.p * b.q + a.q * b.s, a.r * b.p + a.s * b.r, a.r * b.q + a.s * b.s};
}

inline BigFloat apply_exact(const rmap& m, const BigFloat& x, const BigFloatContext& c) {
    const rat xr  = rat_of(x);
    const rat num = m.p * xr + m.q;
    const rat den = m.r * xr + m.s;
    const BigInt N = num.n * den.d;
    const BigInt D = num.d * den.n;
    if (D.is_zero()) return BigFloat::nan();
    return BigFloat::div(BigFloat(N), BigFloat(D), c);
}

inline BigFloat apply_special(const rmap& m, const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (!m.r.n.is_zero()) {
        if (m.r.d.is_zero()) return BigFloat::nan();
        return BigFloat::div(BigFloat(m.p.n * m.r.d), BigFloat(m.p.d * m.r.n), BigFloatContext(64, RoundingMode::nearest_even));
    }
    const bool flip = (m.p.n.is_negative() != m.s.n.is_negative());
    if (m.p.n.is_zero()) return BigFloat::nan();
    return BigFloat::infinity(x.signbit() != flip);
}

inline BigFloat evaluate(const map& first, const map& second, const BigFloat& x, const BigFloatContext& ctx) {
    const int w = pi_weight(first) + pi_weight(second);

    if (w == 0) {
        const rat  none = rat_of(1);
        const rmap m    = compose(realize(second, none), realize(first, none));
        if (!x.is_finite()) return apply_special(m, x);
        return apply_exact(m, x, ctx);
    }

    const std::size_t err_bits = static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(w) + 1.0))) + 3;
    std::size_t       guard    = 32;

    for (;;) {
        const std::size_t     wp = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(wp, RoundingMode::nearest_even);
        const rat             pi = rat_of(multiprecision::constants::pi(BigFloatContext(wp + 8, RoundingMode::nearest_even)));
        const rmap            m  = compose(realize(second, pi), realize(first, pi));
        if (!x.is_finite()) return apply_special(m, x);
        const BigFloat v = apply_exact(m, x, wc);
        if (!v.is_finite() || v.is_zero()) return v;
        multiprecision::BigUInt sig = v.significand();
        const std::size_t       L   = sig.bit_length();
        if (L < wp) sig.shift_left_mutable(wp - L);
        if (multiprecision::constants::bfdetail::round_is_safe(sig, ctx.precision, err_bits + 2)) return v.rounded(ctx);
        if (guard >= 4096) return v.rounded(ctx);
        guard *= 2;
    }
}

template <class R>
R finish(const map& first, const map& second, const BigFloat& x) {
    if constexpr (is_bigfloat<R>::value) return evaluate(first, second, x, BigFloatContext::current());
    else return math::bdetail::eval<R>([&](const BigFloatContext& c) { return evaluate(first, second, x, c); });
}

template <class FromU, class ToU>
struct same_dim : std::integral_constant<bool, FromU::dimension() == ToU::dimension()> {};

template <class T, class U>
using enable_mp_t = typename std::enable_if<is_mp_value<T>::value && is_fizmo_exact_unit_v<U>, int>::type;

template <class T, class FromU, class ToU>
using enable_mp2_t = typename std::enable_if<is_mp_value<T>::value && is_fizmo_exact_unit_v<FromU> && is_fizmo_exact_unit_v<ToU>, int>::type;

} // namespace detail

template <class FromU, class ToU>
constexpr typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, long double>::type
convert(long double value, FromU from, ToU to) noexcept {
    static_assert(FromU::dimension() == ToU::dimension(), "convert: source and target dimensions differ");
    return to.from_base(from.to_base(value));
}

template <class T, class FromU, class ToU, detail::enable_mp2_t<T, FromU, ToU> = 0>
detail::value_result_t<T> convert(const T& value, FromU from, ToU to) {
    static_assert(FromU::dimension() == ToU::dimension(), "convert: source and target dimensions differ");
    return detail::finish<detail::value_result_t<T>>(detail::to_base_map(from), detail::from_base_map(to), multiprecision::BigFloat(value));
}

template <class FromU, class ToU, typename std::enable_if<is_fizmo_exact_unit_v<FromU> && is_fizmo_exact_unit_v<ToU>, int>::type = 0>
multiprecision::BigFloat convert(const multiprecision::BigFloat& value, FromU from, ToU to, const multiprecision::BigFloatContext& ctx) {
    static_assert(FromU::dimension() == ToU::dimension(), "convert: source and target dimensions differ");
    return detail::evaluate(detail::to_base_map(from), detail::from_base_map(to), value, ctx);
}

template <class U>
constexpr typename std::enable_if<is_fizmo_unit_v<U>, long double>::type
to_base(long double value, U from) noexcept {
    return from.to_base(value);
}

template <class T, class U, detail::enable_mp_t<T, U> = 0>
detail::value_result_t<T> to_base(const T& value, U from) {
    return detail::finish<detail::value_result_t<T>>(detail::to_base_map(from), detail::identity_map(), multiprecision::BigFloat(value));
}

template <class U, typename std::enable_if<is_fizmo_exact_unit_v<U>, int>::type = 0>
multiprecision::BigFloat to_base(const multiprecision::BigFloat& value, U from, const multiprecision::BigFloatContext& ctx) {
    return detail::evaluate(detail::to_base_map(from), detail::identity_map(), value, ctx);
}

template <class U>
constexpr typename std::enable_if<is_fizmo_unit_v<U>, long double>::type
from_base(long double base, U to) noexcept {
    return to.from_base(base);
}

template <class T, class U, detail::enable_mp_t<T, U> = 0>
detail::value_result_t<T> from_base(const T& base, U to) {
    return detail::finish<detail::value_result_t<T>>(detail::identity_map(), detail::from_base_map(to), multiprecision::BigFloat(base));
}

template <class U, typename std::enable_if<is_fizmo_exact_unit_v<U>, int>::type = 0>
multiprecision::BigFloat from_base(const multiprecision::BigFloat& base, U to, const multiprecision::BigFloatContext& ctx) {
    return detail::evaluate(detail::identity_map(), detail::from_base_map(to), base, ctx);
}

template <class A, class B>
constexpr typename std::enable_if<is_fizmo_unit_v<A> && is_fizmo_unit_v<B>, bool>::type
same_dimension(A, B) noexcept {
    return A::dimension() == B::dimension();
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr long double scale_ratio(Unit<L,M,T,I,K,N,J,A> from, Unit<L,M,T,I,K,N,J,A> to) noexcept {
    return from.scale / to.scale;
}

template <class R, int L,int M,int T,int I,int K,int N,int J,int A, typename std::enable_if<detail::is_mp_value<R>::value, int>::type = 0>
R scale_ratio(Unit<L,M,T,I,K,N,J,A> from, Unit<L,M,T,I,K,N,J,A> to) {
    return convert(R(1), Unit<L,M,T,I,K,N,J,A>(from.exact_scale), Unit<L,M,T,I,K,N,J,A>(to.exact_scale));
}

} // namespace units
} // namespace fizmo

#endif // FIZMO_CORE_CONVERT_HPP
