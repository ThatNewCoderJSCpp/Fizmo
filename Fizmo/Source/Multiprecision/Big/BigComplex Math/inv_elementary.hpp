#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_INVERSE_ELEMENTARY_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_INVERSE_ELEMENTARY_HPP

#include "elementary.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace cxdetail {

struct ic_val {
    BF          v;
    std::size_t loss  = 0;
    bool        exact = true;
};

inline ic_val ic_exact(const BF& v) { ic_val r; r.v = v; return r; }

inline ic_val ic_rounded(const BF& v, std::size_t loss) {
    ic_val r;
    r.v     = v;
    r.loss  = loss;
    r.exact = false;
    return r;
}

inline std::size_t ic_max(std::size_t a, std::size_t b) noexcept { return a > b ? a : b; }

inline std::size_t ic_gap(const BF& from, const BF& to) noexcept { return positive_part(top_of(from) - top_of(to)); }

inline std::size_t ic_combine(const ic_val& x, const ic_val& y, std::size_t extra) noexcept {
    if (x.exact && y.exact) return 1;
    std::size_t l = 0;
    if (!x.exact) l = x.loss;
    if (!y.exact) l = ic_max(l, y.loss);
    return l + extra;
}

inline bcd::dyadic ic_dy(const BF& x) { return bcd::dy_mul(x, BF::one()); }

inline bool ic_from_dy(const bcd::dyadic& d, BF& out) {
    if (bcd::dy_bad(d)) return false;
    BF v = bcd::dy_exact(d);
    if (!v.is_finite()) return false;
    out = std::move(v);
    return true;
}

inline ic_val ic_neg(const ic_val& x)                   { ic_val r = x; r.v = -x.v;               return r; }
inline ic_val ic_abs(const ic_val& x)                   { ic_val r = x; r.v = x.v.abs();          return r; }
inline ic_val ic_scale(const ic_val& x, std::int64_t k) { ic_val r = x; r.v = x.v.scaled2(k);     return r; }
inline ic_val ic_signed_as(const ic_val& x, bool neg)   { ic_val r = x; r.v = x.v.with_sign(neg); return r; }

inline ic_val ic_fn(const BF& v, const ic_val& x)                   { return ic_rounded(v, ic_combine(x, x, 3)); }
inline ic_val ic_fn(const BF& v, const ic_val& x, const ic_val& y) { return ic_rounded(v, ic_combine(x, y, 3)); }

inline ic_val ic_add(const ic_val& x, const ic_val& y, const BFC& wc) {
    if (x.exact && y.exact) {
        BF e;
        if (ic_from_dy(dy_add(ic_dy(x.v), ic_dy(y.v)), e)) return ic_exact(e);
    }

    const BF v = BF::add(x.v, y.v, wc);

    if (v.is_zero()) {
        if (x.exact && y.exact) return ic_exact(v);       
        return ic_rounded(v, wc.precision);               
    }

    std::size_t l = 0;
    if (!x.exact) l = ic_max(l, x.loss + ic_gap(x.v, v));
    if (!y.exact) l = ic_max(l, y.loss + ic_gap(y.v, v));
    return ic_rounded(v, l + 2);
}

inline ic_val ic_sub(const ic_val& x, const ic_val& y, const BFC& wc) { return ic_add(x, ic_neg(y), wc); }

inline ic_val ic_mul(const ic_val& x, const ic_val& y, const BFC& wc) {
    if (x.exact && y.exact) {
        BF e;
        if (ic_from_dy(bcd::dy_mul(x.v, y.v), e)) return ic_exact(e);
    }

    const BF v = BF::mul(x.v, y.v, wc);
    if ((x.exact && x.v.is_zero()) || (y.exact && y.v.is_zero())) return ic_exact(v);
    return ic_rounded(v, ic_combine(x, y, 3));
}

inline ic_val ic_div(const ic_val& x, const ic_val& y, const BFC& wc) {
    const BF v = BF::div(x.v, y.v, wc);
    if (x.exact && x.v.is_zero()) return ic_exact(v);
    return ic_rounded(v, ic_combine(x, y, 3));
}

inline bool ic_store(const ic_val& re, const ic_val& im, work& r) {
    if (!re.v.is_finite() || !im.v.is_finite()) return false;
    r.re       = re.v;
    r.im       = im.v;
    r.loss_re  = re.loss;
    r.loss_im  = im.loss;
    r.exact_re = re.exact;
    r.exact_im = im.exact;
    return true;
}

inline void ic_csqrt(const ic_val& pr, const ic_val& pi, const BFC& wc, ic_val& sr, ic_val& si) {
    const bool   pneg = pi.v.signbit();
    const ic_val ar   = ic_abs(pr);
    const ic_val ai   = ic_abs(pi);
    const ic_val n2   = ic_add(ic_mul(ar, ar, wc), ic_mul(ai, ai, wc), wc);
    const ic_val m    = ic_fn(math::sqrt(n2.v, wc), n2);
    const ic_val h    = ic_scale(ic_add(ar, m, wc), -1);
    const ic_val t    = ic_fn(math::sqrt(h.v, wc), h);
    const ic_val u    = ic_scale(ic_div(ai, t, wc), -1);

    if (!pr.v.signbit()) { sr = t; si = ic_signed_as(u, pneg); }
    else                 { sr = u; si = ic_signed_as(t, pneg); }
}

template <typename F>
inline BF ic_signed(F&& f, bool neg, const BFC& c) { return neg ? -f(mirrored(c)) : f(c); }

inline BF ic_half_pi(bool neg, const BFC& c) {
    return ic_signed([](const BFC& cc) { return constants::half_pi(cc); }, neg, c);
}

inline BF::ordering ic_cmp1(const BF& v) { return BF::compare(v.abs(), BF::one()); }

inline int ic_dir(const BF& v) noexcept { return v.is_infinite() ? (v.signbit() ? -1 : 1) : 0; }
inline int ic_iabs(int v) noexcept { return v < 0 ? -v : v; }

inline BF ic_dir_angle(int u, int v, bool neg, const BFC& c) {
    if (v == 0 && u >= 0) return BF::zero(neg);
    const BF fv(static_cast<std::int64_t>(v)), fu(static_cast<std::int64_t>(u));
    return ic_signed([&](const BFC& cc) { return math::atan2(fv, fu, cc); }, neg, c);
}

inline BC ic_recip_special(const BF& a, const BF& b) {
    if (a.is_zero() && b.is_zero()) return BC(BF::infinity(a.signbit()), BF::zero(!b.signbit()));
    return BC(BF::zero(a.signbit()), BF::zero(!b.signbit()));
}

inline BC ic_rot(const BC& v) { return BC(v.imaginary(), -v.real()); }   

struct ic_arcs { ic_val x, y, pr, pi, qr, qi; };

inline void ic_arcs_direct(const BF& a, const BF& b, const BFC& wc, ic_arcs& s) {
    const ic_val one = ic_exact(BF::one());
    s.x  = ic_exact(a);
    s.y  = ic_exact(b);
    s.pr = ic_sub(one, s.x, wc);
    s.pi = ic_neg(s.y);
    s.qr = ic_add(one, s.x, wc);
    s.qi = s.y;
}

inline void ic_arcs_recip(const BF& a, const BF& b, const BFC& wc, ic_arcs& s) {
    const ic_val xa = ic_exact(a), yb = ic_exact(b);
    const ic_val n  = ic_add(ic_mul(xa, xa, wc), ic_mul(yb, yb, wc), wc);
    s.x  = ic_div(xa, n, wc);
    s.y  = ic_div(ic_neg(yb), n, wc);
    s.pr = ic_div(ic_sub(n, xa, wc), n, wc);
    s.pi = ic_div(yb, n, wc);
    s.qr = ic_div(ic_add(n, xa, wc), n, wc);
    s.qi = s.y;
}

enum class ic_arc : std::uint8_t { asin_v, acos_v };

inline bool ic_arc_at(ic_arc k, bool recip, const BF& a, const BF& b, std::size_t w, work& r) {
    const BFC wc = work_ctx(w);
    ic_arcs s;
    if (recip) ic_arcs_recip(a, b, wc, s);
    else       ic_arcs_direct(a, b, wc, s);
    ic_val ar, ai, br, bi;
    ic_csqrt(s.pr, s.pi, wc, ar, ai);
    ic_csqrt(s.qr, s.qi, wc, br, bi);

    if (k == ic_arc::asin_v) {
        const ic_val d = ic_sub(ic_mul(ar, br, wc), ic_mul(ai, bi, wc), wc);
        const ic_val e = ic_sub(ic_mul(ar, bi, wc), ic_mul(ai, br, wc), wc);
        return ic_store(ic_fn(math::atan2(s.x.v, d.v, wc), s.x, d), ic_fn(math::arcsinh(e.v, wc), e), r);
    }

    const ic_val e = ic_sub(ic_mul(br, ai, wc), ic_mul(bi, ar, wc), wc);
    return ic_store(ic_scale(ic_fn(math::atan2(ar.v, br.v, wc), ar, br), 1), ic_fn(math::arcsinh(e.v, wc), e), r);
}

inline BC ic_arc_ziv(ic_arc k, bool recip, const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    auto f = [&](std::size_t w, work& r) { return ic_arc_at(k, recip, a, b, w, r); };
    return ziv(f, no_probe{}, cr, ci, 32);
}

inline BC ic_asin_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (a.is_infinite() || b.is_infinite()) {
        return BC(ic_dir_angle(ic_iabs(ic_dir(b)), ic_iabs(ic_dir(a)), a.signbit(), cr), BF::infinity(b.signbit()));
    }

    if (a.is_zero() && b.is_zero()) return BC(a, b);

    if (b.is_zero()) {
        if (ic_cmp1(a) != BF::ordering::greater) return BC(math::arcsin(a, cr), b);
        const BF aa = a.abs();
        return BC(ic_half_pi(a.signbit(), cr), ic_signed([&](const BFC& c) { return math::arccosh(aa, c); }, b.signbit(), ci));
    }

    if (a.is_zero()) return BC(a, math::arcsinh(b, ci));
    return ic_arc_ziv(ic_arc::asin_v, false, a, b, cr, ci);
}

inline BC ic_acos_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (a.is_infinite() || b.is_infinite()) {
        return BC(ic_dir_angle(ic_dir(a), ic_iabs(ic_dir(b)), false, cr), BF::infinity(!b.signbit()));
    }

    if (b.is_zero()) {                                                  
        if (ic_cmp1(a) != BF::ordering::greater) return BC(math::arccos(a, cr), BF::zero(!b.signbit()));
        const BF aa = a.abs();
        const BF re = a.signbit() ? constants::pi(cr) : BF::zero();
        return BC(re, ic_signed([&](const BFC& c) { return math::arccosh(aa, c); }, !b.signbit(), ci));
    }

    if (a.is_zero()) return BC(constants::half_pi(cr), -math::arcsinh(b, mirrored(ci)));
    return ic_arc_ziv(ic_arc::acos_v, false, a, b, cr, ci);
}

inline BC ic_acsc_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {     
    if ((a.is_zero() && b.is_zero()) || a.is_infinite() || b.is_infinite()) {
        const BC w = ic_recip_special(a, b);
        return ic_asin_eval(w.real(), w.imaginary(), cr, ci);
    }

    if (b.is_zero()) {
        if (ic_cmp1(a) != BF::ordering::less) return BC(math::arccsc(a, cr), BF::zero(!b.signbit()));
        const BF aa = a.abs();
        return BC(ic_half_pi(a.signbit(), cr), ic_signed([&](const BFC& c) { return math::arcsech(aa, c); }, !b.signbit(), ci));
    }

    if (a.is_zero()) return BC(a, -math::arccsch(b, mirrored(ci)));
    return ic_arc_ziv(ic_arc::asin_v, true, a, b, cr, ci);
}

inline BC ic_asec_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {      
    if ((a.is_zero() && b.is_zero()) || a.is_infinite() || b.is_infinite()) {
        const BC w = ic_recip_special(a, b);
        return ic_acos_eval(w.real(), w.imaginary(), cr, ci);
    }

    if (b.is_zero()) {
        if (ic_cmp1(a) != BF::ordering::less) return BC(math::arcsec(a, cr), BF::zero(b.signbit()));
        const BF aa = a.abs();
        const BF re = a.signbit() ? constants::pi(cr) : BF::zero();
        return BC(re, ic_signed([&](const BFC& c) { return math::arcsech(aa, c); }, b.signbit(), ci));
    }

    if (a.is_zero()) return BC(constants::half_pi(cr), math::arccsch(b, ci));
    return ic_arc_ziv(ic_arc::acos_v, true, a, b, cr, ci);
}

inline ic_val ic_log_part(const BF& t, const BF& o, const ic_val& n, const BFC& wc) {
    const ic_val one = ic_exact(BF::one());
    const ic_val t2  = ic_exact(t.scaled2(1));
    const ic_val s   = ic_add(one, n, wc);
    const ic_val u   = ic_div(t2, s, wc);

    if (BF::compare(u.v.abs(), BF(BigUInt::one(), false, -1)) == BF::ordering::less) {
        return ic_scale(ic_fn(math::arctanh(u.v, wc), u), -1);
    }

    const ic_val tv = ic_exact(t), ov = ic_exact(o);
    const ic_val o2 = ic_mul(ov, ov, wc);
    const ic_val up = ic_add(one, tv, wc);
    const ic_val dn = ic_sub(one, tv, wc);
    const ic_val q  = ic_div(ic_add(ic_mul(up, up, wc), o2, wc), ic_add(ic_mul(dn, dn, wc), o2, wc), wc);
    return ic_scale(ic_fn(math::ln(q.v, wc), q), -2);
}

inline ic_val ic_half_atan2(const ic_val& y, const ic_val& x, const BFC& wc) {
    return ic_scale(ic_fn(math::atan2(y.v, x.v, wc), y, x), -1);
}

enum class ic_tan : std::uint8_t { atan_v, acot_v, atanh_v, acoth_v };

inline bool ic_tan_at(ic_tan k, const BF& a, const BF& b, std::size_t w, work& r) {
    const BFC    wc  = work_ctx(w);
    const ic_val one = ic_exact(BF::one());
    const ic_val xa  = ic_exact(a), yb = ic_exact(b);
    const ic_val n   = ic_add(ic_mul(xa, xa, wc), ic_mul(yb, yb, wc), wc);
    const ic_val omn = ic_sub(one, n, wc);                  
    const ic_val a2  = ic_exact(a.scaled2(1));
    const ic_val b2  = ic_exact(b.scaled2(1));

    switch (k) {
        case ic_tan::atanh_v: return ic_store(ic_log_part(a, b, n, wc), ic_half_atan2(b2, omn, wc), r);
        case ic_tan::acoth_v: return ic_store(ic_log_part(a, b, n, wc), ic_half_atan2(ic_neg(b2), ic_neg(omn), wc), r);
        case ic_tan::atan_v:  return ic_store(ic_half_atan2(a2, omn, wc), ic_log_part(b, a, n, wc), r);
        default: break;
    }

    ic_val re;

    if (!a.signbit()) {
        re = ic_half_atan2(a2, ic_neg(omn), wc);
    } else {
        const ic_val p = ic_rounded(constants::pi(wc), 1);
        const ic_val t = ic_fn(math::atan2(-a2.v, omn.v, wc), a2, omn);   
        re = ic_scale(ic_add(p, t, wc), -1);
    }

    return ic_store(re, ic_neg(ic_log_part(b, a, n, wc)), r);
}

inline BC ic_tan_ziv(ic_tan k, const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    auto f = [&](std::size_t w, work& r) { return ic_tan_at(k, a, b, w, r); };
    return ziv(f, no_probe{}, cr, ci, 32);
}

inline BC ic_atanh_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (a.is_infinite() || b.is_infinite()) return BC(BF::zero(a.signbit()), ic_half_pi(b.signbit(), ci));
    if (a.is_zero() && b.is_zero()) return BC(a, b);

    if (b.is_zero()) {
        const BF::ordering o = ic_cmp1(a);
        if (o == BF::ordering::less)  return BC(math::arctanh(a, cr), b);
        if (o == BF::ordering::equal) return BC(BF::infinity(a.signbit()), b);
        return BC(math::arccoth(a, cr), ic_half_pi(b.signbit(), ci));
    }

    if (a.is_zero()) return BC(a, math::arctan(b, ci));
    return ic_tan_ziv(ic_tan::atanh_v, a, b, cr, ci);
}

inline BC ic_acoth_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {    
    if ((a.is_zero() && b.is_zero()) || a.is_infinite() || b.is_infinite()) {
        const BC w = ic_recip_special(a, b);
        return ic_atanh_eval(w.real(), w.imaginary(), cr, ci);
    }

    if (b.is_zero()) {
        const BF::ordering o   = ic_cmp1(a);
        const BF           imz = BF::zero(!b.signbit());
        if (o == BF::ordering::greater) return BC(math::arccoth(a, cr), imz);
        if (o == BF::ordering::equal)   return BC(BF::infinity(a.signbit()), imz);
        return BC(math::arctanh(a, cr), ic_half_pi(!b.signbit(), ci));
    }

    if (a.is_zero()) {                                   
        const BF ab = b.abs();
        return BC(a, ic_signed([&](const BFC& c) { return math::arccot(ab, c); }, !b.signbit(), ci));
    }

    return ic_tan_ziv(ic_tan::acoth_v, a, b, cr, ci);
}

inline BC ic_atan_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (a.is_infinite() || b.is_infinite()) return BC(ic_half_pi(a.signbit(), cr), BF::zero(b.signbit()));
    if (a.is_zero() && b.is_zero()) return BC(a, b);
    if (b.is_zero()) return BC(math::arctan(a, cr), b);

    if (a.is_zero()) {
        const BF::ordering o = ic_cmp1(b);
        if (o == BF::ordering::less)  return BC(a, math::arctanh(b, ci));
        if (o == BF::ordering::equal) return BC(a, BF::infinity(b.signbit()));
        return BC(ic_half_pi(a.signbit(), cr), math::arccoth(b, ci));
    }

    return ic_tan_ziv(ic_tan::atan_v, a, b, cr, ci);
}

inline BC ic_acot_eval(const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (a.is_infinite() || b.is_infinite()) {
        return BC(a.signbit() ? constants::pi(cr) : BF::zero(), BF::zero(!b.signbit()));
    }

    if (b.is_zero()) {                                                  // includes z == 0
        if (a.is_zero()) return BC(constants::half_pi(cr), BF::zero(!b.signbit()));
        return BC(math::arccot(a, cr), BF::zero(!b.signbit()));
    }

    if (a.is_zero()) {
        const BF::ordering o = ic_cmp1(b);
        if (o == BF::ordering::less)  return BC(constants::half_pi(cr), -math::arctanh(b, mirrored(ci)));
        if (o == BF::ordering::equal) return BC(constants::half_pi(cr), BF::infinity(!b.signbit()));
        return BC(a.signbit() ? constants::pi(cr) : BF::zero(), -math::arccoth(b, mirrored(ci)));
    }

    return ic_tan_ziv(ic_tan::acot_v, a, b, cr, ci);
}

#define FIZMO_CX_INV_DIRECT(NAME, EVAL)                                                          \
    inline BC NAME(const BC& z, const BFC& cr, const BFC& ci) {                                  \
        if (z.is_undefined()) return BC::undefined();                                            \
        return EVAL(z.real(), z.imaginary(), cr, ci);                                            \
    }

FIZMO_CX_INV_DIRECT(arcsin_eval,  ic_asin_eval)
FIZMO_CX_INV_DIRECT(arccos_eval,  ic_acos_eval)
FIZMO_CX_INV_DIRECT(arctan_eval,  ic_atan_eval)
FIZMO_CX_INV_DIRECT(arccot_eval,  ic_acot_eval)
FIZMO_CX_INV_DIRECT(arcsec_eval,  ic_asec_eval)
FIZMO_CX_INV_DIRECT(arccsc_eval,  ic_acsc_eval)
FIZMO_CX_INV_DIRECT(arctanh_eval, ic_atanh_eval)
FIZMO_CX_INV_DIRECT(arccoth_eval, ic_acoth_eval)

#undef FIZMO_CX_INV_DIRECT

inline BC arcsinh_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    return ic_rot(ic_asin_eval(-z.imaginary(), z.real(), mirrored(ci), cr));
}

inline BC arccsch_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    return ic_rot(ic_acsc_eval(z.imaginary(), -z.real(), mirrored(ci), cr));
}

inline BC arccosh_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    const BF& a = z.real();
    const BF& b = z.imaginary();

    if (!b.signbit()) {
        const BC v = ic_acos_eval(a, b, ci, mirrored(cr));
        return BC(-v.imaginary(), v.real());
    }

    const BC v = ic_acos_eval(a, b, mirrored(ci), cr);
    return BC(v.imaginary(), -v.real());
}

inline BC arcsech_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    const BF& a = z.real();
    const BF& b = z.imaginary();

    if (b.signbit()) {
        const BC v = ic_asec_eval(a, b, ci, mirrored(cr));
        return BC(-v.imaginary(), v.real());
    }

    const BC v = ic_asec_eval(a, b, mirrored(ci), cr);
    return BC(v.imaginary(), -v.real());
}

} // namespace cxdetail

#define FIZMO_CX_INV(NAME)                                                                       \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                        \
    inline BigComplex NAME(const C& z, const BigComplexContext& ctx) {                           \
        return cxdetail::NAME##_eval(z, ctx.real(), ctx.imaginary());                            \
    }                                                                                            \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                        \
    inline BigComplex NAME(const C& z) { return NAME(z, BigComplexContext::current()); }

FIZMO_CX_INV(arcsin)
FIZMO_CX_INV(arccos)
FIZMO_CX_INV(arctan)
FIZMO_CX_INV(arccot)
FIZMO_CX_INV(arcsec)
FIZMO_CX_INV(arccsc)
FIZMO_CX_INV(arcsinh)
FIZMO_CX_INV(arccosh)
FIZMO_CX_INV(arctanh)
FIZMO_CX_INV(arccoth)
FIZMO_CX_INV(arcsech)
FIZMO_CX_INV(arccsch)

#undef FIZMO_CX_INV

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_INVERSE_ELEMENTARY_HPP