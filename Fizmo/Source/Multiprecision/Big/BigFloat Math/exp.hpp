#ifndef FIZMO_MULTIPRECISION_BIG_EXP_HPP
#define FIZMO_MULTIPRECISION_BIG_EXP_HPP

#include "abs_min_max.hpp"
#include "big_float_consts.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

 bool ziv_round_safe(const BigFloat& v, std::size_t w, std::size_t err, std::size_t prec);

inline std::size_t exp_splits(std::size_t p) noexcept {
    std::size_t s = 4, q = 1;
    while (q < p) { q <<= 1; ++s; }
    return s;
}

inline std::size_t exp_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

 BigFloat exp_taylor_small(const BigFloat& r, const BigFloatContext& wc, std::size_t splits, std::size_t& nterms);

inline BigFloat exp_taylor_small(const BigFloat& x, const BigFloatContext& ctx) {
    std::size_t n = 0;
    return exp_taylor_small(x, ctx, exp_splits(ctx.precision), n);
}

inline bool exp_guard_exhausted(std::size_t prec, std::size_t guard) noexcept {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 2;
}

 BigFloat exp_finite(const BigFloat& x, const BigFloatContext& ctx);

} // namespace detail

 BigFloat exp(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat exp(const BigFloat& x) {
    return exp(x, BigFloatContext::current());
}

inline BigFloat exp(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return exp(BigFloat(x), ctx);
}

inline BigFloat exp(const BigUInt& x) {
    return exp(x, BigFloatContext::current());
}

 BigFloat exp(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat exp(const BigInt& x) {
    return exp(x, BigFloatContext::current());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_EXP_HPP