#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_HTRIG_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_HTRIG_HPP

#include "log_pow.hpp"  
#include <cmath>         

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

inline int htrig_target_exp(std::size_t W) noexcept {
    int t = static_cast<int>(std::sqrt(static_cast<double>(W) / 3.0));
    if (t < 4)  t = 4;
    if (t > 64) t = 64;
    return -t;
}

struct SinhCoshResult { BigFloat sinh_val; BigFloat cosh_val; };

inline SinhCoshResult sinh_cosh_series(const BigFloat& x, std::size_t prec) {
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
        BigUint s_term = X,      s_sum = X;
        BigUint c_term = one_fp, c_sum = one_fp;

        for (std::uint64_t k = 1; ; ++k) {
            s_term  = (s_term * X_sq) >> shift_2W;
            s_term /= 2*k * (2*k + 1);                        
            c_term  = (c_term * X_sq) >> shift_2W;
            c_term /= (2*k - 1) * (2*k);                      
            if (s_term.is_zero() && c_term.is_zero()) break;
            s_sum += s_term;
            c_sum += c_term;
        }

        s_fp = std::move(s_sum);
        c_fp = std::move(c_sum);
    }

    for (int i = 0; i < halvings; ++i) {
        BigUint new_s = (s_fp * c_fp) >> (W64 - 1);
        BigUint new_c = (c_fp * c_fp) >> W64;
        new_c += (s_fp * s_fp) >> W64;
        s_fp = std::move(new_s);
        c_fp = std::move(new_c);
    }

    const std::int64_t result_exp = -static_cast<std::int64_t>(W);

    return {
        BigFloat::from_parts(false, s_fp, result_exp, prec),
        BigFloat::from_parts(false, c_fp, result_exp, prec)
    };
}

} // namespace detail

inline BigFloat sinh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    if (x.is_negative_infinity())       return BigFloat::negative_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    auto r = detail::sinh_cosh_series(val, prec);
    if (neg) r.sinh_val.negate();
    return r.sinh_val;
}

inline BigFloat sinh(const BigInt&      x) { return sinh(BigFloat(x)); }
inline BigFloat sinh(const BigUint&     x) { return sinh(BigFloat(x)); }
inline BigFloat sinh(const BigRational& x) { return sinh(static_cast<BigFloat>(x)); }

inline BigFloat cosh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat(1);
    if (x.is_infinite())                return BigFloat::positive_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigFloat val = x.is_negative() ? -x : x;
    return detail::sinh_cosh_series(val, prec).cosh_val;
}

inline BigFloat cosh(const BigInt&      x) { return cosh(BigFloat(x)); }
inline BigFloat cosh(const BigUint&     x) { return cosh(BigFloat(x)); }
inline BigFloat cosh(const BigRational& x) { return cosh(static_cast<BigFloat>(x)); }

inline BigFloat tanh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    if (x.is_positive_infinity())       return BigFloat(1);
    if (x.is_negative_infinity())       return BigFloat(-1);
    std::size_t prec = BigFloat::effective_precision_bits(x);
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    auto r = detail::sinh_cosh_series(val, prec);
    BigFloat result = r.sinh_val; result.set_precision_bits(prec);
    result /= r.cosh_val;
    if (neg) result.negate();
    return result;
}

inline BigFloat tanh(const BigInt&      x) { return tanh(BigFloat(x)); }
inline BigFloat tanh(const BigUint&     x) { return tanh(BigFloat(x)); }
inline BigFloat tanh(const BigRational& x) { return tanh(static_cast<BigFloat>(x)); }

inline BigFloat csch(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::positive_infinity();  
    if (x.is_infinite())                return BigFloat::zero();
    BigFloat result(1); result.set_precision_bits(BigFloat::effective_precision_bits(x) + 64);
    result /= sinh(x);
    return result;
}

inline BigFloat csch(const BigInt&      x) { return csch(BigFloat(x)); }
inline BigFloat csch(const BigUint&     x) { return csch(BigFloat(x)); }
inline BigFloat csch(const BigRational& x) { return csch(static_cast<BigFloat>(x)); }

inline BigFloat sech(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat(1);
    if (x.is_infinite())                return BigFloat::zero();
    BigFloat result(1); result.set_precision_bits(BigFloat::effective_precision_bits(x) + 64);
    result /= cosh(x);
    return result;
}

inline BigFloat sech(const BigInt&      x) { return sech(BigFloat(x)); }
inline BigFloat sech(const BigUint&     x) { return sech(BigFloat(x)); }
inline BigFloat sech(const BigRational& x) { return sech(static_cast<BigFloat>(x)); }

inline BigFloat coth(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::positive_infinity();   
    if (x.is_positive_infinity())       return BigFloat(1);
    if (x.is_negative_infinity())       return BigFloat(-1);
    std::size_t prec = BigFloat::effective_precision_bits(x);
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    auto r = detail::sinh_cosh_series(val, prec);
    BigFloat result = r.cosh_val; result.set_precision_bits(prec);
    result /= r.sinh_val;
    if (neg) result.negate();
    return result;
}

inline BigFloat coth(const BigInt&      x) { return coth(BigFloat(x)); }
inline BigFloat coth(const BigUint&     x) { return coth(BigFloat(x)); }
inline BigFloat coth(const BigRational& x) { return coth(static_cast<BigFloat>(x)); }

inline BigFloat asinh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    if (x.is_positive_infinity())       return BigFloat::positive_infinity();
    if (x.is_negative_infinity())       return BigFloat::negative_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    bool neg = x.is_negative();
    BigFloat ax = neg ? -x : x;
    ax.set_precision_bits(W);
    BigFloat one(1); one.set_precision_bits(W);
    BigFloat x2 = ax; x2.set_precision_bits(W);
    x2 *= ax;
    BigFloat sqrtarg = x2; sqrtarg.set_precision_bits(W);
    sqrtarg += one;
    BigFloat inner = ax; inner.set_precision_bits(W);
    inner += math::sqrt(sqrtarg);
    BigFloat result = ln(inner);
    result.set_precision_bits(prec);
    if (neg) result.negate();
    return result;
}

inline BigFloat asinh(const BigInt&      x) { return asinh(BigFloat(x)); }
inline BigFloat asinh(const BigUint&     x) { return asinh(BigFloat(x)); }
inline BigFloat asinh(const BigRational& x) { return asinh(static_cast<BigFloat>(x)); }

inline BigFloat acosh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    BigFloat one(1);
    if (x < one)                  return BigFloat::nan();
    if (x == one)                 return BigFloat::zero();
    if (x.is_positive_infinity()) return BigFloat::positive_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloat xw = x; xw.set_precision_bits(W);
    BigFloat one_w(1); one_w.set_precision_bits(W);
    BigFloat x2 = xw; x2.set_precision_bits(W);
    x2 *= xw;
    BigFloat sqrtarg = x2; sqrtarg.set_precision_bits(W);
    sqrtarg -= one_w;
    BigFloat inner = xw; inner.set_precision_bits(W);
    inner += sqrt(sqrtarg);
    BigFloat result = ln(inner);
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat acosh(const BigInt&      x) { return acosh(BigFloat(x)); }
inline BigFloat acosh(const BigUint&     x) { return acosh(BigFloat(x)); }
inline BigFloat acosh(const BigRational& x) { return acosh(static_cast<BigFloat>(x)); }

inline BigFloat atanh(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::zero();
    BigFloat ax = x.is_negative() ? -x : x;
    BigFloat one(1);
    if (ax > one)  return BigFloat::nan();
    if (ax == one) return x.is_negative() ? BigFloat::negative_infinity() : BigFloat::positive_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloat xw = x; xw.set_precision_bits(W);
    BigFloat one_w(1); one_w.set_precision_bits(W);
    BigFloat two(2); two.set_precision_bits(W);
    BigFloat arg = one_w; arg.set_precision_bits(W);
    BigFloat arg_denom = one_w; arg_denom.set_precision_bits(W);
    arg_denom -= xw;
    arg += xw;
    arg /= arg_denom;
    BigFloat result = ln(arg);
    result /= two;
    result.set_precision_bits(prec);
    return result;
}

inline BigFloat atanh(const BigInt&      x) { return atanh(BigFloat(x)); }
inline BigFloat atanh(const BigUint&     x) { return atanh(BigFloat(x)); }
inline BigFloat atanh(const BigRational& x) { return atanh(static_cast<BigFloat>(x)); }

inline BigFloat acsch(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::positive_infinity();
    if (x.is_infinite())                return BigFloat::zero();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloat recip(1); 
    recip.set_precision_bits(W);
    recip /= x;
    return asinh(recip);
}

inline BigFloat acsch(const BigInt&      x) { return acsch(BigFloat(x)); }
inline BigFloat acsch(const BigUint&     x) { return acsch(BigFloat(x)); }
inline BigFloat acsch(const BigRational& x) { return acsch(static_cast<BigFloat>(x)); }

inline BigFloat asech(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_zero())                    return BigFloat::positive_infinity();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloat recip(1);
    recip.set_precision_bits(W);
    if (x.is_negative() || x > recip) return BigFloat::nan();
    if (x == recip)                   return BigFloat::zero();
    recip /= x;
    return acosh(recip);
}

inline BigFloat asech(const BigInt&      x) { return asech(BigFloat(x)); }
inline BigFloat asech(const BigUint&     x) { return asech(BigFloat(x)); }
inline BigFloat asech(const BigRational& x) { return asech(static_cast<BigFloat>(x)); }

inline BigFloat acoth(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    if (x.is_infinite())                return BigFloat::zero();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    const std::size_t W = prec + 64;
    BigFloat recip(1);
    recip.set_precision_bits(W);
    BigFloat ax = x.is_negative() ? -x : x;
    if (ax < recip)  return BigFloat::nan();
    if (ax == recip) return x.is_negative() ? BigFloat::negative_infinity() : BigFloat::positive_infinity();
    recip /= x;
    return atanh(recip);
}

inline BigFloat acoth(const BigInt&      x) { return acoth(BigFloat(x)); }
inline BigFloat acoth(const BigUint&     x) { return acoth(BigFloat(x)); }
inline BigFloat acoth(const BigRational& x) { return acoth(static_cast<BigFloat>(x)); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_HTRIG_HPP