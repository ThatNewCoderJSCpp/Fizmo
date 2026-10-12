#ifndef FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP
#define FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP

#include "../abs_min_max.hpp"
#include "bigfloat_bridge.hpp"

namespace fizmo {
namespace math {

namespace edetail {

template <class F>
F inf_signed(bool neg) noexcept { return (neg && !is_fizmo_signed_v<F>) ? F::zero() : F::infinity(neg); }

template <class G>
bool is_integral_value(const G& x) noexcept {
    return x.is_finite() && x.get_fractional_part().is_zero();
}

template <class G>
bool int_to_ll(const G& x, long long& n) noexcept {
    n = 0;
    if (x.is_zero())   return true;
    if (!x.is_finite()) return false;
    if (!(x.exponent_base2() < typename G::sstore_t(63))) return false;
    n = static_cast<long long>(x.get_integer_part_as_int().get_lowest_bits());
    if (x.is_negative()) n = -n;
    return true;
}

template <class G>
long long root_exp_ll(const typename G::sstore_t& e) noexcept {
    const long long v = static_cast<long long>(e.get_lowest_bits());
    return e.is_negative() ? -v : v;
}

template <class F>
F signed_root(const F& mag, bool neg) noexcept {
    if (!neg) return mag;
    if (!is_fizmo_signed_v<F>)  return F::nan();
    if (mag.is_zero())          return F::zero(true);
    if (mag.is_infinite())      return F::infinity(true);
    return F::zero() - mag;
}

template <class G>
bool odd_integer(const G& x) noexcept {                 
    if (x.is_zero()) return false;
    if (!(x.exponent_base2() < typename G::sstore_t(static_cast<long long>(G::mantissa_bits)))) return false;                                   
    return x.get_integer_part_as_int().get_bit(0);
}

} // namespace edetail

namespace ldetail {

using bdetail::is_mp_float;
using bdetail::is_mp_int;
using bdetail::is_operand;
using bdetail::as_float;
using bdetail::common_result;

template <class F, class T>
F widen(const T& v) noexcept { return F(v); }

template <class F, std::size_t B, fizmo::multiprecision::sign S>
F widen(const fizmo::multiprecision::integer<B, S>& n) noexcept {                 
    using GI = typename fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>::guard_t;
    return multiprecision::fmath::float_cast<F>(GI(typename GI::sstore_t(n)));
}

template <class F>
F log_run(const F& a, const F& b, const BigFloat& x, const BigFloat& y) {
    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (a.is_negative()  || b.is_negative())  return F::nan();
    const F one(1);
    if (b == one) return (a == one) ? F::undefined() : F::nan();
    const bool a_inf = a.is_zero() || a.is_infinite();
    const bool b_inf = b.is_zero() || b.is_infinite();
    if (a_inf && b_inf) return F::undefined();

    if (a_inf || b_inf) {
        const bool neg = (a < one) != (b < one);
        return a_inf ? edetail::inf_signed<F>(neg) : F::zero(neg);
    }

    if (a == b) return one;
    return bdetail::eval<F>([&](const fizmo::multiprecision::BigFloatContext& c) { return log(x, y, c); });
}

} // namespace ldetail

namespace pdetail {

using ldetail::is_mp_float;
using ldetail::is_mp_int;
using ldetail::is_operand;
using ldetail::widen;

template <class T>
struct is_int_like : std::integral_constant<bool, is_mp_int<T>::value || std::is_integral<T>::value> {};

template <class A, class B>
struct kind : std::integral_constant<int,
    (std::is_arithmetic<A>::value && std::is_arithmetic<B>::value) ? 0
  : (is_mp_int<A>::value && is_int_like<B>::value)                 ? 1 : 2> {};

template <class A, class B, bool BMp = is_mp_int<B>::value>
struct int_result { using type = A; };

template <class A, class B>
struct int_result<A, B, true> { using type = fizmo_common_type_t<A, B>; };

template <class A, class B>
struct native_result {
    using C = typename std::common_type<A, B>::type;
    using type = typename std::conditional<std::is_floating_point<C>::value, C, double>::type;
};

template <class A, class B, int K = kind<A, B>::value> struct result;
template <class A, class B> struct result<A, B, 0> { using type = typename native_result<A, B>::type; };
template <class A, class B> struct result<A, B, 1> { using type = typename int_result<A, B>::type; };
template <class A, class B> struct result<A, B, 2> { using type = typename ldetail::common_result<A, B>::type; };

template <class T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
exp_parts(T v, std::uint64_t& mag, bool& neg, bool& huge) noexcept {
    neg  = (v < T(0));
    huge = false;
    mag  = neg ? (static_cast<std::uint64_t>(-(v + T(1))) + 1ull) : static_cast<std::uint64_t>(v);
    return true;
}

template <std::size_t B, fizmo::multiprecision::sign S>
bool exp_parts(const fizmo::multiprecision::integer<B, S>& v, std::uint64_t& mag, bool& neg, bool& huge) noexcept {
    if (v.is_undefined()) return false;
    neg  = v.is_negative();
    huge = (v.magnitude().highest_bit() >= 64);
    mag  = v.get_lowest_bits();
    return true;
}

template <class R, class E>
R int_pow(const R& a, const E& e) noexcept {
    std::uint64_t m = 0;
    bool neg = false, huge = false;
    if (!exp_parts(e, m, neg, huge)) return mmdetail::empty_fold<R>::get();
    if (a.is_undefined())            return mmdetail::empty_fold<R>::get();
    if (!huge && m == 0)             return R(1);
    const bool odd  = (m & 1ull) != 0;
    const bool aneg = a.is_negative();
    const auto mag  = a.magnitude();
    if (mag.is_zero()) return neg ? mmdetail::empty_fold<R>::get() : R();   
    const long long hb = mag.highest_bit();
    if (hb == 0) return odd ? a : R(1);                 
    if (neg)  return R();                              
    if (huge) return R();                                

    if (!mag.any_bit_below(static_cast<std::size_t>(hb))) {       
        using U = fizmo::multiprecision::integer<R::bits, fizmo::multiprecision::sign::is_unsigned>;
        const std::uint64_t k = static_cast<std::uint64_t>(hb);
        const bool over = m > (static_cast<std::uint64_t>(R::bits) - 1ull) / k;
        const R r = over ? R() : R(U(std::uint64_t(1)) << static_cast<std::size_t>(k * m));
        return (aneg && odd) ? (R() - r) : r;
    }

    R r(1), p(a);

    for (std::uint64_t t = m; t != 0ull; t >>= 1) {
        if (t & 1ull) r = r * p;
        if (t >> 1)   p = p * p;
    }

    return r;
}

template <class F>
F pow_run(const F& a, const F& b, const BigFloat& x, const BigFloat& y) {
    const F one(1);
    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (b.is_zero())                          return one;
    if (a == one)                             return one;
    if (b == one)                             return a;

    if (b.is_infinite()) {
        const F m = math::abs(a);
        if (m == one) return one;
        return ((m > one) != b.is_negative()) ? F::infinity() : F::zero();
    }

    const bool aneg = a.is_negative();
    const bool bint = edetail::is_integral_value(b);
    const bool rneg = aneg && bint && edetail::odd_integer(b);
    if (a.is_zero())     return b.is_negative() ? edetail::signed_root(F::infinity(), rneg) : edetail::signed_root(F::zero(),     rneg);
    if (a.is_infinite()) return b.is_negative() ? edetail::signed_root(F::zero(),     rneg) : edetail::signed_root(F::infinity(), rneg);
    if (aneg && !bint)   return F::nan();
    return bdetail::eval<F>([&](const fizmo::multiprecision::BigFloatContext& c) { return pow(x, y, c); });
}

} // namespace pdetail

namespace rdetail {

using ldetail::is_mp_float;
using ldetail::is_operand;
using ldetail::widen;

struct degree {
    long long n;
    bool is_int, fits, odd, bad;
    degree() noexcept : n(0), is_int(false), fits(false), odd(false), bad(false) {}
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, degree>::type
read_degree(T v) noexcept {
    degree d;
    d.is_int = true;
    d.odd    = ((static_cast<std::uint64_t>(v) & 1ull) != 0);
    const bool big = (sizeof(T) == 8 && std::is_unsigned<T>::value && static_cast<std::uint64_t>(v) > 0x7FFFFFFFFFFFFFFFull);

    if (!big) {
        d.n    = static_cast<long long>(v);
        d.fits = (d.n != std::numeric_limits<long long>::min());
    }

    return d;
}

template <std::size_t B, fizmo::multiprecision::sign S>
degree read_degree(const fizmo::multiprecision::integer<B, S>& v) noexcept {
    degree d;
    if (v.is_undefined()) { d.bad = true; return d; }
    d.is_int = true;
    d.odd    = ((v.get_lowest_bits() & 1ull) != 0);

    if (v.magnitude().highest_bit() < 63) {
        d.fits = true;
        d.n    = static_cast<long long>(v.get_lowest_bits());
        if (v.is_negative()) d.n = -d.n;
    }

    return d;
}

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, degree>::type
read_degree(T v) noexcept {
    degree d;
    if (!(v == v)) { d.bad = true; return d; }
    if (!(v > -std::numeric_limits<T>::infinity() && v < std::numeric_limits<T>::infinity())) return d;
    if (v != std::floor(v)) return d;
    d.is_int = true;
    if (v >= -9.2e18 && v <= 9.2e18) { d.n = static_cast<long long>(v); d.fits = true; d.odd = ((d.n & 1) != 0); }
    return d;
}

template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
degree read_degree(const fizmo::multiprecision::floatmp<TB, MB, S>& v) noexcept {
    degree d;
    if (v.is_undefined() || v.is_nan()) { d.bad = true; return d; }
    if (!edetail::is_integral_value(v)) return d;
    d.is_int = true;
    d.odd    = edetail::odd_integer(v);
    d.fits   = edetail::int_to_ll(v, d.n);
    if (d.n == std::numeric_limits<long long>::min()) d.fits = false;
    return d;
}

template <class F, class N>
F root_run(const F& x, const N& deg, const BigFloat& a, const BigFloat& n) {
    const degree d = read_degree(deg);
    if (x.is_undefined())    return F::undefined();
    if (x.is_nan() || d.bad) return F::nan();
    const bool xneg = x.is_negative();

    if (!d.is_int) {
        const F fd = widen<F>(deg);
        if (!fd.is_finite()) return F(1);
        if (fd.is_zero())    return F::nan();
        if (xneg)            return F::nan();
        if (x.is_zero())     return fd.is_negative() ? F::infinity() : F::zero();
        if (x.is_infinite()) return fd.is_negative() ? F::zero() : F::infinity();
        if (x == F(1))       return F(1);
        return bdetail::eval<F>([&](const fizmo::multiprecision::BigFloatContext& c) { return nth_root(a, n, c); });
    }

    if (xneg && !d.odd) return F::nan();
    const bool rneg = xneg && d.odd;

    if (!d.fits) {
        if (x.is_zero())     return F::zero(rneg);
        if (x.is_infinite()) return edetail::signed_root(F::infinity(), rneg);
        return bdetail::eval<F>([&](const fizmo::multiprecision::BigFloatContext& c) { return nth_root(a, n, c); });
    }

    if (d.n == 0) return F::nan();
    if (x.is_zero())     return (d.n > 0) ? F::zero(rneg) : edetail::signed_root(F::infinity(), rneg);
    if (x.is_infinite()) return (d.n > 0) ? edetail::signed_root(F::infinity(), rneg) : F::zero(rneg);
    if (d.n == 1) return x;
    return bdetail::eval<F>([&](const fizmo::multiprecision::BigFloatContext& c) { return nth_root(a, n, c); });
}

template <class X, class N, bool Native = (std::is_arithmetic<X>::value && std::is_arithmetic<N>::value)>
struct result { using type = typename ldetail::common_result<X, N>::type; };

template <class X, class N>
struct result<X, N, true> { using type = typename pdetail::native_result<X, N>::type; };

} // namespace rdetail

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP
