#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_ELEMENTARY_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_ELEMENTARY_HPP

#include "../big_complex.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace cxdetail {

namespace bcd = ::fizmo::multiprecision::bcdetail;

using BF  = BigFloat;
using BFC = BigFloatContext;
using BC  = BigComplex;
using BCC = BigComplexContext;

constexpr std::size_t guard_cap = 4096;

inline bool guard_exhausted(std::size_t prec, std::size_t guard) noexcept {
    return guard >= guard_cap || prec + guard >= BFC::max_prec / 4;
}

inline BFC work_ctx(std::size_t w) noexcept { return BFC(w, RoundingMode::nearest_even); }

inline BFC mirrored(const BFC& c) noexcept {
    RoundingMode m = c.rounding_mode;
    if      (m == RoundingMode::toward_pos_inf) m = RoundingMode::toward_neg_inf;
    else if (m == RoundingMode::toward_neg_inf) m = RoundingMode::toward_pos_inf;
    return BFC(c.precision, m);
}

inline std::int64_t top_of(const BF& v) noexcept {
    const std::int64_t e = v.get_exp_base2();
    return (e == BF::exp_none || e == BF::exp_inf) ? 0 : e;
}

inline std::int64_t max_i64(std::int64_t a, std::int64_t b) noexcept { return a > b ? a : b; }
inline std::size_t  positive_part(std::int64_t v) noexcept { return v > 0 ? static_cast<std::size_t>(v) : 0; }

struct work {
    BF          re, im;
    std::size_t loss_re  = 0, loss_im  = 0;
    bool        exact_re = false, exact_im = false;
};

inline bool rounding_is_safe(const BF& v, std::size_t w, std::size_t loss, std::size_t prec) {
    if (!v.is_finite() || v.is_zero()) return false;
    BigUInt           m   = v.significand();          
    const std::size_t L   = m.bit_length();
    std::size_t       err = loss;
    std::size_t       len = L;
    if (L < w) { m.shift_left_mutable(w - L); len = w; }
    else       { err += L - w; }
    if (err + prec + 2 > len) return false;
    return constants::bfdetail::round_is_safe(m, prec, err);
}

inline bool near_zero(const BF& v, std::size_t w, std::size_t loss) noexcept {
    return v.is_zero() || loss + 1 >= w;
}

inline void modulus_loss(work& r, std::size_t loss) {
    const bool         rz = r.re.is_zero(), iz = r.im.is_zero();
    const std::int64_t er = top_of(r.re), ei = top_of(r.im);
    const std::int64_t M  = rz ? ei : (iz ? er : max_i64(er, ei));
    r.loss_re = loss + (rz ? 0 : static_cast<std::size_t>(M - er));
    r.loss_im = loss + (iz ? 0 : static_cast<std::size_t>(M - ei));
}

struct no_probe {
    bool operator()(const work&, std::size_t, work&) const { return false; }
};

template <typename F, typename P>
inline BC ziv(F&& f, P&& probe, const BFC& cr, const BFC& ci, std::size_t guard) {
    const std::size_t p = (cr.precision > ci.precision) ? cr.precision : ci.precision;

    for (;;) {
        const std::size_t w = BFC::clamp_precision(p + guard);
        work r;
        if (!f(w, r)) return BC::undefined();

        if (r.re.is_finite() && r.im.is_finite()) {
            const bool ok_re = r.exact_re || rounding_is_safe(r.re, w, r.loss_re, cr.precision);
            const bool ok_im = r.exact_im || rounding_is_safe(r.im, w, r.loss_im, ci.precision);

            if (!(ok_re && ok_im)) {
                work ex;
                if (probe(r, w, ex)) return BC(ex.re.rounded(cr), ex.im.rounded(ci));
                if (!guard_exhausted(p, guard)) { guard *= 2; continue; }
                if (!ok_re && near_zero(r.re, w, r.loss_re)) r.re = BF::zero();
                if (!ok_im && near_zero(r.im, w, r.loss_im)) r.im = BF::zero();
            }
        }

        return BC(r.re.rounded(cr), r.im.rounded(ci));
    }
}

inline bcd::dyadic dy_add(const bcd::dyadic& x, const bcd::dyadic& y) {
    if (bcd::dy_bad(x) || bcd::dy_bad(y)) return bcd::dy_bad_value();
    if (bcd::dy_zero(x)) return y;
    if (bcd::dy_zero(y)) return x;
    return bcd::dy_add_exact(x, y);
}

inline bool exact_cmul(const BF& a, const BF& b, const BF& c, const BF& d, BF& re, BF& im) {
    const bcd::dyadic r = dy_add(bcd::dy_mul(a, c), bcd::dy_neg(bcd::dy_mul(b, d)));
    const bcd::dyadic i = dy_add(bcd::dy_mul(a, d), bcd::dy_mul(b, c));
    if (bcd::dy_bad(r) || bcd::dy_bad(i)) return false;
    BF x = bcd::dy_exact(r), y = bcd::dy_exact(i);
    if (!x.is_finite() || !y.is_finite()) return false;
    re = std::move(x);
    im = std::move(y);
    return true;
}

inline std::size_t width_of(const BF& x, const BF& y) {
    const bool xz = x.is_zero(), yz = y.is_zero();
    if (xz && yz) return 0;
    if (xz) return y.significand_bits();
    if (yz) return x.significand_bits();
    const std::int64_t tx = x.exponent() + static_cast<std::int64_t>(x.significand_bits());
    const std::int64_t ty = y.exponent() + static_cast<std::int64_t>(y.significand_bits());
    const std::int64_t lo = (x.exponent() < y.exponent()) ? x.exponent() : y.exponent();
    return static_cast<std::size_t>(max_i64(tx, ty) - lo);
}

inline bool exact_cpow(BF a, BF b, std::uint64_t n, std::size_t budget, BF& ra, BF& rb) {
    if (width_of(a, b) > budget) return false;
    BF xa = BF::one(), xb = BF::zero();

    for (;;) {
        if (n & 1u) {
            BF u, v;
            if (!exact_cmul(xa, xb, a, b, u, v) || width_of(u, v) > budget) return false;
            xa = std::move(u);
            xb = std::move(v);
        }

        n >>= 1;
        if (n == 0) break;
        BF u, v;
        if (!exact_cmul(a, b, a, b, u, v) || width_of(u, v) > budget) return false;
        a = std::move(u);
        b = std::move(v);
    }

    ra = std::move(xa);
    rb = std::move(xb);
    return true;
}

inline BF candidate(const BF& v, std::size_t w, std::size_t loss) {
    if (v.is_zero() || loss + 8 >= w) return BF::zero();
    return v.rounded(work_ctx(w - loss - 4));
}

struct root_probe {
    const BC*     z;
    std::int64_t  m;
    std::uint64_t q;   // 0 disables the probe

    mutable bool        tried = false, ready = false;
    mutable BF          zr, zi;
    mutable std::size_t zbits = 0;

    root_probe(const BC& zz, std::int64_t mm, std::uint64_t qq) : z(&zz), m(mm), q(qq) {}

    static std::size_t budget() noexcept { return BigUInt::max_bits / 4; }
    std::uint64_t      am() const noexcept { return detail::ln_abs_u64(m); }

    bool prepare() const {
        if (tried) return ready;
        tried = true;
        const BF N = z->magnitude_squared_exact();
        if (!N.is_finite() || N.is_zero()) return false;
        zbits = N.significand_bits();
        ready = exact_cpow(z->real(), z->imaginary(), am(), budget(), zr, zi);
        return ready;
    }

    bool operator()(const work& r, std::size_t w, work& out) const {
        if (q == 0 || q > 4096 || m == 0 || am() > 4096) return false;
        const BF x = candidate(r.re, w, r.loss_re);
        const BF y = candidate(r.im, w, r.loss_im);
        if ((x.is_zero() && y.is_zero()) || !prepare()) return false;
        const BF nc = BC(x, y).magnitude_squared_exact();
        if (!nc.is_finite() || nc.is_zero()) return false;
        const std::uint64_t cb = nc.significand_bits(), zb = zbits, a = am();

        if (m < 0) {
            if (cb != 1 || zb != 1) return false;
        } else if (q * cb < a * (zb - 1) + 1 || a * zb < q * (cb - 1) + 1) {
            return false;
        }

        BF pr, pi;
        if (!exact_cpow(x, y, q, budget(), pr, pi)) return false;

        if (m > 0) {
            if (!(pr == zr) || !(pi == zi)) return false;
        } else {
            BF ur, ui;
            if (!exact_cmul(pr, pi, zr, zi, ur, ui) || !(ur == BF::one()) || !ui.is_zero()) return false;
        }

        out.re       = x;
        out.im       = y;
        out.exact_re = out.exact_im = true;
        return true;
    }
};

inline BC direction(const BC& z) {
    const BF& a = z.real();
    const BF& b = z.imaginary();
    if (z.is_zero()) return BC(BF::one(a.signbit()), BF::zero(b.signbit()));

    if (z.is_infinite()) {
        return BC(a.is_infinite() ? BF::one(a.signbit()) : BF::zero(a.signbit()),
                  b.is_infinite() ? BF::one(b.signbit()) : BF::zero(b.signbit()));
    }

    return z;
}

inline bool sincos_signs(const BF& b, bool& s_neg, bool& c_neg) {
    const BFC lc = work_ctx(16);
    const BF  s  = math::sin(b, lc);
    const BF  c  = math::cos(b, lc);
    if (!s.is_finite() || !c.is_finite()) return false;
    s_neg = s.signbit();
    c_neg = c.signbit();
    return true;
}

enum class log_base : std::uint8_t { e, two, ten };

inline BF real_log(const BF& x, log_base base, const BFC& c) {
    switch (base) {
        case log_base::two: return math::log2(x, c);
        case log_base::ten: return math::log10(x, c);
        default:            return math::ln(x, c);
    }
}

inline BF log_scale(log_base base, const BFC& wc) {
    return (base == log_base::two) ? constants::inv_ln2(wc) : constants::inv_ln10(wc);
}

inline BF scaled_argument(const BC& z, std::int64_t branch, log_base base, const BFC& ci) {
    if (base == log_base::e) return z.argument(branch, ci);
    std::size_t guard = 32;

    for (;;) {
        const std::size_t w  = ci.precision + guard;
        const BFC         wc = work_ctx(w);
        const BF          th = z.argument(branch, wc);
        if (!th.is_finite() || th.is_zero()) return th;              
        const BF v = BF::mul(th, log_scale(base, wc), wc);
        if (rounding_is_safe(v, w, 2, ci.precision) || guard_exhausted(ci.precision, guard)) return v.rounded(ci);
        guard *= 2;
    }
}

inline BF log_modulus_spread(const BF& a, const BF& b, log_base base, const BFC& cr) {
    const bool a_big = top_of(a) >= top_of(b);
    const BF   L     = (a_big ? a : b).abs();
    const BF   S     = (a_big ? b : a).abs();
    const bool unit  = detail::is_exactly_one(L);

    if (unit && base == log_base::e) {
        const BF half_t = BF::mul(S, S, BFC(BFC::max_prec)).scaled2(-1);
        const BF q      = BF::mul(half_t, half_t, work_ctx(16));
        return BF::sub(half_t, q, cr);
    }

    std::size_t guard = 32;

    for (;;) {
        const std::size_t w      = cr.precision + guard;
        const BFC         wc     = work_ctx(w);
        const BF          ratio  = BF::div(S, L, wc);
        const BF          half_t = BF::mul(ratio, ratio, wc).scaled2(-1);
        BF v = unit ? BF::sub(half_t, BF::mul(half_t, half_t, wc), wc) : BF::add(math::ln(L, wc), half_t, wc);
        if (base != log_base::e) v = BF::mul(v, log_scale(base, wc), wc);
        if (rounding_is_safe(v, w, 3, cr.precision) || guard_exhausted(cr.precision, guard)) return v.rounded(cr);
        guard *= 2;
    }
}

inline BF log_modulus(const BC& z, log_base base, const BFC& cr) {
    const BF& a = z.real();
    const BF& b = z.imaginary();
    if (b.is_zero()) return real_log(a.abs(), base, cr);
    if (a.is_zero()) return real_log(b.abs(), base, cr);
    const BF N = z.magnitude_squared_exact();
    if (N.is_finite()) return real_log(N, base, cr).scaled2(-1);
    return log_modulus_spread(a, b, base, cr);
}

inline BC ln_impl(const BC& z, std::int64_t branch, log_base base, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    const BFC cr = ctx.real(), ci = ctx.imaginary();
    if (z.is_zero())     return BC(BF::infinity(true), scaled_argument(direction(z), branch, base, ci));
    if (z.is_infinite()) return BC(BF::infinity(),     scaled_argument(direction(z), branch, base, ci));
    return BC(log_modulus(z, base, cr), scaled_argument(z, branch, base, ci));
}

inline std::size_t exp_extra(const BF& a) noexcept { return positive_part(top_of(a) - 28); }

inline bool exp_at(const BF& a, const BF& b, const BFC& wc, work& r) {
    const std::size_t loss = 3 + exp_extra(a);

    if (b.is_zero()) {
        r.re       = math::exp(a, wc);
        r.im       = BF::zero(b.signbit());
        r.exact_re = a.is_zero() || r.re.is_zero();
        r.exact_im = true;
        r.loss_re  = loss;
        return !r.re.is_nan() && !r.re.is_undefined();
    }

    const BF s = math::sin(b, wc), c = math::cos(b, wc);
    if (!s.is_finite() || !c.is_finite()) return false;
    const BF E = math::exp(a, wc);
    if (E.is_nan() || E.is_undefined()) return false;

    if (E.is_infinite()) {
        r.re = BF::infinity(c.signbit());
        r.im = BF::infinity(s.signbit());
        return true;
    }

    r.re       = BF::mul(E, c, wc);
    r.im       = BF::mul(E, s, wc);
    r.exact_re = r.re.is_zero();                 
    r.exact_im = r.im.is_zero();
    r.loss_re  = r.loss_im = loss;
    return true;
}

inline BC exp_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    const BF& a = z.real();
    const BF& b = z.imaginary();
    if (b.is_infinite()) return BC::undefined();

    if (a.is_infinite()) {
        if (b.is_zero()) return BC(math::exp(a, cr), b);
        bool sn = false, cn = false;
        if (!sincos_signs(b, sn, cn)) return BC::undefined();
        if (a.signbit()) return BC(BF::zero(cn), BF::zero(sn));
        return BC(BF::infinity(cn), BF::infinity(sn));
    }

    if (a.is_zero() && b.is_zero()) return BC(BF::one(), b);
    auto f = [&](std::size_t w, work& r) { return exp_at(a, b, work_ctx(w), r); };
    return ziv(f, no_probe{}, cr, ci, 32);
}

inline BC sqrt_eval(const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    const BF&  a  = z.real();
    const BF&  b  = z.imaginary();
    const bool bn = b.signbit();

    if (b.is_infinite()) return BC(BF::infinity(), b);
    if (a.is_infinite()) return a.signbit() ? BC(BF::zero(), BF::infinity(bn)) : BC(a, BF::zero(bn));
    if (z.is_zero())     return BC(BF::zero(), b);

    if (b.is_zero()) {                                           // on the real axis
        if (!a.signbit()) return BC(math::sqrt(a, cr), b);
        const BF t = bn ? -math::sqrt(a.abs(), mirrored(ci)) : math::sqrt(a.abs(), ci);
        return BC(BF::zero(), t);
    }

    const BF aa = a.abs(), ab = b.abs();
    const bool a_neg = a.signbit();

    auto f = [&](std::size_t w, work& r) -> bool {
        const BFC wc = work_ctx(w);
        const BF  m  = z.magnitude(wc);
        const BF  t  = math::sqrt(BF::add(aa, m, wc).scaled2(-1), wc);
        const BF  u  = BF::div(ab, t, wc).scaled2(-1);
        if (!a_neg) { r.re = t; r.im = u.with_sign(bn); }
        else        { r.re = u; r.im = t.with_sign(bn); }
        r.loss_re = r.loss_im = 3;
        return true;
    };

    const root_probe probe(z, 1, 2);
    return ziv(f, probe, cr, ci, 32);
}

template <typename Mul, typename Probe>
inline BC pow_core(const BC& z, std::int64_t branch, Mul&& mul_log, Probe&& probe, const BFC& cr, const BFC& ci) {
    std::size_t ext0 = 0;

    {
        const BFC pc = work_ctx(64);
        const BC  L  = ln_impl(z, branch, log_base::e, BCC(pc));
        BF tr, ti;
        if (!L.is_finite() || !mul_log(L, pc, tr, ti) || !tr.is_finite() || !ti.is_finite()) return BC::undefined();
        if (!ti.is_zero() && top_of(ti) > 65536) return BC::undefined();   

        if (!tr.is_zero() && top_of(tr) >= 50) {                           
            bool sn = false, cn = false;
            if (!sincos_signs(ti, sn, cn)) return BC::undefined();
            return tr.signbit() ? BC(BF::zero(cn), BF::zero(sn)) : BC(BF::infinity(cn), BF::infinity(sn));
        }

        ext0 = positive_part(max_i64(top_of(tr), top_of(ti)) + 1);
    }

    auto f = [&](std::size_t w, work& r) -> bool {
        const BFC wc = work_ctx(w);
        const BC  L  = ln_impl(z, branch, log_base::e, BCC(wc));
        BF tr, ti;
        if (!L.is_finite() || !mul_log(L, wc, tr, ti) || !tr.is_finite() || !ti.is_finite()) return false;
        if (!exp_at(tr, ti, wc, r)) return false;
        if (!r.re.is_finite() || !r.im.is_finite()) return true;
        const bool flushed = r.re.is_zero() && r.im.is_zero();
        modulus_loss(r, positive_part(max_i64(top_of(tr), top_of(ti)) + 1) + 5);
        r.exact_re = flushed || (tr.is_zero() && ti.is_zero());
        r.exact_im = flushed || ti.is_zero();
        return true;
    };

    return ziv(f, probe, cr, ci, 48 + ext0);
}

inline BC infinite_power(const BC& z, const BF& y, std::int64_t branch) {
    if (y.signbit()) return BC::zero();
    const BFC lc    = work_ctx(64);
    const BF  theta = direction(z).argument(branch, lc);
    const BF  phi   = BF::mul(y, theta, lc);
    if (!phi.is_finite()) return BC::undefined();
    if (phi.is_zero()) return BC(BF::infinity(), BF::zero(phi.signbit()));
    const BF c = math::cos(phi, lc), s = math::sin(phi, lc);
    if (!c.is_finite() || !s.is_finite()) return BC::undefined();
    auto limit = [](const BF& v) { return (v.is_zero() || top_of(v) < -32) ? BF::zero(v.signbit()) : BF::infinity(v.signbit()); };
    return BC(limit(c), limit(s));
}

inline BC pow_int_eval(const BC& z, const BigInt& n, const BFC& cr, const BFC& ci) {
    if (z.is_undefined() || n.is_nan() || n.is_undefined()) return BC::undefined();
    const bool nzero = n.magnitude().is_zero();

    if (z.is_zero()) {
        if (nzero) return BC::undefined();
        return n.is_negative() ? BC(BF::infinity(), BF::zero()) : BC::zero();
    }

    if (nzero) return BC(BF::one(), BF::zero());
    std::int64_t ni = 0;

    if (!detail::pw_to_i64(n, ni)) {                            
        if (z.is_infinite()) return n.is_negative() ? BC::zero() : BC::undefined();
        const BF y(n);
        auto mul = [&y](const BC& L, const BFC& wc, BF& tr, BF& ti) {
            tr = BF::mul(L.real(), y, wc);
            ti = BF::mul(L.imaginary(), y, wc);
            return true;
        };
        return pow_core(z, 0, mul, no_probe{}, cr, ci);
    }

    const std::uint64_t an  = detail::ln_abs_u64(ni);
    const bool          neg = ni < 0;

    if (z.is_infinite()) {                                       
        const BC d = direction(z);
        BF pr, pi;
        if (!exact_cpow(d.real(), d.imaginary(), an, BigUInt::max_bits / 4, pr, pi)) return BC::undefined();
        if (neg) return BC(bcd::to_zero(pr), bcd::to_zero(-pi));
        return BC(bcd::to_infinity(pr), bcd::to_infinity(pi));
    }

    const BF& a = z.real();
    const BF& b = z.imaginary();

    {   
        const std::size_t p    = (cr.precision > ci.precision) ? cr.precision : ci.precision;
        const std::size_t soft = p + 1024;
        const std::size_t cap  = (soft < BigUInt::max_bits / 2) ? soft : BigUInt::max_bits / 2;
        const std::size_t W    = width_of(a, b);

        if (an <= cap / W) {
            BF pr, pi;
            if (exact_cpow(a, b, an, 2 * cap + 64, pr, pi)) {
                if (!neg) return BC(pr.rounded(cr), pi.rounded(ci));
                return BC(pr, pi).reciprocal(BCC(cr, ci));
            }
        }
    }

    const std::size_t nb = bcd::bits_u64(an);

    auto f = [&](std::size_t w, work& r) -> bool {
        const BCC     wcc(work_ctx(w));
        BC            acc(BF::one(), BF::zero());
        BC            t = z;
        std::uint64_t k = an;

        while (k != 0) {
            if (k & 1u) acc = BC::mul(acc, t, wcc);
            k >>= 1;
            if (k != 0) t = t.square(wcc);
        }

        if (neg) acc = acc.reciprocal(wcc);
        if (acc.is_undefined()) return false;
        r.re = acc.real();
        r.im = acc.imaginary();
        if (acc.is_finite()) modulus_loss(r, nb + 4);
        return true;
    };

    return ziv(f, no_probe{}, cr, ci, 32 + nb);
}

inline BC pow_real_eval(const BC& z, const BF& y, std::int64_t branch, const BFC& cr, const BFC& ci) {
    if (z.is_undefined() || y.is_nan() || y.is_undefined()) return BC::undefined();
    if (y.is_integer()) return pow_int_eval(z, y.get_integer_part(), cr, ci);  

    if (y.is_infinite()) {
        const BF::ordering o = BC::compare_magnitude(z, BF::one());
        if (o == BF::ordering::unordered) return BC::undefined();
        const bool shrink = y.signbit() ? (o == BF::ordering::greater) : (o == BF::ordering::less);
        return shrink ? BC::zero() : BC::undefined();
    }

    if (z.is_zero())     return y.signbit() ? BC(BF::infinity(), BF::zero()) : BC::zero();
    if (z.is_infinite()) return infinite_power(z, y, branch);
    const BF& a = z.real();
    const BF& b = z.imaginary();

    if (branch == 0 && b.is_zero() && !a.signbit()) {              
        return BC(math::pow(a, y, cr), BF::zero(b.signbit() != y.signbit()));
    }

    if (y == BF(BigUInt::one(), false, -1)) {                      
        if (branch & 1) return -sqrt_eval(z, mirrored(cr), mirrored(ci));
        return sqrt_eval(z, cr, ci);
    }

    auto mul = [&y](const BC& L, const BFC& wc, BF& tr, BF& ti) {
        tr = BF::mul(L.real(), y, wc);
        ti = BF::mul(L.imaginary(), y, wc);
        return true;
    };

    std::int64_t  m = 0;
    std::uint64_t q = 0;

    if (y.exponent() < 0 && y.exponent() >= -12 && y.significand_bits() <= 12) {
        const std::int64_t mm = static_cast<std::int64_t>(y.significand().get_lowest_bits());
        m = y.signbit() ? -mm : mm;
        q = std::uint64_t(1) << static_cast<unsigned>(-y.exponent());
    }

    const root_probe probe(z, m, q);
    return pow_core(z, branch, mul, probe, cr, ci);
}

inline BC nth_root_eval(const BC& z, const BigInt& n, std::int64_t branch, const BFC& cr, const BFC& ci) {
    if (z.is_undefined() || n.is_nan() || n.is_undefined()) return BC::undefined();
    if (n.magnitude().is_zero()) return BC::undefined();
    const bool nneg = n.is_negative();
    if (z.is_zero()) return nneg ? BC(BF::infinity(), BF::zero()) : BC::zero();
    std::int64_t ni    = 0;
    const bool   small = detail::pw_to_i64(n, ni);
    if (small && ni == 1)  return z.rounded(BCC(cr, ci));
    if (small && ni == -1) return z.reciprocal(BCC(cr, ci));
    if (z.is_infinite())   return infinite_power(z, BF::div(BF::one(), BF(n), work_ctx(64)), branch);

    if (small && ni == 2) {
        if (branch & 1) return -sqrt_eval(z, mirrored(cr), mirrored(ci));
        return sqrt_eval(z, cr, ci);
    }

    if (branch == 0 && z.imaginary().is_zero() && !z.real().signbit()) {
        return BC(math::nth_root(z.real(), n, cr), BF::zero(z.imaginary().signbit() != nneg));
    }

    const BF nf(n);

    auto mul = [&nf](const BC& L, const BFC& wc, BF& tr, BF& ti) {
        tr = BF::div(L.real(), nf, wc);
        ti = BF::div(L.imaginary(), nf, wc);
        return true;
    };

    const std::uint64_t an = small ? detail::ln_abs_u64(ni) : 0;
    const root_probe    probe(z, nneg ? -1 : 1, (small && an <= 4096) ? an : 0);
    return pow_core(z, branch, mul, probe, cr, ci);
}

inline BC pow_complex_eval(const BC& z, const BC& w, std::int64_t branch, const BFC& cr, const BFC& ci) {
    if (z.is_undefined() || w.is_undefined()) return BC::undefined();
    if (w.imaginary().is_zero()) return pow_real_eval(z, w.real(), branch, cr, ci);
    if (w.is_infinite() || z.is_infinite()) return BC::undefined();

    if (z.is_zero()) {                                           
        const BF& wr = w.real();
        return (!wr.signbit() && !wr.is_zero()) ? BC::zero() : BC::undefined();
    }

    if (branch == 0 && z.imaginary().is_zero() && detail::is_exactly_one(z.real())) return BC(BF::one(), BF::zero());

    auto mul = [&w](const BC& L, const BFC& wc, BF& tr, BF& ti) {
        const BC T = BC::mul(L, w, BCC(wc));
        tr = T.real();
        ti = T.imaginary();
        return T.is_finite();
    };

    return pow_core(z, branch, mul, no_probe{}, cr, ci);
}

enum class hyp_kind : std::uint8_t { sinh_v, cosh_v, tanh_v };

inline BF near_one(std::size_t w, bool above, bool neg) {
    BigUInt u = constants::bfdetail::unit(w + 8);
    if (above) u.add_small_mutable(1);
    else       u.sub_small_mutable(1);
    return BF(std::move(u), neg, -static_cast<std::int64_t>(w + 8));
}

inline void recip_work(work& r, const BFC& wc) {
    if (r.re.is_zero() && r.im.is_zero()) {
        r.re       = BF::infinity();
        r.im       = BF::zero();
        r.exact_re = r.exact_im = true;
        return;
    }

    const BF    n = BF::add(BF::mul(r.re, r.re, wc), BF::mul(r.im, r.im, wc), wc);
    BF          x = BF::div(r.re, n, wc);
    BF          y = BF::div(r.im, n, wc);
    y.negate_mutable();
    const std::size_t l = ((r.loss_re > r.loss_im) ? r.loss_re : r.loss_im) + 3;
    r.exact_re = r.exact_re && x.is_zero();
    r.exact_im = r.exact_im && y.is_zero();
    r.re       = std::move(x);
    r.im       = std::move(y);
    r.loss_re  = r.loss_im = l;
}

inline void recip_exact(work& r) {
    const BC v = BC(r.re, r.im).reciprocal(BCC(64));
    r.re       = v.real();
    r.im       = v.imaginary();
    r.exact_re = r.exact_im = true;
}

inline bool hyp_at_infinity(hyp_kind k, bool a_neg, const BF& b, work& r) {
    bool sn = false, cn = false;
    if (!sincos_signs(b, sn, cn)) return false;
    const bool bz = b.is_zero();

    switch (k) {
        case hyp_kind::sinh_v:
            r.re = BF::infinity(a_neg != cn);
            r.im = bz ? BF::zero(sn) : BF::infinity(sn);
            break;
        case hyp_kind::cosh_v:
            r.re = BF::infinity(cn);
            r.im = bz ? BF::zero(a_neg != sn) : BF::infinity(a_neg != sn);
            break;
        default:
            r.re = BF::one(a_neg);
            r.im = BF::zero(sn != cn);                            
            break;
    }

    r.exact_re = r.exact_im = true;
    return true;
}

inline bool hyp_at(hyp_kind k, bool recip, const BF& a, const BF& b, std::size_t w, work& r) {
    const BFC wc = work_ctx(w);
    const BF  s  = math::sin(b, wc), c = math::cos(b, wc);
    if (!s.is_finite() || !c.is_finite()) return false;
    const BF sh = math::sinh(a, wc), ch = math::cosh(a, wc);
    if (sh.is_nan() || sh.is_undefined() || ch.is_nan() || ch.is_undefined()) return false;
    const bool huge = !sh.is_finite() || !ch.is_finite() || (k == hyp_kind::tanh_v && top_of(sh) > BF::exp_max / 2 - 16);

    if (huge) {
        if (k == hyp_kind::tanh_v) {                              
            r.re       = near_one(w, recip, a.signbit());
            r.im       = BF::zero((s.signbit() != c.signbit()) != recip);
            r.exact_re = r.exact_im = true;
            return true;
        }

        if (!hyp_at_infinity(k, a.signbit(), b, r)) return false;
        if (recip) recip_exact(r);
        return true;
    }

    std::size_t loss = 3;

    switch (k) {
        case hyp_kind::sinh_v:
            r.re = BF::mul(sh, c, wc);
            r.im = BF::mul(ch, s, wc);
            break;
        case hyp_kind::cosh_v:
            r.re = BF::mul(ch, c, wc);
            r.im = BF::mul(sh, s, wc);
            break;
        default: {
            const BF d = BF::add(BF::mul(sh, sh, wc), BF::mul(c, c, wc), wc);
            r.re = BF::div(BF::mul(sh, ch, wc), d, wc);
            r.im = BF::div(BF::mul(s, c, wc), d, wc);
            loss = 5;
            break;
        }
    }

    r.exact_re = r.re.is_zero();                                 
    r.exact_im = r.im.is_zero();
    r.loss_re  = r.loss_im = loss;
    if (recip) recip_work(r, wc);
    return true;
}

inline BC hyp_eval(hyp_kind k, bool recip, const BF& a, const BF& b, const BFC& cr, const BFC& ci) {
    if (b.is_infinite()) return BC::undefined();

    if (a.is_infinite()) {
        work r;
        if (!hyp_at_infinity(k, a.signbit(), b, r)) return BC::undefined();
        if (recip) recip_exact(r);
        return BC(r.re.rounded(cr), r.im.rounded(ci));
    }

    auto f = [&](std::size_t w, work& r) { return hyp_at(k, recip, a, b, w, r); };
    return ziv(f, no_probe{}, cr, ci, 32);
}

inline BC hyperbolic(hyp_kind k, bool recip, const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    return hyp_eval(k, recip, z.real(), z.imaginary(), cr, ci);
}

inline BC circular(hyp_kind k, bool recip, const BC& z, const BFC& cr, const BFC& ci) {
    if (z.is_undefined()) return BC::undefined();
    const BF  a = -z.imaginary();
    const BF& b = z.real();
    if (k == hyp_kind::cosh_v) return hyp_eval(k, recip, a, b, cr, ci);

    if (!recip) {
        const BC v = hyp_eval(k, false, a, b, mirrored(ci), cr);
        return BC(v.imaginary(), -v.real());
    }

    const BC v = hyp_eval(k, true, a, b, ci, mirrored(cr));
    return BC(-v.imaginary(), v.real());
}

} // namespace cxdetail

#define FIZMO_CX_CURRENT(NAME)                                                                   \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                        \
    inline BigComplex NAME(const C& z) { return NAME(z, BigComplexContext::current()); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex exp(const C& z, const BigComplexContext& ctx) { return cxdetail::exp_eval(z, ctx.real(), ctx.imaginary()); }
FIZMO_CX_CURRENT(exp)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex sqrt(const C& z, const BigComplexContext& ctx) { return cxdetail::sqrt_eval(z, ctx.real(), ctx.imaginary()); }
FIZMO_CX_CURRENT(sqrt)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex cbrt(const C& z, const BigComplexContext& ctx) {
    return cxdetail::nth_root_eval(z, BigInt(3), 0, ctx.real(), ctx.imaginary());
}
FIZMO_CX_CURRENT(cbrt)

#define FIZMO_CX_LOG(NAME, BASE)                                                                                   \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                          \
    inline BigComplex NAME(const C& z, std::int64_t branch, const BigComplexContext& ctx) {                        \
        return cxdetail::ln_impl(z, branch, cxdetail::log_base::BASE, ctx);                                        \
    }                                                                                                              \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                          \
    inline BigComplex NAME(const C& z, const BigComplexContext& ctx) { return NAME(z, std::int64_t(0), ctx); }     \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                          \
    inline BigComplex NAME(const C& z, std::int64_t branch) { return NAME(z, branch, BigComplexContext::current()); } \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                          \
    inline BigComplex NAME(const C& z) { return NAME(z, std::int64_t(0), BigComplexContext::current()); }

FIZMO_CX_LOG(ln,    e)
FIZMO_CX_LOG(log2,  two)
FIZMO_CX_LOG(log10, ten)

#undef FIZMO_CX_LOG

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, const BigInt& n, std::int64_t branch, const BigComplexContext& ctx) {
    return cxdetail::nth_root_eval(z, n, branch, ctx.real(), ctx.imaginary());
}

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, const BigInt& n, const BigComplexContext& ctx) { return nth_root(z, n, std::int64_t(0), ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, const BigInt& n, std::int64_t branch) { return nth_root(z, n, branch, BigComplexContext::current()); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, const BigInt& n) { return nth_root(z, n, std::int64_t(0), BigComplexContext::current()); }

#define FIZMO_CX_INTEGRAL(T) typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0

template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, T n, std::int64_t branch, const BigComplexContext& ctx) { return nth_root(z, BigInt(n), branch, ctx); }
template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, T n, const BigComplexContext& ctx) { return nth_root(z, BigInt(n), std::int64_t(0), ctx); }
template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, T n, std::int64_t branch) { return nth_root(z, BigInt(n), branch, BigComplexContext::current()); }
template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex nth_root(const C& z, T n) { return nth_root(z, BigInt(n), std::int64_t(0), BigComplexContext::current()); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigInt& n, const BigComplexContext& ctx) {
    return cxdetail::pow_int_eval(z, n, ctx.real(), ctx.imaginary());
}
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigInt& n) { return pow(z, n, BigComplexContext::current()); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigUInt& n, const BigComplexContext& ctx) { return pow(z, BigInt(n, false), ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigUInt& n) { return pow(z, BigInt(n, false), BigComplexContext::current()); }
template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, T n, const BigComplexContext& ctx) { return pow(z, BigInt(n), ctx); }
template <typename C, FIZMO_CX_INTEGRAL(T), bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, T n) { return pow(z, BigInt(n), BigComplexContext::current()); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigFloat& y, std::int64_t branch, const BigComplexContext& ctx) {
    return cxdetail::pow_real_eval(z, y, branch, ctx.real(), ctx.imaginary());
}
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigFloat& y, const BigComplexContext& ctx) { return pow(z, y, std::int64_t(0), ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigFloat& y, std::int64_t branch) { return pow(z, y, branch, BigComplexContext::current()); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex pow(const C& z, const BigFloat& y) { return pow(z, y, std::int64_t(0), BigComplexContext::current()); }

template <typename C, typename W, bcdetail_fn::if_complex<C> = 0, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const C& z, const W& w, std::int64_t branch, const BigComplexContext& ctx) {
    return cxdetail::pow_complex_eval(z, w, branch, ctx.real(), ctx.imaginary());
}
template <typename C, typename W, bcdetail_fn::if_complex<C> = 0, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const C& z, const W& w, const BigComplexContext& ctx) { return pow(z, w, std::int64_t(0), ctx); }
template <typename C, typename W, bcdetail_fn::if_complex<C> = 0, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const C& z, const W& w, std::int64_t branch) { return pow(z, w, branch, BigComplexContext::current()); }
template <typename C, typename W, bcdetail_fn::if_complex<C> = 0, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const C& z, const W& w) { return pow(z, w, std::int64_t(0), BigComplexContext::current()); }

template <typename W, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const BigFloat& x, const W& w, std::int64_t branch, const BigComplexContext& ctx) {
    return cxdetail::pow_complex_eval(BigComplex(x), w, branch, ctx.real(), ctx.imaginary());
}
template <typename W, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const BigFloat& x, const W& w, const BigComplexContext& ctx) { return pow(x, w, std::int64_t(0), ctx); }
template <typename W, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const BigFloat& x, const W& w, std::int64_t branch) { return pow(x, w, branch, BigComplexContext::current()); }
template <typename W, bcdetail_fn::if_complex<W> = 0>
inline BigComplex pow(const BigFloat& x, const W& w) { return pow(x, w, std::int64_t(0), BigComplexContext::current()); }

#undef FIZMO_CX_INTEGRAL

#define FIZMO_CX_TRIG(NAME, FAMILY, KIND, RECIP)                                                 \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                        \
    inline BigComplex NAME(const C& z, const BigComplexContext& ctx) {                           \
        return cxdetail::FAMILY(cxdetail::hyp_kind::KIND, RECIP, z, ctx.real(), ctx.imaginary()); \
    }                                                                                            \
    FIZMO_CX_CURRENT(NAME)

FIZMO_CX_TRIG(sin,  circular,   sinh_v, false)
FIZMO_CX_TRIG(cos,  circular,   cosh_v, false)
FIZMO_CX_TRIG(tan,  circular,   tanh_v, false)
FIZMO_CX_TRIG(csc,  circular,   sinh_v, true)
FIZMO_CX_TRIG(sec,  circular,   cosh_v, true)
FIZMO_CX_TRIG(cot,  circular,   tanh_v, true)
FIZMO_CX_TRIG(sinh, hyperbolic, sinh_v, false)
FIZMO_CX_TRIG(cosh, hyperbolic, cosh_v, false)
FIZMO_CX_TRIG(tanh, hyperbolic, tanh_v, false)
FIZMO_CX_TRIG(csch, hyperbolic, sinh_v, true)
FIZMO_CX_TRIG(sech, hyperbolic, cosh_v, true)
FIZMO_CX_TRIG(coth, hyperbolic, tanh_v, true)

#undef FIZMO_CX_TRIG
#undef FIZMO_CX_CURRENT

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_ELEMENTARY_HPP