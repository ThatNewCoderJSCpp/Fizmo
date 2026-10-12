#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double ln2d = 0.6931471805599453;

double l2_bern(std::size_t k2) {
    const double n = static_cast<double>(k2);
    return 1.0 + std::lgamma(n + 1.0) / ln2d - n * std::log2(2.0 * 3.141592653589793);
}

BF shifted_re(const BF& x, std::uint64_t k, std::size_t w) {
    if (k == 0) return x;
    return BF::add(x, BF(k), wctx(w + 64 + extra_bits(x)));
}

struct Stirling {
    CX            S;
    CX            W;
    std::uint64_t N   = 0;
    double        err = ninf;
};

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

Stirling stirling(const CX& z, std::size_t w) {
    double R = 0.16 * static_cast<double>(w) + 12.0;
    for (;;) {
        Stirling out;
        const double zre = to_d(z.re);
        out.N = zre >= R ? 0 : static_cast<std::uint64_t>(std::ceil(R - zre));
        out.W = CX(shifted_re(z.re, out.N, w), z.im);
        const CX&    W      = out.W;
        const double lw     = l2(W);
        const double wre    = std::exp2(l2(W.re) - lw);
        const double l2sec2 = 1.0 - std::log2(1.0 + wre);
        const double scale  = lw + std::log2(std::max(1.0, lw * ln2d));
        const double target = scale - static_cast<double>(w) - 2.0;
        const std::size_t wb = w + 8;
        const CX u  = recip(W, wb);
        const CX u2 = sqr(u, wb);
        CX       pw = u;
        CX       sum;
        Term     T;
        double   prev  = pinf;
        double   trunc = ninf;
        bool     retry = false;
        for (std::size_t k = 1;; ++k) {
            const double lc    = l2_bern(2 * k) - std::log2(static_cast<double>(2 * k) * static_cast<double>(2 * k - 1));
            const double lt    = lc - static_cast<double>(2 * k - 1) * lw;
            const double bound = lt + static_cast<double>(k) * l2sec2 + 1.0;
            if (bound < target) {
                trunc = bound;
                break;
            }
            if (k > 2 && bound > prev) {
                retry = true;
                break;
            }
            prev = bound;
            const BF c    = BF::div(multiprecision::constants::bernoulli(2 * k, wctx(wb)), BF(static_cast<std::uint64_t>(2 * k * (2 * k - 1))), wctx(wb));
            const CX term = mul(pw, c, wb);
            sum = add(sum, term, wb);
            T.add(l2(term));
            pw = mul(pw, u2, wb);
        }
        if (retry) {
            R *= 1.6;
            continue;
        }
        const CX  L     = ln(W, wb);
        const BFC c     = wctx(wb);
        const BF  hl2pi = math::ln(BF::mul(multiprecision::constants::pi(c), BF(std::uint64_t(2)), c), c).scaled_pow2(-1);
        const CX  A     = sub(mul(add_real(W, BF::one().scaled_pow2(-1).with_sign(true), wb), L, wb), W, wb);
        out.S = add(add_real(A, hl2pi, wb), sum, wb);
        const double mag = lsum(lsum(lw + std::log2(std::max(1.0, l2(L))), lw), T.l2sum);
        out.err = lsum(mag - static_cast<double>(w) + 4.0, trunc);
        return out;
    }
}

bool pole(const BC& z) { return z.imaginary().is_zero() && z.real().is_integer() && (z.real().signbit() || z.real().is_zero()); }

CX one_minus(const CX& z, std::size_t w) { return CX(BF::sub(BF::one(), z.re, wctx(w + 64 + extra_bits(z.re))), z.im.with_sign(!z.im.signbit())); }

double arg_sum(const CX& z, std::uint64_t N) {
    const double re = std::isfinite(l2(z.re)) ? std::exp2(l2(z.re)) * (z.re.signbit() ? -1.0 : 1.0) : 0.0;
    const double im = std::isfinite(l2(z.im)) ? std::exp2(l2(z.im)) * (z.im.signbit() ? -1.0 : 1.0) : 0.0;
    double s = 0.0;
    for (std::uint64_t k = 0; k < N; ++k) s += std::atan2(im, re + static_cast<double>(k));
    return s;
}

CX shift_product(const CX& z, std::uint64_t N, std::size_t w) {
    CX P(BF::one());
    for (std::uint64_t k = 0; k < N; ++k) P = mul(P, CX(shifted_re(z.re, k, w), z.im), w);
    return P;
}

Approx loggamma_right(const CX& z, std::size_t w) {
    const Stirling st = stirling(z, w);
    if (st.N == 0) return approx(st.S, st.err);
    const std::size_t wp = w + 8 + static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(st.N) + 1.0)));
    const CX P  = shift_product(z, st.N, wp);
    const CX lP = ln(P, w + 8);
    const double want = arg_sum(z, st.N);
    const double m    = std::round((want - to_d(lP.im)) / (2.0 * 3.141592653589793));
    CX r = sub(st.S, lP, w + 8);
    if (m != 0.0) {
        const BFC c = wctx(w + 16);
        r.im = BF::add(r.im, BF::mul(multiprecision::constants::pi(c), BF(static_cast<std::int64_t>(-2.0 * m)), c), wctx(w + 8));
    }
    return approx(r, lsum(st.err, lsum(l2(lP), l2(st.S)) - static_cast<double>(w) + 3.0));
}

BC loggamma_negative_axis(const BC& z, const BCC& ctx) {
    const BF& x  = z.real();
    const BF  k  = math::ceiling(x.with_sign(false));
    const bool below = z.imaginary().signbit();
    const BF  re = math::log_abs_gamma(x, ctx.real());
    BF im = BF::mul(multiprecision::constants::pi(wctx(ctx.imaginary_precision + 16 + extra_bits(k))), k, ctx.imaginary());
    if (!below) im = im.with_sign(true);
    return BC(re, im);
}

} // namespace

BC log_gamma(const BC& z, const BCC& ctx) {
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (pole(z)) return BC::undefined();
    if (z.imaginary().is_zero()) {
        if (!z.real().signbit()) return BC(math::log_gamma(z.real(), ctx.real()), BF::zero());
        return loggamma_negative_axis(z, ctx);
    }
    const CX Z = cx(z);
    const bool left = z.real().signbit() || BF::compare(z.real(), BF::one().scaled_pow2(-1)) == BF::ordering::less;
    return drive(ctx, [&](std::size_t w) {
        if (!left) return loggamma_right(Z, w);
        const std::size_t wi = w + extra_bits(Z) + 8;
        const Approx r = loggamma_right(one_minus(Z, wi), wi);
        const CV  s  = sinpi(Z, wi);
        const CX  ls = ln(s.v, wi);
        const BFC c  = wctx(wi);
        const BF  lpi = math::ln(multiprecision::constants::pi(c), c);
        CX v = sub(sub(CX(lpi), ls, wi), r.v, wi);
        const double xd = to_d(Z.re);
        const double k  = std::floor(xd / 2.0 + 0.25) * (Z.im.signbit() ? -1.0 : 1.0);
        if (k != 0.0) v.im = BF::add(v.im, BF::mul(multiprecision::constants::pi(c), BF(static_cast<std::int64_t>(2.0 * k)), c), c);
        const double e = lsum(r.er, lsum(l2(ls), l2(v)) - static_cast<double>(wi) + 3.0);
        return approx(v, e);
    });
}

namespace {

Approx gamma_right(const CX& z, std::size_t w) {
    const std::size_t extra = static_cast<std::size_t>(std::max(0.0, std::ceil(l2(z) + std::log2(std::max(1.0, l2(z))) + 4)));
    const Stirling st = stirling(z, w + extra);
    const CX E = exp(st.S, w + 8);
    CX v = E;
    double e = l2(E) + lsum(st.err, -static_cast<double>(w) - 6);
    if (st.N > 0) {
        const std::size_t wp = w + 8 + static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(st.N) + 1.0)));
        const CX P = shift_product(z, st.N, wp);
        v = div(E, P, w + 8);
        e = l2(v) + lsum(st.err, -static_cast<double>(w) - 4);
    }
    return approx(v, e + 2.0);
}

} // namespace

BC gamma(const BC& z, const BCC& ctx) {
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (pole(z)) return BC::undefined();
    if (z.imaginary().is_zero()) return BC(math::gamma(z.real(), ctx.real()), BF::zero());
    const CX Z = cx(z);
    const bool left = z.real().signbit() || BF::compare(z.real(), BF::one().scaled_pow2(-1)) == BF::ordering::less;
    return drive(ctx, [&](std::size_t w) {
        if (!left) return gamma_right(Z, w);
        const std::size_t wi = w + 8;
        const Approx g = gamma_right(one_minus(Z, wi + extra_bits(Z)), wi);
        const CV s = sinpi(Z, wi);
        const CX d = mul(s.v, g.v, wi);
        const CX v = div(CX(pi(wi)), d, wi);
        return approx(v, l2(v) + lsum(g.er - l2(g.v), -static_cast<double>(wi)) + 2.0);
    });
}

BC factorial(const BC& z, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.is_finite()) return BC(math::factorial(z.real(), ctx.real()), BF::zero());
    return gamma(BC::add(z, BC::one(), BCC(BFC::max_prec / 8)), ctx);
}

namespace {

CX sum3(const CX& a, const CX& b, const CX& c, std::size_t w, bool sub_c) { return sub_c ? sub(add(a, b, w), c, w) : add(add(a, b, w), c, w); }

} // namespace

BC log_beta(const BC& a, const BC& b, const BCC& ctx) {
    if (a.is_undefined() || b.is_undefined() || !a.is_finite() || !b.is_finite()) return BC::undefined();
    if (a.imaginary().is_zero() && b.imaginary().is_zero() && !a.real().signbit() && !b.real().signbit()) {
        return BC(math::log_beta(a.real(), b.real(), ctx.real()), BF::zero());
    }
    const BC s = BC::add(a, b, BCC(BFC::max_prec / 8));
    if (pole(a) || pole(b) || pole(s)) return BC::undefined();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const BCC c(BFC::clamp_precision(wi));
        const CX la = cx(log_gamma(a, c)), lb = cx(log_gamma(b, c)), ls = cx(log_gamma(s, c));
        const CX v = sum3(la, lb, ls, wi, true);
        return approx(v, lsum(lsum(l2(la), l2(lb)), l2(ls)) - static_cast<double>(wi) + 3.0);
    });
}

BC beta(const BC& a, const BC& b, const BCC& ctx) {
    if (a.is_undefined() || b.is_undefined() || !a.is_finite() || !b.is_finite()) return BC::undefined();
    if (a.imaginary().is_zero() && b.imaginary().is_zero()) return BC(math::beta(a.real(), b.real(), ctx.real()), BF::zero());
    const BC s = BC::add(a, b, BCC(BFC::max_prec / 8));
    if (pole(a) || pole(b)) return pole(s) ? BC::undefined() : BC::undefined();
    if (pole(s)) return BC::zero();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const std::size_t wl = wi + static_cast<std::size_t>(std::max(0.0, std::ceil(std::max(l2(cx(a)), l2(cx(b))) + 6)));
        const BCC c(BFC::clamp_precision(wl));
        const CX la = cx(log_gamma(a, c)), lb = cx(log_gamma(b, c)), ls = cx(log_gamma(s, c));
        const CX L = sum3(la, lb, ls, wl, true);
        const CX v = exp(L, wi);
        const double e = l2(v) + lsum(lsum(lsum(l2(la), l2(lb)), l2(ls)) - static_cast<double>(wl) + 3.0, -static_cast<double>(wi)) + 1.0;
        return approx(v, e);
    });
}

namespace {

std::vector<BigInt> cot_poly(unsigned n) {
    std::vector<BigInt> p{BigInt(0LL), BigInt(1LL)};
    for (unsigned k = 0; k < n; ++k) {
        std::vector<BigInt> d(p.size() > 1 ? p.size() - 1 : 1, BigInt(0LL));
        for (std::size_t i = 1; i < p.size(); ++i) d[i - 1] = p[i] * BigInt(static_cast<long long>(i));
        std::vector<BigInt> q(d.size() + 2, BigInt(0LL));
        for (std::size_t i = 0; i < d.size(); ++i) {
            q[i]     = q[i] - d[i];
            q[i + 2] = q[i + 2] - d[i];
        }
        while (q.size() > 1 && q.back().is_zero()) q.pop_back();
        p = q;
    }
    return p;
}

Approx polygamma_right(unsigned n, const CX& z, std::size_t w) {
    double R = 0.16 * static_cast<double>(w) + 12.0 + static_cast<double>(n);
    for (;;) {
        const double zre = to_d(z.re);
        const std::uint64_t N = zre >= R ? 0 : static_cast<std::uint64_t>(std::ceil(R - zre));
        const CX W(shifted_re(z.re, N, w), z.im);
        const double lw = l2(W);
        const double wre = std::exp2(l2(W.re) - lw);
        const double l2sec2 = 1.0 - std::log2(1.0 + wre);
        const std::size_t wb = w + 8 + n;
        const BFC bc_ = wctx(wb);
        const double lfact = std::lgamma(static_cast<double>(n) + 1.0) / ln2d;
        const double scale = n == 0 ? std::max(1.0, std::log2(std::max(1.0, lw * ln2d)) ) : lfact - static_cast<double>(n) * lw;
        const double target = scale - static_cast<double>(w) - 2.0;
        const CX u = recip(W, wb);
        const CX u2 = sqr(u, wb);
        CX head;
        Term T;
        if (n == 0) {
            head = sub(ln(W, wb), scale2(u, -1), wb);
        } else {
            const CX un = pow_int(u, static_cast<std::int64_t>(n), wb);
            const BF f1 = math::factorial(BF(static_cast<std::uint64_t>(n - 1)), bc_);
            const BF f0 = math::factorial(BF(static_cast<std::uint64_t>(n)), bc_);
            head = add(mul(un, f1, wb), mul(mul(un, u, wb), f0.scaled_pow2(-1), wb), wb);
        }
        T.add(l2(head));
        CX pw = n == 0 ? u2 : mul(pow_int(u, static_cast<std::int64_t>(n), wb), u2, wb);
        CX sum;
        double prev = pinf, trunc = ninf;
        bool retry = false;
        for (std::size_t k = 1;; ++k) {
            const double lc = l2_bern(2 * k) + (std::lgamma(static_cast<double>(2 * k + n)) - std::lgamma(static_cast<double>(2 * k) + 1.0)) / ln2d;
            const double lt = lc - static_cast<double>(2 * k + n) * lw;
            const double bound = lt + static_cast<double>(k + n) * l2sec2 + 1.0;
            if (bound < target) {
                trunc = bound;
                break;
            }
            if (k > 2 && bound > prev) {
                retry = true;
                break;
            }
            prev = bound;
            BF c = multiprecision::constants::bernoulli(2 * k, bc_);
            if (n == 0) c = BF::div(c, BF(static_cast<std::uint64_t>(2 * k)), bc_);
            else {
                const BF num = math::factorial(BF(static_cast<std::uint64_t>(2 * k + n - 1)), bc_);
                const BF den = math::factorial(BF(static_cast<std::uint64_t>(2 * k)), bc_);
                c = BF::mul(c, BF::div(num, den, bc_), bc_);
            }
            const CX term = mul(pw, c, wb);
            sum = add(sum, term, wb);
            T.add(l2(term));
            pw = mul(pw, u2, wb);
        }
        if (retry) {
            R *= 1.6;
            continue;
        }
        CX v = n == 0 ? sub(head, sum, wb) : add(head, sum, wb);
        if (n > 0 && (n % 2 == 0)) v = neg(v);
        double err = lsum(T.l2sum - static_cast<double>(w) + 3.0, trunc);
        if (N > 0) {
            CX acc;
            Term T2;
            const BF fn = math::factorial(BF(static_cast<std::uint64_t>(n)), bc_);
            for (std::uint64_t k = 0; k < N; ++k) {
                const CX zk(shifted_re(z.re, k, wb), z.im);
                const CX r  = n == 0 ? recip(zk, wb) : pow_int(recip(zk, wb), static_cast<std::int64_t>(n + 1), wb);
                acc = add(acc, r, wb);
                T2.add(l2(r));
            }
            if (n > 0) acc = mul(acc, fn, wb);
            const bool plus = n > 0 && (n % 2 == 1);
            v = plus ? add(v, acc, wb) : sub(v, acc, wb);
            err = lsum(err, T2.l2sum + (n > 0 ? lfact : 0.0) - static_cast<double>(w) + 3.0 + std::log2(static_cast<double>(N) + 1.0));
        }
        return approx(v, err);
    }
}

} // namespace

BC polygamma(const BC& z, int n, const BCC& ctx) {
    if (n == -1) return log_gamma(z, ctx);
    if (n < -1 || z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (pole(z)) return BC::undefined();
    if (z.imaginary().is_zero()) return BC(math::polygamma(z.real(), n, ctx.real()), BF::zero());
    const unsigned m = static_cast<unsigned>(n);
    const CX Z = cx(z);
    const bool left = z.real().signbit();
    if (!left) return drive(ctx, [&](std::size_t w) { return polygamma_right(m, Z, w); });
    const std::vector<BigInt> P = cot_poly(m);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8 + m + extra_bits(Z);
        const Approx r = polygamma_right(m, one_minus(Z, wi), wi);
        const CV s = sinpi(Z, wi);
        const CV c = cospi(Z, wi);
        const CX ct = div(c.v, s.v, wi);
        CX poly;
        Term T;
        for (std::size_t i = P.size(); i-- > 0;) {
            poly = mul(poly, ct, wi);
            if (!P[i].is_zero()) poly = add_real(poly, BF(P[i]), wi);
            T.add(l2(poly));
        }
        const BFC pc = wctx(wi);
        BF pin = multiprecision::constants::pi(pc);
        for (unsigned i = 0; i < m; ++i) pin = BF::mul(pin, multiprecision::constants::pi(pc), pc);
        const CX corr = mul(poly, pin, wi);
        CX v = (m % 2 == 0) ? r.v : neg(r.v);
        v = sub(v, corr, wi);
        const double e = lsum(r.er, lsum(T.l2max + l2(pin) + std::log2(static_cast<double>(P.size()) + 1.0) + (l2(ct) > 0 ? 0.0 : 0.0) + 4.0, l2(v)) - static_cast<double>(wi) + 3.0);
        return approx(v, e);
    });
}

BC digamma(const BC& z, const BCC& ctx) { return polygamma(z, 0, ctx); }
BC trigamma(const BC& z, const BCC& ctx) { return polygamma(z, 1, ctx); }

BC harmonic(const BC& z, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.is_finite()) return BC(math::harmonic(z.real(), ctx.real()), BF::zero());
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    const BC z1 = BC::add(z, BC::one(), BCC(BFC::max_prec / 8));
    if (pole(z1)) return BC::undefined();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const CX p = cx(digamma(z1, BCC(BFC::clamp_precision(wi))));
        const BF g = multiprecision::constants::euler_mascheroni(wctx(wi));
        const CX v = add_real(p, g, wi);
        return approx(v, lsum(l2(p), 0.0) - static_cast<double>(wi) + 2.0);
    });
}

namespace {

BC gamma_ratio(const std::vector<BC>& num, const std::vector<BC>& den, const BCC& ctx) {
    std::size_t np = 0, dp = 0;
    for (const BC& a : num) np += pole(a) ? 1 : 0;
    for (const BC& a : den) dp += pole(a) ? 1 : 0;
    if (np > dp) return BC::undefined();
    if (dp > np) return BC::zero();
    if (np > 0) return BC::undefined();
    return drive(ctx, [&](std::size_t w) {
        double mag = 0.0;
        for (const BC& a : num) mag = std::max(mag, l2(cx(a)));
        for (const BC& a : den) mag = std::max(mag, l2(cx(a)));
        const std::size_t wl = w + 10 + static_cast<std::size_t>(std::max(0.0, std::ceil(mag + std::log2(std::max(1.0, mag)) + 4)));
        const BCC c(BFC::clamp_precision(wl));
        CX L;
        double e = ninf;
        for (const BC& a : num) {
            const CX t = cx(log_gamma(a, c));
            L = add(L, t, wl);
            e = lsum(e, l2(t));
        }
        for (const BC& a : den) {
            const CX t = cx(log_gamma(a, c));
            L = sub(L, t, wl);
            e = lsum(e, l2(t));
        }
        const CX v = exp(L, w + 8);
        return approx(v, l2(v) + lsum(e - static_cast<double>(wl) + 3.0, -static_cast<double>(w) - 6.0) + 1.0);
    });
}

BC add_exact(const BC& a, const BC& b) { return BC::add(a, b, BCC(BFC::max_prec / 8)); }
BC sub_exact(const BC& a, const BC& b) { return BC::sub(a, b, BCC(BFC::max_prec / 8)); }
BC from_int(std::int64_t k) { return BC(BF(k)); }

BC product_ratio(const BC& x, std::int64_t n, bool falling, const BCC& ctx) {
    const bool          inv = n < 0;
    const std::uint64_t m   = inv ? static_cast<std::uint64_t>(-(n + 1)) + 1 : static_cast<std::uint64_t>(n);
    if (m == 0) return BC::one();
    const CX X = cx(x);
    for (std::uint64_t k = inv ? 1 : 0; k < m + (inv ? 1 : 0); ++k) {
        const std::int64_t d = (falling != inv) ? -static_cast<std::int64_t>(k) : static_cast<std::int64_t>(k);
        if (pole(add_exact(x, from_int(d))) && x.imaginary().is_zero() && BF::compare(add_exact(x, from_int(d)).real(), BF::zero()) == BF::ordering::equal) {
            if (inv) return BC::undefined();
            return BC::zero();
        }
    }
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wp = w + 8 + static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(m) + 1.0)));
        CV P = cv_exact(CX(BF::one()));
        for (std::uint64_t k = inv ? 1 : 0; k < m + (inv ? 1 : 0); ++k) {
            const std::int64_t d = (falling != inv) ? -static_cast<std::int64_t>(k) : static_cast<std::int64_t>(k);
            CV f = cv_exact(X);
            f = cv_add(f, cv_exact(CX(BF(d))), wp);
            P = cv_mul(P, f, wp);
        }
        if (!inv) return approx(P);
        const CX v = recip(P.v, wp);
        const double rel = lsum(lsum(P.er, P.ei) - l2(P.v), -static_cast<double>(wp)) + 2.0;
        return approx(v, l2(v) + rel);
    });
}

} // namespace

BC rising_pochhammer(const BC& x, std::int64_t n, const BCC& ctx) {
    if (x.is_undefined() || !x.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero()) return BC(math::rising_pochhammer(x.real(), n, ctx.real()), BF::zero());
    return product_ratio(x, n, false, ctx);
}

BC falling_pochhammer(const BC& x, std::int64_t n, const BCC& ctx) {
    if (x.is_undefined() || !x.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero()) return BC(math::falling_pochhammer(x.real(), n, ctx.real()), BF::zero());
    return product_ratio(x, n, true, ctx);
}

BC rising_pochhammer(const BC& x, const BC& a, const BCC& ctx) {
    if (x.is_undefined() || a.is_undefined() || !x.is_finite() || !a.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero() && a.imaginary().is_zero()) return BC(math::rising_pochhammer(x.real(), a.real(), ctx.real()), BF::zero());
    if (a.imaginary().is_zero() && a.real().is_integer() && a.real().get_exp_base2() < 62) {
        return rising_pochhammer(x, static_cast<std::int64_t>(a.real().get_integer_part().get_lowest_bits()) * (a.real().signbit() ? -1 : 1), ctx);
    }
    return gamma_ratio({add_exact(x, a)}, {x}, ctx);
}

BC falling_pochhammer(const BC& x, const BC& a, const BCC& ctx) {
    if (x.is_undefined() || a.is_undefined() || !x.is_finite() || !a.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero() && a.imaginary().is_zero()) return BC(math::falling_pochhammer(x.real(), a.real(), ctx.real()), BF::zero());
    if (a.imaginary().is_zero() && a.real().is_integer() && a.real().get_exp_base2() < 62) {
        return falling_pochhammer(x, static_cast<std::int64_t>(a.real().get_integer_part().get_lowest_bits()) * (a.real().signbit() ? -1 : 1), ctx);
    }
    return gamma_ratio({add_exact(x, BC::one())}, {add_exact(sub_exact(x, a), BC::one())}, ctx);
}

BC combination(const BC& x, std::int64_t k, const BCC& ctx) {
    if (x.is_undefined() || !x.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero()) return BC(math::combination(x.real(), k, ctx.real()), BF::zero());
    if (k < 0) return BC::zero();
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const CX f  = cx(falling_pochhammer(x, k, BCC(BFC::clamp_precision(wi))));
        const BF kf = math::factorial(BF(static_cast<std::uint64_t>(k)), wctx(wi));
        const CX v  = div(f, kf, wi);
        return approx(v, l2(v) - static_cast<double>(wi) + 2.0);
    });
}

BC combination(const BC& x, const BC& k, const BCC& ctx) {
    if (x.is_undefined() || k.is_undefined() || !x.is_finite() || !k.is_finite()) return BC::undefined();
    if (x.imaginary().is_zero() && k.imaginary().is_zero()) return BC(math::combination(x.real(), k.real(), ctx.real()), BF::zero());
    if (k.imaginary().is_zero() && k.real().is_integer() && k.real().get_exp_base2() < 62) {
        return combination(x, static_cast<std::int64_t>(k.real().get_integer_part().get_lowest_bits()) * (k.real().signbit() ? -1 : 1), ctx);
    }
    return gamma_ratio({add_exact(x, BC::one())}, {add_exact(k, BC::one()), add_exact(sub_exact(x, k), BC::one())}, ctx);
}

BC permutation(const BC& x, std::int64_t k, const BCC& ctx) { return falling_pochhammer(x, k, ctx); }
BC permutation(const BC& x, const BC& k, const BCC& ctx) { return falling_pochhammer(x, k, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
