#ifndef FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP
#define FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP

#include "exp.hpp"
#include "logarithms.hpp"
#include "sqrt_cbrt.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

enum class log_op : std::uint8_t { multiply, divide };

inline bool pw_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 2;
}

inline bool pw_strictly_negative(const BigFloat& v) { return v.is_negative() && !v.is_zero(); }

std::int64_t pw_mul_sat(std::int64_t a, std::int64_t b) noexcept;

bool pw_to_i64(const BigInt& n, std::int64_t& out);

inline BigUInt ipow(const BigUInt& m, std::uint64_t n) {
    BigUInt r = BigUInt::one(), t = m;
    while (n != 0) { if (n & 1u) r = r * t; n >>= 1; if (n != 0) t = t * t; }
    return r;
}

BigUInt iroot(const BigUInt& v, std::uint64_t n);

bool nth_root_exact(const BigFloat& ax, std::uint64_t n, BigFloat& out);

BigFloat recip_rounded(const BigFloat& v, const BigFloatContext& ctx, std::size_t err);

BigFloat pow_int_finite(const BigFloat& ax, std::int64_t n, const BigFloatContext& ctx);

BigFloat exp_log_core(const BigFloat& ax, const BigFloat& y, log_op op, bool neg_result, const BigFloatContext& ctx);

bool pow_dyadic(const BigFloat& ax, const BigFloat& y, const BigFloatContext& ctx, BigFloat& out);

BigFloat nth_root_int_finite(const BigFloat& ax, std::int64_t n, const BigFloatContext& ctx);

BigFloat pw_extreme(const BigFloat& ax, bool y_negative, bool neg_result);

} // namespace detail

BigFloat pow(const BigFloat& x, const BigInt& n, const BigFloatContext& ctx);

BigFloat pow(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx);

BigFloat pow2(const BigFloat& x, const BigFloatContext& ctx);

BigFloat pow10(const BigFloat& x, const BigFloatContext& ctx);

#define FIZMO_MP_EXPBASE_FORWARD(FN)                                                   \
    inline BigFloat FN(const BigFloat& x) { return FN(x, BigFloatContext::current()); } \
    inline BigFloat FN(const BigInt& x, const BigFloatContext& c) {                    \
        if (x.is_nan())       return BigFloat::nan();                                 \
        if (x.is_undefined()) return BigFloat::undefined();                           \
        return FN(BigFloat(x), c);                                                    \
    }                                                                                 \
    inline BigFloat FN(const BigInt& x) { return FN(x, BigFloatContext::current()); }  \
    inline BigFloat FN(const BigUInt& x, const BigFloatContext& c) {                   \
        if (x.is_undefined()) return BigFloat::undefined();                           \
        return FN(BigFloat(x), c);                                                    \
    }                                                                                 \
    inline BigFloat FN(const BigUInt& x) { return FN(x, BigFloatContext::current()); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0> \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(BigInt(x)), c); }  \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0> \
    inline BigFloat FN(T x) { return FN(BigFloat(BigInt(x)), BigFloatContext::current()); }

FIZMO_MP_EXPBASE_FORWARD(pow2)
FIZMO_MP_EXPBASE_FORWARD(pow10)

#undef FIZMO_MP_EXPBASE_FORWARD

BigFloat nth_root(const BigFloat& x, const BigInt& n, const BigFloatContext& ctx);

BigFloat nth_root(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx);

#define FIZMO_MP_POW_FORWARD(FN)                                                                                      \
    inline BigFloat FN(const BigFloat& x, const BigUInt& n, const BigFloatContext& c) { return FN(x, BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigFloat& x, T n, const BigFloatContext& c) { return FN(x, BigInt(n), c); }              \
    inline BigFloat FN(const BigInt& x,  const BigFloat& y, const BigFloatContext& c) { return FN(BigFloat(x), y, c); }      \
    inline BigFloat FN(const BigInt& x,  const BigInt& n,   const BigFloatContext& c) { return FN(BigFloat(x), n, c); }      \
    inline BigFloat FN(const BigInt& x,  const BigUInt& n,  const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigInt& x, T n, const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n), c); }      \
    inline BigFloat FN(const BigUInt& x, const BigFloat& y, const BigFloatContext& c) { return FN(BigFloat(x), y, c); }      \
    inline BigFloat FN(const BigUInt& x, const BigInt& n,   const BigFloatContext& c) { return FN(BigFloat(x), n, c); }      \
    inline BigFloat FN(const BigUInt& x, const BigUInt& n,  const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n, false), c); } \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigUInt& x, T n, const BigFloatContext& c) { return FN(BigFloat(x), BigInt(n), c); }     \
    inline BigFloat FN(const BigFloat& x, const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }         \
    inline BigFloat FN(const BigFloat& x, const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }         \
    inline BigFloat FN(const BigFloat& x, const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }         \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigFloat& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }               \
    inline BigFloat FN(const BigInt& x,  const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigInt& x,  const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigInt& x,  const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }          \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigInt& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }                 \
    inline BigFloat FN(const BigUInt& x, const BigFloat& y) { return FN(x, y, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigUInt& x, const BigInt& n)   { return FN(x, n, BigFloatContext::current()); }          \
    inline BigFloat FN(const BigUInt& x, const BigUInt& n)  { return FN(x, n, BigFloatContext::current()); }          \
    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>                         \
    inline BigFloat FN(const BigUInt& x, T n) { return FN(x, BigInt(n), BigFloatContext::current()); }

FIZMO_MP_POW_FORWARD(pow)
FIZMO_MP_POW_FORWARD(nth_root)

#undef FIZMO_MP_POW_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_POW_NTH_ROOT_HPP