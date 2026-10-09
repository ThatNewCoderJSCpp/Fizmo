#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace jcdetail {

double jc_lsum(double a, double b) {
    if (a == jc_ninf) return b;
    if (b == jc_ninf) return a;
    if (a < b) std::swap(a, b);
    return a + std::log2(1.0 + std::exp2(b - a));
}

bool jc_newton(const BF& ua, const BF& m, std::size_t want, BF& phi, double& lphi) {
    using eldetail::el_kind;
    const BF           one   = BF::one();
    const bool         mpos  = !m.signbit();
    const BF           omm   = csdetail::cs_xsub(one, m);                                  
    const std::int64_t eu    = ua.get_exp_base2();
    const std::size_t  extra = static_cast<std::size_t>(eu > 0 ? eu : 0);
    const std::size_t  wp    = want + extra + 16;
    const std::size_t  p0    = std::min<std::size_t>(64 + extra, wp);
    const BFC          c0    = cs_ctx(p0);
    const BF sq = sqrt(omm, c0);
    BF lo = mpos ? BF::mul(ua, sq, c0) : ua;
    BF hi = mpos ? ua : BF::mul(ua, sq, c0);
    lo = BF::sub(lo, lo.scaled_pow2(4 - static_cast<std::int64_t>(p0)), c0);                
    hi = BF::add(hi, hi.scaled_pow2(4 - static_cast<std::int64_t>(p0)), c0);
    const cs_val K = eldetail::el_complete_raw(el_kind::f, BF::zero(), m, p0);
    if (!K.v.is_finite()) return false;
    phi = BF::div(BF::mul(ua, constants::half_pi(c0), c0), K.v, c0);                       
    if (BF::compare(phi, lo) == BF::ordering::less)    phi = lo;
    if (BF::compare(phi, hi) == BF::ordering::greater) phi = hi;
    std::size_t p = p0;

    for (int it = 0; it < 400; ++it) {
        const BFC    pc = cs_ctx(p);
        const cs_val Fv = eldetail::el_incomplete_raw(el_kind::f, phi, BF::zero(), m, p);
        if (!Fv.v.is_finite()) return false;
        const BF res = BF::sub(Fv.v, ua, pc);
        if (res.signbit()) lo = phi; else hi = phi;
        const BF sn   = sin(phi, pc);
        const BF cn   = cos(phi, pc);
        const BF d2   = mpos ? BF::add(omm, BF::mul(m, BF::mul(cn, cn, pc), pc), pc) : BF::sub(one, BF::mul(m, BF::mul(sn, sn, pc), pc), pc);
        const BF step = BF::mul(res, sqrt(d2, pc), pc);                                     
        const bool small = step.is_zero() || step.get_exp_base2() <= phi.get_exp_base2() - static_cast<std::int64_t>(p) + 8;

        if (small && p == wp) {
            const double lres  = cs_l2hi(res);
            const double leps  = cs_l2hi(Fv.v) + Fv.lost - static_cast<double>(p);
            const double ldmax = mpos ? 0.0 : 0.5 * cs_l2hi(omm);
            lphi = jc_lsum(lres, leps) + ldmax + 0.02;
            return true;
        }

        BF next = BF::sub(phi, step, pc);

        if (BF::compare(next, lo) != BF::ordering::greater || BF::compare(next, hi) != BF::ordering::less) {
            next = BF::add(lo, hi, pc).scaled_pow2(-1);                                     
        }

        phi = next;
        if (small) p = std::min<std::size_t>(2 * p, wp);
    }

    return false;
}

void jc_derive(jc_trip& t, const BF& m, std::size_t want) {
    const BFC    wc  = cs_ctx(want);
    const double wd  = static_cast<double>(want);
    const BF     one = BF::one();
    t.s  = sin(t.phi, wc);
    t.c  = cos(t.phi, wc);
    t.ls = jc_lsum(t.lphi + std::min(0.0, cs_l2hi(t.c)), cs_l2hi(t.s) - wd);
    t.lc = jc_lsum(t.lphi + std::min(0.0, cs_l2hi(t.s)), cs_l2hi(t.c) - wd);
    const BF d2 = !m.signbit() ? BF::add(csdetail::cs_xsub(one, m), BF::mul(m, BF::mul(t.c, t.c, wc), wc), wc) : BF::sub(one, BF::mul(m, BF::mul(t.s, t.s, wc), wc), wc);                        
    t.d = sqrt(d2, wc);
    const double lm = m.is_zero() ? jc_ninf : cs_l2hi(m);
    t.ld = jc_lsum(t.lphi + lm + cs_l2hi(t.s) + cs_l2hi(t.c) - cs_l2lo(t.d), cs_l2hi(t.d) + 2.0 - wd);         
}

bool jc_triple(const BF& u, const BF& m, std::size_t want, jc_trip& t) {
    const BFC    wc  = cs_ctx(want);
    const double wd  = static_cast<double>(want);
    const BF     one = BF::one();
    const bool   neg = u.signbit();
    const BF     ua  = u.abs();

    if (m.is_zero()) {                                                                     
        t.phi  = u;
        t.lphi = jc_ninf;
        jc_derive(t, m, want);
        return true;
    }

    if (jc_is_one(m)) {                                                                    
        t.s    = tanh(u, wc);
        t.c    = cosh(u, wc).reciprocal(wc);
        t.d    = t.c;
        t.phi  = arctan(sinh(u, wc), wc);
        t.ls   = cs_l2hi(t.s) - wd;
        t.lc   = cs_l2hi(t.c) + 1.0 - wd;
        t.ld   = t.lc;
        t.lphi = cs_l2hi(t.phi) + 1.6 - wd;
        return true;
    }

    if (BF::compare(m, one) == BF::ordering::less) {
        if (!jc_newton(ua, m, want, t.phi, t.lphi)) return false;
        if (neg) t.phi = -t.phi;
        jc_derive(t, m, want);
        return true;
    }

    const BF mp  = m.reciprocal(wc);
    const BF rm  = sqrt(m, wc);
    const BF up  = BF::mul(ua, rm, wc);
    const BF omp = BF::sub(one, mp, wc);
    jc_trip tp;
    if (!jc_newton(up, mp, want, tp.phi, tp.lphi)) return false;
    const double lup  = cs_l2hi(up) + 1.0 - wd;                                            
    const double lpar = cs_l2hi(tp.phi) - 1.0 - 1.5 * cs_l2lo(omp) + cs_l2hi(mp) - wd;     
    tp.lphi = jc_lsum(jc_lsum(tp.lphi, lup), lpar);
    jc_derive(tp, mp, want);
    tp.ld = jc_lsum(tp.ld, 2.0 * cs_l2hi(tp.s) - 1.0 - cs_l2lo(tp.d) + cs_l2hi(mp) - wd);  
    t.s  = BF::div(tp.s, rm, wc);
    t.ls = jc_lsum(tp.ls - cs_l2lo(rm), cs_l2hi(t.s) + 1.0 - wd);
    t.c  = tp.d;
    t.lc = tp.ld;
    t.d  = tp.c;
    t.ld = tp.lc;
    const BF oms2 = BF::sub(one, BF::mul(t.s, t.s, wc), wc);                               
    t.phi  = arcsin(t.s, wc);
    t.lphi = jc_lsum(t.ls - 0.5 * cs_l2lo(oms2), cs_l2hi(t.phi) - wd);

    if (neg) {
        t.s   = -t.s;
        t.phi = -t.phi;
    }

    return true;
}

void jc_parts(jc_fn f, int& p, int& q) {
    switch (f) {
        case jc_fn::sn: p = 1; q = 0; break;
        case jc_fn::cn: p = 2; q = 0; break;
        case jc_fn::dn: p = 3; q = 0; break;
        case jc_fn::ns: p = 0; q = 1; break;
        case jc_fn::nc: p = 0; q = 2; break;
        case jc_fn::nd: p = 0; q = 3; break;
        case jc_fn::sc: p = 1; q = 2; break;
        case jc_fn::sd: p = 1; q = 3; break;
        case jc_fn::cs: p = 2; q = 1; break;
        case jc_fn::cd: p = 2; q = 3; break;
        case jc_fn::ds: p = 3; q = 1; break;
        default:        p = 3; q = 2; break;                                               
    }
}

cs_val jc_raw(jc_fn f, const BF& u, const BF& m, std::size_t want) {
    jc_trip t;
    if (!jc_triple(u, m, want, t)) return cs_val{BF::undefined(), 0.0};
    const BFC    wc = cs_ctx(want);
    const double wd = static_cast<double>(want);

    if (f == jc_fn::am) {
        if (t.phi.is_zero()) return cs_val{t.phi, 1.0e9};
        return cs_val{t.phi, std::log2(std::exp2(t.lphi - cs_l2lo(t.phi) + wd) + 1.0) + 1.0};
    }

    int pi_, qi_;
    jc_parts(f, pi_, qi_);
    auto pick = [&](int i, BF& x, double& lx) {
        switch (i) {
            case 1:  x = t.s; lx = t.ls; break;
            case 2:  x = t.c; lx = t.lc; break;
            case 3:  x = t.d; lx = t.ld; break;
            default: x = BF::one(); lx = jc_ninf; break;
        }
    };

    BF P, Q;
    double lP, lQ;
    pick(pi_, P, lP);
    pick(qi_, Q, lQ);
    if (Q.is_zero() || P.is_zero()) return cs_val{BF::zero(), 1.0e9};
    const BF v = (qi_ == 0) ? P : ((pi_ == 0) ? Q.reciprocal(wc) : BF::div(P, Q, wc));
    const double lrel = jc_lsum((pi_ != 0) ? lP - cs_l2lo(P) : jc_ninf, (qi_ != 0) ? lQ - cs_l2lo(Q) : jc_ninf);
    return cs_val{v, std::log2(std::exp2(lrel + wd) + 3.0) + 1.0};
}

BF jc_public(jc_fn f, const BF& u, const BF& m, const BFC& ctx) {
    if (u.is_nan() || m.is_nan())             return BF::nan();
    if (u.is_undefined() || m.is_undefined()) return BF::undefined();
    if (m.is_infinite())                      return BF::undefined();
    const bool m1  = jc_is_one(m);
    const BF   one = BF::one();

    if (u.is_infinite()) {                                                                  
        if (!m1) return BF::undefined();
        const bool neg = u.signbit();

        switch (f) {
            case jc_fn::am:
                if (!neg) return constants::half_pi(ctx);
                return csdetail::cs_drive(ctx, [](std::size_t want) { return cs_val{-constants::half_pi(cs_ctx(want)), 1.0}; });
            case jc_fn::sn: case jc_fn::ns: return BF::one(neg);
            case jc_fn::cn: case jc_fn::dn: return BF::zero();
            case jc_fn::nc: case jc_fn::nd: return BF::infinity();
            case jc_fn::sc: case jc_fn::sd: return jc_signed_inf(neg);
            case jc_fn::cs: case jc_fn::ds: return BF::zero(neg);
            default:                        return one;                                    
        }
    }

    if (u.is_zero()) {
        switch (f) {
            case jc_fn::am: case jc_fn::sn: case jc_fn::sc: case jc_fn::sd: return u;
            case jc_fn::ns: case jc_fn::cs: case jc_fn::ds:                 return jc_signed_inf(u.signbit());
            default:                                                        return one;     
        }
    }

    if (m.is_zero()) {
        if (f == jc_fn::am)                     return u.rounded(ctx);
        if (f == jc_fn::dn || f == jc_fn::nd)   return one;
    }

    if (m1 && (f == jc_fn::cd || f == jc_fn::dc)) return one;
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return jc_raw(f, u, m, want); });
}

} // namespace jcdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo
