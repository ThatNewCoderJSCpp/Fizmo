#ifndef FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP
#define FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP

#include "big_float_consts.hpp"
#include "exp.hpp"
#include "pow_nth_root.hpp"
#include "sqrt_cbrt.hpp"
#include "logarithms.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace lwdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;

struct lw_val {
    BF     v;
    double lost;                                                          
};

inline BFC lw_ctx(std::size_t p) { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }

inline double lw_log2(const BF& x) {                                      
    BigUInt           sig = x.significand();
    const std::size_t L   = sig.bit_length();
    std::int64_t      e   = x.exponent();

    if (L > 60) {
        sig.shift_right_mutable(L - 60);
        e += static_cast<std::int64_t>(L - 60);
    }

    return std::log2(static_cast<double>(sig.get_lowest_bits())) + static_cast<double>(e);
}

inline double lw_to_double(const BF& x) {                                 
    if (x.is_zero()) return 0.0;
    const double v = std::exp2(lw_log2(x));
    return x.signbit() ? -v : v;
}

inline bool lw_safe(const BF& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero() || !v.is_finite()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                       
    const double e = std::ceil(lost);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    return constants::bfdetail::round_is_safe(sig, prec, (e <= 0.0) ? 2 : static_cast<std::size_t>(e) + 2);
}

template <typename Eval>
inline BF lw_drive(const BFC& ctx, Eval eval) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = BFC::clamp_precision(ctx.precision + guard);
        const lw_val      r    = eval(want);
        if (!r.v.is_finite()) return r.v;
        if (lw_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (guard >= (1u << 16) || ctx.precision + guard >= BFC::max_prec / 4) return r.v.rounded(ctx);
        guard *= 2;
    }
}

inline int lw_eps(const BF& x, BF& eps) {
    const BF     one  = BF::one();
    const double span = static_cast<double>(x.significand().bit_length()) + std::fabs(static_cast<double>(x.exponent()));
    const double cap  = 4.0 * span + 4096.0;

    for (std::size_t q = 128; ; q *= 2) {
        const BFC qc = lw_ctx(q);
        eps = BF::add(one, BF::mul(constants::e(qc), x, qc), qc);
        if (!eps.is_zero() && lw_log2(eps) > 4.0 - static_cast<double>(q) + 64.0) return eps.signbit() ? -1 : 1;
        if (static_cast<double>(q) > cap) return eps.is_zero() ? 0 : (eps.signbit() ? -1 : 1);
    }
}

inline double lw_halley_d(double w, double x) {
    for (int i = 0; i < 40; ++i) {
        const double ew  = std::exp(w);
        const double f   = w * ew - x;
        const double wp1 = w + 1.0;
        if (wp1 == 0.0) break;
        const double d   = ew * wp1 - (w + 2.0) * f / (2.0 * wp1);
        if (d == 0.0 || !std::isfinite(d)) break;
        const double wn  = w - f / d;
        if (!std::isfinite(wn)) break;
        const bool done = std::fabs(wn - w) <= 1.0e-15 * std::fabs(wn);
        w = wn;
        if (done) break;
    }

    return w;
}

inline BF lw_seed(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t& sp) {
    sp = 128;
    const double ln2 = 0.6931471805599453;

    if (have_eps && BF::compare(eps, BF(0.25)) == BF::ordering::less) {                    
        const double le = lw_log2(eps);
        sp = 128 + static_cast<std::size_t>(std::max(0.0, -(le + 1.0) / 2.0));
        const BFC c  = lw_ctx(sp);
        const BF  p  = sqrt(eps.scaled_pow2(1), c);                                        
        const BF  p2 = BF::mul(p, p, c);
        const BF  p3 = BF::mul(p2, p, c);
        const BF  t2 = BF::div(p2, BF(static_cast<std::uint64_t>(3)), c);
        const BF  t3 = BF::div(BF::mul(p3, BF(static_cast<std::uint64_t>(11)), c), BF(static_cast<std::uint64_t>(72)), c);
        const BF  odd = BF::add(p, t3, c);                                                 
        BF w = BF::sub(BF::one(true), t2, c);                                              
        return lower ? BF::sub(w, odd, c) : BF::add(w, odd, c);
    }

    const double lx = lw_log2(x);

    if (!lower) {
        if (lx < -60.0) return x;                                                          
        if (lx > 60.0) {
            const double L1 = lx * ln2;
            const double L2 = std::log(L1);
            return BF(L1 - L2 + L2 / L1);
        }

        const double xd = lw_to_double(x);
        double w = (xd > 2.718281828459045) ? std::log(xd) - std::log(std::log(xd)) : std::log1p(xd);
        if (!(w > -1.0)) w = -0.9;
        w = lw_halley_d(w, xd);
        if (!(w > -1.0)) w = -0.9;
        return BF(w);
    }

    if (lx < -60.0) {                                                                      
        const double L1 = lx * ln2;                                                        
        const double L2 = std::log(-L1);
        return BF(L1 - L2 + L2 / L1);
    }

    const double xd = lw_to_double(x);
    const double L1 = std::log(-xd);
    double w = L1 - std::log(-L1);
    if (!(w < -1.0)) w = -1.1;
    w = lw_halley_d(w, xd);
    if (!(w < -1.0)) w = -1.1;
    return BF(w);
}

inline bool lw_newton(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t want, BF& w) {
    std::size_t sp = 128;
    w = lw_seed(x, lower, eps, have_eps, sp);
    const BF one  = BF::one();
    const BF mone = BF::one(true);
    const BF w1   = BF::add(w, one, lw_ctx(sp));
    if (w1.is_zero()) return false;
    const double br = std::max(0.0, -lw_log2(w1));                                         
    const double lm = std::log2(std::fabs(lw_to_double(w)) + 4.0);
    const std::size_t wp = want + static_cast<std::size_t>(br) + static_cast<std::size_t>(lm) + 24;
    std::size_t p = std::min<std::size_t>(std::max<std::size_t>(sp, 64), wp);

    for (int it = 0; it < 300; ++it) {
        const BFC pc  = lw_ctx(p);
        const BF  ew  = exp(w, pc);
        const BF  wp1 = BF::add(w, one, pc);
        if (wp1.is_zero()) return false;
        const BF  f    = BF::sub(BF::mul(w, ew, pc), x, pc);
        const BF  step = BF::div(f, BF::mul(ew, wp1, pc), pc);
        BF        wn   = BF::sub(w, step, pc);
        if (!lower && BF::compare(wn, mone) != BF::ordering::greater) wn = BF::add(w, mone, pc).scaled_pow2(-1);   
        if (lower  && BF::compare(wn, mone) != BF::ordering::less)    wn = BF::add(w, mone, pc).scaled_pow2(-1);
        const bool small = step.is_zero() || step.get_exp_base2() <= w.get_exp_base2() - static_cast<std::int64_t>(p) + 8;
        w = wn;

        if (small) {
            if (p == wp) return true;
            p = std::min<std::size_t>(2 * p, wp);
        }
    }

    return false;
}

inline int lw_sign(const BF& y, const BF& x, std::size_t q) {
    const BFC qc   = lw_ctx(q);
    const BF  prod = BF::mul(y, exp(y, qc), qc);
    const BF  g    = BF::sub(prod, x, qc);
    if (g.is_zero() || prod.is_zero()) return 0;
    const double lerr = lw_log2(prod) + std::log2(6.0 + 2.0 * std::fabs(lw_to_double(y))) - static_cast<double>(q);   
    return (lw_log2(g) - 1.0 > lerr) ? (g.signbit() ? -1 : 1) : 0;
}

inline lw_val lw_raw(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t want) {
    const BFC wc = lw_ctx(want);
    BF w;
    if (!lw_newton(x, lower, eps, have_eps, want, w)) return lw_val{BF::undefined(), 0.0};
    const BF one  = BF::one();
    const BF mone = BF::one(true);
    const std::int64_t ew    = w.get_exp_base2();
    const BF           delta = one.scaled_pow2(ew - static_cast<std::int64_t>(want) - 4);
    const BFC          xc    = lw_ctx(w.significand().bit_length() + want + 16);              
    const BF           ylo   = BF::sub(w, delta, xc);
    const BF           yhi   = BF::add(w, delta, xc);
    const BF           v     = w.rounded(wc);
    if (!lower && BF::compare(ylo, mone) != BF::ordering::greater) return lw_val{v, 1.0e9};    
    if (lower  && BF::compare(yhi, mone) != BF::ordering::less)    return lw_val{v, 1.0e9};
    const BF w1 = BF::add(w, one, wc);
    if (w1.is_zero()) return lw_val{v, 1.0e9};
    const std::size_t q = want + 40 + static_cast<std::size_t>(std::max(0.0, -lw_log2(w1))) + static_cast<std::size_t>(std::log2(std::fabs(lw_to_double(w)) + 4.0));
    const int slo = lw_sign(ylo, x, q);
    const int shi = lw_sign(yhi, x, q);
    const bool ok = lower ? (slo > 0 && shi < 0) : (slo < 0 && shi > 0);                      
    return lw_val{v, ok ? 2.0 : 1.0e9};
}

} // namespace lwdetail

inline BigFloat lambert_w(const BigFloat& x, std::int64_t k, const BigFloatContext& ctx) {
    using namespace lwdetail;
    if (x.is_nan())         return BigFloat::nan();
    if (x.is_undefined())   return BigFloat::undefined();
    if (k != 0 && k != -1)  return BigFloat::nan();
    const bool lower = (k == -1);
    if (x.is_infinite())    return (!lower && !x.signbit()) ? BigFloat::infinity() : BigFloat::nan();
    if (x.is_zero())        return lower ? BigFloat::infinity(true) : x;                     
    if (lower && !x.signbit()) return BigFloat::nan();
    BigFloat eps;
    bool     have_eps = false;

    if (x.signbit()) {
        const int s = lw_eps(x, eps);
        if (s < 0)  return BigFloat::nan();                                                  
        if (s == 0) return BigFloat::undefined();                                            
        have_eps = true;
    }

    return lw_drive(ctx, [&](std::size_t want) { return lw_raw(x, lower, eps, have_eps, want); });
}

inline BigFloat lambert_w(const BigFloat& x, std::int64_t k = 0)            { return lambert_w(x, k, BigFloatContext::current()); }
inline BigFloat lambert_w(const BigFloat& x, const BigFloatContext& ctx)    { return lambert_w(x, 0, ctx); }
inline BigFloat lambert_w0(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current())  { return lambert_w(x, 0, ctx); }
inline BigFloat lambert_wm1(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current()) { return lambert_w(x, -1, ctx); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, std::int64_t k, const BigFloatContext& ctx) { return lambert_w(BigFloat(x), k, ctx); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, std::int64_t k = 0) { return lambert_w(BigFloat(x), k, BigFloatContext::current()); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, const BigFloatContext& ctx) { return lambert_w(BigFloat(x), 0, ctx); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP