#ifndef FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP
#define FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP

#include "../big_float.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

 BigUInt isqrt(const BigUInt& v);

 BigFloat sqrt_finite(const BigFloat& x, const BigFloatContext& ctx);

 BigUInt icbrt(const BigUInt& v);

 BigFloat cbrt_finite(const BigFloat& x, const BigFloatContext& ctx);

} // namespace detail

 BigFloat sqrt(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat sqrt(const BigFloat& x) { return sqrt(x, BigFloatContext::current()); }

inline BigFloat sqrt(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return sqrt(BigFloat(x), ctx);
}

inline BigFloat sqrt(const BigUInt& x) { return sqrt(x, BigFloatContext::current()); }

 BigFloat sqrt(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat sqrt(const BigInt& x) { return sqrt(x, BigFloatContext::current()); }

 BigFloat cbrt(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat cbrt(const BigFloat& x) { return cbrt(x, BigFloatContext::current()); }

inline BigFloat cbrt(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return cbrt(BigFloat(x), ctx);
}

inline BigFloat cbrt(const BigUInt& x) { return cbrt(x, BigFloatContext::current()); }

 BigFloat cbrt(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat cbrt(const BigInt& x) { return cbrt(x, BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_SQRT_CBRT_HPP