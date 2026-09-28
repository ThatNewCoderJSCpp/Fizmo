#ifndef FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP
#define FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP

#include "../big_float.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline BigUInt isqrt(const BigUInt& v) {
    if (v.is_undefined()) return v;
    const std::size_t L = v.bit_length();
    if (L <= 2) return v.is_zero() ? BigUInt::zero() : BigUInt::one();
    BigUInt x;

    if (L <= 128) {
        x = BigUInt::one();
        x.shift_left_mutable((L + 1) / 2);                
    } else {
        const std::size_t h = L / 4;
        BigUInt top = v;
        top.shift_right_mutable(2 * h);
        x = isqrt(top);
        x.add_small_mutable(1);
        x.shift_left_mutable(h);                           
    }

    for (;;) {
        BigUInt y = v / x;
        y.add_mutable(x);
        y.shift_right_mutable(1);
        if (!(y < x)) return x;
        x = std::move(y);
    }
}

inline BigFloat sqrt_finite(const BigFloat& x, const BigFloatContext& ctx) {
    const std::size_t   p    = ctx.precision;
    const BigUInt&      m    = x.significand();
    const std::size_t   L    = m.bit_length();
    const std::int64_t  e    = x.exponent();
    const std::size_t   need = 2 * (p + 2);
    std::size_t s = (L < need) ? (need - L) : 0;
    if ((e - static_cast<std::int64_t>(s)) % 2 != 0) ++s;
    if (L + s + 1 > BigUInt::max_bits) return BigFloat::undefined();
    BigUInt M = m;
    M.shift_left_mutable(s);
    BigUInt r = isqrt(M);
    const bool inexact = (r * r) < M;
    const std::int64_t re = (e - static_cast<std::int64_t>(s)) / 2;
    r.shift_left_mutable(1);
    if (inexact) r.add_small_mutable(1);
    return BigFloat(std::move(r), false, re - 1).rounded(ctx);
}

inline BigUInt icbrt(const BigUInt& v) {
    if (v.is_undefined()) return v;
    const std::size_t L = v.bit_length();
    if (L <= 3) return v.is_zero() ? BigUInt::zero() : BigUInt::one();
    BigUInt x;

    if (L <= 192) {
        x = BigUInt::one();
        x.shift_left_mutable((L + 2) / 3);                 
    } else {
        const std::size_t h = L / 6;
        BigUInt top = v;
        top.shift_right_mutable(3 * h);
        x = icbrt(top);
        x.add_small_mutable(1);
        x.shift_left_mutable(h);                           
    }

    for (;;) {
        BigUInt y = v / (x * x);
        BigUInt twice = x;
        twice.shift_left_mutable(1);
        y.add_mutable(twice);
        y.div_small_mutable(3);
        if (!(y < x)) return x;
        x = std::move(y);
    }
}

inline BigFloat cbrt_finite(const BigFloat& x, const BigFloatContext& ctx) {
    const std::size_t   p    = ctx.precision;
    const BigUInt&      m    = x.significand();
    const std::size_t   L    = m.bit_length();
    const std::int64_t  e    = x.exponent();
    const std::size_t   need = 3 * (p + 2);
    std::size_t s = (L < need) ? (need - L) : 0;
    std::int64_t rem = (e - static_cast<std::int64_t>(s)) % 3;
    if (rem < 0) rem += 3;
    s += static_cast<std::size_t>(rem);
    if (L + s + 1 > BigUInt::max_bits) return BigFloat::undefined();
    BigUInt M = m;
    M.shift_left_mutable(s);
    BigUInt r = icbrt(M);
    const bool inexact = (r * r * r) < M;
    const std::int64_t re = (e - static_cast<std::int64_t>(s)) / 3;
    r.shift_left_mutable(1);
    if (inexact) r.add_small_mutable(1);
    return BigFloat(std::move(r), x.signbit(), re - 1).rounded(ctx);
}

} // namespace detail

inline BigFloat sqrt(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return x;                       
    if (x.is_negative())  return BigFloat::nan();
    if (x.is_infinite())  return x;
    return detail::sqrt_finite(x, ctx);
}

inline BigFloat sqrt(const BigFloat& x) { return sqrt(x, BigFloatContext::current()); }

inline BigFloat sqrt(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return sqrt(BigFloat(x), ctx);
}

inline BigFloat sqrt(const BigUInt& x) { return sqrt(x, BigFloatContext::current()); }

inline BigFloat sqrt(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return sqrt(BigFloat(x), ctx);
}

inline BigFloat sqrt(const BigInt& x) { return sqrt(x, BigFloatContext::current()); }

inline BigFloat cbrt(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero() || x.is_infinite()) return x;
    return detail::cbrt_finite(x, ctx);
}

inline BigFloat cbrt(const BigFloat& x) { return cbrt(x, BigFloatContext::current()); }

inline BigFloat cbrt(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return cbrt(BigFloat(x), ctx);
}

inline BigFloat cbrt(const BigUInt& x) { return cbrt(x, BigFloatContext::current()); }

inline BigFloat cbrt(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return cbrt(BigFloat(x), ctx);
}

inline BigFloat cbrt(const BigInt& x) { return cbrt(x, BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP