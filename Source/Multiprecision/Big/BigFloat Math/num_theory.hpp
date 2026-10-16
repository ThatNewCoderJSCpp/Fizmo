#ifndef FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP
#define FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP

#include "abs_min_max.hpp"

namespace fizmo {
namespace math {

BigFloat fmod(const BigFloat& x, const BigFloat& y);

BigUInt gcd(const BigUInt& a, const BigUInt& b);

BigUInt lcm(const BigUInt& a, const BigUInt& b);

BigFloat trunc(const BigFloat& x);

BigFloat round(const BigFloat& x);

BigFloat floor(const BigFloat& x);

BigFloat ceiling(const BigFloat& x);

inline BigFloat mod_floor(const BigFloat& x, const BigFloat& y) {
    BigFloat r = fmod(x, y);
    if (!r.is_finite() || r.is_zero() || !y.is_finite()) return r;
    if (r.signbit() != y.signbit()) r = r + y;
    return r;
}

inline BigFloat mod_euclid(const BigFloat& x, const BigFloat& y) {
    BigFloat r = fmod(x, y);
    if (!r.is_finite() || r.is_zero() || !y.is_finite()) return r;
    if (r.signbit()) r = r + y.abs();
    return r;
}

inline BigFloat remainder(const BigFloat& x, const BigFloat& y) {
    BigFloat r = fmod(x, y);
    if (!r.is_finite() || r.is_zero() || !y.is_finite()) return r;
    const BigFloat ay = y.abs();
    const BigFloat twice = r.abs().scaled2(1);
    bool shift = ay < twice;

    if (!shift && twice == ay) {
        const BigFloatContext ctx = BigFloatContext::current();
        const std::size_t guard = ctx.precision < 64 ? 64 : ctx.precision;
        ScopedContext extended(ctx.extended(guard));
        shift = ((x - r) / y).get_integer_part().is_odd();
    }

    if (shift) r = r.signbit() ? r + ay : r - ay;
    return r;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_NUM_THEORY_HPP