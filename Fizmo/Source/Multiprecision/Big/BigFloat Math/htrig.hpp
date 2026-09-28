#ifndef FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP
#define FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP

#include "exp.hpp"
#include "sqrt_cbrt.hpp"
#include "trig.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline std::size_t hy_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::size_t hy_splits(std::size_t p) noexcept {
    std::size_t s = 3, q = 1;
    while (q < p) { q <<= 1; ++s; }
    return s;
}

inline void hy_small(const BigFloat& ax, const BigFloatContext& wc, BigFloat& sh, BigFloat& ch) {
    const std::size_t  p      = wc.precision;
    const std::size_t  splits = hy_splits(p);
    const BigFloat     u      = ax.scaled_pow2(-static_cast<std::int64_t>(splits));
    const std::int64_t eu     = u.get_exp_base2();
    const BigFloat     u2     = BigFloat::mul(u, u, wc);
    BigFloat term = u;
    BigFloat sum  = u;

    for (std::uint64_t n = 1; ; ++n) {
        term = BigFloat::mul(term, u2, wc);
        term = BigFloat::div(term, BigFloat((2 * n) * (2 * n + 1)), wc);
        sum  = BigFloat::add(sum, term, wc);
        if (term.is_zero()) break;
        if (term.get_exp_base2() < eu - static_cast<std::int64_t>(p + 8)) break;
        if (n > p + 64) break;
    }

    sh = sum;
    BigFloat t = BigFloat::mul(sh, sh, wc);
    ch = math::sqrt(BigFloat::add(BigFloat::one(), t, wc), wc);

    for (std::size_t i = 0; i < splits; ++i) {
        BigFloat ns = BigFloat::mul(sh, ch, wc).scaled_pow2(1);        
        BigFloat ss = BigFloat::mul(sh, sh, wc).scaled_pow2(1);
        ch = BigFloat::add(BigFloat::one(), ss, wc);                   
        sh = ns;
    }
}

inline void hy_raw(const BigFloat& ax, std::size_t want, BigFloat& sh, BigFloat& ch) {
    const std::size_t     slack = 48 + hy_bits_u64(static_cast<std::uint64_t>(want));
    const BigFloatContext wc(want + slack, RoundingMode::nearest_even);
    if (ax.get_exp_base2() < 0) { hy_small(ax, wc, sh, ch); return; }  
    const BigFloat ex = exp(ax, wc);
    if (!ex.is_finite()) { sh = ex; ch = ex; return; }                 
    const BigFloat em = ex.reciprocal(wc);                            
    sh = BigFloat::sub(ex, em, wc).scaled_pow2(-1);
    ch = BigFloat::add(ex, em, wc).scaled_pow2(-1);
}

enum class hyp_sel : std::uint8_t { sinh_v, cosh_v, tanh_v, coth_v, sech_v, csch_v };

inline bool hy_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

inline bool hy_saturates(const BigFloat& ax, std::size_t prec) {
    return BigFloat::compare(ax, BigFloat(static_cast<std::uint64_t>(prec / 2 + 4))) != BigFloat::ordering::less;
}

inline BigFloat hy_dispatch(const BigFloat& x, hyp_sel sel, const BigFloatContext& ctx) {
    const BigFloat ax  = x.abs();
    const bool     neg = x.signbit();
    if ((sel == hyp_sel::tanh_v || sel == hyp_sel::coth_v) && hy_saturates(ax, ctx.precision)) return BigFloat::one(neg);
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = ctx.precision + guard;
        BigFloat sh, ch;
        hy_raw(ax, want, sh, ch);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        BigFloat v;

        switch (sel) {
            case hyp_sel::sinh_v: v = sh.with_sign(neg);                        break;
            case hyp_sel::cosh_v: v = ch;                                       break;
            case hyp_sel::tanh_v: v = BigFloat::div(sh, ch, wc).with_sign(neg); break;
            case hyp_sel::coth_v: v = BigFloat::div(ch, sh, wc).with_sign(neg); break;
            case hyp_sel::sech_v: v = ch.reciprocal(wc);                        break;
            default:              v = sh.reciprocal(wc).with_sign(neg);         break;
        }

        if (!v.is_finite() || v.is_zero()) return v;
        const std::size_t acc = (want > 4) ? (want - 4) : 1;
        const std::size_t L   = v.significand().bit_length();
        const std::size_t err = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (hy_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace detail

inline BigFloat sinh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x;
    if (x.is_zero())      return x;
    return detail::hy_dispatch(x, detail::hyp_sel::sinh_v, ctx);
}

inline BigFloat cosh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::infinity();
    if (x.is_zero())      return BigFloat::one();
    return detail::hy_dispatch(x, detail::hyp_sel::cosh_v, ctx);
}

inline BigFloat tanh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::one(x.signbit());
    if (x.is_zero())      return x;
    return detail::hy_dispatch(x, detail::hyp_sel::tanh_v, ctx);
}

inline BigFloat coth(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::one(x.signbit());
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::hy_dispatch(x, detail::hyp_sel::coth_v, ctx);
}

inline BigFloat sech(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero();
    if (x.is_zero())      return BigFloat::one();
    return detail::hy_dispatch(x, detail::hyp_sel::sech_v, ctx);
}

inline BigFloat csch(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero(x.signbit());
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::hy_dispatch(x, detail::hyp_sel::csch_v, ctx);
}

FIZMO_MP_TRIG_FORWARD(sinh)
FIZMO_MP_TRIG_FORWARD(cosh)
FIZMO_MP_TRIG_FORWARD(tanh)
FIZMO_MP_TRIG_FORWARD(coth)
FIZMO_MP_TRIG_FORWARD(sech)
FIZMO_MP_TRIG_FORWARD(csch)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP