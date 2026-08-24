#ifndef FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP
#define FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP

#include "Details/log_pow_roots_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

template <class T>
typename std::enable_if<std::is_integral<T>::value, double>::type
exp(T v) noexcept { return std::exp(static_cast<double>(v)); }

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
exp(T v) noexcept { return std::exp(v); }

template <std::size_t B, sign S>
fizmo_float_from_int_t<integer<B, S>> exp(const integer<B, S>& n) noexcept {
    using F = fizmo_float_from_int_t<integer<B, S>>;
    using G = typename F::guard_t;
    if (n.is_undefined()) return F::undefined();
    if (n.is_zero())      return F(1);
    const bool neg = n.is_negative();
    if (n.highest_bit() >= 63) return neg ? F::zero() : F::infinity();
    const double ud   = static_cast<double>(n.get_lowest_bits());
    const long long b = edetail::bias_ll(F::math_bits - F::mantissa_bits);
    if (!neg && ud >  static_cast<double>(b) * edetail::LN2_D + 1.0) return F::infinity();
    if ( neg && ud >  static_cast<double>(b + static_cast<long long>(F::mantissa_bits) + 2) * edetail::LN2_D) return F::zero();
    G r = fmath::ipow_u(constants::detail::e_guard_c<G>(), n.get_lowest_bits());
    if (neg) r = r.reciprocal();
    return fmath::float_cast<F>(r);
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> exp(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return F(1);
    if (x.is_infinite())  return x.is_negative() ? F::zero() : F::infinity();
    const double hd   = fmath::head_as_double(x);
    const long long b = edetail::bias_ll(TB - MB);
    if (hd >  static_cast<double>(b) * edetail::LN2_D + 2.0) return F::infinity();
    if (hd < -static_cast<double>(b + static_cast<long long>(MB) + 2) * edetail::LN2_D) return F::zero();
    return fmath::float_cast<F>(edetail::exp_core(fmath::float_cast<G>(x)));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> pow2(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return F(1);
    if (x.is_infinite())  return x.is_negative() ? F::zero() : F::infinity();
    const double hd   = fmath::head_as_double(x);
    const long long b = edetail::bias_ll(TB - MB);
    if (hd >  static_cast<double>(b) + 2.0) return F::infinity();
    if (hd < -static_cast<double>(b + static_cast<long long>(MB) + 2)) return F::zero();
    const G g  = fmath::float_cast<G>(x);
    const G ip = g.get_integer_part();
    long long k = static_cast<long long>(ip.get_integer_part_as_int().get_lowest_bits());
    if (ip.is_negative()) k = -k;
    const G r = edetail::exp_core((g - ip) * constants::detail::ln2_c<G>());   
    return fmath::float_cast<F>(fmath::scalb_i(r, k));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> pow10(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return F(1);
    if (x.is_infinite())  return x.is_negative() ? F::zero() : F::infinity();
    const double hd   = fmath::head_as_double(x);
    const long long b = edetail::bias_ll(TB - MB);
    if (hd >  static_cast<double>(b) * edetail::LOG10_2D + 1.0) return F::infinity();
    if (hd < -static_cast<double>(b + static_cast<long long>(MB) + 2) * edetail::LOG10_2D) return F::zero();
    return fmath::float_cast<F>(edetail::exp_core(fmath::float_cast<G>(x) * constants::detail::ln10_c<G>()));
}

template <class A, class B>
typename std::enable_if<pdetail::is_operand<A>::value && pdetail::is_operand<B>::value, typename pdetail::result<A, B>::type>::type
pow(const A& a, const B& b) noexcept { return pdetail::apply<A, B>::run(a, b); }

template <class T>
typename std::enable_if<std::is_integral<T>::value, double>::type
log(T v) noexcept { return std::log(static_cast<double>(v)); }

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
log(T v) noexcept { return std::log(v); }

template <std::size_t B, sign S>
fizmo_float_from_int_t<integer<B, S>> log(const integer<B, S>& n) noexcept {
    using F = fizmo_float_from_int_t<integer<B, S>>;
    using G = typename F::guard_t;
    if (n.is_undefined()) return F::undefined();
    if (n.is_zero())      return edetail::neg_infinity<F>();
    if (n.is_negative())  return F::nan();
    return fmath::float_cast<F>(edetail::log_core(G(typename G::sstore_t(n))));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> log(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return edetail::neg_infinity<F>();
    if (x.is_negative())  return F::nan();
    if (x.is_infinite())  return F::infinity();
    return fmath::float_cast<F>(edetail::log_core(fmath::float_cast<G>(x)));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> log2(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return edetail::neg_infinity<F>();
    if (x.is_negative())  return F::nan();
    if (x.is_infinite())  return F::infinity();
    long long k = 0;
    const G lf = edetail::log_reduce(fmath::float_cast<G>(x), k);
    return fmath::float_cast<F>(lf * constants::detail::inv_ln2_c<G>() + G(k));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> log10(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return edetail::neg_infinity<F>();
    if (x.is_negative())  return F::nan();
    if (x.is_infinite())  return F::infinity();
    long long k = 0;
    const G lf = edetail::log_reduce(fmath::float_cast<G>(x), k);
    const G ln = k ? (lf + G(k) * constants::detail::ln2_c<G>()) : lf;
    return fmath::float_cast<F>(ln * constants::detail::inv_ln10_c<G>());
}

template <class A, class B>
typename std::enable_if<ldetail::is_operand<A>::value && ldetail::is_operand<B>::value, typename ldetail::common_result<A, B>::type>::type
log(const A& arg, const B& base) noexcept {
    using F = typename ldetail::common_result<A, B>::type;
    return ldetail::apply<F>::run(arg, base);
}

template <class T>
typename std::enable_if<std::is_integral<T>::value, double>::type
sqrt(T v) noexcept { return std::sqrt(static_cast<double>(v)); }

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
sqrt(T v) noexcept { return std::sqrt(v); }

template <std::size_t B, sign S>
fizmo_float_from_int_t<integer<B, S>> sqrt(const integer<B, S>& n) noexcept {
    using F = fizmo_float_from_int_t<integer<B, S>>;
    using G = typename F::guard_t;
    if (n.is_undefined()) return F::undefined();
    if (n.is_zero())      return F::zero();
    if (n.is_negative())  return F::nan();
    return fmath::float_cast<F>(edetail::sqrt_core(G(typename G::sstore_t(n))));
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> sqrt(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return x;                  
    if (x.is_negative())  return F::nan();
    if (x.is_infinite())  return F::infinity();
    return fmath::float_cast<F>(edetail::sqrt_core(fmath::float_cast<G>(x)));
}

template <class T>
typename std::enable_if<std::is_integral<T>::value, double>::type
cbrt(T v) noexcept { return std::cbrt(static_cast<double>(v)); }

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
cbrt(T v) noexcept { return std::cbrt(v); }

template <std::size_t B, sign S>
fizmo_float_from_int_t<integer<B, S>> cbrt(const integer<B, S>& n) noexcept {
    using F = fizmo_float_from_int_t<integer<B, S>>;
    using G = typename F::guard_t;
    if (n.is_undefined()) return F::undefined();
    if (n.is_zero())      return F::zero();
    const bool neg = n.is_negative();
    const G g(typename G::sstore_t(math::abs(n)));
    return edetail::signed_root(fmath::float_cast<F>(edetail::cbrt_core(g)), neg);
}

template <std::size_t TB, std::size_t MB, sign S>
floatmp<TB, MB, S> cbrt(const floatmp<TB, MB, S>& x) noexcept {
    using F = floatmp<TB, MB, S>;
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return x;
    if (x.is_infinite())  return edetail::inf_signed<F>(x.is_negative());
    const bool neg = x.is_negative();
    const G g = fmath::float_cast<G>(math::abs(x));
    return edetail::signed_root(fmath::float_cast<F>(edetail::cbrt_core(g)), neg);
}

template <class X, class N>
typename std::enable_if<rdetail::is_operand<X>::value && rdetail::is_operand<N>::value, typename rdetail::result<X, N>::type>::type
nth_root(const X& x, const N& n) noexcept {
    using F = typename rdetail::result<X, N>::type;
    return rdetail::apply<F>::template run<X, N>(x, n);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP