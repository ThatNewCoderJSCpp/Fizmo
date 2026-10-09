#ifndef FIZMO_MULTIPRECISION_BIG_GAMMA_HPP
#define FIZMO_MULTIPRECISION_BIG_GAMMA_HPP

#include "bernoulli_tangent.hpp"
#include "logarithms.hpp"
#include "trig.hpp"            
#include "exp.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace gmdetail {

 double gm_log2_fine(const BigFloat& x);

 bool gm_half_odd(const BigFloat& z, std::uint64_t& n);

 BigFloat gm_powi(BigFloat b, std::uint64_t e, const BigFloatContext& wc);

 BigUInt gm_powi_u(std::uint64_t k, std::uint64_t e);

 double gm_term_log2(std::size_t j, double lx);

 std::size_t gm_term_prec(std::size_t wprec, std::int64_t eh, double lt, std::size_t G);

static const std::size_t gm_zeta_kbits = 9;   
static const std::size_t gm_zeta_jmin  = 8;

struct gm_zeta_t {
    std::vector<BigUInt> p;
    std::size_t          W = 0;
};

 void gm_zeta_init(gm_zeta_t& zs, std::uint64_t j, std::size_t W);

 void gm_zeta_step(gm_zeta_t& zs, std::size_t Wnext);

 BigFloat gm_zeta_value(const gm_zeta_t& zs, const BigFloatContext& tc);

 bool gm_exact_u64(const BigFloat& z, std::uint64_t& out);   

struct gm_const_cache_t {
    BigFloat    v;
    std::size_t prec = 0;
    gm_const_cache_t() : v(BigFloat::zero()) {}
};

 BigFloat gm_cached_const(gm_const_cache_t& c, const BigFloatContext& wc, BigFloat (*compute)(const BigFloatContext&));

inline BigFloat gm_ln_pi_raw(const BigFloatContext& c)          { return ln(constants::pi(c), c); }
inline BigFloat gm_half_ln_two_pi_raw(const BigFloatContext& c) { return ln(constants::two_pi(c), c).scaled_pow2(-1); }

inline BigFloat gm_ln_pi(const BigFloatContext& wc) {
    static thread_local gm_const_cache_t c;
    return gm_cached_const(c, wc, &gm_ln_pi_raw);
}

inline BigFloat gm_half_ln_two_pi(const BigFloatContext& wc) {
    static thread_local gm_const_cache_t c;
    return gm_cached_const(c, wc, &gm_half_ln_two_pi_raw);
}

struct gm_bcoef_t {
    BigFloat    v;
    std::size_t prec = 0;
    gm_bcoef_t() : v(BigFloat::zero()) {}
};

inline std::vector<gm_bcoef_t>& gm_bcoef_cache() {
    static thread_local std::vector<gm_bcoef_t> c;
    return c;
}

 BigFloat gm_stirling_coeff(std::size_t j, const BigFloatContext& tc);

 void gm_block_poly(std::uint64_t k, std::size_t s, std::vector<BigUInt>& c);

 bool gm_shift_count(const BigFloat& z, std::uint64_t T, std::uint64_t& m);

static const std::size_t gm_rising_block = 12;   // tune: 8-16

 BigFloat gm_rising_rs(const BigFloat& z, std::uint64_t m, const BigFloatContext& wc);

 BigUInt fact_range(std::uint64_t a, std::uint64_t b);

struct fact_cache_t {
    std::uint64_t n = 0;
    BigUInt       v;
    bool          valid = false;
    fact_cache_t() : v(BigUInt::one()) {}
};

inline fact_cache_t& fact_cache() { static thread_local fact_cache_t c; return c; }

 std::uint64_t fact_n_cap();

inline std::size_t gm_target(std::size_t w) { return (w * 3) / 10 + 16; }

 bool gm_shift(const BigFloat& z, const BigFloatContext& wc, const BigFloatContext& xc, BigFloat& x, BigFloat& R, std::uint64_t& m);

 std::size_t gm_terms(double log2x, double w);

 double gm_log2_of(const BigFloat& x);

 BigFloat gm_stirling(const BigFloat& x, const BigFloatContext& wc, std::size_t jcap);

 BigFloat gm_pos(const BigFloat& z, const BigFloatContext& wc, std::size_t& cancelled);

 BigFloat gm_gamma_pos(const BigFloat& z, const BigFloatContext& wc);

 BigFloat gm_frac(const BigFloat& z, const BigFloatContext& wc);

 bool gm_exact_u64(const BigFloat& z, std::uint64_t& out);

struct gm_log_t {
    BigFloat    v;                                    
    bool        neg   = false;                        
    bool        pole  = false;
    std::size_t cancelled = 0;
    gm_log_t() : v(BigFloat::zero()) {}
};

 gm_log_t gm_log_abs(const BigFloat& z, const BigFloatContext& wc);

 BigFloat gm_gamma_abs(const BigFloat& z, const BigFloatContext& wc);

inline bool gm_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

 BigFloat gm_drive(const BigFloat& z, const BigFloatContext& ctx, bool& neg, bool& pole);

 BigFloat gm_gamma_drive(const BigFloat& z, const BigFloatContext& ctx, bool neg);

 BigFloat gm_exp_of_log(const BigFloat& z, const BigFloatContext& ctx, bool, bool pole_in);

} // namespace gmdetail

 BigUInt factorial(std::uint64_t n);

inline BigFloat factorial(const BigFloat& x, const BigFloatContext& ctx);

 int gamma_sign(const BigFloat& z, const BigFloatContext& ctx);

 BigFloat log_abs_gamma(const BigFloat& z, const BigFloatContext& ctx);

 BigFloat log_gamma(const BigFloat& z, const BigFloatContext& ctx);

 BigFloat gamma(const BigFloat& z, const BigFloatContext& ctx);

inline BigFloat factorial(const BigFloat& x, const BigFloatContext& ctx) {
    return gamma(BigFloat::add(x, BigFloat::one(), ctx.extended(16)), ctx);
}

namespace gmdetail {

 BigFloat gm_log_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx, bool& neg, int& state);

} // namespace gmdetail

 BigFloat log_abs_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

 BigFloat log_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

 BigFloat beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

inline BigFloat log_abs_gamma(const BigFloat& z) { return log_abs_gamma(z, BigFloatContext::current()); }
inline BigFloat log_gamma(const BigFloat& z)     { return log_gamma(z, BigFloatContext::current()); }
inline BigFloat gamma(const BigFloat& z)         { return gamma(z, BigFloatContext::current()); }
inline BigFloat factorial(const BigFloat& x)     { return factorial(x, BigFloatContext::current()); }
inline int      gamma_sign(const BigFloat& z)    { return gamma_sign(z, BigFloatContext::current()); }
inline BigFloat log_abs_beta(const BigFloat& a, const BigFloat& b) { return log_abs_beta(a, b, BigFloatContext::current()); }
inline BigFloat log_beta(const BigFloat& a, const BigFloat& b)     { return log_beta(a, b, BigFloatContext::current()); }
inline BigFloat beta(const BigFloat& a, const BigFloat& b)         { return beta(a, b, BigFloatContext::current()); }

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_GAMMA_HPP