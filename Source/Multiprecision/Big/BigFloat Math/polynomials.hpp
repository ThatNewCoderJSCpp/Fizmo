#ifndef FIZMO_MULTIPRECISION_BIG_POLYNOMIALS_HPP
#define FIZMO_MULTIPRECISION_BIG_POLYNOMIALS_HPP

#include "big_float_consts.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace pldetail {

struct pl_val {
    BigFloat v;
    double   le;
};

inline double pl_ninf() noexcept { return -std::numeric_limits<double>::infinity(); }
inline pl_val pl_exact(const BigFloat& v) { return pl_val{v, pl_ninf()}; }

inline int pl_msb(std::uint64_t n) noexcept {
    int i = -1;
    while (n != 0) { ++i; n >>= 1; }
    return i;
}

inline std::uint64_t pl_abs(std::int64_t n) noexcept {
    return (n < 0) ? (static_cast<std::uint64_t>(-(n + 1)) + 1u) : static_cast<std::uint64_t>(n);
}

inline std::int64_t pl_top(const BigFloat& v) {
    return v.exponent() + static_cast<std::int64_t>(v.significand().bit_length()) - 1;
}

inline double pl_mag(const BigFloat& v) {
    return v.is_zero() ? pl_ninf() : static_cast<double>(pl_top(v) + 1);
}

double pl_lsum(double a, double b);

bool pl_mul_exact(const BigFloat& a, const BigFloat& b, std::size_t w);

bool pl_add_exact(const BigFloat& a, const BigFloat& b, std::size_t w);

pl_val pl_mul(const pl_val& a, const pl_val& b, const BigFloatContext& wc);

pl_val pl_addsub(const pl_val& a, const pl_val& b, bool sub, const BigFloatContext& wc);

inline pl_val pl_add(const pl_val& a, const pl_val& b, const BigFloatContext& wc) { return pl_addsub(a, b, false, wc); }
inline pl_val pl_sub(const pl_val& a, const pl_val& b, const BigFloatContext& wc) { return pl_addsub(a, b, true,  wc); }

inline pl_val pl_add_int(const pl_val& a, std::int64_t c, const BigFloatContext& wc) {
    return pl_addsub(a, pl_exact(BigFloat(c)), false, wc);
}

inline pl_val pl_scale2(const pl_val& a, std::int64_t k) {
    return pl_val{a.v.scaled_pow2(k), a.le + static_cast<double>(k)};
}

void pl_lucas_u(std::uint64_t n, const pl_val& P, bool q_neg, const BigFloatContext& wc, pl_val& A, pl_val& B);

pl_val pl_lucas_v(std::uint64_t n, const pl_val& P, bool q_neg, const BigFloatContext& wc);

inline bool pl_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 8192 || prec + guard >= BigFloatContext::max_prec / 4;
}

bool pl_safe(const BigFloat& v, std::size_t want, double le, std::size_t prec);

template <typename Eval>
inline BigFloat pl_drive(std::uint64_t n, bool flip, const BigFloatContext& ctx, Eval eval) {
    std::size_t guard = 32 + 2 * static_cast<std::size_t>(pl_msb(n) + 1);

    for (;;) {
        const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const pl_val          r = eval(wc);
        const BigFloat        v = flip ? -r.v : r.v;
        if (!v.is_finite())    return v;
        if (r.le == pl_ninf()) return v.rounded(ctx);
        if (!v.is_zero() && pl_safe(v, want, r.le, ctx.precision)) return v.rounded(ctx);
        if (pl_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

bool pl_special(const BigFloat& x, std::uint64_t deg, std::uint64_t c0, bool flip, const BigFloatContext& ctx, BigFloat& out);

inline BigInt pc_int(std::int64_t v) { return BigInt(v); }

std::vector<BigInt> pc_step(const std::vector<BigInt>& cur, const std::vector<BigInt>& prev, bool two_x, bool q_neg);

std::vector<BigInt> pc_run(std::size_t n, std::vector<BigInt> y0, std::vector<BigInt> y1, bool two_x, bool q_neg);

} // namespace pldetail

BigFloat chebyshev_t(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

BigFloat chebyshev_u(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

BigFloat chebyshev_v(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

BigFloat chebyshev_w(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

BigFloat fibonacci_polynomial(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

BigFloat lucas_polynomial(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx);

#define FIZMO_MP_POLY_FORWARD(FN)                                                                          \
    inline BigFloat FN(const BigFloat& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); } \
    inline BigFloat FN(const BigInt& x, std::int64_t n, const BigFloatContext& c) {                        \
        if (x.is_nan())       return BigFloat::nan();                                                      \
        if (x.is_undefined()) return BigFloat::undefined();                                                \
        return FN(BigFloat(x), n, c);                                                                      \
    }                                                                                                      \
    inline BigFloat FN(const BigInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }   \
    inline BigFloat FN(const BigUInt& x, std::int64_t n, const BigFloatContext& c) {                       \
        if (x.is_undefined()) return BigFloat::undefined();                                                \
        return FN(BigFloat(x), n, c);                                                                      \
    }                                                                                                      \
    inline BigFloat FN(const BigUInt& x, std::int64_t n) { return FN(x, n, BigFloatContext::current()); }  \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>            \
    inline BigFloat FN(T x, std::int64_t n, const BigFloatContext& c) { return FN(BigFloat(x), n, c); }    \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>            \
    inline BigFloat FN(T x, std::int64_t n) { return FN(BigFloat(x), n, BigFloatContext::current()); }

FIZMO_MP_POLY_FORWARD(chebyshev_t)
FIZMO_MP_POLY_FORWARD(chebyshev_u)
FIZMO_MP_POLY_FORWARD(chebyshev_v)
FIZMO_MP_POLY_FORWARD(chebyshev_w)
FIZMO_MP_POLY_FORWARD(fibonacci_polynomial)
FIZMO_MP_POLY_FORWARD(lucas_polynomial)

#undef FIZMO_MP_POLY_FORWARD

std::vector<BigInt> chebyshev_t_coefficients(std::size_t n);

std::vector<BigInt> chebyshev_u_coefficients(std::size_t n);

std::vector<BigInt> chebyshev_v_coefficients(std::size_t n);

std::vector<BigInt> chebyshev_w_coefficients(std::size_t n);

inline std::vector<BigInt> fibonacci_polynomial_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(0)}, {pc_int(1)}, false, true);
}

std::vector<BigInt> lucas_polynomial_coefficients(std::size_t n);

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_POLYNOMIALS_HPP