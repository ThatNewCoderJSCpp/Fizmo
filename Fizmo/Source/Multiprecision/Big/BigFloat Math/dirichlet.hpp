#ifndef FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP
#define FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP

#include "riemann_zeta.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace dldetail {

using ztdetail::zt_val;

inline double dl_lost(double units) { return std::log2(units) + 1.0; }

 BigFloat dl_omp2(const BigFloat& x, const BigFloatContext& wc);

inline double dl_omp2_units(const BigFloat& x) {
    return 6.0 + 5.0 * 0.34657359027997264 * std::fabs(ztdetail::zt_to_double(x));
}

 double dl_power_sum_bound(std::uint64_t n, double sd);

 zt_val dl_alt(const BigFloat& s, bool odd, std::size_t want);

 zt_val dl_eta_fe(const BigFloat& s, const BigFloat& om, std::size_t want);

 zt_val dl_lambda_pos(const BigFloat& s, const BigFloat& om, std::size_t want);

 zt_val dl_lambda_neg(const BigFloat& s, const BigFloat& om, std::size_t want);

 zt_val dl_beta_fe(const BigFloat& u, std::size_t want);

 bool dl_nonpos_int(const BigFloat& s, bool& fits, std::uint64_t& m, bool& even);

 std::uint64_t dl_log3_threshold(std::size_t prec);

template <typename Eval>
inline BigFloat dl_drive(const BigFloat& s, const BigFloatContext& ctx, Eval eval) {
    std::size_t guard = ztdetail::zt_guard0(s);

    for (;;) {
        const std::size_t want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const zt_val      r    = eval(want);
        if (!r.v.is_finite() || r.v.is_zero()) return r.v;
        if (ztdetail::zt_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return r.v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace dldetail

 BigFloat dirichlet_eta(const BigFloat& s, const BigFloatContext& ctx);

 BigFloat dirichlet_lambda(const BigFloat& s, const BigFloatContext& ctx);

 BigFloat dirichlet_beta(const BigFloat& s, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(dirichlet_eta)
FIZMO_MP_TRIG_FORWARD(dirichlet_lambda)
FIZMO_MP_TRIG_FORWARD(dirichlet_beta)

#define FIZMO_MP_DIRICHLET_ARITH_FORWARD(FN)                                                            \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }                     \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>          \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_eta)
FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_lambda)
FIZMO_MP_DIRICHLET_ARITH_FORWARD(dirichlet_beta)

#undef FIZMO_MP_DIRICHLET_ARITH_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_DIRICHLET_HPP