#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double log2e = 1.4426950408889634;
constexpr double pi_d  = 3.141592653589793;

double sq_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(2.0 * l, 900.0));
}

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

BF two_over_sqrt_pi(std::size_t w) {
    const BFC c = wctx(w + 8);
    return BF::div(BF(static_cast<std::int64_t>(2)), math::sqrt(multiprecision::constants::pi(c), c), wctx(w));
}

CX exp_neg_sq(const CX& z, std::size_t w) {
    const std::size_t wz = w + bits_of(2.0 * l2(z)) + 8;
    return exp(neg(sqr(z, wz)), w);
}

Approx erf_series(const CX& z, std::size_t w) {
    const double xx     = sq_d(z.re);
    const double yy     = sq_d(z.im);
    const bool   kummer = xx >= yy;
    const std::size_t wb = w + bits_of(2.0 * (kummer ? yy : xx) * log2e) + 16;
    const CX z2  = sqr(z, wb + 8);
    const double lz2 = l2(z2);
    Term T;
    CX   sum;
    double tail = ninf;
    std::uint64_t K = 0;
    if (!kummer) {
        const CX mz2 = neg(z2);
        CX t = z;
        for (std::uint64_t k = 0;; ++k) {
            const CX term = div(t, BF(2 * k + 1), wb);
            sum = add(sum, term, wb);
            T.add(l2(term));
            t = div(mul(t, mz2, wb), BF(k + 1), wb);
            if (lz2 - std::log2(static_cast<double>(k + 1)) < -1.0) {
                const double bound = l2(t) + 1.0;
                if (bound < T.l2max - static_cast<double>(wb) || bound == ninf) {
                    tail = bound;
                    K    = k;
                    break;
                }
            }
        }
        const BF     c = two_over_sqrt_pi(wb);
        const CX     v = mul(sum, c, wb);
        const double e = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail) + 1.0;
        return approx(v, lsum(e, l2(v) - static_cast<double>(wb) + 3.0));
    }
    const CX tz2 = scale2(z2, 1);
    CX u(BF::one());
    sum = u;
    T.add(0.0);
    for (std::uint64_t k = 1;; ++k) {
        u = div(mul(u, tz2, wb), BF(2 * k + 1), wb);
        const double lr = lz2 + 1.0 - std::log2(static_cast<double>(2 * k + 3));
        if (lr < -1.0) {
            const double bound = l2(u) + 1.0;
            if (bound < T.l2max - static_cast<double>(wb) || bound == ninf) {
                tail = bound;
                K    = k;
                break;
            }
        }
        sum = add(sum, u, wb);
        T.add(l2(u));
    }
    const CX     P  = mul(mul(z, exp_neg_sq(z, wb), wb), two_over_sqrt_pi(wb), wb);
    const CX     v  = mul(sum, P, wb);
    const double es = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail) + 1.0;
    return approx(v, lsum(l2(P) + es, l2(v) - static_cast<double>(wb) + 5.0));
}

double phase(const CX& z) {
    const double a = l2(z.re), b = l2(z.im);
    const double m = std::max(a, b);
    if (m == ninf) return 0.0;
    const double x = a == ninf ? 0.0 : std::exp2(std::max(a - m, -1000.0)) * (z.re.signbit() ? -1.0 : 1.0);
    const double y = b == ninf ? 0.0 : std::exp2(std::max(b - m, -1000.0)) * (z.im.signbit() ? -1.0 : 1.0);
    return std::atan2(y, x);
}

bool asym_ok(const CX& z, std::size_t w) {
    if (z.re.signbit() || z.re.is_zero()) return false;
    if (std::fabs(phase(z)) > 3.0 * pi_d / 8.0) return false;
    return 2.0 * l2(z) + std::log2(log2e) > std::log2(static_cast<double>(w) + 24.0);
}

bool erfc_asym(const CX& z, std::size_t w, Approx& out) {
    const std::size_t wb  = w + 16;
    const double      phi = std::fabs(phase(z));
    const double      cf  = phi <= pi_d / 4.0 ? 0.0 : -std::log2(std::sin(2.0 * phi));
    const std::size_t wz  = wb + bits_of(2.0 * l2(z)) + 8;
    const CX z2 = sqr(z, wz);
    const CX r  = recip(scale2(z2, 1), wb);
    const double lr = l2(r);
    CX   a(BF::one());
    CX   sum = a;
    Term T;
    T.add(0.0);
    double tail = ninf;
    std::uint64_t M = 0;
    for (std::uint64_t m = 1;; ++m) {
        if (std::log2(static_cast<double>(2 * m - 1)) + lr >= -0.5) return false;
        a = neg(mul(mul(a, r, wb), BF(2 * m - 1), wb));
        const double la = l2(a);
        if (la + cf < T.l2max - static_cast<double>(wb) - 4.0) {
            tail = la + cf + 1.0;
            M    = m;
            break;
        }
        sum = add(sum, a, wb);
        T.add(la);
    }
    const BFC pc = wctx(wb + 8);
    const BF  sp = math::sqrt(multiprecision::constants::pi(pc), pc);
    const CX  P  = div(exp(neg(z2), wb), mul(z, sp, wb), wb);
    const CX  v  = mul(sum, P, wb);
    const double es = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(M) + 8.0) - static_cast<double>(wb), tail);
    out = approx(v, lsum(l2(P) + es, l2(v) - static_cast<double>(wb) + 6.0));
    return true;
}

Approx one_minus(const Approx& a, std::size_t w, bool plus = false) {
    const BFC c = wctx(w + 8);
    Approx r = a;
    r.v.re = plus ? BF::add(BF::one(), a.v.re, c) : BF::sub(BF::one(), a.v.re, c);
    r.v.im = plus ? a.v.im : a.v.im.with_sign(!a.v.im.signbit());
    r.er   = lsum(a.er, l2(r.v.re) - static_cast<double>(w + 8));
    return r;
}

Approx negate(Approx a) {
    a.v = neg(a.v);
    return a;
}

Approx erf_raw(const CX& z, std::size_t w) {
    const bool flip = z.re.signbit();
    const CX   Z    = flip ? neg(z) : z;
    Approx     a;
    if (asym_ok(Z, w) && erfc_asym(Z, w, a)) {
        a = one_minus(a, w + 16);
    } else {
        a = erf_series(Z, w);
    }
    return flip ? negate(a) : a;
}

Approx erfc_raw(const CX& z, std::size_t w) {
    const bool flip = z.re.signbit();
    const CX   Z    = flip ? neg(z) : z;
    Approx     a;
    if (asym_ok(Z, w) && erfc_asym(Z, w, a)) {
        if (!flip) return a;
        const BFC c = wctx(w + 24);
        a.v.re = BF::sub(BF(static_cast<std::int64_t>(2)), a.v.re, c);
        a.v.im = a.v.im.with_sign(!a.v.im.signbit());
        a.er   = lsum(a.er, l2(a.v.re) - static_cast<double>(w + 24));
        return a;
    }
    const double rz2 = sq_d(Z.re) - sq_d(Z.im);
    const std::size_t extra = flip ? 0 : bits_of(rz2 * log2e);
    a = erf_series(Z, w + extra);
    return one_minus(a, w + extra + 16, flip);
}

bool real_axis(const BC& z) { return z.imaginary().is_zero(); }
bool imag_axis(const BC& z) { return z.real().is_zero() && !z.imaginary().is_zero(); }

Approx swap_i(Approx a, bool times_i) {
    a.v = times_i ? mul_i(a.v) : div_i(a.v);
    std::swap(a.er, a.ei);
    return a;
}

} // namespace

BC erf(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return BC(math::erf(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), math::erfi(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return erf_raw(Z, w); });
}

BC erfc(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return BC(math::erfc(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::one(), -math::erfi(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return erfc_raw(Z, w); });
}

BC erfi(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return BC(math::erfi(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), math::erf(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = mul_i(cx(z));
    return drive(ctx, [&](std::size_t w) { return swap_i(erf_raw(Z, w), false); });
}

BC generalized_erf(const BC& z, const BC& b, const BCC& ctx) {
    if (z.is_undefined() || b.is_undefined()) return BC::undefined();
    if (z.is_indeterminate() || b.is_indeterminate()) return BC::indeterminate();
    if (z == b) return BC::zero();
    if (real_axis(z) && real_axis(b)) return BC(math::generalized_erf(z.real(), b.real(), ctx.real()), BF::zero());
    if (!z.is_finite() || !b.is_finite()) return BC::undefined();
    const CX Z = cx(z), B = cx(b);
    const bool right = !Z.re.signbit() && !Z.re.is_zero() && !B.re.signbit() && !B.re.is_zero();
    const bool left  = Z.re.signbit() && !Z.re.is_zero() && B.re.signbit() && !B.re.is_zero();
    return drive(ctx, [&](std::size_t w) {
        Approx p, q;
        if (right) {
            p = erfc_raw(B, w);
            q = erfc_raw(Z, w);
        } else if (left) {
            p = erfc_raw(neg(Z), w);
            q = erfc_raw(neg(B), w);
        } else {
            p = erf_raw(Z, w);
            q = erf_raw(B, w);
        }
        Approx r;
        r.v  = sub(p.v, q.v, w + 8);
        r.er = lsum(lsum(p.er, q.er), l2(r.v.re) - static_cast<double>(w + 8));
        r.ei = lsum(lsum(p.ei, q.ei), l2(r.v.im) - static_cast<double>(w + 8));
        return r;
    });
}

BC generalized_erfc(const BC& z, const BC& b, const BCC& ctx) { return -generalized_erf(z, b, ctx); }

namespace {

struct Target {
    CX     y, c, d;
    double ey = ninf, ec = ninf, ed = ninf;
};

double rel(const BF& v, std::size_t p) { return v.is_zero() ? ninf : l2(v) - static_cast<double>(p); }

Target target_from_y(const CX& y, std::size_t p) {
    Target t;
    const BFC pc = wctx(p);
    t.y  = y;
    t.c  = CX(BF::sub(BF::one(), y.re, pc), y.im.with_sign(!y.im.signbit()));
    t.d  = CX(BF::add(BF::one(), y.re, pc), y.im);
    t.ec = rel(t.c.re, p);
    t.ed = rel(t.d.re, p);
    return t;
}

Target target_from_c(const CX& c, std::size_t p) {
    Target t;
    const BFC pc = wctx(p);
    t.c  = c;
    t.y  = CX(BF::sub(BF::one(), c.re, pc), c.im.with_sign(!c.im.signbit()));
    t.d  = CX(BF::sub(BF(static_cast<std::int64_t>(2)), c.re, pc), c.im.with_sign(!c.im.signbit()));
    t.ey = rel(t.y.re, p);
    t.ed = rel(t.d.re, p);
    return t;
}

void residual(const CX& z, const Target& t, std::size_t w, CX& F, double& eF) {
    Approx a;
    if (l2(z) < 0.0) {
        a  = erf_raw(z, w);
        F  = sub(a.v, t.y, w + 8);
        eF = lsum(lsum(a.er, a.ei), t.ey);
    } else if (!z.re.signbit()) {
        a  = erfc_raw(z, w);
        F  = sub(t.c, a.v, w + 8);
        eF = lsum(lsum(a.er, a.ei), t.ec);
    } else {
        a  = erfc_raw(neg(z), w);
        F  = sub(a.v, t.d, w + 8);
        eF = lsum(lsum(a.er, a.ei), t.ed);
    }
    eF = lsum(eF, l2(F) - static_cast<double>(w + 6));
}

CX newton_step(const CX& z, const CX& F, std::size_t w) {
    const BFC pc = wctx(w + 8);
    const BF  h  = BF::div(math::sqrt(multiprecision::constants::pi(pc), pc), BF(static_cast<std::int64_t>(2)), pc);
    const std::size_t wz = w + bits_of(2.0 * l2(z)) + 8;
    const CX E = exp(sqr(z, wz), w + 8);
    return mul(mul(F, E, w + 8), h, w);
}

bool seed_small(const CX& c, CX& z) {
    const std::size_t p  = 96;
    const BFC         pc = wctx(p);
    const BF          sp = math::sqrt(multiprecision::constants::pi(pc), pc);
    const CX          lc = ln(c, p);
    z = sqrt(neg(lc), p);
    for (int i = 0; i < 8; ++i) {
        const CX arg = mul(mul(c, z, p), sp, p);
        z = sqrt(neg(ln(arg, p)), p);
        if (!finite(z)) return false;
    }
    return true;
}

bool polish(const Target& t, CX& z, std::size_t w, double* err) {
    double prev = pinf;
    for (int it = 0; it < 200; ++it) {
        CX     F;
        double eF;
        residual(z, t, w, F, eF);
        if (!finite(F)) return false;
        const CX     d  = newton_step(z, F, w);
        const double ld = l2(d);
        z = sub(z, d, w);
        const double lz = l2(z);
        if (ld == ninf || ld < lz - static_cast<double>(w) + 2.0) {
            if (err) {
                const CX     g  = newton_step(z, CX(BF::one()), w);
                const double ln = l2(g) + eF;
                *err = lsum(lsum(ld + 1.0, ln + 1.0), lz - static_cast<double>(w) + 2.0) + 1.0;
            }
            return true;
        }
        if ((it > 6 && ld > prev - 0.25) || ld > std::max(lz, 0.0) + 2.0) return false;
        prev = ld;
    }
    return false;
}

bool homotopy(const CX& yfinal, CX& z) {
    const std::size_t p = 80;
    z      = CX();
    double t = 0.0, h = 1.0;
    int    steps = 0;
    while (t < 1.0) {
        if (++steps > 4000 || h < 1e-14) return false;
        const double t1 = std::min(1.0, t + h);
        const CX     yt = mul(yfinal, BF(t1), p);
        const Target T  = target_from_y(yt, p + 32);
        const CX     dy = mul(yfinal, BF(t1 - t), p);
        const CX pr = newton_step(z, dy, p);
        CX zz = add(z, pr, p);
        bool ok = finite(zz) && l2(pr) < std::max(l2(z), 0.0) - 1.0;
        double prev = pinf;
        for (int it = 0; ok && it < 10; ++it) {
            CX     F;
            double eF;
            residual(zz, T, p, F, eF);
            const CX     d  = newton_step(zz, F, p);
            const double ld = l2(d);
            if (!finite(d) || ld > std::max(l2(zz), 0.0) - 1.0 || (it > 0 && ld > prev - 0.5 && ld > l2(zz) - 40.0)) {
                ok = false;
                break;
            }
            zz = sub(zz, d, p);
            if (ld == ninf || ld < std::max(l2(zz), 0.0) - 40.0) break;
            if (it == 9) ok = false;
            prev = ld;
        }
        if (ok) {
            z = zz;
            t = t1;
            h = std::min(1.0, 2.0 * h);
        } else {
            h *= 0.5;
        }
    }
    return true;
}

bool seed_for(const CX& Y, const CX* C, CX& z) {
    const Target t = C ? target_from_c(*C, 96) : target_from_y(Y, 96);
    if (l2(t.c) < -20.0 || (l2(Y) > 20.0 && !Y.re.signbit())) return seed_small(t.c, z);
    if (l2(t.d) < -20.0 || l2(Y) > 20.0) {
        if (!seed_small(t.d, z)) return false;
        z = neg(z);
        return true;
    }
    CX Yh = Y;
    if (Y.im.is_zero()) Yh.im = Y.re.abs().scaled_pow2(-30).with_sign(Y.im.signbit());
    return homotopy(Yh, z);
}

BC inverse(const CX& Y, const CX* C, const BCC& ctx, bool rotate) {
    CX seed;
    if (!seed_for(Y, C, seed)) return BC::undefined();
    return drive(ctx, [&](std::size_t w) {
        const Target T = C ? target_from_c(*C, w + 64) : target_from_y(Y, w + 64);
        CX     z = seed;
        double err;
        if (!polish(T, z, w + 8, &err)) return approx_undefined();
        Approx a = approx(z, err);
        return rotate ? swap_i(a, false) : a;
    });
}

} // namespace

BC inv_erf(const BC& y, const BCC& ctx) {
    if (y.is_indeterminate()) return BC::indeterminate();
    if (y.is_undefined() || !y.is_finite()) return BC::undefined();
    if (real_axis(y) && BF::compare(y.real().abs(), BF::one()) != BF::ordering::greater) return BC(math::inv_erf(y.real(), ctx.real()), BF::zero());
    if (imag_axis(y)) return BC(BF::zero(), math::inv_erfi(y.imaginary(), ctx.imaginary()));
    return inverse(cx(y), nullptr, ctx, false);
}

BC inv_erfc(const BC& c, const BCC& ctx) {
    if (c.is_indeterminate()) return BC::indeterminate();
    if (c.is_undefined() || !c.is_finite()) return BC::undefined();
    if (real_axis(c) && !c.real().signbit() && BF::compare(c.real(), BF(static_cast<std::int64_t>(2))) != BF::ordering::greater) {
        return BC(math::inv_erfc(c.real(), ctx.real()), BF::zero());
    }
    if (BF::compare(c.real(), BF::one()) == BF::ordering::equal) return BC(BF::zero(), -math::inv_erfi(c.imaginary(), ctx.imaginary()));
    const CX C = cx(c);
    const Target t = target_from_c(C, 128);
    return inverse(t.y, &C, ctx, false);
}

BC inv_erfi(const BC& v, const BCC& ctx) {
    if (v.is_indeterminate()) return BC::indeterminate();
    if (v.is_undefined() || !v.is_finite()) return BC::undefined();
    if (real_axis(v)) return BC(math::inv_erfi(v.real(), ctx.real()), BF::zero());
    if (imag_axis(v) && BF::compare(v.imaginary().abs(), BF::one()) != BF::ordering::greater) return BC(BF::zero(), math::inv_erf(v.imaginary(), ctx.imaginary()));
    return inverse(mul_i(cx(v)), nullptr, ctx, true);
}

namespace {

Approx fresnel_series(const CX& z, bool s_kind, std::size_t w) {
    const std::size_t w0 = w + 16;
    const CX     a0 = mul(sqr(z, w0 + 8), pi(w0 + 8).scaled_pow2(-1), w0 + 8);
    const double la = l2(a0);
    const std::size_t wb = w + bits_of(std::exp2(std::min(la, 900.0)) * log2e) + 16;
    const CX a = mul(sqr(z, wb + 8), pi(wb + 8).scaled_pow2(-1), wb + 8);
    CX   p = z;
    CX   sum;
    Term T;
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t j = 0;; ++j) {
        if ((j & 1u) == (s_kind ? 1u : 0u)) {
            CX term = div(p, BF(2 * j + 1), wb);
            if ((j / 2) & 1u) term = neg(term);
            sum = add(sum, term, wb);
            T.add(l2(term));
        }
        p = div(mul(p, a, wb), BF(j + 1), wb);
        if (la - std::log2(static_cast<double>(j + 2)) < -1.0) {
            const double bound = l2(p) + 1.0;
            if (bound == ninf || bound < T.l2max - static_cast<double>(wb)) {
                tail = bound;
                K    = j;
                break;
            }
        }
    }
    const double e = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail) + 1.0;
    return approx(sum, lsum(e, l2(sum) - static_cast<double>(wb) + 2.0));
}

Approx fresnel_erf(const CX& z, bool s_kind, std::size_t w) {
    const std::size_t wz = w + bits_of(2.0 * l2(z)) + 24;
    const BFC zc = wctx(wz + 8);
    const BF  s  = BF::div(math::sqrt(multiprecision::constants::pi(zc), zc), BF(static_cast<std::int64_t>(2)), zc);
    const BFC xc = wctx(wz + 16);
    const BF  dm = BF::sub(z.re, z.im, xc);
    const BF  dp = BF::add(z.re, z.im, xc);
    const CX  z1(BF::mul(dm, s, wctx(wz)), BF::mul(dp, s, wctx(wz)));
    const CX  z2(BF::mul(dp, s, wctx(wz)), BF::mul(-dm, s, wctx(wz)));
    const double lx  = l2(z.re), ly = l2(z.im);
    const double pxy = (lx == ninf || ly == ninf) ? 0.0 : std::exp2(std::min(lx + ly, 900.0)) * pi_d * log2e * ((z.re.signbit() != z.im.signbit()) ? -1.0 : 1.0);
    Approx e1 = erf_raw(z1, w + 8);
    Approx e2 = erf_raw(z2, w + 8);
    const double p1 = pxy + l2(z1) - static_cast<double>(wz) + 3.0;
    const double p2 = -pxy + l2(z2) - static_cast<double>(wz) + 3.0;
    e1.er = lsum(e1.er, p1);
    e1.ei = lsum(e1.ei, p1);
    e2.er = lsum(e2.er, p2);
    e2.ei = lsum(e2.ei, p2);
    const CX ie2 = mul_i(e2.v);
    const std::size_t wb = w + 16;
    const CX in = s_kind ? sub(e1.v, ie2, wb) : add(e1.v, ie2, wb);
    const double ein = lsum(lsum(e1.er, e1.ei), lsum(e2.er, e2.ei));
    const BFC c = wctx(wb);
    CX out;
    if (s_kind) {
        out = CX(BF::sub(in.re, in.im, c), BF::add(in.re, in.im, c));
    } else {
        out = CX(BF::add(in.re, in.im, c), BF::sub(in.im, in.re, c));
    }
    out = scale2(out, -2);
    Approx r;
    r.v  = out;
    r.er = lsum(ein, l2(out) - static_cast<double>(wb) + 3.0);
    r.ei = lsum(ein, l2(out) - static_cast<double>(wb) + 3.0);
    r.er = lsum(r.er, l2(in) - static_cast<double>(wb) + 1.0);
    r.ei = lsum(r.ei, l2(in) - static_cast<double>(wb) + 1.0);
    return r;
}

Approx fresnel_raw(const CX& z, bool s_kind, std::size_t w) {
    if (l2(z) <= 2.0) return fresnel_series(z, s_kind, w);
    return fresnel_erf(z, s_kind, w);
}

} // namespace

BC fresnel_c(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return BC(math::fresnel_c(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), math::fresnel_c(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return fresnel_raw(Z, false, w); });
}

BC fresnel_s(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return BC(math::fresnel_s(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), -math::fresnel_s(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return fresnel_raw(Z, true, w); });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
