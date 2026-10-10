#ifndef FIZMO_MULTIPRECISION_BIG_HURWITZ_LERCH_HPP
#define FIZMO_MULTIPRECISION_BIG_HURWITZ_LERCH_HPP

#include "riemann_zeta.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace hzdetail {

using ztdetail::zt_val;

static const double        hz_ln2       = 0.6931471805599453;
static const std::uint64_t lp_term_cap  = 1ull << 24;

inline double hz_ninf() { return -std::numeric_limits<double>::infinity(); }
inline double hz_l2(const BigFloat& x) { return x.is_zero() ? hz_ninf() : gmdetail::gm_log2_fine(x); }   // log2|x|

double hz_lsum(double a, double b);

inline double hz_ln_add(double la, std::uint64_t k) {                    
    return la * hz_ln2 + std::log1p(static_cast<double>(k) * std::exp2(-la));
}

BigFloat hz_xmul(const BigFloat& x, const BigFloat& y);

BigFloat hz_xadd(const BigFloat& x, const BigFloat& y, bool sub = false);

inline BigUInt hz_upow(BigUInt b, std::uint64_t e) {
    BigUInt r = BigUInt::one();

    while (e != 0) {
        if (e & 1u) r = r * b;
        e >>= 1;
        if (e != 0) b = b * b;
    }

    return r;
}

bool hz_int64(const BigFloat& s, std::int64_t& k);

template <typename Eval>
inline BigFloat hz_drive(std::size_t guard, const BigFloatContext& ctx, Eval eval) {
    for (;;) {
        const std::size_t want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const zt_val      r    = eval(want);
        if (!r.v.is_finite()) return r.v;
        if (ztdetail::zt_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (ztdetail::zt_guard_exhausted(ctx.precision, guard)) return r.v.rounded(ctx);
        guard *= 2;
    }
}

inline std::size_t hz_guard(double extra_bits) {
    return 32 + ((extra_bits > 0.0 && extra_bits < 4096.0) ? static_cast<std::size_t>(extra_bits) : 0);
}

bool hz_pow_exact(const BigFloat& a, std::int64_t k, const BigFloatContext& ctx, BigFloat& out);

BigFloat hz_pow_neg(const BigFloat& a, const BigFloat& s, const BigFloatContext& ctx);

bool hz_neg_int_exact(std::uint64_t m, const BigFloat& a, const BigFloatContext& ctx, BigFloat& out);

struct hz_sinfo {
    double       sd    = 0.0;
    std::int64_t i0    = -1;       
    double       lnear = 0.0;      
};

hz_sinfo hz_info(const BigFloat& s);

struct hz_plan {
    std::uint64_t N     = 0;
    std::uint64_t M     = 0;
    double        log2R = 0.0;
    bool          ok    = false;
};

hz_plan hz_choose(const hz_sinfo& si, double la, std::size_t want);

zt_val hz_em(const BigFloat& s, const BigFloat& om, const BigFloat& a, const hz_plan& pl, std::size_t want);

zt_val hz_eval(const BigFloat& s, const BigFloat& om, const BigFloat& a, const hz_sinfo& si, std::size_t want);

const std::vector<BigUInt>& hz_eulerian(std::size_t m);

bool lp_rational(const BigFloat& z, std::uint64_t m, const BigFloat& a, const BigFloatContext& ctx, BigFloat& out);

struct lp_dplan {
    std::uint64_t N     = 0;
    double        lmax  = hz_ninf();
    double        labs  = hz_ninf();
    double        ltail = hz_ninf();
    bool          ok    = false;
};

lp_dplan lp_plan_direct(double Ld, double sd, double la, std::size_t want, std::uint64_t cap);

BigFloat lp_term(const BigFloat& L, const BigFloat& s, const BigFloat& a, std::uint64_t n, const BigFloatContext& wc);

zt_val lp_direct(const BigFloat& L, bool neg, const BigFloat& s, const BigFloat& a, std::size_t want);

zt_val lp_alt(const BigFloat& L, const BigFloat& s, const BigFloat& a, std::size_t want);

zt_val lp_halving(const BigFloat& L, const BigFloat& s, const BigFloat& om, const BigFloat& a, std::uint64_t k, std::size_t want);

} // namespace hzdetail

BigFloat hurwitz_zeta(const BigFloat& s, const BigFloat& a, const BigFloatContext& ctx);

BigFloat lerch_transcendent(const BigFloat& z, const BigFloat& s, const BigFloat& a, const BigFloatContext& ctx);

inline BigFloat hurwitz_zeta(const BigFloat& s, const BigFloat& a) { return hurwitz_zeta(s, a, BigFloatContext::current()); }
inline BigFloat lerch_transcendent(const BigFloat& z, const BigFloat& s, const BigFloat& a) { return lerch_transcendent(z, s, a, BigFloatContext::current()); }

template <typename S, typename A, typename std::enable_if<!(std::is_same<S, BigFloat>::value && std::is_same<A, BigFloat>::value), int>::type = 0>
inline BigFloat hurwitz_zeta(const S& s, const A& a, const BigFloatContext& c) { return hurwitz_zeta(BigFloat(s), BigFloat(a), c); }

template <typename S, typename A, typename std::enable_if<!(std::is_same<S, BigFloat>::value && std::is_same<A, BigFloat>::value), int>::type = 0>
inline BigFloat hurwitz_zeta(const S& s, const A& a) { return hurwitz_zeta(BigFloat(s), BigFloat(a), BigFloatContext::current()); }

template <typename Z, typename S, typename A, typename std::enable_if<!(std::is_same<Z, BigFloat>::value && std::is_same<S, BigFloat>::value && std::is_same<A, BigFloat>::value), int>::type = 0>
inline BigFloat lerch_transcendent(const Z& z, const S& s, const A& a, const BigFloatContext& c) {
    return lerch_transcendent(BigFloat(z), BigFloat(s), BigFloat(a), c);
}

template <typename Z, typename S, typename A, typename std::enable_if<!(std::is_same<Z, BigFloat>::value && std::is_same<S, BigFloat>::value && std::is_same<A, BigFloat>::value), int>::type = 0>
inline BigFloat lerch_transcendent(const Z& z, const S& s, const A& a) {
    return lerch_transcendent(BigFloat(z), BigFloat(s), BigFloat(a), BigFloatContext::current());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_HURWITZ_LERCH_HPP