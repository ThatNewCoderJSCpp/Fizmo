#ifndef FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP
#define FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP

#include "abs_min_max.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

inline BigFloat fmod(const BigFloat& x, const BigFloat& y) {
    if (x.is_nan() || y.is_nan()) return BigFloat::nan();
    if (x.is_undefined() || y.is_undefined()) return BigFloat::undefined();
    if (y.is_zero()) return BigFloat::undefined();
    if (x.is_infinite()) return BigFloat::undefined();
    if (x.is_zero() || y.is_infinite()) return x;
    const BigFloatContext ctx = BigFloatContext::current();
    const std::size_t guard = ctx.precision < 64 ? 64 : ctx.precision;
    ScopedContext extended(ctx.extended(guard));
    const BigFloat q = x / y;
    const BigInt qi = q.get_integer_part();
    if (qi.is_nan() || qi.is_undefined()) return BigFloat::undefined();
    const BigFloat tq(qi);
    return x - tq * y;
}

inline BigUInt gcd(const BigUInt& a, const BigUInt& b) {
    BigUInt x = a;
    BigUInt y = b;

    while (!y.is_zero()) {
        BigUInt r = x % y;
        if (r.is_undefined()) return r;
        x = std::move(y);
        y = std::move(r);
    }

    return x;
}

inline BigUInt lcm(const BigUInt& a, const BigUInt& b) {
    if (a.is_undefined() || b.is_undefined()) return BigUInt::undefined();
    if (a.is_zero() || b.is_zero()) return BigUInt::zero();
    const BigUInt g = gcd(a, b);
    if (g.is_undefined()) return g;
    return (a / g) * b;
}

inline BigFloat trunc(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined() || x.is_infinite() || x.is_zero()) return x;
    return BigFloat(x.get_integer_part());
}

inline BigFloat round(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined() || x.is_infinite() || x.is_zero()) return x;
    const BigFloat t = trunc(x);
    const BigFloat fraction = x - t;
    const BigFloat half("0.5");
    if (fraction >= half) return t + BigFloat(1);
    if (fraction <= -half) return t - BigFloat(1);
    return t;
}

inline BigFloat floor(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined() || x.is_infinite() || x.is_zero()) return x;
    const BigFloat t = trunc(x);
    return x < t ? t - BigFloat(1) : t;
}

inline BigFloat ceiling(const BigFloat& x) {
    if (x.is_nan() || x.is_undefined() || x.is_infinite() || x.is_zero()) return x;
    const BigFloat t = trunc(x);
    return x > t ? t + BigFloat(1) : t;
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP