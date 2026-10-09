#ifndef FIZMO_MULTIPRECISION_BIG_POLYGAMMA_HPP
#define FIZMO_MULTIPRECISION_BIG_POLYGAMMA_HPP

#include "gamma.hpp"
#include "euler_mascheroni.hpp"
#include "trig.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace pgdetail {

using gmdetail::gm_exact_u64;
using gmdetail::gm_half_odd;
using gmdetail::gm_frac;
using gmdetail::gm_target;
using gmdetail::gm_term_prec;
using gmdetail::gm_log2_fine;
using gmdetail::gm_powi;
using gmdetail::fact_range;
using gmdetail::gm_zeta_t;
using gmdetail::gm_zeta_init;
using gmdetail::gm_zeta_step;
using gmdetail::gm_zeta_value;
using gmdetail::gm_zeta_kbits;
using gmdetail::gm_zeta_jmin;
using gmdetail::gm_guard_exhausted;
using gmdetail::gm_block_poly;
using gmdetail::gm_shift_count;
using gmdetail::gm_const_cache_t;
using gmdetail::gm_half_ln_two_pi;

struct pg_val_t {
    BigFloat    v;
    std::size_t cancelled = 0;
    bool        pole      = false;
    pg_val_t() : v(BigFloat::zero()) {}
};

 std::size_t pg_cancel(const BigFloat& a, const BigFloat& b, const BigFloat& r, std::size_t prec);

inline std::uint64_t pg_target(unsigned n, std::size_t w) {
    return static_cast<std::uint64_t>(gm_target(w)) + ((n >= 2) ? n : 0);
}

struct pg_coef_t {
    BigFloat    v;
    std::size_t prec = 0;
    pg_coef_t() : v(BigFloat::zero()) {}
};

inline std::vector<pg_coef_t>& pg_coef_cache(unsigned n) {
    static thread_local std::unordered_map<unsigned, std::vector<pg_coef_t>> c;
    return c[n];
}

 BigFloat pg_coeff(unsigned n, std::size_t j, const BigFloatContext& tc);

 double pg_term_log2(unsigned n, std::size_t j, double lx);

 std::size_t pg_jcap(int n, std::size_t j0, double lx, std::int64_t eh, std::size_t w);

 BigFloat pg_bsum(int n, std::size_t j0, const BigFloat& x, std::int64_t eh, const BigFloatContext& wc);

 BigFloat pg_asym(unsigned n, const BigFloat& x, const BigFloatContext& wc);

static const std::size_t pg_block = 16;   // tune: 8-24

 BigFloat pg_recip_sum(unsigned n, const BigFloat& z, std::uint64_t m, const BigFloatContext& wc);

static const std::size_t pg_pow_budget = 24;  // max polynomial degree per block

 std::vector<BigUInt> pg_pmul(const std::vector<BigUInt>& a, const std::vector<BigUInt>& b);

 std::vector<BigUInt> pg_binom_poly(std::uint64_t a, std::uint64_t s);

 BigFloat pg_poly_eval(const std::vector<BigUInt>& p, const std::vector<BigFloat>& zp, const BigFloatContext& wc);

 BigFloat pg_power_sum(std::uint64_t s, const BigFloat& z, std::uint64_t m, const BigFloatContext& wc);

 const std::vector<BigUInt>& pg_cot_poly(unsigned n);

 BigFloat pg_cot_eval(const std::vector<BigUInt>& q, const BigFloat& c, const BigFloatContext& wc);

 void pg_ratsum(std::uint64_t a, std::uint64_t b, std::uint64_t s, std::uint64_t o, unsigned e, BigUInt& P, BigUInt& Q);

 pg_val_t pg_exact(unsigned n, std::uint64_t N, bool half, const BigFloatContext& wc);

 pg_val_t pg_pos(unsigned n, const BigFloat& z, const BigFloatContext& wc);

 pg_val_t pg_eval(unsigned n, const BigFloat& z, const BigFloatContext& wc);

 BigFloat pg_drive(unsigned n, const BigFloat& z, const BigFloatContext& ctx);

 BigFloat pg_bern(std::size_t j, const BigFloatContext& wc);

inline std::uint64_t hz_target(std::size_t w, unsigned k) {
    return static_cast<std::uint64_t>(w / 6 + 16 + 2 * static_cast<std::size_t>(k));
}

 void hz_eprod(std::uint64_t lo, std::uint64_t hi, const BigFloat& a, unsigned k, const BigFloatContext& wc, std::vector<BigFloat>& E);

 BigFloat hz_logsum(unsigned k, const BigFloat& a, std::uint64_t N, const BigFloat& y, const BigFloatContext& wc);

 pg_val_t hz_dz(unsigned k, const BigFloat& a, const BigFloatContext& wc);

 BigFloat hz_zeta_prime_neg(unsigned m, const BigFloatContext& wc);

 pg_val_t pg_neg_eval(unsigned M, const BigFloat& x, const BigFloatContext& wc);

 BigFloat pg_neg_drive(unsigned M, const BigFloat& x, const BigFloatContext& ctx);

} // namespace pgdetail

 BigFloat digamma(const BigFloat& z, const BigFloatContext& ctx);

 BigFloat trigamma(const BigFloat& z, const BigFloatContext& ctx);

 BigFloat polygamma(const BigFloat& x, int n, const BigFloatContext& ctx);

inline BigFloat digamma  (const BigFloat& z)        { return digamma(z,      BigFloatContext::current()); }
inline BigFloat trigamma (const BigFloat& z)        { return trigamma(z,     BigFloatContext::current()); }
inline BigFloat polygamma(const BigFloat& x, int n) { return polygamma(x, n, BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_POLYGAMMA_HPP