#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

BigFloat atanh_core(const BigFloat& u, const BigFloatContext& wc, std::size_t& lost) {
    const BigFloat one = BigFloat::one();
    const bool     neg = u.signbit();
    const BigFloat au  = u.abs();
    BigFloat r;

    if (BigFloat::compare(au, iv_half()) != BigFloat::ordering::less) {
        const BigFloat num = BigFloat::add(one, au, wc);
        const BigFloat den = BigFloat::sub(one, au, wc);
        if (den.is_zero()) return BigFloat::infinity(neg);
        r    = ln(BigFloat::div(num, den, wc), wc).scaled_pow2(-1);
        lost = 5;
    } else {
        const std::size_t k = iv_halvings(wc.precision);
        BigFloat v = au;

        for (std::size_t i = 0; i < k; ++i) {
            BigFloat t = BigFloat::mul(v, v, wc);
            t = BigFloat::sub(one, t, wc);
            t = math::sqrt(t, wc);
            t = BigFloat::add(one, t, wc);
            v = BigFloat::div(v, t, wc);
        }

        const BigFloat     v2 = BigFloat::mul(v, v, wc);
        const std::int64_t ev = v.get_exp_base2();
        BigFloat      term = v;
        BigFloat      sum  = v;
        std::uint64_t n    = 1;

        for (;; ++n) {
            term = BigFloat::mul(term, v2, wc);
            if (term.is_zero()) break;
            const BigFloat t = BigFloat::div(term, BigFloat(2 * n + 1), wc);
            if (t.is_zero()) break;
            sum = BigFloat::add(sum, t, wc);
            if (t.get_exp_base2() < ev - static_cast<std::int64_t>(wc.precision + 8)) break;
            if (n > wc.precision + 64) break;
        }

        r    = sum.scaled_pow2(static_cast<std::int64_t>(k));
        lost = iv_bits_u64(8 * static_cast<std::uint64_t>(k) + 4 * n + 32) + 2;
    }

    return neg ? -r : r;
}

BigFloat asinh_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    const bool         neg = x.signbit();
    const BigFloat     ax  = x.abs();
    const BigFloat     one = BigFloat::one();
    const std::int64_t E   = ax.get_exp_base2();
    BigFloat r;

    if (E > static_cast<std::int64_t>(wc.precision / 2 + 4)) {      
        r    = BigFloat::add(ln(ax, wc), constants::ln2(wc), wc);
        lost = 4;
    } else if (E >= 0) {                                            
        BigFloat t = BigFloat::mul(ax, ax, wc);
        t = math::sqrt(BigFloat::add(one, t, wc), wc);
        r    = ln(BigFloat::add(ax, t, wc), wc);
        lost = 5;
    } else {                                                       
        BigFloat t = BigFloat::mul(ax, ax, wc);
        t = math::sqrt(BigFloat::add(one, t, wc), wc);
        r = atanh_core(BigFloat::div(ax, t, wc), wc, lost);
        lost += 3;
    }

    return neg ? -r : r;
}

BigFloat acosh_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    BigFloat d = BigFloat::sub(x, BigFloat::one(), wc).scaled_pow2(-1);
    BigFloat r = asinh_raw(math::sqrt(d, wc), wc, lost);
    lost += 3;
    return r.scaled_pow2(1);
}

BigFloat ih_core(ih_sel sel, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost) {
    lost = 0;

    switch (sel) {
        case ih_sel::asinh_v: return asinh_raw(x, wc, lost);
        case ih_sel::acosh_v: return acosh_raw(x, wc, lost);
        case ih_sel::atanh_v: return atanh_core(x, wc, lost);
        case ih_sel::acoth_v: { BigFloat r = atanh_core(x.reciprocal(wc), wc, lost); lost += 2; return r; }
        case ih_sel::asech_v: { BigFloat r = acosh_raw(x.reciprocal(wc), wc, lost);  lost += 2; return r; }
        default:              { BigFloat r = asinh_raw(x.reciprocal(wc), wc, lost);  lost += 2; return r; }
    }
}

BigFloat ih_dispatch(ih_sel sel, const BigFloat& x, const BigFloatContext& ctx) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t     want = ctx.precision + guard;
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        std::size_t lost = 0;
        const BigFloat v = ih_core(sel, x, wc, lost);
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, iv_err_of(v, want, lost) + 2)) return v.rounded(ctx);
        if (iv_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
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

BigFloat arcsinh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x;
    if (x.is_zero())      return x;
    return detail::ih_dispatch(detail::ih_sel::asinh_v, x, ctx);
}

BigFloat arccosh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? BigFloat::nan() : BigFloat::infinity();
    const BigFloat::ordering c = BigFloat::compare(x, BigFloat::one());
    if (c == BigFloat::ordering::less)  return BigFloat::nan();
    if (c == BigFloat::ordering::equal) return BigFloat::zero();
    return detail::ih_dispatch(detail::ih_sel::acosh_v, x, ctx);
}

BigFloat arctanh(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::nan();
    if (x.is_zero())      return x;
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::greater) return BigFloat::nan();
    if (c == BigFloat::ordering::equal)   return BigFloat::infinity(x.signbit());
    return detail::ih_dispatch(detail::ih_sel::atanh_v, x, ctx);
}

BigFloat arccoth(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero(x.signbit());
    if (x.is_zero())      return BigFloat::nan();
    const BigFloat::ordering c = BigFloat::compare(x.abs(), BigFloat::one());
    if (c == BigFloat::ordering::less)  return BigFloat::nan();
    if (c == BigFloat::ordering::equal) return BigFloat::infinity(x.signbit());
    return detail::ih_dispatch(detail::ih_sel::acoth_v, x, ctx);
}

BigFloat arcsech(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::nan();
    if (x.is_zero())      return BigFloat::infinity();             
    if (x.is_negative())  return BigFloat::nan();
    const BigFloat::ordering c = BigFloat::compare(x, BigFloat::one());
    if (c == BigFloat::ordering::greater) return BigFloat::nan();
    if (c == BigFloat::ordering::equal)   return BigFloat::zero();
    return detail::ih_dispatch(detail::ih_sel::asech_v, x, ctx);
}

BigFloat arccsch(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero(x.signbit());
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::ih_dispatch(detail::ih_sel::acsch_v, x, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
