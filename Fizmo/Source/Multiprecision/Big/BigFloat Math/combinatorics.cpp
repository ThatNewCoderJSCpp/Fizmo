#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace cbdetail {

bool cb_fits_u64(const BigInt& v, std::uint64_t& out) {
    if (!v.is_finite() || v.is_negative() || v.bit_length() > 64) return false;
    out = v.get_lowest_bits();
    return true;
}

bool cb_int_ok(const BigInt& lo, const BigInt& hi, std::uint64_t n) {        
    const double b = static_cast<double>((std::max)(lo.bit_length(), hi.bit_length()));
    return static_cast<double>(n) * b + 256.0 < static_cast<double>(BigUInt::max_bits);
}

BigInt cb_ap(const BigInt& a, const BigInt& d, std::uint64_t lo, std::uint64_t hi) {
    if (hi <= lo) return BigInt::one();

    if (hi - lo <= cb_leaf) {
        BigInt f = a + d * BigInt(lo);
        BigInt r = f;

        for (std::uint64_t i = lo + 1; i < hi; ++i) {
            f += d;
            r *= f;
        }

        return r;
    }

    const std::uint64_t mid = lo + (hi - lo) / 2;
    return cb_ap(a, d, lo, mid) * cb_ap(a, d, mid, hi);
}

BigInt cb_rise_int(const BigInt& a, std::uint64_t n) {
    if (n == 0) return BigInt::one();
    const BigInt last = a + BigInt(n - 1);

    if (!a.is_positive()) {
        if (!last.is_negative()) return BigInt::zero();                               
        const BigInt r = cb_rise_int(-last, n);                                       
        return (n & 1u) ? -r : r;
    }

    std::uint64_t au = 0;
    if (cb_fits_u64(a, au) && au <= (std::numeric_limits<std::uint64_t>::max)() - n) return BigInt(gmdetail::fact_range(au, au + n));
    return cb_ap(a, BigInt::one(), 0, n);
}

BF cb_gamma_ratio(const std::vector<BF>& num, const std::vector<BF>& den, bool neg, const BFC& ctx) {
    const BFC   pc  = cb_ctx(64);
    double      lsz = hzdetail::hz_ninf();
    for (const BF& z : num) lsz = hzdetail::hz_lsum(lsz, hzdetail::hz_l2(log_abs_gamma(z, pc)));
    for (const BF& z : den) lsz = hzdetail::hz_lsum(lsz, hzdetail::hz_l2(log_abs_gamma(z, pc)));
    const std::size_t E  = (lsz > 0.0 && lsz < 1.0e7) ? static_cast<std::size_t>(std::ceil(lsz)) + 4 : 4;   // bits of the log terms above 1
    const std::size_t nt = num.size() + den.size();

    return hzdetail::hz_drive(32, ctx, [&](std::size_t want) -> zt_val {
        const BFC lc = cb_ctx(want + E);
        BF        L  = BF::zero();
        double    la = hzdetail::hz_ninf();

        for (std::size_t i = 0; i < nt; ++i) {
            const bool top = i < num.size();
            const BF   l   = log_abs_gamma(top ? num[i] : den[i - num.size()], lc);
            if (!l.is_finite()) return zt_val{BF::undefined(), 0.0};
            L  = top ? BF::add(L, l, lc) : BF::sub(L, l, lc);
            la = hzdetail::hz_lsum(la, hzdetail::hz_l2(l));
        }

        const BF v = exp(L, cb_ctx(want));
        if (!v.is_finite() || v.is_zero()) return zt_val{v, 0.0};
        const double eL    = la + std::log2(2.0 * static_cast<double>(nt) + 2.0) - static_cast<double>(want + E);   
        const double units = std::exp2(eL + static_cast<double>(want)) + 2.0;
        return zt_val{neg ? -v : v, std::log2(units) + 1.0};
    });
}

BF cb_gamma_quot(const BF& zt, const BF& zb, const BFC& ctx) {                 
    const int st = gamma_sign(zt, ctx);
    const int sb = gamma_sign(zb, ctx);
    if (st == 3 || sb == 3) return BF::undefined();
    if (st == 2)            return BF::undefined();                                   
    if (sb == 2)            return BF::zero();                                        
    return cb_gamma_ratio({zt}, {zb}, (st == 1) != (sb == 1), ctx);
}

BF cb_rise_float(const BF& x, std::uint64_t n, bool inv, bool binom, bool neg, const BFC& ctx) {
    return hzdetail::hz_drive(32 + zt_bitlen(n), ctx, [&](std::size_t want) -> zt_val {
        const BFC wc = cb_ctx(want);
        BF p = x.rounded(wc);
        for (std::uint64_t i = 1; i < n; ++i) p = BF::mul(p, BF::add(x, BF(i), wc), wc);
        if (binom) p = BF::div(p, BF(gmdetail::fact_range(1, n + 1)).rounded(wc), wc);
        if (inv)   p = p.reciprocal(wc);
        if (neg)   p = -p;
        return zt_val{p, std::log2(2.0 * static_cast<double>(n) + 6.0) + 1.0};
    });
}

BF cb_rising_core(BF x, std::uint64_t n, bool inv, bool binom, const BFC& ctx) {
    bool neg = false;

    if (x.is_integer() && cb_nonpos(x)) {
        const BF last = hzdetail::hz_xadd(x, BF(n - 1));
        if (!cb_nonpos(last) || last.is_zero()) return inv ? BF::undefined() : BF::zero();  
        neg = (n & 1u) != 0;
        x   = -last;                                                                     
    }

    const double nd     = static_cast<double>(n);
    const double lb     = static_cast<double>(zt_bitlen(n));
    const double whole  = static_cast<double>((std::max)(x.get_exp_base2(), std::int64_t(0))) + 1.0;
    const double frac   = x.is_integer() ? 0.0 : -static_cast<double>(x.exponent());
    const double bits   = nd * ((std::max)(whole, lb) + frac + 2.0) + (binom ? nd * lb : 0.0);
    const double budget = 16.0 * static_cast<double>(ctx.precision) + 65536.0;

    if (bits <= budget && bits + 256.0 < static_cast<double>(BigUInt::max_bits) && nd * frac < 4.0e18) {
        BigInt       P;
        std::int64_t sh = 0;

        if (x.is_integer()) {
            P = cb_rise_int(x.get_integer_part(), n);
        } else {                                                                        
            BigUInt d = BigUInt::one();
            d.shift_left_mutable(static_cast<std::size_t>(-x.exponent()));
            P  = cb_ap(BigInt::from_magnitude(x.significand(), x.signbit()), BigInt(std::move(d)), 0, n);
            sh = x.exponent() * static_cast<std::int64_t>(n);
        }

        BF V(P, sh);
        if (neg)   V = -V;
        if (inv)   return BF::div(BF::one(), V, ctx);                                    
        if (binom) return BF::div(V, BF(gmdetail::fact_range(1, n + 1)), ctx);
        return V.rounded(ctx);
    }

    if (n <= cb_loop_cap) return cb_rise_float(x, n, inv, binom, neg, ctx);
    const BF  z1 = hzdetail::hz_xadd(x, BF(n));                                          
    const int s1 = gamma_sign(z1, ctx);
    const int s0 = gamma_sign(x, ctx);
    if (s1 >= 2 || s0 >= 2) return BF::undefined();
    std::vector<BF> num(1, z1), den(1, x);
    if (binom) den.push_back(hzdetail::hz_xadd(BF(n), BF::one()));
    if (inv)   std::swap(num, den);
    return cb_gamma_ratio(num, den, neg != ((s1 == 1) != (s0 == 1)), ctx);
}

} // namespace cbdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigInt rising_pochhammer(const BigInt& x, std::int64_t n) {                 
    if (!x.is_finite()) return x;
    if (n < 0)  return BigInt::undefined();                                        
    if (n == 0) return BigInt::one();
    const std::uint64_t m    = static_cast<std::uint64_t>(n);
    const BigInt        last = x + BigInt(m - 1);
    if (!x.is_positive() && !last.is_negative()) return BigInt::zero();
    if (!cbdetail::cb_int_ok(x, last, m)) return BigInt::undefined();
    return cbdetail::cb_rise_int(x, m);
}

BigInt falling_pochhammer(const BigInt& x, std::int64_t n) {                
    if (!x.is_finite()) return x;
    if (n < 0)  return BigInt::undefined();
    if (n == 0) return BigInt::one();
    return rising_pochhammer(x - BigInt(static_cast<std::uint64_t>(n) - 1), n);
}

BigInt combination(const BigInt& n, std::int64_t k) {
    if (!n.is_finite()) return n;
    if (k < 0)  return BigInt::zero();
    if (k == 0) return BigInt::one();
    std::uint64_t m = static_cast<std::uint64_t>(k);

    if (n.is_negative()) {                                                             
        const BigInt r = combination(BigInt(m) - n - BigInt::one(), k);
        return (m & 1u) ? -r : r;
    }

    const BigInt rest = n - BigInt(m);
    if (rest.is_negative()) return BigInt::zero();
    std::uint64_t ru = 0;
    if (cbdetail::cb_fits_u64(rest, ru) && ru < m) m = ru;                            
    if (m == 0) return BigInt::one();
    const BigInt lo = n - BigInt(m - 1);
    if (!cbdetail::cb_int_ok(lo, n, m)) return BigInt::undefined();
    return cbdetail::cb_rise_int(lo, m) / BigInt(gmdetail::fact_range(1, m + 1));     
}

BigFloat rising_pochhammer(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (n == 0)    return BigFloat::one();
    const std::uint64_t m = cb_uabs(n);

    if (x.is_infinite()) {
        const bool s = x.signbit() && (m & 1u);
        return (n < 0) ? BigFloat::zero(s) : BigFloat::infinity(s);
    }

    if (n > 0) return cb_rising_core(x, m, false, false, ctx);
    return cb_rising_core(hzdetail::hz_xadd(x, BigFloat(m), true), m, true, false, ctx);      
}

BigFloat falling_pochhammer(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (n == 0)    return BigFloat::one();
    const std::uint64_t m = cb_uabs(n);

    if (x.is_infinite()) {
        const bool s = x.signbit() && (m & 1u);
        return (n < 0) ? BigFloat::zero(s) : BigFloat::infinity(s);
    }

    if (n > 0) return cb_rising_core(hzdetail::hz_xadd(x, BigFloat(m - 1), true), m, false, false, ctx);
    return cb_rising_core(hzdetail::hz_xadd(x, BigFloat::one()), m, true, false, ctx);                  
}

BigFloat combination(const BigFloat& x, std::int64_t k, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (k < 0)     return BigFloat::zero();
    if (k == 0)    return BigFloat::one();
    std::uint64_t m = static_cast<std::uint64_t>(k);
    if (x.is_infinite()) return BigFloat::infinity(x.signbit() && (m & 1u));

    if (x.is_integer() && !x.signbit()) {                                                 
        const BigFloat r = hzdetail::hz_xadd(x, BigFloat(m), true);
        if (r.signbit() && !r.is_zero()) return BigFloat::zero();
        std::uint64_t ru = 0;
        if (gmdetail::gm_exact_u64(r, ru) && ru < m) m = ru;
        if (m == 0) return BigFloat::one();
    }

    return cb_rising_core(hzdetail::hz_xadd(x, BigFloat(m - 1), true), m, false, true, ctx);   
}

BigFloat rising_pochhammer(const BigFloat& x, const BigFloat& a, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (cb_bad(a)) return cb_nan_of(a);
    std::int64_t k = 0;
    if (hzdetail::hz_int64(a, k)) return rising_pochhammer(x, k, ctx);
    if (x.is_infinite() || a.is_infinite()) return BigFloat::undefined();
    return cb_gamma_quot(hzdetail::hz_xadd(x, a), x, ctx);                                
}

BigFloat falling_pochhammer(const BigFloat& x, const BigFloat& a, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (cb_bad(a)) return cb_nan_of(a);
    std::int64_t k = 0;
    if (hzdetail::hz_int64(a, k)) return falling_pochhammer(x, k, ctx);
    if (x.is_infinite() || a.is_infinite()) return BigFloat::undefined();
    const BigFloat x1 = hzdetail::hz_xadd(x, BigFloat::one());
    return cb_gamma_quot(x1, hzdetail::hz_xadd(x1, a, true), ctx);                       
}

BigFloat combination(const BigFloat& x, const BigFloat& k, const BigFloatContext& ctx) {
    using namespace cbdetail;
    if (cb_bad(x)) return cb_nan_of(x);
    if (cb_bad(k)) return cb_nan_of(k);
    std::int64_t ki = 0;
    if (hzdetail::hz_int64(k, ki)) return combination(x, ki, ctx);
    if (x.is_infinite() || k.is_infinite()) return BigFloat::undefined();

    if (k.is_integer()) {                                                                 
        if (k.signbit()) return BigFloat::zero();

        if (x.is_integer() && x.signbit()) {                                              
            const BigFloat y = hzdetail::hz_xadd(hzdetail::hz_xadd(k, x, true), BigFloat::one(), true);
            const BigFloat v = combination(y, k, ctx);
            return (k.exponent() == 0) ? -v : v;                                          
        }
    }

    const BigFloat x1 = hzdetail::hz_xadd(x, BigFloat::one());
    const BigFloat k1 = hzdetail::hz_xadd(k, BigFloat::one());
    const BigFloat d1 = hzdetail::hz_xadd(x1, k, true);                                   
    const int s1 = gamma_sign(x1, ctx);
    const int s2 = gamma_sign(k1, ctx);
    const int s3 = gamma_sign(d1, ctx);
    if (s1 == 3 || s2 == 3 || s3 == 3) return BigFloat::undefined();
    if (s1 == 2)                       return BigFloat::undefined();                      
    if (s2 == 2 || s3 == 2)            return BigFloat::zero();
    return cb_gamma_ratio({x1}, {k1, d1}, (s1 == 1) != ((s2 == 1) != (s3 == 1)), ctx);  
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
