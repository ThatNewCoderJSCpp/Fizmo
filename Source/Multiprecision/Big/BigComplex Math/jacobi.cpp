#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::max(std::min(l, 1000.0), -1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

struct Trip {
    CV sn, cn, dn;
    bool ok = true;
};

CV cv_div(const CV& a, const CV& b, std::size_t w) {
    const CX q = div(a.v, b.v, w + 8);
    const double lb = l2(b.v);
    const double e = lsum(lsum(a.er, a.ei) - lb, l2(q) + lsum(b.er, b.ei) - lb) + 1.5;
    const double r = l2(q) - static_cast<double>(w) + 2.0;
    return CV{q, lsum(e, r), lsum(e, r)};
}

BF one_minus_real(const BF& m) {
    const std::int64_t hi = std::max<std::int64_t>(2, m.is_zero() ? 2 : m.get_exp_base2() + 2);
    const std::int64_t lo = m.is_zero() ? 0 : std::min<std::int64_t>(0, m.exponent());
    return BF::sub(BF::one(), m, wctx(static_cast<std::size_t>(hi - lo + 8)));
}

Trip real_m(const CX& u, const BF& m, std::size_t w) {
    const std::size_t wb = w + 16;
    const BF  m1 = one_minus_real(m);
    Trip t;
    math::jcdetail::jc_trip a, b;
    if (!math::jcdetail::jc_triple(u.re, m, wb + 8, a) || !math::jcdetail::jc_triple(u.im, m1, wb + 8, b)) {
        t.ok = false;
        return t;
    }
    for (const BF* p : {&a.s, &a.c, &a.d, &b.s, &b.c, &b.d}) {
        if (!p->is_finite()) {
            t.ok = false;
            return t;
        }
    }
    auto cvr = [&](const BF& v, double le) { return cv_from(CX(v.rounded(wctx(wb))), lsum(le, v.is_zero() ? ninf : l2(v) - static_cast<double>(wb))); };
    const CV S = cvr(a.s, a.ls), C = cvr(a.c, a.lc), D = cvr(a.d, a.ld), S1 = cvr(b.s, b.ls), C1 = cvr(b.c, b.lc), D1 = cvr(b.d, b.ld);
    const CV Mv = cv_exact(CX(m));
    const CV ss1 = cv_mul(S, S1, wb);
    const CV del = cv_add(cv_mul(C1, C1, wb), cv_mul(Mv, cv_mul(ss1, ss1, wb), wb), wb);
    if (del.v.re.is_zero()) {
        t.ok = false;
        return t;
    }
    auto mk = [&](const CV& re, const CV& im) { return CV{CX(re.v.re, im.v.re), re.er, im.er}; };
    const CV sn_n = mk(cv_mul(S, D1, wb), cv_mul(cv_mul(C, D, wb), cv_mul(S1, C1, wb), wb));
    const CV cn_n = mk(cv_mul(C, C1, wb), cv_scale(cv_mul(cv_mul(S, D, wb), cv_mul(S1, D1, wb), wb), BF::one(true), wb));
    const CV dn_n = mk(cv_mul(D, cv_mul(C1, D1, wb), wb), cv_scale(cv_mul(Mv, cv_mul(cv_mul(S, C, wb), S1, wb), wb), BF::one(true), wb));
    auto dv = [&](const CV& a) {
        const CX q = div(a.v, del.v.re, wb + 8);
        const double rd = del.er - l2(del.v.re);
        const double er = lsum(a.er - l2(del.v.re), l2(q.re) + rd) + 0.5;
        const double ei = lsum(a.ei - l2(del.v.re), l2(q.im) + rd) + 0.5;
        return CV{q, lsum(er, l2(q.re) - static_cast<double>(wb)), lsum(ei, l2(q.im) - static_cast<double>(wb))};
    };
    t.sn = dv(sn_n);
    t.cn = dv(cn_n);
    t.dn = dv(dn_n);
    return t;
}

struct Theta {
    CX     v;
    double e;
};

void thetas(const CX& z, const CX& q, std::size_t wb, double zfac, Theta out[4], bool at_zero) {
    const double lq = l2(q);
    CX s1, s2, s3(BF::one()), s4(BF::one());
    Term T1, T2, T3, T4;
    T3.add(0.0);
    T4.add(0.0);
    CX qp(BF::one());
    CX qn(BF::one());
    const CX q2 = sqr(q, wb);
    CX qodd = q;
    CX qe(BF::one());
    const double liz = std::fabs(to_d(z.im));
    double tail = ninf;
    for (std::int64_t n = 0;; ++n) {
        const std::size_t wt = wb + bits_of(std::log2(static_cast<double>(2 * n + 1) * (1.0 + std::exp2(std::min(l2(z), 900.0))))) + 8;
        const CX zz = mul(z, BF(2 * n + 1), wt);
        const CX sv = at_zero ? CX() : sin(zz, wb);
        const CX cv = at_zero ? CX(BF::one()) : cos(zz, wb);
        const CX a1 = mul(qp, sv, wb);
        const CX a2 = mul(qp, cv, wb);
        s1 = (n & 1) ? sub(s1, a1, wb) : add(s1, a1, wb);
        s2 = add(s2, a2, wb);
        T1.add(l2(a1) + (n > 0 ? std::log2(static_cast<double>(n * (n + 1)) + zfac * static_cast<double>(2 * n + 1)) : std::log2(zfac + 1.0)));
        T2.add(l2(a2) + (n > 0 ? std::log2(static_cast<double>(n * (n + 1)) + zfac * static_cast<double>(2 * n + 1)) : std::log2(zfac + 1.0)));
        const std::int64_t k = n + 1;
        qn = mul(qn, qodd, wb);
        qodd = mul(qodd, q2, wb);
        const CX zk = mul(z, BF(2 * k), wt + 1);
        const CX ck = at_zero ? CX(BF::one()) : cos(zk, wb);
        const CX b  = scale2(mul(qn, ck, wb), 1);
        s3 = add(s3, b, wb);
        s4 = (k & 1) ? sub(s4, b, wb) : add(s4, b, wb);
        const double wk = std::log2(static_cast<double>(k * k) + zfac * static_cast<double>(2 * k));
        T3.add(l2(b) + wk);
        T4.add(l2(b) + wk);
        qe = mul(qe, q2, wb);
        qp = mul(qp, qe, wb);
        const double nd = static_cast<double>(n);
        const double n12  = lq * (nd + 1.0) * (nd + 2.0) + (2.0 * nd + 3.0) * liz * 1.4426950408889634 + 1.0;
        const double n34  = lq * (nd + 2.0) * (nd + 2.0) + 2.0 * (nd + 2.0) * liz * 1.4426950408889634 + 2.0;
        const double next = std::max(n12, n34) + std::log2((nd + 2.0) * (nd + 3.0) + zfac * (2.0 * nd + 5.0));
        const double dec  = lq * (2.0 * nd + 2.0) + 2.0 * liz * 1.4426950408889634;
        if (dec < -1.0 && next < std::max({T1.l2max, T2.l2max, T3.l2max, T4.l2max}) - static_cast<double>(wb) - 8.0) {
            tail = next + 1.0;
            break;
        }
        if (n > 1000000) break;
    }
    const double r = -static_cast<double>(wb) + 4.0;
    out[0] = Theta{scale2(s1, 1), lsum(T1.l2sum + 1.0 + r, tail)};
    out[1] = Theta{scale2(s2, 1), lsum(T2.l2sum + 1.0 + r, tail)};
    out[2] = Theta{s3, lsum(T3.l2sum + r, tail)};
    out[3] = Theta{s4, lsum(T4.l2sum + r, tail)};
}

CV th_cv(const Theta& t) { return cv_from(t.v, t.e); }

Trip theta_core(const CX& u, const CX& m, std::size_t w) {
    Trip t;
    const std::size_t wb = w + 24;
    const std::size_t wk = wb + 32 + bits_of(std::fabs(l2(u)));
    const CX m1(one_minus_real(m.re), m.im.with_sign(!m.im.signbit()));
    const CX K  = cx(elliptic_k(bc(m), BCC(BFC::clamp_precision(wk))));
    const CX Kp = cx(elliptic_k(bc(m1), BCC(BFC::clamp_precision(wk))));
    if (!finite(K) || !finite(Kp) || is_zero(K)) {
        t.ok = false;
        return t;
    }
    const CX tau = div(mul_i(Kp), K, wk);
    const double imt = to_d(tau.im);
    if (!(imt > 0.0)) {
        t.ok = false;
        return t;
    }
    const CX tt = div(u, scale2(K, 1), wk);
    const double beta  = to_d(tt.im) / imt;
    const double alpha = to_d(tt.re) - beta * to_d(tau.re);
    const double a = std::nearbyint(alpha), b = std::nearbyint(beta);
    if (std::fabs(a) > 1e15 || std::fabs(b) > 1e15) {
        t.ok = false;
        return t;
    }
    const std::int64_t ai = static_cast<std::int64_t>(a), bi = static_cast<std::int64_t>(b);
    CX u0 = u;
    if (ai != 0) u0 = sub(u0, mul(K, BF(2 * ai), wk), wk);
    if (bi != 0) u0 = sub(u0, mul_i(mul(Kp, BF(2 * bi), wk)), wk);
    const CX z = div(mul(u0, pi(wk), wk), scale2(K, 1), wb + 8);
    const CX q = exp(mul_i(mul(tau, pi(wk), wk)), wb + 8);
    const double zfac = std::exp2(std::min(l2(z), 60.0)) + 1.0;
    Theta at[4], a0[4];
    thetas(z, q, wb, zfac + std::fabs(a) + std::fabs(b) + 4.0, at, false);
    thetas(CX(), q, wb, 1.0, a0, true);
    const CV t1 = th_cv(at[0]), t2 = th_cv(at[1]), t3 = th_cv(at[2]), t4 = th_cv(at[3]);
    const CV z2 = th_cv(a0[1]), z3 = th_cv(a0[2]), z4 = th_cv(a0[3]);
    if (is_zero(z2.v) || is_zero(z3.v) || is_zero(t4.v)) {
        t.ok = false;
        return t;
    }
    CV sn = cv_div(cv_mul(z3, t1, wb), cv_mul(z2, t4, wb), wb);
    CV cn = cv_div(cv_mul(z4, t2, wb), cv_mul(z2, t4, wb), wb);
    CV dn = cv_div(cv_mul(z4, t3, wb), cv_mul(z3, t4, wb), wb);
    if (ai & 1) sn.v = neg(sn.v);
    if ((ai + bi) & 1) cn.v = neg(cn.v);
    if (bi & 1) dn.v = neg(dn.v);
    t.sn = sn;
    t.cn = cn;
    t.dn = dn;
    return t;
}

Trip complex_m(const CX& u, const CX& m, std::size_t w) {
    const CX m1(one_minus_real(m.re), m.im.with_sign(!m.im.signbit()));
    const CX K  = cx(elliptic_k(bc(m), BCC(BFC::clamp_precision(64))));
    const CX Kp = cx(elliptic_k(bc(m1), BCC(BFC::clamp_precision(64))));
    const CX tau = div(mul_i(Kp), K, 64);
    const CX itau = neg(recip(tau, 64));
    if (to_d(tau.im) >= to_d(itau.im)) return theta_core(u, m, w);
    const Trip r = theta_core(mul_i(u), m1, w + 8);
    Trip t;
    if (!r.ok) {
        t.ok = false;
        return t;
    }
    t.sn = cv_div(r.sn, r.cn, w + 8);
    t.sn = CV{div_i(t.sn.v), t.sn.ei, t.sn.er};
    t.cn = cv_div(cv_exact(CX(BF::one())), r.cn, w + 8);
    t.dn = cv_div(r.dn, r.cn, w + 8);
    return t;
}

enum class jf { am, sn, cn, dn, ns, nc, nd, sc, sd, cs, cd, ds, dc };

CV pick(jf f, const Trip& t, std::size_t w) {
    const CV one = cv_exact(CX(BF::one()));
    switch (f) {
        case jf::sn: return t.sn;
        case jf::cn: return t.cn;
        case jf::dn: return t.dn;
        case jf::ns: return cv_div(one, t.sn, w);
        case jf::nc: return cv_div(one, t.cn, w);
        case jf::nd: return cv_div(one, t.dn, w);
        case jf::sc: return cv_div(t.sn, t.cn, w);
        case jf::sd: return cv_div(t.sn, t.dn, w);
        case jf::cs: return cv_div(t.cn, t.sn, w);
        case jf::cd: return cv_div(t.cn, t.dn, w);
        case jf::ds: return cv_div(t.dn, t.sn, w);
        default: return cv_div(t.dn, t.cn, w);
    }
}

Approx am_from(const Trip& t, const CX& u, const BF* mreal, const CX& m, std::size_t w) {
    const std::size_t wb = w + 8;
    const CX e  = add(t.cn.v, mul_i(t.sn.v), wb + 8);
    const double le = l2(e);
    if (le == ninf) return approx_undefined();
    const CX L  = ln(e, wb + 8);
    CX v = div_i(L);
    double ref;
    if (mreal) {
        ref = to_d(math::jacobi_am(u.re, *mreal, wctx(64)));
    } else {
        const CX K = cx(elliptic_k(bc(m), BCC(BFC::clamp_precision(64))));
        ref = to_d(div(mul(u, pi(64), 64), scale2(K, 1), 64).re);
    }
    const double k = std::nearbyint((ref - to_d(v.re)) / (2.0 * 3.141592653589793));
    if (k != 0.0) v = add_real(v, BF::mul(BF(static_cast<std::int64_t>(2.0 * k)), pi(wb + 8 + bits_of(std::log2(std::fabs(k) + 1.0))), wctx(wb + 8)), wb + 8);
    const double ee = lsum(lsum(t.cn.er, t.cn.ei), lsum(t.sn.er, t.sn.ei)) - le + 1.0;
    return approx(v, lsum(ee, l2(v) - static_cast<double>(wb) + 3.0));
}

BC imag_axis(jf f, const BF& y, const BF& m, const BCC& ctx) {
    const BF   m1 = one_minus_real(m);
    const BFC& cr = ctx.real();
    const BFC& ci = ctx.imaginary();
    switch (f) {
        case jf::sn: return BC(BF::zero(), math::jacobi_sc(y, m1, ci));
        case jf::cn: return BC(math::jacobi_nc(y, m1, cr), BF::zero());
        case jf::dn: return BC(math::jacobi_dc(y, m1, cr), BF::zero());
        case jf::ns: return BC(BF::zero(), -math::jacobi_cs(y, m1, ci));
        case jf::nc: return BC(math::jacobi_cn(y, m1, cr), BF::zero());
        case jf::nd: return BC(math::jacobi_cd(y, m1, cr), BF::zero());
        case jf::sc: return BC(BF::zero(), math::jacobi_sn(y, m1, ci));
        case jf::sd: return BC(BF::zero(), math::jacobi_sd(y, m1, ci));
        case jf::cs: return BC(BF::zero(), -math::jacobi_ns(y, m1, ci));
        case jf::cd: return BC(math::jacobi_nd(y, m1, cr), BF::zero());
        case jf::ds: return BC(BF::zero(), -math::jacobi_ds(y, m1, ci));
        default: return BC(math::jacobi_dn(y, m1, cr), BF::zero());
    }
}

BF real_fn(jf f, const BF& x, const BF& m, const BFC& c) {
    switch (f) {
        case jf::am: return math::jacobi_am(x, m, c);
        case jf::sn: return math::jacobi_sn(x, m, c);
        case jf::cn: return math::jacobi_cn(x, m, c);
        case jf::dn: return math::jacobi_dn(x, m, c);
        case jf::ns: return math::jacobi_ns(x, m, c);
        case jf::nc: return math::jacobi_nc(x, m, c);
        case jf::nd: return math::jacobi_nd(x, m, c);
        case jf::sc: return math::jacobi_sc(x, m, c);
        case jf::sd: return math::jacobi_sd(x, m, c);
        case jf::cs: return math::jacobi_cs(x, m, c);
        case jf::cd: return math::jacobi_cd(x, m, c);
        case jf::ds: return math::jacobi_ds(x, m, c);
        default: return math::jacobi_dc(x, m, c);
    }
}

BC jacobi_public(jf f, const BC& u, const BC& m, const BCC& ctx) {
    if (u.is_undefined() || m.is_undefined() || !u.is_finite() || !m.is_finite()) return BC::undefined();
    const bool mr = m.imaginary().is_zero();
    if (mr && u.imaginary().is_zero()) return BC(real_fn(f, u.real(), m.real(), ctx.real()), BF::zero());
    const CX U = cx(u), Mc = cx(m);
    const BF Mr = m.real();
    if (mr && f != jf::am && u.real().is_zero()) return imag_axis(f, u.imaginary(), Mr, ctx);
    if (!mr && (m.is_zero() || (Mc.im.is_zero() && BF::compare(Mc.re, BF::one()) == BF::ordering::equal))) return BC::undefined();
    return drive(ctx, [&](std::size_t w) {
        const Trip t = mr ? real_m(U, Mr, w) : complex_m(U, Mc, w);
        if (!t.ok) return approx_undefined();
        if (f == jf::am) return am_from(t, U, mr ? &Mr : nullptr, Mc, w);
        return approx(pick(f, t, w + 8));
    });
}

} // namespace

BC jacobi_am(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::am, u, m, ctx); }
BC jacobi_sn(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::sn, u, m, ctx); }
BC jacobi_cn(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::cn, u, m, ctx); }
BC jacobi_dn(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::dn, u, m, ctx); }
BC jacobi_ns(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::ns, u, m, ctx); }
BC jacobi_nc(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::nc, u, m, ctx); }
BC jacobi_nd(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::nd, u, m, ctx); }
BC jacobi_sc(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::sc, u, m, ctx); }
BC jacobi_sd(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::sd, u, m, ctx); }
BC jacobi_cs(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::cs, u, m, ctx); }
BC jacobi_cd(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::cd, u, m, ctx); }
BC jacobi_ds(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::ds, u, m, ctx); }
BC jacobi_dc(const BC& u, const BC& m, const BCC& ctx) { return jacobi_public(jf::dc, u, m, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
