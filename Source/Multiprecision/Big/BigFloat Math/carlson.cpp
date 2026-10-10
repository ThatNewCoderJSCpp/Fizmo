#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace csdetail {

unsigned cs_order(std::size_t want) { return static_cast<unsigned>(std::max<std::size_t>(8, std::min<std::size_t>(48, want / 24))); }

double cs_to_double(const BF& x) {
    if (x.is_zero())    return 0.0;
    if (!x.is_finite()) return x.signbit() ? -cs_inf : cs_inf;
    BigUInt           sig = x.significand();
    const std::size_t L   = sig.bit_length();
    std::int64_t      e   = x.exponent();

    if (L > 60) {
        sig.shift_right_mutable(L - 60);
        e += static_cast<std::int64_t>(L - 60);
    }

    e = std::max<std::int64_t>(-4000, std::min<std::int64_t>(4000, e));
    const double v = std::ldexp(static_cast<double>(sig.get_lowest_bits()), static_cast<int>(e));
    return x.signbit() ? -v : v;
}

bool cs_safe(const BF& v, std::size_t want, double lost, std::size_t prec) {
    if (v.is_zero() || !v.is_finite()) return false;
    BigUInt           sig = v.significand();
    const std::size_t L   = sig.bit_length();
    if (L < want) sig.shift_left_mutable(want - L);                       
    const double e = std::ceil(lost);
    if (!(e + 4.0 < static_cast<double>(sig.bit_length()))) return false;
    return constants::bfdetail::round_is_safe(sig, prec, (e <= 0.0) ? 2 : static_cast<std::size_t>(e) + 2);
}

BF cs_xsub(const BF& x, const BF& y) {                             
    if (y.is_zero()) return x;
    if (x.is_zero()) return -y;
    const std::int64_t hi = std::max(x.get_exp_base2(), y.get_exp_base2());
    const std::int64_t lo = std::min(x.exponent(), y.exponent());
    return BF::sub(x, y, cs_ctx(static_cast<std::size_t>(hi - lo) + 3));
}

bool cs_pow4(const BF& x, std::int64_t& k) {                       
    if (!x.is_finite() || x.is_zero() || x.signbit() || !x.significand().is_one()) return false;
    const std::int64_t e = x.exponent();
    if (e % 2 != 0) return false;
    k = e / 2;
    return true;
}

double cs_devs(const std::vector<BF>& v, const BF& A, const BFC& wc, std::vector<BF>& Z) {
    Z.resize(v.size());
    double r = 0.0;

    for (std::size_t i = 0; i < v.size(); ++i) {
        Z[i] = BF::div(BF::sub(A, v[i], wc), A, wc);
        r    = std::max(r, std::fabs(cs_to_double(Z[i])));
    }

    return r * (1.0 + 1.0e-12) + std::ldexp(1.0, -static_cast<int>(std::min<std::size_t>(wc.precision, 1000)) + 4);
}

double cs_tail_l2(double r, unsigned a2, unsigned M) {
    if (!(r < 1.0)) return cs_inf;
    if (r == 0.0)   return -cs_inf;
    const double a     = static_cast<double>(a2) / 2.0;
    const double md    = static_cast<double>(M);
    const double lcoef = (std::lgamma(a + md + 1.0) - std::lgamma(a) - std::lgamma(md + 2.0)) / 0.6931471805599453;
    const double q     = std::max(1.0, (a + md + 1.0) / (md + 2.0));     
    const double rq    = r * q;
    if (!(rq < 1.0)) return cs_inf;
    return lcoef + (md + 1.0) * std::log2(r) - std::log2(1.0 - rq);
}

BF cs_series(const std::vector<BF>& Z, const unsigned* b2, unsigned a2, unsigned M, const BFC& wc) {
    const std::size_t K  = Z.size();
    unsigned          c2 = 0;
    for (std::size_t i = 0; i < K; ++i) c2 += b2[i];
    std::vector<BF> P(M + 1, BF::zero());
    std::vector<BF> pw(Z);

    for (unsigned k = 2; k <= M; ++k) {
        BF s = BF::zero();

        for (std::size_t i = 0; i < K; ++i) {
            pw[i] = BF::mul(pw[i], Z[i], wc);
            s     = BF::add(s, BF::mul(BF(static_cast<std::uint64_t>(b2[i])), pw[i], wc), wc);
        }

        P[k] = s.scaled_pow2(-1);
    }

    std::vector<BF> T(M + 1, BF::zero());
    T[0] = BF::one();
    BF S    = BF::one();
    BF coef = BF::one();

    for (unsigned N = 1; N <= M; ++N) {
        BF t = BF::zero();
        for (unsigned k = 2; k <= N; ++k) t = BF::add(t, BF::mul(P[k], T[N - k], wc), wc);
        T[N] = BF::div(t, BF(static_cast<std::uint64_t>(N)), wc);
        coef = BF::div(BF::mul(coef, BF(static_cast<std::uint64_t>(a2 + 2 * N - 2)), wc), BF(static_cast<std::uint64_t>(c2 + 2 * N - 2)), wc);
        S    = BF::add(S, BF::mul(coef, T[N], wc), wc);
    }

    return S;
}

cs_val cs_rc_raw(BF x, BF y, std::size_t want) {
    static const unsigned b2[2] = {1, 2};
    const BFC      wc     = cs_ctx(want);
    const unsigned M      = cs_order(want);
    const double   target = -static_cast<double>(want) - 8.0;
    const BF       three  = BF(static_cast<std::uint64_t>(3));
    std::vector<BF> Z;

    for (std::size_t n = 0; n <= cs_cap(want); ++n) {
        const BF     A  = BF::div(BF::add(x, y.scaled_pow2(1), wc), three, wc);
        const double lt = cs_tail_l2(cs_devs({x, y}, A, wc, Z), 1, M);

        if (lt <= target) {
            const BF v = BF::div(cs_series(Z, b2, 1, M, wc), sqrt(A, wc), wc);
            const double units = 3.0 * static_cast<double>(n) + static_cast<double>(M) + 12.0 + std::exp2(lt + static_cast<double>(want));
            return cs_val{v, std::log2(units) + 1.0};
        }

        const BF lam = BF::add(BF::mul(sqrt(x, wc), sqrt(y, wc), wc).scaled_pow2(1), y, wc);
        x = BF::add(x, lam, wc).scaled_pow2(-2);
        y = BF::add(y, lam, wc).scaled_pow2(-2);
    }

    return cs_undef();
}

cs_val cs_rf_raw(BF x, BF y, BF z, std::size_t want) {
    static const unsigned b2[3] = {1, 1, 1};
    const BFC      wc     = cs_ctx(want);
    const unsigned M      = cs_order(want);
    const double   target = -static_cast<double>(want) - 8.0;
    const BF       three  = BF(static_cast<std::uint64_t>(3));
    std::vector<BF> Z;

    for (std::size_t n = 0; n <= cs_cap(want); ++n) {
        const BF     A  = BF::div(BF::add(BF::add(x, y, wc), z, wc), three, wc);
        const double lt = cs_tail_l2(cs_devs({x, y, z}, A, wc, Z), 1, M);

        if (lt <= target) {
            const BF v = BF::div(cs_series(Z, b2, 1, M, wc), sqrt(A, wc), wc);
            const double units = 3.0 * static_cast<double>(n) + static_cast<double>(M) + 12.0 + std::exp2(lt + static_cast<double>(want));
            return cs_val{v, std::log2(units) + 1.0};
        }

        const BF sx  = sqrt(x, wc);
        const BF sy  = sqrt(y, wc);
        const BF sz  = sqrt(z, wc);
        const BF lam = BF::add(BF::add(BF::mul(sx, sy, wc), BF::mul(sy, sz, wc), wc), BF::mul(sz, sx, wc), wc);
        x = BF::add(x, lam, wc).scaled_pow2(-2);
        y = BF::add(y, lam, wc).scaled_pow2(-2);
        z = BF::add(z, lam, wc).scaled_pow2(-2);
    }

    return cs_undef();
}

cs_val cs_rd_raw(BF x, BF y, BF z, std::size_t want) {
    static const unsigned b2[3] = {1, 1, 3};
    const BFC      wc     = cs_ctx(want);
    const BFC      ac     = cs_ctx(want + 32);
    const unsigned M      = cs_order(want);
    const double   target = -static_cast<double>(want) - 8.0;
    const BF       three  = BF(static_cast<std::uint64_t>(3));
    const BF       five   = BF(static_cast<std::uint64_t>(5));
    BF sum = BF::zero();
    std::vector<BF> Z;

    for (std::size_t n = 0; n <= cs_cap(want); ++n) {
        const BF     A  = BF::div(BF::add(BF::add(x, y, wc), BF::mul(z, three, wc), wc), five, wc);
        const double lt = cs_tail_l2(cs_devs({x, y, z}, A, wc, Z), 3, M);

        if (lt <= target) {
            const BF term = BF::div(cs_series(Z, b2, 3, M, wc), BF::mul(A, sqrt(A, wc), wc), wc);
            const BF v    = BF::add(sum, term.scaled_pow2(-2 * static_cast<std::int64_t>(n)), wc);
            const double units = 8.0 * static_cast<double>(n) + static_cast<double>(M) + 20.0 + std::exp2(lt + static_cast<double>(want));
            return cs_val{v, std::log2(units) + 1.0};
        }

        const BF sx  = sqrt(x, wc);
        const BF sy  = sqrt(y, wc);
        const BF sz  = sqrt(z, wc);
        const BF lam = BF::add(BF::add(BF::mul(sx, sy, wc), BF::mul(sy, sz, wc), wc), BF::mul(sz, sx, wc), wc);
        const BF t   = BF::div(three, BF::mul(sz, BF::add(z, lam, wc), wc), wc);            
        sum = BF::add(sum, t.scaled_pow2(-2 * static_cast<std::int64_t>(n)), ac);
        x = BF::add(x, lam, wc).scaled_pow2(-2);
        y = BF::add(y, lam, wc).scaled_pow2(-2);
        z = BF::add(z, lam, wc).scaled_pow2(-2);
    }

    return cs_undef();
}

cs_val cs_rj_raw(BF x, BF y, BF z, BF p, std::size_t want) {
    static const unsigned b2[4] = {1, 1, 1, 2};
    const BFC      wc     = cs_ctx(want);
    const BFC      ac     = cs_ctx(want + 32);
    const unsigned M      = cs_order(want);
    const double   target = -static_cast<double>(want) - 8.0;
    const BF       three  = BF(static_cast<std::uint64_t>(3));
    const BF       five   = BF(static_cast<std::uint64_t>(5));
    BF     sum      = BF::zero();
    double rc_units = 0.0;
    std::vector<BF> Z;

    for (std::size_t n = 0; n <= cs_cap(want); ++n) {
        const BF     A  = BF::div(BF::add(BF::add(BF::add(x, y, wc), z, wc), p.scaled_pow2(1), wc), five, wc);
        const double lt = cs_tail_l2(cs_devs({x, y, z, p}, A, wc, Z), 3, M);

        if (lt <= target) {
            const BF term = BF::div(cs_series(Z, b2, 3, M, wc), BF::mul(A, sqrt(A, wc), wc), wc);
            const BF v    = BF::add(sum, term.scaled_pow2(-2 * static_cast<std::int64_t>(n)), wc);
            const double units = static_cast<double>(n) * 8.0 + rc_units + static_cast<double>(M) + 24.0 + std::exp2(lt + static_cast<double>(want));
            return cs_val{v, std::log2(units) + 1.0};
        }

        const BF sx  = sqrt(x, wc);
        const BF sy  = sqrt(y, wc);
        const BF sz  = sqrt(z, wc);
        const BF lam = BF::add(BF::add(BF::mul(sx, sy, wc), BF::mul(sy, sz, wc), wc), BF::mul(sz, sx, wc), wc);
        const BF al0 = BF::add(BF::mul(p, BF::add(BF::add(sx, sy, wc), sz, wc), wc), BF::mul(BF::mul(sx, sy, wc), sz, wc), wc);
        const BF pl  = BF::add(p, lam, wc);
        const BF al  = BF::mul(al0, al0, wc);                                                  
        const BF be  = BF::mul(p, BF::mul(pl, pl, wc), wc);                                    
        const cs_val rc = cs_rc_raw(al, be, want);
        if (!rc.v.is_finite()) return rc;
        rc_units = std::max(rc_units, std::exp2(rc.lost) + 8.0);
        sum = BF::add(sum, BF::mul(three, rc.v, wc).scaled_pow2(-2 * static_cast<std::int64_t>(n)), ac);
        x = BF::add(x, lam, wc).scaled_pow2(-2);
        y = BF::add(y, lam, wc).scaled_pow2(-2);
        z = BF::add(z, lam, wc).scaled_pow2(-2);
        p = BF::add(p, lam, wc).scaled_pow2(-2);
    }

    return cs_undef();
}

void cs_sort3(const BF& x, const BF& y, const BF& z, BF& lo, BF& mid, BF& hi) {
    lo = x; mid = y; hi = z;
    if (BF::compare(mid, lo) == BF::ordering::less) std::swap(lo, mid);
    if (BF::compare(hi, mid) == BF::ordering::less) std::swap(mid, hi);
    if (BF::compare(mid, lo) == BF::ordering::less) std::swap(lo, mid);
}

cs_val cs_rc_pv_raw(const BF& x, const BF& y, std::size_t want) {
    if (!y.signbit()) return cs_rc_raw(x, y, want);
    if (x.is_zero())  return cs_val{BF::zero(), 0.0};                                          
    const BFC wc  = cs_ctx(want);
    const BF  ny  = -y;
    const BF  xmy = BF::add(x, ny, wc);                                                        
    const cs_val r = cs_rc_raw(xmy, ny, want);                                                 
    if (!r.v.is_finite()) return r;
    const BF f = sqrt(BF::div(x, xmy, wc), wc);
    return cs_val{BF::mul(f, r.v, wc), std::log2(std::exp2(r.lost) + 6.0)};
}

cs_val cs_rj_pv_raw(const BF& x, const BF& y, const BF& z, const BF& p, std::size_t want) {
    const BFC wc    = cs_ctx(want);
    const BF  three = BF(static_cast<std::uint64_t>(3));
    const BF  ymp   = cs_xsub(y, p);                                                          
    const BF  b     = BF::div(BF::mul(cs_xsub(z, y), cs_xsub(y, x), wc), ymp, wc);            
    const BF  q     = BF::add(y, b, wc);
    const BF  rho   = BF::div(BF::mul(x, z, wc), y, wc);
    const BF  tau   = BF::div(BF::mul(p, q, wc), y, wc);                                      
    const cs_val rj = cs_rj_raw(x, y, z, q, want);
    if (!rj.v.is_finite()) return rj;
    const cs_val rf = cs_rf_raw(x, y, z, want);
    if (!rf.v.is_finite()) return rf;
    const cs_val rc = cs_rc_pv_raw(rho, tau, want);
    if (!rc.v.is_finite()) return rc;
    const BF u1    = BF::mul(b, rj.v, wc);
    const BF u2    = BF::mul(three, rc.v, wc);
    const BF u3    = BF::mul(three, rf.v, wc);
    const BF inner = BF::sub(BF::add(u1, u2, wc), u3, wc);
    const BF v     = BF::div(inner, ymp, wc);
    if (inner.is_zero()) return cs_val{v, 1.0e9};
    const double li = cs_l2lo(inner);
    const double units = std::exp2(cs_l2hi(u1) + std::log2(std::exp2(rj.lost) + 8.0)  - li)
                       + std::exp2(cs_l2hi(u2) + std::log2(std::exp2(rc.lost) + 10.0) - li)
                       + std::exp2(cs_l2hi(u3) + std::log2(std::exp2(rf.lost) + 2.0)  - li)
                       + 2.0;
    return cs_val{v, std::log2(units) + 1.0};
}

cs_val cs_rg_raw(const BF& a, const BF& m, const BF& c, std::size_t want) {
    const BFC wc = cs_ctx(want);
    const cs_val rf = cs_rf_raw(a, c, m, want);
    if (!rf.v.is_finite()) return rf;
    const cs_val rd = cs_rd_raw(a, c, m, want);
    if (!rd.v.is_finite()) return rd;
    const BF t1 = BF::mul(m, rf.v, wc);
    const BF w  = BF::mul(cs_xsub(m, a), cs_xsub(c, m), wc);                                   
    const BF t2 = BF::div(BF::mul(w, rd.v, wc), BF(static_cast<std::uint64_t>(3)), wc);
    const BF t3 = sqrt(BF::div(BF::mul(a, c, wc), m, wc), wc);
    const BF v  = BF::add(BF::add(t1, t2, wc), t3, wc).scaled_pow2(-1);
    const double part = std::max(std::exp2(rf.lost) + 1.0, std::max(std::exp2(rd.lost) + 3.0, 3.0));
    return cs_val{v, std::log2(part + 2.0) + 1.0};
}

} // namespace csdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat carlson_rc(const BigFloat& x, const BigFloat& y, const BigFloatContext& ctx) {
    using namespace csdetail;
    if (x.is_nan() || y.is_nan())             return BigFloat::nan();
    if (x.is_undefined() || y.is_undefined()) return BigFloat::undefined();
    if (cs_neg(x))                            return BigFloat::nan();
    if (y.is_zero())                          return BigFloat::infinity();
    if (x.is_infinite() || y.is_infinite())   return BigFloat::zero();

    if (!y.signbit()) {
        std::int64_t k = 0;
        if (BigFloat::compare(x, y) == BigFloat::ordering::equal && cs_pow4(x, k)) return BigFloat::one().scaled_pow2(-k);
        return cs_drive(ctx, [&](std::size_t want) { return cs_rc_raw(x, y, want); });
    }

    if (x.is_zero()) return BigFloat::zero();                                                   
    const BigFloat xmy = cs_xsub(x, y);                                                         
    const BigFloat ny  = -y;

    return cs_drive(ctx, [&](std::size_t want) -> cs_val {                                      
        const cs_val r = cs_rc_raw(xmy, ny, want);
        if (!r.v.is_finite()) return r;
        const BigFloatContext wc = cs_ctx(want);
        const BigFloat f = sqrt(BigFloat::div(x, xmy, wc), wc);
        return cs_val{BigFloat::mul(f, r.v, wc), std::log2(std::exp2(r.lost) + 4.0)};
    });
}

BigFloat carlson_rf(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx) {
    using namespace csdetail;
    if (cs_any({&x, &y, &z}, cs_is_nan))   return BigFloat::nan();
    if (cs_any({&x, &y, &z}, cs_is_undef)) return BigFloat::undefined();
    if (cs_any({&x, &y, &z}, cs_is_neg))   return BigFloat::nan();
    if (static_cast<int>(x.is_zero()) + static_cast<int>(y.is_zero()) + static_cast<int>(z.is_zero()) >= 2) return BigFloat::infinity();
    if (cs_any({&x, &y, &z}, cs_is_inf))   return BigFloat::zero();
    std::int64_t k = 0;

    if (BigFloat::compare(x, y) == BigFloat::ordering::equal && BigFloat::compare(y, z) == BigFloat::ordering::equal && cs_pow4(x, k)) {
        return BigFloat::one().scaled_pow2(-k);                                                 
    }

    return cs_drive(ctx, [&](std::size_t want) { return cs_rf_raw(x, y, z, want); });
}

BigFloat carlson_rd(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx) {
    using namespace csdetail;
    if (cs_any({&x, &y, &z}, cs_is_nan))   return BigFloat::nan();
    if (cs_any({&x, &y, &z}, cs_is_undef)) return BigFloat::undefined();
    if (cs_any({&x, &y, &z}, cs_is_neg))   return BigFloat::nan();
    if (z.is_zero() || (x.is_zero() && y.is_zero())) return BigFloat::infinity();
    if (cs_any({&x, &y, &z}, cs_is_inf))   return BigFloat::zero();
    std::int64_t k = 0;

    if (BigFloat::compare(x, y) == BigFloat::ordering::equal && BigFloat::compare(y, z) == BigFloat::ordering::equal && cs_pow4(x, k)) {
        return BigFloat::one().scaled_pow2(-3 * k);                                             
    }

    return cs_drive(ctx, [&](std::size_t want) { return cs_rd_raw(x, y, z, want); });
}

BigFloat carlson_rj(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloat& p, const BigFloatContext& ctx) {
    using namespace csdetail;
    if (cs_any({&x, &y, &z, &p}, cs_is_nan))   return BigFloat::nan();
    if (cs_any({&x, &y, &z, &p}, cs_is_undef)) return BigFloat::undefined();
    if (cs_any({&x, &y, &z}, cs_is_neg))       return BigFloat::nan();
    if (p.is_zero() || static_cast<int>(x.is_zero()) + static_cast<int>(y.is_zero()) + static_cast<int>(z.is_zero()) >= 2) return BigFloat::infinity();
    if (cs_any({&x, &y, &z, &p}, cs_is_inf))   return BigFloat::zero();

    if (p.signbit()) {
        BigFloat lo, mid, hi;
        cs_sort3(x, y, z, lo, mid, hi);                                                         
        return cs_drive(ctx, [&](std::size_t want) { return cs_rj_pv_raw(lo, mid, hi, p, want); });
    }

    std::int64_t k = 0;

    if (BigFloat::compare(x, y) == BigFloat::ordering::equal && BigFloat::compare(y, z) == BigFloat::ordering::equal &&
        BigFloat::compare(z, p) == BigFloat::ordering::equal && cs_pow4(x, k)) {
        return BigFloat::one().scaled_pow2(-3 * k);
    }

    return cs_drive(ctx, [&](std::size_t want) { return cs_rj_raw(x, y, z, p, want); });
}

BigFloat carlson_rg(const BigFloat& x, const BigFloat& y, const BigFloat& z, const BigFloatContext& ctx) {
    using namespace csdetail;
    if (cs_any({&x, &y, &z}, cs_is_nan))   return BigFloat::nan();
    if (cs_any({&x, &y, &z}, cs_is_undef)) return BigFloat::undefined();
    if (cs_any({&x, &y, &z}, cs_is_neg))   return BigFloat::nan();
    if (cs_any({&x, &y, &z}, cs_is_inf))   return BigFloat::infinity();
    BigFloat lo, mid, hi;
    cs_sort3(x, y, z, lo, mid, hi);
    if (hi.is_zero())  return BigFloat::zero();                                                 
    if (mid.is_zero()) return sqrt(hi, ctx).scaled_pow2(-1);                                    
    if (BigFloat::compare(lo, hi) == BigFloat::ordering::equal) return sqrt(lo, ctx);           
    return cs_drive(ctx, [&](std::size_t want) { return cs_rg_raw(lo, mid, hi, want); });
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
