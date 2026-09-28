#ifndef FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP
#define FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP

#include "gamma.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace ggdetail {

inline BigFloat gg_plus_one(const BigFloat& x) {                        
    if (x.is_zero()) return BigFloat::one();
    const std::int64_t e   = x.get_exp_base2();
    const std::int64_t top = (e > 0) ? e : 0;
    const std::int64_t lsb = (x.exponent() < 0) ? x.exponent() : 0;
    const std::size_t  p   = BigFloatContext::clamp_precision(static_cast<std::size_t>(top - lsb) + 3);
    return BigFloat::add(x, BigFloat::one(), BigFloatContext(p, RoundingMode::nearest_even));
}

inline BigUInt gg_pow(BigUInt b, std::uint64_t e) {
    BigUInt r = BigUInt::one();

    while (e != 0) {
        if (e & 1u) r = r * b;
        e >>= 1;
        if (e != 0) b = b * b;
    }

    return r;
}

inline bool gg_safe(const BigFloat& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                     
    const double e = std::ceil(lost);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    const std::size_t err = (e <= 0.0) ? 0 : static_cast<std::size_t>(e);
    return constants::bfdetail::round_is_safe(sig, prec, err + 2);
}

inline bool gg_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

inline double gg_units(double nu, double lnb) {
    const double tpsi = nu * (std::fabs(std::log(nu)) + 1.0) + 1.0;
    return 1.0 + tpsi + 2.0 + 4.0 * nu * std::fabs(lnb) + 3.0;
}

inline bool gg_integer_nu(const BigFloat& a1, const BigFloat& c, std::uint64_t& n) {
    const std::size_t     La = a1.significand().bit_length();
    const std::size_t     Lc = c.significand().bit_length();
    const BigFloatContext qc(BigFloatContext::clamp_precision(La + Lc + 64), RoundingMode::nearest_even);
    const BigFloat        q = BigFloat::div(a1.abs(), c.abs(), qc);
    if (!q.get_fractional_part().is_zero() || q.get_exp_base2() > 62) return false;
    const BigFloatContext mc(BigFloatContext::clamp_precision(q.significand().bit_length() + Lc + 2), RoundingMode::nearest_even);
    if (BigFloat::compare(BigFloat::mul(q, c.abs(), mc), a1.abs()) != BigFloat::ordering::equal) return false;
    n = q.get_integer_part().get_lowest_bits();
    return n >= 1;
}

inline bool gg_exact(std::uint64_t n, const BigFloat& b, const BigFloat& c, const BigFloatContext& ctx, BigFloat& out) {
    if (n - 1 > gmdetail::fact_n_cap()) return false;
    const BigUInt& B    = b.significand();
    const double   nd   = static_cast<double>(n);
    const double   bits = nd * static_cast<double>(B.bit_length()) + std::lgamma(nd) / 0.6931471805599453 + static_cast<double>(c.significand().bit_length());
    if (bits > 16.0 * static_cast<double>(ctx.precision) + 65536.0) return false;
    if (bits + 256.0 >= static_cast<double>(BigUInt::max_bits))     return false;
    const double sh = nd * static_cast<double>(b.exponent()) + static_cast<double>(c.exponent());
    if (std::fabs(sh) > 4.0e18) return false;
    const std::int64_t e   = -static_cast<std::int64_t>(n) * b.exponent() - c.exponent();
    const BigUInt      den = c.significand() * gg_pow(B, n);
    out = BigFloat::div(BigFloat(gmdetail::fact_range(1, n), false, e), BigFloat(den), ctx);
    return true;
}

} // namespace ggdetail

inline BigFloat generalized_gaussian_moment(const BigFloat& power, const BigFloat& rate, const BigFloat& shape, const BigFloatContext& ctx) {
    if (power.is_nan()       || rate.is_nan()       || shape.is_nan())       return BigFloat::nan();
    if (power.is_undefined() || rate.is_undefined() || shape.is_undefined()) return BigFloat::undefined();
    if (power.is_infinite()  || shape.is_infinite())                         return BigFloat::undefined();
    if (shape.is_zero())                                                     return BigFloat::infinity();
    const BigFloat a1 = ggdetail::gg_plus_one(power);
    if (a1.is_zero() || a1.signbit() != shape.signbit())                     return BigFloat::infinity();   
    if (rate.is_zero() || rate.signbit())                                    return BigFloat::infinity();
    if (rate.is_infinite())                                                  return BigFloat::zero();
    std::uint64_t n = 0;
    BigFloat      out;
    if (ggdetail::gg_integer_nu(a1, shape, n) && ggdetail::gg_exact(n, rate, shape, ctx, out)) return out;
    const BigFloat an = a1.abs();
    const BigFloat cn = shape.abs();
    const double nu0  = std::exp2(gmdetail::gm_log2_fine(an) - gmdetail::gm_log2_fine(cn));
    const double lnb  = gmdetail::gm_log2_fine(rate) * 0.6931471805599453;
    const double est  = std::log2(ggdetail::gg_units(nu0, lnb)) + 1.0;
    std::size_t guard = 32 + ((est > 0.0 && est < 4096.0) ? static_cast<std::size_t>(est) : 0);

    for (;;) {
        const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const BigFloat        nu = BigFloat::div(an, cn, wc);
        const BigFloat        g  = gamma(nu, wc);
        if (!g.is_finite()) return g;
        const BigFloat p = exp(-BigFloat::mul(nu, ln(rate, wc), wc), wc);                         
        const BigFloat v = BigFloat::div(BigFloat::mul(g, p, wc), cn, wc);
        if (!v.is_finite() || v.is_zero()) return v;
        const double nud  = std::exp2(gmdetail::gm_log2_fine(nu));
        const double lost = std::log2(ggdetail::gg_units(nud, lnb)) + 1.0;
        if (ggdetail::gg_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (ggdetail::gg_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat generalized_gaussian_integral(const BigFloat& rate, const BigFloat& shape, const BigFloatContext& ctx) {
    return generalized_gaussian_moment(BigFloat::zero(), rate, shape, ctx);
}

inline BigFloat generalized_gaussian_moment(const BigFloat& power, const BigFloat& rate, const BigFloat& shape) {
    return generalized_gaussian_moment(power, rate, shape, BigFloatContext::current());
}

inline BigFloat generalized_gaussian_integral(const BigFloat& rate, const BigFloat& shape) {
    return generalized_gaussian_integral(rate, shape, BigFloatContext::current());
}

template <typename P, typename R, typename S, typename std::enable_if<!(std::is_same<P, BigFloat>::value && std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_moment(const P& power, const R& rate, const S& shape, const BigFloatContext& c) {
    return generalized_gaussian_moment(BigFloat(power), BigFloat(rate), BigFloat(shape), c);
}

template <typename P, typename R, typename S, typename std::enable_if<!(std::is_same<P, BigFloat>::value && std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_moment(const P& power, const R& rate, const S& shape) {
    return generalized_gaussian_moment(BigFloat(power), BigFloat(rate), BigFloat(shape), BigFloatContext::current());
}

template <typename R, typename S, typename std::enable_if<!(std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_integral(const R& rate, const S& shape, const BigFloatContext& c) {
    return generalized_gaussian_integral(BigFloat(rate), BigFloat(shape), c);
}

template <typename R, typename S, typename std::enable_if<!(std::is_same<R, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat generalized_gaussian_integral(const R& rate, const S& shape) {
    return generalized_gaussian_integral(BigFloat(rate), BigFloat(shape), BigFloatContext::current());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_GENERALIZED_GAUSSIAN_HPP