#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

std::size_t ef_err_of(const BigFloat& v, std::size_t want, std::size_t lost) {
    const std::size_t acc = (want > lost + 4) ? (want - lost - 4) : 1;
    const std::size_t L   = v.significand().bit_length();
    return (L > acc) ? (L - acc) : 0;
}

long double ef_to_ld(const BigFloat& v) {
    if (!v.is_finite() || v.is_zero()) return 0.0L;
    BigUInt           m = v.significand();
    const std::size_t L = m.bit_length();
    std::int64_t      e = v.exponent();
    if (L > 64) { m.shift_right_mutable(L - 64); e += static_cast<std::int64_t>(L - 64); }
    if (e >  16000) return v.signbit() ? -std::numeric_limits<long double>::infinity() :  std::numeric_limits<long double>::infinity();
    if (e < -16000) return 0.0L;
    const long double r = std::ldexp(static_cast<long double>(m.get_lowest_bits()), static_cast<int>(e));
    return v.signbit() ? -r : r;
}

std::size_t ef_x2_log2e(const BigFloat& ax) {            
    const long double v = ef_x2_ld(ax) * 1.4426950408889634L;
    if (!(v < 1.0e15L)) return BigFloatContext::max_prec / 8;
    return static_cast<std::size_t>(v) + 1;
}

std::size_t ef_x2_bits(const BigFloat& ax) {            
    const std::int64_t e = ax.get_exp_base2();
    if (e == BigFloat::exp_none || e == BigFloat::exp_inf || e <= 0) return 0;
    if (e > 1 << 20) return 1 << 21;
    return static_cast<std::size_t>(2 * e + 2);
}

bool ef_asymptotic(const BigFloat& ax, std::size_t want) {
    const long double need = 0.6931471805599453L * static_cast<long double>(want + 16) + 8.0L;
    return ef_x2_ld(ax) >= need;
}

bool ef_tiny(const BigFloat& x, std::size_t prec) {
    const std::int64_t e = x.get_exp_base2();
    if (e == BigFloat::exp_none) return true;
    if (e == BigFloat::exp_inf)  return false;
    return e < -static_cast<std::int64_t>(prec + 4);
}

BigFloat ef_just_under_one(const BigFloatContext& ctx) {
    if (ctx.rounding_mode == RoundingMode::toward_zero || ctx.rounding_mode == RoundingMode::toward_neg_inf) {
        BigUInt u = constants::bfdetail::unit(ctx.precision);
        u.sub_small_mutable(1);
        return BigFloat(std::move(u), false, -static_cast<std::int64_t>(ctx.precision));
    }

    return BigFloat::one();
}

BigFloat ef_just_over_one(const BigFloatContext& ctx) {
    if (ctx.rounding_mode != RoundingMode::toward_pos_inf) return BigFloat::one();
    BigUInt u = constants::bfdetail::unit(ctx.precision - 1);
    u.add_small_mutable(1);
    return BigFloat(std::move(u), false, -static_cast<std::int64_t>(ctx.precision - 1));
}

BigFloat ef_erf_series(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok) {
    const BigFloat x2    = BigFloat::mul(ax, ax, wc);
    const BigFloat step  = x2.scaled_pow2(1);
    const std::size_t lim = 4 * wc.precision + 4 * ef_x2_log2e(ax) + 256;
    BigFloat      term = BigFloat::one();
    BigFloat      sum  = BigFloat::one();
    std::uint64_t n    = 1;
    ok = true;

    for (;; ++n) {
        term = BigFloat::mul(term, step, wc);
        term = BigFloat::div(term, BigFloat(2 * n + 1), wc);
        sum  = BigFloat::add(sum, term, wc);
        if (term.is_zero()) break;
        if (term.get_exp_base2() < sum.get_exp_base2() - static_cast<std::int64_t>(wc.precision + 8)) break;
        if (n > lim) { ok = false; break; }
    }

    BigFloat r = BigFloat::mul(sum, ax, wc);
    r = BigFloat::mul(r, exp(-x2, wc), wc);
    r = BigFloat::mul(r, constants::two_inv_sqrt_pi(wc), wc);
    lost = ef_bits_u64(4 * n + 64) + ef_x2_bits(ax) + 4;
    return r;
}

BigFloat ef_erfcx_asym(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost) {
    const BigFloat x2 = BigFloat::mul(ax, ax, wc);
    const BigFloat h  = x2.scaled_pow2(1).reciprocal(wc);
    BigFloat      term = BigFloat::one();
    BigFloat      sum  = BigFloat::one();
    std::uint64_t n    = 0;

    for (;;) {
        BigFloat nx = BigFloat::mul(term, h, wc);
        nx = BigFloat::mul(nx, BigFloat(2 * n + 1), wc);
        if (nx.is_zero()) break;
        if (BigFloat::compare(nx, term) != BigFloat::ordering::less) break;   
        term = nx;
        ++n;
        sum = (n & 1u) ? BigFloat::sub(sum, term, wc) : BigFloat::add(sum, term, wc);
        if (term.get_exp_base2() < -static_cast<std::int64_t>(wc.precision + 8)) break;
        if (n > wc.precision + 64) break;
    }

    const std::int64_t et  = term.get_exp_base2();
    const std::size_t  got = (et == BigFloat::exp_none) ? wc.precision : ((et < 0) ? static_cast<std::size_t>(-et) : 0);
    lost = ((got < wc.precision) ? (wc.precision - got) : 0) + ef_bits_u64(4 * n + 64) + 4;
    BigFloat r = BigFloat::mul(sum, constants::inv_sqrt_pi(wc), wc);
    return BigFloat::div(r, ax, wc);
}

BigFloat ef_erfix_asym(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost) {
    const BigFloat x2 = BigFloat::mul(ax, ax, wc);
    const BigFloat h  = x2.scaled_pow2(1).reciprocal(wc);
    BigFloat      term = BigFloat::one();
    BigFloat      sum  = BigFloat::one();
    std::uint64_t n    = 0;

    for (;;) {
        BigFloat nx = BigFloat::mul(term, h, wc);
        nx = BigFloat::mul(nx, BigFloat(2 * n + 1), wc);
        if (nx.is_zero()) break;
        if (BigFloat::compare(nx, term) != BigFloat::ordering::less) break;
        term = nx;
        ++n;
        sum = BigFloat::add(sum, term, wc);
        if (term.get_exp_base2() < -static_cast<std::int64_t>(wc.precision + 8)) break;
        if (n > wc.precision + 64) break;
    }

    const std::int64_t et  = term.get_exp_base2();
    const std::size_t  got = (et == BigFloat::exp_none) ? wc.precision : ((et < 0) ? static_cast<std::size_t>(-et) : 0);
    lost = ((got < wc.precision) ? (wc.precision - got) : 0) + ef_bits_u64(4 * n + 64) + 6;
    BigFloat r = BigFloat::mul(sum, constants::inv_sqrt_pi(wc), wc);
    return BigFloat::div(r, ax, wc);
}

BigFloat ef_erfi_series(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok) {
    const BigFloat    x2  = BigFloat::mul(ax, ax, wc);
    const std::size_t lim = 4 * wc.precision + 4 * ef_x2_log2e(ax) + 256;
    BigFloat      u = ax;
    BigFloat      sum = ax;
    std::uint64_t n = 1;
    ok = true;

    for (;; ++n) {
        u = BigFloat::mul(u, x2, wc);
        u = BigFloat::div(u, BigFloat(n), wc);
        const BigFloat t = BigFloat::div(u, BigFloat(2 * n + 1), wc);
        sum = BigFloat::add(sum, t, wc);
        if (t.is_zero()) break;
        if (t.get_exp_base2() < sum.get_exp_base2() - static_cast<std::int64_t>(wc.precision + 8)) break;
        if (n > lim) { ok = false; break; }
    }

    lost = ef_bits_u64(4 * n + 64) + 4;
    return BigFloat::mul(sum, constants::two_inv_sqrt_pi(wc), wc);
}

BigFloat ef_erfc_pos(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok) {
    ok = true;

    if (ef_asymptotic(ax, wc.precision)) {
        std::size_t    l = 0;
        const BigFloat s = ef_erfcx_asym(ax, wc, l);
        const BigFloat e = exp(-BigFloat::mul(ax, ax, wc), wc);
        lost = l + ef_x2_bits(ax) + 4;
        return BigFloat::mul(s, e, wc);
    }

    const std::size_t     xb = ef_x2_log2e(ax);           // bits lost to 1 - erf
    const BigFloatContext w2(BigFloatContext::clamp_precision(wc.precision + xb + 16), RoundingMode::nearest_even);
    std::size_t    l2 = 0;
    const BigFloat e1 = ef_erf_series(ax, w2, l2, ok);
    lost = (l2 > 16) ? (l2 - 16) : 0;
    return BigFloat::sub(BigFloat::one(), e1, w2);
}

BigFloat ef_erfi_pos(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok) {
    ok = true;

    if (ef_asymptotic(ax, wc.precision)) {
        std::size_t    l = 0;
        const BigFloat s = ef_erfix_asym(ax, wc, l);
        const BigFloat e = exp(BigFloat::mul(ax, ax, wc), wc);
        if (!e.is_finite()) return e;
        lost = l + ef_x2_bits(ax) + 4;
        return BigFloat::mul(s, e, wc);
    }

    return ef_erfi_series(ax, wc, lost, ok);
}

BigFloat ef_dispatch(ef_kind k, const BigFloat& ax, const BigFloatContext& ctx) {
    std::size_t guard = 48;

    for (;;) {
        const std::size_t     want = ctx.precision + guard;
        const BigFloatContext wc(BigFloatContext::clamp_precision(want), RoundingMode::nearest_even);
        std::size_t lost = 0;
        bool        ok   = true;
        BigFloat    v;

        switch (k) {
            case ef_kind::erf_v:  v = ef_erf_series(ax, wc, lost, ok); break;
            case ef_kind::erfc_v: v = ef_erfc_pos(ax, wc, lost, ok);   break;
            default:              v = ef_erfi_pos(ax, wc, lost, ok);   break;
        }

        if (!ok) return BigFloat::undefined();
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, ef_err_of(v, want, lost) + 2)) return v.rounded(ctx);
        if (ef_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
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

BigFloat erf(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::one(x.signbit());
    if (x.is_zero())      return x;
    const BigFloat ax = x.abs();
    if (detail::ef_asymptotic(ax, ctx.precision + 8)) return detail::ef_just_under_one(ctx).with_sign(x.signbit());
    return detail::ef_dispatch(detail::ef_kind::erf_v, ax, ctx).with_sign(x.signbit());
}

BigFloat erfc(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? BigFloat(static_cast<std::uint64_t>(2)) : BigFloat::zero();
    if (x.is_zero())      return BigFloat::one();
    const BigFloat ax = x.abs();

    if (x.signbit()) {                                  
        if (detail::ef_tiny(ax, ctx.precision)) return detail::ef_just_over_one(ctx);
        if (detail::ef_asymptotic(ax, ctx.precision + 8))
            return BigFloat::add(BigFloat::one(), detail::ef_just_under_one(ctx), ctx);
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + 48), RoundingMode::nearest_even);
        return BigFloat::add(BigFloat::one(), detail::ef_dispatch(detail::ef_kind::erf_v, ax, wc), ctx);
    }

    if (detail::ef_tiny(ax, ctx.precision)) return detail::ef_just_under_one(ctx);
    return detail::ef_dispatch(detail::ef_kind::erfc_v, ax, ctx);
}

BigFloat erfi(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x;
    if (x.is_zero())      return x;
    return detail::ef_dispatch(detail::ef_kind::erfi_v, x.abs(), ctx).with_sign(x.signbit());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

BigFloat ef_newton_step(ef_solve k, const BigFloat& x, const BigFloat& target, const BigFloatContext& wc) {
    const BigFloat x2  = BigFloat::mul(x, x, wc);
    const BigFloat hsp = constants::sqrt_pi(wc).scaled_pow2(-1);
    std::size_t    l   = 0;
    bool           ok  = true;
    BigFloat       f;

    switch (k) {
        case ef_solve::erf_s:  f = ef_erf_series(x, wc, l, ok); break;
        case ef_solve::erfc_s: f = ef_erfc_pos(x, wc, l, ok);   break;
        default:               f = ef_erfi_pos(x, wc, l, ok);   break;
    }

    if (!ok || !f.is_finite()) return x;
    BigFloat d = BigFloat::sub(f, target, wc);
    d = BigFloat::mul(d, hsp, wc);
    d = BigFloat::mul(d, exp((k == ef_solve::erfi_s) ? -x2 : x2, wc), wc);
    if (!d.is_finite()) return x;
    return (k == ef_solve::erfc_s) ? BigFloat::add(x, d, wc) : BigFloat::sub(x, d, wc);
}

BigFloat ef_newton(ef_solve k, const BigFloat& seed, const BigFloat& target, std::size_t bits) {
    BigFloat    x   = seed;
    std::size_t acc = 8;

    for (;;) {
        const std::size_t     goal = (acc * 2 < bits) ? (acc * 2) : bits;
        const BigFloatContext wc(BigFloatContext::clamp_precision(goal + ef_x2_bits(x) + 64), RoundingMode::nearest_even);
        x = ef_newton_step(k, x, target, wc);
        if (!x.is_finite()) return x;
        if (goal >= bits) return x;
        acc = goal;
    }
}

BigFloat ef_seed_erfc(const BigFloat& z) {
    const BigFloatContext c(64, RoundingMode::nearest_even);
    const long double     p = ef_to_ld(z) * 0.5L;
    long double t;

    if (p > 1.0e-300L) t = std::sqrt(-2.0L * std::log(p));
    else               t = std::sqrt(-2.0L * (ef_to_ld(ln(z, c)) - 0.6931471805599453L));

    const long double num = 2.515517L + t * (0.802853L + t * 0.010328L);
    const long double den = 1.0L + t * (1.432788L + t * (0.189269L + t * 0.001308L));
    long double       q   = t - num / den;
    if (!(q > 0.0L)) q = 1.0e-3L;
    return BigFloat(q * 0.7071067811865476L);
}

BigFloat ef_seed_erf(const BigFloat& y) {
    const long double t = ef_to_ld(y) * 0.8862269254527580L;
    const long double t2 = t * t;
    return BigFloat(t * (1.0L + t2 * (1.0L / 3.0L + t2 * (7.0L / 30.0L))));
}

long double ef_erfi_ld(long double x) {
    long double u = x, s = x;
    const long double x2 = x * x;

    for (int n = 1; n < 600; ++n) {
        u = u * x2 / static_cast<long double>(n);
        const long double t = u / static_cast<long double>(2 * n + 1);
        s += t;
        if (t < s * 1.0e-19L) break;
    }

    return s * 1.1283791670955126L;
}

BigFloat ef_seed_erfi(const BigFloat& y) {
    const BigFloatContext c(64, RoundingMode::nearest_even);
    const long double     Ly = ef_to_ld(ln(y, c));

    if (Ly > 80.0L) {                                    
        long double x = std::sqrt(Ly);

        for (int i = 0; i < 8; ++i) {
            const long double a = Ly - std::log(x * 1.7724538509055160L);
            if (a > 0.0L) x = std::sqrt(a);
        }

        return BigFloat(x);
    }

    const long double yv = ef_to_ld(y);
    long double       x  = (yv < 1.0L) ? (yv * 0.8862269254527580L) : std::sqrt(std::log(1.0L + yv * yv));

    for (int i = 0; i < 60; ++i) {
        const long double s = (ef_erfi_ld(x) - yv) / (1.1283791670955126L * std::exp(x * x));
        x -= s;
        if (!(x > 0.0L)) x = 1.0e-20L;
        if (std::fabs(s) < 1.0e-18L * ((x > 1.0L) ? x : 1.0L)) break;
    }

    return BigFloat(x);
}

BigFloat ef_inv_dispatch(ef_solve k, const BigFloat& target, const BigFloat& seed, const BigFloatContext& ctx) {
    std::size_t guard = 48;

    for (;;) {
        const BigFloat v = ef_newton(k, seed, target, ctx.precision + guard);
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, 8)) return v.rounded(ctx);
        if (ef_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat ef_exact_sub(const BigFloat& a, const BigFloat& b) {
    const std::size_t w = BigFloatContext::clamp_precision(a.significand_bits() + b.significand_bits() + 8);
    return BigFloat::sub(a, b, BigFloatContext(w, RoundingMode::nearest_even));
}

} // namespace detail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat inv_erf(const BigFloat& y, const BigFloatContext& ctx) {
    if (y.is_nan())       return BigFloat::nan();
    if (y.is_undefined()) return BigFloat::undefined();
    if (y.is_infinite())  return BigFloat::nan();
    if (y.is_zero())      return y;
    const BigFloat           ay = y.abs();
    const BigFloat           h(BigUInt::one(), false, -1);
    const BigFloat::ordering c  = BigFloat::compare(ay, BigFloat::one());
    if (c == BigFloat::ordering::greater) return BigFloat::nan();
    if (c == BigFloat::ordering::equal)   return BigFloat::infinity(y.signbit());

    if (BigFloat::compare(ay, h) != BigFloat::ordering::greater) {
        const BigFloat r = detail::ef_inv_dispatch(detail::ef_solve::erf_s, ay, detail::ef_seed_erf(ay), ctx);
        return r.with_sign(y.signbit());
    }

    const BigFloat z = detail::ef_exact_sub(BigFloat::one(), ay);
    const BigFloat r = detail::ef_inv_dispatch(detail::ef_solve::erfc_s, z, detail::ef_seed_erfc(z), ctx);
    return r.with_sign(y.signbit());
}

BigFloat inv_erfc(const BigFloat& z, const BigFloatContext& ctx) {
    if (z.is_nan())       return BigFloat::nan();
    if (z.is_undefined()) return BigFloat::undefined();
    if (z.is_infinite())  return BigFloat::nan();
    if (z.is_negative())  return BigFloat::nan();
    if (z.is_zero())      return BigFloat::infinity();
    const BigFloat two(static_cast<std::uint64_t>(2));
    const BigFloat::ordering c2 = BigFloat::compare(z, two);
    if (c2 == BigFloat::ordering::greater) return BigFloat::nan();
    if (c2 == BigFloat::ordering::equal)   return BigFloat::infinity(true);
    const BigFloat h(BigUInt::one(), false, -1);
    if (BigFloat::compare(z, BigFloat::one()) == BigFloat::ordering::greater) return -inv_erfc(detail::ef_exact_sub(two, z), ctx);      

    if (BigFloat::compare(z, h) == BigFloat::ordering::greater) { // x near 0: solve on the erf side
        const BigFloat y = detail::ef_exact_sub(BigFloat::one(), z);
        if (y.is_zero()) return BigFloat::zero();
        return detail::ef_inv_dispatch(detail::ef_solve::erf_s, y, detail::ef_seed_erf(y), ctx);
    }

    return detail::ef_inv_dispatch(detail::ef_solve::erfc_s, z, detail::ef_seed_erfc(z), ctx);
}

BigFloat inv_erfi(const BigFloat& y, const BigFloatContext& ctx) {
    if (y.is_nan())       return BigFloat::nan();
    if (y.is_undefined()) return BigFloat::undefined();
    if (y.is_infinite())  return y;
    if (y.is_zero())      return y;
    const BigFloat ay = y.abs();
    const BigFloat r  = detail::ef_inv_dispatch(detail::ef_solve::erfi_s, ay, detail::ef_seed_erfi(ay), ctx);
    return r.with_sign(y.signbit());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

bool ef_abs_ge_one(const BigFloat& v) {
    return v.is_finite() && BigFloat::compare(v.abs(), BigFloat::one()) != BigFloat::ordering::less;
}

BigFloat ef_gen_diff(const BigFloat& x, const BigFloat& b, const BigFloatContext& ctx) {
    const bool  tail = x.is_comparable() && b.is_comparable() && (x.sign() == b.sign()) && ef_abs_ge_one(x) && ef_abs_ge_one(b);
    std::size_t guard = 48;
    std::size_t extra = 0;

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard + extra), RoundingMode::nearest_even);
        BigFloat p, q, v;

        if (tail) {                                       
            p = erfc(x.abs(), wc);
            q = erfc(b.abs(), wc);
            v = x.is_negative() ? BigFloat::sub(p, q, wc) : BigFloat::sub(q, p, wc);
        } else {
            p = erf(x, wc);
            q = erf(b, wc);
            v = BigFloat::sub(p, q, wc);
        }

        if (!v.is_finite()) return v;

        if (v.is_zero()) {                                
            if (ef_guard_exhausted(ctx.precision, guard + extra)) return v;
            extra = extra * 2 + 64;
            continue;
        }

        const std::int64_t ep  = p.get_exp_base2();
        const std::int64_t eq  = q.get_exp_base2();
        const std::int64_t top = (ep == BigFloat::exp_none) ? eq : ((eq == BigFloat::exp_none) ? ep : ((ep > eq) ? ep : eq));
        const std::int64_t ev  = v.get_exp_base2();
        const std::size_t  can = (top != BigFloat::exp_none && top > ev) ? static_cast<std::size_t>(top - ev) : 0;

        if (can > extra) {
            if (ef_guard_exhausted(ctx.precision, guard + extra)) return v.rounded(ctx);
            extra = can + 16;
            continue;
        }

        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, ef_err_of(v, wc.precision, extra + 8) + 2)) return v.rounded(ctx);
        if (ef_guard_exhausted(ctx.precision, guard + extra)) return v.rounded(ctx);
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

BigFloat generalized_erf(const BigFloat& x, const BigFloat& base, const BigFloatContext& ctx) {
    if (x.is_nan()       || base.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || base.is_undefined()) return BigFloat::undefined();
    if (BigFloat::compare(x, base) == BigFloat::ordering::equal) return BigFloat::zero();
    if (x.is_infinite() && base.is_infinite())   return BigFloat(static_cast<std::uint64_t>(2)).with_sign(x.signbit());
    return detail::ef_gen_diff(x, base, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
