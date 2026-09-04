#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_ABS_ROOT_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_ABS_ROOT_HPP

#include "../big_float.hpp"
#include "../big_rational.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

// ── abs ──────────────────────────────────────────────────────────────────────

inline BigFloat    abs(const BigFloat& x)    noexcept { if (x.is_nan() || x.is_undefined()) return x; return x.is_negative() ? -x : x; }
inline BigInt      abs(const BigInt& x)               { if (x.is_error()) return x; return x.is_negative() ? -x : x; }
inline BigUint     abs(const BigUint& x)              { return x; }

// Rational abs stays rational — no resolution needed, just flip the numerator sign.
inline BigRational abs(const BigRational& x) {
    if (x.is_nan() || x.is_undefined()) return x;
    BigRational r = x;
    r.abs_in_place();
    return r;
}

namespace detail {

inline BigUint isqrt(const BigUint& n) {
    if (n.is_zero()) return BigUint(0);
    const std::size_t bit_len = n.get_exponent_base2() + 1;

    if (bit_len <= 64) {
        BigUint x = BigUint(1) << static_cast<std::uint64_t>((bit_len + 1) / 2);
        for (;;) {
            BigUint q  = n / x;
            BigUint x1 = (x + q) >> static_cast<std::uint64_t>(1);
            if (x1 >= x) break;
            x = x1;
        }
        return x;
    }

    const std::uint64_t k = static_cast<std::uint64_t>(bit_len / 4);
    BigUint x = isqrt(n >> (2 * k)) << k;  
    if (x.is_zero()) x = BigUint(1);
    x = (x + n / x) >> static_cast<std::uint64_t>(1);
    if (x * x > n) x -= 1u;
    return x;
}

inline BigUint icbrt(const BigUint& n) {
    if (n.is_zero()) return BigUint(0);
    const std::size_t bit_len = n.get_exponent_base2() + 1;

    if (bit_len <= 64) {
        BigUint x = BigUint(1) << static_cast<std::uint64_t>((bit_len + 2) / 3);

        for (;;) {
            BigUint x2 = x * x;
            BigUint q  = n / x2;
            BigUint x1 = (x + x + q) / 3u;
            if (x1 >= x) break;
            x = x1;
        }
        
        while (x * x * x > n) x -= 1u;
        return x;
    }

    const std::uint64_t k = static_cast<std::uint64_t>(bit_len / 6);
    BigUint x = icbrt(n >> (3 * k)) << k;  
    if (x.is_zero()) x = BigUint(1);
    const BigUint x2 = x * x;
    x = (x + x + n / x2) / 3u;
    if (x * x * x > n) x -= 1u;
    return x;
}

inline std::uint64_t mod_positive(std::int64_t v, std::uint64_t d) {
    std::int64_t r = v % static_cast<std::int64_t>(d);
    if (r < 0) r += static_cast<std::int64_t>(d);
    return static_cast<std::uint64_t>(r);
}

} // namespace detail

inline BigFloat sqrt(const BigFloat& x) {
    if (x.is_nan())                      return BigFloat::nan();
    if (x.is_undefined())                return BigFloat::undefined();
    if (x.is_negative() && !x.is_zero()) return BigFloat::nan();
    if (x.is_zero())                     return BigFloat::zero();
    if (x.is_positive_infinity())        return BigFloat::positive_infinity();
    if (x.is_negative_infinity())        return BigFloat::nan();
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigUint sig       = x.raw_significand();
    std::int64_t exp  = x.raw_exponent();
    std::uint64_t adj = detail::mod_positive(exp, 2);
    std::uint64_t scale_shift = 2 * static_cast<std::uint64_t>(prec) + adj;
    BigUint scaled = sig << scale_shift;
    BigUint root   = detail::isqrt(scaled);
    std::int64_t adjusted   = exp - static_cast<std::int64_t>(adj);
    std::int64_t half_exp   = adjusted / 2;
    std::int64_t result_exp = half_exp - static_cast<std::int64_t>(prec);
    return BigFloat::from_parts(false, root, result_exp, prec);
}

inline BigFloat sqrt(const BigInt&      x) { return sqrt(BigFloat(x)); }
inline BigFloat sqrt(const BigUint&     x) { return sqrt(BigFloat(x)); }
inline BigFloat sqrt(const BigRational& x) { return sqrt(static_cast<BigFloat>(x)); }

inline BigFloat cbrt(const BigFloat& x) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return BigFloat::zero();
    if (x.is_infinite())  return x.is_negative() ? BigFloat::negative_infinity() : BigFloat::positive_infinity();
    bool neg = x.is_negative();
    BigFloat val = neg ? -x : x;
    std::size_t prec = BigFloat::effective_precision_bits(x);
    BigUint sig       = val.raw_significand();
    std::int64_t exp  = val.raw_exponent();
    std::uint64_t adj = detail::mod_positive(exp, 3);
    std::uint64_t scale_shift = 3 * static_cast<std::uint64_t>(prec) + adj;
    BigUint scaled = sig << scale_shift;
    BigUint root   = detail::icbrt(scaled);
    std::int64_t adjusted   = exp - static_cast<std::int64_t>(adj);
    std::int64_t third_exp  = adjusted / 3;
    std::int64_t result_exp = third_exp - static_cast<std::int64_t>(prec);
    return BigFloat::from_parts(neg, root, result_exp, prec);
}

inline BigFloat cbrt(const BigInt&      x) { return cbrt(BigFloat(x)); }
inline BigFloat cbrt(const BigUint&     x) { return cbrt(BigFloat(x)); }
inline BigFloat cbrt(const BigRational& x) { return cbrt(static_cast<BigFloat>(x)); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_ABS_ROOT_HPP