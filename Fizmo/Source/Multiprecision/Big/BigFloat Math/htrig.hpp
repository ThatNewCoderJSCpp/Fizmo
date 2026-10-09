#ifndef FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP
#define FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP

#include "exp.hpp"
#include "sqrt_cbrt.hpp"
#include "trig.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline std::size_t hy_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::size_t hy_splits(std::size_t p) noexcept {
    std::size_t s = 3, q = 1;
    while (q < p) { q <<= 1; ++s; }
    return s;
}

 void hy_small(const BigFloat& ax, const BigFloatContext& wc, BigFloat& sh, BigFloat& ch);

 void hy_raw(const BigFloat& ax, std::size_t want, BigFloat& sh, BigFloat& ch);

enum class hyp_sel : std::uint8_t { sinh_v, cosh_v, tanh_v, coth_v, sech_v, csch_v };

inline bool hy_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

 bool hy_saturates(const BigFloat& ax, std::size_t prec);

 BigFloat hy_dispatch(const BigFloat& x, hyp_sel sel, const BigFloatContext& ctx);

} // namespace detail

 BigFloat sinh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat cosh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat tanh(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat coth(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat sech(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat csch(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(sinh)
FIZMO_MP_TRIG_FORWARD(cosh)
FIZMO_MP_TRIG_FORWARD(tanh)
FIZMO_MP_TRIG_FORWARD(coth)
FIZMO_MP_TRIG_FORWARD(sech)
FIZMO_MP_TRIG_FORWARD(csch)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_HYPERBOLIC_HPP