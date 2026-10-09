#ifndef FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP
#define FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP

#include "polynomials.hpp"
#include "polylog.hpp"
#include "inv_htrig.hpp"
#include "riemann_zeta.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace lgdetail {

using pldetail::pl_val;
using hzdetail::hz_xadd;
using hzdetail::hz_xmul;
using hzdetail::hz_l2;
using hzdetail::hz_upow;
using ztdetail::zt_val;

static const std::uint64_t lg_n_cap = 1ull << 28;

 double lg_span(const BigFloat& x);

 bool lg_p_exact(std::uint64_t n, const BigFloat& x, const BigFloatContext& ctx, BigFloat& out);

 pl_val lg_div_int(const pl_val& a, std::uint64_t d, const BigFloatContext& wc);

 pl_val lg_recur(std::uint64_t n, const BigFloat& x, pl_val f0, pl_val f1, const BigFloatContext& wc);

 bool lg_q_zero(std::uint64_t n, const BigFloatContext& ctx, BigFloat& out);

 double lg_xi(const BigFloat& ax);

 zt_val lg_q_hyper(std::uint64_t n, const BigFloat& ax, std::size_t want);

 bool lc_neg_int(std::uint64_t m, const BigFloat& x, const BigFloatContext& ctx, BigFloat& out);

} // namespace lgdetail

 BigFloat legendre_p(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

 BigFloat legendre_q(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

 BigFloat legendre_chi(const BigFloat& x, const BigFloat& s, const BigFloatContext& ctx);

#define FIZMO_MP_LEGENDRE_FORWARD(FN)                                                                         \
    inline BigFloat FN(const BigFloat& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }    \
    inline BigFloat FN(const BigInt& x, std::int64_t n, const BigFloatContext& c) {                           \
        if (x.is_nan())       return BigFloat::nan();                                                         \
        if (x.is_undefined()) return BigFloat::undefined();                                                   \
        return FN(BigFloat(x), n, c);                                                                         \
    }                                                                                                         \
    inline BigFloat FN(const BigInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }      \
    inline BigFloat FN(const BigUInt& x, std::int64_t n, const BigFloatContext& c) {                          \
        if (x.is_undefined()) return BigFloat::undefined();                                                   \
        return FN(BigFloat(x), n, c);                                                                         \
    }                                                                                                         \
    inline BigFloat FN(const BigUInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }     \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>               \
    inline BigFloat FN(T x, std::int64_t n, const BigFloatContext& c) { return FN(BigFloat(x), n, c); }       \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>               \
    inline BigFloat FN(T x, std::int64_t n) { return FN(BigFloat(x), n, BigFloatContext::current()); }

FIZMO_MP_LEGENDRE_FORWARD(legendre_p)
FIZMO_MP_LEGENDRE_FORWARD(legendre_q)

#undef FIZMO_MP_LEGENDRE_FORWARD

inline BigFloat legendre_chi(const BigFloat& x, const BigFloat& s) { return legendre_chi(x, s, BigFloatContext::current()); }

template <typename X, typename S, typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat legendre_chi(const X& x, const S& s, const BigFloatContext& c) { return legendre_chi(BigFloat(x), BigFloat(s), c); }

template <typename X, typename S, typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<S, BigFloat>::value), int>::type = 0>
inline BigFloat legendre_chi(const X& x, const S& s) { return legendre_chi(BigFloat(x), BigFloat(s), BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_LEGENDRE_HPP