#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace acdetail {

BigFloat ac_once(ac_kind k, std::uint64_t n, std::size_t wp, std::size_t& lost) {
    if (k == ac_kind::beraha) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(wp), RoundingMode::nearest_even);
        const BigFloat x = BigFloat::div(pi(wc), BigFloat(n), wc);
        const BigFloat c = math::cos(x, wc);
        if (!c.is_finite()) return c;
        lost = 8;
        return BigFloat::mul(c, c, wc).scaled_pow2(2);
    }

    const bool          is_ratio = (k == ac_kind::ratio || k == ac_kind::inv_ratio || k == ac_kind::phi_plus);
    const std::uint64_t q        = (k == ac_kind::phi_plus) ? 1 : n;
    const std::uint64_t r        = is_ratio ? (q * q + 4) : (4 * q + 1);
    const std::size_t   N        = wp + 4;
    BigUInt v = bfdetail::sqrt_int_fixed(r, N);          
    if (v.is_undefined()) return BigFloat::undefined();
    BigUInt t = bfdetail::unit(N);
    if (is_ratio) t.mul_small_mutable(q);
    v.add_mutable(t);
    v.shift_right_mutable(1);

    if (k == ac_kind::phi_plus) {
        BigUInt o = bfdetail::unit(N);
        o.mul_small_mutable(n);
        v.add_mutable(o);
    }

    lost = 4;
    BigFloat x(std::move(v), false, -static_cast<std::int64_t>(N));
    if (k != ac_kind::inv_ratio && k != ac_kind::inv_mean) return x;
    lost = 6;
    return x.reciprocal(BigFloatContext(BigFloatContext::clamp_precision(wp), RoundingMode::nearest_even));
}

BigFloat ac_drive(ac_kind k, std::uint64_t n, const BigFloatContext& ctx, const_cache* c) {
    if (
        c != nullptr && c->valid && 
        c->good >= ctx.precision + 8 &&
        bfdetail::round_is_safe(c->v.significand(), ctx.precision, c->err + 2)
    ) {
        return c->v.rounded(ctx);
    }

    std::size_t guard = 32;

    for (;;) {
        const std::size_t wp   = ctx.precision + guard;
        std::size_t       lost = 0;
        const BigFloat    v    = ac_once(k, n, wp, lost);
        if (!v.is_finite() || v.is_zero()) return v;
        const std::size_t good = (wp > lost) ? (wp - lost) : 1;
        const std::size_t L    = v.significand().bit_length();
        const std::size_t err  = (L > good) ? (L - good) : 0;

        if (c != nullptr && (!c->valid || good > c->good)) {
            c->v = v; c->err = err; c->good = good; c->valid = true;
        }

        if (bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (guard >= 8192 || ctx.precision + guard >= BigFloatContext::max_prec / 4) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace acdetail
} // namespace constants
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace constants {

BigFloat metallic_ratio(std::uint64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::one();                        
    if (n == 1) return phi(ctx);                               
    if (n > acdetail::ac_ratio_cap) return BigFloat::undefined();
    return acdetail::ac_drive(acdetail::ac_kind::ratio, n, ctx, nullptr);
}

BigFloat inv_metallic_ratio(std::uint64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::one();
    if (n > acdetail::ac_ratio_cap) return BigFloat::undefined();
    return acdetail::ac_drive(acdetail::ac_kind::inv_ratio, n, ctx, nullptr);
}

BigFloat metallic_mean(std::uint64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::one();
    if (n == 1) return phi(ctx);
    if (n > acdetail::ac_mean_cap) return BigFloat::undefined();
    return acdetail::ac_drive(acdetail::ac_kind::mean, n, ctx, nullptr);
}

BigFloat inv_metallic_mean(std::uint64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::one();
    if (n > acdetail::ac_mean_cap) return BigFloat::undefined();
    return acdetail::ac_drive(acdetail::ac_kind::inv_mean, n, ctx, nullptr);
}

BigFloat beraha(std::uint64_t n, const BigFloatContext& ctx) {
    switch (n) {
        case 0:  return BigFloat::undefined();
        case 1:  return BigFloat(static_cast<std::uint64_t>(4));
        case 2:  return BigFloat::zero();                      
        case 3:  return BigFloat::one();
        case 4:  return BigFloat(static_cast<std::uint64_t>(2));
        case 6:  return BigFloat(static_cast<std::uint64_t>(3));
        case 5:  return acdetail::ac_drive(acdetail::ac_kind::phi_plus, 1, ctx, nullptr);
        case 10: return acdetail::ac_drive(acdetail::ac_kind::phi_plus, 2, ctx, nullptr);
        default: break;
    }
    return acdetail::ac_drive(acdetail::ac_kind::beraha, n, ctx, nullptr);
}

BigFloat silver_constant(const BigFloatContext& ctx) {
    return acdetail::ac_drive(acdetail::ac_kind::beraha, 7, ctx, &acdetail::ac_silver_c_cache());
}

} // namespace constants
} // namespace multiprecision
} // namespace fizmo
