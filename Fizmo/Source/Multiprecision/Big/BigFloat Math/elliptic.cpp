#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace eldetail {

BF el_xmul(const BF& x, const BF& y) {                                     
    if (x.is_zero() || y.is_zero()) return BF::zero();
    return BF::mul(x, y, csdetail::cs_ctx(x.significand().bit_length() + y.significand().bit_length() + 1));
}

cs_val el_rj_any(const BF& x, const BF& y, const BF& z, const BF& p, std::size_t want) {
    if (p.is_zero())  return el_undef();
    if (!p.signbit()) return csdetail::cs_rj_raw(x, y, z, p, want);
    BF lo, mid, hi;
    csdetail::cs_sort3(x, y, z, lo, mid, hi);
    return csdetail::cs_rj_pv_raw(lo, mid, hi, p, want);
}

bool el_reduce(const BF& phi, std::size_t want, el_red& r) {
    if (BF::compare(phi.abs(), BF(1.5)) == BF::ordering::less) {
        r.j     = 0;
        r.theta = phi;
        return true;
    }

    const std::int64_t e = phi.get_exp_base2();
    if (e > 60) return false;
    const std::size_t eb = static_cast<std::size_t>(e > 0 ? e : 0);
    const BFC         qc = csdetail::cs_ctx(eb + 80);
    const BF          q  = BF::mul(phi.abs(), constants::inv_pi(qc), qc);
    std::int64_t      j  = static_cast<std::int64_t>(BF::add(q, BF::one().scaled_pow2(-1), qc).get_integer_part().get_lowest_bits());
    if (phi.signbit()) j = -j;
    const BFC rc = csdetail::cs_ctx(want + eb + 28);
    const BF  pi = constants::pi(rc);
    const BF  hp = pi.scaled_pow2(-1);
    BF theta = BF::sub(phi, BF::mul(BF(j), pi, rc), rc);

    if (BF::compare(theta, hp) == BF::ordering::greater) {
        ++j;
        theta = BF::sub(theta, pi, rc);
    } else if (BF::compare(theta, -hp) == BF::ordering::less) {
        --j;
        theta = BF::add(theta, pi, rc);
    }

    r.j     = j;
    r.theta = theta;
    return true;
}

cs_val el_complete_raw(el_kind kind, const BF& n, const BF& m, std::size_t want) {
    using namespace csdetail;
    const BFC wc    = cs_ctx(want);
    const BF  one   = BF::one();
    const BF  zero  = BF::zero();
    const BF  three = BF(static_cast<std::uint64_t>(3));
    if (kind == el_kind::e && el_is(m, 1)) return cs_val{one, 0.0};                 
    const BF omm = cs_xsub(one, m);                                                 
    if (omm.is_zero() || omm.signbit()) return el_undef();
    const cs_val rf = cs_rf_raw(zero, omm, one, want);
    if (!rf.v.is_finite() || kind == el_kind::f) return rf;
    BF     t2;
    double l2;

    if (kind == el_kind::e) {
        const cs_val rd = cs_rd_raw(zero, omm, one, want);
        if (!rd.v.is_finite()) return rd;
        t2 = -BF::div(BF::mul(m, rd.v, wc), three, wc);
        l2 = cs_l2hi(t2) + std::log2(std::exp2(rd.lost) + 2.0);
    } else {
        const BF omn = cs_xsub(one, n);
        if (omn.is_zero()) return el_undef();                                       
        const cs_val rj = el_rj_any(zero, omm, one, omn, want);
        if (!rj.v.is_finite()) return rj;
        t2 = BF::div(BF::mul(n, rj.v, wc), three, wc);
        l2 = cs_l2hi(t2) + std::log2(std::exp2(rj.lost) + 2.0);
    }

    const BF v = BF::add(rf.v, t2, wc);
    if (v.is_zero()) return cs_val{v, 1.0e9};
    const double lv    = cs_l2lo(v);
    const double units = std::exp2(cs_l2hi(rf.v) + rf.lost - lv) + std::exp2(l2 - lv) + 1.0;
    return cs_val{v, std::log2(units) + 1.0};
}

cs_val el_incomplete_raw(el_kind kind, const BF& phi, const BF& n, const BF& m, std::size_t want) {
    using namespace csdetail;
    const BFC wc    = cs_ctx(want);
    const BF  one   = BF::one();
    const BF  three = BF(static_cast<std::uint64_t>(3));
    el_red red;
    if (!el_reduce(phi, want, red)) return el_undef();
    if (red.j != 0 && el_gt_one(m)) return el_nan();                                  
    const BF   s0    = sin(red.theta, wc);
    const BF   c0    = cos(red.theta, wc);
    const bool use_s = BF::compare(s0.abs(), c0.abs()) != BF::ordering::greater;
    BF s, s2, c2;

    if (use_s) {
        s  = s0;
        s2 = el_xmul(s, s);
        c2 = cs_xsub(one, s2);
    } else {
        c2 = el_xmul(c0, c0);
        s2 = cs_xsub(one, c2);
        s  = sqrt(s2, wc);
        if (s0.signbit()) s = -s;
    }

    const BF Y = cs_xsub(one, el_xmul(m, s2));                                      
    if (cs_neg(Y)) return el_nan();
    const cs_val rf = cs_rf_raw(c2, Y, one, want);
    if (!rf.v.is_finite()) return rf;
    const BF t1 = BF::mul(s, rf.v, wc);
    double   l1 = cs_l2hi(t1) + std::log2(std::exp2(rf.lost) + (use_s ? 1.0 : 2.0));
    double   l2 = -std::numeric_limits<double>::infinity();
    BF       v  = t1;
    const double Dd = std::sqrt(std::max(0.0, cs_to_double(Y)));
    double gprime;                                                                 

    if (kind == el_kind::f) {
        gprime = 1.0 / Dd;
    } else {
        const BF s3 = use_s ? el_xmul(s, s2) : BF::mul(s, s2, wc);                  

        if (kind == el_kind::e) {
            const cs_val rd = cs_rd_raw(c2, Y, one, want);
            if (!rd.v.is_finite()) return rd;
            const BF t2 = BF::div(BF::mul(BF::mul(m, s3, wc), rd.v, wc), three, wc);
            v      = BF::sub(t1, t2, wc);
            l2     = cs_l2hi(t2) + std::log2(std::exp2(rd.lost) + (use_s ? 3.0 : 5.0));
            gprime = Dd;
        } else {
            const BF P = cs_xsub(one, el_xmul(n, s2));                              
            if (P.is_zero()) return el_undef();                                     
            const cs_val rj = el_rj_any(c2, Y, one, P, want);
            if (!rj.v.is_finite()) return rj;
            const BF t2 = BF::div(BF::mul(BF::mul(n, s3, wc), rj.v, wc), three, wc);
            v      = BF::add(t1, t2, wc);
            l2     = cs_l2hi(t2) + std::log2(std::exp2(rj.lost) + (use_s ? 3.0 : 5.0));
            gprime = 1.0 / (Dd * std::fabs(cs_to_double(P)));
        }
    }

    const double sd    = std::fabs(cs_to_double(s0));
    const double cd    = std::fabs(cs_to_double(c0));
    const double big   = use_s ? cd : sd;
    const double small = use_s ? sd : cd;
    const double shift = (red.j != 0) ? big * std::ldexp(1.0, -8) : 0.0;
    const double lsens = std::log2((small + shift) / big * gprime + 1.0e-300);

    BF     total = v;
    double lper  = -std::numeric_limits<double>::infinity();

    if (red.j != 0) {
        const cs_val C = el_complete_raw(kind, n, m, want);
        if (!C.v.is_finite()) return C;
        const BF per = BF::mul(BF(static_cast<std::int64_t>(2 * red.j)), C.v, wc);
        total = BF::add(per, v, wc);
        lper  = cs_l2hi(per) + std::log2(std::exp2(C.lost) + 1.0);
    }

    if (total.is_zero()) return cs_val{total, 1.0e9};
    const double lv    = cs_l2lo(total);
    const double units = std::exp2(l1 - lv) + std::exp2(l2 - lv) + std::exp2(lper - lv) + std::exp2(lsens - lv) + 2.0;
    return cs_val{total, std::log2(units) + 1.0};
}

} // namespace eldetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat elliptic_f(const BigFloat& phi, const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(phi) || el_bad(m)) return el_bad_of(phi, m, m);
    if (m.is_infinite())          return BigFloat::undefined();
    if (phi.is_zero())            return phi;
    if (m.is_zero())              return phi.rounded(ctx);                                  
    if (phi.is_infinite())        return (BigFloat::compare(m, BigFloat::one()) == BigFloat::ordering::less) ? phi : BigFloat::undefined();
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_incomplete_raw(el_kind::f, phi, BigFloat::zero(), m, want); });
}

BigFloat elliptic_e(const BigFloat& phi, const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(phi) || el_bad(m)) return el_bad_of(phi, m, m);
    if (m.is_infinite())          return BigFloat::undefined();
    if (phi.is_zero())            return phi;
    if (m.is_zero())              return phi.rounded(ctx);                                  
    if (phi.is_infinite())        return el_gt_one(m) ? BigFloat::undefined() : phi;
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_incomplete_raw(el_kind::e, phi, BigFloat::zero(), m, want); });
}

BigFloat elliptic_pi(const BigFloat& phi, const BigFloat& n, const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(phi) || el_bad(n) || el_bad(m))            return el_bad_of(phi, n, m);
    if (phi.is_infinite() || n.is_infinite() || m.is_infinite()) return BigFloat::undefined();
    if (phi.is_zero())                                    return phi;
    if (n.is_zero())                                      return elliptic_f(phi, m, ctx);
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_incomplete_raw(el_kind::pi, phi, n, m, want); });
}

BigFloat elliptic_k(const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(m))       return el_bad_of(m, m, m);
    if (m.is_infinite()) return m.signbit() ? BigFloat::zero() : BigFloat::nan();
    if (m.is_zero())     return constants::half_pi(ctx);
    if (el_is(m, 1))     return BigFloat::infinity();
    if (el_gt_one(m))    return BigFloat::nan();
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_complete_raw(el_kind::f, BigFloat::zero(), m, want); });
}

BigFloat elliptic_e(const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(m))       return el_bad_of(m, m, m);
    if (m.is_infinite()) return m.signbit() ? BigFloat::infinity() : BigFloat::nan();
    if (m.is_zero())     return constants::half_pi(ctx);
    if (el_is(m, 1))     return BigFloat::one();
    if (el_gt_one(m))    return BigFloat::nan();
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_complete_raw(el_kind::e, BigFloat::zero(), m, want); });
}

BigFloat elliptic_pi(const BigFloat& n, const BigFloat& m, const BigFloatContext& ctx) {
    using namespace eldetail;
    if (el_bad(n) || el_bad(m))            return el_bad_of(n, m, m);
    if (n.is_infinite() || m.is_infinite()) return BigFloat::undefined();
    if (n.is_zero())                       return elliptic_k(m, ctx);
    if (el_gt_one(m))                      return BigFloat::nan();
    if (el_is(n, 1) || el_is(m, 1))        return BigFloat::infinity();
    return csdetail::cs_drive(ctx, [&](std::size_t want) { return el_complete_raw(el_kind::pi, n, m, want); });
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
