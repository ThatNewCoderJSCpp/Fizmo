#ifndef FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP
#define FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP

#include "polynomials.hpp"
#include "polylog.hpp"
#include "inv_htrig.hpp"
#include "riemann_zeta.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace lgdetail {

using pldetail::pl_val;
using hzdetail::hz_xadd;
using hzdetail::hz_xmul;
using hzdetail::hz_l2;
using hzdetail::hz_upow;
using ztdetail::zt_val;

static const std::uint64_t lg_n_cap = 1ull << 28;

inline double lg_span(const BigFloat& x) {
    return x.is_zero() ? 0.0 : static_cast<double>(x.significand().bit_length()) + std::fabs(static_cast<double>(x.exponent()));
}

inline bool lg_p_exact(std::uint64_t n, const BigFloat& x, const BigFloatContext& ctx, BigFloat& out) {
    if (n > (1u << 20)) return false;
    const double bits = static_cast<double>(n) * (lg_span(x) + 4.0);
    if (bits > 16.0 * static_cast<double>(ctx.precision) + 65536.0) return false;
    BigUInt c = BigUInt::one();

    for (std::uint64_t i = 1; i <= n; ++i) {                                          
        c.mul_small_mutable(n + i);
        c.div_small_mutable(i);
    }

    const BigFloat      y = hz_xmul(x, x);
    const std::uint64_t K = n / 2;
    BigFloat S   = BigFloat::zero();
    bool     neg = false;

    for (std::uint64_t k = 0; ; ++k) {
        S = hz_xadd(hz_xmul(S, y), BigFloat(c, neg, 0));
        if (k == K) break;
        c.mul_small_mutable(n - k);
        c.mul_small_mutable(n - 2 * k);
        c.mul_small_mutable(n - 2 * k - 1);
        c.div_small_mutable(k + 1);
        c.div_small_mutable(2 * n - 2 * k);
        c.div_small_mutable(2 * n - 2 * k - 1);
        neg = !neg;
    }

    if (n % 2 == 1) S = hz_xmul(S, x);
    out = S.scaled_pow2(-static_cast<std::int64_t>(n)).rounded(ctx);
    return true;
}

inline pl_val lg_div_int(const pl_val& a, std::uint64_t d, const BigFloatContext& wc) {
    const BigFloat r  = BigFloat::div(a.v, BigFloat(d), wc);
    const double   lr = r.is_zero() ? pldetail::pl_ninf() : pldetail::pl_mag(r) - static_cast<double>(wc.precision);
    return pl_val{r, pldetail::pl_lsum(a.le - std::log2(static_cast<double>(d)), lr)};
}

inline pl_val lg_recur(std::uint64_t n, const BigFloat& x, pl_val f0, pl_val f1, const BigFloatContext& wc) {
    if (n == 0) return f0;
    const pl_val X = pldetail::pl_exact(x);

    for (std::uint64_t k = 1; k < n; ++k) {
        const pl_val a = pldetail::pl_mul(pldetail::pl_exact(BigFloat(2 * k + 1)), pldetail::pl_mul(X, f1, wc), wc);
        const pl_val b = pldetail::pl_mul(pldetail::pl_exact(BigFloat(k)), f0, wc);
        f0 = f1;
        f1 = lg_div_int(pldetail::pl_sub(a, b, wc), k + 1, wc);
    }

    return f1;
}

inline bool lg_q_zero(std::uint64_t n, const BigFloatContext& ctx, BigFloat& out) {
    if (n % 2 == 0) { out = BigFloat::zero(); return true; }
    const std::uint64_t m = (n - 1) / 2;
    if (2 * m + 1 > gmdetail::fact_n_cap()) return false;
    const BigUInt f = gmdetail::fact_range(1, m + 1);
    out = BigFloat::div(BigFloat(f * f, m % 2 == 0, 2 * static_cast<std::int64_t>(m)), BigFloat(gmdetail::fact_range(1, 2 * m + 2)), ctx);
    return true;
}

inline double lg_xi(const BigFloat& ax) {                                              
    const double d = std::exp2(hz_l2(hz_xadd(ax, BigFloat::one(), true)));
    return std::log1p(d + std::sqrt(d * (d + 2.0)));
}

inline zt_val lg_q_hyper(std::uint64_t n, const BigFloat& ax, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + 32), RoundingMode::nearest_even);
    const BigFloat one = BigFloat::one();
    const BigFloat u   = BigFloat::add(ax, sqrt(hz_xadd(hz_xmul(ax, ax), one, true), wc), wc).reciprocal(wc);   
    const BigFloat t   = BigFloat::mul(u, u, wc);                                                               
    const double   td  = std::exp2(hz_l2(t));
    BigFloat C = BigFloat(static_cast<std::uint64_t>(2));

    for (std::uint64_t j = 1; j <= n; ++j) {
        C = BigFloat::div(BigFloat::mul(C, BigFloat(2 * j), wc), BigFloat(2 * j + 1), wc);
    }

    const BigFloat un   = gmdetail::gm_powi(u, n + 1, wc);
    BigFloat       S    = one;
    BigFloat       term = one;
    std::uint64_t  K    = 0;

    for (std::uint64_t k = 0; ; ++k) {                                                  
        term = BigFloat::mul(term, BigFloat(2 * k + 1), wc);
        term = BigFloat::mul(term, BigFloat(n + 1 + k), wc);
        term = BigFloat::div(term, BigFloat(k + 1), wc);
        term = BigFloat::div(term, BigFloat(2 * n + 3 + 2 * k), wc);
        term = BigFloat::mul(term, t, wc);
        S    = BigFloat::add(S, term, ac);
        K    = k + 1;
        if (hz_l2(term) - std::log2(1.0 - td) <= hz_l2(S) - static_cast<double>(want) - 4.0) break;
        if (K > hzdetail::lp_term_cap) return zt_val{BigFloat::undefined(), 0.0};
    }

    const BigFloat v  = BigFloat::mul(BigFloat::mul(C, un, wc), S.rounded(wc), wc);
    const double   nd = static_cast<double>(n);
    const double   units = (2.0 * nd + 1.0) + 3.0 * (nd + 1.0) + 2.0 * std::log2(nd + 2.0) + 12.0 * static_cast<double>(K) + 4.0;
    return zt_val{v, std::log2(units) + 1.0};
}

inline bool lc_neg_int(std::uint64_t m, const BigFloat& x, const BigFloatContext& ctx, BigFloat& out) {
    const BigFloat one = BigFloat::one();
    const BigFloat y   = hz_xmul(x, x);
    const BigFloat omy = hz_xadd(one, y, true);
    if (omy.is_zero() || m > 2048) return false;
    const double bits = (static_cast<double>(m) + 1.0) * (lg_span(y) + lg_span(omy) + std::log2(2.0 * static_cast<double>(m) + 2.0)) + lg_span(x);
    if (bits > 8.0 * static_cast<double>(ctx.precision) + 16384.0) return false;
    const std::size_t    M = static_cast<std::size_t>(m);
    std::vector<BigUInt> d(M + 1);
    for (std::size_t i = 0; i <= M; ++i) d[i] = hz_upow(BigUInt(static_cast<std::uint64_t>(2 * i + 1)), m);

    for (std::size_t j = 1; j <= M; ++j) {                                              
        for (std::size_t i = M; i >= j; --i) d[i].sub_mutable(d[i - 1]);
    }

    std::vector<BigFloat> yp(M + 1, one), op(M + 2, one);
    for (std::size_t i = 1; i <= M; ++i)     yp[i] = hz_xmul(yp[i - 1], y);
    for (std::size_t i = 1; i <= M + 1; ++i) op[i] = hz_xmul(op[i - 1], omy);
    BigFloat P = BigFloat::zero();
    for (std::size_t j = 0; j <= M; ++j) P = hz_xadd(P, hz_xmul(BigFloat(d[j]), hz_xmul(yp[j], op[M - j])));
    out = BigFloat::div(hz_xmul(x, P), op[M + 1], ctx);
    return true;
}

} // namespace lgdetail

inline BigFloat legendre_p(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    const std::uint64_t k   = (n < 0) ? pldetail::pl_abs(n) - 1 : static_cast<std::uint64_t>(n);
    const BigFloat      one = BigFloat::one();
    const bool          odd = (k & 1u) != 0;
    if (k == 0)          return one;
    if (x.is_infinite()) return BigFloat::infinity(x.signbit() && odd);
    if (BigFloat::compare(x.abs(), one) == BigFloat::ordering::equal) return one.with_sign(x.signbit() && odd);
    BigFloat out;
    if (lgdetail::lg_p_exact(k, x, ctx, out)) return out;
    if (k > lgdetail::lg_n_cap) return BigFloat::undefined();
    const BigFloat ax = x.abs();
    return pldetail::pl_drive(k, x.signbit() && odd, ctx, [&](const BigFloatContext& wc) {
        return lgdetail::lg_recur(k, ax, pldetail::pl_exact(one), pldetail::pl_exact(ax), wc);
    });
}

inline BigFloat legendre_q(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (n < 0)            return BigFloat::undefined();                                  
    const std::uint64_t k    = static_cast<std::uint64_t>(n);
    const bool          flip = x.signbit() && (k % 2 == 0);                              
    const BigFloat      one  = BigFloat::one();
    if (x.is_infinite()) return BigFloat::zero(flip);
    BigFloat out;
    if (x.is_zero() && lgdetail::lg_q_zero(k, ctx, out)) return out;
    const BigFloat::ordering c = BigFloat::compare(x.abs(), one);
    if (c == BigFloat::ordering::equal) return BigFloat::infinity(flip);
    if (k > lgdetail::lg_n_cap) return BigFloat::undefined();
    const BigFloat ax     = x.abs();
    const bool     inside = (c == BigFloat::ordering::less);

    if (!inside) {
        const double xi   = lgdetail::lg_xi(ax);
        const double canc = 2.0 * (static_cast<double>(k) + 1.0) * xi / 0.6931471805599453;  

        if (canc > 64.0) {
            const double p    = static_cast<double>(ctx.precision);
            const double hyp  = (p + 40.0) * 0.6931471805599453 / (2.0 * xi) + static_cast<double>(k);
            const double rec  = static_cast<double>(k) * (1.0 + (canc + 64.0) / p);

            if (hyp <= rec) {
                return hzdetail::hz_drive(32 + ztdetail::zt_bitlen(k), ctx, [&](std::size_t want) {
                    lgdetail::zt_val r = lgdetail::lg_q_hyper(k, ax, want);
                    if (flip) r.v = -r.v;
                    return r;
                });
            }
        }
    }

    return pldetail::pl_drive(k, flip, ctx, [&](const BigFloatContext& wc) {
        const BigFloat L  = inside ? arctanh(ax, wc) : arccoth(ax, wc);                 
        const lgdetail::pl_val q0 = lgdetail::pl_val{L, L.is_zero() ? pldetail::pl_ninf() : pldetail::pl_mag(L) - static_cast<double>(wc.precision)};
        const lgdetail::pl_val X  = pldetail::pl_exact(ax);
        const lgdetail::pl_val q1 = pldetail::pl_sub(pldetail::pl_mul(X, q0, wc), pldetail::pl_exact(one), wc);
        return lgdetail::lg_recur(k, ax, q0, q1, wc);
    });
}

inline BigFloat legendre_chi(const BigFloat& x, const BigFloat& s, const BigFloatContext& ctx) {
    using ztdetail::zt_val;
    if (x.is_nan()       || s.is_nan())       return BigFloat::nan();
    if (x.is_undefined() || s.is_undefined()) return BigFloat::undefined();
    if (x.is_zero()) return x;
    const BigFloat           one = BigFloat::one();
    const BigFloat::ordering ca  = x.is_infinite() ? BigFloat::ordering::greater : BigFloat::compare(x.abs(), one);

    if (s.is_infinite()) {                                                               
        if (s.signbit()) return BigFloat::undefined();
        return (ca == BigFloat::ordering::greater) ? BigFloat::nan() : x.rounded(ctx);
    }

    if (x.is_infinite()) return BigFloat::undefined();
    std::int64_t k    = 0;
    const bool   sint = hzdetail::hz_int64(s, k);
    if (sint && k == 1) return arctanh(x, ctx);

    if (sint && k <= 0) {                                                                
        const std::uint64_t m = static_cast<std::uint64_t>(-k);
        if (ca == BigFloat::ordering::equal) return (m % 2 == 1) ? BigFloat::infinity(x.signbit()) : BigFloat::undefined();
        BigFloat out;
        if (lgdetail::lc_neg_int(m, x, ctx, out)) return out;
        if (ca == BigFloat::ordering::greater) return BigFloat::undefined();
    }

    if (ca == BigFloat::ordering::greater) return BigFloat::nan();                       
    const BigFloat om = ztdetail::zt_one_minus(s);

    if (ca == BigFloat::ordering::equal) {                                               
        if (BigFloat::compare(s, one) != BigFloat::ordering::greater) return BigFloat::infinity(x.signbit());
        if (!x.signbit()) return dirichlet_lambda(s, ctx);
        return hzdetail::hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) {
            zt_val r = dldetail::dl_lambda_pos(s, om, want);
            r.v = -r.v;
            return r;
        });
    }

    const BigFloat a  = x.abs();
    const BigFloat na = -a;
    return hzdetail::hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) -> zt_val {
        const zt_val A = pldetail::pl_raw_unit(a, s, om, want);
        if (!A.v.is_finite()) return A;
        const zt_val B = pldetail::pl_raw_unit(na, s, om, want);
        if (!B.v.is_finite()) return B;
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        BigFloat v = BigFloat::sub(A.v, B.v, wc).scaled_pow2(-1);
        if (x.signbit()) v = -v;                                                         
        if (v.is_zero()) return zt_val{v, 1.0e9};
        const double lv    = hzdetail::hz_l2(v);
        const double units = std::exp2(hzdetail::hz_l2(A.v) - lv + A.lost) + std::exp2(hzdetail::hz_l2(B.v) - lv + B.lost) + 1.0;
        return zt_val{v, std::log2(units) + 1.0};
    });
}

#define FIZMO_MP_LEGENDRE_FORWARD(FN)                                                                         \
    inline BigFloat FN(const BigFloat& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }    \
    inline BigFloat FN(const BigInt& x, std::int64_t n, const BigFloatContext& c) {                           \
        if (x.is_nan())       return BigFloat::nan();                                                         \
        if (x.is_undefined()) return BigFloat::undefined();                                                   \
        return FN(BigFloat(x), n, c);                                                                         \
    }                                                                                                         \
    inline BigFloat FN(const BigInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }      \
    inline BigFloat FN(const BigUInt& x, std::int64_t n, const BigFloatContext& c) {                          \
        if (x.is_undefined()) return BigFloat::undefined();                                                   \
        return FN(BigFloat(x), n, c);                                                                         \
    }                                                                                                         \
    inline BigFloat FN(const BigUInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }     \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>               \
    inline BigFloat FN(T x, std::int64_t n, const BigFloatContext& c) { return FN(BigFloat(x), n, c); }       \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>               \
    inline BigFloat FN(T x, std::int64_t n) { return FN(BigFloat(x), n, BigFloatContext::current()); }

FIZMO_MP_LEGENDRE_FORWARD(legendre_p)
FIZMO_MP_LEGENDRE_FORWARD(legendre_q)

#undef FIZMO_MP_LEGENDRE_FORWARD

inline BigFloat legendre_chi(const BigFloat& x, const BigFloat& s) { return legendre_chi(x, s, BigFloatContext::current()); }

template <typename X, typename S, typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat legendre_chi(const X& x, const S& s, const BigFloatContext& c) { return legendre_chi(BigFloat(x), BigFloat(s), c); }

template <typename X, typename S, typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat legendre_chi(const X& x, const S& s) { return legendre_chi(BigFloat(x), BigFloat(s), BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP