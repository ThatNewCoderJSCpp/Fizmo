#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double log2e  = 1.4426950408889634;
constexpr std::uint64_t term_cap = 4000000;

BCC bcc(std::size_t w) { return BCC(BFC::clamp_precision(w)); }

double magd(const CX& z) { return std::exp2(std::min(l2(z), 1000.0)); }

double red(const CX& z) {
    const double l = l2(z.re);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 1000.0)) * (z.re.signbit() ? -1.0 : 1.0);
}

double imd(const CX& z) {
    const double l = l2(z.im);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 1000.0)) * (z.im.signbit() ? -1.0 : 1.0);
}

double absd(double re, double im) { return std::hypot(re, im); }

std::size_t bits(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(std::ceil(b), 1.0e7)); }

CV with_mag_err(const CX& v, double e) { return CV{v, e, e}; }

CV computed(const BC& v, std::size_t w) {
    const CX c = cx(v);
    return CV{c, c.re.is_zero() ? ninf : l2(c.re) - static_cast<double>(w) + 1.0, c.im.is_zero() ? ninf : l2(c.im) - static_cast<double>(w) + 1.0};
}

CV exp_with_input_error(const CX& E, double e_abs, std::size_t w) {
    const CX v = exp(E, w + 4);
    if (!finite(v)) return CV{v, ninf, ninf};
    const double lv  = l2(v);
    const double inp = (e_abs == ninf) ? ninf : lv + e_abs + 0.6;
    CV r;
    r.v  = v;
    r.er = lsum(v.re.is_zero() ? ninf : l2(v.re) - static_cast<double>(w + 4) + 1.0, inp);
    r.ei = lsum(v.im.is_zero() ? ninf : l2(v.im) - static_cast<double>(w + 4) + 1.0, inp);
    return r;
}

bool nonpositive_integer(const BC& a, std::int64_t& n) {
    if (!a.imaginary().is_zero() || !a.real().is_integer()) return false;
    if (!a.real().signbit() && !a.real().is_zero()) return false;
    const BF m = a.real().abs();
    if (BF::compare(m, BF(static_cast<std::uint64_t>(1) << 40)) == BF::ordering::greater) return false;
    n = static_cast<std::int64_t>(m.to_double());
    return true;
}

bool positive_integer(const BF& a, std::uint64_t& n) {
    if (!a.is_integer() || a.signbit() || a.is_zero()) return false;
    if (BF::compare(a, BF(static_cast<std::uint64_t>(4096))) == BF::ordering::greater) return false;
    n = static_cast<std::uint64_t>(a.to_double());
    return true;
}

struct Prefactor {
    CV   v;
    bool zero = false;
};

Prefactor prefactor(const BC& ab, const CX& z, bool minus_one, bool with_exp, bool regularized, std::size_t w) {
    Prefactor out;
    const CX     a    = cx(ab);
    const double lnz  = std::fabs(l2(z)) * 0.6931471805599453 + 4.0;
    double       mag  = std::max({magd(a) * lnz + 1.0, with_exp ? magd(z) : 0.0, 1.0});
    if (regularized) mag = std::max(mag, magd(cx(log_gamma(ab, bcc(64)))) + 1.0);
    const std::size_t wb = w + 12 + bits(std::log2(mag));
    CX A = a;
    if (minus_one) A = CX(BF::sub(a.re, BF::one(), BFC(BFC::max_prec, RoundingMode::nearest_even)), a.im);
    CX E = mul(A, ln(z, wb), wb);
    if (with_exp) E = sub(E, z, wb);
    if (regularized) E = sub(E, cx(log_gamma(ab, bcc(wb))), wb);
    const double eE = std::log2(4.0 * mag + magd(E)) - static_cast<double>(wb) + 3.0;
    if (E.re.signbit() && l2(E.re) > 46.0) {
        out.zero = true;
        out.v    = CV{CX(), ninf, ninf};
        return out;
    }
    out.v = exp_with_input_error(E, eE, w + 4);
    return out;
}

struct Series {
    CV   s;
    bool ok = false;
};

Series kummer(const CX& a, const CX& z, std::size_t w) {
    Series out;
    const double zr = red(z), zi = imd(z), az = absd(zr, zi);
    const double ar = red(a), ai = imd(a);
    const double loss = std::max(0.0, az - std::fabs(zr)) * log2e;
    const std::size_t wb = w + 16 + bits(loss) + bits(std::log2(az + 2.0) * 2.0);
    CX   t = recip(a, wb);
    CX   s = t;
    Term T;
    T.add(l2(t));
    double tail = ninf;
    std::uint64_t K = 0;

    for (std::uint64_t k = 0; k < term_cap; ++k) {
        const CX den = add_real(a, BF(k + 1), wb + 8);
        t = div(mul(t, z, wb), den, wb);
        s = add(s, t, wb);
        T.add(l2(t));
        const double kd = static_cast<double>(k) + 2.0;
        if (kd < -ar) continue;
        const double dn = absd(ar + kd, ai);
        const double r  = az / dn * (1.0 + 1.0e-12);
        if (!(r < 0.5)) continue;
        const double bound = l2(t) + std::log2(r / (1.0 - r));
        if (t.re.is_zero() && t.im.is_zero()) {
            K = k;
            break;
        }
        if (bound < T.l2max - static_cast<double>(wb) - 2.0) {
            tail = bound;
            K    = k;
            break;
        }
        if (k + 1 == term_cap) return out;
    }

    const double e = lsum(T.l2sum + std::log2(8.0 * static_cast<double>(K) + 16.0) - static_cast<double>(wb), tail);
    out.s  = with_mag_err(s, lsum(e, T.l2max + std::log2(static_cast<double>(K) + 2.0) - static_cast<double>(wb)));
    out.ok = true;
    return out;
}

Series alternating(const CX& a, const CX& z, std::size_t w) {
    Series out;
    const double zr = red(z), zi = imd(z), az = absd(zr, zi);
    const double ar = red(a), ai = imd(a);
    const double loss = std::max(0.0, az - std::fabs(zr)) * log2e;
    const std::size_t wb = w + 16 + bits(loss) + bits(std::log2(az + 2.0) * 2.0);
    const CX mz = neg(z);
    CX   p(BF::one());
    CX   s = recip(a, wb);
    Term T;
    T.add(l2(s));
    double tail = ninf;
    std::uint64_t K = 0;

    for (std::uint64_t k = 1; k < term_cap; ++k) {
        p = div(mul(p, mz, wb), BF(k), wb);
        const CX term = div(p, add_real(a, BF(k), wb + 8), wb);
        s = add(s, term, wb);
        T.add(l2(term));
        const double kd = static_cast<double>(k);
        if (kd + 1.0 < -ar) continue;
        const double rho = az / (kd + 2.0);
        if (!(rho < 0.5)) continue;
        const double dn    = absd(ar + kd + 1.0, ai);
        const double bound = l2(p) + std::log2(az / (kd + 1.0)) - std::log2(dn) + 1.0;
        if (p.re.is_zero() && p.im.is_zero()) {
            K = k;
            break;
        }
        if (bound < T.l2max - static_cast<double>(wb) - 2.0) {
            tail = bound;
            K    = k;
            break;
        }
        if (k + 1 == term_cap) return out;
    }

    const double e = lsum(T.l2sum + std::log2(8.0 * static_cast<double>(K) + 16.0) - static_cast<double>(wb), tail);
    out.s  = with_mag_err(s, lsum(e, T.l2max + std::log2(static_cast<double>(K) + 2.0) - static_cast<double>(wb)));
    out.ok = true;
    return out;
}

double asym_factor(double ar, double zr, double az, double n) {
    const double m = std::max(0.0, ar - n - 1.0);
    if (!(zr > m)) return 1.0e301;
    return std::log2(az / (zr - m));
}

bool asym_plan(const CX& a, const CX& z, std::size_t w) {
    const double zr = red(z), az = magd(z), ar = red(a), ai = imd(a);
    if (!(zr > 0.0)) return false;
    const double lz = l2(z);
    double l    = 0.0;
    double prev = 0.0;
    const double target = -(static_cast<double>(w) + 16.0);
    for (std::uint64_t k = 1; k < 200000; ++k) {
        const double kd = static_cast<double>(k);
        const double d  = absd(ar - kd, ai);
        if (d == 0.0) return true;
        l += std::log2(d) - lz;
        const double f = asym_factor(ar, zr, az, kd);
        if (f < 1.0e300 && l + f < target) return true;
        if (kd > std::fabs(ar) + std::fabs(ai) + 2.0 && l >= prev) return false;
        prev = l;
    }
    return false;
}

Series asym_sum(const CX& a, const CX& z, std::size_t w) {
    Series out;
    const double zr = red(z), az = magd(z), ar = red(a), ai = imd(a);
    const std::size_t wb = w + 16;
    const CX r = recip(z, wb + 8);
    CX   u(BF::one());
    CX   s = u;
    Term T;
    T.add(0.0);
    double prev = 0.0;

    for (std::uint64_t k = 1; k < 400000; ++k) {
        const double kd = static_cast<double>(k);
        u = mul(mul(u, add_real(a, BF(-static_cast<std::int64_t>(k)), wb + 8), wb), r, wb);
        const double lu = l2(u);
        const double f  = asym_factor(ar, zr * (1.0 - 1.0e-14), az, kd);
        if (lu == ninf) {
            out.s  = with_mag_err(s, T.l2sum + std::log2(8.0 * kd + 16.0) - static_cast<double>(wb));
            out.ok = true;
            return out;
        }
        if (f < 1.0e300 && lu + f < T.l2max - static_cast<double>(wb) - 2.0) {
            const double e = lsum(T.l2sum + std::log2(8.0 * kd + 16.0) - static_cast<double>(wb), lu + f + 1.0e-9);
            out.s  = with_mag_err(s, lsum(e, T.l2max + std::log2(kd + 2.0) - static_cast<double>(wb)));
            out.ok = true;
            return out;
        }
        if (kd > std::fabs(ar) + std::fabs(ai) + 2.0 && lu >= prev) return out;
        s = add(s, u, wb);
        T.add(lu);
        prev = lu;
    }

    return out;
}

enum class Kind { lower, upper };

Approx incomplete_raw(const BC& ab, const BC& zb, Kind kind, bool regularized, std::size_t w) {
    const CX a = cx(ab);
    const CX z = cx(zb);
    const std::size_t wi = w + 8;

    auto one_or_gamma = [&]() -> CV {
        if (regularized) return CV{CX(BF::one()), ninf, ninf};
        return computed(gamma(ab, bcc(wi + 4)), wi + 4);
    };

    if (asym_plan(a, z, wi)) {
        const Series A = asym_sum(a, z, wi);
        if (A.ok) {
            const Prefactor P    = prefactor(ab, z, true, true, regularized, wi);
            const CV        main = P.zero ? CV{CX(), ninf, ninf} : cv_mul(P.v, A.s, wi);
            if (kind == Kind::upper) return approx(main);
            return approx(cv_sub(one_or_gamma(), main, wi));
        }
    }

    const bool   k_form = !z.re.signbit();
    const Series S      = k_form ? kummer(a, z, wi) : alternating(a, z, wi);
    if (!S.ok) return approx_undefined();
    const Prefactor P = prefactor(ab, z, false, k_form, regularized, wi);
    const CV main = P.zero ? CV{CX(), ninf, ninf} : cv_mul(P.v, S.s, wi);
    if (kind == Kind::lower) return approx(main);
    return approx(cv_sub(one_or_gamma(), main, wi));
}

Approx upper_pole(const BC& z, std::int64_t n, std::size_t w) {
    const std::size_t wi = w + 8;
    const CV E = computed(exp_integral_generalized(z, n + 1, bcc(wi + 4)), wi + 4);
    if (n == 0) return approx(E);
    const CX Pv = pow_int(cx(z), -n, wi + 4);
    const CV P{Pv, Pv.re.is_zero() ? ninf : l2(Pv.re) - static_cast<double>(wi + 4) + 1.0, Pv.im.is_zero() ? ninf : l2(Pv.im) - static_cast<double>(wi + 4) + 1.0};
    return approx(cv_mul(E, P, wi));
}

BF factorial_bf(std::uint64_t n) {
    BigUInt f = BigUInt::one();
    for (std::uint64_t k = 2; k <= n; ++k) f = f * BigUInt(k);
    return BF(f);
}

BC real_integer_upper(std::uint64_t n, const BF& x, bool lower, bool regularized, const BCC& ctx) {
    const BFC exact(BFC::max_prec, RoundingMode::nearest_even);
    BF        N    = BF::one();
    BigUInt   coef = BigUInt::one();
    for (std::uint64_t k = n - 1; k-- > 0;) {
        coef = coef * BigUInt(k + 1);
        N    = BF::add(BF::mul(N, x, exact), BF(coef), exact);
    }
    const BF fact = factorial_bf(n - 1);
    const BF re = tracked::drive(ctx.real(), [&](std::size_t w) {
        const tracked::RV E = tracked::exp(tracked::exact(-x), w + 8);
        tracked::RV       G = tracked::mul(E, tracked::exact(N), w + 8);
        if (regularized) G = tracked::div(G, tracked::exact(fact), w + 8);
        if (!lower) return G;
        const tracked::RV one = tracked::exact(regularized ? BF::one() : fact);
        return tracked::sub(one, G, w);
    });
    return BC(re, BF::zero());
}

BC real_regularized(const BF& a, const BF& x, bool lower, const BCC& ctx) {
    const BF re = tracked::drive(ctx.real(), [&](std::size_t w) {
        const BFC         c = tracked::ctx(w + 8);
        const tracked::RV v = tracked::rounded(lower ? math::lower_incomplete_gamma(a, x, c) : math::upper_incomplete_gamma(a, x, c), w + 8);
        const tracked::RV g = tracked::rounded(math::gamma(a, c), w + 8);
        return tracked::div(v, g, w);
    });
    return BC(re, BF::zero());
}

BC incomplete(const BC& a, const BC& z, Kind kind, bool regularized, const BCC& ctx) {
    if (a.is_undefined() || z.is_undefined()) return BC::undefined();
    if (a.is_indeterminate() || z.is_indeterminate()) return BC::indeterminate();
    if (!a.is_finite() || !z.is_finite()) return BC::undefined();
    std::int64_t pn = 0;
    const bool pole = nonpositive_integer(a, pn);

    if (pole) {
        if (regularized) return kind == Kind::lower ? BC(BF::one(), BF::zero()) : BC(BF::zero(), BF::zero());
        if (kind == Kind::lower) return BC::undefined();
        if (z.real().is_zero() && z.imaginary().is_zero()) return BC::undefined();
        return drive(ctx, [&](std::size_t w) { return upper_pole(z, pn, w); });
    }

    if (z.real().is_zero() && z.imaginary().is_zero()) {
        if (kind == Kind::lower) return a.real().signbit() || a.real().is_zero() ? BC::undefined() : BC::zero();
        if (regularized) return BC(BF::one(), BF::zero());
        return gamma(a, ctx);
    }

    if (a.imaginary().is_zero() && z.imaginary().is_zero()) {
        const BF& ar = a.real();
        const BF& xr = z.real();

        if (!xr.signbit()) {
            if (regularized) {
                if (!ar.signbit()) {
                    const BF v = kind == Kind::lower ? math::regularized_gamma_p(ar, xr, ctx.real()) : math::regularized_gamma_q(ar, xr, ctx.real());
                    return BC(v, BF::zero());
                }
                return real_regularized(ar, xr, kind == Kind::lower, ctx);
            }
            const BF v = kind == Kind::lower ? math::lower_incomplete_gamma(ar, xr, ctx.real()) : math::upper_incomplete_gamma(ar, xr, ctx.real());
            return BC(v, BF::zero());
        }

        std::uint64_t n = 0;
        if (positive_integer(ar, n)) return real_integer_upper(n, xr, kind == Kind::lower, regularized, ctx);
    }

    return drive(ctx, [&](std::size_t w) { return incomplete_raw(a, z, kind, regularized, w); });
}

Approx fib_raw(const CX& z, bool luc, std::size_t w) {
    const std::size_t wb = w + 12 + extra_bits(z);
    const BFC c = wctx(wb + 8);
    const BF  s5 = math::sqrt(BF(5), c);
    const BF  lp = math::ln(BF::add(BF::one(), s5, c).scaled_pow2(-1), c);
    const CX  E  = mul(z, CX(lp), wb + 8);
    const double eE = l2(z) + l2(lp) - static_cast<double>(wb + 8) + 3.0;
    const CV A = exp_with_input_error(E, eE, wb);
    const CV B = exp_with_input_error(neg(E), eE, wb);
    const CV C = cospi(z, wb);
    const CV D = cv_mul(C, B, wb);
    if (luc) return approx(cv_add(A, D, wb));
    CV R = cv_sub(A, D, wb);
    const BF inv = BF::div(BF::one(), s5, c);
    R = cv_scale(R, inv, wb);
    const double rel = l2(R.v) - static_cast<double>(wb) + 1.0;
    R.er = lsum(R.er, rel);
    R.ei = lsum(R.ei, rel);
    return approx(R);
}

BC fib_complex(const BC& z, bool luc, const BCC& ctx) {
    if (z.is_indeterminate()) return BC::indeterminate();
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero()) {
        const BF v = luc ? math::lucas(z.real(), ctx.real()) : math::fibonacci(z.real(), ctx.real());
        return BC(v, BF::zero());
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return fib_raw(Z, luc, w); });
}

CX one_minus(const CX& z, std::size_t w) { return sub(CX(BF::one()), z, w); }

double safe_l2(const CX& z) { return is_zero(z) ? ninf : l2(z); }

struct BetaPart {
    CX     v;
    double l2max = ninf;
    double err   = ninf;
    bool   ok    = false;
};

BetaPart beta_series(const CX& z, const CX& a, const CX& b, std::size_t wb) {
    BetaPart out;
    const double az = magd(z);
    const double am = magd(a);
    const double bm = magd(b);
    CX t(BF::one());
    CX s = recip(a, wb);
    Term T;
    T.add(safe_l2(s));
    std::uint64_t K = 0;

    for (std::uint64_t n = 0;; ++n) {
        if (n + 1 >= term_cap) return out;
        const BF k(n + 1);
        t = div(mul(mul(t, sub(CX(k), b, wb), wb), z, wb), k, wb);
        const CX d = add_real(a, k, wb);
        if (is_zero(d)) return out;
        const CX term = div(t, d, wb);
        s = add(s, term, wb);
        const double lt = safe_l2(term);
        T.add(lt);
        K = n + 1;
        if (is_zero(t)) break;
        const double nd = static_cast<double>(n) + 1.0;
        if (nd < am + bm + 4.0) continue;
        const double r = az * (1.0 + (bm + 1.0) / nd);
        if (!(r < 0.95)) continue;
        if (lt + std::log2(r / (1.0 - r)) + 2.0 < T.l2max - static_cast<double>(wb)) break;
    }

    const CX p = pow(z, a, wb);
    out.v     = mul(p, s, wb);
    out.l2max = T.l2max + safe_l2(p);
    out.err   = out.l2max + std::log2(static_cast<double>(K) + 4.0) + 4.0 - static_cast<double>(wb);
    out.ok    = finite(out.v);
    return out;
}

CX scale_by(const CX& h, double f, std::size_t w) { return mul(h, BF(static_cast<long double>(f)), w); }

BetaPart beta_walk(const CX& z, const CX& a, const CX& b, std::size_t wb) {
    BetaPart out;
    const CX half(BF::one().scaled_pow2(-1));
    BetaPart start = beta_series(half, a, b, wb);
    if (!start.ok) return out;

    const CX ab2 = sub(sub(CX(BF(2)), a, wb), b, wb);
    CX f = pow(CX(BF(2)), ab2, wb);
    CX B = start.v;
    CX t = half;
    double l2max = start.l2max;
    double err   = start.err;
    std::uint64_t work = 0;

    const CX am1 = add_real(a, BF(-1), wb);
    const CX bm1 = add_real(b, BF(-1), wb);
    const CX P1  = ab2;

    std::vector<CX> path;
    if (z.im.is_zero()) {
        const BF dist = BF::sub(z.re, half.re, wctx(wb)).abs();
        BF lift = BF::compare(dist, BF::one()) == BF::ordering::less ? BF::one() : dist;
        if (BF::compare(z.re, half.re) == BF::ordering::greater) lift = -lift;
        path.push_back(CX(half.re, lift));
        path.push_back(CX(z.re, lift));
    }
    path.push_back(z);

    for (const CX& target : path) {
        for (std::uint64_t guard = 0;; ++guard) {
            if (guard > 200000) return out;
            const CX hf = sub(target, t, wb);
            if (is_zero(hf)) break;
            const double L   = magd(hf);
            const double rad = std::min(magd(t), magd(one_minus(t, wb)));
            const double cap = 0.4 * rad;
            const bool   last = L <= cap;
            const CX h = last ? hf : scale_by(hf, cap / L, wb);

            const CX omt  = one_minus(t, wb);
            const CX Q0i  = recip(mul(t, omt, wb), wb);
            const CX Q1   = sub(omt, t, wb);
            const CX P0   = sub(mul(am1, omt, wb), mul(bm1, t, wb), wb);

            CX cprev;
            CX c     = f;
            CX hn(BF::one());
            CX fsum  = f;
            CX I     = mul(f, h, wb);
            double lmax  = safe_l2(I);
            int    small = 0;

            for (std::uint64_t n = 0;; ++n) {
                if (n > 4 * wb + 200) return out;
                const BF nb(n);
                const CX lhs = mul(sub(P0, mul(Q1, nb, wb), wb), c, wb);
                const CX rhs = mul(add_real(P1, BF(static_cast<std::int64_t>(n) - 1), wb), cprev, wb);
                const CX cn  = div(mul(add(lhs, rhs, wb), Q0i, wb), BF(n + 1), wb);
                cprev = c;
                c     = cn;
                hn    = mul(hn, h, wb);
                const CX fterm = mul(c, hn, wb);
                fsum = add(fsum, fterm, wb);
                const CX iterm = div(mul(fterm, h, wb), BF(n + 2), wb);
                I = add(I, iterm, wb);
                const double li = safe_l2(iterm);
                if (li > lmax) lmax = li;
                ++work;
                const double lf = std::max(safe_l2(fterm), li);
                if (lf < std::max(lmax, safe_l2(fsum)) - static_cast<double>(wb) - 4.0) {
                    if (++small >= 2) break;
                } else {
                    small = 0;
                }
            }

            B = add(B, I, wb);
            f = fsum;
            t = last ? target : add(t, h, wb);
            l2max = std::max({l2max, lmax, safe_l2(B)});
            err   = lsum(err, lmax + 3.0 - static_cast<double>(wb));
            if (last) break;
        }
    }

    out.v     = B;
    out.l2max = l2max;
    out.err   = lsum(err, l2max + std::log2(static_cast<double>(work) + 8.0) + 6.0 - static_cast<double>(wb));
    out.ok    = finite(out.v);
    return out;
}

bool pole_at(const CX& a) {
    if (!a.im.is_zero() || !a.re.is_integer()) return false;
    return a.re.signbit() || a.re.is_zero();
}

BetaPart beta_value(const CX& z, const CX& a, const CX& b, std::size_t wb) {
    if (pole_at(a)) return BetaPart{};
    if (magd(z) <= 0.6) return beta_series(z, a, b, wb);
    const CX w = one_minus(z, wb);

    if (magd(w) <= 0.6 && !pole_at(b)) {
        BetaPart tail = beta_series(w, b, a, wb);
        if (!tail.ok) return tail;
        const BC full = beta(bc(a), bc(b), bcc(wb));
        if (full.is_undefined() || !full.is_finite()) return BetaPart{};
        const CX F = cx(full);
        BetaPart out;
        out.v     = sub(F, tail.v, wb);
        out.l2max = std::max(tail.l2max, safe_l2(F));
        out.err   = lsum(tail.err, out.l2max + 2.0 - static_cast<double>(wb));
        out.ok    = finite(out.v);
        return out;
    }

    return beta_walk(z, a, b, wb);
}

enum class BetaKind { plain, regularized, complement };

Approx beta_raw(const CX& z, const CX& a, const CX& b, BetaKind kind, std::size_t w) {
    const std::size_t wb = w + 20 + extra_bits(z) + extra_bits(a) + extra_bits(b);
    BetaPart part = kind == BetaKind::complement ? beta_value(one_minus(z, wb), b, a, wb) : beta_value(z, a, b, wb);
    if (!part.ok) return approx_undefined();
    if (kind == BetaKind::plain) return approx(part.v, part.err);
    const BC full = beta(bc(a), bc(b), bcc(wb));
    if (full.is_undefined() || !full.is_finite() || full.is_zero()) return approx_undefined();
    const CX F = cx(full);
    const CX v = div(part.v, F, wb);
    const double e = lsum(part.err - safe_l2(F), safe_l2(v) + 2.0 - static_cast<double>(wb)) + 1.0;
    return approx(v, e);
}

bool real_beta_domain(const BC& z, const BC& a, const BC& b) {
    if (!z.imaginary().is_zero() || !a.imaginary().is_zero() || !b.imaginary().is_zero()) return false;
    const BF& x = z.real();
    if (x.signbit() || BF::compare(x, BF::one()) == BF::ordering::greater) return false;
    return !a.real().signbit() && !a.real().is_zero() && !b.real().signbit() && !b.real().is_zero();
}

BC beta_complex(const BC& z, const BC& a, const BC& b, BetaKind kind, const BCC& ctx) {
    if (z.is_indeterminate() || a.is_indeterminate() || b.is_indeterminate()) return BC::indeterminate();
    if (z.is_undefined() || a.is_undefined() || b.is_undefined()) return BC::undefined();
    if (!z.is_finite() || !a.is_finite() || !b.is_finite()) return BC::undefined();

    if (real_beta_domain(z, a, b)) {
        const BF& x = z.real();
        const BF& p = a.real();
        const BF& q = b.real();
        if (kind == BetaKind::plain) return BC(math::incomplete_beta(x, p, q, ctx.real()), BF::zero());
        if (kind == BetaKind::regularized) return BC(math::regularized_beta(x, p, q, ctx.real()), BF::zero());
        return BC(math::regularized_beta_complement(x, p, q, ctx.real()), BF::zero());
    }

    const CX Z = cx(z), A = cx(a), Bb = cx(b);
    const bool at_zero = is_zero(Z);
    const bool at_one  = Z.im.is_zero() && BF::compare(Z.re, BF::one()) == BF::ordering::equal;

    if (at_zero || at_one) {
        const bool empty = (kind == BetaKind::complement) ? at_one : at_zero;
        const CX&  edge  = (kind == BetaKind::complement) ? Bb : A;
        if (empty) {
            if (edge.re.signbit() || edge.re.is_zero()) return BC::undefined();
            return BC::zero();
        }
        if (kind != BetaKind::plain) {
            const CX& other = (kind == BetaKind::complement) ? A : Bb;
            if (other.re.signbit() || other.re.is_zero()) return BC::undefined();
            return BC(BF::one(), BF::zero());
        }
        if (Bb.re.signbit() || Bb.re.is_zero()) return BC::undefined();
        return beta(a, b, ctx);
    }

    return drive(ctx, [&](std::size_t w) { return beta_raw(Z, A, Bb, kind, w); });
}

} // namespace

BC lower_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::lower, false, ctx); }
BC upper_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::upper, false, ctx); }
BC regularized_gamma_p(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::lower, true, ctx); }
BC regularized_gamma_q(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::upper, true, ctx); }

BC fibonacci(const BC& z, const BCC& ctx) { return fib_complex(z, false, ctx); }
BC lucas(const BC& z, const BCC& ctx) { return fib_complex(z, true, ctx); }

BC incomplete_beta(const BC& z, const BC& a, const BC& b, const BCC& ctx) { return beta_complex(z, a, b, BetaKind::plain, ctx); }
BC regularized_beta(const BC& z, const BC& a, const BC& b, const BCC& ctx) { return beta_complex(z, a, b, BetaKind::regularized, ctx); }
BC regularized_beta_complement(const BC& z, const BC& a, const BC& b, const BCC& ctx) { return beta_complex(z, a, b, BetaKind::complement, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
