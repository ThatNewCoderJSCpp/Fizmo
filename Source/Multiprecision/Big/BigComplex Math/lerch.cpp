#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double log2e = 1.4426950408889634;

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::max(std::min(l, 1000.0), -1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

bool int64_of(const BF& x, std::int64_t& k) {
    if (!x.is_finite() || !x.is_integer()) return false;
    if (x.is_zero()) {
        k = 0;
        return true;
    }
    if (x.get_exp_base2() > 40) return false;
    const std::int64_t v = static_cast<std::int64_t>(x.get_integer_part().get_lowest_bits());
    k = x.signbit() ? -v : v;
    return true;
}

CX shift(const CX& a, std::int64_t n, std::size_t w) {
    const std::int64_t hi = std::max<std::int64_t>(64, a.re.is_zero() ? 64 : a.re.get_exp_base2() + 2);
    const std::int64_t lo = a.re.is_zero() ? 0 : std::min<std::int64_t>(0, a.re.exponent());
    const std::size_t  p  = std::max<std::size_t>(w, static_cast<std::size_t>(hi - lo + 8));
    return CX(BF::add(a.re, BF(n), wctx(p)), a.im);
}

CX cpow_neg(const CX& a, const CX& s, std::size_t w) {
    if (a.im.is_zero() && !a.re.signbit() && !a.re.is_zero()) return real_pow_neg(a.re, s, w);
    const std::size_t wl = w + extra_bits(s) + 8;
    const CX L = ln(a, wl);
    return exp(neg(mul(s, L, wl + extra_bits(L))), w);
}

Approx direct(const CX& z, const CX& s, const CX& a, std::size_t w) {
    const double lz  = l2(z);
    const double as  = std::exp2(std::min(l2(s), 60.0));
    const double rea = to_d(a.re);
    const std::size_t wb = w + 16;
    CX   zn(BF::one());
    CX   sum;
    Term T;
    double tail = ninf;
    std::uint64_t N = 0;
    for (std::uint64_t n = 0;; ++n) {
        const CX an = shift(a, static_cast<std::int64_t>(n), wb);
        if (is_zero(an)) return approx_undefined();
        const CX t = mul(zn, cpow_neg(an, s, wb + 4), wb);
        sum = add(sum, t, wb);
        T.add(l2(t));
        const double base = static_cast<double>(n) + rea;
        if (base >= 2.0) {
            const double rho = lz + log2e * 2.0 * as / base;
            if (rho < -0.15) {
                const double bound = l2(t) + rho - std::log2(1.0 - std::exp2(rho));
                if (bound == ninf || bound < T.l2max - static_cast<double>(wb)) {
                    tail = bound;
                    N    = n;
                    break;
                }
            }
        }
        zn = mul(zn, z, wb + 4);
        if (n > 100000000) return approx_undefined();
    }
    return approx(sum, lsum(T.l2sum + std::log2(4.0 * static_cast<double>(N) + 8.0) - static_cast<double>(wb), tail));
}

Approx expansion_core(const CX& z, const CX& s, const CX& a, std::size_t w);

Approx expansion(const CX& z, const CX& s, const CX& a, std::size_t w) {
    const double rea = to_d(a.re);
    if (rea <= 1.0) return expansion_core(z, s, a, w);
    const std::int64_t m = static_cast<std::int64_t>(std::ceil(rea)) - 1;
    if (m > 100000) return approx_undefined();
    const std::size_t wb = w + 16;
    const CX a0 = shift(a, -m, wb);
    const Approx r = expansion_core(z, s, a0, wb);
    if (r.undefined) return r;
    CX   zj(BF::one());
    CX   sum;
    Term T;
    for (std::int64_t j = 0; j < m; ++j) {
        const CX t = mul(zj, cpow_neg(shift(a0, j, wb), s, wb + 4), wb);
        sum = add(sum, t, wb);
        T.add(l2(t));
        zj = mul(zj, z, wb + 4);
    }
    const CX d = sub(r.v, sum, wb);
    const CX v = div(d, zj, wb);
    const double lz = l2(zj);
    double e = lsum(lsum(r.er, r.ei) + 0.5, T.l2sum + std::log2(4.0 * static_cast<double>(m) + 8.0) - static_cast<double>(wb));
    e = lsum(e, l2(d) - static_cast<double>(wb) + 2.0);
    return approx(v, lsum(e - lz + 0.5, l2(v) - static_cast<double>(wb) + 2.0));
}

Approx expansion_core(const CX& z, const CX& s, const CX& a, std::size_t w) {
    const std::size_t wb = w + 20;
    const CX mu  = ln(z, wb + 8);
    const CX lmm = ln(neg(mu), wb + 8);
    std::int64_t m = 0;
    const bool sint = s.im.is_zero() && int64_of(s.re, m);
    CX     sum;
    Term   T;
    double err = ninf;
    if (!(sint && m >= 1)) {
        const CX one_s(BF::sub(BF::one(), s.re, wctx(wb + 64 + extra_bits(s))), s.im.with_sign(!s.im.signbit()));
        const CX g    = cx(gamma(bc(one_s), BCC(BFC::clamp_precision(wb + 8))));
        const CX sm1  = add_real(s, BF::one(true), wb + 64);
        const CX p    = exp(mul(sm1, lmm, wb + 8 + extra_bits(sm1)), wb + 8);
        sum = mul(g, p, wb);
        T.add(l2(sum));
    }
    const CX lead = sum;
    const double lead_l2 = T.l2max;
    const double ratio = l2(mu) - std::log2(2.0 * 3.141592653589793);
    const double res = to_d(s.re);
    const double ima = std::fabs(to_d(a.im));
    std::size_t K = static_cast<std::size_t>((static_cast<double>(wb) + 16.0 + std::fabs(res) * std::log2(static_cast<double>(wb) + 8.0) + ima * 9.07) / std::max(0.5, -ratio)) + static_cast<std::size_t>(std::max<std::int64_t>(m, 0)) + 8;
    for (;;) {
        std::vector<double> drop(K + 1);
        {
            const double lmu = l2(mu);
            std::vector<double> L(K + 1);
            double Lmax = lead_l2 == ninf ? -1e300 : lead_l2;
            double lf = 0.0;
            for (std::size_t k = 0; k <= K; ++k) {
                if (k > 0) lf += std::log2(static_cast<double>(k));
                const double x   = static_cast<double>(k) + 1.0 - res;
                const double est = x > 0.5 ? std::lgamma(x) * log2e - x * std::log2(2.0 * 3.141592653589793) : 0.0;
                L[k] = std::max(est, 0.0) + static_cast<double>(k) * lmu - lf;
                Lmax = std::max(Lmax, L[k]);
            }
            for (std::size_t k = 0; k <= K; ++k) drop[k] = std::max(0.0, Lmax - L[k] - 16.0 - ima * 9.07);
        }
        const std::vector<Approx> hz = hurwitz_shifted(s, a, K, wb, drop);
        sum  = lead;
        T    = Term();
        if (lead_l2 != ninf) T.add(lead_l2);
        err  = ninf;
        CX     mk(BF::one());
        BF     fact = BF::one();
        std::vector<double> lts;
        bool   ok   = false;
        for (std::size_t kk = 0; kk <= K; ++kk) {
            const std::int64_t k = static_cast<std::int64_t>(kk);
            CX term;
            if (sint && m >= 1 && k == m - 1) {
                const BFC hc = wctx(wb + 16);
                BF hsum = BF::zero();
                for (std::int64_t j = 1; j < m; ++j) hsum = BF::add(hsum, BF::div(BF::one(), BF(j), hc), hc);
                const BF psim = BF::sub(hsum, multiprecision::constants::euler_mascheroni(hc), hc);
                const CX psia = cx(digamma(bc(a), BCC(BFC::clamp_precision(wb + 8))));
                const CX c    = sub(sub(CX(psim), psia, wb + 8), lmm, wb + 8);
                term = div(mul(mk, c, wb), fact, wb);
            } else {
                if (hz[kk].undefined) return approx_undefined();
                const CX f = div(mk, fact, wb);
                term = mul(hz[kk].v, f, wb);
                err  = lsum(err, lsum(hz[kk].er, hz[kk].ei) + l2(f) + 1.0);
            }
            sum = add(sum, term, wb);
            const double lt = l2(term);
            T.add(lt);
            lts.push_back(lt);
            if (k >= std::max<std::int64_t>(m, 2) + 8) {
                const double slope = (lt - lts[kk - 8]) / 8.0;
                const double top   = std::max(lt, lts[kk - 1]);
                if (slope < -0.4) {
                    const double tb = top + 2.0 - std::log2(1.0 - std::exp2(slope));
                    if (tb < T.l2max - static_cast<double>(wb)) {
                        err = lsum(err, tb);
                        ok  = true;
                        break;
                    }
                }
            }
            mk   = mul(mk, mu, wb);
            fact = BF::mul(fact, BF(k + 1), wctx(wb + 8));
        }
        if (ok) break;
        if (K > 40 * wb + 1000) return approx_undefined();
        K *= 2;
    }
    const CX E = exp(neg(mul(a, mu, wb + 8 + extra_bits(a))), wb + 8);
    const CX v = mul(E, sum, wb);
    const double e = l2(E) + lsum(err, T.l2sum + std::log2(static_cast<double>(T.l2sum - T.l2max) + 64.0) - static_cast<double>(wb) + 3.0);
    return approx(v, lsum(e, l2(v) - static_cast<double>(wb) + 3.0));
}

using cd = std::complex<double>;

constexpr double two_pi_d = 6.283185307179586;

cd to_cd(const CX& z) { return cd(to_d(z.re), to_d(z.im)); }

double nome_distance(const cd& mu) {
    return std::min({std::abs(mu), std::abs(mu - cd(0.0, two_pi_d)), std::abs(mu + cd(0.0, two_pi_d))});
}

Approx watson(const CX& z, const CX& s, const CX& a, std::size_t w) {
    const std::size_t wb  = w + 24;
    const double      wbd = static_cast<double>(wb);
    const cd          zd  = to_cd(z);
    const cd          mu(0.6931471805599453 * l2(z), std::atan2(zd.imag(), zd.real()));
    const double      rho = nome_distance(mu);
    const double      sig = to_d(s.re);
    if (!(rho > 0.0) || !(sig > 0.0)) return approx_undefined();
    const double R  = 0.95 * rho;
    const double t0 = 0.85 * rho;
    double MR = 0.0;
    for (int k = 0; k < 512; ++k) {
        const cd t = std::polar(R, two_pi_d * static_cast<double>(k) / 512.0);
        MR = std::max(MR, 1.0 / std::abs(1.0 - zd * std::exp(-t)));
    }
    const double lMR = std::log2(MR) + 2.0;
    const double az  = std::min(1.0, std::abs(zd));
    const double lF2 = -std::log2(1.0 - az * std::exp(-t0));
    const CX     g   = cx(gamma(bc(s), BCC(BFC::clamp_precision(64))));
    const double lgs = l2(g);
    if (lgs == ninf || !std::isfinite(lgs)) return approx_undefined();
    const double target = l2(cpow_neg(a, s, 64)) - wbd - 4.0;
    const double rea    = to_d(a.re);
    auto region2 = [&](double Ar, double c, double lcoef) {
        const double den = Ar - std::max(0.0, c - 1.0) / t0;
        if (!(den > 0.0)) return pinf;
        return lcoef + (c - 1.0) * std::log2(t0) - Ar * t0 * log2e - std::log2(den) - lgs;
    };
    std::uint64_t N = static_cast<std::uint64_t>(std::max(0.0, std::ceil((wbd * 0.6931471805599453) / t0 - rea)));
    for (;;) {
        const double Ar = rea + static_cast<double>(N);
        const double lcs = lMR - std::log2(1.0 - t0 / R);
        if (Ar * t0 > sig + 0.5 * wbd + 8.0 && region2(Ar, sig, lsum(lF2, lcs)) < target - 12.0 && std::log2(R * Ar) > std::log2(0.4 * wbd + 8.0)) break;
        N += 1 + N / 16;
        if (N > 100000000) return approx_undefined();
    }
    CX   zn(BF::one());
    CX   sum;
    Term T;
    const bool unit_a = a.im.is_zero() && BF::compare(a.re, BF::one()) == BF::ordering::equal;
    PowTable table(s, wb + 8);
    for (std::uint64_t n = 0; n < N; ++n) {
        const CX t = mul(zn, unit_a ? table.get(n + 1) : cpow_neg(shift(a, static_cast<std::int64_t>(n), wb), s, wb + 4), wb);
        sum = add(sum, t, wb);
        T.add(l2(t));
        zn = mul(zn, z, wb + 8);
    }
    const CX A  = shift(a, static_cast<std::int64_t>(N), wb);
    const double Ar = to_d(A.re);
    const std::size_t wc = wb + 24;
    const CX omz(BF::sub(BF::one(), z.re, wctx(wc + 64)), z.im.with_sign(!z.im.signbit()));
    std::vector<CX> h;
    h.push_back(div(z, omz, wc));
    const CX iA = recip(A, wc);
    CX   P   = cpow_neg(A, s, wc);
    CX   tail = mul(add_real(h[0], BF::one(), wc), P, wc);
    Term U;
    U.add(l2(tail));
    const double lbase = -Ar * t0 * log2e - lgs;
    auto den_l2 = [&](double c) {
        const double den = Ar - std::max(0.0, c - 1.0) / t0;
        return den > 0.0 ? std::log2(den) : ninf;
    };
    double s2 = lsum(lF2 + (sig - 1.0) * std::log2(t0) - den_l2(sig), l2(add_real(h[0], BF::one(), 64)) + (sig - 1.0) * std::log2(t0) - den_l2(sig));
    double rem = pinf;
    std::size_t J = 1;
    for (;; ++J) {
        const double e1 = lMR - std::log2(1.0 - t0 / R) - static_cast<double>(J) * std::log2(R) + std::lgamma(sig + static_cast<double>(J)) * log2e - (sig + static_cast<double>(J)) * std::log2(Ar) - lgs;
        const double e2 = s2 + lbase;
        rem = lsum(e1, e2);
        if (rem < target) break;
        if (J > 4 * wb + 64) return approx_undefined();
        const std::size_t j = J - 1;
        CX conv;
        for (std::size_t i = 0; i <= j; ++i) conv = add(conv, mul(h[i], h[j - i], wc), wc);
        h.push_back(div(neg(add(h[j], conv, wc)), BF(static_cast<std::uint64_t>(j + 1)), wc));
        P = mul(mul(P, add_real(s, BF(static_cast<std::uint64_t>(j)), wc), wc), iA, wc);
        const CX term = mul(h[J], P, wc);
        tail = add(tail, term, wc);
        U.add(l2(term));
        const double cj = sig + static_cast<double>(J);
        const double dl = den_l2(cj);
        if (dl == ninf) return approx_undefined();
        s2 = lsum(s2, l2(h[J]) + (cj - 1.0) * std::log2(t0) - dl);
    }
    const CX tz = mul(tail, zn, wb);
    const CX v  = add(sum, tz, wb);
    double e = T.l2sum + std::log2(4.0 * static_cast<double>(N) + 8.0) - wbd;
    e = lsum(e, l2(zn) + lsum(rem, U.l2sum + 2.0 * std::log2(static_cast<double>(J) + 2.0) + 4.0 - static_cast<double>(wc)));
    return approx(v, lsum(e, l2(v) - wbd + 3.0));
}

enum class meth { direct, expansion, watson };

struct Choice {
    meth         m;
    std::int64_t q;
};

Choice choose(const CX& z, const CX& s, std::size_t w) {
    const double wbd = static_cast<double>(w) + 24.0;
    const double lr  = 0.6931471805599453 * l2(z);
    const cd     zd  = to_cd(z);
    const double th  = std::atan2(zd.imag(), zd.real());
    const double sig = to_d(s.re);
    Choice best{meth::expansion, 1};
    double cost = pinf;
    if (lr < -1e-3) {
        cost = wbd / (-lr * log2e);
        best = Choice{meth::direct, 1};
    }
    for (std::int64_t q = 1; q <= 8; ++q) {
        const double qd = static_cast<double>(q);
        const cd     mq(qd * lr, std::remainder(qd * th, two_pi_d));
        const double m  = std::abs(mq);
        if (m < 3.0 && m > 0.0) {
            const double K  = wbd / std::log2(two_pi_d / m);
            const double Nh = (0.76 * wbd + K) / two_pi_d * 1.15 + 8.0;
            const double c  = qd * (1.5 * Nh + (K * Nh + 1.5 * K * K) / 120.0 + 40.0);
            if (c < cost) {
                cost = c;
                best = Choice{meth::expansion, q};
            }
        }
        const double rho = nome_distance(mq);
        if (sig > 0.0 && rho > 0.05) {
            const double N = wbd * 0.6931471805599453 / (0.85 * rho) + 8.0;
            const double J = 0.3 * wbd;
            const double c = qd * (N + J * J / 400.0);
            if (c < cost) {
                cost = c;
                best = Choice{meth::watson, q};
            }
        }
    }
    return best;
}

Approx run(meth m, const CX& z, const CX& s, const CX& a, std::size_t w) {
    switch (m) {
        case meth::direct: return direct(z, s, a, w);
        case meth::watson: {
            const Approx r = watson(z, s, a, w);
            if (!r.undefined) return r;
            return std::fabs(to_d(z.re)) + std::fabs(to_d(z.im)) < 0.9 ? direct(z, s, a, w) : expansion(z, s, a, w);
        }
        default: return expansion(z, s, a, w);
    }
}

Approx lerch_core(const CX& z, const CX& s, const CX& a, std::size_t w) {
    const double rea = to_d(a.re);
    if (rea <= 0.5) {
        const std::int64_t m = static_cast<std::int64_t>(std::ceil(1.0 - rea));
        const std::size_t wb = w + 16;
        CX   zn(BF::one());
        CX   sum;
        Term T;
        for (std::int64_t n = 0; n < m; ++n) {
            const CX an = shift(a, n, wb);
            if (is_zero(an)) return approx_undefined();
            const CX t = mul(zn, cpow_neg(an, s, wb + 4), wb);
            sum = add(sum, t, wb);
            T.add(l2(t));
            zn = mul(zn, z, wb + 4);
        }
        const Approx r = lerch_core(z, s, shift(a, m, wb), w + 4);
        if (r.undefined) return r;
        const CX rv = mul(zn, r.v, wb);
        const CX v  = add(sum, rv, wb);
        const double lzn = l2(zn);
        Approx out;
        out.v  = v;
        const double base = lsum(T.l2sum + std::log2(4.0 * static_cast<double>(m) + 8.0) - static_cast<double>(wb), l2(rv) - static_cast<double>(wb) + 3.0);
        out.er = lsum(base, lsum(r.er, r.ei) + lzn + 0.5);
        out.ei = out.er;
        return out;
    }
    const Choice c = choose(z, s, w);
    if (c.q == 1) return run(c.m, z, s, a, w);
    const std::int64_t q = c.q;
    const std::size_t wb = w + 16;
    const std::size_t wp = wb + 64 + extra_bits(s);
    const CX zq = pow_int(z, q, wp);
    const BF qb(q);
    CX   zj(BF::one());
    CX   sum;
    double err = ninf;
    for (std::int64_t j = 0; j < q; ++j) {
        const CX aj = div(shift(a, j, wp), qb, wp);
        const Approx r = run(c.m, zq, s, aj, wb);
        if (r.undefined) return r;
        const CX t = mul(zj, r.v, wb);
        sum = add(sum, t, wb);
        err = lsum(err, lsum(lsum(r.er, r.ei) + l2(zj) + 0.5, l2(t) - static_cast<double>(wb) + 2.0));
        zj = mul(zj, z, wp);
    }
    const CX f = cpow_neg(CX(qb), s, wb + 4);
    const CX v = mul(f, sum, wb);
    return approx(v, lsum(l2(f) + err + 1.0, l2(v) - static_cast<double>(wb) + 3.0));
}

bool real_nonpos_int(const BC& a) {
    return a.imaginary().is_zero() && (a.real().is_zero() || (a.real().signbit() && a.real().is_integer()));
}

} // namespace

BC lerch_transcendent(const BC& z, const BC& s, const BC& a, const BCC& ctx) {
    if (z.is_undefined() || s.is_undefined() || a.is_undefined()) return BC::undefined();
    if (!z.is_finite() || !s.is_finite() || !a.is_finite()) return BC::undefined();
    if (real_nonpos_int(a)) return BC::undefined();
    const CX Z = cx(z), S = cx(s), A = cx(a);
    const BFC xc(BFC::max_prec, RoundingMode::nearest_even);
    const BF  nz = BF::add(BF::mul(Z.re, Z.re, xc), BF::mul(Z.im, Z.im, xc), xc);
    if (BF::compare(nz, BF::one()) == BF::ordering::greater) return BC::undefined();
    if (z.imaginary().is_zero() && s.imaginary().is_zero() && a.imaginary().is_zero() && !a.real().signbit()) {
        return BC(math::lerch_transcendent(z.real(), s.real(), a.real(), ctx.real()), BF::zero());
    }
    if (z.imaginary().is_zero() && BF::compare(z.real(), BF::one()) == BF::ordering::equal) return hurwitz_zeta(s, a, ctx);
    if (z.is_zero()) return drive(ctx, [&](std::size_t w) {
        const CX v = cpow_neg(A, S, w + 8);
        return approx(v, l2(v) - static_cast<double>(w) + 2.0);
    });
    return drive(ctx, [&](std::size_t w) { return lerch_core(Z, S, A, w); });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
