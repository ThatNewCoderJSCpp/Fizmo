#ifndef FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP
#define FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP

#include "big_float_consts.hpp"
#include "exp.hpp"
#include "pow_nth_root.hpp"
#include "sqrt_cbrt.hpp"
#include "logarithms.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace lwdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;

struct lw_val {
    BF     v;
    double lost;                                                          
};

inline BFC lw_ctx(std::size_t p) { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }

 double lw_log2(const BF& x);

inline double lw_to_double(const BF& x) {                                 
    if (x.is_zero()) return 0.0;
    const double v = std::exp2(lw_log2(x));
    return x.signbit() ? -v : v;
}

 bool lw_safe(const BF& v, std::size_t want, double lost, std::size_t prec);

template <typename Eval>
inline BF lw_drive(const BFC& ctx, Eval eval) {
    std::size_t guard = 32;

    for (;;) {
        const std::size_t want = BFC::clamp_precision(ctx.precision + guard);
        const lw_val      r    = eval(want);
        if (!r.v.is_finite()) return r.v;
        if (lw_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (guard >= (1u << 16) || ctx.precision + guard >= BFC::max_prec / 4) return r.v.rounded(ctx);
        guard *= 2;
    }
}

 int lw_eps(const BF& x, BF& eps);

 double lw_halley_d(double w, double x);

 BF lw_seed(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t& sp);

 bool lw_newton(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t want, BF& w);

 int lw_sign(const BF& y, const BF& x, std::size_t q);

 lw_val lw_raw(const BF& x, bool lower, const BF& eps, bool have_eps, std::size_t want);

} // namespace lwdetail

 BigFloat lambert_w(const BigFloat& x, std::int64_t k, const BigFloatContext& ctx);

inline BigFloat lambert_w(const BigFloat& x, std::int64_t k = 0)            { return lambert_w(x, k, BigFloatContext::current()); }
inline BigFloat lambert_w(const BigFloat& x, const BigFloatContext& ctx)    { return lambert_w(x, 0, ctx); }
inline BigFloat lambert_w0(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current())  { return lambert_w(x, 0, ctx); }
inline BigFloat lambert_wm1(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current()) { return lambert_w(x, -1, ctx); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, std::int64_t k, const BigFloatContext& ctx) { return lambert_w(BigFloat(x), k, ctx); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, std::int64_t k = 0) { return lambert_w(BigFloat(x), k, BigFloatContext::current()); }

template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline BigFloat lambert_w(T x, const BigFloatContext& ctx) { return lambert_w(BigFloat(x), 0, ctx); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_LAMBERT_W_HPP