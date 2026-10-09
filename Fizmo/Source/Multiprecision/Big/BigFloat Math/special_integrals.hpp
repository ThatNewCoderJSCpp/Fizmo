#ifndef FIZMO_MULTIPRECISION_BIG_SPECIAL_INTEGRALS_HPP
#define FIZMO_MULTIPRECISION_BIG_SPECIAL_INTEGRALS_HPP

#include "polylog.hpp"
#include "trig.hpp"
#include "euler_mascheroni.hpp"

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

namespace spdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;
using ztdetail::zt_val;
using hzdetail::hz_l2;
using hzdetail::hz_lsum;
using hzdetail::hz_xmul;
using hzdetail::hz_upow;

static const double        sp_ninf = -std::numeric_limits<double>::infinity();
static const std::uint64_t sp_cap  = 1ull << 26;

inline BFC    sp_ctx(std::size_t p)                        { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }
inline zt_val sp_undef()                                   { return zt_val{BF::undefined(), 0.0}; }
inline double sp_abs(const zt_val& r, std::size_t want)    { return r.v.is_zero() ? sp_ninf : hz_l2(r.v) + r.lost - static_cast<double>(want); }
inline double sp_dbl(const BF& x)                          { return x.is_zero() ? 0.0 : std::exp2(hz_l2(x)); }
inline std::size_t sp_guard(double extra)                  { return 32 + ((extra > 0.0 && extra < 1.0e7) ? static_cast<std::size_t>(extra) : 0); }

struct sp_sum {
    BF     s    = BF::zero();
    double err  = sp_ninf;
    double lmax = sp_ninf;

    void add(const BF& t, double units, const BFC& ac, std::size_t want);

    void add_abs(double l)                          { err = hz_lsum(err, l); }
    bool done(double ltail, std::size_t want) const { return ltail <= lmax - static_cast<double>(want) - 8.0; }
};

 zt_val sp_finish(const BF& v, double err, std::size_t want);

 zt_val sp_si_series(const BF& x, bool hyp, std::size_t want);

 zt_val sp_ci_series(const BF& x, bool hyp, std::size_t want);

 bool sp_sici_asym_ok(const BF& x, double target);

 zt_val sp_sici_asym(bool ci, const BF& x, std::size_t want);

 zt_val sp_sici_raw(int kind, const BF& x, std::size_t want);

 std::size_t sp_sici_guard(int kind, const BF& x, std::size_t prec);

 zt_val sp_ei_series(const BF& x, std::size_t want);

 zt_val sp_en_series(const BF& x, std::uint64_t n, std::size_t want);

 bool sp_en_asym_ok(double lx, std::uint64_t n, double target);

 zt_val sp_en_asym(const BF& x, std::uint64_t n, std::size_t want);

 zt_val sp_en_raw(const BF& x, std::uint64_t n, std::size_t want);

 std::size_t sp_en_guard(const BF& x, std::uint64_t n, std::size_t prec);

 zt_val sp_ei_raw(const BF& x, std::size_t want);

inline std::size_t sp_ei_guard(const BF& x, std::size_t prec) { return x.signbit() ? sp_en_guard(-x, 1, prec) : 32; }

 zt_val sp_li_raw(const BF& x, std::size_t want);

 zt_val sp_lin_series(const BF& y, std::uint64_t n, std::size_t want);

 zt_val sp_lin_raw(const BF& x, std::uint64_t n, std::size_t want);

inline std::size_t sp_lin_guard(const BF& x, std::uint64_t n, std::size_t prec) {
    const BF u = ln(x, sp_ctx(64));
    return u.signbit() ? sp_en_guard(u.abs(), n, prec) : 32;
}

 zt_val sp_fact_poly(const BF& z, std::uint64_t m, std::size_t want);

 std::size_t sp_fact_poly_guard(const BF& z, std::uint64_t m);

 zt_val sp_linneg_raw(const BF& x, std::uint64_t m, std::size_t want);

inline std::size_t sp_linneg_guard(const BF& x, std::uint64_t m) {
    return sp_fact_poly_guard(-ln(x, sp_ctx(64)), m);
}

 zt_val sp_en_neg_raw(const BF& x, std::uint64_t m, std::size_t want);

 void sp_sincospi(const BF& q, const BFC& wc, BF& s, BF& c);

 zt_val sp_fresnel_series(bool sine, const BF& x, std::size_t want);

 bool sp_fresnel_asym_ok(const BF& x, double target);

 zt_val sp_fresnel_asym(bool sine, const BF& x, std::size_t want);

 zt_val sp_fresnel_raw(bool sine, const BF& x, std::size_t want);

 std::size_t sp_fresnel_guard(const BF& x, std::size_t prec);

 bool sp_reduce_2pi(const BF& t, std::size_t want, BF& r, bool& reduced);

 zt_val sp_clausen_raw(const BF& theta, std::uint64_t s, std::size_t want);

 const std::vector<BigUInt>& sp_cl_poly(std::size_t m);

 zt_val sp_clausen_neg_raw(const BF& theta, std::uint64_t m, std::size_t want);

 zt_val sp_debye_small(const BF& x, std::uint64_t n, std::size_t want);

 zt_val sp_debye_tail_sum(const BF& x, std::uint64_t n, std::size_t want);

 zt_val sp_debye_raw(int kind, const BF& x, std::uint64_t n, std::size_t want);

 BF sp_neg_constant(const BFC& ctx, BF (*fn)(const BFC&));

inline bool sp_bad(const BF& x) { return x.is_nan() || x.is_undefined(); }

} // namespace spdetail

 BigFloat fresnel_c(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat fresnel_s(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat sin_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat cos_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat sinh_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat cosh_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat exp_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat exp_integral_generalized(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat log_integral(const BigFloat& x, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat log_integral_generalized(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current());

 BigFloat clausen(const BigFloat& theta, std::int64_t s, const BigFloatContext& ctx = BigFloatContext::current());

inline BigFloat clausen_integral(const BigFloat& theta, const BigFloatContext& ctx = BigFloatContext::current()) {
    return clausen(theta, 2, ctx);
}

namespace spdetail {

 BigFloat sp_debye_public(int kind, const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

} // namespace spdetail

inline BigFloat debye(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current())          { return spdetail::sp_debye_public(2, x, n, ctx); }
inline BigFloat debye_integral(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current()) { return spdetail::sp_debye_public(0, x, n, ctx); }
inline BigFloat debye_tail(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx = BigFloatContext::current())     { return spdetail::sp_debye_public(1, x, n, ctx); }

#define FIZMO_MP_SPI_FWD1(FN)                                                                                  \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>                 \
    inline BigFloat FN(T x, const BigFloatContext& c = BigFloatContext::current()) { return FN(BigFloat(x), c); }

#define FIZMO_MP_SPI_FWD2(FN)                                                                                  \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>                 \
    inline BigFloat FN(T x, std::int64_t n, const BigFloatContext& c = BigFloatContext::current()) { return FN(BigFloat(x), n, c); }

FIZMO_MP_SPI_FWD1(fresnel_c)
FIZMO_MP_SPI_FWD1(fresnel_s)
FIZMO_MP_SPI_FWD1(sin_integral)
FIZMO_MP_SPI_FWD1(cos_integral)
FIZMO_MP_SPI_FWD1(sinh_integral)
FIZMO_MP_SPI_FWD1(cosh_integral)
FIZMO_MP_SPI_FWD1(exp_integral)
FIZMO_MP_SPI_FWD1(log_integral)
FIZMO_MP_SPI_FWD1(clausen_integral)
FIZMO_MP_SPI_FWD2(exp_integral_generalized)
FIZMO_MP_SPI_FWD2(log_integral_generalized)
FIZMO_MP_SPI_FWD2(clausen)
FIZMO_MP_SPI_FWD2(debye)
FIZMO_MP_SPI_FWD2(debye_integral)
FIZMO_MP_SPI_FWD2(debye_tail)

#undef FIZMO_MP_SPI_FWD1
#undef FIZMO_MP_SPI_FWD2

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_SPECIAL_INTEGRALS_HPP