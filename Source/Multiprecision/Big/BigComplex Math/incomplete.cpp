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
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero()) {
        const BF v = luc ? math::lucas(z.real(), ctx.real()) : math::fibonacci(z.real(), ctx.real());
        return BC(v, BF::zero());
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return fib_raw(Z, luc, w); });
}

} // namespace

BC lower_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::lower, false, ctx); }
BC upper_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::upper, false, ctx); }
BC regularized_gamma_p(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::lower, true, ctx); }
BC regularized_gamma_q(const BC& a, const BC& z, const BCC& ctx) { return incomplete(a, z, Kind::upper, true, ctx); }

BC fibonacci(const BC& z, const BCC& ctx) { return fib_complex(z, false, ctx); }
BC lucas(const BC& z, const BCC& ctx) { return fib_complex(z, true, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
