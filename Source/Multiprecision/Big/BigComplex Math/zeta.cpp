#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <cmath>
#include <vector>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double two_pi_l2 = 2.651496129472319;

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

BF shifted(const BF& x, std::uint64_t k, std::size_t w) {
    if (k == 0) return x;
    return BF::add(x, BF(k), wctx(w + 64 + extra_bits(x)));
}

CX cpow_neg(const CX& base, const CX& s, std::size_t w) {
    if (base.im.is_zero() && !base.re.signbit()) return real_pow_neg(base.re, s, w + 4);
    const double lb = std::fabs(l2(base)) * 0.6931471805599453 + 3.2;
    const std::size_t wq = w + 8 + static_cast<std::size_t>(std::max(0.0, std::ceil(l2(s) + std::log2(lb + 1.0) + 1.0)));
    const CX L = ln(base, wq);
    return exp(neg(mul(s, L, wq)), w + 8);
}

bool exact_one(const CX& z) { return z.im.is_zero() && BF::compare(z.re, BF::one()) == BF::ordering::equal; }

} // namespace

Approx hurwitz_core(const CX& s, const CX& a, std::size_t w) {
    const double ls   = l2(s);
    const double abss = ls == ninf ? 0.0 : std::exp2(std::min(ls, 60.0));
    const double res  = to_d(s.re);
    const double rea  = to_d(a.re);
    double X = (1.1 * 0.6931471805599453 * static_cast<double>(w) + abss) / (2.0 * 3.141592653589793) * 1.15 + 8.0;
    const bool unit_a = exact_one(a);
    PowTable table(s, w + 24);
    for (;;) {
        const std::uint64_t N = rea >= X ? 0 : static_cast<std::uint64_t>(std::ceil(X - rea));
        const std::size_t   wb = w + 8 + static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(N) + 2.0)));
        CX   S1;
        Term T;
        for (std::uint64_t k = 0; k < N; ++k) {
            const CX ak(shifted(a.re, k, wb), a.im);
            if (is_zero(ak)) return approx_undefined();
            const CX t = exact_one(ak) ? CX(BF::one()) : (unit_a ? table.get(k + 1) : cpow_neg(ak, s, wb));
            S1 = add(S1, t, wb);
            T.add(l2(t));
        }
        const CX A(shifted(a.re, N, wb), a.im);
        const CX q  = unit_a ? table.get(N + 1) : cpow_neg(A, s, wb);
        const CX u  = recip(A, wb);
        const CX u2 = sqr(u, wb);
        const double la = l2(A);
        const CX sm1 = add_real(s, BF::one(true), wb);
        if (is_zero(sm1)) return approx_undefined();
        CX tail = add(div(mul(q, A, wb), sm1, wb), scale2(q, -1), wb);
        T.add(l2(tail));
        CX   poch = s;
        CX   qu   = mul(q, u, wb);
        BF     fact  = BF::one();
        double prev  = pinf;
        double trunc = ninf;
        bool   retry = false;
        const double target = std::max(T.l2max, l2(tail)) - static_cast<double>(w) - 3.0;
        for (std::size_t j = 1;; ++j) {
            const double lb  = 1.0 - static_cast<double>(2 * j) * two_pi_l2;
            const double lt  = lb + l2(poch) + l2(qu);
            const double rs2 = res + static_cast<double>(2 * j - 1);
            if (rs2 > 0.5) {
                const double fac   = l2(add_real(s, BF(static_cast<std::uint64_t>(2 * j - 1)), 64)) - std::log2(rs2);
                const double bound = lt + std::max(0.0, fac) + 1.0;
                if (bound < target) {
                    trunc = bound;
                    break;
                }
                if (j > 3 && bound > prev) {
                    retry = true;
                    break;
                }
                prev = bound;
            }
            if (j > 4 * w + 100) {
                retry = true;
                break;
            }
            fact = BF::mul(fact, BF(static_cast<std::uint64_t>((2 * j - 1) * (2 * j))), wctx(wb + 16));
            const BF   bj = BF::div(multiprecision::constants::bernoulli(2 * j, wctx(wb + 8)), fact, wctx(wb));
            const CX   term = mul(mul(poch, qu, wb), bj, wb);
            tail = add(tail, term, wb);
            T.add(l2(term));
            poch = mul(mul(poch, add_real(s, BF(static_cast<std::uint64_t>(2 * j - 1)), wb), wb), add_real(s, BF(static_cast<std::uint64_t>(2 * j)), wb), wb);
            qu   = mul(qu, u2, wb);
        }
        if (retry) {
            X = X * 2.0 + 16.0;
            continue;
        }
        (void)la;
        const CX v = add(S1, tail, wb);
        const double e = lsum(T.l2sum - static_cast<double>(w) + 3.0 + std::log2(static_cast<double>(N) + 8.0), trunc);
        return approx(v, e);
    }
}

namespace {

bool is_one_bc(const BC& z) { return z.imaginary().is_zero() && BF::compare(z.real(), BF::one()) == BF::ordering::equal; }
bool real_nonpos_int(const BC& z) { return z.imaginary().is_zero() && z.real().is_integer() && (z.real().signbit() || z.real().is_zero()); }
bool below_half(const BC& z) { return BF::compare(z.real(), BF::one().scaled_pow2(-1)) == BF::ordering::less; }

CX one_minus(const CX& z, std::size_t w) { return CX(BF::sub(BF::one(), z.re, wctx(w + 64 + extra_bits(z.re))), z.im.with_sign(!z.im.signbit())); }

CX exp_lin(const CX& s, const BF& c1, const BF& c0, std::size_t w) {
    const std::size_t wu = w + extra_bits(s) + 8;
    const CX u = add_real(mul(s, c1, wu), c0, wu);
    return exp(u, w + 8);
}

Approx chi_low(const CX& s, std::size_t w) {
    const std::size_t wi = w + 8;
    const BFC c   = wctx(wi + extra_bits(s) + 8);
    const BF  l2c = multiprecision::constants::ln2(c);
    const BF  lpi = math::ln(multiprecision::constants::pi(c), c);
    const CX  p   = exp_lin(s, BF::add(l2c, lpi, c), lpi.with_sign(true), wi);
    const CV  sn  = sinpi(scale2(s, -1), wi);
    const CX  g   = cx(gamma(bc(one_minus(s, wi)), BCC(BFC::clamp_precision(wi))));
    const CX  v   = mul(mul(p, sn.v, wi), g, wi);
    return approx(v, l2(v) - static_cast<double>(w) + 1.0);
}

struct BernCache {
    std::vector<BF> b;
    std::size_t     w = 0;
    const BF& get(std::size_t j, std::size_t wb) {
        if (wb > w) {
            b.clear();
            w = wb;
        }
        while (b.size() <= j) {
            const std::size_t i = b.size();
            if (i == 0) {
                b.push_back(BF::zero());
                continue;
            }
            BF f = BF::one();
            for (std::uint64_t t = 2; t <= 2 * i; ++t) f = BF::mul(f, BF(t), wctx(w + 16));
            b.push_back(BF::div(multiprecision::constants::bernoulli(2 * i, wctx(w + 8)), f, wctx(w)));
        }
        return b[j];
    }
};

bool em_tail(const CX& sig, const CX& cN, const CX& Nb, std::size_t wb, double target, BernCache& bern, CX& tail, double& trunc, Term& T) {
    const CX sm1 = add_real(sig, BF::one(true), wb);
    if (is_zero(sm1)) return false;
    tail = add(div(mul(cN, Nb, wb), sm1, wb), scale2(cN, -1), wb);
    T.add(l2(tail));
    const double res = to_d(sig.re);
    const CX iN  = recip(Nb, wb);
    const CX iN2 = sqr(iN, wb);
    CX     poch = sig;
    CX     qu   = mul(cN, iN, wb);
    double prev = pinf;
    for (std::size_t j = 1;; ++j) {
        const double lb  = 1.0 - static_cast<double>(2 * j) * two_pi_l2;
        const double lt  = lb + l2(poch) + l2(qu);
        const double rs2 = res + static_cast<double>(2 * j - 1);
        if (rs2 > 0.5) {
            const double fac   = l2(add_real(sig, BF(static_cast<std::uint64_t>(2 * j - 1)), 64)) - std::log2(rs2);
            const double bound = lt + std::max(0.0, fac) + 1.0;
            if (bound < target) {
                trunc = bound;
                return true;
            }
            if (j > 3 && bound > prev) return false;
            prev = bound;
        }
        if (j > 100000) return false;
        const CX term = mul(mul(poch, qu, wb), bern.get(j, wb), wb);
        tail = add(tail, term, wb);
        T.add(l2(term));
        poch = mul(mul(poch, add_real(sig, BF(static_cast<std::uint64_t>(2 * j - 1)), wb), wb), add_real(sig, BF(static_cast<std::uint64_t>(2 * j)), wb), wb);
        qu   = mul(qu, iN2, wb);
    }
}

} // namespace

std::vector<Approx> zeta_shifted(const CX& s, std::size_t K, std::size_t w) {
    std::vector<Approx> out(K + 1);
    const double abss = std::exp2(std::min(l2(s), 60.0));
    const double res  = to_d(s.re);
    const std::size_t wb = w + 24 + static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(K) + 2.0)));
    const std::uint64_t N = static_cast<std::uint64_t>(std::ceil((1.1 * 0.6931471805599453 * static_cast<double>(wb) + abss) / (2.0 * 3.141592653589793) * 1.15 + 8.0));
    const BF Nb(N);
    PowTable P(s, wb + 8);
    std::vector<CX> B(N + 1);
    for (std::uint64_t n = 1; n <= N; ++n) B[n] = recip(mul(P.get(n), BF(n), wb + 8), wb + 8);
    std::vector<BF> pw(N + 1, BF::one());
    std::vector<BF> inv(N + 1);
    for (std::uint64_t n = 1; n <= N; ++n) inv[n] = BF::div(BF::one(), BF(n), wctx(wb + 8));
    BernCache bern;
    bool fe_started = false;
    std::size_t k0 = 0;
    CX E, G;
    CV S0, C0;
    for (std::size_t k = 0; k <= K; ++k) {
        const bool direct = res - static_cast<double>(k) >= 0.5;
        const std::size_t wk = wb + 64 + extra_bits(s);
        const CX sig = direct ? add_real(s, BF(-static_cast<std::int64_t>(k)), wk)
                              : CX(BF::add(BF::sub(BF::one(), s.re, wctx(wk)), BF(static_cast<std::uint64_t>(k)), wctx(wk)), s.im.with_sign(!s.im.signbit()));
        if (!direct && !fe_started) {
            fe_started = true;
            k0 = k;
            for (std::uint64_t n = 2; n <= N; ++n) pw[n] = math::pow(inv[n], BigInt(static_cast<long long>(k)), wctx(wb + 8));
            pw[1] = BF::one();
            const CX u = add_real(s, BF(-static_cast<std::int64_t>(k)), wk);
            const BFC c = wctx(wb + 8 + extra_bits(u) + 8);
            const BF  l2c = multiprecision::constants::ln2(c);
            const BF  lpi = math::ln(multiprecision::constants::pi(c), c);
            E  = exp(add_real(mul(u, BF::add(l2c, lpi, c), wb + 8 + extra_bits(u)), lpi.with_sign(true), wb + 8 + extra_bits(u)), wb + 8);
            S0 = sinpi(scale2(u, -1), wb + 8);
            C0 = cospi(scale2(u, -1), wb + 8);
            G  = cx(gamma(bc(CX(BF::sub(BF::one(), u.re, wctx(wk)), u.im.with_sign(!u.im.signbit()))), BCC(BFC::clamp_precision(wb + 8))));
        } else if (!direct) {
            for (std::uint64_t n = 2; n <= N; ++n) pw[n] = BF::mul(pw[n], inv[n], wctx(wb + 8));
            E = div(E, BF::mul(multiprecision::constants::pi(wctx(wb + 16)), BF(static_cast<std::int64_t>(2)), wctx(wb + 16)), wb + 8);
            G = mul(G, CX(BF::sub(BF(static_cast<std::uint64_t>(k)), s.re, wctx(wk)), s.im.with_sign(!s.im.signbit())), wb + 8);
        } else if (k > 0) {
            for (std::uint64_t n = 2; n <= N; ++n) pw[n] = BF::mul(pw[n], BF(n), wctx(wb + 8));
        }
        CX   sum;
        Term T;
        const double rsig = to_d(sig.re);
        auto dirichlet_tail = [&](std::uint64_t m) {
            const double md = static_cast<double>(m);
            return rsig > 1.0 ? -rsig * std::log2(md) + std::log2(1.0 + md / (rsig - 1.0)) : pinf;
        };
        double trunc = ninf;
        bool   done  = false;
        for (std::uint64_t n = 1; n < N; ++n) {
            const CX c = mul(direct ? P.get(n) : B[n], pw[n], wb);
            sum = add(sum, c, wb);
            T.add(l2(c));
            if (!direct) {
                const double tb = dirichlet_tail(n + 1);
                if (tb < T.l2max - static_cast<double>(wb) - 4.0) {
                    trunc = tb;
                    done  = true;
                    break;
                }
            }
        }
        const double target = T.l2max - static_cast<double>(wb) - 4.0;
        CX tail;
        const double dt = dirichlet_tail(N);
        if (!done && dt < target) trunc = dt;
        if (!done && dt >= target) {
            const CX cN = mul(direct ? P.get(N) : B[N], pw[N], wb);
            if (!em_tail(sig, cN, CX(Nb), wb, target, bern, tail, trunc, T)) {
                out[k] = approx_undefined();
                continue;
            }
        }
        const CX z = add(sum, tail, wb);
        double e = lsum(T.l2sum - static_cast<double>(wb) + 3.0 + std::log2(static_cast<double>(N) + 8.0), trunc);
        if (direct) {
            out[k] = approx(z, e);
            continue;
        }
        const std::size_t j = k - k0;
        CV tr = (j % 2 == 0) ? S0 : C0;
        if (j % 4 == 1 || j % 4 == 2) tr.v = neg(tr.v);
        const CX chi = mul(mul(E, tr.v, wb), G, wb);
        const CX v = mul(chi, z, wb);
        const double lchi = l2(chi);
        e = lsum(e + lchi, l2(v) - static_cast<double>(wb) + std::log2(static_cast<double>(j) + 32.0) + 2.0);
        out[k] = approx(v, e);
    }
    return out;
}

std::vector<Approx> hurwitz_shifted(const CX& s, const CX& a, std::size_t K, std::size_t w, const std::vector<double>& drop) {
    std::vector<Approx> out(K + 1);
    const double abss = std::exp2(std::min(l2(s), 60.0));
    const double res  = to_d(s.re);
    const double ims  = std::fabs(to_d(s.im));
    const double absa = std::exp2(std::min(l2(a), 60.0));
    std::vector<double> need(K + 1);
    for (std::size_t k = 0; k <= K; ++k) {
        const double d = k < drop.size() ? std::max(0.0, drop[k]) : 0.0;
        need[k] = std::max(40.0, static_cast<double>(w) + 24.0 - d);
    }
    std::vector<double> prec(K + 1);
    std::uint64_t N = 8;
    for (int it = 0; it < 3; ++it) {
        double nn = 8.0;
        for (std::size_t k = 0; k <= K; ++k) {
            const double pk = it == 0 ? need[k] : prec[k];
            nn = std::max(nn, (1.1 * 0.6931471805599453 * pk + abss + static_cast<double>(k)) / (2.0 * 3.141592653589793) * 1.15 + 8.0);
        }
        N = static_cast<std::uint64_t>(std::ceil(nn));
        for (std::size_t k = 0; k <= K; ++k) {
            const double kk  = static_cast<double>(k);
            const double big = (kk - res) * std::log2(static_cast<double>(N) + absa + 1.0) + std::log2(static_cast<double>(N) + 1.0) + ims * 2.2662;
            const double x   = kk + 1.0 - res;
            const double est = x > 0.5 ? std::lgamma(x) / 0.6931471805599453 - x * std::log2(2.0 * 3.141592653589793) : 0.0;
            prec[k] = need[k] + std::max(0.0, big - est);
        }
    }
    for (std::size_t k = K; k-- > 0;) prec[k] = std::max(prec[k], prec[k + 1]);
    const std::size_t extra = static_cast<std::size_t>(std::ceil(std::log2(static_cast<double>(K + N) + 2.0)));
    auto wk = [&](std::size_t k) { return static_cast<std::size_t>(prec[k]) + extra; };
    const std::size_t wb0 = wk(0);
    const std::size_t wa  = wb0 + 64 + extra_bits(a);
    std::vector<CX> base(N + 1), C(N + 1);
    for (std::uint64_t n = 0; n <= N; ++n) {
        base[n] = CX(shifted(a.re, n, wa), a.im);
        if (is_zero(base[n])) {
            for (auto& o : out) o = approx_undefined();
            return out;
        }
        C[n] = cpow_neg(base[n], s, wb0);
    }
    BernCache bern;
    for (std::size_t k = 0; k <= K; ++k) {
        const std::size_t wb = wk(k);
        if (k > 0) {
            for (std::uint64_t n = 0; n <= N; ++n) C[n] = mul(C[n], base[n], wb);
        }
        const CX sig = add_real(s, BF(-static_cast<std::int64_t>(k)), wb + 64 + extra_bits(s));
        CX   sum;
        Term T;
        for (std::uint64_t n = 0; n < N; ++n) {
            sum = add(sum, C[n], wb);
            T.add(l2(C[n]));
        }
        const double target = T.l2max - static_cast<double>(wb) - 4.0;
        CX     tail;
        double trunc = ninf;
        if (!em_tail(sig, C[N], base[N], wb, target, bern, tail, trunc, T)) {
            out[k] = approx_undefined();
            continue;
        }
        const CX z = add(sum, tail, wb);
        out[k] = approx(z, lsum(T.l2sum - static_cast<double>(wb) + 3.0 + std::log2(static_cast<double>(N) + 8.0) + std::log2(static_cast<double>(k) + 2.0), trunc));
    }
    return out;
}

BC hurwitz_zeta(const BC& s, const BC& a, const BCC& ctx) {
    if (!(s.is_undefined() || a.is_undefined()) && (s.is_indeterminate() || a.is_indeterminate())) return BC::indeterminate();
    if (s.is_undefined() || a.is_undefined() || !s.is_finite() || !a.is_finite()) return BC::undefined();
    if (is_one_bc(s) || real_nonpos_int(a)) return BC::undefined();
    if (s.imaginary().is_zero() && a.imaginary().is_zero() && !a.real().signbit()) {
        return BC(math::hurwitz_zeta(s.real(), a.real(), ctx.real()), BF::zero());
    }
    const CX S = cx(s), A = cx(a);
    return drive(ctx, [&](std::size_t w) { return hurwitz_core(S, A, w); });
}

BC riemann_zeta(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::riemann_zeta(s.real(), ctx.real()), BF::zero());
    const CX S = cx(s);
    if (!below_half(s)) return drive(ctx, [&](std::size_t w) { return hurwitz_core(S, CX(BF::one()), w); });
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const Approx ch = chi_low(S, wi);
        const Approx z  = hurwitz_core(one_minus(S, wi), CX(BF::one()), wi);
        const CX v = mul(ch.v, z.v, wi);
        const double rel = lsum(ch.er - l2(ch.v), z.er - l2(z.v));
        return approx(v, l2(v) + lsum(rel, -static_cast<double>(wi)) + 1.0);
    });
}

BC riemann_chi(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::riemann_chi(s.real(), ctx.real()), BF::zero());
    const CX S = cx(s);
    const bool low = below_half(s) || BF::compare(s.real(), BF::one().scaled_pow2(-1)) == BF::ordering::equal;
    return drive(ctx, [&](std::size_t w) {
        if (low) return chi_low(S, w);
        const Approx c = chi_low(one_minus(S, w + 8), w + 8);
        const CX v = recip(c.v, w + 8);
        return approx(v, l2(v) + lsum(c.er - l2(c.v), -static_cast<double>(w + 8)) + 1.0);
    });
}

BC riemann_xi(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::riemann_xi(s.real(), ctx.real()), BF::zero());
    const CX S0 = cx(s);
    const bool critical = BF::compare(s.real(), BF::one().scaled_pow2(-1)) == BF::ordering::equal;
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 12;
        const CX S = below_half(s) ? one_minus(S0, wi) : S0;
        const Approx z = hurwitz_core(S, CX(BF::one()), wi);
        const BFC c = wctx(wi + extra_bits(S) + 8);
        const BF  lpi = math::ln(multiprecision::constants::pi(c), c);
        const CX  p = exp_lin(S, lpi.with_sign(true).scaled_pow2(-1), BF::zero(), wi);
        const CX  g = cx(gamma(bc(scale2(S, -1)), BCC(BFC::clamp_precision(wi))));
        const CX  ss = mul(S, add_real(S, BF::one(true), wi), wi);
        const CX  v = scale2(mul(mul(mul(ss, p, wi), g, wi), z.v, wi), -1);
        Approx r = approx(v, l2(v) + lsum(z.er - l2(z.v), -static_cast<double>(wi)) + 2.0);
        if (critical) {
            r.v.im = BF::zero();
            r.ei   = ninf;
        }
        return r;
    });
}

namespace {

BC zeta_times(const BC& s, bool eta, const BCC& ctx) {
    const CX S = cx(s);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const CX z = cx(riemann_zeta(s, BCC(BFC::clamp_precision(wi))));
        const BFC c = wctx(wi + extra_bits(S) + 8);
        const BF  ln2 = multiprecision::constants::ln2(c);
        const CX  p = eta ? exp_lin(S, ln2.with_sign(true), ln2, wi) : exp_lin(S, ln2.with_sign(true), BF::zero(), wi);
        const CX  f = sub(CX(BF::one()), p, wi);
        const CX  v = mul(f, z, wi);
        const double e = lsum(lsum(l2(f) + l2(z) - static_cast<double>(wi), l2(z) + l2(p) + std::log2(l2(S) > 0 ? std::exp2(l2(S)) + 1.0 : 2.0) - static_cast<double>(wi)), l2(v) - static_cast<double>(wi)) + 2.0;
        return approx(v, e);
    });
}

} // namespace

BC dirichlet_eta(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::dirichlet_eta(s.real(), ctx.real()), BF::zero());
    return zeta_times(s, true, ctx);
}

BC dirichlet_lambda(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::dirichlet_lambda(s.real(), ctx.real()), BF::zero());
    return zeta_times(s, false, ctx);
}

BC dirichlet_beta(const BC& s, const BCC& ctx) {
    if (s.is_indeterminate()) return BC::indeterminate();
    if (s.is_undefined() || !s.is_finite()) return BC::undefined();
    if (s.imaginary().is_zero()) return BC(math::dirichlet_beta(s.real(), ctx.real()), BF::zero());
    const CX S0 = cx(s);
    const bool low = below_half(s);
    auto core = [](const CX& S, std::size_t w) {
        const std::size_t wi = w + 8;
        const Approx z1 = hurwitz_core(S, CX(BF::one().scaled_pow2(-2)), wi);
        const Approx z3 = hurwitz_core(S, CX(BF(std::uint64_t(3)).scaled_pow2(-2)), wi);
        const BFC c = wctx(wi + extra_bits(S) + 8);
        const BF  ln4 = multiprecision::constants::ln2(c).scaled_pow2(1);
        const CX  p = exp_lin(S, ln4.with_sign(true), BF::zero(), wi);
        const CX  d = sub(z1.v, z3.v, wi);
        const CX  v = mul(p, d, wi);
        const double e = l2(p) + lsum(lsum(z1.er, z3.er), lsum(l2(z1.v), l2(z3.v)) - static_cast<double>(wi)) + 2.0;
        return approx(v, lsum(e, l2(v) - static_cast<double>(wi) + 1.0));
    };
    return drive(ctx, [&](std::size_t w) {
        if (!low) return core(S0, w);
        const std::size_t wi = w + 8;
        const CX T = one_minus(S0, wi);
        const Approx b = core(T, wi);
        const BFC c = wctx(wi + extra_bits(S0) + 8);
        const BF  lh = math::ln(multiprecision::constants::half_pi(c), c);
        const CX  p = exp_lin(S0, lh, lh.with_sign(true), wi);
        const CV  cs = cospi(scale2(S0, -1), wi);
        const CX  g = cx(gamma(bc(T), BCC(BFC::clamp_precision(wi))));
        const CX  v = mul(mul(mul(p, cs.v, wi), g, wi), b.v, wi);
        return approx(v, l2(v) + lsum(b.er - l2(b.v), -static_cast<double>(wi)) + 2.0);
    });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
