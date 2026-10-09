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
    bm_main_t();
};

void bm_main_split(std::uint64_t a, std::uint64_t b, std::uint64_t n2, bool need_P, bm_main_t& r);

struct bm_corr_t {
    BigUInt P, Q, T;
    bm_corr_t() : P(BigUInt::zero()), Q(BigUInt::zero()), T(BigUInt::zero()) {}
};

void bm_corr_split(std::uint64_t a, std::uint64_t b, std::uint64_t n2, bool need_P, bm_corr_t& r);

BigFloat bm_gamma(std::size_t wp, std::size_t& lost);

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

BigFloat euler_mascheroni(const BigFloatContext& ctx);

inline BigFloat euler_mascheroni() { return euler_mascheroni(BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_EULER_GAMMA_HPP