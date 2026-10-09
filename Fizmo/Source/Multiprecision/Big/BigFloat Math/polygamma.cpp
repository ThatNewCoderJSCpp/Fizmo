#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace pgdetail {

std::size_t pg_cancel(const BigFloat& a, const BigFloat& b, const BigFloat& r, std::size_t prec) {
    const std::int64_t ea = a.get_exp_base2();
    const std::int64_t eb = b.get_exp_base2();
    const std::int64_t er = r.get_exp_base2();
    std::int64_t hi = ea;
    if (eb != BigFloat::exp_none && (hi == BigFloat::exp_none || eb > hi)) hi = eb;
    if (hi == BigFloat::exp_none) return 0;
    if (er == BigFloat::exp_none) return prec;                         
    return (hi > er) ? static_cast<std::size_t>(hi - er) : 0;
}

BigFloat pg_coeff(unsigned n, std::size_t j, const BigFloatContext& tc) {
    if (j == 0 || j > constants::detail::bn_index_cap) return BigFloat::undefined();
    std::vector<pg_coef_t>& cache = pg_coef_cache(n);
    if (cache.size() <= j) cache.resize(j + 1);
    pg_coef_t& e = cache[j];

    if (e.prec < tc.precision + 8) {
        const BigFloatContext hc(BigFloatContext::clamp_precision(tc.precision + 32), RoundingMode::nearest_even);
        BigUInt num = constants::tangent_number(j);
        if (num.is_undefined()) return BigFloat::undefined();
        if (n > 0) num = num * fact_range(2 * static_cast<std::uint64_t>(j), 2 * static_cast<std::uint64_t>(j) + n);
        BigUInt den = constants::bfdetail::unit(2 * j);
        den.sub_small_mutable(1);
        e.v = BigFloat::div(BigFloat(num), BigFloat(den), hc).scaled_pow2(-2 * static_cast<std::int64_t>(j)).with_sign(j % 2 == 0);
        e.prec = hc.precision;
    }

    return e.v.rounded(tc);
}

double pg_term_log2(unsigned n, std::size_t j, double lx) {
    const double l2    = 0.6931471805599453;
    const double lg2pi = 2.6514961294723187;
    const double u     = 2.0 * static_cast<double>(j);
    return 1.0 + std::lgamma(u + static_cast<double>(n)) / l2 - u * lg2pi - (u + static_cast<double>(n)) * lx;
}

std::size_t pg_jcap(int n, std::size_t j0, double lx, std::int64_t eh, std::size_t w) {
    const double target = static_cast<double>(eh) - static_cast<double>(w) - 8.0;
    double prev = 1.0e300;

    for (std::size_t j = j0; j <= (1u << 20); ++j) {
        const double lt = pg_term_log2(n, j, lx);
        if (lt < target || lt >= prev) return j + 8;
        prev = lt;
    }

    return 1u << 20;
}

BigFloat pg_bsum(int n, std::size_t j0, const BigFloat& x, std::int64_t eh, const BigFloatContext& wc) {
    const BigFloat      u    = x.reciprocal(wc);
    const BigFloat      u2   = BigFloat::mul(u, u, wc);
    const std::uint64_t e0   = static_cast<std::uint64_t>(2 * static_cast<std::int64_t>(j0) + n);
    BigFloat            t    = gm_powi(u, e0, wc);                         
    const double        lx2  = gm_log2_fine(x);
    const std::size_t   jcap = pg_jcap(n, j0, lx2, eh, wc.precision);
    const std::size_t   G    = 32 + detail::ln_bit_length_u64(jcap);
    std::size_t jz = jcap + 1;

    for (std::size_t j = (j0 > gm_zeta_jmin) ? j0 : gm_zeta_jmin; j <= jcap; ++j) {
        const std::size_t p = gm_term_prec(wc.precision, eh, pg_term_log2(n, j, lx2), G);
        if (p + 32 <= 2 * j * gm_zeta_kbits) { jz = j; break; }
    }

    constants::detail::tangent_reserve(((jz <= jcap) ? jz : jcap) + 1);
    BigFloat     S    = BigFloat::zero();
    BigFloat     a    = BigFloat::zero();
    BigFloat     c    = BigFloat::zero();
    gm_zeta_t    zs;
    bool         zon  = false;
    std::int64_t prev = (std::numeric_limits<std::int64_t>::max)();

    for (std::size_t j = j0; j <= jcap; ++j) {
        const std::size_t     p   = gm_term_prec(wc.precision, eh, pg_term_log2(n, j, lx2), G);
        const BigFloatContext tc(BigFloatContext::clamp_precision(p), RoundingMode::nearest_even);
        const std::uint64_t   u2j = 2 * static_cast<std::uint64_t>(j);
        const std::uint64_t   v   = static_cast<std::uint64_t>(static_cast<std::int64_t>(u2j) + n);   // 2j + n >= 1
        BigFloat term;

        if (!zon && j >= jz) {
            const BigFloat tp = constants::two_pi(tc);
            const BigFloat f  = BigFloat(fact_range(1, v)).rounded(tc);
            a = BigFloat::div(BigFloat::mul(f, t, tc), gm_powi(tp, u2j, tc), tc).scaled_pow2(1);
            c = BigFloat::div(u2.rounded(tc), BigFloat::mul(tp, tp, tc), tc);
            gm_zeta_init(zs, j, p + 32);
            zon = true;
        }

        if (zon) {
            term = BigFloat::mul(a, gm_zeta_value(zs, tc), tc);
            if (j % 2 == 0) term = term.with_sign(true);
        } else {
            const BigFloat b = pg_coeff(n, j, tc);
            if (!b.is_finite()) break;
            term = BigFloat::mul(b, t, tc);
        }

        if (!term.is_finite()) break;
        const std::int64_t et = term.get_exp_base2();
        if (et == BigFloat::exp_none) break;
        if (et >= prev) break;

        if (et < eh - static_cast<std::int64_t>(wc.precision)) {
            S = BigFloat::add(S, term, wc);
            break;
        }

        prev = et;
        S = BigFloat::add(S, term, wc);

        if (zon) {
            a = BigFloat::mul(a, BigFloat(v * (v + 1)), tc);
            a = BigFloat::mul(a, c.rounded(tc), tc);
            const std::size_t pn = gm_term_prec(wc.precision, eh, pg_term_log2(n, j + 1, lx2), G);
            gm_zeta_step(zs, pn + 32);
        } else {
            t = BigFloat::mul(t, u2.rounded(tc), tc);
        }
    }

    return S;
}

BigFloat pg_asym(unsigned n, const BigFloat& x, const BigFloatContext& wc) {
    const BigFloat u = x.reciprocal(wc);
    BigFloat head;

    if (n == 0) {
        const BigFloat lx = ln(x, wc);
        if (!lx.is_finite()) return lx;
        head = BigFloat::sub(lx, u.scaled_pow2(-1), wc);
    } else {
        const BigFloat un = gm_powi(u, n, wc);
        const BigFloat h  = BigFloat::mul(un, BigFloat(fact_range(1, n)).rounded(wc), wc);       
        const BigFloat hn = BigFloat::mul(h, BigFloat(static_cast<std::uint64_t>(n)), wc);       
        head = BigFloat::add(h, BigFloat::mul(hn, u, wc).scaled_pow2(-1), wc);
    }

    const std::int64_t eh  = head.get_exp_base2();
    const std::int64_t ehp = (eh == BigFloat::exp_none) ? 0 : eh;
    const BigFloat     S   = pg_bsum(static_cast<int>(n), 1, x, ehp, wc);
    if (n == 0) return BigFloat::sub(head, S, wc);
    const BigFloat r = BigFloat::add(head, S, wc);
    return (n % 2 == 0) ? r.with_sign(!r.signbit()) : r;
}

BigFloat pg_recip_sum(unsigned n, const BigFloat& z, std::uint64_t m, const BigFloatContext& wc) {
    const std::size_t Sp = (m < pg_block) ? static_cast<std::size_t>(m) : pg_block;
    std::vector<BigFloat> zp(Sp + 1, BigFloat::one());
    zp[1] = z;
    for (std::size_t j = 2; j <= Sp; ++j) zp[j] = BigFloat::mul(zp[j - 1], z, wc);
    BigFloat             N = BigFloat::zero();
    BigFloat             D = BigFloat::one();
    std::vector<BigUInt> c;

    for (std::uint64_t k = 0; k < m; ) {
        const std::size_t s = (m - k < Sp) ? static_cast<std::size_t>(m - k) : Sp;
        gm_block_poly(k, s, c);
        const std::uint64_t su = static_cast<std::uint64_t>(s);
        BigFloat B  = zp[s];                                                          
        BigFloat B1 = BigFloat::mul(zp[s - 1], BigFloat(su), wc);
        BigFloat B2 = (n == 1 && s >= 2) ? BigFloat::mul(zp[s - 2], BigFloat(su * (su - 1)), wc) : BigFloat::zero();

        for (std::size_t j = 0; j < s; ++j) {
            if (c[j].is_zero()) continue;
            B = BigFloat::add(B, BigFloat::mul(zp[j], BigFloat(c[j]), wc), wc);

            if (j >= 1) {
                BigUInt d1 = c[j];
                d1.mul_small_mutable(static_cast<std::uint64_t>(j));
                B1 = BigFloat::add(B1, BigFloat::mul(zp[j - 1], BigFloat(d1), wc), wc);
            }

            if (n == 1 && j >= 2) {
                BigUInt d2 = c[j];
                d2.mul_small_mutable(static_cast<std::uint64_t>(j * (j - 1)));
                B2 = BigFloat::add(B2, BigFloat::mul(zp[j - 2], BigFloat(d2), wc), wc);
            }
        }

        if (n == 0) {
            N = BigFloat::add(BigFloat::mul(N, B, wc), BigFloat::mul(D, B1, wc), wc);
            D = BigFloat::mul(D, B, wc);
        } else {
            const BigFloat P  = BigFloat::sub(BigFloat::mul(B1, B1, wc), BigFloat::mul(B, B2, wc), wc);  // loses <= log2(s) bits
            const BigFloat Bq = BigFloat::mul(B, B, wc);
            N = BigFloat::add(BigFloat::mul(N, Bq, wc), BigFloat::mul(D, P, wc), wc);
            D = BigFloat::mul(D, Bq, wc);
        }

        if (!N.is_finite() || !D.is_finite()) return BigFloat::undefined();
        k += s;
    }

    return BigFloat::div(N, D, wc);
}

std::vector<BigUInt> pg_pmul(const std::vector<BigUInt>& a, const std::vector<BigUInt>& b) {
    std::vector<BigUInt> r(a.size() + b.size() - 1, BigUInt::zero());

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].is_zero()) continue;

        for (std::size_t j = 0; j < b.size(); ++j) {
            if (b[j].is_zero()) continue;
            r[i + j].add_mutable(a[i] * b[j]);
        }
    }

    return r;
}

std::vector<BigUInt> pg_binom_poly(std::uint64_t a, std::uint64_t s) {
    std::vector<BigUInt> c(static_cast<std::size_t>(s) + 1, BigUInt::zero());
    c[static_cast<std::size_t>(s)] = BigUInt::one();

    for (std::uint64_t j = s; j > 0; --j) {                     
        BigUInt v = c[static_cast<std::size_t>(j)];
        v.mul_small_mutable(a);
        v.mul_small_mutable(j);
        v.div_small_mutable(s - j + 1);
        c[static_cast<std::size_t>(j - 1)] = v;
    }

    return c;
}

BigFloat pg_poly_eval(const std::vector<BigUInt>& p, const std::vector<BigFloat>& zp, const BigFloatContext& wc) {
    BigFloat r = BigFloat::zero();

    for (std::size_t j = 0; j < p.size(); ++j) {
        if (p[j].is_zero()) continue;
        r = BigFloat::add(r, BigFloat::mul(zp[j], BigFloat(p[j]), wc), wc);
    }

    return r;
}

BigFloat pg_power_sum(std::uint64_t s, const BigFloat& z, std::uint64_t m, const BigFloatContext& wc) {
    BigFloat N = BigFloat::zero();
    BigFloat D = BigFloat::one();

    if (s > pg_pow_budget) {                                  
        for (std::uint64_t k = 0; k < m; ++k) {
            const BigFloat p = gm_powi(BigFloat::add(z, BigFloat(k), wc), s, wc);
            N = BigFloat::add(BigFloat::mul(N, p, wc), D, wc);
            D = BigFloat::mul(D, p, wc);
            if (!N.is_finite() || !D.is_finite()) return BigFloat::undefined();
        }

        return BigFloat::div(N, D, wc);
    }

    std::size_t S = static_cast<std::size_t>(pg_pow_budget / s);
    if (S < 1) S = 1;
    if (S > m) S = static_cast<std::size_t>(m);
    const std::size_t deg = static_cast<std::size_t>(s) * S;
    std::vector<BigFloat> zp(deg + 1, BigFloat::one());
    if (deg >= 1) zp[1] = z;
    for (std::size_t j = 2; j <= deg; ++j) zp[j] = BigFloat::mul(zp[j - 1], z, wc);
    std::vector<std::vector<BigUInt>> F(S), pre(S + 1), suf(S + 1);

    for (std::uint64_t k = 0; k < m; ) {
        const std::size_t sb = (m - k < S) ? static_cast<std::size_t>(m - k) : S;
        for (std::size_t i = 0; i < sb; ++i) F[i] = pg_binom_poly(k + i, s);
        pre[0] = std::vector<BigUInt>(1, BigUInt::one());
        for (std::size_t i = 0; i < sb; ++i) pre[i + 1] = pg_pmul(pre[i], F[i]);
        suf[sb] = std::vector<BigUInt>(1, BigUInt::one());
        for (std::size_t i = sb; i > 0; --i) suf[i - 1] = pg_pmul(F[i - 1], suf[i]);
        std::vector<BigUInt> Nb(static_cast<std::size_t>(s) * (sb - 1) + 1, BigUInt::zero());

        for (std::size_t i = 0; i < sb; ++i) {
            const std::vector<BigUInt> q = pg_pmul(pre[i], suf[i + 1]);
            for (std::size_t j = 0; j < q.size(); ++j) Nb[j].add_mutable(q[j]);
        }

        const BigFloat Nv = pg_poly_eval(Nb, zp, wc);
        const BigFloat Dv = pg_poly_eval(pre[sb], zp, wc);
        N = BigFloat::add(BigFloat::mul(N, Dv, wc), BigFloat::mul(D, Nv, wc), wc);
        D = BigFloat::mul(D, Dv, wc);
        if (!N.is_finite() || !D.is_finite()) return BigFloat::undefined();
        k += sb;
    }

    return BigFloat::div(N, D, wc);
}

const std::vector<BigUInt>& pg_cot_poly(unsigned n) {
    static thread_local std::unordered_map<unsigned, std::vector<BigUInt>> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    std::vector<BigUInt> q(2, BigUInt::zero());
    q[1] = BigUInt::one();

    for (unsigned k = 0; k < n; ++k) {
        std::vector<BigUInt> r(q.size() + 1, BigUInt::zero());

        for (std::size_t i = 1; i < q.size(); ++i) {
            if (q[i].is_zero()) continue;
            BigUInt d = q[i];
            d.mul_small_mutable(static_cast<std::uint64_t>(i));     
            r[i - 1].add_mutable(d);
            r[i + 1].add_mutable(d);
        }

        q.swap(r);
    }

    return cache.emplace(n, std::move(q)).first->second;
}

BigFloat pg_cot_eval(const std::vector<BigUInt>& q, const BigFloat& c, const BigFloatContext& wc) {
    const std::size_t deg = q.size() - 1;
    const std::size_t par = deg % 2;
    const BigFloat    c2  = BigFloat::mul(c, c, wc);
    BigFloat acc = BigFloat(q[deg]).rounded(wc);

    for (std::size_t i = deg; i >= par + 2; i -= 2) {
        acc = BigFloat::add(BigFloat::mul(acc, c2, wc), BigFloat(q[i - 2]).rounded(wc), wc);
    }

    return (par == 1) ? BigFloat::mul(acc, c, wc) : acc;
}

void pg_ratsum(std::uint64_t a, std::uint64_t b, std::uint64_t s, std::uint64_t o, unsigned e, BigUInt& P, BigUInt& Q) {
    if (b - a == 1) {
        const std::uint64_t d = s * a + o;
        Q = BigUInt(d);
        if (e == 2) Q.mul_small_mutable(d);
        P = BigUInt::one();
        return;
    }

    const std::uint64_t mid = a + (b - a) / 2;
    BigUInt PL, QL, PR, QR;
    pg_ratsum(a, mid, s, o, e, PL, QL);
    pg_ratsum(mid, b, s, o, e, PR, QR);
    P = PL * QR;
    P.add_mutable(PR * QL);
    Q = QL * QR;
}

pg_val_t pg_exact(unsigned n, std::uint64_t N, bool half, const BigFloatContext& wc) {
    pg_val_t r;
    BigFloat s = BigFloat::zero();

    if (N > 0) {
        BigUInt P, Q;
        pg_ratsum(0, N, half ? 2 : 1, 1, n + 1, P, Q);
        s = BigFloat::div(BigFloat(P).rounded(wc), BigFloat(Q).rounded(wc), wc);
    }

    BigFloat a, b;

    if (n == 0) {
        a = half ? s.scaled_pow2(1) : s;
        b = constants::euler_mascheroni(wc);
        if (half) b = BigFloat::add(b, constants::ln2(wc).scaled_pow2(1), wc);
    } else {
        const BigFloat z2 = constants::zeta2(wc);
        a = half ? BigFloat::mul(z2, BigFloat(static_cast<std::uint64_t>(3)), wc) : z2;
        b = half ? s.scaled_pow2(2) : s;
    }

    r.v = BigFloat::sub(a, b, wc);
    r.cancelled = pg_cancel(a, b, r.v, wc.precision);
    return r;
}

pg_val_t pg_pos(unsigned n, const BigFloat& z, const BigFloatContext& wc) {
    pg_val_t r;
    const std::uint64_t T = pg_target(n, wc.precision);
    std::uint64_t k = 0;

    if (n <= 1) {
        if (gm_exact_u64(z, k) && k >= 1 && k < T) return pg_exact(n, k - 1, false, wc);
        if (gm_half_odd(z, k) && k < T)            return pg_exact(n, k, true, wc);
    }

    std::uint64_t m = 0;
    if (!gm_shift_count(z, T, m)) { r.v = BigFloat::undefined(); return r; }
    const BigFloat x = (m == 0) ? z : BigFloat::add(z, BigFloat(m), wc);
    const BigFloat ax = pg_asym(n, x, wc);
    if (!ax.is_finite() || m == 0) { r.v = ax; return r; }

    if (n >= 2) {
        const BigFloat ps = pg_power_sum(static_cast<std::uint64_t>(n) + 1, z, m, wc);
        if (!ps.is_finite()) { r.v = ps; return r; }
        BigFloat sh = BigFloat::mul(ps, BigFloat(fact_range(1, static_cast<std::uint64_t>(n) + 1)).rounded(wc), wc);
        if (n % 2 == 0) sh = sh.with_sign(true);
        r.v = BigFloat::add(ax, sh, wc);
        return r;
    }

    const BigFloat s = pg_recip_sum(n, z, m, wc);
    if (!s.is_finite()) { r.v = s; return r; }

    if (n == 0) {
        r.v = BigFloat::sub(ax, s, wc);                          
        r.cancelled = pg_cancel(ax, s, r.v, wc.precision);
    } else {
        r.v = BigFloat::add(ax, s, wc);                          
    }

    return r;
}

pg_val_t pg_eval(unsigned n, const BigFloat& z, const BigFloatContext& wc) {
    if (!z.is_zero() && !z.signbit()) return pg_pos(n, z, wc);
    pg_val_t r;
    const BigFloatContext fc(BigFloatContext::clamp_precision(wc.precision + 64), RoundingMode::nearest_even);
    const BigFloat f = gm_frac(z, fc);
    if (!f.is_finite()) { r.v = BigFloat::undefined(); return r; }
    if (f.is_zero())    { r.pole = true; return r; }
    const BigFloat om = BigFloat::sub(BigFloat::one(), z, wc);
    const pg_val_t p  = pg_pos(n, om, wc);
    if (!p.v.is_finite()) return p;
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    const auto     fh   = BigFloat::compare(f, half);
    const bool     flip = (fh == BigFloat::ordering::greater);
    const BigFloat g    = flip ? BigFloat::sub(BigFloat::one(), f, fc) : f;    
    const BigFloat pi   = constants::pi(wc);
    const BigFloat pg   = BigFloat::mul(pi, g, wc);

    if (n == 0) {
        if (fh == BigFloat::ordering::equal) return p;
        BigFloat t = BigFloat::mul(pi, cot(pg, wc), wc);
        if (!t.is_finite()) { r.v = BigFloat::undefined(); return r; }
        if (flip) t = t.with_sign(!t.signbit());
        r.v = BigFloat::sub(p.v, t, wc);
        r.cancelled = p.cancelled + pg_cancel(p.v, t, r.v, wc.precision);
    } else if (n == 1) {
        const BigFloat s = (fh == BigFloat::ordering::equal) ? BigFloat::one() : sin(pg, wc);
        if (!s.is_finite() || s.is_zero()) { r.v = BigFloat::undefined(); return r; }
        const BigFloat t = BigFloat::div(BigFloat::mul(pi, pi, wc), BigFloat::mul(s, s, wc), wc);
        r.v = BigFloat::sub(t, p.v, wc);
        r.cancelled = p.cancelled + pg_cancel(t, p.v, r.v, wc.precision);
    } else {
        BigFloat c = BigFloat::zero();

        if (fh != BigFloat::ordering::equal) {
            c = cot(pg, wc);
            if (!c.is_finite()) { r.v = BigFloat::undefined(); return r; }
            if (flip) c = c.with_sign(!c.signbit());
        }

        const BigFloat q = pg_cot_eval(pg_cot_poly(n), c, wc);
        const BigFloat t = BigFloat::mul(gm_powi(pi, static_cast<std::uint64_t>(n) + 1, wc), q, wc);
        BigFloat v = BigFloat::sub(p.v, t, wc);
        r.cancelled = p.cancelled + pg_cancel(p.v, t, v, wc.precision);
        if (n % 2 == 1) v = v.with_sign(!v.signbit());
        r.v = v;
    }

    return r;
}

BigFloat pg_drive(unsigned n, const BigFloat& z, const BigFloatContext& ctx) {
    std::size_t       guard = 32;
    const double      T     = static_cast<double>(pg_target(n, ctx.precision + guard));
    const std::size_t base  = static_cast<std::size_t>(std::log2(T)) + 24;
    std::size_t       extra = base;

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard + extra), RoundingMode::nearest_even);
        const pg_val_t g = pg_eval(n, z, wc);
        if (g.pole) return BigFloat::undefined();
        if (!g.v.is_finite()) return g.v;

        if (g.cancelled + base > extra) {
            if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
            extra = g.cancelled + base;
            continue;
        }

        if (g.v.is_zero()) return g.v;
        const std::size_t acc = (wc.precision > extra + 8) ? (wc.precision - extra - 8) : 1;
        const std::size_t L   = g.v.significand().bit_length();
        const std::size_t err = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(g.v.significand(), ctx.precision, err + 2)) return g.v.rounded(ctx);
        if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat pg_bern(std::size_t j, const BigFloatContext& wc) {
    if (j == 0)     return BigFloat::one();
    if (j == 1)     return BigFloat::one().scaled_pow2(-1).with_sign(true);
    if (j % 2 == 1) return BigFloat::zero();
    return constants::bernoulli_float(j, wc);
}

void hz_eprod(std::uint64_t lo, std::uint64_t hi, const BigFloat& a, unsigned k, const BigFloatContext& wc, std::vector<BigFloat>& E) {
    if (hi - lo == 1) {
        E.assign(k + 1, BigFloat::one());
        E[0] = BigFloat::add(a, BigFloat(lo), wc);
        return;
    }

    const std::uint64_t mid = lo + (hi - lo) / 2;
    std::vector<BigFloat> L, R;
    hz_eprod(lo, mid, a, k, wc, L);
    hz_eprod(mid, hi, a, k, wc, R);
    const std::uint64_t len = mid - lo;
    std::vector<std::uint64_t> C(k + 1, 0);
    C[0] = 1;
    for (unsigned t = 1; t <= k; ++t) C[t] = (t <= len) ? C[t - 1] * (len - t + 1) / t : 0;
    std::vector<bool> rone(k + 1);
    for (unsigned s = 0; s <= k; ++s) rone[s] = (BigFloat::compare(R[s], BigFloat::one()) == BigFloat::ordering::equal);
    E.assign(k + 1, BigFloat::one());

    for (unsigned r = 0; r <= k; ++r) {
        BigFloat v = L[r];

        for (unsigned s = 0; s <= r; ++s) {
            const std::uint64_t e = C[r - s];
            if (e == 0 || rone[s]) continue;
            v = BigFloat::mul(v, (e == 1) ? R[s] : gm_powi(R[s], e, wc), wc);
        }

        E[r] = v;
    }
}

BigFloat hz_logsum(unsigned k, const BigFloat& a, std::uint64_t N, const BigFloat& y, const BigFloatContext& wc) {
    const double l2  = 0.6931471805599453;
    const double ly2 = gm_log2_fine(y);
    double lc = 0.0;                                           
    if (N > k) lc = (std::lgamma(static_cast<double>(N) + 1.0) - std::lgamma(static_cast<double>(k) + 2.0) - std::lgamma(static_cast<double>(N - k))) / l2;
    BigFloat L = BigFloat::zero();

    if (lc + std::log2(ly2 + 1.0) >= 58.0) {                    
        for (std::uint64_t j = 0; j < N; ++j) {
            const BigFloat v = BigFloat::add(a, BigFloat(j), wc);
            L = BigFloat::add(L, BigFloat::mul(gm_powi(v, k, wc), ln(v, wc), wc), wc);
        }

        return L;
    }

    std::vector<BigFloat> E;
    hz_eprod(0, N, a, k, wc, E);
    std::vector<std::vector<BigUInt>> S2(k + 1, std::vector<BigUInt>(k + 1, BigUInt::zero()));
    S2[0][0] = BigUInt::one();

    for (unsigned i = 1; i <= k; ++i) {
        for (unsigned r = 1; r <= i; ++r) {
            BigUInt v = S2[i - 1][r];
            v.mul_small_mutable(r);
            v.add_mutable(S2[i - 1][r - 1]);
            S2[i][r] = v;
        }
    }

    std::vector<BigFloat> ap(k + 1, BigFloat::one());
    for (unsigned i = 1; i <= k; ++i) ap[i] = BigFloat::mul(ap[i - 1], a, wc);
    std::vector<BigUInt> ck(k + 1, BigUInt::one());

    for (unsigned i = 1; i <= k; ++i) {
        ck[i] = ck[i - 1];
        ck[i].mul_small_mutable(k - i + 1);
        ck[i].div_small_mutable(i);
    }

    for (unsigned r = 0; r <= k; ++r) {
        if (static_cast<std::uint64_t>(r) >= N) break;        
        BigFloat al = BigFloat::zero();

        for (unsigned i = r; i <= k; ++i) {
            if (S2[i][r].is_zero()) continue;
            al = BigFloat::add(al, BigFloat::mul(ap[k - i], BigFloat(ck[i] * S2[i][r]), wc), wc);
        }

        if (al.is_zero()) continue;
        al = BigFloat::mul(al, BigFloat(fact_range(1, static_cast<std::uint64_t>(r) + 1)), wc);
        L  = BigFloat::add(L, BigFloat::mul(al, ln(E[r], wc), wc), wc);
    }

    return L;
}

pg_val_t hz_dz(unsigned k, const BigFloat& a, const BigFloatContext& wc) {
    pg_val_t      r;
    std::uint64_t N = 0;
    if (!gm_shift_count(a, hz_target(wc.precision, k), N)) { r.v = BigFloat::undefined(); return r; }
    const BigFloat y  = (N == 0) ? a : BigFloat::add(a, BigFloat(N), wc);
    const BigFloat ly = ln(y, wc);
    if (!ly.is_finite()) { r.v = BigFloat::undefined(); return r; }
    const BigFloat yk  = gm_powi(y, k, wc);
    const BigFloat yk1 = BigFloat::mul(yk, y, wc);
    const BigFloat k1(static_cast<std::uint64_t>(k) + 1);
    BigFloat head = BigFloat::div(BigFloat::mul(yk1, ly, wc), k1, wc);
    head = BigFloat::sub(head, BigFloat::div(yk1, BigFloat::mul(k1, k1, wc), wc), wc);
    head = BigFloat::sub(head, BigFloat::mul(yk, ly, wc).scaled_pow2(-1), wc);

    for (unsigned i = 1; 2 * i - 1 <= k; ++i) {
        const BigFloat b = BigFloat::div(pg_bern(2 * i, wc), BigFloat(fact_range(1, 2 * static_cast<std::uint64_t>(i) + 1)).rounded(wc), wc);
        const BigFloat P = BigFloat(fact_range(k - 2 * i + 2, static_cast<std::uint64_t>(k) + 1)).rounded(wc);
        BigFloat hd = BigFloat::zero();
        for (unsigned q = k - 2 * i + 2; q <= k; ++q) hd = BigFloat::add(hd, BigFloat(static_cast<std::uint64_t>(q)).reciprocal(wc), wc);
        BigFloat term = BigFloat::mul(BigFloat::mul(b, P, wc), BigFloat::add(hd, ly, wc), wc);
        term = BigFloat::mul(term, gm_powi(y, k - 2 * i + 1, wc), wc);
        head = BigFloat::add(head, term, wc);
    }

    const std::int64_t eh  = head.get_exp_base2();
    const std::int64_t ehp = (eh == BigFloat::exp_none) ? 0 : eh;
    BigFloat tail = pg_bsum(-static_cast<int>(k) - 1, static_cast<std::size_t>((k + 1) / 2 + 1), y, ehp, wc);
    tail = BigFloat::mul(tail, BigFloat(fact_range(1, static_cast<std::uint64_t>(k) + 1)).rounded(wc), wc);
    if (k % 2 == 1) tail = tail.with_sign(!tail.signbit());
    const BigFloat ht = BigFloat::add(head, tail, wc);
    if (N == 0) { r.v = ht; return r; }
    const BigFloat L = hz_logsum(k, a, N, y, wc);
    if (!L.is_finite()) { r.v = BigFloat::undefined(); return r; }
    r.v = BigFloat::sub(ht, L, wc);
    r.cancelled = pg_cancel(ht, L, r.v, wc.precision);
    return r;
}

BigFloat hz_zeta_prime_neg(unsigned m, const BigFloatContext& wc) {
    static thread_local std::unordered_map<unsigned, gm_const_cache_t> cache;
    gm_const_cache_t& c = cache[m];

    if (c.prec < wc.precision + 16) {
        const double T = static_cast<double>(hz_target(wc.precision, m));
        std::size_t extra = 64 + static_cast<std::size_t>(static_cast<double>(m + 1) * std::log2(T + 1.0));

        for (;;) {
            const BigFloatContext hc(BigFloatContext::clamp_precision(wc.precision + extra), RoundingMode::nearest_even);
            const pg_val_t z = hz_dz(m, BigFloat::one(), hc);
            if (!z.v.is_finite()) return z.v;
            const std::size_t good = (hc.precision > z.cancelled + 32) ? (hc.precision - z.cancelled - 32) : 0;

            if (good >= wc.precision + 16 || gm_guard_exhausted(wc.precision, extra)) {
                c.v    = z.v;
                c.prec = good;
                break;
            }

            extra = z.cancelled + 64;
        }
    }

    return c.v.rounded(wc);
}

pg_val_t pg_neg_eval(unsigned M, const BigFloat& x, const BigFloatContext& wc) {
    pg_val_t r;
    const pg_val_t g0 = hz_dz(M - 1, x, wc);
    if (!g0.v.is_finite()) return g0;
    const BigFloat g = BigFloat::div(g0.v, BigFloat(fact_range(1, M)).rounded(wc), wc);      
    std::vector<BigFloat> q(1, gm_half_ln_two_pi(wc));                                      

    for (unsigned m = 1; m < M; ++m) {
        const std::size_t sz = (q.size() + 1 > m + 2) ? q.size() + 1 : m + 2;
        std::vector<BigFloat> nq(sz, BigFloat::zero());
        for (std::size_t i = 0; i < q.size(); ++i) nq[i + 1] = BigFloat::div(q[i], BigFloat(static_cast<std::uint64_t>(i + 1)), wc);
        const BigFloat mf = BigFloat(fact_range(1, static_cast<std::uint64_t>(m) + 1)).rounded(wc);       
        nq[0] = BigFloat::sub(nq[0], BigFloat::div(hz_zeta_prime_neg(m, wc), mf, wc), wc);
        const BigFloat den = BigFloat::mul(mf, BigFloat(static_cast<std::uint64_t>(m) * (m + 1)), wc);
        BigUInt bin = BigUInt::one();                                                      

        for (unsigned i = 1; i <= m + 1; ++i) {
            bin.mul_small_mutable(m + 2 - i);
            bin.div_small_mutable(i);
            const BigFloat bb = pg_bern(m + 1 - i, wc);
            if (bb.is_zero()) continue;
            nq[i] = BigFloat::sub(nq[i], BigFloat::div(BigFloat::mul(bb, BigFloat(bin), wc), den, wc), wc);
        }

        q.swap(nq);
    }

    BigFloat     v  = g;
    std::int64_t hi = g.get_exp_base2();
    BigFloat     xp = BigFloat::one();

    for (std::size_t i = 0; i < q.size(); ++i) {
        const BigFloat term = BigFloat::mul(q[i], xp, wc);
        const std::int64_t et = term.get_exp_base2();
        if (et != BigFloat::exp_none && (hi == BigFloat::exp_none || et > hi)) hi = et;
        v  = BigFloat::add(v, term, wc);
        xp = BigFloat::mul(xp, x, wc);
    }

    const std::int64_t ev = v.get_exp_base2();
    r.v = v;
    r.cancelled = g0.cancelled;
    if (ev == BigFloat::exp_none)                 r.cancelled += wc.precision;
    else if (hi != BigFloat::exp_none && hi > ev) r.cancelled += static_cast<std::size_t>(hi - ev);
    return r;
}

BigFloat pg_neg_drive(unsigned M, const BigFloat& x, const BigFloatContext& ctx) {
    std::size_t       guard = 32;
    const double      T     = static_cast<double>(hz_target(ctx.precision + guard, M - 1));
    const std::size_t base  = static_cast<std::size_t>(std::log2(T)) + 24;
    std::size_t       extra = base + static_cast<std::size_t>(static_cast<double>(M) * std::log2(T + 1.0));   // expected shift-sum cancellation

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard + extra), RoundingMode::nearest_even);
        const pg_val_t g = pg_neg_eval(M, x, wc);
        if (!g.v.is_finite()) return g.v;

        if (g.cancelled + base > extra) {
            if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
            extra = g.cancelled + base;
            continue;
        }

        if (g.v.is_zero()) return g.v;
        const std::size_t acc = (wc.precision > extra + 8) ? (wc.precision - extra - 8) : 1;
        const std::size_t L   = g.v.significand().bit_length();
        const std::size_t err = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(g.v.significand(), ctx.precision, err + 2)) return g.v.rounded(ctx);
        if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
        guard *= 2;
    }
}

} // namespace pgdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat digamma(const BigFloat& z, const BigFloatContext& ctx) {
    if (z.is_nan())       return BigFloat::nan();
    if (z.is_undefined()) return BigFloat::undefined();
    if (z.is_infinite())  return z.signbit() ? BigFloat::undefined() : z;
    return pgdetail::pg_drive(0, z, ctx);
}

BigFloat trigamma(const BigFloat& z, const BigFloatContext& ctx) {
    if (z.is_nan())       return BigFloat::nan();
    if (z.is_undefined()) return BigFloat::undefined();
    if (z.is_infinite())  return z.signbit() ? BigFloat::undefined() : BigFloat::zero();
    return pgdetail::pg_drive(1, z, ctx);
}

BigFloat polygamma(const BigFloat& x, int n, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (n == 0) return digamma(x, ctx);
    if (n == 1) return trigamma(x, ctx);

    if (n == -1) {
        if (x.is_infinite()) return x.signbit() ? BigFloat::undefined() : x;
        if (gamma_sign(x, ctx) == 2) return BigFloat::infinity();      
        return log_gamma(x, ctx);
    }

    if (n < -1) return pgdetail::pg_neg_drive(static_cast<unsigned>(-n), x, ctx);
    if (x.is_infinite()) return x.signbit() ? BigFloat::undefined() : BigFloat::zero();
    return pgdetail::pg_drive(static_cast<unsigned>(n), x, ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
