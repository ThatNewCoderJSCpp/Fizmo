#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace hzdetail {

double hz_lsum(double a, double b) {
    if (a == hz_ninf()) return b;
    if (b == hz_ninf()) return a;
    if (a < b) std::swap(a, b);
    return a + std::log2(1.0 + std::exp2(b - a));
}

BigFloat hz_xmul(const BigFloat& x, const BigFloat& y) {
    if (x.is_zero() || y.is_zero()) return BigFloat::zero();
    const std::size_t p = x.significand().bit_length() + y.significand().bit_length() + 1;
    return BigFloat::mul(x, y, BigFloatContext(BigFloatContext::clamp_precision(p), RoundingMode::nearest_even));
}

BigFloat hz_xadd(const BigFloat& x, const BigFloat& y, bool sub) {
    if (y.is_zero()) return x;
    if (x.is_zero()) return sub ? -y : y;
    const std::int64_t hi = std::max(x.get_exp_base2(), y.get_exp_base2());
    const std::int64_t lo = std::min(x.exponent(), y.exponent());
    const BigFloatContext xc(BigFloatContext::clamp_precision(static_cast<std::size_t>(hi - lo) + 3), RoundingMode::nearest_even);
    return sub ? BigFloat::sub(x, y, xc) : BigFloat::add(x, y, xc);
}

bool hz_int64(const BigFloat& s, std::int64_t& k) {
    if (!s.is_finite()) return false;
    if (s.is_zero()) { k = 0; return true; }
    if (!s.get_fractional_part().is_zero() || s.get_exp_base2() > 61) return false;
    const BigInt i = s.get_integer_part();
    k = static_cast<std::int64_t>(i.get_lowest_bits());
    if (i.is_negative()) k = -k;
    return true;
}

bool hz_pow_exact(const BigFloat& a, std::int64_t k, const BigFloatContext& ctx, BigFloat& out) {
    if (k == 0) { out = BigFloat::one(); return true; }
    const std::uint64_t m    = (k < 0) ? static_cast<std::uint64_t>(-k) : static_cast<std::uint64_t>(k);
    const BigUInt&      A    = a.significand();
    const double        bits = static_cast<double>(m) * static_cast<double>(A.bit_length());
    if (bits > 16.0 * static_cast<double>(ctx.precision) + 65536.0 || bits + 256.0 >= static_cast<double>(BigUInt::max_bits)) return false;
    if (std::fabs(static_cast<double>(m) * static_cast<double>(a.exponent())) > 4.0e18) return false;
    const std::int64_t e  = static_cast<std::int64_t>(m) * a.exponent();
    const BigUInt      Am = hz_upow(A, m);
    out = (k > 0) ? BigFloat::div(BigFloat(BigUInt::one(), false, -e), BigFloat(Am), ctx) : BigFloat(Am, false, e).rounded(ctx);
    return true;
}

BigFloat hz_pow_neg(const BigFloat& a, const BigFloat& s, const BigFloatContext& ctx) {
    std::int64_t k = 0;
    BigFloat     out;
    if (hz_int64(s, k) && hz_pow_exact(a, k, ctx, out)) return out;
    const double x = std::fabs(ztdetail::zt_to_double(s)) * std::fabs(hz_l2(a)) * hz_ln2;
    return hz_drive(hz_guard(std::log2(x + 1.0)), ctx, [&](std::size_t want) -> zt_val {
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        return zt_val{exp(-BigFloat::mul(s, ln(a, wc), wc), wc), std::log2(3.0 + 3.0 * x) + 1.0};
    });
}

bool hz_neg_int_exact(std::uint64_t m, const BigFloat& a, const BigFloatContext& ctx, BigFloat& out) {
    const std::uint64_t n  = m + 1;
    if (n / 2 > constants::detail::bn_index_cap) return false;
    const double La   = static_cast<double>(a.significand().bit_length()) + std::fabs(static_cast<double>(a.exponent()));
    const double bits = static_cast<double>(n) * (La + 2.0 * std::log2(static_cast<double>(n) + 1.0));
    if (bits > 16.0 * static_cast<double>(ctx.precision) + 65536.0) return false;
    std::vector<constants::BigBernoulliNumber> B(static_cast<std::size_t>(n) + 1);
    BigUInt L = BigUInt::one();

    for (std::uint64_t k = 0; k <= n; ++k) {
        if (k > 1 && (k & 1u)) continue;
        B[k] = constants::bernoulli_number(k);
        if (!B[k].is_valid()) return false;
        const BigUInt& d = B[k].denominator();
        L = (L / constants::detail::bn_gcd(L, d)) * d;
    }

    BigFloat acc = BigFloat::zero();
    BigFloat ap  = BigFloat::one();                                     
    BigUInt  C   = BigUInt::one();                                      

    for (std::uint64_t k = n; ; --k) {
        if ((k <= 1 || !(k & 1u)) && !B[k].is_zero()) {
            BigUInt mag = C * B[k].numerator().magnitude();
            mag = mag * (L / B[k].denominator());
            acc = hz_xadd(acc, hz_xmul(BigFloat(mag, B[k].is_negative(), 0), ap));
        }

        if (k == 0) break;
        ap = hz_xmul(ap, a);
        C.mul_small_mutable(k);
        C.div_small_mutable(n - k + 1);
    }

    if (acc.is_zero()) { out = BigFloat::zero(); return true; }
    BigUInt den = L;
    den.mul_small_mutable(n);
    out = BigFloat::div(-acc, BigFloat(den), ctx);
    return true;
}

hz_sinfo hz_info(const BigFloat& s) {
    hz_sinfo si;
    si.sd = ztdetail::zt_to_double(s);

    if (si.sd <= 0.5 && si.sd > -1.0e15) {
        const double r = std::floor(-si.sd + 0.5);

        if (r >= 0.0) {
            si.i0 = static_cast<std::int64_t>(r);
            const BigFloat t = hz_xadd(s, BigFloat(static_cast<std::uint64_t>(r)));
            si.lnear = hz_l2(t);
        }
    }

    return si;
}

hz_plan hz_choose(const hz_sinfo& si, double la, std::size_t want) {
    const double        l2pi = 2.651496129472319;
    const double        sd   = si.sd;
    const double        wd   = static_cast<double>(want);
    const std::uint64_t Mcap = std::min<std::uint64_t>(constants::detail::bn_index_cap, 4 * want + 64 + ((sd < 0.0) ? static_cast<std::uint64_t>(-sd) : 0));
    hz_plan       best;
    double        best_cost = std::numeric_limits<double>::infinity();
    double        lpoch     = 0.0;
    std::uint64_t idx       = 0;

    for (std::uint64_t M = 1; M <= Mcap; ++M) {
        for (; idx < 2 * M; ++idx) {
            lpoch += (si.i0 >= 0 && idx == static_cast<std::uint64_t>(si.i0)) ? si.lnear : std::log2(std::fabs(sd + static_cast<double>(idx)));
        }

        const double den = sd + 2.0 * static_cast<double>(M) - 1.0;
        if (den < 0.5) continue;
        const bool   zeroR = (lpoch == hz_ninf());
        const double C     = zeroR ? 0.0 : 2.0 + lpoch - 2.0 * static_cast<double>(M) * l2pi - std::log2(den);
        double lX = la;

        if (!zeroR) {
            lX = (sd >= 0.0) ? (C + sd * la + wd + 6.0) / den : (C + std::log2(std::fabs(sd - 1.0)) + wd + 6.0) / (2.0 * static_cast<double>(M));
        }

        double Nd;
        if (lX <= la)      Nd = 0.0;
        else if (lX > 62.) Nd = std::numeric_limits<double>::infinity();
        else               Nd = std::ceil(std::exp2(lX) - std::exp2(la));

        if (!(Nd <= 67108864.0)) continue;
        const std::uint64_t N  = static_cast<std::uint64_t>(Nd);
        const double        lx = (N == 0) ? la : hz_ln_add(la, N) / hz_ln2;
        const double        lR = zeroR ? hz_ninf() : C + (1.0 - sd - 2.0 * static_cast<double>(M)) * lx;
        const double        cost = static_cast<double>(N) + 0.1 * static_cast<double>(M);

        if (cost < best_cost) {
            best.N = N; best.M = M; best.log2R = lR; best.ok = true;
            best_cost = cost;
        } else if (best.ok && M > best.M + 64) {
            break;
        }
    }

    return best;
}

zt_val hz_em(const BigFloat& s, const BigFloat& om, const BigFloat& a, const hz_plan& pl, std::size_t want) {
    using gmdetail::gm_zeta_t;
    const double          l2pi = 2.651496129472319;                                     
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + ztdetail::zt_bitlen(pl.N + pl.M) + 8), RoundingMode::nearest_even);
    const double          sd = ztdetail::zt_to_double(s);
    BigFloat S1 = BigFloat::zero();

    for (std::uint64_t k = 0; k < pl.N; ++k) {
        S1 = BigFloat::add(S1, exp(-BigFloat::mul(s, ln(hz_xadd(a, BigFloat(k)), wc), wc), wc), ac);
    }

    const BigFloat X  = hz_xadd(a, BigFloat(pl.N));
    const BigFloat lX = ln(X, wc);
    const BigFloat Q  = exp(BigFloat::mul(om, lX, wc), wc);                           
    const BigFloat I  = BigFloat::div(Q, -om, wc);                                    
    const BigFloat u  = X.reciprocal(wc);
    const BigFloat P  = BigFloat::mul(Q, u, wc);                                      
    const BigFloat H  = P.scaled_pow2(-1);
    const BigFloat u2 = BigFloat::mul(u, u, wc);
    const BigFloat tp = constants::two_pi(wc);
    const BigFloat cz = BigFloat::div(u2, BigFloat::mul(tp, tp, wc), wc);             
    const double   lXd = hz_l2(X) * hz_ln2;
    const double   eQ  = 2.0 + 3.0 * std::fabs(1.0 - sd) * std::fabs(lXd);
    const double   eP  = eQ + 3.0;
    const std::uint64_t      M     = pl.M;
    const std::size_t        G     = 32 + ztdetail::zt_bitlen(M);
    const double             lhead = std::max(hz_l2(S1), std::max(hz_l2(I), hz_l2(H)));
    const double             l2X   = hz_l2(X);
    std::vector<std::size_t> prec(static_cast<std::size_t>(M) + 2, want);
    std::uint64_t            jz    = M + 1;
    double lg = 1.0 + hz_l2(s) + hz_l2(P) - l2X - 2.0 * l2pi;                          

    for (std::uint64_t j = 1; j <= M; ++j) {
        const double drop = (lg == hz_ninf()) ? static_cast<double>(want) : std::max(0.0, lhead - lg);
        double p = static_cast<double>(want + G) - drop;
        if (p < 64.0) p = 64.0;
        if (p > static_cast<double>(want)) p = static_cast<double>(want);
        prec[j] = static_cast<std::size_t>(p);
        if (jz > M && j >= gmdetail::gm_zeta_jmin && prec[j] + 32 <= 2 * j * gmdetail::gm_zeta_kbits) jz = j;
        lg += std::log2(std::fabs(sd + 2.0 * j - 1.0)) + std::log2(std::fabs(sd + 2.0 * j)) - 2.0 * (l2pi + l2X);
    }

    prec[M + 1] = prec[M];
    constants::detail::tangent_reserve(static_cast<std::size_t>(std::min(jz, M)) + 1);

    BigFloat  r     = BigFloat::mul(BigFloat::mul(s, P, wc), u, wc).scaled_pow2(-1);  
    BigFloat  g     = BigFloat::zero();                                               
    gm_zeta_t zs;
    bool      zon   = false;
    BigFloat  T     = BigFloat::zero();
    double    lTabs = hz_ninf();
    double    errT  = hz_ninf();                                                      
    double    lrel  = std::log2(eP + 2.0) - static_cast<double>(want);                

    for (std::uint64_t j = 1; j <= M; ++j) {
        const std::size_t     pj = prec[j];
        const BigFloatContext tc(BigFloatContext::clamp_precision(pj), RoundingMode::nearest_even);
        const double          lp = -static_cast<double>(pj);
        BigFloat term;

        if (!zon && j >= jz) {
            if (r.is_zero()) break;
            const std::uint64_t u2j = 2 * j;
            const BigFloat      f   = BigFloat(gmdetail::fact_range(1, u2j + 1)).rounded(tc);                   // (2j)!
            g    = BigFloat::div(BigFloat::mul(r, f, tc).scaled_pow2(1), gmdetail::gm_powi(constants::two_pi(tc), u2j, tc), tc);
            lrel = hz_lsum(lrel, std::log2(static_cast<double>(u2j) + 2.0 * std::log2(static_cast<double>(u2j)) + 4.0) + lp);
            gmdetail::gm_zeta_init(zs, j, pj + 32);
            zon  = true;
        }

        if (zon) {
            if (g.is_zero()) break;
            term = BigFloat::mul(g, gmdetail::gm_zeta_value(zs, tc), tc);
            if (j % 2 == 0) term = -term;                                               
        } else {
            if (r.is_zero()) break;
            term = BigFloat::mul(constants::bernoulli_float(static_cast<std::size_t>(2 * j), tc), r, tc);
        }

        T     = BigFloat::add(T, term, ac);
        lTabs = hz_lsum(lTabs, hz_l2(term));
        errT  = hz_lsum(errT, hz_l2(term) + hz_lsum(lrel, 2.0 + lp));                  
        if (j == M) break;

        const std::size_t     pn = prec[j + 1];
        const BigFloatContext nc(BigFloatContext::clamp_precision(pn), RoundingMode::nearest_even);
        const BigFloat        f  = BigFloat::mul(hz_xadd(s, BigFloat(2 * j - 1)), hz_xadd(s, BigFloat(2 * j)), nc);

        if (zon) {
            g = BigFloat::mul(BigFloat::mul(g, f, nc), cz.rounded(nc), nc);
            gmdetail::gm_zeta_step(zs, pn + 32);
        } else {
            r = BigFloat::mul(BigFloat::mul(r, f, nc), u2.rounded(nc), nc);
            r = BigFloat::div(r, BigFloat((2 * j + 1) * (2 * j + 2)), nc);
        }

        lrel = hz_lsum(lrel, std::log2(6.0) - static_cast<double>(pn));
        lrel = hz_lsum(lrel, std::log2(6.0) - static_cast<double>(want));
    }

    BigFloat v = BigFloat::add(S1, I, ac);
    v = BigFloat::add(v, H, ac);
    v = BigFloat::add(v, T, ac).rounded(wc);
    if (v.is_zero() || !v.is_finite()) return zt_val{v, 1.0e9};
    const double lv    = hz_l2(v);
    const double wd    = static_cast<double>(want);
    const double e1    = 2.0 + 3.0 * std::fabs(sd) * std::max(std::fabs(hz_l2(a) * hz_ln2), std::fabs(lXd));
    const double units = std::exp2(hz_l2(S1) - lv) * (e1 + 1.0)
                       + std::exp2(hz_l2(I)  - lv) * (eQ + 2.0)
                       + std::exp2(hz_l2(H)  - lv) * (eP + 1.0)
                       + std::exp2(errT + wd - lv)
                       + std::exp2(lTabs     - lv)
                       + std::exp2(pl.log2R + wd - lv) + 1.0;
    return zt_val{v, std::log2(units) + 1.0};
}

zt_val hz_eval(const BigFloat& s, const BigFloat& om, const BigFloat& a, const hz_sinfo& si, std::size_t want) {
    const hz_plan pl = hz_choose(si, hz_l2(a), want);
    if (!pl.ok) return zt_val{BigFloat::undefined(), 0.0};
    return hz_em(s, om, a, pl, want);
}

const std::vector<BigUInt>& hz_eulerian(std::size_t m) {
    static thread_local std::vector<std::vector<BigUInt>> rows(1);

    while (rows.size() <= m) {
        const std::size_t    j = rows.size();
        std::vector<BigUInt> nr(j, BigUInt::zero());

        if (j == 1) {
            nr[0] = BigUInt::one();
        } else {
            const std::vector<BigUInt>& pr = rows[j - 1];

            for (std::size_t i = 0; i < j; ++i) {                                  
                if (i + 1 < j) { BigUInt t = pr[i];     t.mul_small_mutable(i + 1); nr[i] = t; }
                if (i >= 1)    { BigUInt t = pr[i - 1]; t.mul_small_mutable(j - i); nr[i].add_mutable(t); }
            }
        }

        rows.push_back(std::move(nr));
    }

    return rows[m];
}

bool lp_rational(const BigFloat& z, std::uint64_t m, const BigFloat& a, const BigFloatContext& ctx, BigFloat& out) {
    const BigFloat one = BigFloat::one();
    const BigFloat omz = hz_xadd(one, z, true);
    if (omz.is_zero() || m > 2048) return false;
    auto span = [](const BigFloat& x) {
        return x.is_zero() ? 0.0 : static_cast<double>(x.significand().bit_length()) + std::fabs(static_cast<double>(x.exponent()));
    };
    const double bits = static_cast<double>(m + 1) * (span(z) + span(a) + span(omz) + std::log2(static_cast<double>(m) + 2.0));
    if (bits > 8.0 * static_cast<double>(ctx.precision) + 16384.0) return false;
    const std::size_t     M = static_cast<std::size_t>(m);
    std::vector<BigFloat> d(M + 1), zp(M + 1, one), op(M + 2, one);
    for (std::size_t i = 1; i <= M; ++i)     zp[i] = hz_xmul(zp[i - 1], z);
    for (std::size_t i = 1; i <= M + 1; ++i) op[i] = hz_xmul(op[i - 1], omz);

    for (std::size_t i = 0; i <= M; ++i) {                             // f(i) = (i + a)^m, exact
        BigFloat p = one;
        BigFloat b = hz_xadd(a, BigFloat(static_cast<std::uint64_t>(i)));

        for (std::uint64_t e = m; e != 0; e >>= 1) {
            if (e & 1u) p = hz_xmul(p, b);
            if (e > 1)  b = hz_xmul(b, b);
        }

        d[i] = p;
    }

    for (std::size_t j = 1; j <= M; ++j) {                             
        for (std::size_t i = M; i >= j; --i) d[i] = hz_xadd(d[i], d[i - 1], true);
    }

    BigFloat P = BigFloat::zero();
    for (std::size_t j = 0; j <= M; ++j) P = hz_xadd(P, hz_xmul(d[j], hz_xmul(zp[j], op[M - j])));
    out = BigFloat::div(P, op[M + 1], ctx);
    return true;
}

lp_dplan lp_plan_direct(double Ld, double sd, double la, std::size_t want, std::uint64_t cap) {
    lp_dplan p;

    for (std::uint64_t n = 0; n <= cap; ++n) {
        const double lt = (static_cast<double>(n) * Ld - sd * hz_ln_add(la, n)) / hz_ln2;
        const double q  = (sd >= 0.0) ? std::exp(Ld) : std::exp(Ld - sd * (hz_ln_add(la, n + 1) - hz_ln_add(la, n)));
        if (lt > p.lmax) p.lmax = lt;

        if (q < 1.0) {
            const double tail = lt - std::log2(1.0 - q);
            if (tail <= p.lmax - static_cast<double>(want) - 4.0) { p.N = n; p.ltail = tail; p.ok = true; return p; }
        }

        p.labs = hz_lsum(p.labs, lt);
    }

    return p;
}

BigFloat lp_term(const BigFloat& L, const BigFloat& s, const BigFloat& a, std::uint64_t n, const BigFloatContext& wc) {
    return exp(BigFloat::sub(BigFloat::mul(BigFloat(n), L, wc), BigFloat::mul(s, ln(hz_xadd(a, BigFloat(n)), wc), wc), wc), wc);
}

zt_val lp_direct(const BigFloat& L, bool neg, const BigFloat& s, const BigFloat& a, std::size_t want) {
    const double   Ld = ztdetail::zt_to_double(L);
    const double   sd = ztdetail::zt_to_double(s);
    const double   la = hz_l2(a);
    const lp_dplan p  = lp_plan_direct(Ld, sd, la, want, lp_term_cap);
    if (!p.ok) return zt_val{BigFloat::undefined(), 0.0};
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + ztdetail::zt_bitlen(p.N) + 8), RoundingMode::nearest_even);
    BigFloat S = BigFloat::zero();

    for (std::uint64_t n = 0; n < p.N; ++n) {
        const BigFloat t = lp_term(L, s, a, n, wc);
        S = (neg && (n & 1u)) ? BigFloat::sub(S, t, ac) : BigFloat::add(S, t, ac);
    }

    const BigFloat v = S.rounded(wc);
    if (v.is_zero()) return zt_val{v, 1.0e9};
    const double lv    = hz_l2(v);
    const double ce    = 4.0 + 4.0 * static_cast<double>(p.N) * std::fabs(Ld) + 3.0 * std::fabs(sd) * std::max(std::fabs(hz_ln_add(la, 0)), std::fabs(hz_ln_add(la, p.N)));
    const double units = std::exp2(p.labs - lv) * (ce + 1.0) + std::exp2(p.ltail + static_cast<double>(want) - lv) + 1.0;
    return zt_val{v, std::log2(units) + 1.0};
}

zt_val lp_alt(const BigFloat& L, const BigFloat& s, const BigFloat& a, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const double          Ld = ztdetail::zt_to_double(L);
    const double          sd = ztdetail::zt_to_double(s);
    const double          la = hz_l2(a);
    const std::uint64_t   n  = static_cast<std::uint64_t>(std::ceil((static_cast<double>(want) + 2.0) / 2.5431)) + 1;
    const BigFloat        t0 = lp_term(L, s, a, 0, wc);
    BigUInt c  = BigUInt::one();
    BigUInt dn = BigUInt::one();

    for (std::uint64_t i = 1; i <= n; ++i) {
        ztdetail::zt_borwein_step(c, n, i);
        dn.add_mutable(c);
    }

    const std::int64_t F = static_cast<std::int64_t>(want + ztdetail::zt_bitlen(n)) + 3 - (static_cast<std::int64_t>(dn.bit_length()) - 1) - static_cast<std::int64_t>(std::floor(hz_l2(t0)));
    const double l0 = -sd * hz_ln_add(la, 0) / hz_ln2;
    BigInt  A(static_cast<std::int64_t>(0));
    BigUInt dk = BigUInt::one();
    double  bs = 0.0;
    c = BigUInt::one();

    for (std::uint64_t k = 0; k < n; ++k) {
        const BigFloat t = (k == 0) ? t0 : lp_term(L, s, a, k, wc);
        BigUInt e = dn;
        e.sub_mutable(dk);
        const BigInt q = BigFloat::mul(BigFloat(e), t, wc).scaled_pow2(F).get_integer_part();
        if (k & 1u) A.sub_mutable(q); else A.add_mutable(q);
        ztdetail::zt_borwein_step(c, n, k + 1);
        dk.add_mutable(c);
        bs += std::exp2((static_cast<double>(k) * Ld - sd * hz_ln_add(la, k)) / hz_ln2 - l0);
    }

    const BigFloat v  = BigFloat::div(BigFloat(A).scaled_pow2(-F), BigFloat(dn), wc);
    const double   ce = 4.0 + 3.0 * (static_cast<double>(n) * std::fabs(Ld) + std::fabs(sd) * std::max(std::fabs(hz_ln_add(la, 0)), std::fabs(hz_ln_add(la, n))));
    return zt_val{v, std::log2(2.0 * (0.25 + 1.01 * bs * ce) + 0.5 + 2.0 + 1.0) + 1.0};
}

zt_val lp_halving(const BigFloat& L, const BigFloat& s, const BigFloat& om, const BigFloat& a, std::uint64_t k, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloatContext ac(BigFloatContext::clamp_precision(want + 8), RoundingMode::nearest_even);
    const double          sd = ztdetail::zt_to_double(s);
    const BigFloat        lc = BigFloat::mul(om, constants::ln2(wc), wc);
    auto coef  = [&](std::uint64_t i) { return exp(BigFloat::mul(BigFloat(i), lc, wc), wc); };
    auto ecoef = [&](std::uint64_t i) { return 4.0 + 3.0 * static_cast<double>(i) * std::fabs(1.0 - sd) * hz_ln2; };
    std::vector<double> lp, up;
    const zt_val b = lp_direct(L.scaled_pow2(static_cast<std::int64_t>(k)), false, s, a.scaled_pow2(-static_cast<std::int64_t>(k)), want);
    if (!b.v.is_finite()) return b;
    const BigFloat pk = BigFloat::mul(coef(k), b.v, wc);
    BigFloat v = pk;
    lp.push_back(hz_l2(pk));
    up.push_back(std::exp2(b.lost) + ecoef(k) + 1.0);

    for (std::uint64_t i = 0; i < k; ++i) {
        const zt_val A = lp_alt(L.scaled_pow2(static_cast<std::int64_t>(i)), s, a.scaled_pow2(-static_cast<std::int64_t>(i)), want);
        if (!A.v.is_finite()) return A;
        const BigFloat p = BigFloat::mul(coef(i), A.v, wc);
        v = BigFloat::sub(v, p, ac);
        lp.push_back(hz_l2(p));
        up.push_back(std::exp2(A.lost) + ecoef(i) + 1.0);
    }

    v = v.rounded(wc);
    if (v.is_zero()) return zt_val{v, 1.0e9};
    const double lv = hz_l2(v);
    double units = 1.0;
    for (std::size_t i = 0; i < lp.size(); ++i) units += std::exp2(lp[i] - lv) * up[i];
    return zt_val{v, std::log2(units) + 1.0};
}

} // namespace hzdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat hurwitz_zeta(const BigFloat& s, const BigFloat& a, const BigFloatContext& ctx) {
    using namespace hzdetail;
    if (s.is_nan()       || a.is_nan())       return BigFloat::nan();
    if (s.is_undefined() || a.is_undefined()) return BigFloat::undefined();
    if (a.is_zero() || a.signbit())           return BigFloat::nan();
    const BigFloat one = BigFloat::one();
    if (a.is_infinite()) return (BigFloat::compare(s, one) == BigFloat::ordering::greater) ? BigFloat::zero() : BigFloat::undefined();

    if (s.is_infinite()) {
        if (s.signbit()) return BigFloat::undefined();
        const BigFloat::ordering c = BigFloat::compare(a, one);
        return (c == BigFloat::ordering::less) ? BigFloat::infinity() : (c == BigFloat::ordering::equal) ? one : BigFloat::zero();
    }

    if (BigFloat::compare(a, one) == BigFloat::ordering::equal) return riemann_zeta(s, ctx);
    if (BigFloat::compare(s, one) == BigFloat::ordering::equal) return BigFloat::undefined();          // pole
    std::int64_t k    = 0;
    const bool   sint = hz_int64(s, k);
    BigFloat     out;
    if (sint && k <= 0 && hz_neg_int_exact(static_cast<std::uint64_t>(-k), a, ctx, out)) return out;

    if (sint && k >= 2 && a.significand().is_one() && std::fabs(static_cast<double>(k) * static_cast<double>(a.exponent())) < 4.0e18) {
        const double ad = std::exp2(hz_l2(a));
        const double ld = static_cast<double>(k) * std::log2(ad / (ad + 1.0)) + std::log2(1.0 + (ad + 1.0) / (static_cast<double>(k) - 1.0));

        if (ld < -static_cast<double>(ctx.precision) - 3.0) {
            const BigFloat base = one.scaled_pow2(-k * a.exponent());
            const BigFloatContext hc(BigFloatContext::clamp_precision(ctx.precision + 16), RoundingMode::nearest_even);
            return BigFloat::add(base, base.scaled_pow2(-static_cast<std::int64_t>(ctx.precision) - 4), hc).rounded(ctx);
        }
    }

    const BigFloat om = ztdetail::zt_one_minus(s);
    const hz_sinfo si = hz_info(s);
    return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) { return hz_eval(s, om, a, si, want); });
}

BigFloat lerch_transcendent(const BigFloat& z, const BigFloat& s, const BigFloat& a, const BigFloatContext& ctx) {
    using namespace hzdetail;
    if (z.is_nan()       || s.is_nan()       || a.is_nan())       return BigFloat::nan();
    if (z.is_undefined() || s.is_undefined() || a.is_undefined()) return BigFloat::undefined();
    if (a.is_zero() || a.signbit())                              return BigFloat::nan();
    if (a.is_infinite() || z.is_infinite())                      return BigFloat::undefined();
    const BigFloat           one = BigFloat::one();
    const BigFloat::ordering cz  = BigFloat::compare(z.abs(), one);

    if (s.is_infinite()) {
        if (s.signbit() || cz == BigFloat::ordering::greater) return BigFloat::undefined();
        const BigFloat::ordering c = BigFloat::compare(a, one);
        return (c == BigFloat::ordering::less) ? BigFloat::infinity() : (c == BigFloat::ordering::equal) ? one : BigFloat::zero();
    }

    if (BigFloat::compare(z, one) == BigFloat::ordering::equal) return hurwitz_zeta(s, a, ctx);
    std::int64_t k    = 0;
    const bool   sint = hz_int64(s, k);
    BigFloat     out;
    if (sint && k <= 0 && lp_rational(z, static_cast<std::uint64_t>(-k), a, ctx, out)) return out;
    if (z.is_zero()) return hz_pow_neg(a, s, ctx);
    if (cz == BigFloat::ordering::greater) return BigFloat::undefined();
    const bool     neg  = z.signbit();
    const bool     spos = !s.signbit() && !s.is_zero();
    const double   sd   = ztdetail::zt_to_double(s);
    const BigFloat om   = ztdetail::zt_one_minus(s);

    if (cz == BigFloat::ordering::equal) {                                                            // z = -1
        if (spos) return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) { return lp_alt(BigFloat::zero(), s, a, want); });
        const hz_sinfo si = hz_info(s);
        const BigFloat a1 = a.scaled_pow2(-1);
        const BigFloat a2 = hz_xadd(a, one).scaled_pow2(-1);
        return hz_drive(ztdetail::zt_guard0(s), ctx, [&](std::size_t want) -> zt_val {            // 2^-s [zeta(s, a/2) - zeta(s, (a+1)/2)]
            const zt_val z1 = hz_eval(s, om, a1, si, want);
            if (!z1.v.is_finite()) return z1;
            const zt_val z2 = hz_eval(s, om, a2, si, want);
            if (!z2.v.is_finite()) return z2;
            const BigFloatContext wc(want, RoundingMode::nearest_even);
            const BigFloat d = BigFloat::sub(z1.v, z2.v, wc);
            const BigFloat v = BigFloat::mul(exp(-BigFloat::mul(s, constants::ln2(wc), wc), wc), d, wc);
            if (v.is_zero()) return zt_val{v, 1.0e9};
            const double ld    = hz_l2(d);
            const double units = std::exp2(hz_l2(z1.v) - ld + z1.lost) + std::exp2(hz_l2(z2.v) - ld + z2.lost) + 4.0 + 3.0 * std::fabs(sd) * hz_ln2;
            return zt_val{v, std::log2(units) + 1.0};
        });
    }

    enum class meth { direct, alt, halving };
    const double  Ld0   = hz_l2(z) * hz_ln2;
    const double  la    = hz_l2(a);
    meth          m     = meth::direct;
    std::uint64_t lev   = 0;
    double        extra = 0.0;

    if (spos && neg && Ld0 > -2.0794415416798357) {                                                  
        m = meth::alt;
    } else if (spos && !neg && Ld0 > -hz_ln2) {                                                      
        while (std::ldexp(std::fabs(Ld0), static_cast<int>(lev)) < hz_ln2) ++lev;
        const std::size_t w0 = ctx.precision + 32;
        const double      wh = static_cast<double>(w0) + static_cast<double>(lev) * sd;
        const double      ch = static_cast<double>(lev) * 0.4 * wh + 1.5 * wh;
        const lp_dplan    dp = lp_plan_direct(Ld0, sd, la, w0, static_cast<std::uint64_t>(std::min(ch, 1.0e9)));
        if (!dp.ok) { m = meth::halving; extra = static_cast<double>(lev) * sd * 1.05; }
    }

    return hz_drive(hz_guard(extra), ctx, [&](std::size_t want) -> zt_val {
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        const BigFloat        L = ln(z.abs(), wc);
        switch (m) {
            case meth::alt:     return lp_alt(L, s, a, want);
            case meth::halving: return lp_halving(L, s, om, a, lev, want);
            default:            return lp_direct(L, neg, s, a, want);
        }
    });
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
