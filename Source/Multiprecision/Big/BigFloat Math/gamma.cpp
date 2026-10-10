#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace gmdetail {

double gm_log2_fine(const BigFloat& x) {
    if (!x.is_finite() || x.is_zero()) return 0.0;
    const BigUInt&    m    = x.significand();
    const std::size_t L    = m.bit_length();
    const std::size_t take = (L < 53) ? L : 53;
    double f = 0.0;
    for (std::size_t i = 1; i <= take; ++i) f = f * 2.0 + (m.get_bit(L - i) ? 1.0 : 0.0);
    return std::log2(f) + static_cast<double>(L - take) + static_cast<double>(x.exponent());
}

bool gm_half_odd(const BigFloat& z, std::uint64_t& n) {
    if (!z.is_finite() || z.is_zero() || z.signbit()) return false;
    const BigUInt&  m  = z.significand();
    const long long tz = m.count_trailing_zeros();
    if (z.exponent() + static_cast<std::int64_t>(tz) != -1) return false;
    const std::size_t L = m.bit_length() - static_cast<std::size_t>(tz);
    if (L > 63) return false;
    std::uint64_t k = 0;
    for (std::size_t i = L; i > 0; --i) k = (k << 1) | (m.get_bit(static_cast<std::size_t>(tz) + i - 1) ? 1u : 0u);
    n = k >> 1;
    return true;
}

BigFloat gm_powi(BigFloat b, std::uint64_t e, const BigFloatContext& wc) {
    BigFloat r = BigFloat::one();

    while (e != 0) {
        if (e & 1u) r = BigFloat::mul(r, b, wc);
        e >>= 1;
        if (e != 0) b = BigFloat::mul(b, b, wc);
    }

    return r;
}

BigUInt gm_powi_u(std::uint64_t k, std::uint64_t e) {
    BigUInt r = BigUInt::one();
    BigUInt b(k);

    while (e != 0) {
        if (e & 1u) r = r * b;
        e >>= 1;
        if (e != 0) b = b * b;
    }

    return r;
}

double gm_term_log2(std::size_t j, double lx) {
    const double l2    = 0.6931471805599453;
    const double lg2pi = 2.6514961294723187;
    const double u     = 2.0 * static_cast<double>(j);
    return 1.0 + std::lgamma(u - 1.0) / l2 - u * lg2pi - (u - 1.0) * lx;
}

std::size_t gm_term_prec(std::size_t wprec, std::int64_t eh, double lt, std::size_t G) {
    const double drop = static_cast<double>(eh) - lt;
    double p = static_cast<double>(wprec + G) - ((drop > 0.0) ? drop : 0.0);
    if (p < 64.0) p = 64.0;
    if (p > static_cast<double>(wprec)) p = static_cast<double>(wprec);
    return static_cast<std::size_t>(p);
}

void gm_zeta_init(gm_zeta_t& zs, std::uint64_t j, std::size_t W) {
    zs.p.clear();
    zs.W = W;
    const BigUInt top = constants::bfdetail::unit(W);

    for (std::uint64_t k = 2; ; ++k) {
        const BigUInt d = gm_powi_u(k, 2 * j);
        if (d.bit_length() > W) break;
        zs.p.push_back(top / d);
    }
}

void gm_zeta_step(gm_zeta_t& zs, std::size_t Wnext) {
    if (zs.W > Wnext + 64) {
        const std::size_t d = zs.W - Wnext;
        for (BigUInt& v : zs.p) v.shift_right_mutable(d);
        zs.W = Wnext;
    }

    for (std::size_t i = 0; i < zs.p.size(); ++i) {
        const std::uint64_t k = static_cast<std::uint64_t>(i) + 2;
        zs.p[i].div_small_mutable(k * k);
    }

    while (!zs.p.empty() && zs.p.back().is_zero()) zs.p.pop_back();
}

BigFloat gm_zeta_value(const gm_zeta_t& zs, const BigFloatContext& tc) {
    BigUInt s = BigUInt::zero();
    for (const BigUInt& v : zs.p) s.add_mutable(v);
    if (s.is_zero()) return BigFloat::one();
    return BigFloat::add(BigFloat::one(), BigFloat(s, false, -static_cast<std::int64_t>(zs.W)), tc);
}

BigFloat gm_cached_const(gm_const_cache_t& c, const BigFloatContext& wc, BigFloat (*compute)(const BigFloatContext&)) {
    if (c.prec < wc.precision + 16) {
        const std::size_t want = wc.precision + 64;
        const std::size_t grow = c.prec + c.prec / 2;       
        const BigFloatContext hc(BigFloatContext::clamp_precision(want > grow ? want : grow), RoundingMode::nearest_even);
        c.v    = compute(hc);
        c.prec = hc.precision;
    }

    return c.v.rounded(wc);
}

BigFloat gm_stirling_coeff(std::size_t j, const BigFloatContext& tc) {
    if (j == 0 || j > constants::detail::bn_index_cap) return BigFloat::undefined();
    std::vector<gm_bcoef_t>& cache = gm_bcoef_cache();
    if (cache.size() <= j) cache.resize(j + 1);
    gm_bcoef_t& e = cache[j];

    if (e.prec < tc.precision + 8) {
        const BigFloatContext hc(BigFloatContext::clamp_precision(tc.precision + 32), RoundingMode::nearest_even);
        const BigUInt num = constants::tangent_number(j);
        if (num.is_undefined()) return BigFloat::undefined();
        BigUInt den = constants::bfdetail::unit(2 * j);
        den.sub_small_mutable(1);
        den.mul_small_mutable(static_cast<std::uint64_t>(2 * j - 1));
        e.v = BigFloat::div(BigFloat(num), BigFloat(den), hc).scaled_pow2(-2 * static_cast<std::int64_t>(j)).with_sign(j % 2 == 0);
        e.prec = hc.precision;
    }

    return e.v.rounded(tc);
}

void gm_block_poly(std::uint64_t k, std::size_t s, std::vector<BigUInt>& c) {
    c.assign(s + 1, BigUInt::zero());
    c[0] = BigUInt::one();

    for (std::size_t i = 0; i < s; ++i) {
        const std::uint64_t a = k + i;

        for (std::size_t j = i + 1; j > 0; --j) {
            c[j].mul_small_mutable(a);
            c[j].add_mutable(c[j - 1]);
        }

        c[0].mul_small_mutable(a);
    }
}

bool gm_shift_count(const BigFloat& z, std::uint64_t T, std::uint64_t& m) {
    m = 0;
    if (BigFloat::compare(z, BigFloat(T)) != BigFloat::ordering::less) return true;
    const BigInt ip = z.get_integer_part();
    std::uint64_t fl = 0;
    if (ip.is_undefined() || !gm_exact_u64(BigFloat(ip), fl) || fl >= T) return false;
    m = T - fl;
    return true;
}

BigFloat gm_rising_rs(const BigFloat& z, std::uint64_t m, const BigFloatContext& wc) {
    if (m == 0) return BigFloat::one();
    const std::size_t S  = gm_rising_block;
    const std::size_t Sp = (m < S) ? static_cast<std::size_t>(m) : S;
    std::vector<BigFloat> zp(Sp + 1, BigFloat::one());
    zp[1] = z;
    for (std::size_t j = 2; j <= Sp; ++j) zp[j] = BigFloat::mul(zp[j - 1], z, wc);
    BigFloat             R = BigFloat::one();
    std::vector<BigUInt> c;

    for (std::uint64_t k = 0; k < m; ) {
        const std::size_t s = (m - k < Sp) ? static_cast<std::size_t>(m - k) : Sp;
        gm_block_poly(k, s, c);
        BigFloat B = zp[s];                                        

        for (std::size_t j = 0; j < s; ++j) {
            if (c[j].is_zero()) continue;
            B = BigFloat::add(B, BigFloat::mul(zp[j], BigFloat(c[j]), wc), wc);
        }

        R = BigFloat::mul(R, B, wc);
        if (!R.is_finite()) return R;
        k += s;
    }

    return R;
}

BigUInt fact_range(std::uint64_t a, std::uint64_t b) {
    if (b <= a) return BigUInt::one();
    const std::uint64_t n = b - a;

    if (n <= 16) {
        BigUInt       r   = BigUInt::one();
        std::uint64_t acc = 1;

        for (std::uint64_t k = a; k < b; ++k) {
            if (acc <= (std::numeric_limits<std::uint64_t>::max)() / k) {
                acc *= k;
            } else {
                r.mul_small_mutable(acc);
                acc = k;
            }
        }

        r.mul_small_mutable(acc);
        return r;
    }

    const std::uint64_t m = a + n / 2;
    return fact_range(a, m) * fact_range(m, b);
}

std::uint64_t fact_n_cap() {
    static const std::uint64_t c = []() -> std::uint64_t {
        const double avail = static_cast<double>((BigUInt::max_bits > 256) ? (BigUInt::max_bits - 256) : 1);
        double lo = 1.0, hi = 1.0e12;

        for (int i = 0; i < 80; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (std::lgamma(mid + 1.0) / 0.6931471805599453 <= avail) lo = mid; else hi = mid;
        }

        return static_cast<std::uint64_t>(lo);
    }();

    return c;
}

bool gm_shift(const BigFloat& z, const BigFloatContext& wc, const BigFloatContext& xc, BigFloat& x, BigFloat& R, std::uint64_t& m) {
    x = z;
    R = BigFloat::one();
    if (!gm_shift_count(z, static_cast<std::uint64_t>(gm_target(wc.precision)), m)) return false;
    if (m == 0) return true;
    x = BigFloat::add(z, BigFloat(m), xc);
    R = gm_rising_rs(z, m, wc);
    return x.is_finite() && R.is_finite();
}

std::size_t gm_terms(double log2x, double w) {
    const double l2    = 0.6931471805599453;
    const double lg2pi = 2.6514961294723187;          
    double prev = 1.0e300;

    for (std::size_t j = 1; j <= (1u << 20); ++j) {
        const double u  = 2.0 * static_cast<double>(j);
        const double lt = 1.0 + std::lgamma(u + 1.0) / l2 - u * lg2pi - std::log2(u * (u - 1.0)) - (u - 1.0) * log2x;
        if (lt < -w - 8.0) return j;
        if (lt >= prev)    return j;                 
        prev = lt;
    }

    return 1u << 20;
}

double gm_log2_of(const BigFloat& x) {
    const std::int64_t e = x.get_exp_base2();
    if (e == BigFloat::exp_none) return 1.0;
    return static_cast<double>(e);
}

BigFloat gm_stirling(const BigFloat& x, const BigFloatContext& wc, std::size_t jcap) {
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    const BigFloat lx   = ln(x, wc);
    if (!lx.is_finite()) return lx;
    BigFloat head = BigFloat::mul(BigFloat::sub(x, half, wc), lx, wc);
    head = BigFloat::sub(head, x, wc);
    head = BigFloat::add(head, gm_half_ln_two_pi(wc), wc);
    const BigFloat     u   = x.reciprocal(wc);
    const BigFloat     u2  = BigFloat::mul(u, u, wc);
    const std::int64_t eh  = head.get_exp_base2();
    const std::int64_t ehp = (eh == BigFloat::exp_none) ? 0 : eh;
    const double       lx2 = gm_log2_fine(x);
    const std::size_t  G   = 32 + detail::ln_bit_length_u64(jcap);
    std::size_t jz = jcap + 1;

    for (std::size_t j = gm_zeta_jmin; j <= jcap; ++j) {
        const std::size_t p = gm_term_prec(wc.precision, ehp, gm_term_log2(j, lx2), G);
        if (p + 32 <= 2 * j * gm_zeta_kbits) { jz = j; break; }
    }

    constants::detail::tangent_reserve(((jz <= jcap) ? jz : jcap) + 1);
    BigFloat     S    = BigFloat::zero();
    BigFloat     t    = u;
    BigFloat     a    = BigFloat::zero();
    BigFloat     c    = BigFloat::zero();
    gm_zeta_t    zs;
    bool         zon  = false;
    std::int64_t prev = (std::numeric_limits<std::int64_t>::max)();

    for (std::size_t j = 1; j <= jcap; ++j) {
        const std::size_t     p = gm_term_prec(wc.precision, ehp, gm_term_log2(j, lx2), G);
        const BigFloatContext tc(BigFloatContext::clamp_precision(p), RoundingMode::nearest_even);
        const std::uint64_t   u2j = 2 * static_cast<std::uint64_t>(j);
        BigFloat term;

        if (!zon && j >= jz) {
            const BigFloat tp = constants::two_pi(tc);
            const BigFloat f  = BigFloat(fact_range(1, u2j - 1)).rounded(tc);
            a = BigFloat::div(BigFloat::mul(f, t, tc), gm_powi(tp, u2j, tc), tc).scaled_pow2(1);
            c = BigFloat::div(u2.rounded(tc), BigFloat::mul(tp, tp, tc), tc);
            gm_zeta_init(zs, u2j / 2, p + 32);
            zon = true;
        }

        if (zon) {
            term = BigFloat::mul(a, gm_zeta_value(zs, tc), tc);
            if (j % 2 == 0) term = term.with_sign(true);
        } else {
            const BigFloat b = gm_stirling_coeff(j, tc);
            if (!b.is_finite()) break;
            term = BigFloat::mul(b, t, tc);
        }

        if (!term.is_finite()) break;
        const std::int64_t et = term.get_exp_base2();
        if (et == BigFloat::exp_none) break;
        if (et >= prev) break;

        if (eh != BigFloat::exp_none && et < eh - static_cast<std::int64_t>(wc.precision)) {
            S = BigFloat::add(S, term, wc);
            break;
        }

        prev = et;
        S = BigFloat::add(S, term, wc);

        if (zon) {
            a = BigFloat::mul(a, BigFloat(u2j * (u2j - 1)), tc);
            a = BigFloat::mul(a, c.rounded(tc), tc);
            const std::size_t pn = gm_term_prec(wc.precision, ehp, gm_term_log2(j + 1, lx2), G);
            gm_zeta_step(zs, pn + 32);
        } else {
            t = BigFloat::mul(t, u2.rounded(tc), tc);
        }
    }

    return BigFloat::add(head, S, wc);
}

BigFloat gm_pos(const BigFloat& z, const BigFloatContext& wc, std::size_t& cancelled) {
    cancelled = 0;
    std::uint64_t hn = 0;

    if (gm_half_odd(z, hn) && hn <= wc.precision && 2 * hn <= fact_n_cap()) {
        const BigFloat lp = gm_ln_pi(wc).scaled_pow2(-1);
        if (hn == 0) return lp;
        const BigFloat q  = BigFloat(fact_range(hn + 1, 2 * hn + 1)).rounded(wc).scaled_pow2(-2 * static_cast<std::int64_t>(hn));
        const BigFloat lq = ln(q, wc);
        if (!lq.is_finite()) return BigFloat::undefined();
        const BigFloat v  = BigFloat::add(lq, lp, wc);
        const std::int64_t ea = lq.get_exp_base2();
        const std::int64_t eb = lp.get_exp_base2();
        const std::int64_t hi = (ea > eb) ? ea : eb;
        const std::int64_t ev = v.get_exp_base2();
        if (hi != BigFloat::exp_none && ev != BigFloat::exp_none && hi > ev) cancelled = static_cast<std::size_t>(hi - ev);
        return v;
    }

    BigFloat      x = BigFloat::zero();
    BigFloat      R = BigFloat::one();
    std::uint64_t m = 0;
    if (!gm_shift(z, wc, wc, x, R, m)) return BigFloat::undefined();
    const std::size_t J  = gm_terms(gm_log2_fine(x), static_cast<double>(wc.precision));
    const BigFloat    lg = gm_stirling(x, wc, J + 8);
    if (m == 0 || !lg.is_finite()) return lg;
    const BigFloat lr = ln(R, wc);
    if (!lr.is_finite()) return BigFloat::undefined();
    const BigFloat v = BigFloat::sub(lg, lr, wc);
    const std::int64_t ea = lg.get_exp_base2();
    const std::int64_t eb = lr.get_exp_base2();
    const std::int64_t hi = (ea > eb) ? ea : eb;
    const std::int64_t ev = v.get_exp_base2();
    if (hi != BigFloat::exp_none && ev != BigFloat::exp_none && hi > ev) cancelled = static_cast<std::size_t>(hi - ev);
    return v;
}

BigFloat gm_gamma_pos(const BigFloat& z, const BigFloatContext& wc) {
    std::uint64_t hn = 0;

    if (gm_half_odd(z, hn) && hn <= wc.precision && 2 * hn <= fact_n_cap()) {   // Gamma(n+1/2) = (n+1)...(2n) / 4^n * sqrt(pi)
        const BigFloat sp = constants::sqrt_pi(wc);
        if (hn == 0) return sp;
        const BigFloat q = BigFloat(fact_range(hn + 1, 2 * hn + 1)).rounded(wc).scaled_pow2(-2 * static_cast<std::int64_t>(hn));
        return BigFloat::mul(q, sp, wc);
    }

    const double T  = static_cast<double>(gm_target(wc.precision));
    double       lx = gm_log2_fine(z);
    if (lx < std::log2(T + 1.0)) lx = std::log2(T + 1.0);
    const std::size_t E = static_cast<std::size_t>(lx + std::log2(lx * 0.6931471805599453 + 1.0)) + 4;
    const BigFloatContext xc(BigFloatContext::clamp_precision(wc.precision + E), RoundingMode::nearest_even);
    BigFloat      x = BigFloat::zero();
    BigFloat      R = BigFloat::one();
    std::uint64_t m = 0;
    if (!gm_shift(z, wc, xc, x, R, m)) return BigFloat::undefined();
    const std::size_t J  = gm_terms(gm_log2_fine(x), static_cast<double>(xc.precision));
    const BigFloat    st = gm_stirling(x, xc, J + 8);
    if (!st.is_finite()) return st;
    const BigFloat g = exp(st, wc);
    if (!g.is_finite() || m == 0) return g;
    return BigFloat::div(g, R, wc);
}

BigFloat gm_frac(const BigFloat& z, const BigFloatContext& wc) {
    const BigInt t = z.get_integer_part();
    if (t.is_undefined()) return BigFloat::undefined();
    BigFloat r = BigFloat::sub(z, BigFloat(t), wc);
    if (r.signbit() && !r.is_zero()) r = BigFloat::add(r, BigFloat::one(), wc);
    return r;
}

bool gm_exact_u64(const BigFloat& z, std::uint64_t& out) {
    if (!z.is_finite() || z.signbit()) return false;
    if (z.is_zero()) { out = 0; return true; }
    const std::uint64_t top = (std::numeric_limits<std::uint64_t>::max)();
    if (BigFloat::compare(z, BigFloat(top)) == BigFloat::ordering::greater) return false;
    std::uint64_t v = 0;

    for (int b = 63; b >= 0; --b) {
        const std::uint64_t cand = v | (std::uint64_t(1) << b);
        if (BigFloat::compare(BigFloat(cand), z) != BigFloat::ordering::greater) v = cand;
    }

    if (BigFloat::compare(BigFloat(v), z) != BigFloat::ordering::equal) return false;
    out = v;
    return true;
}

gm_log_t gm_log_abs(const BigFloat& z, const BigFloatContext& wc) {
    gm_log_t r;

    if (!z.is_zero() && !z.signbit()) {
        r.v = gm_pos(z, wc, r.cancelled);
        return r;
    }

    const BigFloatContext fc(BigFloatContext::clamp_precision(wc.precision + 64), RoundingMode::nearest_even);
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    const BigFloat f = gm_frac(z, fc);
    if (!f.is_finite()) { r.v = BigFloat::undefined(); return r; }
    if (f.is_zero())    { r.pole = true; return r; }
    const BigFloat h = gm_frac(z.scaled_pow2(-1), fc);
    r.neg = (BigFloat::compare(h, half) == BigFloat::ordering::greater);
    const BigFloat om = BigFloat::sub(BigFloat::one(), z, wc);
    std::size_t    c1 = 0;
    const BigFloat lg = gm_pos(om, wc, c1);
    if (!lg.is_finite()) { r.v = lg; return r; }
    const BigFloat lpi = gm_ln_pi(wc);
    const auto     fh  = BigFloat::compare(f, half);

    if (fh == BigFloat::ordering::equal) {
        r.v = BigFloat::sub(lpi, lg, wc);
        r.cancelled = c1 + 4;
        return r;
    }

    const BigFloat g = (fh == BigFloat::ordering::greater) ? BigFloat::sub(BigFloat::one(), f, fc) : f;   // exact
    const BigFloat s = sin(BigFloat::mul(constants::pi(wc), g, wc), wc);                                 // s > 0
    if (!s.is_finite() || s.is_zero()) { r.v = BigFloat::undefined(); return r; }
    r.v = BigFloat::sub(BigFloat::sub(lpi, ln(s, wc), wc), lg, wc);
    r.cancelled = c1 + 4;
    return r;
}

BigFloat gm_gamma_abs(const BigFloat& z, const BigFloatContext& wc) {
    if (!z.is_zero() && !z.signbit()) return gm_gamma_pos(z, wc);
    const BigFloatContext fc(BigFloatContext::clamp_precision(wc.precision + 64), RoundingMode::nearest_even);
    const BigFloat f = gm_frac(z, fc);
    if (!f.is_finite() || f.is_zero()) return BigFloat::undefined();
    const std::int64_t ez = z.get_exp_base2();
    const std::size_t  Ez = (ez != BigFloat::exp_none && ez > 0) ? static_cast<std::size_t>(ez) + detail::ln_bit_length_u64(static_cast<std::uint64_t>(ez)) + 8 : 8;
    const BigFloatContext oc(BigFloatContext::clamp_precision(wc.precision + Ez), RoundingMode::nearest_even);
    const BigFloat om = BigFloat::sub(BigFloat::one(), z, oc);
    const BigFloat g  = gm_gamma_pos(om, wc);
    if (!g.is_finite()) return g;
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    const auto     fh   = BigFloat::compare(f, half);
    BigFloat s = BigFloat::one();

    if (fh != BigFloat::ordering::equal) {
        const BigFloat r = (fh == BigFloat::ordering::greater) ? BigFloat::sub(BigFloat::one(), f, fc) : f;
        s = sin(BigFloat::mul(constants::pi(wc), r, wc), wc);
        if (!s.is_finite() || s.is_zero()) return BigFloat::undefined();
    }

    return BigFloat::div(constants::pi(wc), BigFloat::mul(s, g, wc), wc);   // pi / (sin(pi z) Gamma(1 - z))
}

BigFloat gm_drive(const BigFloat& z, const BigFloatContext& ctx, bool& neg, bool& pole) {
    neg = false;
    pole = false;
    std::size_t guard = 48;
    const double T = double(gm_target(ctx.precision + guard));
    std::size_t extra = std::size_t(std::log2(T * std::log(T))) + 24;

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard + extra), RoundingMode::nearest_even);
        const gm_log_t g = gm_log_abs(z, wc);
        neg  = g.neg;
        pole = g.pole;
        if (g.pole) return BigFloat::infinity();
        if (!g.v.is_finite()) return g.v;
        if (g.v.is_zero()) return g.v;

        if (g.cancelled > extra) {
            if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
            extra = g.cancelled + 16;
            continue;
        }

        const std::size_t want = ctx.precision + guard + extra;
        const std::size_t acc  = (want > extra + 16) ? (want - extra - 16) : 1;
        const std::size_t L    = g.v.significand().bit_length();
        const std::size_t err  = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(g.v.significand(), ctx.precision, err + 2)) return g.v.rounded(ctx);
        if (gm_guard_exhausted(ctx.precision, guard + extra)) return g.v.rounded(ctx);
        guard *= 2;
    }
}

BigFloat gm_gamma_drive(const BigFloat& z, const BigFloatContext& ctx, bool neg) {
    std::size_t guard = 32;
    const double      T     = static_cast<double>(gm_target(ctx.precision + guard));
    const std::size_t extra = static_cast<std::size_t>(std::log2(T * std::log(T))) + 24;

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard + extra), RoundingMode::nearest_even);
        const BigFloat v = gm_gamma_abs(z, wc);
        if (!v.is_finite()) return v;
        if (v.is_zero()) return v.with_sign(neg);
        const std::size_t acc = (wc.precision > extra + 16) ? (wc.precision - extra - 16) : 1;
        const std::size_t L   = v.significand().bit_length();
        const std::size_t err = (L > acc) ? (L - acc) : 0;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.with_sign(neg).rounded(ctx);
        if (gm_guard_exhausted(ctx.precision, guard + extra)) return v.with_sign(neg).rounded(ctx);
        guard *= 2;
    }
}

BigFloat gm_exp_of_log(const BigFloat& z, const BigFloatContext& ctx, bool, bool pole_in) {
    if (pole_in) return BigFloat::undefined();
    const BigFloatContext probe(128, RoundingMode::nearest_even);   
    bool n0 = false, p0 = false;
    const BigFloat lg0 = gm_drive(z, probe, n0, p0);
    if (p0) return BigFloat::undefined();
    if (!lg0.is_finite()) return lg0;
    const std::int64_t e = lg0.get_exp_base2();
    const std::size_t  E = (e != BigFloat::exp_none && e > 0) ? static_cast<std::size_t>(e) + 2 : 2;
    const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + E + 64), RoundingMode::nearest_even);
    bool n1 = false, p1 = false;
    const BigFloat lg = gm_drive(z, wc, n1, p1);
    if (p1) return BigFloat::undefined();
    if (!lg.is_finite()) return lg;
    const BigFloat g = exp(lg, wc);
    if (!g.is_finite()) return g;
    return g.with_sign(n1).rounded(ctx);
}

} // namespace gmdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigUInt factorial(std::uint64_t n) {
    if (n > gmdetail::fact_n_cap()) return BigUInt::undefined();
    gmdetail::fact_cache_t& c = gmdetail::fact_cache();
    if (c.valid && c.n == n) return c.v;
    BigUInt v = (c.valid && n > c.n && n - c.n <= 256) ? (c.v * gmdetail::fact_range(c.n + 1, n + 1)) : gmdetail::fact_range(1, n + 1);
    c.n = n; c.v = v; c.valid = true;
    return v;
}

int gamma_sign(const BigFloat& z, const BigFloatContext& ctx) {
    if (!z.is_finite()) return 3;
    if (!z.is_zero() && !z.signbit()) return 0;
    const BigFloatContext fc(BigFloatContext::clamp_precision(ctx.precision + 64), RoundingMode::nearest_even);
    const BigFloat f = gmdetail::gm_frac(z, fc);
    if (!f.is_finite()) return 3;
    if (f.is_zero()) return 2;
    const BigFloat h = gmdetail::gm_frac(z.scaled_pow2(-1), fc);
    return (BigFloat::compare(h, BigFloat::one().scaled_pow2(-1)) == BigFloat::ordering::greater) ? 1 : 0;
}

BigFloat log_abs_gamma(const BigFloat& z, const BigFloatContext& ctx) {
    if (z.is_nan())       return BigFloat::nan();
    if (z.is_undefined()) return BigFloat::undefined();
    if (z.is_infinite())  return z.signbit() ? BigFloat::undefined() : z;
    std::uint64_t k = 0;

    if (gmdetail::gm_exact_u64(z, k) && k >= 1 && k - 1 <= gmdetail::fact_n_cap()) {
        const BigUInt f = factorial(k - 1);  

        if (!f.is_undefined()) {
            if (f.is_one()) return BigFloat::zero();               
            return ln(BigFloat(f), ctx);
        }
    }

    bool neg = false, pole = false;
    return gmdetail::gm_drive(z, ctx, neg, pole);
}

BigFloat log_gamma(const BigFloat& z, const BigFloatContext& ctx) {
    const int s = gamma_sign(z, ctx);
    if (s == 1 || s == 2) return BigFloat::undefined();
    if (s == 3) return z.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    return log_abs_gamma(z, ctx);
}

BigFloat gamma(const BigFloat& z, const BigFloatContext& ctx) {
    if (z.is_nan())       return BigFloat::nan();
    if (z.is_undefined()) return BigFloat::undefined();
    if (z.is_infinite())  return z.signbit() ? BigFloat::undefined() : z;
    std::uint64_t k = 0;

    if (gmdetail::gm_exact_u64(z, k)) {
        if (k == 0) return BigFloat::undefined();  

        if (k - 1 <= gmdetail::fact_n_cap()) {
            const BigUInt f = factorial(k - 1);                     
            if (!f.is_undefined()) return BigFloat(f).rounded(ctx);
        }
    }

    const int s = gamma_sign(z, ctx);
    if (s == 2 || s == 3) return BigFloat::undefined();
    return gmdetail::gm_gamma_drive(z, ctx, s == 1);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace gmdetail {

BigFloat gm_log_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx, bool& neg, int& state) {
    neg = false;
    state = 0;
    const BigFloat s = BigFloat::add(a, b, ctx.extended(16));
    const int sa = gamma_sign(a, ctx);
    const int sb = gamma_sign(b, ctx);
    const int ss = gamma_sign(s, ctx);
    if (sa == 3 || sb == 3 || ss == 3) { state = 3; return BigFloat::undefined(); }
    const bool pa = (sa == 2), pb = (sb == 2), ps = (ss == 2);
    if (pa || pb) { state = ps ? 3 : 2; return BigFloat::undefined(); }  
    if (ps)       { state = 1; return BigFloat::undefined(); }            
    neg = ((sa == 1) != (sb == 1)) != (ss == 1);
    std::size_t guard = 64;

    for (;;) {
        const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard), RoundingMode::nearest_even);
        bool n = false, p = false;
        const BigFloat la = gm_drive(a, wc, n, p);
        const BigFloat lb = gm_drive(b, wc, n, p);
        const BigFloat ls = gm_drive(s, wc, n, p);
        if (!la.is_finite() || !lb.is_finite() || !ls.is_finite()) { state = 3; return BigFloat::undefined(); }
        const BigFloat v = BigFloat::sub(BigFloat::add(la, lb, wc), ls, wc);
        std::int64_t hi = la.get_exp_base2();
        const std::int64_t eb2 = lb.get_exp_base2();
        const std::int64_t es  = ls.get_exp_base2();
        if (eb2 != BigFloat::exp_none && (hi == BigFloat::exp_none || eb2 > hi)) hi = eb2;
        if (es  != BigFloat::exp_none && (hi == BigFloat::exp_none || es  > hi)) hi = es;
        const std::int64_t ev  = v.get_exp_base2();
        const std::size_t lost = (hi != BigFloat::exp_none && ev != BigFloat::exp_none && hi > ev) ? static_cast<std::size_t>(hi - ev) : 0;
        const std::size_t good = (wc.precision > lost + 8) ? (wc.precision - lost - 8) : 1;
        const std::size_t L    = v.significand().bit_length();
        const std::size_t err  = (L > good) ? (L - good) : 0;
        if (v.is_zero()) return v;
        if (constants::bfdetail::round_is_safe(v.significand(), ctx.precision, err + 2)) return v.rounded(ctx);
        if (gm_guard_exhausted(ctx.precision, guard)) return v.rounded(ctx);
        guard = guard * 2 + lost;
    }
}

} // namespace gmdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat log_abs_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    bool neg = false;
    int  st  = 0;
    const BigFloat v = gmdetail::gm_log_beta(a, b, ctx, neg, st);
    if (st == 1) return BigFloat::infinity(true);    
    if (st == 2) return BigFloat::infinity();         
    if (st == 3) return BigFloat::undefined();
    return v;
}

BigFloat log_beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    bool neg = false;
    int  st  = 0;
    const BigFloat v = gmdetail::gm_log_beta(a, b, ctx, neg, st);
    if (st == 1) return BigFloat::infinity(true);     
    if (st != 0 || neg) return BigFloat::undefined(); 
    return v;
}

BigFloat beta(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    bool neg = false;
    int  st  = 0;
    const BigFloatContext probe(128, RoundingMode::nearest_even);   
    const BigFloat lb0 = gmdetail::gm_log_beta(a, b, probe, neg, st);
    if (st == 1) return BigFloat::zero();
    if (st >= 2) return BigFloat::undefined();
    if (!lb0.is_finite()) return lb0;
    const std::int64_t e = lb0.get_exp_base2();
    const std::size_t  E = (e != BigFloat::exp_none && e > 0) ? static_cast<std::size_t>(e) + 2 : 2;
    const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + E + 64), RoundingMode::nearest_even);
    const BigFloat lb = gmdetail::gm_log_beta(a, b, wc, neg, st);
    if (st != 0 || !lb.is_finite()) return BigFloat::undefined();
    const BigFloat v = exp(lb, wc);
    if (!v.is_finite()) return v;
    return v.with_sign(neg).rounded(ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
