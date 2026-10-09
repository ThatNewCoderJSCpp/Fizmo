#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace ztdetail {

double zt_to_double(const BigFloat& x) {
    if (x.is_zero()) return 0.0;
    const double m = std::exp2(gmdetail::gm_log2_fine(x));
    return x.signbit() ? -m : m;
}

BigFloat zt_one_minus(const BigFloat& x) {                      
    if (x.is_zero()) return BigFloat::one();
    const std::int64_t e   = x.get_exp_base2();
    const std::int64_t top = (e > 0) ? e : 0;
    const std::int64_t lsb = (x.exponent() < 0) ? x.exponent() : 0;
    const std::size_t  p   = BigFloatContext::clamp_precision(static_cast<std::size_t>(top - lsb) + 3);
    return BigFloat::sub(BigFloat::one(), x, BigFloatContext(p, RoundingMode::nearest_even));
}

void zt_reduce_half(const BigFloat& y, BigFloat& f, bool& flip) {
    const BigInt q = y.get_integer_part();
    f    = y.get_fractional_part();
    flip = q.is_odd();
    if (f.is_zero()) return;
    if (BigFloat::compare(f.abs(), BigFloat::one().scaled_pow2(-1)) != BigFloat::ordering::greater) return;
    const BigFloatContext ec(BigFloatContext::clamp_precision(static_cast<std::size_t>(-f.exponent()) + 4), RoundingMode::nearest_even);
    f    = f.signbit() ? BigFloat::add(f, BigFloat::one(), ec) : BigFloat::sub(f, BigFloat::one(), ec);
    flip = !flip;
}

BigFloat zt_sticky(const BigFloat& base, bool neg, std::size_t gap, const BigFloatContext& ctx) {
    const BigFloatContext hc(BigFloatContext::clamp_precision(gap + 16), RoundingMode::nearest_even);
    return BigFloat::add(base, BigFloat::one(neg).scaled_pow2(-static_cast<std::int64_t>(gap)), hc).rounded(ctx);
}

bool zt_safe(const BigFloat& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                     
    const double e = std::ceil(lost);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    const std::size_t err = (e <= 0.0) ? 0 : static_cast<std::size_t>(e);
    return constants::bfdetail::round_is_safe(sig, prec, err + 2);
}

std::size_t zt_guard0(const BigFloat& s) {
    const std::int64_t e = s.get_exp_base2();
    return 32 + ((e > 0 && e < 4096) ? static_cast<std::size_t>(e) : 0);
}

std::vector<BigFloat> zt_powers(const BigFloat& s, std::uint64_t M, const BigFloatContext& wc) {
    std::vector<BigFloat>      pw(static_cast<std::size_t>(M) + 1, BigFloat::one());
    std::vector<std::uint64_t> spf(static_cast<std::size_t>(M) + 1, 0);

    for (std::uint64_t k = 2; k <= M; ++k) {
        if (spf[k] == 0) {
            for (std::uint64_t j = k; j <= M; j += k) if (spf[j] == 0) spf[j] = k;
        }

        if (spf[k] == k) pw[k] = exp(-BigFloat::mul(s, ln(BigFloat(k), wc), wc), wc);
        else             pw[k] = BigFloat::mul(pw[spf[k]], pw[k / spf[k]], wc);
    }

    return pw;
}

void zt_borwein_step(BigUInt& c, std::uint64_t n, std::uint64_t i) {
    c.mul_small_mutable(2 * (n + i - 1));
    c.mul_small_mutable(n - i + 1);
    c.div_small_mutable(i * (2 * i - 1));
}

zt_val zt_core(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(want + 2))) != BigFloat::ordering::less) return zt_val{BigFloat::one(), 1.0};
    const double        sd = zt_to_double(s);
    const double        as = std::fabs(sd);
    const std::uint64_t nb = static_cast<std::uint64_t>(std::ceil((static_cast<double>(want) + 3.0) / 2.5431)) + 1;
    std::uint64_t K = 0;

    if (sd >= 2.0) {                                                    
        for (std::uint64_t k = 2; k <= nb / 2; ++k) {
            const double kd = static_cast<double>(k);
            if (-sd * std::log2(kd) + std::log2(1.0 + kd / (sd - 1.0)) <= -static_cast<double>(want) - 3.0) { K = k; break; }
        }
    }

    if (K != 0) {
        const std::uint64_t         M  = K - 1;
        const std::vector<BigFloat> pw = zt_powers(s, M, wc);
        const std::int64_t          F  = static_cast<std::int64_t>(want + zt_bitlen(M)) + 2;
        BigInt A(static_cast<std::int64_t>(0));
        for (std::uint64_t k = 1; k <= M; ++k) A.add_mutable(pw[k].scaled_pow2(F).get_integer_part());
        const double ce = 3.0 * as * std::log(static_cast<double>(M) + 1.0) + 2.0 * std::log2(static_cast<double>(M) + 1.0) + 3.0;
        return zt_val{BigFloat(A).scaled_pow2(-F).rounded(wc), std::log2(ce + 2.0) + 1.0};
    }

    const std::uint64_t         n  = nb;
    const std::vector<BigFloat> pw = zt_powers(s, n, wc);
    BigUInt c  = BigUInt::one();
    BigUInt dn = BigUInt::one();

    for (std::uint64_t i = 1; i <= n; ++i) {
        zt_borwein_step(c, n, i);
        dn.add_mutable(c);
    }

    const std::int64_t F = static_cast<std::int64_t>(want + zt_bitlen(n)) + 2 - (static_cast<std::int64_t>(dn.bit_length()) - 1);
    BigInt  A(static_cast<std::int64_t>(0));
    BigUInt dk = BigUInt::one();
    c = BigUInt::one();

    for (std::uint64_t k = 0; k < n; ++k) {
        BigUInt e = dn;
        e.sub_mutable(dk);
        const BigInt t = BigFloat::mul(BigFloat(e), pw[k + 1], wc).scaled_pow2(F).get_integer_part();
        if (k & 1u) A.sub_mutable(t); else A.add_mutable(t);
        zt_borwein_step(c, n, k + 1);
        dk.add_mutable(c);
    }

    const BigFloat h   = BigFloat::mul(om, constants::ln2(wc), wc).scaled_pow2(-1);
    const BigFloat em1 = BigFloat::mul(sinh(h, wc), exp(h, wc), wc).scaled_pow2(1);      
    const BigFloat q   = BigFloat::div(BigFloat(A).scaled_pow2(-F), BigFloat::mul(BigFloat(dn), em1, wc), wc);
    const double ce  = 3.0 * as * std::log(static_cast<double>(n) + 1.0) + 2.0 * std::log2(static_cast<double>(n) + 1.0) + 4.0;
    const double ah  = std::fabs(zt_to_double(h));
    const double tot = (0.25 + 2.0 * std::sqrt(static_cast<double>(n)) * ce + 1.0) / 0.6 + (6.0 + 5.0 * ah) + 3.0;
    return zt_val{-q, std::log2(tot) + 1.0};                         
}

zt_val zt_chi_low(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    BigFloat f;
    bool     flip = false;
    zt_reduce_half(s.scaled_pow2(-1), f, flip);
    if (f.is_zero()) return zt_val{BigFloat::zero(), 0.0};
    const BigFloat sn = sin(BigFloat::mul(constants::pi(wc), f, wc), wc);
    const BigFloat pw = exp(BigFloat::mul(s, gmdetail::gm_half_ln_two_pi(wc), wc).scaled_pow2(1), wc);
    const BigFloat g  = gamma(om, wc);
    if (!pw.is_finite()) return zt_val{pw, 0.0};
    if (!g.is_finite())  return zt_val{g, 0.0};
    BigFloat v = BigFloat::mul(pw, constants::inv_pi(wc), wc);
    v = BigFloat::mul(v, sn, wc);
    v = BigFloat::mul(v, g, wc);
    const double a = std::fabs(zt_to_double(s)) * 1.8378770664093453;  
    return zt_val{flip ? -v : v, std::log2(10.0 + 3.0 * a) + 1.0};
}

zt_val zt_fe(const BigFloat& s, const BigFloat& om, std::size_t want) {          
    const zt_val c = zt_chi_low(s, om, want);
    if (!c.v.is_finite() || c.v.is_zero()) return c;
    const zt_val z = zt_core(om, s, want);
    if (!z.v.is_finite()) return z;
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    return zt_val{BigFloat::mul(c.v, z.v, wc), std::log2(std::exp2(c.lost) + std::exp2(z.lost) + 1.0) + 1.0};
}

bool zt_neg_int(const BigFloat& s, const BigFloatContext& ctx, BigFloat& out) {
    if (!s.signbit() || !s.get_fractional_part().is_zero()) return false;
    const BigInt k = s.get_integer_part();

    if (k.bit_length() > 62) {
        if (!k.is_even()) return false;
        out = BigFloat::zero();
        return true;
    }

    const std::uint64_t m = k.get_lowest_bits();
    if (m % 2 == 0) { out = BigFloat::zero(); return true; }
    const std::uint64_t n = (m + 1) / 2;
    if (n > constants::detail::bn_index_cap) return false;
    const BigUInt T = constants::tangent_number(n);
    if (T.is_undefined()) return false;
    BigUInt den = constants::bfdetail::unit(2 * n);
    den.sub_small_mutable(1);
    out = BigFloat::div(BigFloat(T, n % 2 == 1, -2 * static_cast<std::int64_t>(n)), BigFloat(den), ctx);   // sign before rounding
    return true;
}

} // namespace ztdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat riemann_zeta(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return s.signbit() ? BigFloat::undefined() : BigFloat::one();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    if (s.is_zero()) return -half;
    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return BigFloat::undefined();          // pole
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(2))) == BigFloat::ordering::equal) return constants::zeta2(ctx);
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(3))) == BigFloat::ordering::equal) return constants::apery(ctx);
    BigFloat out;
    if (ztdetail::zt_neg_int(s, ctx, out)) return out;

    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(ctx.precision + 2))) != BigFloat::ordering::less) {
        return ztdetail::zt_sticky(one, false, ctx.precision + 4, ctx);
    }

    if (s.get_exp_base2() < -static_cast<std::int64_t>(ctx.precision) - 10) {
        return ztdetail::zt_sticky(-half, !s.signbit(), ctx.precision + 8, ctx);
    }

    const BigFloat om    = ztdetail::zt_one_minus(s);
    const bool     fe    = BigFloat::compare(s, half) == BigFloat::ordering::less;
    std::size_t    guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const ztdetail::zt_val r    = fe ? ztdetail::zt_fe(s, om, want) : ztdetail::zt_core(s, om, want);
        if (!r.v.is_finite() || r.v.is_zero()) return r.v;
        if (ztdetail::zt_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return r.v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat riemann_xi(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return BigFloat::infinity();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    const BigFloat t    = (BigFloat::compare(s, half) == BigFloat::ordering::less) ? ztdetail::zt_one_minus(s) : s;
    if (BigFloat::compare(t, one) == BigFloat::ordering::equal) return half;                          // xi(0) = xi(1) = 1/2
    const BigFloat om    = ztdetail::zt_one_minus(t);
    std::size_t    guard = ztdetail::zt_guard0(t);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext  wc(want, RoundingMode::nearest_even);
        const ztdetail::zt_val z = ztdetail::zt_core(t, om, want);
        if (!z.v.is_finite()) return z.v;
        const BigFloat pis = exp(-BigFloat::mul(t, gmdetail::gm_ln_pi(wc), wc).scaled_pow2(-1), wc);
        const BigFloat g   = gamma(t.scaled_pow2(-1), wc);
        if (!g.is_finite()) return g;
        BigFloat v = BigFloat::mul(t, om, wc);                                                        // = -s (s - 1)
        v = BigFloat::mul(v, pis, wc);
        v = BigFloat::mul(v, g, wc);
        v = BigFloat::mul(v, z.v, wc);
        v = (-v).scaled_pow2(-1);
        if (!v.is_finite()) return v;
        const double a    = std::fabs(ztdetail::zt_to_double(t)) * 0.5723649429247001;               // |s| ln(pi) / 2
        const double lost = std::log2(9.0 + 3.0 * a + std::exp2(z.lost)) + 1.0;
        if (ztdetail::zt_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat riemann_chi(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return BigFloat::undefined();                                               // oscillates
    const BigFloat           half = BigFloat::one().scaled_pow2(-1);
    const BigFloat::ordering ch   = BigFloat::compare(s, half);
    if (ch == BigFloat::ordering::equal) return BigFloat::one();                                      // chi(1/2)^2 = 1
    const bool     low = ch == BigFloat::ordering::less;
    const BigFloat om  = ztdetail::zt_one_minus(s);
    const BigFloat a   = low ? s : om;                                                                // chi(s) = 1 / chi(1 - s)
    const BigFloat b   = low ? om : s;
    BigFloat f;
    bool     flip = false;
    ztdetail::zt_reduce_half(a.scaled_pow2(-1), f, flip);
    if (f.is_zero()) return low ? BigFloat::zero() : BigFloat::undefined();
    std::size_t guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t      want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext  wc(want, RoundingMode::nearest_even);
        const ztdetail::zt_val c = ztdetail::zt_chi_low(a, b, want);
        if (!c.v.is_finite()) return low ? c.v : BigFloat::zero(c.v.signbit());
        const BigFloat v    = low ? c.v : c.v.reciprocal(wc);
        const double   lost = low ? c.lost : c.lost + 1.0;
        if (ztdetail::zt_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
