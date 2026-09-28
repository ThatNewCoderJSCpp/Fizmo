#ifndef FIZMO_MULTIPRECISION_BIG_EULER_GAMMA_HPP
#define FIZMO_MULTIPRECISION_BIG_EULER_GAMMA_HPP

#include "logarithms.hpp"

#include <cmath>
#include <cstdint>

namespace fizmo {
namespace multiprecision {
namespace constants {

namespace detail {

// n <= cap keeps the correction leaves' (2k-1)^3 and 32*k*n^2 inside uint64 (64 n^3 < 2^64).
static const std::uint64_t bm_n_cap = 650000;

struct bm_main_t {
    BigUInt P, Q, T, D, C, V;
    bm_main_t() : P(BigUInt::zero()), Q(BigUInt::zero()), T(BigUInt::zero()), D(BigUInt::zero()), C(BigUInt::zero()), V(BigUInt::zero()) {}
};

inline void bm_main_split(std::uint64_t a, std::uint64_t b, std::uint64_t n2, bool need_P, bm_main_t& r) {
    if (b - a == 1) {
        r.P = BigUInt(n2);
        r.Q = BigUInt(a * a);
        r.T = r.P;
        r.D = BigUInt(a);
        r.C = BigUInt::one();
        r.V = r.P;
        return;
    }

    const std::uint64_t m = a + (b - a) / 2;
    bm_main_t L, R;
    bm_main_split(a, m, n2, true,   L);        
    bm_main_split(m, b, n2, need_P, R);        
    const BigUInt PLTR = L.P * R.T;
    r.T = L.T * R.Q;
    r.T.add_mutable(PLTR);
    BigUInt inner = R.Q * L.V;
    inner.add_mutable(L.C * PLTR);
    r.V = R.D * inner;
    r.V.add_mutable(L.D * (L.P * R.V));
    r.C = L.C * R.D;
    r.C.add_mutable(R.C * L.D);
    r.D = L.D * R.D;
    r.Q = r.D * r.D;                          
    if (need_P) r.P = L.P * R.P;
}

struct bm_corr_t {
    BigUInt P, Q, T;
    bm_corr_t() : P(BigUInt::zero()), Q(BigUInt::zero()), T(BigUInt::zero()) {}
};

inline void bm_corr_split(std::uint64_t a, std::uint64_t b, std::uint64_t n2, bool need_P, bm_corr_t& r) {
    if (b - a == 1) {
        const std::uint64_t o = 2 * a - 1;
        r.P = BigUInt(o * o * o);
        r.Q = BigUInt(32 * a * n2);
        r.T = r.P;
        return;
    }

    const std::uint64_t m = a + (b - a) / 2;
    bm_corr_t L, R;
    bm_corr_split(a, m, n2, true,   L);
    bm_corr_split(m, b, n2, need_P, R);
    r.T = L.T * R.Q;
    r.T.add_mutable(L.P * R.T);
    r.Q = L.Q * R.Q;
    if (need_P) r.P = L.P * R.P;
}

inline BigFloat bm_gamma(std::size_t wp, std::size_t& lost) {
    const BigFloatContext wc(BigFloatContext::clamp_precision(wp), RoundingMode::nearest_even);
    const double          p = static_cast<double>(wc.precision);
    const std::uint64_t n = static_cast<std::uint64_t>(std::ceil((p + 10.0) * 0.0866433975699932)) + 1;
    if (n > bm_n_cap) return BigFloat::undefined();
    const std::uint64_t K = static_cast<std::uint64_t>(std::ceil(4.970625759544232 * static_cast<double>(n))) + 16;
    const double lgK = std::lgamma(static_cast<double>(K) + 1.0) / 0.6931471805599453;
    if (3.0 * lgK + 3.0 * static_cast<double>(n) + 256.0 > static_cast<double>(BigUInt::max_bits)) return BigFloat::undefined();
    const std::uint64_t n2 = n * n;
    bm_main_t M;
    bm_main_split(1, K + 1, n2, false, M);    
    bm_corr_t R;
    bm_corr_split(1, 2 * n + 1, n2, false, R); 
    BigUInt Iq = M.Q;  Iq.add_mutable(M.T);
    BigUInt Wq = R.Q;  Wq.add_mutable(R.T);
    const BigFloat If   = BigFloat(Iq).rounded(wc);
    BigFloat       X    = BigFloat::div(BigFloat(M.V).rounded(wc), If, wc);
    X                   = BigFloat::div(X, BigFloat(M.D).rounded(wc), wc);
    const BigFloat invI = BigFloat::div(BigFloat(M.Q).rounded(wc), If, wc);
    BigFloat W = BigFloat::div(BigFloat(Wq).rounded(wc), BigFloat(R.Q).rounded(wc), wc);
    W = BigFloat::div(W, BigFloat(static_cast<std::uint64_t>(4 * n)), wc);
    const BigFloat corr = BigFloat::mul(W, BigFloat::mul(invI, invI, wc), wc);
    const BigFloat lnn  = math::ln(BigFloat(n), wc);
    const BigFloat v    = BigFloat::sub(BigFloat::sub(X, corr, wc), lnn, wc);
    const std::int64_t ex = X.get_exp_base2();
    const std::int64_t ev = v.get_exp_base2();
    lost = ((ex != BigFloat::exp_none && ev != BigFloat::exp_none && ex > ev) ? static_cast<std::size_t>(ex - ev) : 0) + 8;
    return v;
}

inline bool bm_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

struct bm_cache_t {
    BigFloat    v;
    std::size_t err  = 0;   
    std::size_t good = 0;   
    bool        valid = false;
};

inline bm_cache_t& bm_cache() {
    static thread_local bm_cache_t c;
    return c;
}

} // namespace detail

inline BigFloat euler_mascheroni(const BigFloatContext& ctx) {
    detail::bm_cache_t& c = detail::bm_cache();

    if (
        c.valid && 
        c.good >= ctx.precision + 8 &&
        bfdetail::round_is_safe(c.v.significand(), ctx.precision, c.err + 2)
    ) {
        return c.v.rounded(ctx);
    }

    std::size_t guard = 32;

    for (;;) {
        const std::size_t wp   = ctx.precision + guard;
        std::size_t       lost = 0;
        const BigFloat    v    = detail::bm_gamma(wp, lost);
        if (!v.is_finite()) return v;
        const std::size_t good = (wp > lost) ? (wp - lost) : 1;
        const std::size_t L    = v.significand().bit_length();
        const std::size_t err  = (L > good) ? (L - good) : 0;
        if (!c.valid || good > c.good) { c.v = v; c.err = err; c.good = good; c.valid = true; }
        if (bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (detail::bm_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

inline BigFloat euler_mascheroni() { return euler_mascheroni(BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_EULER_GAMMA_HPP