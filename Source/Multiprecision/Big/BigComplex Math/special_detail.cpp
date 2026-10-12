#include "fizmo_library.hpp"
#include "special_detail.hpp"

#include <algorithm>

namespace fizmo {
namespace math {
namespace cxsf {

BFC wctx(std::size_t w) { return BFC(BFC::clamp_precision(w), RoundingMode::nearest_even); }

double l2(const BF& x) {
    if (x.is_zero()) return ninf;
    if (!x.is_finite()) return pinf;
    BigUInt           sig = x.significand();
    const std::size_t L   = sig.bit_length();
    std::int64_t      e   = x.exponent();
    if (L > 60) {
        sig.shift_right_mutable(L - 60);
        e += static_cast<std::int64_t>(L - 60);
    }
    return std::log2(static_cast<double>(sig.get_lowest_bits())) + static_cast<double>(e);
}

double lsum(double a, double b) {
    if (a == ninf) return b;
    if (b == ninf) return a;
    if (a < b) std::swap(a, b);
    return a + std::log2(1.0 + std::exp2(b - a));
}

double l2(const CX& z) {
    const double a = l2(z.re);
    const double b = l2(z.im);
    if (a == ninf) return b;
    if (b == ninf) return a;
    return 0.5 * lsum(2 * a, 2 * b);
}

CX cx(const BC& z) { return CX(z.real(), z.imaginary()); }
BC bc(const CX& z) { return BC(z.re, z.im); }

CX add(const CX& a, const CX& b, std::size_t w) {
    const BFC c = wctx(w);
    return CX(BF::add(a.re, b.re, c), BF::add(a.im, b.im, c));
}

CX sub(const CX& a, const CX& b, std::size_t w) {
    const BFC c = wctx(w);
    return CX(BF::sub(a.re, b.re, c), BF::sub(a.im, b.im, c));
}

CX add_real(const CX& a, const BF& b, std::size_t w) { return CX(BF::add(a.re, b, wctx(w)), a.im); }

CX mul(const CX& a, const CX& b, std::size_t w) {
    const BFC c = wctx(w);
    if (a.im.is_zero() && b.im.is_zero()) return CX(BF::mul(a.re, b.re, c));
    if (b.im.is_zero()) return CX(BF::mul(a.re, b.re, c), BF::mul(a.im, b.re, c));
    if (a.im.is_zero()) return CX(BF::mul(a.re, b.re, c), BF::mul(a.re, b.im, c));
    const BFC c2 = wctx(w + 8);
    return CX(BF::sub(BF::mul(a.re, b.re, c2), BF::mul(a.im, b.im, c2), c),
              BF::add(BF::mul(a.re, b.im, c2), BF::mul(a.im, b.re, c2), c));
}

CX mul(const CX& a, const BF& b, std::size_t w) {
    const BFC c = wctx(w);
    return CX(BF::mul(a.re, b, c), BF::mul(a.im, b, c));
}

CX sqr(const CX& a, std::size_t w) { return mul(a, a, w); }

CX div(const CX& a, const BF& b, std::size_t w) {
    const BFC c = wctx(w);
    return CX(BF::div(a.re, b, c), BF::div(a.im, b, c));
}

CX div(const CX& a, const CX& b, std::size_t w) {
    if (b.im.is_zero()) return div(a, b.re, w);
    const BFC c2 = wctx(w + 8);
    const BF  d  = BF::add(BF::mul(b.re, b.re, c2), BF::mul(b.im, b.im, c2), c2);
    const CX  n  = mul(a, conj(b), w + 8);
    return div(n, d, w);
}

CX recip(const CX& a, std::size_t w) { return div(CX(BF::one()), a, w); }
CX neg(const CX& a) { return CX(a.re.with_sign(!a.re.signbit()), a.im.with_sign(!a.im.signbit())); }
CX conj(const CX& a) { return CX(a.re, a.im.with_sign(!a.im.signbit())); }
CX mul_i(const CX& a) { return CX(a.im.with_sign(!a.im.signbit()), a.re); }
CX div_i(const CX& a) { return CX(a.im, a.re.with_sign(!a.re.signbit())); }
CX scale2(const CX& a, std::int64_t k) { return CX(a.re.scaled_pow2(k), a.im.scaled_pow2(k)); }

namespace {
BCC bcc(std::size_t w) { return BCC(BFC::clamp_precision(w)); }
} // namespace

CX exp(const CX& a, std::size_t w) {
    if (a.im.is_zero()) return CX(math::exp(a.re, wctx(w)));
    return cx(math::exp(bc(a), bcc(w)));
}

CX ln(const CX& a, std::size_t w) {
    if (a.im.is_zero() && !a.re.signbit() && !a.re.is_zero()) return CX(math::ln(a.re, wctx(w)));
    return cx(math::ln(bc(a), bcc(w)));
}

CX sqrt(const CX& a, std::size_t w) {
    if (a.im.is_zero() && !a.re.signbit()) return CX(math::sqrt(a.re, wctx(w)));
    return cx(math::sqrt(bc(a), bcc(w)));
}

CX sin(const CX& a, std::size_t w) {
    if (a.im.is_zero()) return CX(math::sin(a.re, wctx(w)));
    return cx(math::sin(bc(a), bcc(w)));
}

CX cos(const CX& a, std::size_t w) {
    if (a.im.is_zero()) return CX(math::cos(a.re, wctx(w)));
    return cx(math::cos(bc(a), bcc(w)));
}

CX pow(const CX& a, const CX& b, std::size_t w) { return cx(math::pow(bc(a), bc(b), bcc(w))); }

CX pow_int(const CX& a, std::int64_t n, std::size_t w) { return cx(math::pow(bc(a), BigInt(static_cast<long long>(n)), bcc(w))); }

BF pi(std::size_t w) { return multiprecision::constants::pi(wctx(w)); }

CX real_pow_neg(const BF& x, const CX& s, std::size_t w) {
    const double lx = l2(x);
    const double ll = std::log2(std::fabs(lx) * 0.6931471805599453 + 1.0);
    const std::size_t wq = w + 8 + extra_bits(s) + static_cast<std::size_t>(std::max(0.0, std::ceil(ll)));
    const BF L = math::ln(x, wctx(wq));
    return exp(neg(mul(s, L, wq)), w);
}

void PowTable::grow(std::uint64_t n) {
    const std::size_t old = t_.size();
    std::size_t m = std::max<std::size_t>({64, 2 * old, static_cast<std::size_t>(n) + 1});
    spf_.assign(m, 0);
    for (std::size_t i = 2; i < m; ++i) {
        if (spf_[i] != 0) continue;
        for (std::size_t j = i; j < m; j += i) {
            if (spf_[j] == 0) spf_[j] = static_cast<std::uint32_t>(i);
        }
    }
    t_.resize(m);
    for (std::size_t i = std::max<std::size_t>(old, 1); i < m; ++i) {
        if (i == 1) {
            t_[1] = CX(BF::one());
        } else if (spf_[i] == i) {
            t_[i] = real_pow_neg(BF(static_cast<std::uint64_t>(i)), s_, w_);
        } else {
            t_[i] = mul(t_[spf_[i]], t_[i / spf_[i]], w_);
        }
    }
}

const CX& PowTable::get(std::uint64_t n) {
    if (n >= t_.size()) grow(n);
    return t_[n];
}

bool settle(const BF& x, double err, const BFC& c, bool last, BF& out) {
    if (err == ninf) {
        out = x.rounded(c);
        return true;
    }
    if (x.is_zero() || l2(x) <= err + 1.0) {
        out = BF::zero();
        return last;
    }
    const std::int64_t e     = static_cast<std::int64_t>(std::ceil(err));
    const BF           delta = BF(BigUInt::one(), false, e);
    const BFC          xc(BFC::max_prec, RoundingMode::nearest_even);
    const BF           lo = BF::sub(x, delta, xc).rounded(c);
    const BF           hi = BF::add(x, delta, xc).rounded(c);
    if (BF::compare(lo, hi) == BF::ordering::equal && lo.signbit() == hi.signbit()) {
        out = lo;
        return true;
    }
    out = x.rounded(c);
    return last;
}


namespace {

std::size_t bits_of(const BF& x) { return x.is_zero() ? 0 : x.significand().bit_length(); }

BF round_to(const BF& x, std::size_t w, double& err) {
    if (x.is_zero() || bits_of(x) <= w) return x;
    const BF r = x.rounded(wctx(w));
    err = lsum(err, static_cast<double>(x.get_exp_base2()) - static_cast<double>(w));
    return r;
}

BF xmul(const BF& a, const BF& b, std::size_t w, double& err) {
    if (a.is_zero() || b.is_zero()) return BF::zero();
    const std::size_t need = bits_of(a) + bits_of(b);
    if (need <= 4 * w + 64) return round_to(BF::mul(a, b, wctx(need)), w, err);
    const BF r = BF::mul(a, b, wctx(w));
    err = lsum(err, static_cast<double>(r.get_exp_base2()) - static_cast<double>(w));
    return r;
}

BF xadd(const BF& a, const BF& b, std::size_t w, double& err) {
    if (a.is_zero()) return round_to(b, w, err);
    if (b.is_zero()) return round_to(a, w, err);
    const std::int64_t hi = std::max(a.get_exp_base2(), b.get_exp_base2());
    const std::int64_t lo = std::min(a.exponent(), b.exponent());
    const std::int64_t span = hi - lo + 3;
    if (span <= static_cast<std::int64_t>(4 * w + 64)) return round_to(BF::add(a, b, wctx(static_cast<std::size_t>(span))), w, err);
    const BF r = BF::add(a, b, wctx(w));
    if (!r.is_zero()) err = lsum(err, static_cast<double>(r.get_exp_base2()) - static_cast<double>(w));
    else              err = lsum(err, static_cast<double>(hi) - static_cast<double>(w));
    return r;
}

BF negate(const BF& x) { return x.with_sign(!x.signbit()); }

double prod_err(double la, double ea, double lb, double eb) { return lsum(lsum(la + eb, lb + ea), ea + eb); }

} // namespace

CV cv_from(const CX& v, double err) { return CV{v, err, err}; }

CV cv_add(const CV& a, const CV& b, std::size_t w) {
    CV r;
    r.er = lsum(a.er, b.er);
    r.ei = lsum(a.ei, b.ei);
    r.v.re = xadd(a.v.re, b.v.re, w, r.er);
    r.v.im = xadd(a.v.im, b.v.im, w, r.ei);
    return r;
}

CV cv_sub(const CV& a, const CV& b, std::size_t w) {
    CV nb{CX(negate(b.v.re), negate(b.v.im)), b.er, b.ei};
    return cv_add(a, nb, w);
}

CV cv_mul(const CV& a, const CV& b, std::size_t w) {
    const double lar = l2(a.v.re), lai = l2(a.v.im), lbr = l2(b.v.re), lbi = l2(b.v.im);
    CV r;
    double e1 = prod_err(lar, a.er, lbr, b.er);
    double e2 = prod_err(lai, a.ei, lbi, b.ei);
    double e3 = prod_err(lar, a.er, lbi, b.ei);
    double e4 = prod_err(lai, a.ei, lbr, b.er);
    const std::size_t w2 = w + 8;
    const BF p1 = xmul(a.v.re, b.v.re, w2, e1);
    const BF p2 = xmul(a.v.im, b.v.im, w2, e2);
    const BF p3 = xmul(a.v.re, b.v.im, w2, e3);
    const BF p4 = xmul(a.v.im, b.v.re, w2, e4);
    r.er = lsum(e1, e2);
    r.ei = lsum(e3, e4);
    r.v.re = xadd(p1, negate(p2), w, r.er);
    r.v.im = xadd(p3, p4, w, r.ei);
    return r;
}

CV cv_scale(const CV& a, const BF& s, std::size_t w) {
    const double ls = l2(s);
    CV r;
    r.er = a.er == ninf ? ninf : a.er + ls;
    r.ei = a.ei == ninf ? ninf : a.ei + ls;
    r.v.re = xmul(a.v.re, s, w, r.er);
    r.v.im = xmul(a.v.im, s, w, r.ei);
    return r;
}

CV cv_div_int(const CV& a, std::uint64_t d, std::size_t w) {
    const BF     D(d);
    const double ld = std::log2(static_cast<double>(d));
    CV r;
    r.er = a.er == ninf ? ninf : a.er - ld;
    r.ei = a.ei == ninf ? ninf : a.ei - ld;
    auto q = [&](const BF& x, double& err) {
        if (x.is_zero()) return BF::zero();
        const BF qq = BF::div(x, D, wctx(w));
        if (BF::compare(BF::mul(qq, D, wctx(bits_of(qq) + 64)), x) != BF::ordering::equal)
            err = lsum(err, static_cast<double>(qq.get_exp_base2()) - static_cast<double>(w));
        return qq;
    };
    r.v.re = q(a.v.re, r.er);
    r.v.im = q(a.v.im, r.ei);
    return r;
}

std::size_t extra_bits(const BF& x) {
    const double l = l2(x);
    if (l <= 0) return 4;
    return static_cast<std::size_t>(std::ceil(l)) + 4;
}

std::size_t extra_bits(const CX& z) {
    const double l = l2(z);
    if (l <= 0) return 4;
    return static_cast<std::size_t>(std::ceil(l)) + 4;
}

void sincospi(const BF& x, std::size_t w, BF& s, BF& c) {
    if (x.is_zero()) {
        s = BF::zero();
        c = BF::one();
        return;
    }
    const BF n2 = math::round(x.scaled_pow2(1));
    const BF n  = n2.scaled_pow2(-1);
    const std::int64_t hi   = std::max(x.get_exp_base2(), n.is_zero() ? x.get_exp_base2() : n.get_exp_base2());
    const std::int64_t lo   = std::min(x.exponent(), n.is_zero() ? x.exponent() : n.exponent());
    const BF t = BF::sub(x, n, wctx(static_cast<std::size_t>(std::max<std::int64_t>(hi - lo + 4, 8))));
    unsigned q = 0;
    if (!n2.is_zero()) {
        const BigInt k = n2.get_integer_part();
        q = static_cast<unsigned>(k.get_lowest_bits() & 3u);
        if (k.is_negative()) q = (4u - q) & 3u;
    }
    BF st, ct;
    if (t.is_zero()) {
        st = BF::zero();
        ct = BF::one();
    } else {
        const BFC c2 = wctx(w + 8);
        const BF  a  = BF::mul(t, multiprecision::constants::pi(c2), c2);
        st = math::sin(a, wctx(w + 4));
        ct = math::cos(a, wctx(w + 4));
    }
    auto ng = [](const BF& v) { return v.is_zero() ? v : v.with_sign(!v.signbit()); };
    switch (q) {
        case 0: s = st;     c = ct;     break;
        case 1: s = ct;     c = ng(st); break;
        case 2: s = ng(st); c = ng(ct); break;
        default: s = ng(ct); c = st;    break;
    }
}

namespace {

void hyp_pi(const BF& y, std::size_t w, BF& sh, BF& ch) {
    if (y.is_zero()) {
        sh = BF::zero();
        ch = BF::one();
        return;
    }
    const std::size_t wy = w + extra_bits(y) + 4;
    const BFC c2 = wctx(wy);
    const BF  a  = BF::mul(y, multiprecision::constants::pi(c2), c2);
    sh = math::sinh(a, wctx(w + 4));
    ch = math::cosh(a, wctx(w + 4));
}

} // namespace

CV sinpi(const CX& z, std::size_t w) {
    BF s, c, sh, ch;
    sincospi(z.re, w + 4, s, c);
    hyp_pi(z.im, w + 4, sh, ch);
    const BFC wc = wctx(w + 4);
    CV r;
    r.v.re = BF::mul(s, ch, wc);
    r.v.im = BF::mul(c, sh, wc);
    r.er = r.v.re.is_zero() ? ninf : l2(r.v.re) - static_cast<double>(w);
    r.ei = r.v.im.is_zero() ? ninf : l2(r.v.im) - static_cast<double>(w);
    return r;
}

CV cospi(const CX& z, std::size_t w) {
    BF s, c, sh, ch;
    sincospi(z.re, w + 4, s, c);
    hyp_pi(z.im, w + 4, sh, ch);
    const BFC wc = wctx(w + 4);
    CV r;
    r.v.re = BF::mul(c, ch, wc);
    const BF im = BF::mul(s, sh, wc);
    r.v.im = im.is_zero() ? im : im.with_sign(!im.signbit());
    r.er = r.v.re.is_zero() ? ninf : l2(r.v.re) - static_cast<double>(w);
    r.ei = r.v.im.is_zero() ? ninf : l2(r.v.im) - static_cast<double>(w);
    return r;
}

BC finish_undefined() { return BC::undefined(); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
