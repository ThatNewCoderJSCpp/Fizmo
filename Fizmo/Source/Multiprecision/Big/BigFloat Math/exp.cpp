#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace detail {

bool ziv_round_safe(const BigFloat& v, std::size_t w, std::size_t err, std::size_t prec) {
    if (!v.is_finite() || v.is_zero()) return false;
    BigUInt           m = v.significand();
    const std::size_t L = m.bit_length();
    std::size_t     len = L;
    if (L < w) { m.shift_left_mutable(w - L); len = w; }
    else       { err += L - w; }
    if (err + prec + 2 > len) return false;
    return constants::bfdetail::round_is_safe(m, prec, err);
}

BigFloat exp_taylor_small(const BigFloat& r, const BigFloatContext& wc, std::size_t splits, std::size_t& nterms) {
    const std::size_t p    = wc.precision;
    const BigFloat    y    = r.scaled_pow2(-static_cast<std::int64_t>(splits));
    BigFloat          term = BigFloat::one();
    BigFloat          sum  = BigFloat::one();
    std::size_t       n    = 1;

    for (;; ++n) {
        term = BigFloat::mul(term, y, wc);
        term = BigFloat::div(term, BigFloat(static_cast<std::uint64_t>(n)), wc);
        sum  = BigFloat::add(sum, term, wc);
        if (term.is_zero() || term.get_exp_base2() < -static_cast<std::int64_t>(p + 8)) break;
        if (n > p * 2 + 64) break;                      
    }

    nterms = n;
    for (std::size_t i = 0; i < splits; ++i) sum = BigFloat::mul(sum, sum, wc);
    return sum;
}

BigFloat exp_finite(const BigFloat& x, const BigFloatContext& ctx) {
    const std::size_t  p  = ctx.precision;
    const std::int64_t ex = x.get_exp_base2();
    if (ex >= 50) return x.signbit() ? BigFloat::zero() : BigFloat::infinity();
    if (ex < -static_cast<std::int64_t>(p + 4)) return BigFloat::add(BigFloat::one(), x, ctx);
    const std::size_t     ib = (ex > 0) ? static_cast<std::size_t>(ex) : 0;
    const BigFloatContext kc(ib + 64, RoundingMode::nearest_even);
    const BigFloat        q  = BigFloat::mul(x, constants::inv_ln2(kc), kc);
    const BigFloat        hq = BigFloat::add(q, BigFloat(BigUInt::one(), q.signbit(), -1), kc);
    const BigInt          k  = hq.get_integer_part();           
    if (k.is_nan() || k.is_undefined()) return BigFloat::undefined();
    const std::uint64_t   ak = k.magnitude().is_zero() ? 0 : k.get_lowest_bits();
    const std::int64_t    ik = k.is_negative() ? -static_cast<std::int64_t>(ak) : static_cast<std::int64_t>(ak);
    const BigFloat        kb(k);
    const std::size_t     kbits = exp_bits_u64(ak);
    std::size_t guard = 40 + exp_splits(p) + exp_bits_u64(p);

    for (;;) {
        const std::size_t     w  = p + guard;
        const BigFloatContext wc(w, RoundingMode::nearest_even);
        BigFloat r = x;

        if (ak != 0) {
            const BigFloatContext rc(w + kbits + 8, RoundingMode::nearest_even);
            r = BigFloat::sub(x, BigFloat::mul(kb, constants::ln2(rc), rc), rc);
        }

        const std::size_t s = exp_splits(w);
        std::size_t       n = 0;
        const BigFloat    v = exp_taylor_small(r, wc, s, n).scaled_pow2(ik);
        if (!v.is_finite() || v.is_zero()) return v;            
        const std::size_t err = s + exp_bits_u64(static_cast<std::uint64_t>(n) + 4);
        if (ziv_round_safe(v, w, err, p)) return v.rounded(ctx);
        if (exp_guard_exhausted(p, guard)) return v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace detail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat exp(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.is_negative() ? BigFloat::zero() : BigFloat::infinity();
    if (x.is_zero())      return BigFloat::one();
    return detail::exp_finite(x, ctx);
}

BigFloat exp(const BigInt& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    return exp(BigFloat(x), ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
