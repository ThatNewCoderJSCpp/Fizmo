#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace hmdetail {

void hm_split(std::uint64_t a, std::uint64_t b, BigUInt& T, BigUInt& Q) {
    if (b - a <= 32) {
        T = BigUInt::zero();
        Q = BigUInt::one();

        for (std::uint64_t k = a; k < b; ++k) {
            T.mul_small_mutable(k);
            T.add_mutable(Q);
            Q.mul_small_mutable(k);
        }

        return;
    }

    const std::uint64_t m = a + (b - a) / 2;
    BigUInt TL, QL, TR, QR;
    hm_split(a, m, TL, QL);
    hm_split(m, b, TR, QR);
    T = TL * QR;
    T.add_mutable(TR * QL);
    Q = QL * QR;
}

bool hm_exact_ok(std::uint64_t n, std::size_t prec) {
    if (n <= 64) return true;
    const double bits = std::lgamma(static_cast<double>(n) + 1.0) / 0.6931471805599453;   
    return bits <= 8.0 * static_cast<double>(prec) + 4096.0 && bits + 256.0 < static_cast<double>(BigUInt::max_bits);
}

BigFloat hm_exact(std::uint64_t n, const BigFloatContext& ctx) {
    if (n == 0) return BigFloat::zero();
    BigUInt T, Q;
    hm_split(1, n + 1, T, Q);
    if (T.is_undefined() || Q.is_undefined()) return BigFloat::undefined();
    return BigFloat::div(BigFloat(T), BigFloat(Q), ctx);                 
}

bool hm_as_u64(const BigFloat& x, std::uint64_t& n) {
    if (!x.is_finite() || x.signbit()) return false;
    if (x.is_zero()) { n = 0; return true; }
    const long long tz = x.significand().count_trailing_zeros();
    if (x.exponent() + static_cast<std::int64_t>(tz) < 0) return false;
    if (x.get_exp_base2() > 62) return false;
    n = x.get_integer_part().get_lowest_bits();
    return true;
}

BigFloat hm_plus_one(const BigFloat& x) {                          
    const std::int64_t e   = x.get_exp_base2();
    const std::int64_t top = (e > 0) ? e : 0;
    const std::int64_t lsb = (x.exponent() < 0) ? x.exponent() : 0;
    const std::size_t  p   = BigFloatContext::clamp_precision(static_cast<std::size_t>(top - lsb) + 3);
    return BigFloat::add(x, BigFloat::one(), BigFloatContext(p, RoundingMode::nearest_even));
}

std::size_t hm_cancel(const BigFloat& a, const BigFloat& b, const BigFloat& r) {
    std::int64_t       hi = a.get_exp_base2();
    const std::int64_t eb = b.get_exp_base2();
    if (hi == BigFloat::exp_none || (eb != BigFloat::exp_none && eb > hi)) hi = eb;
    const std::int64_t er = r.get_exp_base2();
    if (hi == BigFloat::exp_none || er == BigFloat::exp_none) return 0;
    return (hi > er) ? static_cast<std::size_t>(hi - er) : 0;
}

bool hm_safe(const BigFloat& v, std::size_t want, std::size_t lost, std::size_t prec) {
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                        
    if (lost + 4 >= sig.bit_length()) return false;
    return constants::bfdetail::round_is_safe(sig, prec, lost + 2);
}

} // namespace hmdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat harmonic(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? BigFloat::undefined() : x;
    if (x.is_zero())      return x;
    std::uint64_t n = 0;
    if (hmdetail::hm_as_u64(x, n) && hmdetail::hm_exact_ok(n, ctx.precision)) return hmdetail::hm_exact(n, ctx);
    BigFloat    xp1;
    bool        have_xp1 = false;
    std::size_t guard    = 32;

    for (;;) {
        const std::size_t     want = BigFloatContext::clamp_precision(ctx.precision + guard);
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        BigFloat    v;
        std::size_t lost = 3;

        if (hmdetail::hm_tiny(x, want)) {
            v = BigFloat::mul(constants::zeta2(wc), x, wc);
        } else {
            if (!have_xp1) { xp1 = hmdetail::hm_plus_one(x); have_xp1 = true; }
            const BigFloat ps = digamma(xp1, wc);
            if (!ps.is_finite()) return ps;
            const BigFloat g = constants::euler_mascheroni(wc);
            v     = BigFloat::add(ps, g, wc);
            lost += hmdetail::hm_cancel(ps, g, v);
        }

        if (!v.is_zero() && hmdetail::hm_safe(v, want, lost, ctx.precision)) return v.rounded(ctx);
        if (hmdetail::hm_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
