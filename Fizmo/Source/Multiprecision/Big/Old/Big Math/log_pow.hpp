#ifndef FIZMO_MULTIPRECISION_EXP_LOG_HPP
#define FIZMO_MULTIPRECISION_EXP_LOG_HPP

#include "ntrig.hpp"   

namespace fizmo {
namespace multiprecision {

namespace math {
namespace detail {

inline BigFloat atanh_reciprocal(std::uint64_t p, std::size_t prec_bits) {
    const std::size_t   W   = prec_bits + 64;
    const std::uint64_t W64 = static_cast<std::uint64_t>(W);
    const std::uint64_t p2  = p * p;                           
    BigUint term = (BigUint(1) << W64) / p;                    
    BigUint sum  = term;

    for (std::uint64_t k = 3; ; k += 2) {
        term /= p2;                                            
        if (term.is_zero()) break;
        BigUint c = term / k;                                  
        if (c.is_zero()) break;
        sum += c;
    }

    return BigFloat::from_parts(false, sum, -static_cast<std::int64_t>(W), prec_bits);
}

inline BSResult atanh_bs(std::uint64_t p2, std::size_t a, std::size_t b) {
    if (b - a == 1) {
        const auto k = static_cast<std::uint64_t>(a);
        if (k == 0) return { BigInt(1), BigUint(1), BigInt(1) };
        const auto num = static_cast<std::int64_t>(2 * k - 1);
        const auto den = static_cast<std::uint64_t>((2 * k + 1) * p2);
        return { BigInt(num), BigUint(den), BigInt(num) };
    }

    const std::size_t m = (a + b) / 2;
    BSResult L = atanh_bs(p2, a, m);
    BSResult R = atanh_bs(p2, m, b);
    return { L.P * R.P, L.Q * R.Q, L.T * BigInt(R.Q) + L.P * R.T };
}

inline BigFloat atanh_reciprocal_bs(std::uint64_t p, std::size_t prec_bits) {
    const std::size_t    W   = prec_bits + 64;
    const auto           W64 = static_cast<std::uint64_t>(W);
    const std::uint64_t  p2  = p * p;
    const double         bpt = 2.0 * std::log2(static_cast<double>(p));
    const std::size_t    N   = static_cast<std::size_t>(static_cast<double>(W) / bpt) + 2;
    BSResult r      = atanh_bs(p2, 0, N);
    BigUint  base   = (BigUint(1) << W64) / BigUint(p);
    BigUint  result = base * r.T.magnitude() / r.Q;   
    return BigFloat::from_parts(false, result, -static_cast<std::int64_t>(W), prec_bits);
}

inline bool has_converged_term(const BigFloat& term, std::size_t prec_bits, std::size_t safety_bits = 4) {
    if (!term.is_finite() || term.is_zero()) return true;
    const std::int64_t eb2 = term.exponent_base2();
    const std::int64_t thresh = -static_cast<std::int64_t>(prec_bits + safety_bits);
    return eb2 <= thresh;
}

constexpr inline std::size_t default_newton_iters(std::size_t prec_bits) noexcept {
    std::size_t iters = 0;
    std::size_t target = (prec_bits > 53 ? prec_bits / 53 : 1);
    while (target > 1) { target = (target + 1) / 2; ++iters; }
    return iters + 3; 
}

struct PadePQ {
    BigFloat P;
    BigFloat Q;
};

struct ExpBSNode {
    BigUint     P;          // product of numerators
    BigUint     Q_int;      // integer (factorial) part of Q
    std::size_t Q_pow;      // power-of-two part of Q
    BigUint     T;          // running numerator of the partial sum
};

inline PadePQ pade_exp_7(const BigFloat& x, std::size_t W) {
    static const std::int64_t p_raw[8] = {
        64764752532480000LL,
        32382376266240000LL,
        7771770303897600LL,
        1187353796428800LL,
        129060195264000LL,
        10559470521600LL,
        670442572800LL,
        33522128640LL
    };

    static const std::int64_t q_raw[8] = {
        64764752532480000LL,
       -32382376266240000LL,
        7771770303897600LL,
       -1187353796428800LL,
        129060195264000LL,
       -10559470521600LL,
        670442572800LL,
       -33522128640LL
    };

    BigFloat P(0), Q(0);
    P.set_precision_bits(W);
    Q.set_precision_bits(W);

    for (int i = 7; i >= 0; --i) {
        BigFloat pi(p_raw[i]);
        BigFloat qi(q_raw[i]);
        pi.set_precision_bits(W);
        qi.set_precision_bits(W);
        P *= x;
        P += pi;
        Q *= x;
        Q += qi;
    }

    return { P, Q };
}

inline std::vector<BigUint> factorials_up_to(std::size_t n) {
    std::vector<BigUint> fact(n + 1);
    fact[0] = BigUint(1);
    for (std::size_t i = 1; i <= n; ++i) { fact[i] = fact[i - 1] * BigUint(static_cast<std::uint64_t>(i)); }
    return fact;
}

inline PadePQ pade_exp_mm(const BigFloat& x, std::size_t W, std::size_t m) {
    PadePQ res;
    res.P = BigFloat(0);
    res.Q = BigFloat(0);
    res.P.set_precision_bits(W);
    res.Q.set_precision_bits(W);

    if (m == 0) {
        BigFloat one(1);
        one.set_precision_bits(W);
        res.P = one;
        res.Q = one;
        return res;
    }

    const std::size_t max_n = 2 * m;
    const std::vector<BigUint>& fact = factorials_up_to(max_n);
    BigFloat xk(1);        
    xk.set_precision_bits(W);

    for (std::size_t k = 0; k <= m; ++k) {
        const std::size_t n_num = 2 * m - k;
        const std::size_t n_a   = m - k;
        const std::size_t n_b   = k;
        const BigUint& num_u = fact[n_num];
        const BigUint& den_a = fact[n_a];
        const BigUint& den_b = fact[n_b];
        BigFloat num(num_u);
        BigFloat den(den_a);
        den *= BigFloat(den_b);
        num.set_precision_bits(W);
        den.set_precision_bits(W);
        BigFloat ck = num / den;
        ck.set_precision_bits(W);
        BigFloat termP = ck * xk;
        termP.set_precision_bits(W);
        res.P += termP;
        BigFloat termQ = termP;
        if (k & 1) termQ.negate();
        res.Q += termQ;
        if (k != m) {
            xk *= x;
            xk.set_precision_bits(W);
        }
    }

    res.P.set_precision_bits(W);
    res.Q.set_precision_bits(W);
    return res;
}

std::size_t pade_m_for_precision(std::size_t prec_bits) {
    std::size_t W = prec_bits + 64;
    double m_est = std::sqrt(static_cast<double>(W) / 2.0);
    std::size_t m = static_cast<std::size_t>(m_est);
    if (m < 3) m = 3;
    if ((m & 1) == 0) ++m; 
    return m;
}

inline ExpBSNode exp_bs_split(std::uint64_t a, std::uint64_t b, const BigUint& num, std::size_t S) {
    if (b - a == 1) {
        if (a == 0) return { BigUint(1), BigUint(1), 0, BigUint(1) };
        return { num, BigUint(a), S, num };
    }
    const std::uint64_t m = a + (b - a) / 2;
    ExpBSNode L = exp_bs_split(a, m, num, S);
    ExpBSNode R = exp_bs_split(m, b, num, S);
    BigUint P     = L.P.mul_fft(R.P);
    BigUint Q_int = L.Q_int.mul_fft(R.Q_int);
    std::size_t Q_pow = L.Q_pow + R.Q_pow;
    BigUint t_left  = L.T.mul_fft(R.Q_int);
    if (R.Q_pow > 0) t_left <<= static_cast<std::uint64_t>(R.Q_pow);
    BigUint t_right = L.P.mul_fft(R.T);
    BigUint T = t_left + t_right;
    return { std::move(P), std::move(Q_int), Q_pow, std::move(T) };
}

} // namespace detail
} // namespace math

namespace constants {

inline BigFloat compute_ln2(std::size_t prec_input, bool input_is_decimal) {
    std::size_t prec_bits = input_is_decimal ? BigFloat::decimal_digits_to_bits(prec_input) : prec_input;
    std::size_t work = prec_bits + 64;
    BigFloat a = (prec_bits > 256) ? math::detail::atanh_reciprocal_bs(3, work) : math::detail::atanh_reciprocal(3, work);

    BigFloat val = BigFloat::from_parts(
        false,
        a.raw_significand(),
        a.raw_exponent() + 1,
        work
    );

    val.set_precision_bits(prec_bits);
    return val;
}

inline BigFloat ln2(std::size_t prec_input = 0, bool input_is_decimal = true) {
    std::size_t prec_bits = prec_input;
    if (input_is_decimal) prec_bits = BigFloat::decimal_digits_to_bits(prec_bits);
    if (prec_bits == 0) prec_bits = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    thread_local std::size_t cached_prec = 0;
    thread_local BigFloat    cached_val;

    if (prec_bits <= cached_prec) {
        BigFloat r = cached_val;
        r.set_precision_bits(prec_bits);
        return r;
    }

    cached_val  = compute_ln2(prec_bits + 64, false);
    cached_prec = prec_bits + 64;
    BigFloat r = cached_val;
    r.set_precision_bits(prec_bits);
    return r;
}

inline BigFloat compute_ln10(std::size_t prec_input, bool input_is_decimal) {
    std::size_t prec_bits = input_is_decimal ? BigFloat::decimal_digits_to_bits(prec_input) : prec_input;
    std::size_t work = prec_bits + 64;
    BigFloat ln2_val = ln2(work, false);
    BigFloat a9 = (prec_bits > 256) ? math::detail::atanh_reciprocal_bs(9, work) : math::detail::atanh_reciprocal(9, work);

    BigFloat two_at9 = BigFloat::from_parts(
        false,
        a9.raw_significand(),
        a9.raw_exponent() + 1,
        work
    );

    BigFloat result = BigFloat(3) * ln2_val + two_at9;
    result.set_precision_bits(prec_bits);
    return result;
}

inline BigFloat ln10(std::size_t prec_input = 0, bool input_is_decimal = true) {
    std::size_t prec_bits = prec_input;
    if (input_is_decimal) prec_bits = BigFloat::decimal_digits_to_bits(prec_bits);
    if (prec_bits == 0) prec_bits = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    thread_local std::size_t cached_prec = 0;
    thread_local BigFloat    cached_val;

    if (prec_bits <= cached_prec) {
        BigFloat r = cached_val;
        r.set_precision_bits(prec_bits);
        return r;
    }

    cached_val  = compute_ln10(prec_bits + 64, false);
    cached_prec = prec_bits + 64;
    BigFloat r = cached_val;
    r.set_precision_bits(prec_bits);
    return r;
}

} // namespace constants

namespace math {

inline BigFloat exp(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero()) return BigFloat(1);
    if (x.is_negative_infinity()) return BigFloat::zero();
    if (x.is_positive_infinity()) return BigFloat::positive_infinity();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloatContext::set_precision(W); 
    const std::uint64_t W64 = static_cast<std::uint64_t>(W);
    const BigFloat ln2_val = constants::ln2(W, false);
    const BigFloat ratio   = x / ln2_val;
    BigFloat n_bf = ratio.get_integer_part();
    if (ratio.is_negative() && !ratio.is_integer()) { n_bf = n_bf - BigFloat(1); }
    std::int64_t n = 0;
    {
        BigUint nm = n_bf.get_integer_part_uint();
        if (nm.limb_count() > 1) return n_bf.is_negative() ? BigFloat::zero() : BigFloat::positive_infinity();
        if (!nm.is_zero()) n = static_cast<std::int64_t>(nm.limb(0));
        if (n_bf.is_negative()) n = -n;
    }
    BigFloat r = x - n_bf * ln2_val;
    int halvings = 0;
    if (!r.is_zero()) {
        const int te = detail::optimal_target_exp(W);
        const std::int64_t eb2 = r.exponent_base2();
        if (eb2 > static_cast<std::int64_t>(te)) {
            halvings = static_cast<int>(eb2 - te) + 1;
            r = BigFloat::from_parts(false, r.raw_significand(), r.raw_exponent() - halvings, W);
        }
    }
    const BigUint one_fp = BigUint(1) << W64;
    const BigUint X      = detail::to_fixed_point(r, W);
    BigUint sum  = one_fp + X;
    BigUint term = X;
    for (std::uint64_t k = 2; ; ++k) {
        term = (term * X) >> W64;
        term /= k;
        if (term.is_zero()) break;
        sum += term;
    }
    for (int i = 0; i < halvings; ++i) sum = (sum * sum) >> W64;
    return BigFloat::from_parts(false, sum, -static_cast<std::int64_t>(W) + n, prec);
}

inline BigFloat exp(const BigInt&      x) { return exp(BigFloat(x)); }
inline BigFloat exp(const BigUint&     x) { return exp(BigFloat(x)); }
inline BigFloat exp(const BigRational& x) { return exp(static_cast<BigFloat>(x)); }

inline BigFloat pade_exp(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat(1);
    if (x.is_negative_infinity())       return BigFloat::zero();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    std::size_t W = prec + 128;
    const std::size_t m = detail::pade_m_for_precision(W);
    W = W + m + static_cast<std::size_t>(std::log2(m)) + 128;
    const std::size_t saved_prec = BigFloatContext::precision();
    const BigFloat ln2_val = constants::ln2(W, false);
    BigFloat ratio = x / ln2_val;
    BigFloat n_bf  = ratio.get_integer_part();
    if (ratio.is_negative() && !ratio.is_integer()) { n_bf = n_bf - BigFloat(1); }
    std::int64_t n = 0;
    {
        BigUint nm = n_bf.get_integer_part_uint();
        if (nm.limb_count() > 1) { return n_bf.is_negative() ? BigFloat::zero() : BigFloat::positive_infinity(); }
        if (!nm.is_zero()) n = static_cast<std::int64_t>(nm.limb(0));
        if (n_bf.is_negative()) n = -n;
    }
    BigFloat r = x - n_bf * ln2_val;
    r.set_precision_bits(W);
    int halvings = 0;
    if (!r.is_zero()) {
        const double lg_m   = std::lgamma(static_cast<double>(m + 1));
        const double lg_2m  = std::lgamma(static_cast<double>(2 * m + 1));
        const double free_bits = (2.0 * lg_m - lg_2m) / std::log(2.0) - std::log2(static_cast<double>(2 * m + 1));
        const std::int64_t eb2 = r.exponent_base2();
        const auto order = static_cast<std::int64_t>(2 * m + 1);
        const auto target = static_cast<std::int64_t>((-static_cast<double>(W) - free_bits) / static_cast<double>(order));
        if (eb2 > target) {
            halvings = static_cast<int>(eb2 - target) + 1;
            r = BigFloat::from_parts(
                r.is_negative(),
                r.raw_significand(),
                r.raw_exponent() - halvings,
                W
            );
        }
    }
    const std::size_t W_eff = W + 2 * static_cast<std::size_t>(halvings) + 128;
    BigFloatContext::set_precision(W_eff);
    r.set_precision_bits(W_eff);
    auto pq = detail::pade_exp_mm(r, W_eff, m);
    BigFloat er = pq.P / pq.Q;
    er.set_precision_bits(W_eff);
    for (int i = 0; i < halvings; ++i) {
        er = er * er;
        const std::size_t bl = er.raw_significand().bit_length();
        if (bl > W_eff + 1) {
            const std::size_t shift = bl - W_eff;
            er = BigFloat::from_parts(
                er.is_negative(),
                er.raw_significand() >> shift,
                er.raw_exponent() + static_cast<std::int64_t>(shift),
                W_eff
            );
        }
    }
    BigFloat result = er * BigFloat::from_parts(false, BigUint(1), n, W_eff);
    BigFloatContext::set_precision(saved_prec);
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat exp_bs(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    if (x.is_negative_infinity())       return BigFloat::zero();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    if (x.is_zero()) {
        BigFloat one(1);
        one.set_precision_bits(prec);
        return one;
    }
    const bool neg_arg = x.is_negative();
    BigFloat ax = x;
    if (neg_arg) ax.abs_in_place();
    std::size_t K = 1;
    while (K * K < prec) ++K;
    K = std::max<std::size_t>(10, K);
    const std::size_t W   = prec + K + 128; 
    const auto        W64 = static_cast<std::uint64_t>(W);
    ax.set_precision_bits(W);
    BigFloat ln2_w = constants::ln2(W, false);
    ln2_w.set_precision_bits(W);
    BigFloat ratio = ax / ln2_w;
    BigUint  n_uint = ratio.get_integer_part_uint();
    BigFloat n_bf(n_uint);
    n_bf.set_precision_bits(W);
    BigFloat r = ax - n_bf * ln2_w;
    if (r.is_negative()) {
        r += ln2_w;
        if (!n_uint.is_zero()) n_uint -= 1ULL;
    }
    if (!r.is_negative() && r >= ln2_w) {
        r -= ln2_w;
        n_uint += 1ULL;
    }
    if (n_uint.bit_length() > 62) {
        return neg_arg ? BigFloat::zero() : BigFloat::positive_infinity();
    }
    const std::int64_t n = n_uint.is_zero() ? 0 : static_cast<std::int64_t>(n_uint.limb(0));
    BigUint R;
    if (!r.is_zero()) {
        const std::int64_t shift = r.raw_exponent() + static_cast<std::int64_t>(W) - static_cast<std::int64_t>(K);
        R = (shift >= 0) ? r.raw_significand() << static_cast<std::uint64_t>(shift) : r.raw_significand() >> static_cast<std::uint64_t>(-shift);
    }
    BigUint sum;                                    
    static constexpr std::size_t BS_THRESHOLD = 4096;
    if (W <= BS_THRESHOLD || R.is_zero()) {
        sum = BigUint(1) << W64;                    
        if (!R.is_zero()) {
            BigUint term = R;
            sum += term;
            for (std::uint64_t k = 2; ; ++k) {
                term = (term.mul_fft(R)) >> W64;
                term = term / k;
                if (term.is_zero()) break;
                sum += term;
            }
        }
    } else {
        const BigUint& num = r.raw_significand();
        const std::size_t S = static_cast<std::size_t>(-r.raw_exponent() + static_cast<std::int64_t>(K));
        const std::uint64_t N = detail::exp_term_count(W, K);
        detail::ExpBSNode bs = detail::exp_bs_split(0, N, num, S);
        if (W >= bs.Q_pow) {
            BigUint numer = bs.T << static_cast<std::uint64_t>(W - bs.Q_pow);
            sum = numer / bs.Q_int;
        } else {
            BigUint denom = bs.Q_int << static_cast<std::uint64_t>(bs.Q_pow - W);
            sum = bs.T / denom;
        }
    }
    for (std::size_t i = 0; i < K; ++i) { sum = (sum.mul_fft(sum)) >> W64; }
    BigFloat result = BigFloat::from_parts(
        false,
        std::move(sum),
        -static_cast<std::int64_t>(W) + n,
        prec
    );
    if (neg_arg) result = result.reciprocal();
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat exp_bs(const BigInt&  x) { return exp_bs(BigFloat(x)); }
inline BigFloat exp_bs(const BigUint& x) { return exp_bs(BigFloat(x)); }

inline BigFloat ln(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())              return BigFloat::negative_infinity();
    if (x.is_negative())          return BigFloat::nan();
    if (x.is_positive_infinity()) return BigFloat::positive_infinity();
    if (x == BigFloat(1))         return BigFloat::zero();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W    = prec + 64;
    const auto        W64  = static_cast<std::uint64_t>(W);
    const std::int64_t n   = x.exponent_base2();

    BigFloat m = BigFloat::from_parts(
        false,
        x.raw_significand(),
        x.raw_exponent() - n,   
        W
    );

    BigFloat one_w(1); 
    one_w.set_precision_bits(W);
    BigFloat z = (m - one_w) / (m + one_w);
    const BigUint Z  = detail::to_fixed_point(z, W);
    const BigUint Z2 = (Z * Z) >> W64;   
    BigUint term = Z;
    BigUint sum  = Z;

    for (std::uint64_t k = 3; ; k += 2) {
        term = (term * Z2) >> W64;
        if (term.is_zero()) break;
        BigUint contrib = term / k;
        if (contrib.is_zero()) break;
        sum += contrib;
    }

    BigFloat ln_m = BigFloat::from_parts(false, sum, -static_cast<std::int64_t>(W) + 1, prec);
    if (n == 0) return ln_m;
    BigFloat ln2_val = constants::ln2(W, false);
    ln2_val.set_precision_bits(prec);
    return BigFloat(n) * ln2_val + ln_m;      
}

inline BigFloat ln(const BigInt&      x) { return ln(BigFloat(x)); }
inline BigFloat ln(const BigUint&     x) { return ln(BigFloat(x)); }

inline BigFloat ln(const BigRational& x) { 
    const std::size_t prec = x.precision_bits();
    const std::size_t W = prec + 64;
    BigFloat res = ln(x.numerator());  res.set_precision_bits(W);
    BigFloat den = ln(x.denominator()); den.set_precision_bits(W);
    res -= den;
    BigFloat result = res;
    result.set_precision_bits(prec);
    return result;
}

} // namespace math

namespace constants {

inline BigFloat compute_e(std::size_t prec_input, bool input_is_decimal) {
    std::size_t prec_bits = input_is_decimal ? BigFloat::decimal_digits_to_bits(prec_input) : prec_input;
    BigFloat one(1);
    one.set_precision_bits(prec_bits);
    BigFloat val = math::exp(one);
    val.set_precision_bits(prec_bits);
    return val;
}

inline BigFloat e(std::size_t prec_input = 0, bool input_is_decimal = true) {
    std::size_t prec_bits = prec_input;
    if (input_is_decimal) prec_bits = BigFloat::decimal_digits_to_bits(prec_bits);
    if (prec_bits == 0) prec_bits = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    thread_local std::size_t cached_prec = 0;
    thread_local BigFloat    cached_val;

    if (prec_bits <= cached_prec) {
        BigFloat r = cached_val;
        r.set_precision_bits(prec_bits);
        return r;
    }

    cached_val  = compute_e(prec_bits + 64, false);
    cached_prec = prec_bits + 64;
    BigFloat r = cached_val;
    r.set_precision_bits(prec_bits);
    return r;
}

inline BigFloat compute_phi(std::size_t prec_input, bool input_is_decimal) {
    std::size_t prec_bits = input_is_decimal ? BigFloat::decimal_digits_to_bits(prec_input) : prec_input;
    std::size_t work = prec_bits + 64;
    BigFloat five(5); five.set_precision_bits(work);
    BigFloat sq5 = math::sqrt(five);
    BigFloat one(1); one.set_precision_bits(work);
    BigFloat result = one;
    result += sq5;
    result /= BigFloat(2);
    result.set_precision_bits(prec_bits);
    return result;
}

inline BigFloat phi(std::size_t prec_input = 0, bool input_is_decimal = true) {
    std::size_t prec_bits = prec_input;
    if (input_is_decimal) prec_bits = BigFloat::decimal_digits_to_bits(prec_bits);
    if (prec_bits == 0) prec_bits = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    thread_local std::size_t cached_prec = 0;
    thread_local BigFloat    cached_val;

    if (prec_bits <= cached_prec) {
        BigFloat r = cached_val;
        r.set_precision_bits(prec_bits);
        return r;
    }

    cached_val  = compute_phi(prec_bits + 64, false);
    cached_prec = prec_bits + 64;
    BigFloat r = cached_val;
    r.set_precision_bits(prec_bits);
    return r;
}

} // namespace constants 

namespace math {

inline BigFloat pow2(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat(1);
    if (x.is_negative_infinity())       return BigFloat::zero();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    if (x.is_negative())                return BigFloat(1) / pow2(-x); 

    if (!x.is_negative() && x.is_integer()) {
        BigUint n = x.get_integer_part_uint();

        if (n.limb_count() > 1 ||
            n.limb(0) > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
        ) {
            return BigFloat::positive_infinity();
        }

        return BigFloat::from_parts(false, BigUint(1), static_cast<std::int64_t>(n.limb(0)), BigFloat::effective_precision_bits(x));
    }

    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W    = prec + 64;
    BigFloat ln2_val = constants::ln2(W, false);
    ln2_val.set_precision_bits(W);
    BigFloat arg = x;
    arg.set_precision_bits(W);
    return exp(arg * ln2_val);
}

inline BigFloat pow2(const BigUint& x) {
    if (x.is_zero()) return BigFloat(1);
    
    if (
        x.limb_count() > 1 ||
        x.limb(0) > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
    ) {
        return BigFloat::positive_infinity();
    }

    const std::size_t prec = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    return BigFloat::from_parts(false, BigUint(1), static_cast<std::int64_t>(x.limb(0)), prec);
}

inline BigFloat pow2(const BigInt&      x) { 
    if (x.is_negative()) return BigFloat(1) / pow2(x.magnitude());
    return pow2(x.magnitude());
}

inline BigFloat pow2(const BigRational& x) { return pow2(static_cast<BigFloat>(x)); }

inline BigFloat pow10(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined())  return x;
    if (x.is_zero())                     return BigFloat(1);
    if (x.is_negative_infinity())        return BigFloat::zero();
    if (x.is_positive_infinity())        return BigFloat::positive_infinity();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W    = prec + 64;
    BigFloat ln10_val = constants::ln10(W, false);
    ln10_val.set_precision_bits(W);
    BigFloat arg = x;
    arg.set_precision_bits(W);
    return exp(arg * ln10_val);
}

inline BigFloat pow10(const BigUint& n) {
    if (n.is_zero()) return BigFloat(1);

    if (n.limb_count() > 1 ||
        n.limb(0) > std::numeric_limits<std::int64_t>::max()
    ) {
        return BigFloat::positive_infinity();
    }

    std::size_t e = static_cast<std::size_t>(n.limb(0));
    BigUint five_pow(1);
    if (e == 0) { return BigFloat::from_parts(false, five_pow, e, BigFloatContext::precision()); }
    BigUint b(5);
    
    while (e > 0) {
        if (e & 1) five_pow *= b;
        b *= b;
        e >>= 1;
    }

    return BigFloat::from_parts(false, five_pow, e, BigFloatContext::precision());
}

inline BigFloat pow10(const BigInt& n) {
    if (n.is_negative()) return BigFloat(1) / pow10(n.magnitude());
    return pow10(n.magnitude());
}

inline BigFloat pow10(const BigRational& x) { return pow10(static_cast<BigFloat>(x)); }

inline BigFloat log2(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::negative_infinity();
    if (x.is_negative())                return BigFloat::nan();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W    = prec + 64;
    BigFloat ln2_val = constants::ln2(W, false);
    ln2_val.set_precision_bits(W);
    BigFloat lnx = ln(x);
    lnx.set_precision_bits(W);
    BigFloat result = lnx / ln2_val;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat log2(const BigInt&      x) { return log2(BigFloat(x)); }
inline BigFloat log2(const BigUint&     x) { return log2(BigFloat(x)); }

inline BigFloat log2(const BigRational& x) {
    const std::size_t prec = x.precision_bits();
    const std::size_t W = prec + 64;
    BigFloat res = ln(x.numerator());  res.set_precision_bits(W);
    BigFloat den = ln(x.denominator()); den.set_precision_bits(W);
    res -= den;
    BigFloat ln2_val = constants::ln2(W, false);
    BigFloat result = res / ln2_val;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat log10(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::negative_infinity();
    if (x.is_negative())                return BigFloat::nan();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    const std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W    = prec + 64;
    BigFloat ln10_val = constants::ln10(W, false);
    ln10_val.set_precision_bits(W);
    BigFloat lnx = ln(x);
    lnx.set_precision_bits(W);
    BigFloat result = lnx / ln10_val;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat log10(const BigInt&      x) { return log10(BigFloat(x)); }
inline BigFloat log10(const BigUint&     x) { return log10(BigFloat(x)); }

inline BigFloat log10(const BigRational& x) { 
    const std::size_t prec = x.precision_bits();
    const std::size_t W = prec + 64;
    BigFloat res = ln(x.numerator());  res.set_precision_bits(W);
    BigFloat den = ln(x.denominator()); den.set_precision_bits(W);
    res -= den;
    BigFloat ln10_val = constants::ln10(W, false);
    BigFloat result = res / ln10_val;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat pow(const BigFloat& base, const BigFloat& exponent) {
    if (base.is_nan() || base.is_undefined())     return base;
    if (exponent.is_nan() || exponent.is_undefined()) return exponent;
    const std::size_t prec = BigFloat::effective_precision_bits(base, exponent);
    if (exponent.is_zero()) return BigFloat(1);
    if (!base.is_negative() && base == BigFloat(1)) return BigFloat(1);

    if (base.is_zero()) {
        if (exponent.is_negative()) return BigFloat::positive_infinity();
        return BigFloat::zero();
    }

    if (exponent.is_infinite()) {
        BigFloat abs_base = base.is_negative() ? -base : base;
        BigFloat one(1);
        bool base_gt1 = abs_base > one;
        bool base_lt1 = abs_base < one;
        if (exponent.is_positive_infinity()) return base_gt1 ? BigFloat::positive_infinity() : (base_lt1 ? BigFloat::zero() : BigFloat(1));
        return base_gt1 ? BigFloat::zero() : (base_lt1 ? BigFloat::positive_infinity() : BigFloat(1));
    }

    if (base.is_positive_infinity()) {
        return exponent.is_negative() ? BigFloat::zero() : BigFloat::positive_infinity();
    }

    if (base.is_negative_infinity()) {
        if (!exponent.is_integer()) return BigFloat::nan();
        bool odd = !(exponent.get_integer_part_uint() % BigUint(2)).is_zero();
        BigFloat mag = exponent.is_negative() ? BigFloat::zero() : BigFloat::positive_infinity();
        if (odd && !exponent.is_negative()) mag = BigFloat::negative_infinity();
        return mag;
    }

    if (base.is_negative() && !exponent.is_integer()) return BigFloat::nan();

    if (base.is_negative()) {
        bool odd = !(exponent.get_integer_part_uint() % BigUint(2)).is_zero();
        BigFloat pos_result = pow(-base, exponent);
        return (odd && exponent.is_negative()) || (odd && !exponent.is_negative()) ? -pos_result : pos_result;
    }

    const std::size_t W = prec + 64;
    BigFloat ln_base = ln(base);
    ln_base.set_precision_bits(W);
    BigFloat exp_w = exponent;
    exp_w.set_precision_bits(W);
    BigFloat result = exp(exp_w * ln_base);
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat pow(const BigFloat& base, const BigUint& exponent) {
    if (exponent.is_zero()) return BigFloat(1);
    BigUint e = exponent;
    BigFloat result(1);
    BigFloat cur = base;

    while (!e.is_zero()) {
        if (e.limb(0) & 1) result = result * cur;
        cur = cur * cur;
        e >>= 1;
    }

    return result;
}

inline BigFloat pow(const BigFloat& base, const BigInt& exponent) {
    if (exponent.is_zero()) return BigFloat(1);

    if (exponent.is_negative()) {
        BigFloat pos = pow(base, exponent.magnitude());
        return BigFloat(1) / pos;
    }

    BigUint e = exponent.magnitude();
    BigFloat result(1);
    BigFloat cur = base;

    while (!e.is_zero()) {
        if (e.limb(0) & 1) result = result * cur;
        cur = cur * cur;
        e >>= 1;
    }

    return result;
}

inline BigFloat pow(const BigFloat& b, const BigRational& e) { return pow(b, static_cast<BigFloat>(e)); }
inline BigFloat pow(const BigInt&      b, const BigFloat& e) { return pow(BigFloat(b), e); }
inline BigFloat pow(const BigUint&     b, const BigFloat& e) { return pow(BigFloat(b), e); }
inline BigFloat pow(const BigRational& b, const BigFloat& e) { return pow(static_cast<BigFloat>(b), e); }
inline BigFloat pow(const BigInt&      b, const BigInt&   e) { return pow(BigFloat(b), BigFloat(e)); }
inline BigFloat pow(const BigUint&     b, const BigUint&  e) { return pow(BigFloat(b), BigFloat(e)); }

inline BigFloat log(const BigFloat& x, const BigFloat& base) {
    if (x.is_nan()    || x.is_undefined())    return x;
    if (base.is_nan() || base.is_undefined()) return base;
    const std::size_t prec = BigFloat::effective_precision_bits(x, base);
    if (base == BigFloat(1) || base.is_zero()) return BigFloat::undefined();
    if (x.is_zero())              return BigFloat::negative_infinity();
    if (x.is_negative())          return BigFloat::nan();
    if (x.is_positive_infinity()) return BigFloat::positive_infinity();
    if (x == BigFloat(1))         return BigFloat::zero();
    if (base == BigFloat(2))  return log2(x);
    if (base == BigFloat(10)) return log10(x);
    const std::size_t W = prec + 64;
    BigFloat lnx   = ln(x);    lnx.set_precision_bits(W);
    BigFloat lnb   = ln(base); lnb.set_precision_bits(W);
    BigFloat result = lnx / lnb;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat log(const BigFloat& x, const BigInt&      base) { return log(x, BigFloat(base)); }
inline BigFloat log(const BigFloat& x, const BigUint&     base) { return log(x, BigFloat(base)); }
inline BigFloat log(const BigFloat& x, const BigRational& base) { return log(x, static_cast<BigFloat>(base)); }
inline BigFloat log(const BigInt&      x, const BigFloat& base) { return log(BigFloat(x), base); }
inline BigFloat log(const BigUint&     x, const BigFloat& base) { return log(BigFloat(x), base); }
inline BigFloat log(const BigRational& x, const BigFloat& base) { return log(static_cast<BigFloat>(x), base); }
inline BigFloat log(const BigInt&      x, const BigInt&   base) { return log(BigFloat(x), BigFloat(base)); }
inline BigFloat log(const BigUint&     x, const BigUint&  base) { return log(BigFloat(x), BigFloat(base)); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_EXP_LOG_HPP