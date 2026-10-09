#ifndef FIZMO_MULTIPRECISION_BIG_LOGARITHMS_HPP
#define FIZMO_MULTIPRECISION_BIG_LOGARITHMS_HPP

#include "sqrt_cbrt.hpp"
#include "big_float_consts.hpp"

#include <cmath>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace detail {

struct fixed_approx {
    BigUInt      mag;
    bool         neg      = false;
    std::int64_t scale    = 0;
    std::size_t  err_bits = 0;
    bool         capped   = false;   
};

inline std::size_t ln_bit_length_u64(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline std::uint64_t ln_abs_u64(std::int64_t v) noexcept {
    return v < 0 ? (~static_cast<std::uint64_t>(v) + 1u) : static_cast<std::uint64_t>(v);
}

void ln_shift(BigUInt& v, std::int64_t s);

inline bool is_exactly_one(const BigFloat& x) {
    return x.is_finite() && !x.signbit() && x.significand().is_one() && x.exponent() == 0;
}

fixed_approx ln_fixed(const BigFloat& x, std::size_t bits);

BigFloat ln_finite(const BigFloat& x, const BigFloatContext& ctx);

BigUInt log_pow5(std::uint64_t n);

// x == 2^k exactly  (significand is always odd, so a power of two has significand 1)
bool log2_exact(const BigFloat& x, std::int64_t& k);

// x == 10^k exactly; only k >= 0 can be exact in binary, since 10^-k is not a dyadic rational
bool log10_exact(const BigFloat& x, std::int64_t& k);

// exact test for b^n == x, using integer arithmetic only; false whenever it is not cheaply decidable
bool log_pow_matches(const BigFloat& b, std::int64_t n, const BigFloat& x);

// nearest integer to v, but only when v sits within ~2^-(prec/2) of it
bool log_nearest_int(const BigFloat& v, const BigFloatContext& wc, std::int64_t& n);

inline bool log_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 2;
}

enum class ln_kind : std::uint8_t {
    nan_v,        // v < 0, or v is nan
    undef_v,      // v is undefined
    neg_inf,      // v == 0
    neg_finite,   // 0 < v < 1
    pos_zero,     // v == 1
    pos_finite,   // v > 1
    pos_inf       // v == inf
};

ln_kind classify_ln(const BigFloat& v);

bool log_special(ln_kind a, ln_kind b, BigFloat& out);

} // namespace detail

BigFloat ln(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat ln(const BigFloat& x) { return ln(x, BigFloatContext::current()); }

inline BigFloat ln(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return ln(BigFloat(x), ctx);
}

inline BigFloat ln(const BigUInt& x) { return ln(x, BigFloatContext::current()); }

BigFloat ln(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat ln(const BigInt& x) { return ln(x, BigFloatContext::current()); }

namespace detail {
    BigFloat log_scaled(const BigFloat& x, BigFloat (*inv_const)(const BigFloatContext&), const BigFloatContext& ctx);

    BigFloat log_ratio(const BigFloat& x, const BigFloat& b, const BigFloatContext& ctx);
}

BigFloat log2(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat log2(const BigFloat& x) { return log2(x, BigFloatContext::current()); }

inline BigFloat log2(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return log2(BigFloat(x), ctx);
}

inline BigFloat log2(const BigUInt& x) { return log2(x, BigFloatContext::current()); }

BigFloat log2(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat log2(const BigInt& x) { return log2(x, BigFloatContext::current()); }

BigFloat log10(const BigFloat& x, const BigFloatContext& ctx);

inline BigFloat log10(const BigFloat& x) { return log10(x, BigFloatContext::current()); }

inline BigFloat log10(const BigUInt& x, const BigFloatContext& ctx) {
    if (x.is_undefined()) return BigFloat::undefined();
    return log10(BigFloat(x), ctx);
}

inline BigFloat log10(const BigUInt& x) { return log10(x, BigFloatContext::current()); }

BigFloat log10(const BigInt& x, const BigFloatContext& ctx);

inline BigFloat log10(const BigInt& x) { return log10(x, BigFloatContext::current()); }

BigFloat log(const BigFloat& x, const BigFloat& base, const BigFloatContext& ctx);

inline BigFloat log(const BigFloat& x, const BigFloat& base) {
    return log(x, base, BigFloatContext::current());
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_LOGARITHMS_HPP