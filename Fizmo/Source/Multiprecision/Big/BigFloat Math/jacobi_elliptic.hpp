#ifndef FIZMO_MULTIPRECISION_BIG_JACOBI_ELLIPTIC_HPP
#define FIZMO_MULTIPRECISION_BIG_JACOBI_ELLIPTIC_HPP

#include "elliptic.hpp"
#include "htrig.hpp"
#include "inv_trig.hpp"
#include "trig.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace jcdetail {

using BF  = BigFloat;
using BFC = BigFloatContext;
using csdetail::cs_val;
using csdetail::cs_ctx;
using csdetail::cs_l2hi;
using csdetail::cs_l2lo;

static const double jc_ninf = -std::numeric_limits<double>::infinity();

enum class jc_fn : int { am, sn, cn, dn, ns, nc, nd, sc, sd, cs, cd, ds, dc };

double jc_lsum(double a, double b);

inline bool jc_is_one(const BF& x) { return BF::compare(x, BF::one()) == BF::ordering::equal; }

struct jc_trip {
    BF     phi, s, c, d;
    double lphi = jc_ninf;
    double ls   = jc_ninf;
    double lc   = jc_ninf;
    double ld   = jc_ninf;
};

bool jc_newton(const BF& ua, const BF& m, std::size_t want, BF& phi, double& lphi);

void jc_derive(jc_trip& t, const BF& m, std::size_t want);

bool jc_triple(const BF& u, const BF& m, std::size_t want, jc_trip& t);

void jc_parts(jc_fn f, int& p, int& q);

cs_val jc_raw(jc_fn f, const BF& u, const BF& m, std::size_t want);

inline BF jc_signed_inf(bool neg) { return BF::infinity(neg); }

BF jc_public(jc_fn f, const BF& u, const BF& m, const BFC& ctx);

} // namespace jcdetail

#define FIZMO_MP_JACOBI_FN(NAME, KIND)                                                                             \
    inline BigFloat NAME(const BigFloat& u, const BigFloat& m, const BigFloatContext& ctx = BigFloatContext::current()) { \
        return jcdetail::jc_public(jcdetail::jc_fn::KIND, u, m, ctx);                                              \
    }                                                                                                              \
    template <typename A, typename B,                                                                              \
              typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value, int>::type = 0> \
    inline BigFloat NAME(A u, B m, const BigFloatContext& ctx = BigFloatContext::current()) {                      \
        return NAME(BigFloat(u), BigFloat(m), ctx);                                                                \
    }

FIZMO_MP_JACOBI_FN(jacobi_am, am)
FIZMO_MP_JACOBI_FN(jacobi_sn, sn)
FIZMO_MP_JACOBI_FN(jacobi_cn, cn)
FIZMO_MP_JACOBI_FN(jacobi_dn, dn)
FIZMO_MP_JACOBI_FN(jacobi_ns, ns)
FIZMO_MP_JACOBI_FN(jacobi_nc, nc)
FIZMO_MP_JACOBI_FN(jacobi_nd, nd)
FIZMO_MP_JACOBI_FN(jacobi_sc, sc)
FIZMO_MP_JACOBI_FN(jacobi_sd, sd)
FIZMO_MP_JACOBI_FN(jacobi_cs, cs)
FIZMO_MP_JACOBI_FN(jacobi_cd, cd)
FIZMO_MP_JACOBI_FN(jacobi_ds, ds)
FIZMO_MP_JACOBI_FN(jacobi_dc, dc)

#undef FIZMO_MP_JACOBI_FN

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_JACOBI_ELLIPTIC_HPP