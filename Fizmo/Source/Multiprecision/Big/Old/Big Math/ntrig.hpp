#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_TRIG_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_TRIG_HPP

#include "abs_root.hpp"  
#include <cmath>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

inline BigUint to_fixed_point(const BigFloat& val, std::size_t W) {
    BigUint sig      = val.raw_significand();
    std::int64_t exp = val.raw_exponent();
    std::int64_t shift = exp + static_cast<std::int64_t>(W);
    return shift < 0 ? (sig >> static_cast<std::uint64_t>(-shift)) : (sig << static_cast<std::uint64_t>(shift));
}

inline int optimal_target_exp(std::size_t W) noexcept {
    int t = static_cast<int>(std::sqrt(static_cast<double>(W) / 3.0));
    if (t < 4)  t = 4;
    if (t > 64) t = 64;
    return -t;
}

inline BigFloat atan_reciprocal(std::uint64_t p, std::size_t prec_bits) {
    const std::size_t   guard = 64;
    const std::size_t   W     = prec_bits + guard;
    const std::uint64_t W64   = static_cast<std::uint64_t>(W);
    const std::uint64_t p2    = p * p;                          
    BigUint one_shifted = BigUint(1) << W64;
    BigUint term        = one_shifted / p;                      
    BigUint pos_sum = term, neg_sum(0);
    bool next_neg = true;

    for (std::uint64_t k = 3; ; k += 2) {
        term /= p2;                                             
        if (term.is_zero()) break;
        BigUint contrib = term / k;                             
        if (contrib.is_zero()) break;
        if (next_neg) neg_sum += contrib; else pos_sum += contrib;
        next_neg = !next_neg;
    }

    return BigFloat::from_parts(false, pos_sum - neg_sum, -static_cast<std::int64_t>(W), prec_bits);
}

struct BSResult { BigInt P; BigUint Q; BigInt T; };

inline BSResult atan_bs(std::uint64_t p2, std::size_t a, std::size_t b) {
    if (b - a == 1) {
        std::uint64_t k = static_cast<std::uint64_t>(a);
        if (k == 0) return { BigInt(1), BigUint(1), BigInt(1) };
        std::int64_t  num = -static_cast<std::int64_t>(2 * k - 1);
        std::uint64_t den = (2 * k + 1) * p2;
        return { BigInt(num), BigUint(den), BigInt(num) };
    }
    std::size_t m = (a + b) / 2;
    BSResult L = atan_bs(p2, a, m);
    BSResult R = atan_bs(p2, m, b);
    BigInt  P = L.P * R.P;
    BigUint Q = L.Q * R.Q;
    BigInt  T = L.T * BigInt(R.Q) + L.P * R.T;
    return { P, Q, T };
}

inline BigFloat atan_reciprocal_bs(std::uint64_t p, std::size_t prec_bits) {
    std::size_t guard = 64;
    std::size_t W     = prec_bits + guard;
    std::uint64_t p2  = p * p;
    double bits_per_term = 2.0 * std::log2(static_cast<double>(p));
    std::size_t N = static_cast<std::size_t>(static_cast<double>(W) / bits_per_term) + 2;
    BSResult r = atan_bs(p2, 0, N);
    BigUint one_shifted = BigUint(1) << static_cast<std::uint64_t>(W);
    BigUint base = one_shifted / BigUint(p);
    bool neg = r.T.is_negative();
    BigUint abs_T = r.T.magnitude();
    BigUint scaled = base * abs_T;
    BigUint result = scaled / r.Q;
    std::int64_t result_exp = -static_cast<std::int64_t>(W);
    return BigFloat::from_parts(neg, result, result_exp, prec_bits);
}

inline BigFloat atan_taylor_bf(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero()) return BigFloat::zero();
    const std::size_t prec  = BigFloat::effective_precision_bits(x);
    const std::size_t W     = prec + 64;
    BigFloat val = x;
    bool neg = val.is_negative();
    if (neg) val.negate();
    val.set_precision_bits(W);
    const int target = optimal_target_exp(W);  
    int reductions = 0;
    BigFloat one_w(1); one_w.set_precision_bits(W);

    while (!val.is_zero() && val.exponent_base2() > static_cast<std::int64_t>(target)) {
        BigFloat x2 = val; 
        x2 *= val; 
        x2 += one_w;
        BigFloat sq = sqrt(x2); 
        sq += one_w;
        val /= sq;
        ++reductions;
        if (reductions > static_cast<int>(W) + 128) break;
    }

    BigUint X = to_fixed_point(val, W);
    if (X.is_zero()) return BigFloat::zero();
    const std::uint64_t W64    = static_cast<std::uint64_t>(W);
    const std::uint64_t shift2 = 2 * W64;
    BigUint X2 = X * X;
    BigUint term = X;
    BigUint pos_sum = X, neg_sum(0);
    bool next_neg = true;

    for (std::uint64_t k = 3; ; k += 2) {
        term = (term * X2) >> shift2;
        if (term.is_zero()) break;
        BigUint contrib = term / k;
        if (contrib.is_zero()) break;
        if (next_neg) neg_sum += contrib; else pos_sum += contrib;
        next_neg = !next_neg;
    }

    BigUint sum = pos_sum - neg_sum;
    if (reductions > 0) sum <<= static_cast<std::uint64_t>(reductions);
    return BigFloat::from_parts(neg, sum, -static_cast<std::int64_t>(W), prec);
}

struct SinCosSeriesResult { BigFloat sin_val; BigFloat cos_val; };

inline SinCosSeriesResult sin_cos_series(const BigFloat& x, std::size_t prec) {
    const std::size_t   W        = prec + 64;
    const std::uint64_t W64      = static_cast<std::uint64_t>(W);
    const std::uint64_t shift_2W = 2 * W64;
    int halvings = 0;
    BigFloat xw = x; xw.set_precision_bits(W);

    if (!xw.is_zero()) {
        const int        target = optimal_target_exp(W);
        const std::int64_t eb2  = xw.exponent_base2();

        if (eb2 > static_cast<std::int64_t>(target)) {
            halvings = static_cast<int>(eb2 - target) + 1;
            xw = BigFloat::from_parts(false, xw.raw_significand(), xw.raw_exponent() - halvings, W);
        }
    }

    const BigUint one_fp = BigUint(1) << W64;
    BigUint s_fp, c_fp;

    if (xw.is_zero()) {
        s_fp = BigUint(0);
        c_fp = one_fp;
    } else {
        const BigUint X    = to_fixed_point(xw, W);
        const BigUint X_sq = X * X;
        BigUint s_term = X,      s_pos = X,      s_neg(0);
        BigUint c_term = one_fp, c_pos = one_fp, c_neg(0);
        bool s_next_neg = true;  
        bool c_next_neg = true;  

        for (std::uint64_t k = 1; ; ++k) {
            s_term  = (s_term * X_sq) >> shift_2W;
            s_term /= (2*k) * (2*k + 1);
            c_term  = (c_term * X_sq) >> shift_2W;
            c_term /= (2*k - 1) * (2*k);
            if (s_term.is_zero() && c_term.is_zero()) break;
            if (s_next_neg) s_neg += s_term; else s_pos += s_term;
            if (c_next_neg) c_neg += c_term; else c_pos += c_term;
            s_next_neg = !s_next_neg;
            c_next_neg = !c_next_neg;
        }

        s_fp = s_pos - s_neg;
        c_fp = c_pos - c_neg;
    }

    for (int i = 0; i < halvings; ++i) {
        BigUint new_s = (s_fp * c_fp) >> (W64 - 1);
        BigUint cc    = (c_fp * c_fp) >> W64;
        BigUint ss    = (s_fp * s_fp) >> W64;
        BigUint new_c = cc - ss;
        s_fp = std::move(new_s);
        c_fp = std::move(new_c);
    }

    const std::int64_t result_exp = -static_cast<std::int64_t>(W);

    return {
        BigFloat::from_parts(false, s_fp, result_exp, prec),
        BigFloat::from_parts(false, c_fp, result_exp, prec)
    };
}

inline std::uint64_t exp_term_count(std::size_t W, std::size_t K) {
    std::uint64_t N = 16;
    for (;;) {
        double lg2_Nfact = 0;
        if (N > 1) {
            double dN = static_cast<double>(N);
            lg2_Nfact = dN * std::log2(dN / 2.718281828) + 0.5 * std::log2(6.2831853 * dN);
        }
        double lhs = static_cast<double>(N) * static_cast<double>(K > 0 ? K - 1 : 0) + lg2_Nfact;
        if (lhs > static_cast<double>(W) + 64.0) break;   
        N += (N < 64) ? 8 : N / 4;
        if (N > 1000000) break; 
    }
    return N;
}

} // namespace detail
} // namespace math

namespace constants {

inline BigFloat compute_pi(std::size_t prec_input, bool input_is_decimal = true) {
    std::size_t prec_bits = input_is_decimal ? BigFloat::decimal_digits_to_bits(prec_input) : prec_input;
    std::size_t work_prec = prec_bits + 64;
    BigFloat a1, a2;
    if (prec_bits > 256) {
        a1 = math::detail::atan_reciprocal_bs(5,   work_prec);
        a2 = math::detail::atan_reciprocal_bs(239, work_prec);
    } else {
        a1 = math::detail::atan_reciprocal(5,   work_prec);
        a2 = math::detail::atan_reciprocal(239, work_prec);
    }
    BigFloat sixteen_a1 = BigFloat::from_parts(false, a1.raw_significand(), a1.raw_exponent() + 4, work_prec);
    BigFloat four_a2    = BigFloat::from_parts(false, a2.raw_significand(), a2.raw_exponent() + 2, work_prec);
    BigFloat result = sixteen_a1; result -= four_a2;
    result.set_precision_bits(prec_bits);
    return result;
}

inline BigFloat pi(std::size_t prec_input = 0, bool input_is_decimal = true) {
    std::size_t prec_bits = prec_input;
    if (input_is_decimal) prec_bits = BigFloat::decimal_digits_to_bits(prec_bits);
    if (prec_bits == 0) prec_bits = BigFloatContext::is_active() ? BigFloatContext::precision() : 256;
    thread_local std::size_t cached_prec = 0;
    thread_local BigFloat    cached_pi;
    if (prec_bits <= cached_prec) { BigFloat r = cached_pi; r.set_precision_bits(prec_bits); return r; }
    std::size_t compute_prec = prec_bits + 64;
    cached_pi   = compute_pi(compute_prec, false);
    cached_prec = compute_prec;
    BigFloat r = cached_pi; r.set_precision_bits(prec_bits); return r;
}

} // namespace constants

namespace math {

inline BigFloat atan(const BigFloat& x) {
    if (x.is_zero()) return BigFloat::zero();
    if (x.is_nan() || x.is_undefined()) return x;
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigFloat recip_2(2);
    recip_2.set_precision_bits(prec);
    recip_2.reciprocal_inplace();

    if (x.is_infinite()) {
        BigFloat result = constants::pi(prec, false); result *= recip_2;
        if (x.is_negative()) result.negate();
        return result;
    }

    bool negative = x.is_negative();
    BigFloat abs_x = abs(x);
    BigFloat one(1); one.set_precision_bits(prec);

    if (abs_x > one) {
        BigFloat pi_half = constants::pi(prec, false); 
        pi_half.set_precision_bits(prec);
        pi_half *= recip_2;
        BigFloat recip = abs_x;
        recip.set_precision_bits(prec);
        recip.reciprocal_inplace();
        BigFloat result = pi_half; 
        result.set_precision_bits(prec);
        result -= detail::atan_taylor_bf(recip);
        if (negative) result.negate();
        return result;
    }

    BigFloat result = detail::atan_taylor_bf(abs_x);
    if (negative) result.negate();
    return result;
}

inline BigFloat atan(const BigInt&      x) { return atan(BigFloat(x)); }
inline BigFloat atan(const BigUint&     x) { return atan(BigFloat(x)); }
inline BigFloat atan(const BigRational& x) { return atan(static_cast<BigFloat>(x)); }

inline BigFloat atan2(const BigFloat& y, const BigFloat& x) {
    if (y.is_nan() || x.is_nan())             return BigFloat::nan();
    if (y.is_undefined() || x.is_undefined()) return BigFloat::undefined();

    const std::size_t prec = BigFloat::effective_precision_bits(y, x);
    const std::size_t work = prec + 32;         
    BigFloat recip_2(2);
    recip_2.set_precision_bits(work);
    recip_2.reciprocal_inplace();
    BigFloat pi_val  = constants::pi(work, /*input_is_decimal=*/false);
    pi_val.set_precision_bits(work);
    BigFloat pi_half = pi_val; 
    pi_half.set_precision_bits(work);
    pi_half *= recip_2;
    BigFloat pi_qtr  = pi_half; 
    pi_qtr.set_precision_bits(work);
    pi_qtr  *= recip_2;

    if (x.is_infinite() && y.is_infinite()) {
        const bool xn = x.is_negative();
        const bool yn = y.is_negative();
        if (!xn) { return yn ? -pi_qtr : pi_qtr; }
        BigFloat three_pq = pi_qtr; 
        three_pq.set_precision_bits(work);
        three_pq *= BigFloat(3);
        return yn ? -three_pq : three_pq;
    }

    if (x.is_infinite()) {
        if (!x.is_negative()) return BigFloat::zero();    
        BigFloat p = pi_val;
        if (y.is_negative()) p.negate();                   
        p.set_precision_bits(prec);
        return p;
    }

    if (y.is_infinite()) {
        if (y.is_negative()) pi_half.negate();
        pi_half.set_precision_bits(prec);
        return pi_half;
    }

    if (x.is_zero()) {
        if (y.is_zero()) return BigFloat::nan();           
        if (y.is_negative()) pi_half.negate();
        pi_half.set_precision_bits(prec);
        return pi_half;
    }

    if (y.is_zero()) {
        if (x.is_negative()) {
            BigFloat p = pi_val;
            if (y.is_negative()) p.negate();
            p.set_precision_bits(prec);
            return p;
        }
        return y;  
    }

    const BigFloat abs_y = abs(y);
    const BigFloat abs_x = abs(x);
    BigFloat alpha;

    if (abs_y <= abs_x) {
        BigFloat ratio = abs_y; 
        ratio.set_precision_bits(work);
        ratio /= abs_x;
        alpha = atan(ratio);
    } else {
        BigFloat ratio = abs_x; 
        ratio.set_precision_bits(work);
        ratio /= abs_y;
        alpha = pi_half; alpha -= atan(ratio);
    }

    BigFloat result;

    if (!x.is_negative()) {
        result = alpha;
    } else {
        result = pi_val;
        result -= alpha;                       
    }

    if (y.is_negative()) result.negate();
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat atan2(const BigRational& y, const BigFloat&    x) { return atan2(static_cast<BigFloat>(y), x); }
inline BigFloat atan2(const BigFloat&    y, const BigRational& x) { return atan2(y, static_cast<BigFloat>(x)); }
inline BigFloat atan2(const BigRational& y, const BigRational& x) { return atan2(static_cast<BigFloat>(y), static_cast<BigFloat>(x)); }
inline BigFloat atan2(const BigRational& yx)                      { return atan2(yx.numerator(), yx.denominator()); }

inline BigFloat sin(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    if (x.is_infinite())                return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    val.set_precision_bits(prec);
    auto r = detail::sin_cos_series(val, prec);
    if (neg) r.sin_val.negate();
    return r.sin_val;
}

inline BigFloat sin(const BigInt&      x) { return sin(BigFloat(x)); }
inline BigFloat sin(const BigUint&     x) { return sin(BigFloat(x)); }
inline BigFloat sin(const BigRational& x) { return sin(static_cast<BigFloat>(x)); }

inline BigFloat sinc(const BigFloat& x) { return sin(x) / x; }
inline BigFloat sinc(const BigInt&      x) { return sinc(BigFloat(x)); }
inline BigFloat sinc(const BigUint&     x) { return sinc(BigFloat(x)); }
inline BigFloat sinc(const BigRational& x) { return sinc(static_cast<BigFloat>(x)); }

inline BigFloat cos(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat(1);
    if (x.is_infinite())                return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigFloat val = x.is_negative() ? -x : x;
    val.set_precision_bits(prec);
    return detail::sin_cos_series(val, prec).cos_val;
}

inline BigFloat cos(const BigInt&      x) { return cos(BigFloat(x)); }
inline BigFloat cos(const BigUint&     x) { return cos(BigFloat(x)); }
inline BigFloat cos(const BigRational& x) { return cos(static_cast<BigFloat>(x)); }

inline BigFloat tan(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    if (x.is_infinite())                return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    val.set_precision_bits(prec);
    auto r = detail::sin_cos_series(val, prec);
    BigFloat result = r.sin_val; result.set_precision_bits(prec);
    result /= r.cos_val;
    if (neg) result.negate();
    return result;
}

inline BigFloat tan(const BigInt&      x) { return tan(BigFloat(x)); }
inline BigFloat tan(const BigUint&     x) { return tan(BigFloat(x)); }
inline BigFloat tan(const BigRational& x) { return tan(static_cast<BigFloat>(x)); }

inline BigFloat csc(const BigFloat& x) { return sin(x).reciprocal(); }
inline BigFloat csc(const BigInt&      x) { return csc(BigFloat(x)); }
inline BigFloat csc(const BigUint&     x) { return csc(BigFloat(x)); }
inline BigFloat csc(const BigRational& x) { return csc(static_cast<BigFloat>(x)); }

inline BigFloat sec(const BigFloat& x) { return cos(x).reciprocal(); }
inline BigFloat sec(const BigInt&      x) { return sec(BigFloat(x)); }
inline BigFloat sec(const BigUint&     x) { return sec(BigFloat(x)); }
inline BigFloat sec(const BigRational& x) { return sec(static_cast<BigFloat>(x)); }

inline BigFloat cot(const BigFloat& x) { return tan(x).reciprocal(); }
inline BigFloat cot(const BigInt&      x) { return cot(BigFloat(x)); }
inline BigFloat cot(const BigUint&     x) { return cot(BigFloat(x)); }
inline BigFloat cot(const BigRational& x) { return cot(static_cast<BigFloat>(x)); }

inline BigFloat asin(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero()) return BigFloat::zero();
    if (x.is_infinite()) return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigFloat one(1); one.set_precision_bits(prec);
    BigFloat ax = abs(x); ax.set_precision_bits(prec);
    if (ax > one) return BigFloat::nan();

    if (ax == one) {
        BigFloat recip_2(2); 
        recip_2.set_precision_bits(prec);
        recip_2.reciprocal_inplace();
        BigFloat result = constants::pi(prec, false);
        result.set_precision_bits(prec);
        result *= recip_2;
        if (x.is_negative()) result.negate();
        return result;
    }

    BigFloat x2 = x; 
    x2 *= x;
    BigFloat one_minus = one; 
    one_minus -= x2;
    BigFloat s = sqrt(one_minus);
    return atan2(x, s);
}

inline BigFloat asin(const BigInt&      x) { return asin(BigFloat(x)); }
inline BigFloat asin(const BigUint&     x) { return asin(BigFloat(x)); }
inline BigFloat asin(const BigRational& x) { return asin(static_cast<BigFloat>(x)); }

inline BigFloat acos(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_infinite()) return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigFloat one(1); one.set_precision_bits(prec);
    BigFloat ax = abs(x);
    ax.set_precision_bits(prec);
    if (ax > one) return BigFloat::nan();
    if (ax == one && !x.is_negative()) return BigFloat::zero();
    if (ax == one && x.is_negative()) return constants::pi(prec, false);
    
    if (x.is_zero()) {
        BigFloat recip_2(2); 
        recip_2.set_precision_bits(prec);
        recip_2.reciprocal_inplace();
        BigFloat result = constants::pi(prec, false);
        result.set_precision_bits(prec);
        result *= recip_2;
        return result;
    }

    BigFloat x2 = x; 
    x2.set_precision_bits(prec);
    x2 *= x;
    BigFloat one_minus = one; 
    one_minus.set_precision_bits(prec);
    one_minus -= x2;
    BigFloat s = sqrt(one_minus);
    s.set_precision_bits(prec);
    return atan2(s, x);
}

inline BigFloat acos(const BigInt&      x) { return acos(BigFloat(x)); }
inline BigFloat acos(const BigUint&     x) { return acos(BigFloat(x)); }
inline BigFloat acos(const BigRational& x) { return acos(static_cast<BigFloat>(x)); }

inline BigFloat acot(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    std::size_t prec = BigFloat::effective_precision_bits(x);
    if (x.is_infinite()) return BigFloat::zero();

    if (x.is_zero()) {
        BigFloat recip_2(2); 
        recip_2.set_precision_bits(prec);
        recip_2.reciprocal_inplace();
        BigFloat result = constants::pi(prec, false);
        result.set_precision_bits(prec);
        result *= recip_2;
        return result;
    }

    BigFloat one(1); 
    one.set_precision_bits(prec);
    return atan2(one, x);
}

inline BigFloat acot(const BigInt&      x) { return acot(BigFloat(x)); }
inline BigFloat acot(const BigUint&     x) { return acot(BigFloat(x)); }
inline BigFloat acot(const BigRational& x) { return acot(static_cast<BigFloat>(x)); }

inline BigFloat asec(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    std::size_t prec = BigFloat::effective_precision_bits(x);

    if (x.is_infinite()) {
        BigFloat recip_2(2); 
        recip_2.set_precision_bits(prec);
        recip_2.reciprocal_inplace();
        BigFloat result = constants::pi(prec, false);
        result.set_precision_bits(prec);
        result *= recip_2;
        return result;
    }

    if (x.is_zero()) return BigFloat::nan();
    BigFloat one(1); one.set_precision_bits(prec);
    BigFloat ax = abs(x); ax.set_precision_bits(prec);
    if (ax < one) return BigFloat::nan();
    return acos(x.reciprocal());
}

inline BigFloat asec(const BigInt&      x) { return asec(BigFloat(x)); }
inline BigFloat asec(const BigUint&     x) { return asec(BigFloat(x)); }
inline BigFloat asec(const BigRational& x) { return asec(static_cast<BigFloat>(x)); }

inline BigFloat acsc(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_infinite()) return BigFloat::zero();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    if (x.is_zero()) return BigFloat::nan();
    BigFloat one(1); one.set_precision_bits(prec);
    BigFloat ax = abs(x);
    ax.set_precision_bits(prec);
    if (ax < one) return BigFloat::nan();
    return asin(x.reciprocal());
}

inline BigFloat acsc(const BigInt&      x) { return acsc(BigFloat(x)); }
inline BigFloat acsc(const BigUint&     x) { return acsc(BigFloat(x)); }
inline BigFloat acsc(const BigRational& x) { return acsc(static_cast<BigFloat>(x)); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_TRIG_HPP