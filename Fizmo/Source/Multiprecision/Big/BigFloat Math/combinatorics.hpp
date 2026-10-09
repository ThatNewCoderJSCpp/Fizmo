#ifndef FIZMO_MULTIPRECISION_BIG_COMBINATORICS_HPP
#define FIZMO_MULTIPRECISION_BIG_COMBINATORICS_HPP

#include "gamma.hpp"
#include "hurwitz_lerch.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace cbdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;
using ztdetail::zt_val;
using ztdetail::zt_bitlen;

static const std::uint64_t cb_leaf     = 16;
static const std::uint64_t cb_loop_cap = 1ull << 16;                                 

inline BFC           cb_ctx(std::size_t p)      { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }
inline std::uint64_t cb_uabs(std::int64_t n)    { return (n < 0) ? static_cast<std::uint64_t>(-(n + 1)) + 1u : static_cast<std::uint64_t>(n); }
inline bool          cb_bad(const BF& x)        { return x.is_nan() || x.is_undefined(); }
inline BF            cb_nan_of(const BF& x)     { return x.is_nan() ? BF::nan() : BF::undefined(); }
inline bool          cb_nonpos(const BF& x)     { return x.is_zero() || x.signbit(); }

 bool cb_fits_u64(const BigInt& v, std::uint64_t& out);

 bool cb_int_ok(const BigInt& lo, const BigInt& hi, std::uint64_t n);

 BigInt cb_ap(const BigInt& a, const BigInt& d, std::uint64_t lo, std::uint64_t hi);

 BigInt cb_rise_int(const BigInt& a, std::uint64_t n);

 BF cb_gamma_ratio(const std::vector<BF>& num, const std::vector<BF>& den, bool neg, const BFC& ctx);

 BF cb_gamma_quot(const BF& zt, const BF& zb, const BFC& ctx);

 BF cb_rise_float(const BF& x, std::uint64_t n, bool inv, bool binom, bool neg, const BFC& ctx);

 BF cb_rising_core(BF x, std::uint64_t n, bool inv, bool binom, const BFC& ctx);

} // namespace cbdetail

 BigInt rising_pochhammer(const BigInt& x, std::int64_t n);

 BigInt falling_pochhammer(const BigInt& x, std::int64_t n);

inline BigInt permutation(const BigInt& n, std::int64_t k) { return falling_pochhammer(n, k); }

 BigInt combination(const BigInt& n, std::int64_t k);

inline BigInt rising_pochhammer(std::int64_t x, std::int64_t n)   { return rising_pochhammer(BigInt(x), n); }
inline BigInt falling_pochhammer(std::int64_t x, std::int64_t n)  { return falling_pochhammer(BigInt(x), n); }
inline BigInt permutation(std::int64_t n, std::int64_t k)         { return permutation(BigInt(n), k); }
inline BigInt combination(std::int64_t n, std::int64_t k)         { return combination(BigInt(n), k); }

inline BigInt rising_pochhammer(const BigUInt& x, std::int64_t n) { return rising_pochhammer(BigInt(x), n); }
inline BigInt falling_pochhammer(const BigUInt& x, std::int64_t n){ return falling_pochhammer(BigInt(x), n); }
inline BigInt permutation(const BigUInt& n, std::int64_t k)       { return permutation(BigInt(n), k); }
inline BigInt combination(const BigUInt& n, std::int64_t k)       { return combination(BigInt(n), k); }

 BigFloat rising_pochhammer(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat falling_pochhammer(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current());

inline BigFloat permutation(const BigFloat& x, std::int64_t k, const BigFloatContext& ctx = BigFloatContext::current()) {
    return falling_pochhammer(x, k, ctx);
}

 BigFloat combination(const BigFloat& x, std::int64_t k, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat rising_pochhammer(const BigFloat& x, const BigFloat& a, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat falling_pochhammer(const BigFloat& x, const BigFloat& a, const BigFloatContext& ctx = BigFloatContext::current());

inline BigFloat permutation(const BigFloat& x, const BigFloat& k, const BigFloatContext& ctx = BigFloatContext::current()) {
    return falling_pochhammer(x, k, ctx);
}

 BigFloat combination(const BigFloat& x, const BigFloat& k, const BigFloatContext& ctx = BigFloatContext::current());

#define FIZMO_MP_CB_FWD(FN)                                                                                                     \
    template <typename T, typename U,                                                                                           \
              typename std::enable_if<std::is_arithmetic<T>::value && std::is_arithmetic<U>::value &&                           \
                                      (std::is_floating_point<T>::value || std::is_floating_point<U>::value), int>::type = 0>   \
    inline BigFloat FN(T x, U n, const BigFloatContext& c = BigFloatContext::current()) { return FN(BigFloat(x), BigFloat(n), c); } \
                                                                                                                                \
    template <typename U, typename std::enable_if<std::is_floating_point<U>::value, int>::type = 0>                             \
    inline BigFloat FN(const BigFloat& x, U n, const BigFloatContext& c = BigFloatContext::current()) { return FN(x, BigFloat(n), c); }

FIZMO_MP_CB_FWD(rising_pochhammer)
FIZMO_MP_CB_FWD(falling_pochhammer)
FIZMO_MP_CB_FWD(permutation)
FIZMO_MP_CB_FWD(combination)

#undef FIZMO_MP_CB_FWD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMBINATORICS_HPP