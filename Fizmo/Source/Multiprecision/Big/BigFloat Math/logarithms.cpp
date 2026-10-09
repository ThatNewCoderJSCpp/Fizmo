#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

void ln_shift(BigUInt& v, std::int64_t s) {
    if (s > 0)      v.shift_left_mutable(static_cast<std::size_t>(s));
    else if (s < 0) v.shift_right_mutable(static_cast<std::size_t>(-s));
}

fixed_approx ln_fixed(const BigFloat& x, std::size_t bits) {
    using constants::bfdetail::unit;
    fixed_approx out;
    const BigUInt&     m = x.significand();
    const std::size_t  L = m.bit_length();
    const bool         below = (L >= 2) && m.get_bit(L - 2);
    const std::size_t  Lf    = below ? L : L - 1;
    const std::int64_t E     = x.exponent() + static_cast<std::int64_t>(Lf);
    std::size_t t = 0;

    if (E == 0) {
        BigUInt D;
        if (below) { D = unit(Lf); D.sub_mutable(m); }
        else       { D = m;        D.sub_mutable(unit(Lf)); }
        if (D.is_zero()) { out.mag = BigUInt::zero(); return out; }   
        const std::size_t Db = D.bit_length();
        t = (Lf + 1 > Db) ? (Lf + 1 - Db) : 0;
    }

    const std::size_t kb = static_cast<std::size_t>(std::sqrt(static_cast<double>(bits) / 20.0));
    const std::size_t k  = (t >= kb) ? 0 : (kb - t);
    const bool          eneg = E < 0;
    const std::uint64_t absE = ln_abs_u64(E);
    const std::size_t   B    = ln_bit_length_u64(absE);
    const std::size_t cap = (BigUInt::max_bits > 512) ? (BigUInt::max_bits - 256) / 2 : 128;
    std::size_t N = bits + t + k + ln_bit_length_u64(bits + t + k + 64) + 4;
    if (N >= cap) { N = cap; out.capped = true; }
    BigUInt G = m;
    ln_shift(G, static_cast<std::int64_t>(N) - static_cast<std::int64_t>(Lf));

    for (std::size_t i = 0; i < k; ++i) {
        G.shift_left_mutable(N);
        G = isqrt(G);
    }

    const BigUInt one  = unit(N);
    const bool    zneg = G < one;
    BigUInt num;
    if (zneg) { num = one; num.sub_mutable(G); }
    else      { num = G;   num.sub_mutable(one); }
    num.shift_left_mutable(N);
    BigUInt den = G;
    den.add_mutable(one);
    const BigUInt Z = num / den;
    BigUInt Z2 = Z * Z;
    Z2.shift_right_mutable(N);
    BigUInt       P = Z;
    BigUInt       T = Z;
    std::uint64_t n = 1;

    for (std::uint64_t j = 3; ; j += 2) {
        P = P * Z2;
        P.shift_right_mutable(N);
        if (P.is_zero()) break;
        BigUInt q = P;
        q.div_small_mutable(j);
        T.add_mutable(q);
        ++n;
    }

    T.shift_left_mutable(k + 1);
    const std::size_t err_bits = k + 2 + ln_bit_length_u64(2 * n + 6);

    if (E == 0) {
        out.mag = std::move(T);
        out.neg = zneg;
    } else {
        const std::size_t S  = N + B;
        const BigFloat    l2 = constants::ln2(BigFloatContext(S + 4, RoundingMode::nearest_even));
        BigUInt Q = l2.significand();
        ln_shift(Q, l2.exponent() + static_cast<std::int64_t>(S));
        Q = Q * BigUInt(absE);
        Q.shift_right_mutable(B);

        if (eneg == zneg) { Q.add_mutable(T); out.mag = std::move(Q); out.neg = eneg; }
        else if (T < Q)   { Q.sub_mutable(T); out.mag = std::move(Q); out.neg = eneg; }
        else              { T.sub_mutable(Q); out.mag = std::move(T); out.neg = zneg; }
    }

    out.scale    = -static_cast<std::int64_t>(N);
    out.err_bits = err_bits;
    return out;
}

BigFloat ln_finite(const BigFloat& x, const BigFloatContext& ctx) {
    if (is_exactly_one(x)) return BigFloat::zero();
    const std::size_t p = ctx.precision;
    std::size_t extra = 16;

    for (;;) {
        fixed_approx a = ln_fixed(x, p + extra);
        if (a.capped || constants::bfdetail::round_is_safe(a.mag, p, a.err_bits + 1)) { return BigFloat(std::move(a.mag), a.neg, a.scale).rounded(ctx); }
        extra = extra * 2 + 16;
    }
}

BigUInt log_pow5(std::uint64_t n) {
    BigUInt r = BigUInt::one(), b(std::uint64_t(5));
    while (n != 0) { if (n & 1u) r = r * b; n >>= 1; if (n != 0) b = b * b; }
    return r;
}

bool log2_exact(const BigFloat& x, std::int64_t& k) {
    if (!x.is_finite() || x.signbit() || x.is_zero()) return false;
    if (!x.significand().is_one())                   return false;
    k = x.exponent();
    return true;
}

bool log10_exact(const BigFloat& x, std::int64_t& k) {
    if (!x.is_finite() || x.signbit() || x.is_zero()) return false;
    const std::int64_t e = x.exponent();
    if (e < 0 || e > 4096)                           return false;
    if (e == 0)                                      return x.significand().is_one() ? (k = 0, true) : false;
    const BigUInt p = log_pow5(static_cast<std::uint64_t>(e));
    if (x.significand().bit_length() != p.bit_length()) return false;
    if (x.significand().compare(p) != 0)               return false;
    k = e;
    return true;
}

bool log_pow_matches(const BigFloat& b, std::int64_t n, const BigFloat& x) {
    if (!b.is_finite() || !x.is_finite())            return false;
    if (b.is_zero()    || x.is_zero())               return false;
    if (b.signbit()    || x.signbit())               return false;
    if (n == 0) return x.significand().is_one() && x.exponent() == 0;
    const BigUInt&      bm = b.significand();
    const std::int64_t  be = b.exponent();
    const std::uint64_t an = ln_abs_u64(n);
    if (an > (std::uint64_t(1) << 32))                                   return false;
    if (be != 0 && ln_abs_u64(be) > (std::uint64_t(1) << 40) / an)       return false;
    const std::int64_t xe = be * n;

    if (n < 0) {
        if (!bm.is_one()) return false;   // 1/m is dyadic only when m == 1
        return x.significand().is_one() && x.exponent() == xe;
    }

    const std::size_t L = bm.bit_length();
    if (L > 1 && an > (BigUInt::max_bits / 2) / L) return false;
    BigUInt       r = BigUInt::one();
    BigUInt       t = bm;
    std::uint64_t e = an;

    while (e != 0) {
        if (e & 1u) r = r * t;
        e >>= 1;
        if (e != 0) t = t * t;
    }

    return x.exponent() == xe && x.significand().compare(r) == 0;
}

bool log_nearest_int(const BigFloat& v, const BigFloatContext& wc, std::int64_t& n) {
    if (!v.is_finite()) return false;
    const std::int64_t e2 = v.get_exp_base2();
    if (e2 != BigFloat::exp_none && e2 > 40) return false;
    const BigInt i = v.get_integer_part();
    if (i.is_nan() || i.is_undefined())      return false;
    if (i.magnitude().bit_length() > 40)     return false;
    const std::uint64_t bits = i.get_lowest_bits();
    const std::int64_t  k    = i.is_negative() ? -static_cast<std::int64_t>(bits) : static_cast<std::int64_t>(bits);
    const std::int64_t  tiny = -static_cast<std::int64_t>(wc.precision / 2);
    const BigFloat      f    = v.get_fractional_part();
    if (f.is_zero())              { n = k; return true; }
    if (f.get_exp_base2() < tiny) { n = k; return true; }
    const BigFloat d = BigFloat::sub(BigFloat::one(), f.abs(), wc);

    if (d.is_zero() || d.get_exp_base2() < tiny) {
        n = v.signbit() ? k - 1 : k + 1;
        return true;
    }

    return false;
}

ln_kind classify_ln(const BigFloat& v) {
    if (v.is_nan())        return ln_kind::nan_v;
    if (v.is_undefined())  return ln_kind::undef_v;
    if (v.is_zero())       return ln_kind::neg_inf;    
    if (v.is_negative())   return ln_kind::nan_v;
    if (v.is_infinite())   return ln_kind::pos_inf;
    if (is_exactly_one(v)) return ln_kind::pos_zero;  
    return (v > BigFloat::one()) ? ln_kind::pos_finite : ln_kind::neg_finite;
}

bool log_special(ln_kind a, ln_kind b, BigFloat& out) {
    if (a == ln_kind::nan_v   || b == ln_kind::nan_v)   { out = BigFloat::nan();       return true; }
    if (a == ln_kind::undef_v || b == ln_kind::undef_v) { out = BigFloat::undefined(); return true; }
    const bool a_inf  = (a == ln_kind::neg_inf) || (a == ln_kind::pos_inf);
    const bool b_inf  = (b == ln_kind::neg_inf) || (b == ln_kind::pos_inf);
    const bool a_zero = (a == ln_kind::pos_zero);
    const bool b_zero = (b == ln_kind::pos_zero);
    if (a_inf  && b_inf)  { out = BigFloat::undefined(); return true; }   
    if (a_zero && b_zero) { out = BigFloat::undefined(); return true; }   
    const bool aneg = (a == ln_kind::neg_inf) || (a == ln_kind::neg_finite);
    const bool bneg = (b == ln_kind::neg_inf) || (b == ln_kind::neg_finite);
    const bool neg  = (aneg != bneg);
    if (a_inf)  { out = BigFloat::infinity(neg); return true; }  
    if (b_inf)  { out = BigFloat::zero(neg);     return true; }  
    if (b_zero) { out = BigFloat::infinity(neg); return true; }  
    if (a_zero) { out = BigFloat::zero(neg);     return true; }  
    return false;
}

} // namespace detail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat ln(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::infinity(true); 
    if (x.is_negative())  return BigFloat::nan();
    if (x.is_infinite())  return BigFloat::infinity();
    return detail::ln_finite(x, ctx);
}

BigFloat ln(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return ln(BigFloat(x), ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

BigFloat log_scaled(const BigFloat& x, BigFloat (*inv_const)(const BigFloatContext&), const BigFloatContext& ctx) {
        const std::size_t err = 4;   
        std::size_t guard = 32;

        for (;;) {
            const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
            const BigFloat v = BigFloat::mul(ln(x, wc), inv_const(wc), wc);
            if (!v.is_finite()) return v;
            if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err)) return v.rounded(ctx);
            if (log_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
            guard *= 2;
        }
    }

BigFloat log_ratio(const BigFloat& x, const BigFloat& b, const BigFloatContext& ctx) {
        const std::size_t err = 4;
        std::size_t guard = 32;

        for (;;) {
            const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
            const BigFloat v = BigFloat::div(ln(x, wc), ln(b, wc), wc);
            if (!v.is_finite()) return v;
            if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err)) return v.rounded(ctx);
            std::int64_t n = 0;
            if (log_nearest_int(v, wc, n) && log_pow_matches(b, n, x)) return BigFloat(n).rounded(ctx);
            if (log_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
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

BigFloat log2(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::infinity(true);
    if (x.is_negative())  return BigFloat::nan();
    if (x.is_infinite())  return BigFloat::infinity();
    std::int64_t k = 0;
    if (detail::log2_exact(x, k)) return BigFloat(k).rounded(ctx);
    return detail::log_scaled(x, &constants::inv_ln2, ctx);
}

BigFloat log2(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return log2(BigFloat(x), ctx);
}

BigFloat log10(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::infinity(true);
    if (x.is_negative())  return BigFloat::nan();
    if (x.is_infinite())  return BigFloat::infinity();
    std::int64_t k = 0;
    if (detail::log10_exact(x, k)) return BigFloat(k).rounded(ctx);
    return detail::log_scaled(x, &constants::inv_ln10, ctx);
}

BigFloat log10(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return log10(BigFloat(x), ctx);
}

BigFloat log(const BigFloat& x, const BigFloat& base, const BigFloatContext& ctx) {
    BigFloat out;
    if (detail::log_special(detail::classify_ln(x), detail::classify_ln(base), out)) return out;
    if (BigFloat::compare(x, base) == BigFloat::ordering::equal) return BigFloat::one();
    std::int64_t k = 0;
    if (detail::log2_exact(base, k)  && k == 1) return log2(x, ctx);
    if (detail::log10_exact(base, k) && k == 1) return log10(x, ctx);
    return detail::log_ratio(x, base, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
