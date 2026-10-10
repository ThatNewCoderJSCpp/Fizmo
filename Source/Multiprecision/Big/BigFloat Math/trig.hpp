#ifndef FIZMO_MULTIPRECISION_BIG_TRIGONOMETRIC_HPP
#define FIZMO_MULTIPRECISION_BIG_TRIGONOMETRIC_HPP

#include "sqrt_cbrt.hpp"
#include "big_float_consts.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

static const std::size_t tg_reduce_cap = std::size_t(1) << 16;

inline std::size_t tg_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::size_t tg_splits(std::size_t p) noexcept {
    std::size_t s = 3, q = 1;
    while (q < p) { q <<= 1; ++s; }
    return s;
}

void tg_sincos_small(const BigFloat& r, const BigFloatContext& wc, BigFloat& s, BigFloat& c);

bool tg_sincos_raw(const BigFloat& x, std::size_t want, BigFloat& sx, BigFloat& cx);

enum class trig_sel : std::uint8_t { sin_v, cos_v, tan_v, cot_v, sec_v, csc_v };

inline bool tg_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

BigFloat tg_dispatch(const BigFloat& x, trig_sel sel, const BigFloatContext& ctx);

BigFloat tg_just_under_one(const BigFloatContext& ctx);

bool tg_negligible(const BigFloat& x, std::size_t prec);

std::size_t tg_err_of(const BigFloat& v, std::size_t want);

BigFloat sinc_finite(const BigFloat& x, const BigFloatContext& ctx);

} // namespace detail

#define FIZMO_MP_TRIG_FORWARD(FN)                                                                  \
    inline BigFloat FN(const BigFloat& x) { return FN(x, BigFloatContext::current()); }            \
    inline BigFloat FN(const BigUInt& x, const BigFloatContext& c) {                               \
        if (x.is_undefined()) return BigFloat::undefined();                                        \
        return FN(BigFloat(x), c);                                                                 \
    }                                                                                              \
    inline BigFloat FN(const BigUInt& x) { return FN(x, BigFloatContext::current()); }             \
    inline BigFloat FN(const BigInt& x, const BigFloatContext& c) {                                \
        if (x.is_nan())       return BigFloat::nan();                                              \
        if (x.is_undefined()) return BigFloat::undefined();                                        \
        return FN(BigFloat(x), c);                                                                 \
    }                                                                                              \
    inline BigFloat FN(const BigInt& x) { return FN(x, BigFloatContext::current()); }

BigFloat sin(const BigFloat& x, const BigFloatContext& ctx);

namespace detail {
    void tg_fold_half(const BigFloat& x, BigFloat& f, bool& flip);

    BigFloat normalized_sinc_finite(const BigFloat& x, const BigFloatContext& ctx);
}

BigFloat cos(const BigFloat& x, const BigFloatContext& ctx);

BigFloat tan(const BigFloat& x, const BigFloatContext& ctx);

BigFloat cot(const BigFloat& x, const BigFloatContext& ctx);

BigFloat sec(const BigFloat& x, const BigFloatContext& ctx);

BigFloat csc(const BigFloat& x, const BigFloatContext& ctx);

BigFloat sinc(const BigFloat& x, const BigFloatContext& ctx);

BigFloat normalized_sinc(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(sin)
FIZMO_MP_TRIG_FORWARD(cos)
FIZMO_MP_TRIG_FORWARD(tan)
FIZMO_MP_TRIG_FORWARD(cot)
FIZMO_MP_TRIG_FORWARD(sec)
FIZMO_MP_TRIG_FORWARD(csc)
FIZMO_MP_TRIG_FORWARD(sinc)
FIZMO_MP_TRIG_FORWARD(normalized_sinc)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_TRIGONOMETRIC_HPP