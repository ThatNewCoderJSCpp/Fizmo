#ifndef FIZMO_MULTIPRECISION_BIG_ELLIPTIC_HPP
#define FIZMO_MULTIPRECISION_BIG_ELLIPTIC_HPP

#include "carlson.hpp"
#include "trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace eldetail {

using BF  = BigFloat;
using BFC = BigFloatContext;
using csdetail::cs_val;

enum class el_kind { f, e, pi };

inline cs_val el_nan()   { return cs_val{BF::nan(), 0.0}; }
inline cs_val el_undef() { return cs_val{BF::undefined(), 0.0}; }

BF el_xmul(const BF& x, const BF& y);

inline bool el_is(const BF& x, std::uint64_t v) { return BF::compare(x, BF(v)) == BF::ordering::equal; }
inline bool el_gt_one(const BF& x)              { return BF::compare(x, BF::one()) == BF::ordering::greater; }

cs_val el_rj_any(const BF& x, const BF& y, const BF& z, const BF& p, std::size_t want);

struct el_red {
    std::int64_t j = 0;
    BF           theta;
};

bool el_reduce(const BF& phi, std::size_t want, el_red& r);

cs_val el_complete_raw(el_kind kind, const BF& n, const BF& m, std::size_t want);

cs_val el_incomplete_raw(el_kind kind, const BF& phi, const BF& n, const BF& m, std::size_t want);

inline bool el_bad(const BF& x) { return x.is_nan() || x.is_undefined(); }
inline BF   el_bad_of(const BF& x, const BF& y, const BF& z) {
    if (x.is_nan() || y.is_nan() || z.is_nan()) return BF::nan();
    return BF::undefined();
}

} // namespace eldetail

BigFloat elliptic_f(const BigFloat& phi, const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

BigFloat elliptic_e(const BigFloat& phi, const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

BigFloat elliptic_pi(const BigFloat& phi, const BigFloat& n, const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

BigFloat elliptic_k(const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

BigFloat elliptic_e(const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

BigFloat elliptic_pi(const BigFloat& n, const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current());

template <typename A, typename B, typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, int>::type = 0>
inline BigFloat elliptic_f(A phi, B m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_f(BigFloat(phi), BigFloat(m), c); }

template <typename A, typename B, typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, int>::type = 0>
inline BigFloat elliptic_e(A phi, B m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_e(BigFloat(phi), BigFloat(m), c); }

template <typename A, typename B, typename C,
          typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value && std::is_arithmetic<C>::value, int>::type = 0>
inline BigFloat elliptic_pi(A phi, B n, C m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_pi(BigFloat(phi), BigFloat(n), BigFloat(m), c); }

template <typename A, typename std::enable_if<std::is_arithmetic<A>::value, int>::type = 0>
inline BigFloat elliptic_k(A m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_k(BigFloat(m), c); }

template <typename A, typename std::enable_if<std::is_arithmetic<A>::value, int>::type = 0>
inline BigFloat elliptic_e(A m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_e(BigFloat(m), c); }

template <typename A, typename B, typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, int>::type = 0>
inline BigFloat elliptic_pi(A n, B m, const BigFloatContext& c = BigFloatContext::current()) { return elliptic_pi(BigFloat(n), BigFloat(m), c); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_ELLIPTIC_HPP