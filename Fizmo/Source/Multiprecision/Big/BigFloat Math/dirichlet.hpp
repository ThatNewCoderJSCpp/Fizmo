#ifndef FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP
#define FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP

#include "riemann_zeta.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace dldetail {

using ztdetail::zt_val;

inline double dl_lost(double units) { return std::log2(units) + 1.0; }

inline BigFloat dl_omp2(const BigFloat& x, const BigFloatContext& wc) {
    const BigFloat h = BigFloat::mul(x, constants::ln2(wc), wc).scaled_pow2(-1);
    return -BigFloat::mul(sinh(h, wc), exp(h, wc), wc).scaled_pow2(1);
}

inline double dl_omp2_units(const BigFloat& x) {
    return 6.0 + 5.0 * 0.34657359027997264 * std::fabs(ztdetail::zt_to_double(x));
}

inline double dl_power_sum_bound(std::uint64_t n, double sd) {
    const double nd = static_cast<double>(n);
    if (std::fabs(sd - 1.0) < 1e-9) return 1.01 * (1.0 + std::log(nd));
    return 1.01 * (1.0 + (std::pow(nd, 1.0 - sd) - 1.0) / (1.0 - sd));
}

inline zt_val dl_alt(const BigFloat& s, bool odd, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const double          sd = ztdetail::zt_to_double(s);
    const double          as = std::fabs(sd);
    const double          lb = odd ? 1.5849625007211562 : 1.0;          
    if (sd * lb >= static_cast<double>(want) + 2.0) return zt_val{BigFloat::one(), 1.0};  
    const std::uint64_t n = static_cast<std::uint64_t>(std::ceil((static_cast<double>(want) + 2.0) / 2.5431)) + 1;
    std::uint64_t K = 0;

    if (sd >= 2.0) {                                                    
        for (std::uint64_t k = 1; k <= n / 2; ++k) {
            const double idx = odd ? 2.0 * static_cast<double>(k) + 1.0 : static_cast<double>(k) + 1.0;
            if (-sd * std::log2(idx) <= -static_cast<double>(want) - 3.0) { K = k; break; }
        }
    }

    if (K != 0) {
        const std::uint64_t         M  = odd ? 2 * K - 1 : K;
        const std::vector<BigFloat> pw = ztdetail::zt_powers(s, M, wc);
        const std::int64_t          F  = static_cast<std::int64_t>(want + ztdetail::zt_bitlen(K)) + 2;
        BigInt A(static_cast<std::int64_t>(0));

        for (std::uint64_t k = 0; k < K; ++k) {
            const BigInt t = pw[odd ? 2 * k + 1 : k + 1].scaled_pow2(F).get_integer_part();
            if (k & 1u) A.sub_mutable(t); else A.add_mutable(t);
        }

        const double ce = 3.0 * as * std::log(static_cast<double>(M) + 1.0) + 2.0 * std::log2(static_cast<double>(M) + 1.0) + 3.0;
        return zt_val{BigFloat(A).scaled_pow2(-F).rounded(wc), dl_lost(2.0 + 4.0 * ce)};
    }

    const std::uint64_t         M  = odd ? 2 * n - 1 : n;
    const std::vector<BigFloat> pw = ztdetail::zt_powers(s, M, wc);
    BigUInt c  = BigUInt::one();
    BigUInt dn = BigUInt::one();

    for (std::uint64_t i = 1; i <= n; ++i) {
        ztdetail::zt_borwein_step(c, n, i);
        dn.add_mutable(c);
    }

    const std::int64_t F = static_cast<std::int64_t>(want + ztdetail::zt_bitlen(n)) + 2 - (static_cast<std::int64_t>(dn.bit_length()) - 1);
    BigInt  A(static_cast<std::int64_t>(0));
    BigUInt dk = BigUInt::one();
    c = BigUInt::one();

    for (std::uint64_t k = 0; k < n; ++k) {
        BigUInt e = dn;
        e.sub_mutable(dk);
        const BigInt t = BigFloat::mul(BigFloat(e), pw[odd ? 2 * k + 1 : k + 1], wc).scaled_pow2(F).get_integer_part();
        if (k & 1u) A.sub_mutable(t); else A.add_mutable(t);
        ztdetail::zt_borwein_step(c, n, k + 1);
        dk.add_mutable(c);
    }

    const BigFloat v  = BigFloat::div(BigFloat(A).scaled_pow2(-F), BigFloat(dn), wc);
    const double   ce = 3.0 * as * std::log(static_cast<double>(M) + 1.0) + 2.0 * std::log2(static_cast<double>(M) + 1.0) + 4.0;
    const double   B  = dl_power_sum_bound(n, sd);                      
    return zt_val{v, dl_lost(2.0 * (0.25 + B * ce) + 0.5 + 1.0)};
}

inline zt_val dl_eta_fe(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const zt_val c = ztdetail::zt_chi_low(s, om, want);
    if (!c.v.is_finite() || c.v.is_zero()) return c;
    const zt_val e = dl_alt(om, false, want);
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    BigFloat v = BigFloat::mul(dl_omp2(om, wc), c.v, wc);
    v = BigFloat::mul(v, e.v, wc);
    v = BigFloat::div(v, dl_omp2(s, wc), wc);
    return zt_val{v, dl_lost(dl_omp2_units(om) + dl_omp2_units(s) + std::exp2(c.lost) + std::exp2(e.lost) + 3.0)};
}

inline zt_val dl_lambda_pos(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const zt_val e = dl_alt(s, false, want);
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloat ms = -s;
    BigFloat v = BigFloat::mul(e.v, dl_omp2(ms, wc), wc);
    v = BigFloat::div(v, dl_omp2(om, wc), wc);
    return zt_val{v, dl_lost(dl_omp2_units(ms) + dl_omp2_units(om) + std::exp2(e.lost) + 2.0)};
}

inline zt_val dl_lambda_neg(const BigFloat& s, const BigFloat& om, std::size_t want) {
    const zt_val c = ztdetail::zt_chi_low(s, om, want);
    if (!c.v.is_finite() || c.v.is_zero()) return c;
    const zt_val e = dl_alt(om, false, want);
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloat p = exp(-BigFloat::mul(s, constants::ln2(wc), wc), wc);
    BigFloat v = BigFloat::mul(p, c.v, wc);
    v = BigFloat::mul(v, e.v, wc);
    const double a = std::fabs(ztdetail::zt_to_double(s)) * 0.6931471805599453;
    return zt_val{-v, dl_lost(2.0 + 3.0 * a + std::exp2(c.lost) + std::exp2(e.lost) + 2.0)};
}

inline zt_val dl_beta_fe(const BigFloat& u, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    BigFloat f;
    bool     flip = false;
    ztdetail::zt_reduce_half(u.scaled_pow2(-1), f, flip);
    if (f.is_zero()) return zt_val{BigFloat::zero(), 0.0};
    const BigFloat sn = sin(BigFloat::mul(constants::pi(wc), f, wc), wc);
    const BigFloat l  = BigFloat::sub(constants::ln2(wc), gmdetail::gm_ln_pi(wc), wc);    
    const BigFloat pw = exp(BigFloat::mul(u, l, wc), wc);
    const BigFloat g  = gamma(u, wc);
    if (!g.is_finite()) return zt_val{g, 0.0};
    const zt_val   b  = dl_alt(u, true, want);
    BigFloat v = BigFloat::mul(pw, sn, wc);
    v = BigFloat::mul(v, g, wc);
    v = BigFloat::mul(v, b.v, wc);
    const double ud = std::fabs(ztdetail::zt_to_double(u));
    return zt_val{flip ? -v : v, dl_lost(2.0 + 4.0 * ud + 3.0 + 1.0 + std::exp2(b.lost) + 3.0)};
}

inline bool dl_nonpos_int(const BigFloat& s, bool& fits, std::uint64_t& m, bool& even) {
    if (!(s.signbit() || s.is_zero()) || !s.get_fractional_part().is_zero()) return false;
    const BigInt k = s.get_integer_part();
    even = k.is_even();
    fits = k.bit_length() <= 62;
    m    = fits ? k.get_lowest_bits() : 0;
    return true;
}

inline std::uint64_t dl_log3_threshold(std::size_t prec) {         
    return static_cast<std::uint64_t>(std::ceil((static_cast<double>(prec) + 2.0) / 1.5849625007211562)) + 1;
}

template <typename Eval>
inline BigFloat dl_drive(const BigFloat& s, const BigFloatContext& ctx, Eval eval) {
    std::size_t guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const zt_val      r    = eval(want);
        if (!r.v.is_finite() || r.v.is_zero()) return r.v;
        if (ztdetail::zt_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return r.v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace dldetail

inline BigFloat dirichlet_eta(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return s.signbit() ? BigFloat::undefined() : BigFloat::one();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    bool          fits = false, even = false;
    std::uint64_t m    = 0;

    if (dldetail::dl_nonpos_int(s, fits, m, even)) {
        if (even) return (fits && m == 0) ? half : BigFloat::zero();  

        if (fits) {                                                  
            const std::uint64_t n = (m + 1) / 2;

            if (n <= constants::detail::bn_index_cap) {
                const BigUInt T = constants::tangent_number(n);
                if (!T.is_undefined()) return BigFloat(T, n % 2 == 0, -2 * static_cast<std::int64_t>(n)).rounded(ctx);
            }
        }
    }

    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return constants::ln2(ctx);
    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(2))) == BigFloat::ordering::equal) return constants::zeta2(ctx).scaled_pow2(-1);

    if (BigFloat::compare(s, BigFloat(static_cast<std::uint64_t>(ctx.precision + 2))) != BigFloat::ordering::less) {
        return ztdetail::zt_sticky(one, true, ctx.precision + 6, ctx);
    }

    if (s.get_exp_base2() < -static_cast<std::int64_t>(ctx.precision) - 10) {
        return ztdetail::zt_sticky(half, s.signbit(), ctx.precision + 8, ctx);
    }

    const bool     pos = !s.signbit();
    const BigFloat om  = ztdetail::zt_one_minus(s);

    return dldetail::dl_drive(s, ctx, [&](std::size_t want) -> dldetail::zt_val {
        return pos ? dldetail::dl_alt(s, false, want) : dldetail::dl_eta_fe(s, om, want);
    });
}

inline BigFloat dirichlet_lambda(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return s.signbit() ? BigFloat::undefined() : BigFloat::one();
    const BigFloat one = BigFloat::one();
    bool          fits = false, even = false;
    std::uint64_t m    = 0;

    if (dldetail::dl_nonpos_int(s, fits, m, even)) {
        if (even) return BigFloat::zero();                              

        if (fits) {                                                     
            const std::uint64_t n = (m + 1) / 2;

            if (n <= constants::detail::bn_index_cap) {
                const BigUInt T = constants::tangent_number(n);

                if (!T.is_undefined()) {
                    BigUInt a = constants::bfdetail::unit(2 * n - 1);
                    a.sub_small_mutable(1);
                    BigUInt den = constants::bfdetail::unit(2 * n);
                    den.sub_small_mutable(1);
                    return BigFloat::div(BigFloat(a * T, n % 2 == 0, -2 * static_cast<std::int64_t>(n)), BigFloat(den), ctx);
                }
            }
        }
    }

    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return BigFloat::undefined();     
    
    if (BigFloat::compare(s, BigFloat(dldetail::dl_log3_threshold(ctx.precision))) != BigFloat::ordering::less) {
        return ztdetail::zt_sticky(one, false, ctx.precision + 4, ctx);
    }

    const bool     pos = !s.signbit();
    const BigFloat om  = ztdetail::zt_one_minus(s);

    return dldetail::dl_drive(s, ctx, [&](std::size_t want) -> dldetail::zt_val {
        if (s.get_exp_base2() < -static_cast<std::int64_t>(want) - 4) {                         
            const BigFloatContext wc(want, RoundingMode::nearest_even);
            return dldetail::zt_val{(-BigFloat::mul(constants::ln2(wc), s, wc)).scaled_pow2(-1), 3.0};
        }

        return pos ? dldetail::dl_lambda_pos(s, om, want) : dldetail::dl_lambda_neg(s, om, want);
    });
}

inline BigFloat dirichlet_beta(const BigFloat& s, const BigFloatContext& ctx) {
    if (s.is_nan())       return BigFloat::nan();
    if (s.is_undefined()) return BigFloat::undefined();
    if (s.is_infinite())  return s.signbit() ? BigFloat::undefined() : BigFloat::one();
    const BigFloat one  = BigFloat::one();
    const BigFloat half = one.scaled_pow2(-1);
    bool          fits = false, even = false;
    std::uint64_t m    = 0;

    if (dldetail::dl_nonpos_int(s, fits, m, even)) {
        if (!even) return BigFloat::zero();                             

        if (fits) {                                                     
            const std::uint64_t k = m / 2;
            const BigUInt       S = constants::secant_number(k);
            if (!S.is_undefined()) return BigFloat(S, k % 2 == 1, -1).rounded(ctx);
        }
    }

    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return constants::quarter_pi(ctx);

    if (BigFloat::compare(s, BigFloat(dldetail::dl_log3_threshold(ctx.precision))) != BigFloat::ordering::less) {
        return ztdetail::zt_sticky(one, true, ctx.precision + 6, ctx);
    }

    if (s.get_exp_base2() < -static_cast<std::int64_t>(ctx.precision) - 10) {
        return ztdetail::zt_sticky(half, s.signbit(), ctx.precision + 8, ctx);
    }

    const bool     pos = !s.signbit();
    const BigFloat u   = pos ? s : ztdetail::zt_one_minus(s);
    
    return dldetail::dl_drive(s, ctx, [&](std::size_t want) -> dldetail::zt_val {
        return pos ? dldetail::dl_alt(s, true, want) : dldetail::dl_beta_fe(u, want);
    });
}

FIZMO_MP_TRIG_FORWARD(dirichlet_eta)
FIZMO_MP_TRIG_FORWARD(dirichlet_lambda)
FIZMO_MP_TRIG_FORWARD(dirichlet_beta)

#define FIZMO_MP_DIRICHLET_ARITH_FORWARD(FN)                                                            \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }                     \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_eta)
FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_lambda)
FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_beta)

#undef FIZMO_MP_DIRICHLET_ARITH_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP