#ifndef FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP
#define FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP

#include "trig.hpp"

#include <cmath>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline std::size_t iv_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

std::size_t iv_halvings(std::size_t w) noexcept;

std::size_t iv_err_of(const BigFloat& v, std::size_t want, std::size_t lost);

inline bool iv_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

inline BigFloat iv_half() { return BigFloat(BigUInt::one(), false, -1); }

BigFloat atan_core(const BigFloat& u0, const BigFloatContext& wc, std::size_t& lost);

BigFloat atan_raw(const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

enum class iv_sel : std::uint8_t { asin_v, acos_v, atan_v, acot_v, asec_v, acsc_v };

BigFloat iv_core(iv_sel sel, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

BigFloat iv_dispatch(iv_sel sel, const BigFloat& x, const BigFloatContext& ctx);

BigFloat at2_core(const BigFloat& y, const BigFloat& x, const BigFloatContext& wc, std::size_t& lost);

} // namespace detail

BigFloat arcsin(const BigFloat& x, const BigFloatContext& ctx);

BigFloat arccos(const BigFloat& x, const BigFloatContext& ctx);

BigFloat arctan(const BigFloat& x, const BigFloatContext& ctx);

BigFloat arccot(const BigFloat& x, const BigFloatContext& ctx);

BigFloat arcsec(const BigFloat& x, const BigFloatContext& ctx);

BigFloat arccsc(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(arcsin)
FIZMO_MP_TRIG_FORWARD(arccos)
FIZMO_MP_TRIG_FORWARD(arctan)
FIZMO_MP_TRIG_FORWARD(arccot)
FIZMO_MP_TRIG_FORWARD(arcsec)
FIZMO_MP_TRIG_FORWARD(arccsc)

BigFloat atan2(const BigFloat& y, const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat atan2(const BigFloat& y, const BigFloat& x) {
    return atan2(y, x, BigFloatContext::current());
}

#define FIZMO_MP_ATAN2_FORWARD()                                                                           \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value             \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat atan2(const A& y, const B& x, const BigFloatContext& c) { return atan2(BigFloat(y), BigFloat(x), c); } \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value             \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat atan2(const A& y, const B& x) { return atan2(BigFloat(y), BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_ATAN2_FORWARD()

#undef FIZMO_MP_ATAN2_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INVERSE_TRIGONOMETRIC_HPP