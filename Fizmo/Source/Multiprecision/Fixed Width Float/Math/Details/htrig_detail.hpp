#ifndef FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_DETAIL_HPP
#define FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_DETAIL_HPP

#include "log_pow_roots_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace edetail {

constexpr long long HYP_POW3_MAX = 20;      // 3^20 < 2^32, so div_u32 still applies

template <class G>
G sinh_series(const G& t) noexcept {
    using S = typename G::sstore_t;
    if (t.is_zero()) return t;
    const long long P = static_cast<long long>(G::mantissa_bits);
    const S tiny = t.exponent_base2() - S(P + 8);
    const G t2 = t * t;
    G term(t), sum(t);

    for (long long n = 3; n <= 2 * P + 16; n += 2) {
        term = div_ll(term * t2, n * (n - 1));
        if (term.is_zero()) break;
        sum = sum + term;
        if (term.exponent_base2() < tiny) break;
    }

    return sum;
}

template <class G>
G sinh_small(const G& a) noexcept {
    if (a.is_zero()) return a;
    const long long P = static_cast<long long>(G::mantissa_bits);
    long long m = isqrt_ll(P / 6);
    if (m > HYP_POW3_MAX) m = HYP_POW3_MAX;
    std::uint32_t p = 1u;
    for (long long i = 0; i < m; ++i) p *= 3u;
    G s = sinh_series(m ? div_u32(a, p) : a);
    const G three(3);
    for (long long i = 0; i < m; ++i) s = s * (three + fmath::scalb_i(s * s, 2));
    return s;
}

template <class G>
G hyp_half() noexcept { return fmath::scalb_i(G(1), -1); }

template <class G>
double hyp_sat() noexcept {
    const double a = (static_cast<double>(G::mantissa_bits) + 4.0) * 0.5 * LN2_D;
    const double b = static_cast<double>(bias_ll(G::exponent_bits)) * LN2_D * 0.5 - 2.0;
    return (a < b) ? a : b;
}

template <class F>
double hyp_overflow() noexcept {
    return static_cast<double>(bias_ll(F::exponent_bits)) * LN2_D + 2.0;
}

template <class G>
G sinh_core(const G& x) noexcept {
    if (x < hyp_half<G>()) return sinh_small(x);
    const G u = exp_core(x);
    return fmath::scalb_i(u - u.reciprocal(), -1);
}

template <class G>
G cosh_core(const G& x) noexcept {
    if (x < hyp_half<G>()) {
        const G h = sinh_small(fmath::scalb_i(x, -1));
        return G(1) + fmath::scalb_i(h * h, 1);          
    }

    const G u = exp_core(x);
    return fmath::scalb_i(u + u.reciprocal(), -1);
}

template <class G>
G tanh_core(const G& x) noexcept {
    if (x < hyp_half<G>()) {
        const G h  = sinh_small(fmath::scalb_i(x, -1));
        const G ch = G(1) + fmath::scalb_i(h * h, 1);
        return sinh_small(x) / ch;
    }

    const G u2 = exp_core(fmath::scalb_i(x, 1));           
    const G one(1);
    return (u2 - one) / (u2 + one);
}

template <class F>
F sinh_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return x;
    const bool neg = x.is_negative();
    if (x.is_infinite())  return inf_signed<F>(neg);
    if (fmath::head_as_double(x) > hyp_overflow<F>()) return inf_signed<F>(neg);
    const G g = fmath::float_cast<G>(math::abs(x));
    return signed_root(fmath::float_cast<F>(sinh_core(g)), neg);
}

template <class F>
F cosh_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return F(1);
    if (x.is_infinite())  return F::infinity();
    if (fmath::head_as_double(x) > hyp_overflow<F>()) return F::infinity();
    const G g = fmath::float_cast<G>(math::abs(x));
    return fmath::float_cast<F>(cosh_core(g));           
}

template <class F>
F tanh_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return x;
    const bool neg = x.is_negative();
    if (x.is_infinite())  return signed_root(F(1), neg);
    if (fmath::head_as_double(x) > hyp_sat<G>()) return signed_root(F(1), neg);
    const G g = fmath::float_cast<G>(math::abs(x));
    return signed_root(fmath::float_cast<F>(tanh_core(g)), neg);
}

template <class F>
F csch_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    const bool neg = x.is_negative();
    if (x.is_zero())      return inf_signed<F>(neg);
    if (x.is_infinite())  return signed_root(F::zero(), neg);
    if (fmath::head_as_double(x) > hyp_overflow<F>()) return signed_root(F::zero(), neg);
    const G s = sinh_core(fmath::float_cast<G>(math::abs(x)));
    if (s.is_zero()) return inf_signed<F>(neg);
    return signed_root(fmath::float_cast<F>(s.reciprocal()), neg);
}

template <class F>
F sech_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    if (x.is_zero())      return F(1);
    if (x.is_infinite())  return F::zero();
    if (fmath::head_as_double(x) > hyp_overflow<F>()) return F::zero();
    const G c = cosh_core(fmath::float_cast<G>(math::abs(x)));
    if (c.is_zero()) return F::infinity();
    return fmath::float_cast<F>(c.reciprocal());
}

template <class F>
F coth_run(const F& x) noexcept {
    using G = typename F::guard_t;
    if (x.is_undefined()) return F::undefined();
    if (x.is_nan())       return F::nan();
    const bool neg = x.is_negative();
    if (x.is_zero())      return inf_signed<F>(neg);
    if (x.is_infinite())  return signed_root(F(1), neg);
    if (fmath::head_as_double(x) > hyp_sat<G>()) return signed_root(F(1), neg);
    const G t = tanh_core(fmath::float_cast<G>(math::abs(x)));
    if (t.is_zero()) return inf_signed<F>(neg);
    return signed_root(fmath::float_cast<F>(t.reciprocal()), neg);
}

} // namespace edetail

namespace hdetail {

template <class T> T native_sinh(T v) noexcept { return std::sinh(v); }
template <class T> T native_cosh(T v) noexcept { return std::cosh(v); }
template <class T> T native_tanh(T v) noexcept { return std::tanh(v); }
template <class T> T native_csch(T v) noexcept { return T(1) / std::sinh(v); }
template <class T> T native_sech(T v) noexcept { return T(1) / std::cosh(v); }
template <class T> T native_coth(T v) noexcept { return T(1) / std::tanh(v); }

} // namespace hdetail

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_HYPERBOLIC_TRIG_DETAIL_HPP