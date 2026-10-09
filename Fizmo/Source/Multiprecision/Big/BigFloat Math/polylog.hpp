#ifndef FIZMO_MULTIPRECISION_BIG_POLYLOG_HPP
#define FIZMO_MULTIPRECISION_BIG_POLYLOG_HPP

#include "hurwitz_lerch.hpp"
#include "dirichlet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace pldetail {

using ztdetail::zt_val;
using hzdetail::hz_l2;
using hzdetail::hz_lsum;
using hzdetail::hz_ninf;
using hzdetail::hz_xadd;
using hzdetail::hz_xmul;
using hzdetail::hz_upow;
using hzdetail::hz_ln2;

static const double pl_pi  = 3.141592653589793;
static const double pl_2pi = 6.283185307179586;

struct pl_vcache {
    std::vector<BigFloat>    v;
    std::vector<std::size_t> p;
};

template <typename F>
inline BigFloat pl_cached(pl_vcache& c, std::size_t i, const BigFloatContext& wc, F compute) {
    if (c.v.size() <= i) { c.v.resize(i + 1, BigFloat::zero()); c.p.resize(i + 1, 0); }

    if (c.p[i] < wc.precision + 16) {
        const std::size_t hp = BigFloatContext::clamp_precision(std::max(wc.precision + 64, c.p[i] + c.p[i] / 2));
        c.v[i] = compute(BigFloatContext(hp, RoundingMode::nearest_even));
        c.p[i] = hp;
    }

    return c.v[i].rounded(wc);
}

inline pl_vcache& pl_zeta_cache() { static thread_local pl_vcache c; return c; }
inline pl_vcache& pl_eta_cache()  { static thread_local pl_vcache c; return c; }
inline pl_vcache& pl_zneg_cache() { static thread_local pl_vcache c; return c; }

BigFloat pl_zeta_pos(std::uint64_t j, const BigFloatContext& wc);

BigFloat pl_eta_pos(std::uint64_t j, const BigFloatContext& wc);

BigFloat pl_zeta_negodd(std::uint64_t m, const BigFloatContext& wc);

struct pl_acc {
    double err  = hz_ninf();
    double sens = hz_ninf();
    double lmax = hz_ninf();

    void add(const BigFloat& t, double e, double sw);

    zt_val finish(const BigFloat& S, const BigFloatContext& wc, double ltail) const;
};

bool pl_neg_int(std::uint64_t m, const BigFloat& z, const BigFloatContext& ctx, BigFloat& out);

zt_val pl_int_direct(std::uint64_t n, const BigFloat& z, std::size_t want);


zt_val pl_int_mu(std::uint64_t n, const BigFloat& mu, bool eta, std::size_t want);

zt_val pl_int_raw(std::uint64_t n, const BigFloat& z, std::size_t want);

zt_val pl_int_inversion(std::uint64_t n, const BigFloat& z, std::size_t want);

zt_val pl_nonint_mu(const BigFloat& s, const BigFloat& om, const BigFloat& mu, bool eta, std::size_t want);

zt_val pl_raw_unit(const BigFloat& z, const BigFloat& s, const BigFloat& om, std::size_t want);

zt_val pl_li2_raw(const BigFloat& z, std::size_t want);

} // namespace pldetail

BigFloat polylog(const BigFloat& z, const BigFloat& s, const BigFloatContext& ctx);

inline BigFloat polylog(const BigFloat& z, const BigFloat& s) { return polylog(z, s, BigFloatContext::current()); }

template <typename S, typename Z, typename std::enable_if<!(std::is_same<S, BigFloat>::value && std::is_same<Z, BigFloat>::value), int>::type = 0>
inline BigFloat polylog(const Z& z, const S& s, const BigFloatContext& c) { return polylog(BigFloat(z), BigFloat(s), c); }

template <typename S, typename Z, typename std::enable_if<!(std::is_same<S, BigFloat>::value && std::is_same<Z, BigFloat>::value), int>::type = 0>
inline BigFloat polylog(const Z& z, const S& s) { return polylog(BigFloat(z), BigFloat(s), BigFloatContext::current()); }

inline BigFloat dilogarithm(const BigFloat& z, const BigFloatContext& ctx)  { return polylog(z, BigFloat(static_cast<std::uint64_t>(2)), ctx); }
inline BigFloat trilogarithm(const BigFloat& z, const BigFloatContext& ctx) { return polylog(z, BigFloat(static_cast<std::uint64_t>(3)), ctx); }
inline BigFloat dilogarithm(const BigFloat& z)  { return dilogarithm(z, BigFloatContext::current()); }
inline BigFloat trilogarithm(const BigFloat& z) { return trilogarithm(z, BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_POLYLOG_HPP