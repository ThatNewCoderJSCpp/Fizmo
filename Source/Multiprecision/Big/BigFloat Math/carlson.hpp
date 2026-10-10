#ifndef FIZMO_MULTIPRECISION_BIG_CARLSON_HPP
#define FIZMO_MULTIPRECISION_BIG_CARLSON_HPP

#include "big_float_consts.hpp"
#include "pow_nth_root.hpp"
#include "sqrt_cbrt.hpp"
#include "logarithms.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace csdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;

struct cs_val {
    BF     v;
    double lost;                                                          
};

static const double cs_inf = std::numeric_limits<double>::infinity();

inline BFC    cs_ctx(std::size_t p) { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }
inline cs_val cs_undef()            { return cs_val{BF::undefined(), 0.0}; }
inline bool   cs_neg(const BF& v)   { return v.signbit() && !v.is_zero(); }
inline std::size_t cs_cap(std::size_t want) { return 4 * want + 256; }
unsigned    cs_order(std::size_t want);

double cs_to_double(const BF& x);

bool cs_safe(const BF& v, std::size_t want, double lost, std::size_t prec);

template <typename Eval>
inline BF cs_drive(const BFC& ctx, Eval eval) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = BFC::clamp_precision(ctx.precision + guard);
        const cs_val      r    = eval(want);
        if (!r.v.is_finite()) return r.v;
        if (cs_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (guard >= (1u << 16) || ctx.precision + guard >= BFC::max_prec / 4) return r.v.rounded(ctx);
        guard *= 2;
    }
}

BF cs_xsub(const BF& x, const BF& y);

bool cs_pow4(const BF& x, std::int64_t& k);

double cs_devs(const std::vector<BF>& v, const BF& A, const BFC& wc, std::vector<BF>& Z);

double cs_tail_l2(double r, unsigned a2, unsigned M);

BF cs_series(const std::vector<BF>& Z, const unsigned* b2, unsigned a2, unsigned M, const BFC& wc);

cs_val cs_rc_raw(BF x, BF y, std::size_t want);

cs_val cs_rf_raw(BF x, BF y, BF z, std::size_t want);

cs_val cs_rd_raw(BF x, BF y, BF z, std::size_t want);

cs_val cs_rj_raw(BF x, BF y, BF z, BF p, std::size_t want);

inline bool cs_any(std::initializer_list<const BF*> v, bool (*pred)(const BF&)) {
    for (const BF* p : v) if (pred(*p)) return true;
    return false;
}

inline bool cs_is_nan(const BF& v)   { return v.is_nan(); }
inline bool cs_is_undef(const BF& v) { return v.is_undefined(); }
inline bool cs_is_neg(const BF& v)   { return cs_neg(v); }
inline bool cs_is_inf(const BF& v)   { return v.is_infinite(); }

inline double cs_l2lo(const BF& x) { return x.is_zero() ? -cs_inf : static_cast<double>(x.get_exp_base2()); }
inline double cs_l2hi(const BF& x) { return cs_l2lo(x) + 1.0; }

void cs_sort3(const BF& x, const BF& y, const BF& z, BF& lo, BF& mid, BF& hi);

cs_val cs_rc_pv_raw(const BF& x, const BF& y, std::size_t want);

cs_val cs_rj_pv_raw(const BF& x, const BF& y, const BF& z, const BF& p, std::size_t want);

cs_val cs_rg_raw(const BF& a, const BF& m, const BF& c, std::size_t want);

} // namespace csdetail

BigFloat carlson_rc(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx);

BigFloat carlson_rf(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx);

BigFloat carlson_rd(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx);

BigFloat carlson_rj(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloat& p, const BigFloatContext& ctx);

inline BigFloat carlson_rc(const BigFloat& x, const BigFloat& y)                                       { return carlson_rc(x, y, BigFloatContext::current()); }
inline BigFloat carlson_rf(const BigFloat& x, const BigFloat& y, const BigFloat& z)                    { return carlson_rf(x, y, z, BigFloatContext::current()); }
inline BigFloat carlson_rd(const BigFloat& x, const BigFloat& y, const BigFloat& z)                    { return carlson_rd(x, y, z, BigFloatContext::current()); }
inline BigFloat carlson_rj(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloat& p) { return carlson_rj(x, y, z, p, BigFloatContext::current()); }

template <typename X, typename Y,
          typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<Y, BigFloat>::value), int>::type = 0
          >
inline BigFloat carlson_rc(const X& x, const Y& y, const BigFloatContext& c = BigFloatContext::current()) {
    return carlson_rc(BigFloat(x), BigFloat(y), c);
}

template <typename X, typename Y, typename Z,
          typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<Y, BigFloat>::value &&
                                    std::is_same<Z, BigFloat>::value), int>::type = 0
                                 >
inline BigFloat carlson_rf(const X& x, const Y& y, const Z& z, const BigFloatContext& c = BigFloatContext::current()) {
    return carlson_rf(BigFloat(x), BigFloat(y), BigFloat(z), c);
}

template <typename X, typename Y, typename Z,
          typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<Y, BigFloat>::value &&
                                    std::is_same<Z, BigFloat>::value), int>::type = 0
                                 >
inline BigFloat carlson_rd(const X& x, const Y& y, const Z& z, const BigFloatContext& c = BigFloatContext::current()) {
    return carlson_rd(BigFloat(x), BigFloat(y), BigFloat(z), c);
}

template <typename X, typename Y, typename Z, typename P,
          typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<Y, BigFloat>::value &&
                                    std::is_same<Z, BigFloat>::value && std::is_same<P, BigFloat>::value), int>::type = 0
                                 >
inline BigFloat carlson_rj(const X& x, const Y& y, const Z& z, const P& p, const BigFloatContext& c = BigFloatContext::current()) {
    return carlson_rj(BigFloat(x), BigFloat(y), BigFloat(z), BigFloat(p), c);
}

BigFloat carlson_rg(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx);

inline BigFloat carlson_rg(const BigFloat& x, const BigFloat& y, const BigFloat& z) { return carlson_rg(x, y, z, BigFloatContext::current()); }

template <typename X, typename Y, typename Z,
          typename std::enable_if<!(std::is_same<X, BigFloat>::value && std::is_same<Y, BigFloat>::value &&
                                    std::is_same<Z, BigFloat>::value), int>::type = 0
                                 >
inline BigFloat carlson_rg(const X& x, const Y& y, const Z& z, const BigFloatContext& c = BigFloatContext::current()) {
    return carlson_rg(BigFloat(x), BigFloat(y), BigFloat(z), c);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_CARLSON_HPP