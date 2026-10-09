#ifndef FIZMO_MULTIPRECISION_BIG_ERROR_FUNCTIONS_HPP
#define FIZMO_MULTIPRECISION_BIG_ERROR_FUNCTIONS_HPP

#include "exp.hpp"
#include "logarithms.hpp"
#include "sqrt_cbrt.hpp"
#include "big_float_consts.hpp"

#include <cmath>
#include <limits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

inline std::size_t ef_bits_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline bool ef_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

std::size_t ef_err_of(const BigFloat& v, std::size_t want, std::size_t lost);

long double ef_to_ld(const BigFloat& v);

inline long double ef_x2_ld(const BigFloat& ax) { const long double d = ef_to_ld(ax); return d * d; }

std::size_t ef_x2_log2e(const BigFloat& ax);

std::size_t ef_x2_bits(const BigFloat& ax);

bool ef_asymptotic(const BigFloat& ax, std::size_t want);

bool ef_tiny(const BigFloat& x, std::size_t prec);

BigFloat ef_just_under_one(const BigFloatContext& ctx);

BigFloat ef_just_over_one(const BigFloatContext& ctx);

BigFloat ef_erf_series(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok);

BigFloat ef_erfcx_asym(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost);

BigFloat ef_erfix_asym(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost);

BigFloat ef_erfi_series(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok);

BigFloat ef_erfc_pos(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok);

BigFloat ef_erfi_pos(const BigFloat& ax, const BigFloatContext& wc, std::size_t& lost, bool& ok);

enum class ef_kind : std::uint8_t { erf_v, erfc_v, erfi_v };

BigFloat ef_dispatch(ef_kind k, const BigFloat& ax, const BigFloatContext& ctx);

} // namespace detail

BigFloat erf(const BigFloat& x, const BigFloatContext& ctx);

BigFloat erfc(const BigFloat& x, const BigFloatContext& ctx);

BigFloat erfi(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat erf(const BigFloat& x)  { return erf(x,  BigFloatContext::current()); }
inline BigFloat erfc(const BigFloat& x) { return erfc(x, BigFloatContext::current()); }
inline BigFloat erfi(const BigFloat& x) { return erfi(x, BigFloatContext::current()); }

namespace detail {

enum class ef_solve : std::uint8_t { erf_s, erfc_s, erfi_s };

BigFloat ef_newton_step(ef_solve k, const BigFloat& x, const BigFloat& target, const BigFloatContext& wc);

BigFloat ef_newton(ef_solve k, const BigFloat& seed, const BigFloat& target, std::size_t bits);

BigFloat ef_seed_erfc(const BigFloat& z);

BigFloat ef_seed_erf(const BigFloat& y);

long double ef_erfi_ld(long double x);

BigFloat ef_seed_erfi(const BigFloat& y);

BigFloat ef_inv_dispatch(ef_solve k, const BigFloat& target, const BigFloat& seed, const BigFloatContext& ctx);

BigFloat ef_exact_sub(const BigFloat& a, const BigFloat& b);

} // namespace detail

BigFloat inv_erf(const BigFloat& y, const BigFloatContext& ctx);

BigFloat inv_erfc(const BigFloat& z, const BigFloatContext& ctx);

BigFloat inv_erfi(const BigFloat& y, const BigFloatContext& ctx);

inline BigFloat inv_erf(const BigFloat& y)  { return inv_erf(y,  BigFloatContext::current()); }
inline BigFloat inv_erfc(const BigFloat& z) { return inv_erfc(z, BigFloatContext::current()); }
inline BigFloat inv_erfi(const BigFloat& y) { return inv_erfi(y, BigFloatContext::current()); }

namespace detail {

bool ef_abs_ge_one(const BigFloat& v);

BigFloat ef_gen_diff(const BigFloat& x, const BigFloat& b, const BigFloatContext& ctx);

} // namespace detail

BigFloat generalized_erf(const BigFloat& x, const BigFloat& base, const BigFloatContext& ctx);

inline BigFloat generalized_erfc(const BigFloat& x, const BigFloat& base, const BigFloatContext& ctx) {
    return -generalized_erf(x, base, ctx);               
}

inline BigFloat generalized_erf(const BigFloat& x, const BigFloat& base) {
    return generalized_erf(x, base, BigFloatContext::current());
}

inline BigFloat generalized_erfc(const BigFloat& x, const BigFloat& base) {
    return generalized_erfc(x, base, BigFloatContext::current());
}

#define FIZMO_MP_ERF_FORWARD(FN)                                                       \
    inline BigFloat FN(const BigUInt& x, const BigFloatContext& c) {                   \
        if (x.is_undefined()) return BigFloat::undefined();                            \
        return FN(BigFloat(x), c);                                                     \
    }                                                                                  \
    inline BigFloat FN(const BigUInt& x) { return FN(x, BigFloatContext::current()); } \
    inline BigFloat FN(const BigInt& x, const BigFloatContext& c) {                    \
        if (x.is_nan())       return BigFloat::nan();                                  \
        if (x.is_undefined()) return BigFloat::undefined();                            \
        return FN(BigFloat(x), c);                                                     \
    }                                                                                  \
    inline BigFloat FN(const BigInt& x) { return FN(x, BigFloatContext::current()); }  \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }   \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_ERF_FORWARD(erf)
FIZMO_MP_ERF_FORWARD(erfc)
FIZMO_MP_ERF_FORWARD(erfi)
FIZMO_MP_ERF_FORWARD(inv_erf)
FIZMO_MP_ERF_FORWARD(inv_erfc)
FIZMO_MP_ERF_FORWARD(inv_erfi)

#undef FIZMO_MP_ERF_FORWARD

#define FIZMO_MP_ERF2_FORWARD(FN)                                                                         \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value            \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat FN(const A& x, const B& b, const BigFloatContext& c) { return FN(BigFloat(x), BigFloat(b), c); } \
    template <typename A, typename B, typename std::enable_if<!std::is_same<A, BigFloat>::value            \
                                                           || !std::is_same<B, BigFloat>::value, int>::type = 0> \
    inline BigFloat FN(const A& x, const B& b) { return FN(BigFloat(x), BigFloat(b), BigFloatContext::current()); }

FIZMO_MP_ERF2_FORWARD(generalized_erf)
FIZMO_MP_ERF2_FORWARD(generalized_erfc)

#undef FIZMO_MP_ERF2_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_ERROR_FUNCTIONS_HPP