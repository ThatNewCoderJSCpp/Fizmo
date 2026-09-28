#ifndef FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP
#define FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP

#include "trig.hpp"

#include <cmath>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline std::size_t iv_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::size_t iv_halvings(std::size_t w) noexcept {
    std::size_t k = static_cast<std::size_t>(std::sqrt(static_cast<double>(w) / 2.0));
    if (k < 2)    k = 2;
    if (k > 4096) k = 4096;
    return k;
}

inline std::size_t iv_err_of(const BigFloat& v, std::size_t want, std::size_t lost) {
    const std::size_t acc = (want > lost + 4) ? (want - lost - 4) : 1;
    const std::size_t L   = v.significand().bit_length();
    return (L > acc) ? (L - acc) : 0;
}

inline bool iv_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

inline BigFloat iv_half() { return BigFloat(BigUInt::one(), false, -1); }

inline BigFloat atan_core(const BigFloat& u0, const BigFloatContext& wc, std::size_t& lost) {
    const BigFloat    one = BigFloat::one();
    const std::size_t k   = iv_halvings(wc.precision);
    BigFloat u = u0;

    for (std::size_t i = 0; i < k; ++i) {
        BigFloat t = BigFloat::mul(u, u, wc);
        t = BigFloat::add(one, t, wc);
        t = math::sqrt(t, wc);
        t = BigFloat::add(one, t, wc);
        u = BigFloat::div(u, t, wc);
    }

    const BigFloat     u2 = BigFloat::mul(u, u, wc);
    const std::int64_t eu = u.get_exp_base2();
    BigFloat      term = u;
    BigFloat      sum  = u;
    std::uint64_t n    = 1;

    for (;; ++n) {
        term = BigFloat::mul(term, u2, wc);
        if (term.is_zero()) break;
        const BigFloat t = BigFloat::div(term, BigFloat(2 * n + 1), wc);
        if (t.is_zero()) break;
        sum = (n & 1u) ? BigFloat::sub(sum, t, wc) : BigFloat::add(sum, t, wc);
        if (t.get_exp_base2() < eu - static_cast<std::int64_t>(wc.precision + 8)) break;
        if (n > wc.precision + 64) break;
    }

    lost = iv_bits_u64(8 * static_cast<std::uint64_t>(k) + 4 * n + 32) + 2;
    return sum.scaled_pow2(static_cast<std::int64_t>(k));
}

inline BigFloat atan_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    const bool               neg = x.signbit();
    const BigFloat           ax  = x.abs();
    const BigFloat::ordering c   = BigFloat::compare(ax, BigFloat::one());
    BigFloat r;

    if (c == BigFloat::ordering::equal) {
        r    = constants::quarter_pi(wc);
        lost = 2;
    } else if (c == BigFloat::ordering::greater) {          // result >= pi/4: no cancellation
        r = BigFloat::sub(constants::half_pi(wc), atan_core(ax.reciprocal(wc), wc, lost), wc);
        lost += 3;
    } else {
        r = atan_core(ax, wc, lost);
    }

    return neg ? -r : r;
}

enum class iv_sel : std::uint8_t { asin_v, acos_v, atan_v, acot_v, asec_v, acsc_v };

inline BigFloat iv_core(iv_sel sel, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    const BigFloat one = BigFloat::one();
    lost = 0;

    switch (sel) {
        case iv_sel::atan_v: return atan_raw(x, wc, lost);

        case iv_sel::acot_v: {                            
            BigFloat r = atan_raw(x.reciprocal(wc), wc, lost);
            lost += 2;
            if (x.signbit()) r = BigFloat::add(constants::pi(wc), r, wc);
            return r;
        }

        case iv_sel::asec_v:
        case iv_sel::acsc_v: {
            const iv_sel s2 = (sel == iv_sel::asec_v) ? iv_sel::acos_v : iv_sel::asin_v;
            BigFloat     r  = iv_core(s2, x.reciprocal(wc), wc, lost);
            lost += 2;
            return r;
        }

        case iv_sel::asin_v: {                              
            const BigFloat a = BigFloat::sub(one, x, wc);
            const BigFloat b = BigFloat::add(one, x, wc);
            const BigFloat d = math::sqrt(BigFloat::mul(a, b, wc), wc);
            if (d.is_zero()) return constants::half_pi(wc).with_sign(x.signbit());
            BigFloat r = atan_raw(BigFloat::div(x, d, wc), wc, lost);
            lost += 3;
            return r;
        }

        default: {                                          
            const BigFloat a = BigFloat::sub(one, x, wc);
            const BigFloat b = BigFloat::add(one, x, wc);
            if (b.is_zero()) return constants::pi(wc);
            const BigFloat t = math::sqrt(BigFloat::div(a, b, wc), wc);
            if (t.is_zero()) return BigFloat::zero();
            BigFloat r = atan_raw(t, wc, lost).scaled_pow2(1);
            lost += 3;
            return r;
        }
    }
}

inline BigFloat iv_dispatch(iv_sel sel, const BigFloat& x, const BigFloatContext& ctx) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t     want = ctx.precision + guard;
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        std::size_t lost = 0;
        const BigFloat v = iv_core(sel, x, wc, lost);
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, iv_err_of(v, want, lost) + 2)) return v.rounded(ctx);
        if (iv_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat at2_core(const BigFloat& y, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    lost = 0;
    const BigFloat r = BigFloat::div(y, x, wc);

    if (!r.is_finite()) {                                  
        lost = 2;
        return constants::half_pi(wc).with_sign(y.signbit());
    }

    if (r.is_zero()) {                                     
        lost = 2;
        if (!x.signbit()) return BigFloat::zero(y.signbit());
        return constants::pi(wc).with_sign(y.signbit());
    }

    BigFloat a = atan_raw(r, wc, lost);
    lost += 2;
    if (!x.signbit()) return a;                            
    const BigFloat p = constants::pi(wc);                  
    lost += 2;
    return y.signbit() ? BigFloat::sub(a, p, wc) : BigFloat::add(a, p, wc);
}

} // namespace detail

inline BigFloat arcsin(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::nan();
    if (x.is_zero())      return x;
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::greater) return BigFloat::nan();
    if (c == BigFloat::ordering::equal)   return constants::half_pi(ctx).with_sign(x.signbit());
    return detail::iv_dispatch(detail::iv_sel::asin_v, x, ctx);
}

inline BigFloat arccos(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::nan();
    if (x.is_zero())      return constants::half_pi(ctx);
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::greater) return BigFloat::nan();
    if (c == BigFloat::ordering::equal)   return x.signbit() ? constants::pi(ctx) : BigFloat::zero();
    return detail::iv_dispatch(detail::iv_sel::acos_v, x, ctx);
}

inline BigFloat arctan(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return constants::half_pi(ctx).with_sign(x.signbit());
    if (x.is_zero())      return x;
    return detail::iv_dispatch(detail::iv_sel::atan_v, x, ctx);
}

inline BigFloat arccot(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? constants::pi(ctx) : BigFloat::zero();
    if (x.is_zero())      return constants::half_pi(ctx);
    return detail::iv_dispatch(detail::iv_sel::acot_v, x, ctx);
}

inline BigFloat arcsec(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return constants::half_pi(ctx);
    if (x.is_zero())      return BigFloat::nan();
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::less)  return BigFloat::nan();
    if (c == BigFloat::ordering::equal) return x.signbit() ? constants::pi(ctx) : BigFloat::zero();
    return detail::iv_dispatch(detail::iv_sel::asec_v, x, ctx);
}

inline BigFloat arccsc(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero(x.signbit());
    if (x.is_zero())      return BigFloat::nan();
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::less)  return BigFloat::nan();
    if (c == BigFloat::ordering::equal) return constants::half_pi(ctx).with_sign(x.signbit());
    return detail::iv_dispatch(detail::iv_sel::acsc_v, x, ctx);
}

FIZMO_MP_TRIG_FORWARD(arcsin)
FIZMO_MP_TRIG_FORWARD(arccos)
FIZMO_MP_TRIG_FORWARD(arctan)
FIZMO_MP_TRIG_FORWARD(arccot)
FIZMO_MP_TRIG_FORWARD(arcsec)
FIZMO_MP_TRIG_FORWARD(arccsc)

inline BigFloat atan2(const BigFloat& y, const BigFloat& x, const BigFloatContext& ctx) {
    if (y.is_nan()       || x.is_nan())       return BigFloat::nan();
    if (y.is_undefined() || x.is_undefined()) return BigFloat::undefined();

    if (y.is_infinite()) {
        if (x.is_infinite()) return BigFloat::undefined();          
        return constants::half_pi(ctx).with_sign(y.signbit());
    }

    if (x.is_infinite()) return x.signbit() ? constants::pi(ctx).with_sign(y.signbit()) : BigFloat::zero(y.signbit());

    if (y.is_zero()) {
        if (x.is_zero()) return BigFloat::undefined();             
        return x.signbit() ? constants::pi(ctx).with_sign(y.signbit()) : BigFloat::zero(y.signbit());
    }

    if (x.is_zero()) return constants::half_pi(ctx).with_sign(y.signbit());
    std::size_t guard = 32;

    for (;;) {
        const std::size_t     want = ctx.precision + guard;
        const BigFloatContext wc(BigFloatContext::clamp_precision(want), RoundingMode::nearest_even);
        std::size_t lost = 0;
        const BigFloat v = detail::at2_core(y, x, wc, lost);
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, detail::iv_err_of(v, want, lost) + 2)) return v.rounded(ctx);
        if (detail::iv_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat atan2(const BigFloat& y, const BigFloat& x) {
    return atan2(y, x, BigFloatContext::current());
}

#define FIZMO_MP_ATAN2_FORWARD()                                                                           \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value             \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat atan2(const A& y, const B& x, const BigFloatContext& c) { return atan2(BigFloat(y), BigFloat(x), c); } \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value             \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat atan2(const A& y, const B& x) { return atan2(BigFloat(y), BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_ATAN2_FORWARD()

#undef FIZMO_MP_ATAN2_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP