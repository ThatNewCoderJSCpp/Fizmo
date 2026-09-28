#ifndef FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP
#define FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP

#include "htrig.hpp"
#include "inv_htrig.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace gddetail {

inline bool gd_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

inline bool gd_safe(const BigFloat& v, std::size_t want, std::size_t lost, std::size_t prec) {
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);
    if (lost + 4 >= sig.bit_length()) return false;
    return constants::bfdetail::round_is_safe(sig, prec, lost + 2);
}

inline bool gd_in_domain(const BigFloat& ax) {
    if (BigFloat::compare(ax, BigFloat(1.5)) == BigFloat::ordering::less)                               return true;
    if (BigFloat::compare(ax, BigFloat(static_cast<std::uint64_t>(2))) != BigFloat::ordering::less)    return false;
    std::size_t hb = BigFloatContext::clamp_precision(ax.significand().bit_length() + 64);

    for (;;) {
        const BigFloatContext hc(hb, RoundingMode::nearest_even);
        const BigFloat        d = BigFloat::sub(ax, constants::half_pi(hc), hc);
        if (!d.is_zero() && d.get_exp_base2() > 2 - static_cast<std::int64_t>(hb)) return d.signbit();
        if (hb >= BigFloatContext::max_prec / 4) return false;
        hb = BigFloatContext::clamp_precision(2 * hb);
    }
}

} // namespace gddetail

inline BigFloat gudermannian(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return x;
    const bool     neg = x.signbit();
    const BigFloat ax  = x.abs();
    std::size_t guard = 32;

    for (;;) {
        const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const bool     sat = BigFloat::compare(ax, BigFloat(static_cast<std::uint64_t>(want + 8))) != BigFloat::ordering::less;
        const BigFloat v   = sat ? constants::half_pi(wc) : arctan(sinh(ax, wc), wc);
        if (!v.is_finite()) return v;
        if (gddetail::gd_safe(v, want, 3, ctx.precision)) return v.with_sign(neg).rounded(ctx);
        if (gddetail::gd_guard_exhausted(ctx.precision, guard)) return v.with_sign(neg).rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat inv_gudermannian(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::nan();
    if (x.is_zero())      return x;
    const bool     neg = x.signbit();
    const BigFloat ax  = x.abs();
    if (!gddetail::gd_in_domain(ax)) return BigFloat::nan();
    std::size_t guard = 32;

    for (;;) {
        const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const BigFloat        v = arcsinh(tan(ax, wc), wc);
        if (!v.is_finite()) return v;
        if (gddetail::gd_safe(v, want, 3, ctx.precision)) return v.with_sign(neg).rounded(ctx);
        if (gddetail::gd_guard_exhausted(ctx.precision, guard)) return v.with_sign(neg).rounded(ctx);
        guard *= 2;
    }
}

FIZMO_MP_TRIG_FORWARD(gudermannian)
FIZMO_MP_TRIG_FORWARD(inv_gudermannian)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP