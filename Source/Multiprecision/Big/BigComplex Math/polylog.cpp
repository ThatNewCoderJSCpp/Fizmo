#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <cmath>
#include <vector>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double pi_d = 3.141592653589793;

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 1000.0)) * (x.signbit() ? -1.0 : 1.0);
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

Approx force_real(Approx a) {
    a.v.im = BF::zero();
    a.ei   = ninf;
    return a;
}

CX exact_one_minus(const CX& z) {
    const std::int64_t hi = std::max<std::int64_t>(1, z.re.is_zero() ? 1 : z.re.get_exp_base2());
    const std::int64_t lo = z.re.is_zero() ? 0 : std::min<std::int64_t>(0, z.re.exponent());
    const BF re = BF::sub(BF::one(), z.re, wctx(static_cast<std::size_t>(hi - lo + 4)));
    return CX(re, z.im.with_sign(!z.im.signbit()));
}

Approx series(const CX& s, const CX& z, std::size_t w, bool s_int, std::int64_t n, bool odd = false) {
    const double lz  = l2(z);
    const double res = to_d(s.re);
    const std::size_t wb = w + 10;
    const std::uint64_t step = odd ? 2 : 1;
    const CX zstep = odd ? sqr(z, wb + 8) : z;
    CX   zk = z;
    CX   sum;
    Term T;
    double tail = ninf;
    PowTable table(s, wb + 8);
    for (std::uint64_t k = 1;; k += step) {
        const double lk = std::log2(static_cast<double>(k));
        if (k > 2) {
            const double growth = res < 0 ? -res * std::log2(static_cast<double>(k + step) / static_cast<double>(k)) : 0.0;
            const double ratio  = static_cast<double>(step) * lz + growth;
            if (ratio < -0.05) {
                const double next = static_cast<double>(k) * lz - res * lk;
                const double bound = next - std::log2(1.0 - std::exp2(ratio));
                if (bound < T.l2max - static_cast<double>(w) - 4.0) {
                    tail = bound;
                    break;
                }
            }
        }
        CX term;
        if (s_int) {
            const BF kn = math::pow(BF(k), BigInt(static_cast<long long>(n)), wctx(wb + 8));
            term = div(zk, kn, wb);
        } else {
            term = mul(zk, table.get(k), wb);
        }
        sum = add(sum, term, wb);
        T.add(l2(term));
        zk = mul(zk, zstep, wb);
        if (k > 1000000) break;
    }
    return approx(sum, lsum(T.l2sum - static_cast<double>(w) + std::log2(T.l2sum - T.l2max + 8.0) + 2.0, tail));
}

Approx mu_expansion(const CX& s, const CX& z, std::size_t w) {
    const std::size_t wb = w + 12;
    const CX u  = ln(z, wb + 8);
    const double lu = l2(u);
    const CX one_s = exact_one_minus(s);
    const CX g  = cx(gamma(bc(one_s), BCC(BFC::clamp_precision(wb))));
    const CX mu = neg(u);
    const CX lead = mul(g, exp(mul(add_real(s, BF::one(true), wb), ln(mu, wb + 8), wb + 8 + extra_bits(s)), wb), wb);
    const double ratio = lu - std::log2(2.0 * pi_d);
    if (!(ratio < -0.1)) return approx_undefined();
    const double res = to_d(s.re);
    std::size_t K = static_cast<std::size_t>((static_cast<double>(w) + 24.0 + std::fabs(res) * std::log2(static_cast<double>(w) + 8.0)) / -ratio) + 8;
    for (;;) {
        const std::vector<Approx> zs = zeta_shifted(s, K, wb);
        CX     sum = lead;
        Term   T;
        T.add(l2(lead));
        CX     uk(BF::one());
        BF     fact = BF::one();
        double err  = ninf;
        double tail = ninf;
        double prev_lt = ninf;
        bool   ok   = false;
        for (std::size_t k = 0; k <= K; ++k) {
            if (zs[k].undefined) return approx_undefined();
            const CX f    = div(uk, fact, wb);
            const CX term = mul(zs[k].v, f, wb);
            sum = add(sum, term, wb);
            T.add(l2(term));
            err = lsum(err, lsum(zs[k].er, zs[k].ei) + l2(f) + 0.5);
            const double lt = l2(term);
            const double top = std::max(lt, prev_lt);
            prev_lt = lt;
            if (k > 4) {
                const double bound = top + ratio + 2.0 - std::log2(1.0 - std::exp2(ratio + 0.5));
                if (bound < T.l2max - static_cast<double>(w) - 4.0) {
                    tail = bound;
                    ok   = true;
                    break;
                }
            }
            uk   = mul(uk, u, wb);
            fact = BF::mul(fact, BF(static_cast<std::uint64_t>(k + 1)), wctx(wb + 8));
        }
        if (ok) return approx(sum, lsum(lsum(T.l2sum - static_cast<double>(w) + 4.0, tail), err));
        K *= 2;
    }
}

Approx general(const CX& s, const CX& z, std::size_t w) {
    const std::size_t wb = w + 12;
    const CX v   = exact_one_minus(s);
    const CX lmz = ln(neg(z), wb + 8);
    const BFC c  = wctx(wb + 8);
    const BF two_pi = multiprecision::constants::pi(c).scaled_pow2(1);
    const CX y   = div(div_i(lmz), two_pi, wb + 8);
    const BF half = BF::one().scaled_pow2(-1);
    const Approx h1 = hurwitz_core(v, add_real(y, half, wb + 8), wb);
    const Approx h2 = hurwitz_core(v, add_real(neg(y), half, wb + 8), wb);
    const BF hp = multiprecision::constants::half_pi(c);
    const CX ip  = exp(mul_i(mul(v, hp, wb + 8 + extra_bits(v))), wb);
    const CX im  = exp(neg(mul_i(mul(v, hp, wb + 8 + extra_bits(v)))), wb);
    const CX g   = cx(gamma(bc(v), BCC(BFC::clamp_precision(wb))));
    const CX tp  = exp(neg(mul(v, math::ln(two_pi, wctx(wb + 8 + extra_bits(v))), wb + 8 + extra_bits(v))), wb);
    const CX a1  = mul(ip, h1.v, wb);
    const CX a2  = mul(im, h2.v, wb);
    const CX br  = add(a1, a2, wb);
    const CX r   = mul(mul(g, tp, wb), br, wb);
    const double scale = l2(g) + l2(tp);
    const double e = scale + lsum(lsum(h1.er + l2(ip), h2.er + l2(im)), lsum(l2(a1), l2(a2)) - static_cast<double>(wb) + 3.0);
    return approx(r, lsum(e, l2(r) - static_cast<double>(wb) + 2.0));
}

CX bernpoly(unsigned n, const CX& x, std::size_t w) {
    CX acc;
    BF binom = BF::one();
    for (unsigned k = 0; k <= n; ++k) {
        const BF bk = multiprecision::constants::bernoulli(k, wctx(w));
        if (!bk.is_zero()) acc = add(acc, mul(pow_int(x, static_cast<std::int64_t>(n - k), w), BF::mul(binom, bk, wctx(w)), w), w);
        binom = BF::div(BF::mul(binom, BF(static_cast<std::uint64_t>(n - k)), wctx(w + 8)), BF(static_cast<std::uint64_t>(k + 1)), wctx(w + 8));
    }
    return acc;
}

Approx continuation(std::int64_t n, const CX& z, std::size_t w) {
    const std::size_t wb = w + 16 + static_cast<std::size_t>(n);
    const BFC c = wctx(wb);
    const CX tpi = CX(BF::zero(), multiprecision::constants::pi(c).scaled_pow2(1));
    const CX lz  = ln(z, wb);
    const CX x   = div(lz, tpi, wb);
    const CX bp  = bernpoly(static_cast<unsigned>(n), x, wb);
    const BF nf  = math::factorial(BF(static_cast<std::uint64_t>(n)), c);
    CX a = neg(div(mul(pow_int(tpi, n, wb), bp, wb), nf, wb));
    if (z.im.signbit() && !z.im.is_zero()) {
        const BF nf1 = math::factorial(BF(static_cast<std::uint64_t>(n - 1)), c);
        a = sub(a, div(mul(tpi, pow_int(lz, n - 1, wb), wb), nf1, wb), wb);
    } else if (z.im.is_zero() && !z.re.signbit() && BF::compare(z.re, BF::one()) != BF::ordering::less) {
        const BF nf1 = math::factorial(BF(static_cast<std::uint64_t>(n - 1)), c);
        a = sub(a, div(mul(tpi, pow_int(lz, n - 1, wb), wb), nf1, wb), wb);
    }
    const double scale = l2(pow_int(tpi, n, 64)) + std::max(0.0, l2(bp)) - l2(nf) + std::max(0.0, static_cast<double>(n) * std::max(0.0, l2(x)));
    return approx(a, lsum(scale, l2(a)) - static_cast<double>(w) - 6.0);
}

Approx unitcircle(std::int64_t n, const CX& z, std::size_t w) {
    const std::size_t wb = w + 16;
    const BFC c  = wctx(wb);
    const CX lz  = ln(z, wb);
    const double lu = l2(lz);
    const double ratio = lu - std::log2(2.0 * pi_d);
    CX   sum;
    Term T;
    CX   lm(BF::one());
    BF   fact = BF::one();
    double tail = ninf;
    for (std::int64_t m = 0;; ++m) {
        if (m != n - 1) {
            const BF zv = math::riemann_zeta(BF(n - m), c);
            const CX term = div(mul(lm, zv, wb), fact, wb);
            if (!zv.is_zero()) {
                sum = add(sum, term, wb);
                T.add(l2(term));
                if (m > n + 2 && ratio < -0.1) {
                    const double bound = l2(term) + ratio + 1.0 - std::log2(1.0 - std::exp2(ratio + 0.5));
                    if (bound < T.l2max - static_cast<double>(w) - 4.0) {
                        tail = bound;
                        break;
                    }
                }
            }
        }
        lm   = mul(lm, lz, wb);
        fact = BF::mul(fact, BF(static_cast<std::uint64_t>(m + 1)), wctx(wb + 8));
        if (m > 20 * static_cast<std::int64_t>(w) + 100) break;
    }
    const BF hn = n > 1 ? math::harmonic(BF(static_cast<std::uint64_t>(n - 1)), c) : BF::zero();
    const BF nf = math::factorial(BF(static_cast<std::uint64_t>(n - 1)), c);
    const CX llz = ln(neg(lz), wb);
    const CX extra = div(mul(pow_int(lz, n - 1, wb), sub(CX(hn), llz, wb), wb), nf, wb);
    sum = add(sum, extra, wb);
    T.add(l2(extra));
    return approx(sum, lsum(T.l2sum - static_cast<double>(w) + 4.0, tail));
}

Approx negative_int(std::uint64_t m, const CX& z, std::size_t w) {
    const std::size_t wb = w + 16 + static_cast<std::size_t>(m);
    const CX omz = exact_one_minus(z);
    CV num = cv_exact(CX());
    CV zk  = cv_exact(z);
    if (m == 0) {
        const CX v = div(z, omz, wb);
        return approx(v, l2(v) - static_cast<double>(wb) + 2.0);
    }
    for (std::size_t k = 0; k < m; ++k) {
        const BigUInt a = multiprecision::constants::eulerian_number(m, k);
        num = cv_add(num, cv_scale(zk, BF(a), wb), wb);
        zk  = cv_mul(zk, cv_exact(z), wb);
    }
    const CV den = [&] {
        CV d = cv_exact(CX(BF::one()));
        for (std::uint64_t i = 0; i <= m; ++i) d = cv_mul(d, cv_exact(omz), wb);
        return d;
    }();
    const CX v = div(num.v, den.v, wb);
    const double rel = lsum(lsum(num.er, num.ei) - l2(num.v), lsum(den.er, den.ei) - l2(den.v));
    return approx(v, l2(v) + lsum(rel, -static_cast<double>(wb)) + 2.0);
}

} // namespace

Approx polylog_core(const CX& s, const CX& z, std::size_t w) {
    std::int64_t n = 0;
    const bool   sint = s.im.is_zero() && int64_of(s.re, n);
    const double lz   = l2(z);
    if (is_zero(z)) return approx(CX(), ninf);
    if (sint && n <= 0) return negative_int(static_cast<std::uint64_t>(-n), z, w);
    if (sint && n == 1) {
        const CX v = neg(ln(exact_one_minus(z), w + 8));
        return approx(v, l2(v) - static_cast<double>(w) - 6.0);
    }
    if (lz <= std::log2(0.75) || (!sint && lz < std::log2(0.9))) return series(s, z, w, sint, n);
    if (sint) {
        if (lz >= std::log2(1.4)) {
            const CX zi = recip(z, w + 16);
            const Approx a = series(s, zi, w + 8, true, n);
            const Approx b = continuation(n, z, w + 8);
            const CX sa = (n % 2 == 0) ? neg(a.v) : a.v;
            const CX v = add(sa, b.v, w + 8);
            return approx(v, lsum(lsum(a.er, b.er), l2(v) - static_cast<double>(w) - 6.0) + 1.0);
        }
        return unitcircle(n, z, w);
    }
    const double lu = l2(ln(z, 64));
    if (lu < 0.0) return mu_expansion(s, z, w);
    return general(s, z, w);
}

BC polylog(const BC& z, const BC& s, const BCC& ctx) {
    if (!(z.is_undefined() || s.is_undefined()) && (z.is_indeterminate() || s.is_indeterminate())) return BC::indeterminate();
    if (z.is_undefined() || s.is_undefined() || !z.is_finite() || !s.is_finite()) return BC::undefined();
    if (z.is_zero()) return BC::zero();
    const bool sreal = s.imaginary().is_zero();
    const bool zreal = z.imaginary().is_zero();
    std::int64_t n = 0;
    const bool sint = sreal && int64_of(s.real(), n);
    const bool z_one = zreal && BF::compare(z.real(), BF::one()) == BF::ordering::equal;
    if (z_one) {
        if (sint && n <= 1) return BC::undefined();
        return riemann_zeta(s, ctx);
    }
    if (sreal && zreal && BF::compare(z.real(), BF::one()) == BF::ordering::less) {
        const bool inside = BF::compare(z.real().abs(), BF::one()) == BF::ordering::less;
        if (inside || (!s.real().signbit() && !s.real().is_zero())) return BC(math::polylog(z.real(), s.real(), ctx.real()), BF::zero());
    }
    const bool real_result = sreal && zreal && BF::compare(z.real(), BF::one()) == BF::ordering::less;
    const CX Z = cx(z), S = cx(s);
    return drive(ctx, [&](std::size_t w) {
        const Approx a = polylog_core(S, Z, w);
        return real_result ? force_real(a) : a;
    });
}

BC dilogarithm(const BC& z, const BCC& ctx) { return polylog(z, BC(BF(std::uint64_t(2))), ctx); }
BC trilogarithm(const BC& z, const BCC& ctx) { return polylog(z, BC(BF(std::uint64_t(3))), ctx); }

BC spence_function(const BC& z, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.is_finite() && !(BF::compare(z.real(), BF::one(true)) == BF::ordering::less)) {
        return BC(math::spence_function(z.real(), ctx.real()), BF::zero());
    }
    const BC mz = z.negated();
    return drive(ctx, [&](std::size_t w) {
        const Approx a = polylog_core(CX(BF(std::uint64_t(2))), cx(mz), w);
        Approx r = a;
        r.v = neg(a.v);
        return r;
    });
}

BC spence_integral(const BC& z, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.is_finite() && !z.real().signbit()) return BC(math::spence_integral(z.real(), ctx.real()), BF::zero());
    const CX omz = exact_one_minus(cx(z));
    if (is_zero(omz)) return BC::zero();
    return drive(ctx, [&](std::size_t w) { return polylog_core(CX(BF(std::uint64_t(2))), omz, w); });
}

namespace {

Approx rogers_core(const CX& z, std::size_t w) {
    const std::size_t wb = w + 8;
    const Approx li = polylog_core(CX(BF(std::uint64_t(2))), z, wb);
    const CX a = ln(z, wb);
    const CX b = ln(exact_one_minus(z), wb);
    const CX p = scale2(mul(a, b, wb), -1);
    const CX v = add(li.v, p, wb);
    return approx(v, lsum(li.er, lsum(l2(p) + 2.0, l2(v)) - static_cast<double>(wb) + 1.0));
}

} // namespace

BC rogers_l(const BC& z, const BCC& ctx) {
    if (z.is_indeterminate()) return BC::indeterminate();
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero()) {
        const BF x = z.real();
        if (!x.signbit() || x.is_zero()) return BC(math::rogers_l(x, ctx.real()), BF::zero());
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return rogers_core(Z, w); });
}

BC rogers_l_normalized(const BC& z, const BCC& ctx) {
    if (z.is_indeterminate()) return BC::indeterminate();
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero()) {
        const BF x = z.real();
        if (!x.signbit() || x.is_zero()) return BC(math::rogers_l_normalized(x, ctx.real()), BF::zero());
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) {
        const Approx r = rogers_core(Z, w + 4);
        const BF z2 = multiprecision::constants::zeta2(wctx(w + 12));
        const CX v = div(r.v, z2, w + 8);
        return approx(v, lsum(r.er - l2(z2), l2(v) - static_cast<double>(w) - 6.0) + 1.0);
    });
}

BC legendre_chi(const BC& z, const BC& s, const BCC& ctx) {
    if (!(z.is_undefined() || s.is_undefined()) && (z.is_indeterminate() || s.is_indeterminate())) return BC::indeterminate();
    if (z.is_undefined() || s.is_undefined() || !z.is_finite() || !s.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero() && s.imaginary().is_zero()) {
        const BF r = math::legendre_chi(z.real(), s.real(), wctx(ctx.real_precision + 2));
        if (r.is_finite()) return BC(math::legendre_chi(z.real(), s.real(), ctx.real()), BF::zero());
    }
    if (z.is_zero()) return BC::zero();
    const CX Z = cx(z), S = cx(s);
    const bool real_result = z.imaginary().is_zero() && s.imaginary().is_zero() && BF::compare(z.real().abs(), BF::one()) == BF::ordering::less;
    std::int64_t sn = 0;
    const bool sint = S.im.is_zero() && int64_of(S.re, sn);
    const bool odd_series = l2(Z) <= std::log2(0.75) || (!sint && l2(Z) < std::log2(0.9));
    return drive(ctx, [&](std::size_t w) {
        if (odd_series) {
            const Approx r = series(S, Z, w, sint, sn, true);
            return real_result ? force_real(r) : r;
        }
        const Approx a = polylog_core(S, Z, w + 4);
        const Approx b = polylog_core(S, neg(Z), w + 4);
        const CX v = scale2(sub(a.v, b.v, w + 4), -1);
        const Approx r = approx(v, lsum(lsum(a.er, b.er), lsum(l2(a.v), l2(b.v)) - static_cast<double>(w) - 2.0));
        return real_result ? force_real(r) : r;
    });
}

BC clausen(const BC& theta, std::int64_t s, const BCC& ctx) {
    if (theta.is_indeterminate()) return BC::indeterminate();
    if (theta.is_undefined() || !theta.is_finite()) return BC::undefined();
    if (theta.imaginary().is_zero()) return BC(math::clausen(theta.real(), s, ctx.real()), BF::zero());
    const CX T = cx(theta);
    const bool even = (s % 2) == 0;
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wb = w + 8 + extra_bits(T);
        const CX e1 = exp(mul_i(T), wb);
        const CX e2 = exp(neg(mul_i(T)), wb);
        const CX S{BF(s)};
        const Approx a = polylog_core(S, e1, wb);
        const Approx b = polylog_core(S, e2, wb);
        CX v = even ? div_i(sub(a.v, b.v, wb)) : add(a.v, b.v, wb);
        v = scale2(v, -1);
        return approx(v, lsum(lsum(a.er, b.er), lsum(l2(a.v), l2(b.v)) - static_cast<double>(wb) + 1.0));
    });
}

namespace {

Approx debye_integral_core(const CX& x, std::uint64_t n, std::size_t w) {
    const std::size_t wb = w + 12 + static_cast<std::size_t>(n);
    const double lx = l2(x);
    if (lx < std::log2(pi_d)) {
        const CX x2 = sqr(x, wb);
        CX   acc = CX(BF::div(BF::one(), BF(n), wctx(wb)));
        acc = sub(acc, div(x, BF(2 * (n + 1)), wb), wb);
        Term T;
        T.add(l2(acc));
        CX   xp = x2;
        BF   f2 = BF(std::uint64_t(2));
        double tail = ninf;
        for (std::uint64_t k = 1;; ++k) {
            const BF bk = multiprecision::constants::bernoulli(2 * k, wctx(wb));
            const BF c  = BF::div(bk, BF::mul(BF(2 * k + n), f2, wctx(wb)), wctx(wb));
            const CX term = mul(xp, c, wb);
            acc = add(acc, term, wb);
            T.add(l2(term));
            const double ratio = 2.0 * (lx - std::log2(2.0 * pi_d));
            if (k > 2) {
                const double bound = l2(term) + ratio + 1.0 - std::log2(1.0 - std::exp2(ratio + 0.2));
                if (bound < T.l2max - static_cast<double>(w) - 4.0) {
                    tail = bound;
                    break;
                }
            }
            xp = mul(xp, x2, wb);
            f2 = BF::mul(f2, BF((2 * k + 1) * (2 * k + 2)), wctx(wb + 8));
            if (k > 10 * w + 100) break;
        }
        const CX xn = pow_int(x, static_cast<std::int64_t>(n), wb);
        const CX v  = mul(xn, acc, wb);
        return approx(v, l2(xn) + lsum(T.l2sum - static_cast<double>(w) + 4.0, tail) + 1.0);
    }
    const BFC c  = wctx(wb);
    const BF  nf = math::factorial(BF(n), c);
    const BF  zt = math::riemann_zeta(BF(n + 1), c);
    const CX  emx = exp(neg(x), wb + extra_bits(x));
    CX   sum(BF::mul(nf, zt, c));
    Term T;
    T.add(l2(sum));
    double err = ninf;
    BF   coef = BF::one();
    for (std::uint64_t k = 0; k <= n; ++k) {
        if (k > 0) coef = BF::mul(coef, BF(n - k + 1), c);
        const Approx li = polylog_core(CX(BF(k + 1)), emx, wb);
        const CX t = mul(mul(pow_int(x, static_cast<std::int64_t>(n - k), wb), li.v, wb), coef, wb);
        sum = sub(sum, t, wb);
        T.add(l2(t));
        err = lsum(err, li.er + l2(coef) + static_cast<double>(n - k) * lx);
    }
    return approx(sum, lsum(err, T.l2sum - static_cast<double>(w) + 4.0));
}

Approx debye_tail_core(const CX& x, std::uint64_t n, std::size_t w) {
    const std::size_t wb = w + 12 + static_cast<std::size_t>(n);
    const BFC c  = wctx(wb);
    const double lx = l2(x);
    const CX  emx = exp(neg(x), wb + extra_bits(x));
    CX   sum;
    Term T;
    double err = ninf;
    BF   coef = BF::one();
    for (std::uint64_t k = 0; k <= n; ++k) {
        if (k > 0) coef = BF::mul(coef, BF(n - k + 1), c);
        const Approx li = polylog_core(CX(BF(k + 1)), emx, wb);
        const CX t = mul(mul(pow_int(x, static_cast<std::int64_t>(n - k), wb), li.v, wb), coef, wb);
        sum = add(sum, t, wb);
        T.add(l2(t));
        err = lsum(err, li.er + l2(coef) + static_cast<double>(n - k) * lx);
    }
    return approx(sum, lsum(err, T.l2sum - static_cast<double>(w) + 4.0));
}

} // namespace

BC debye_integral(const BC& x, std::int64_t n, const BCC& ctx) {
    if (x.is_indeterminate()) return BC::indeterminate();
    if (x.is_undefined() || !x.is_finite() || n < 1) return BC::undefined();
    if (x.imaginary().is_zero() && !x.real().signbit()) return BC(math::debye_integral(x.real(), n, ctx.real()), BF::zero());
    const CX X = cx(x);
    const bool real_result = x.imaginary().is_zero() && l2(X) < std::log2(2.0 * pi_d);
    return drive(ctx, [&](std::size_t w) {
        const Approx a = debye_integral_core(X, static_cast<std::uint64_t>(n), w);
        return real_result ? force_real(a) : a;
    });
}

BC debye_tail(const BC& x, std::int64_t n, const BCC& ctx) {
    if (x.is_indeterminate()) return BC::indeterminate();
    if (x.is_undefined() || !x.is_finite() || n < 1) return BC::undefined();
    if (x.imaginary().is_zero() && !x.real().signbit()) return BC(math::debye_tail(x.real(), n, ctx.real()), BF::zero());
    const CX X = cx(x);
    return drive(ctx, [&](std::size_t w) { return debye_tail_core(X, static_cast<std::uint64_t>(n), w); });
}

BC debye(const BC& x, std::int64_t n, const BCC& ctx) {
    if (x.is_indeterminate()) return BC::indeterminate();
    if (x.is_undefined() || !x.is_finite() || n < 1) return BC::undefined();
    if (x.imaginary().is_zero() && !x.real().signbit()) return BC(math::debye(x.real(), n, ctx.real()), BF::zero());
    const CX X = cx(x);
    const bool real_result = x.imaginary().is_zero() && l2(X) < std::log2(2.0 * pi_d);
    return drive(ctx, [&](std::size_t w) {
        const Approx a = debye_integral_core(X, static_cast<std::uint64_t>(n), w + 4);
        const CX xn = pow_int(X, n, w + 8);
        const CX v  = div(mul(a.v, BF(n), w + 8), xn, w + 8);
        const Approx r = approx(v, l2(v) + lsum(lsum(a.er, a.ei) - l2(a.v), -static_cast<double>(w) - 6.0) + 1.0);
        return real_result ? force_real(r) : r;
    });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
