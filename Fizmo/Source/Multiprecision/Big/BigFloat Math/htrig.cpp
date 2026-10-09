#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

void hy_small(const BigFloat& ax, const BigFloatContext& wc, BigFloat& sh, BigFloat& ch) {
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

void hy_raw(const BigFloat& ax, std::size_t want, BigFloat& sh, BigFloat& ch) {
    const std::size_t     slack = 48 + hy_bits_u64(static_cast<std::uint64_t>(want));
    const BigFloatContext wc(want + slack, RoundingMode::nearest_even);
    if (ax.get_exp_base2() < 0) { hy_small(ax, wc, sh, ch); return; }  
    const BigFloat ex = exp(ax, wc);
    if (!ex.is_finite()) { sh = ex; ch = ex; return; }                 
    const BigFloat em = ex.reciprocal(wc);                            
    sh = BigFloat::sub(ex, em, wc).scaled_pow2(-1);
    ch = BigFloat::add(ex, em, wc).scaled_pow2(-1);
}

bool hy_saturates(const BigFloat& ax, std::size_t prec) {
    return BigFloat::compare(ax, BigFloat(static_cast<std::uint64_t>(prec / 2 + 4))) != BigFloat::ordering::less;
}

BigFloat hy_dispatch(const BigFloat& x, hyp_sel sel, const BigFloatContext& ctx) {
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
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat sinh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x;
    if (x.is_zero())      return x;
    return detail::hy_dispatch(x, detail::hyp_sel::sinh_v, ctx);
}

BigFloat cosh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::infinity();
    if (x.is_zero())      return BigFloat::one();
    return detail::hy_dispatch(x, detail::hyp_sel::cosh_v, ctx);
}

BigFloat tanh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::one(x.signbit());
    if (x.is_zero())      return x;
    return detail::hy_dispatch(x, detail::hyp_sel::tanh_v, ctx);
}

BigFloat coth(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::one(x.signbit());
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::hy_dispatch(x, detail::hyp_sel::coth_v, ctx);
}

BigFloat sech(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero();
    if (x.is_zero())      return BigFloat::one();
    return detail::hy_dispatch(x, detail::hyp_sel::sech_v, ctx);
}

BigFloat csch(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero(x.signbit());
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::hy_dispatch(x, detail::hyp_sel::csch_v, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
