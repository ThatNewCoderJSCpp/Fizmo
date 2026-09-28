#ifndef FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP
#define FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP

#include "exp.hpp"
#include "logarithms.hpp"
#include "sqrt_cbrt.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

enum class log_op : std::uint8_t { multiply, divide };

inline bool pw_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 2;
}

inline bool pw_strictly_negative(const BigFloat& v) { return v.is_negative() && !v.is_zero(); }

inline std::int64_t pw_mul_sat(std::int64_t a, std::int64_t b) noexcept {
    if (a == 0 || b == 0) return 0;
    const std::uint64_t ua  = ln_abs_u64(a);
    const std::uint64_t ub  = ln_abs_u64(b);
    const bool          neg = (a < 0) != (b < 0);
    const std::uint64_t lim = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (ua > lim / ub) return neg ? std::numeric_limits<std::int64_t>::min() : std::numeric_limits<std::int64_t>::max();
    const std::int64_t m = static_cast<std::int64_t>(ua * ub);
    return neg ? -m : m;
}

inline bool pw_to_i64(const BigInt& n, std::int64_t& out) {
    if (n.is_nan() || n.is_undefined())   return false;
    if (n.magnitude().bit_length() > 62)  return false;
    const std::uint64_t b = n.get_lowest_bits();
    out = n.is_negative() ? -static_cast<std::int64_t>(b) : static_cast<std::int64_t>(b);
    return true;
}

inline BigUInt ipow(const BigUInt& m, std::uint64_t n) {
    BigUInt r = BigUInt::one(), t = m;
    while (n != 0) { if (n & 1u) r = r * t; n >>= 1; if (n != 0) t = t * t; }
    return r;
}

inline BigUInt iroot(const BigUInt& v, std::uint64_t n) {
    if (v.is_undefined()) return v;
    if (n == 1)           return v;
    if (v.is_zero())      return BigUInt::zero();
    if (n == 2)           return isqrt(v);
    if (n == 3)           return icbrt(v);
    const std::size_t L = v.bit_length();
    if (static_cast<std::uint64_t>(L) <= n) return BigUInt::one();
    BigUInt x = BigUInt::one();
    x.shift_left_mutable(static_cast<std::size_t>((static_cast<std::uint64_t>(L) + n - 1) / n));

    for (;;) {
        BigUInt y = v / ipow(x, n - 1);
        BigUInt t = x;
        t.mul_small_mutable(n - 1);
        y.add_mutable(t);
        y.div_small_mutable(n);
        if (!(y < x)) return x;
        x = std::move(y);
    }
}

inline bool nth_root_exact(const BigFloat& ax, std::uint64_t n, BigFloat& out) {
    if (n == 0 || !ax.is_finite() || ax.is_zero()) return false;
    const BigUInt&     m = ax.significand();
    const std::int64_t e = ax.exponent();
    if (e % static_cast<std::int64_t>(n) != 0) return false;
    if (m.is_one()) { out = BigFloat(BigUInt::one(), false, e / static_cast<std::int64_t>(n)); return true; }
    const std::size_t L = m.bit_length();
    if (n > 4096 || L > 65536)                        return false;
    if (static_cast<std::uint64_t>(L) <= n)           return false;   
    const BigUInt r = iroot(m, n);
    if (r.is_undefined())                             return false;
    if (ipow(r, n).compare(m) != 0)                   return false;
    out = BigFloat(r, false, e / static_cast<std::int64_t>(n));
    return true;
}

inline BigFloat recip_rounded(const BigFloat& v, const BigFloatContext& ctx, std::size_t err) {
    std::size_t guard = 32;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
        const BigFloat r = v.reciprocal(wc);
        if (!r.is_finite() || r.is_zero()) return r;
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, err)) return r.rounded(ctx);
        if (pw_guard_exhausted(ctx.precision, guard)) return r.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat pow_int_finite(const BigFloat& ax, std::int64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::one();
    const BigUInt&      m  = ax.significand();
    const std::int64_t  e  = ax.exponent();
    const std::uint64_t an = ln_abs_u64(n);

    if (m.is_one()) {                                  
        const BigFloat r(BigUInt::one(), false, pw_mul_sat(e, n));
        return r;                                          
    }

    const std::size_t L    = m.bit_length();
    const std::size_t soft = ctx.precision + 1024;
    const std::size_t cap  = (soft < BigUInt::max_bits / 2) ? soft : BigUInt::max_bits / 2;

    if (an <= static_cast<std::uint64_t>(cap) / L) {       
        const BigFloat r(ipow(m, an), false, pw_mul_sat(e, static_cast<std::int64_t>(an)));
        if (!r.is_finite() || r.is_zero()) return (n > 0) ? r : (r.is_zero() ? BigFloat::infinity() : BigFloat::zero());
        return (n > 0) ? r.rounded(ctx) : recip_rounded(r, ctx, 3);
    }

    const std::size_t nb    = ln_bit_length_u64(an);
    const std::size_t err   = nb + 2;
          std::size_t guard = nb + 32;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
        BigFloat      r = BigFloat::one();
        BigFloat      t = ax;
        std::uint64_t k = an;

        while (k != 0) {
            if (k & 1u) r = BigFloat::mul(r, t, wc);
            k >>= 1;
            if (k != 0) t = BigFloat::mul(t, t, wc);
        }

        if (!r.is_finite() || r.is_zero()) {
            if (n > 0) return r;
            return r.is_zero() ? BigFloat::infinity() : BigFloat::zero();
        }

        if (n < 0) r = r.reciprocal(wc);
        if (!r.is_finite() || r.is_zero()) return r;
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, err + 1)) return r.rounded(ctx);
        if (pw_guard_exhausted(ctx.precision, guard)) return r.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat exp_log_core(const BigFloat& ax, const BigFloat& y, log_op op, bool neg_result, const BigFloatContext& ctx) {
    const std::size_t err = 6;
    std::size_t extra = 0;

    {   
        const BigFloatContext pc(64, RoundingMode::nearest_even);
        const BigFloat l = ln(ax, pc);
        const BigFloat t = (op == log_op::multiply) ? BigFloat::mul(y, l, pc) : BigFloat::div(l, y, pc);
        if (t.is_nan() || t.is_undefined()) return t;
        const std::int64_t E = t.get_exp_base2();
        if (E != BigFloat::exp_none && E != BigFloat::exp_inf && E > 0) extra = (E > 4096) ? 4096 : static_cast<std::size_t>(E);
    }

    std::size_t guard = 48;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard + extra, RoundingMode::nearest_even);
        const BigFloat l = ln(ax, wc);
        const BigFloat t = (op == log_op::multiply) ? BigFloat::mul(y, l, wc) : BigFloat::div(l, y, wc);
        if (t.is_nan() || t.is_undefined()) return t;
        const BigFloat r = exp(t, wc);
        if (!r.is_finite() || r.is_zero()) return r.with_sign(neg_result);
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, err)) return r.with_sign(neg_result).rounded(ctx);
        if (pw_guard_exhausted(ctx.precision, guard + extra)) return r.with_sign(neg_result).rounded(ctx);
        guard *= 2;
    }
}

inline bool pow_dyadic(const BigFloat& ax, const BigFloat& y, const BigFloatContext& ctx, BigFloat& out) {
    if (!y.is_finite() || y.is_zero()) return false;
    const std::int64_t e = y.exponent();
    if (e >= 0 || e < -16)                    return false;
    const BigUInt& ym = y.significand();
    if (ym.bit_length() > 12)                 return false;
    const std::size_t  k  = static_cast<std::size_t>(-e);
    const std::int64_t mm = static_cast<std::int64_t>(ym.get_lowest_bits());
    const std::int64_t m  = y.signbit() ? -mm : mm;
    const std::size_t  err = 6 + 2 * k;
    std::size_t guard = 32 + 8 * k;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
        BigFloat r = pow_int_finite(ax, m, wc);
        for (std::size_t i = 0; i < k && r.is_finite() && !r.is_zero(); ++i) r = math::sqrt(r, wc);
        if (!r.is_finite() || r.is_zero()) { out = r; return true; }
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, err)) { out = r.rounded(ctx); return true; }
        if (pw_guard_exhausted(ctx.precision, guard)) { out = r.rounded(ctx); return true; }
        guard *= 2;
    }
}

inline BigFloat nth_root_int_finite(const BigFloat& ax, std::int64_t n, const BigFloatContext& ctx) {
    const std::uint64_t an = ln_abs_u64(n);
    if (an == 1) return (n > 0) ? ax.rounded(ctx) : recip_rounded(ax, ctx, 3);
    BigFloat ex;
    if (an <= 4096 && nth_root_exact(ax, an, ex)) return (n > 0) ? ex.rounded(ctx) : recip_rounded(ex, ctx, 3);
    if (n == 2) return math::sqrt(ax, ctx);
    if (n == 3) return math::cbrt(ax, ctx);

    if (n == -2 || n == -3) {
        const BigFloatContext wc(ctx.precision + 96, RoundingMode::nearest_even);
        const BigFloat r = (n == -2) ? math::sqrt(ax, wc) : math::cbrt(ax, wc);
        return recip_rounded(r, ctx, 8);
    }

    return exp_log_core(ax, BigFloat(n), log_op::divide, false, ctx);
}

inline BigFloat pw_extreme(const BigFloat& ax, bool y_negative, bool neg_result) {
    const BigFloat::ordering c = BigFloat::compare(ax, BigFloat::one());
    if (c == BigFloat::ordering::equal) return BigFloat::one(neg_result);
    const bool grow = (c == BigFloat::ordering::greater) != y_negative;
    return grow ? BigFloat::infinity(neg_result) : BigFloat::zero(neg_result);
}

} // namespace detail

inline BigFloat pow(const BigFloat& x, const BigInt& n, const BigFloatContext& ctx) {
    if (x.is_nan()       || n.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || n.is_undefined()) return BigFloat::undefined();
    const bool nzero = n.magnitude().is_zero();
    const bool neg   = x.signbit() && n.is_odd();
    
    if (x.is_zero()) {
        if (nzero) return BigFloat::undefined();                                 
        return n.is_negative() ? BigFloat::infinity(neg) : BigFloat::zero(neg); 
    }

    if (nzero)       return BigFloat::one();
    if (x.is_infinite()) return n.is_negative() ? BigFloat::zero(neg) : BigFloat::infinity(neg);
    const BigFloat ax = x.abs();
    std::int64_t   ni = 0;

    if (!detail::pw_to_i64(n, ni)) {                     
        const BigFloat::ordering c = BigFloat::compare(ax, BigFloat::one());
        if (c == BigFloat::ordering::equal) return BigFloat::one(neg);
        const bool grow = (c == BigFloat::ordering::greater) != n.is_negative();
        return grow ? BigFloat::infinity(neg) : BigFloat::zero(neg);
    }

    return detail::pow_int_finite(ax, ni, ctx).with_sign(neg);
}

inline BigFloat pow(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx) {
    if (x.is_nan()       || y.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || y.is_undefined()) return BigFloat::undefined();

    if (y.is_integer()) {                      
        const BigInt n = y.get_integer_part();
        if (!n.is_nan() && !n.is_undefined()) return pow(x, n, ctx);
    }

    if (detail::pw_strictly_negative(x)) return BigFloat::nan();   
    if (x.is_zero())     return y.is_negative() ? BigFloat::infinity() : BigFloat::zero();
    if (x.is_infinite()) return y.is_negative() ? BigFloat::zero()     : BigFloat::infinity();
    if (y.is_infinite()) return detail::pw_extreme(x, y.is_negative(), false);
    if (detail::is_exactly_one(x)) return BigFloat::one();
    BigFloat out;
    if (detail::pow_dyadic(x, y, ctx, out)) return out;
    return detail::exp_log_core(x, y, detail::log_op::multiply, false, ctx);
}

inline BigFloat pow2(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    if (x.is_zero())      return BigFloat::one();

    if (x.is_integer()) {                                 
        const BigInt n  = x.get_integer_part();
        std::int64_t ni = 0;
        if (!detail::pw_to_i64(n, ni)) return n.is_negative() ? BigFloat::zero() : BigFloat::infinity();
        return BigFloat(BigUInt::one(), false, ni);         
    }

    const BigInt   k  = x.get_integer_part();
    const BigFloat f  = x.get_fractional_part();          
    std::int64_t   ki = 0;
    if (!detail::pw_to_i64(k, ki)) return k.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    std::size_t guard = 32;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
        BigFloat r = exp(BigFloat::mul(f, constants::ln2(wc), wc), wc);
        if (!r.is_finite()) return r;
        r = r.scaled_pow2(ki);                             
        if (!r.is_finite() || r.is_zero()) return r;
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, 6)) return r.rounded(ctx);
        if (detail::pw_guard_exhausted(ctx.precision, guard)) return r.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat pow10(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    if (x.is_zero())      return BigFloat::one();
    const BigFloat ten(static_cast<std::uint64_t>(10));

    if (x.is_integer()) {
        const BigInt n  = x.get_integer_part();
        std::int64_t ni = 0;
        if (!detail::pw_to_i64(n, ni)) return n.is_negative() ? BigFloat::zero() : BigFloat::infinity();
        return detail::pow_int_finite(ten, ni, ctx);       
    }

    const BigInt   k  = x.get_integer_part();
    const BigFloat f  = x.get_fractional_part();
    std::int64_t   ki = 0;
    if (!detail::pw_to_i64(k, ki)) return k.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    std::size_t guard = 32;

    for (;;) {
        const BigFloatContext wc(ctx.precision + guard, RoundingMode::nearest_even);
        const BigFloat base = detail::pow_int_finite(ten, ki, wc);
        if (!base.is_finite() || base.is_zero()) return base;
        const BigFloat frac = exp(BigFloat::mul(f, constants::ln10(wc), wc), wc);
        if (!frac.is_finite()) return frac;
        const BigFloat r = BigFloat::mul(base, frac, wc);
        if (!r.is_finite() || r.is_zero()) return r;
        if (constants::bfdetail::round_is_safe(r.significand(), ctx.precision, 8)) return r.rounded(ctx);
        if (detail::pw_guard_exhausted(ctx.precision, guard)) return r.rounded(ctx);
        guard *= 2;
    }
}

#define FIZMO_MP_EXPBASE_FORWARD(FN)                                                   \
    inline BigFloat FN(const BigFloat& x) { return FN(x, BigFloatContext::current()); } \
    inline BigFloat FN(const BigInt& x, const BigFloatContext& c) {                    \
        if (x.is_nan())       return BigFloat::nan();                                 \
        if (x.is_undefined()) return BigFloat::undefined();                           \
        return FN(BigFloat(x), c);                                                    \
    }                                                                                 \
    inline BigFloat FN(const BigInt& x) { return FN(x, BigFloatContext::current()); }  \
    inline BigFloat FN(const BigUInt& x, const BigFloatContext& c) {                   \
        if (x.is_undefined()) return BigFloat::undefined();                           \
        return FN(BigFloat(x), c);                                                    \
    }                                                                                 \
    inline BigFloat FN(const BigUInt& x) { return FN(x, BigFloatContext::current()); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0> \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(BigInt(x)), c); }  \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0> \
    inline BigFloat FN(T x) { return FN(BigFloat(BigInt(x)), BigFloatContext::current()); }

FIZMO_MP_EXPBASE_FORWARD(pow2)
FIZMO_MP_EXPBASE_FORWARD(pow10)

#undef FIZMO_MP_EXPBASE_FORWARD

inline BigFloat nth_root(const BigFloat& x, const BigInt& n, const BigFloatContext& ctx) {
    if (x.is_nan()       || n.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || n.is_undefined()) return BigFloat::undefined();
    if (n.magnitude().is_zero())              return BigFloat::undefined();
    if (detail::pw_strictly_negative(x) && n.is_even()) return BigFloat::nan();
    const bool neg = x.signbit() && n.is_odd();
    if (x.is_zero())     return n.is_negative() ? BigFloat::infinity(neg) : BigFloat::zero(neg);
    if (x.is_infinite()) return n.is_negative() ? BigFloat::zero(neg)     : BigFloat::infinity(neg);
    std::int64_t ni = 0;
    if (!detail::pw_to_i64(n, ni)) return BigFloat::one(neg);  
    return detail::nth_root_int_finite(x.abs(), ni, ctx).with_sign(neg);
}

inline BigFloat nth_root(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx) {
    if (x.is_nan()       || y.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || y.is_undefined()) return BigFloat::undefined();
    if (y.is_zero())                          return BigFloat::undefined();

    if (y.is_integer()) {                       
        const BigInt n = y.get_integer_part();
        if (!n.is_nan() && !n.is_undefined()) return nth_root(x, n, ctx);
    }

    if (detail::pw_strictly_negative(x)) return BigFloat::nan();   
    if (x.is_zero())     return y.is_negative() ? BigFloat::infinity() : BigFloat::zero();
    if (y.is_infinite()) return x.is_infinite() ? BigFloat::undefined() : BigFloat::one();
    if (x.is_infinite()) return y.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    if (detail::is_exactly_one(x)) return BigFloat::one();
    return detail::exp_log_core(x, y, detail::log_op::divide, false, ctx);
}

#define FIZMO_MP_POW_FORWARD(FN)                                                                                      \
    inline BigFloat FN(const BigFloat& x, const BigUInt& n, const BigFloatContext& c) { return FN(x, BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigFloat& x, T n, const BigFloatContext& c) { return FN(x, BigInt(n), c); }              \
    inline BigFloat FN(const BigInt& x,  const BigFloat& y, const BigFloatContext& c) { return FN(BigFloat(x), y, c); }      \
    inline BigFloat FN(const BigInt& x,  const BigInt& n,   const BigFloatContext& c) { return FN(BigFloat(x), n, c); }      \
    inline BigFloat FN(const BigInt& x,  const BigUInt& n,  const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigInt& x, T n, const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n), c); }      \
    inline BigFloat FN(const BigUInt& x, const BigFloat& y, const BigFloatContext& c) { return FN(BigFloat(x), y, c); }      \
    inline BigFloat FN(const BigUInt& x, const BigInt& n,   const BigFloatContext& c) { return FN(BigFloat(x), n, c); }      \
    inline BigFloat FN(const BigUInt& x, const BigUInt& n,  const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigUInt& x, T n, const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n), c); }     \
    inline BigFloat FN(const BigFloat& x, const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }         \
    inline BigFloat FN(const BigFloat& x, const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }         \
    inline BigFloat FN(const BigFloat& x, const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }         \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigFloat& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }               \
    inline BigFloat FN(const BigInt& x,  const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigInt& x,  const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigInt& x,  const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }          \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigInt& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }                 \
    inline BigFloat FN(const BigUInt& x, const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigUInt& x, const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigUInt& x, const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }          \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigUInt& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }

FIZMO_MP_POW_FORWARD(pow)
FIZMO_MP_POW_FORWARD(nth_root)

#undef FIZMO_MP_POW_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP