#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

void tg_sincos_small(const BigFloat& r, const BigFloatContext& wc, BigFloat& s, BigFloat& c) {
    if (r.is_zero()) { s = BigFloat::zero(r.signbit()); c = BigFloat::one(); return; }
    const std::size_t p      = wc.precision;
    const std::size_t splits = tg_splits(p);
    const BigFloat    u      = r.scaled_pow2(-static_cast<std::int64_t>(splits));
    const std::int64_t eu    = u.get_exp_base2();
    const BigFloat    u2     = BigFloat::mul(u, u, wc);
    BigFloat term = u;
    BigFloat sum  = u;

    for (std::uint64_t n = 1; ; ++n) {
        term = BigFloat::mul(term, u2, wc);
        term = BigFloat::div(term, BigFloat((2 * n) * (2 * n + 1)), wc);
        term.negate_mutable();
        sum = BigFloat::add(sum, term, wc);
        if (term.is_zero()) break;
        if (term.get_exp_base2() < eu - static_cast<std::int64_t>(p + 8)) break;
        if (n > p + 64) break;
    }

    s = sum;
    BigFloat t = BigFloat::mul(s, s, wc);
    c = math::sqrt(BigFloat::sub(BigFloat::one(), t, wc), wc);

    for (std::size_t i = 0; i < splits; ++i) {
        BigFloat ns = BigFloat::mul(s, c, wc).scaled_pow2(1);          
        BigFloat ss = BigFloat::mul(s, s, wc).scaled_pow2(1);
        c = BigFloat::sub(BigFloat::one(), ss, wc);                    
        s = ns;
    }
}

bool tg_sincos_raw(const BigFloat& x, std::size_t want, BigFloat& sx, BigFloat& cx) {
    const std::int64_t E  = x.get_exp_base2();
    const std::size_t  Ep = (E > 0) ? static_cast<std::size_t>(E) : 0;
    if (Ep > tg_reduce_cap) return false;
    std::size_t lost = 0;

    for (;;) {
        const std::size_t slack = 48 + tg_bits_u64(static_cast<std::uint64_t>(want + Ep + lost));
        const std::size_t w     = want + Ep + lost + slack;
        if (w >= BigFloatContext::max_prec / 2) return false;
        const BigFloatContext wc(w, RoundingMode::nearest_even);
        const BigFloat  q  = BigFloat::mul(x, constants::two_inv_pi(wc), wc);
        BigInt          k  = q.get_integer_part();
        if (k.is_nan() || k.is_undefined()) return false;
        BigFloat        kb(k);
        const BigFloat  half(BigUInt::one(), false, -1);
        const BigFloat  frac = BigFloat::sub(q, kb, wc);

        if (frac.sign() > 0 && BigFloat::compare(frac, half) != BigFloat::ordering::less) {
            kb = BigFloat::add(kb, BigFloat::one(), wc);
            k  = kb.get_integer_part();
        } else if (frac.sign() < 0 && BigFloat::compare(frac, -half) != BigFloat::ordering::greater) {
            kb = BigFloat::sub(kb, BigFloat::one(), wc);
            k  = kb.get_integer_part();
        }

        const BigFloat r = BigFloat::sub(x, BigFloat::mul(kb, constants::half_pi(wc), wc), wc);
        if (r.is_zero()) { lost = lost * 2 + 64; continue; }
        const std::int64_t er = r.get_exp_base2();
        const std::size_t  t  = (er < 0) ? static_cast<std::size_t>(-er) : 0;
        if (t > lost) { lost = t + 8; continue; }
        BigFloat s, c;
        tg_sincos_small(r, wc, s, c);
        const std::uint64_t low  = k.get_lowest_bits() & 3u;
        const unsigned      quad = k.is_negative() ? static_cast<unsigned>((4u - low) & 3u) : static_cast<unsigned>(low);

        switch (quad) {
            case 0: sx =  s;            cx =  c;            break;
            case 1: sx =  c;            cx = -s;            break;
            case 2: sx = -s;            cx = -c;            break;
            default: sx = -c;           cx =  s;            break;
        }

        return true;
    }
}

BigFloat tg_dispatch(const BigFloat& x, trig_sel sel, const BigFloatContext& ctx) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = ctx.precision + guard;
        BigFloat s, c;
        if (!tg_sincos_raw(x, want, s, c)) return BigFloat::undefined();
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        BigFloat v;

        switch (sel) {
            case trig_sel::sin_v: v = s;                        break;
            case trig_sel::cos_v: v = c;                        break;
            case trig_sel::tan_v: v = BigFloat::div(s, c, wc);  break;
            case trig_sel::cot_v: v = BigFloat::div(c, s, wc);  break;
            case trig_sel::sec_v: v = c.reciprocal(wc);         break;
            default:              v = s.reciprocal(wc);         break;
        }

        if (!v.is_finite() || v.is_zero()) return v;
        const std::size_t acc = (want > 4) ? (want - 4) : 1;
        const std::size_t L   = v.significand().bit_length();
        const std::size_t err = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (tg_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat tg_just_under_one(const BigFloatContext& ctx) {
    if (ctx.rounding_mode == RoundingMode::toward_zero || ctx.rounding_mode == RoundingMode::toward_neg_inf) {
        BigUInt u = constants::bfdetail::unit(ctx.precision);
        u.sub_small_mutable(1);
        return BigFloat(std::move(u), false, -static_cast<std::int64_t>(ctx.precision));
    }

    return BigFloat::one();
}

bool tg_negligible(const BigFloat& x, std::size_t prec) {
    const std::int64_t e = x.get_exp_base2();
    if (e == BigFloat::exp_none || e == BigFloat::exp_inf) return false;
    return e < -static_cast<std::int64_t>(prec / 2 + 4);
}

std::size_t tg_err_of(const BigFloat& v, std::size_t want) {
    const std::size_t acc = (want > 4) ? (want - 4) : 1;
    const std::size_t L   = v.significand().bit_length();
    return (L > acc) ? (L - acc) : 0;
}

BigFloat sinc_finite(const BigFloat& x, const BigFloatContext& ctx) {
    if (tg_negligible(x, ctx.precision)) return tg_just_under_one(ctx);
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = ctx.precision + guard;
        BigFloat s, c;
        if (!tg_sincos_raw(x, want, s, c)) return BigFloat::undefined();
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const BigFloat v = BigFloat::div(s, x, wc);
        if (!v.is_finite() || v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, tg_err_of(v, want) + 3)) return v.rounded(ctx);
        if (tg_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
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

BigFloat sin(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();  
    if (x.is_zero())      return x;                       
    return detail::tg_dispatch(x, detail::trig_sel::sin_v, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

void tg_fold_half(const BigFloat& x, BigFloat& f, bool& flip) {
        const BigInt k = x.get_integer_part();
        f    = x.get_fractional_part();
        flip = k.is_odd();
        const BigFloat half(BigUInt::one(), false, -1);
        if (BigFloat::compare(f.abs(), half) != BigFloat::ordering::greater) return;
        const std::size_t     eb = BigFloatContext::clamp_precision(f.significand_bits() + 8);
        const BigFloatContext ec(eb, RoundingMode::nearest_even);   
        f    = f.signbit() ? BigFloat::add(f, BigFloat::one(), ec) : BigFloat::sub(f, BigFloat::one(), ec);
        flip = !flip;
    }

BigFloat normalized_sinc_finite(const BigFloat& x, const BigFloatContext& ctx) {
        if (x.is_integer()) return BigFloat::zero();             
        if (tg_negligible(x, ctx.precision)) return tg_just_under_one(ctx);
        BigFloat f;
        bool     flip = false;
        tg_fold_half(x, f, flip);
        std::size_t guard = 32;

        for (;;) {
            const std::size_t     want = ctx.precision + guard;
            const BigFloatContext wc(want, RoundingMode::nearest_even);
            const BigFloat p   = constants::pi(wc);
            const BigFloat num = math::sin(BigFloat::mul(f, p, wc), wc);      
            const BigFloat den = BigFloat::mul(x, p, wc);
            BigFloat       v   = BigFloat::div(num, den, wc);
            if (flip) v.negate_mutable();
            if (!v.is_finite() || v.is_zero()) return v;
            if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, tg_err_of(v, want) + 4)) return v.rounded(ctx);
            if (tg_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
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

BigFloat cos(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::one();
    return detail::tg_dispatch(x, detail::trig_sel::cos_v, ctx);
}

BigFloat tan(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();
    if (x.is_zero())      return x;
    return detail::tg_dispatch(x, detail::trig_sel::tan_v, ctx);
}

BigFloat cot(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::tg_dispatch(x, detail::trig_sel::cot_v, ctx);
}

BigFloat sec(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::one();
    return detail::tg_dispatch(x, detail::trig_sel::sec_v, ctx);
}

BigFloat csc(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::infinity(x.signbit());
    return detail::tg_dispatch(x, detail::trig_sel::csc_v, ctx);
}

BigFloat sinc(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero();     
    if (x.is_zero())      return BigFloat::one();
    return detail::sinc_finite(x, ctx);
}

BigFloat normalized_sinc(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return BigFloat::zero();
    if (x.is_zero())      return BigFloat::one();
    return detail::normalized_sinc_finite(x, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
