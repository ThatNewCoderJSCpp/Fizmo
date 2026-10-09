#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace spdetail {

auto sp_sum::add(const BF& t, double units, const BFC& ac, std::size_t want) -> void {
        s = BF::add(s, t, ac);
        if (t.is_zero()) return;
        const double lt = hz_l2(t);
        if (lt > lmax) lmax = lt;
        err = hz_lsum(err, lt + std::log2(units) - static_cast<double>(want));
        err = hz_lsum(err, lmax - static_cast<double>(want) - 39.0);
    }

zt_val sp_finish(const BF& v, double err, std::size_t want) {
    if (v.is_zero() || !v.is_finite()) return zt_val{v, 1.0e9};
    return zt_val{v, std::log2(std::exp2(err - hz_l2(v) + static_cast<double>(want)) + 1.0) + 1.0};
}

zt_val sp_si_series(const BF& x, bool hyp, std::size_t want) {                   
    const BFC    wc  = sp_ctx(want);
    const BFC    ac  = sp_ctx(want + 40);
    const BF     x2  = BF::mul(x, x, wc);
    const BF     q   = hyp ? x2 : -x2;
    const double lx2 = hz_l2(x2);
    sp_sum a;
    BF t = x;                                                                               
    a.add(t, 1.0, ac, want);

    for (std::uint64_t k = 0; k < sp_cap; ++k) {
        t = BF::div(BF::mul(t, q, wc), BF((2 * k + 2) * (2 * k + 3)), wc);
        const BF term = BF::div(t, BF(2 * k + 3), wc);
        a.add(term, 3.0 * static_cast<double>(k) + 6.0, ac, want);
        const double rho = std::exp2(lx2 - std::log2(static_cast<double>(2 * k + 4) * static_cast<double>(2 * k + 5)));

        if (rho < 0.5) {
            const double lt = hz_l2(term) + std::log2(2.0 * rho);
            if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
        }
    }

    return sp_undef();
}

zt_val sp_ci_series(const BF& x, bool hyp, std::size_t want) {                   
    const BFC    wc  = sp_ctx(want);
    const BFC    ac  = sp_ctx(want + 40);
    const BF     x2  = BF::mul(x, x, wc);
    const BF     q   = hyp ? x2 : -x2;
    const double lx2 = hz_l2(x2);
    sp_sum a;
    a.add(constants::euler_mascheroni(wc), 1.0, ac, want);
    a.add(ln(x, wc), 1.0, ac, want);
    BF u = q.scaled_pow2(-1);                                                               

    for (std::uint64_t k = 1; k < sp_cap; ++k) {
        const BF term = BF::div(u, BF(2 * k), wc);
        a.add(term, 3.0 * static_cast<double>(k) + 3.0, ac, want);
        u = BF::div(BF::mul(u, q, wc), BF((2 * k + 1) * (2 * k + 2)), wc);
        const double rho = std::exp2(lx2 - std::log2(static_cast<double>(2 * k + 1) * static_cast<double>(2 * k + 2)));

        if (rho < 0.5) {
            const double lt = hz_l2(term) + std::log2(2.0 * rho);
            if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
        }
    }

    return sp_undef();
}

bool sp_sici_asym_ok(const BF& x, double target) {
    const double lx = hz_l2(x);
    double la = 0.0, lb = 0.0;

    for (std::uint64_t k = 1; k < 4000000; ++k) {
        const double kd = static_cast<double>(k);
        const double ra = std::log2((2 * kd - 1) * (2 * kd)) - 2.0 * lx;
        const double rb = std::log2((2 * kd) * (2 * kd + 1)) - 2.0 * lx;
        if (ra >= 0.0 || rb >= 0.0) return false;
        la += ra;
        lb += rb;
        if (std::max(la, lb) <= -target) return true;
    }

    return false;
}

zt_val sp_sici_asym(bool ci, const BF& x, std::size_t want) {
    const BFC wc  = sp_ctx(want);
    const BFC ac  = sp_ctx(want + 40);
    const BF  one = BF::one();
    const BF  ix2 = BF::mul(x, x, wc).reciprocal(wc);
    BF a = one, b = one, F = one, G = one;
    double eF = sp_ninf, eG = sp_ninf, tail = sp_ninf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > sp_cap) return sp_undef();
        a = BF::mul(BF::mul(a, BF((2 * k - 1) * (2 * k)), wc), ix2, wc);                   
        b = BF::mul(BF::mul(b, BF((2 * k) * (2 * k + 1)), wc), ix2, wc);                   
        const double l = std::max(hz_l2(a), hz_l2(b));
        if (l <= -static_cast<double>(want) - 8.0) { tail = l + 1.0; break; }
        const bool minus = (k & 1u) != 0;
        F  = minus ? BF::sub(F, a, ac) : BF::add(F, a, ac);
        G  = minus ? BF::sub(G, b, ac) : BF::add(G, b, ac);
        eF = hz_lsum(eF, hz_l2(a) + std::log2(4.0 * static_cast<double>(k) + 2.0));
        eG = hz_lsum(eG, hz_l2(b) + std::log2(4.0 * static_cast<double>(k) + 2.0));
    }

    const BF f  = BF::div(F, x, wc);
    const BF g  = BF::div(G, BF::mul(x, x, wc), wc);
    const BF s  = sin(x, wc);
    const BF c  = cos(x, wc);
    const BF p1 = BF::mul(f, ci ? s : c, wc);
    const BF p2 = BF::mul(g, ci ? c : s, wc);
    const BF v  = ci ? BF::sub(p1, p2, wc) : BF::sub(BF::sub(constants::half_pi(wc), p1, wc), p2, wc);
    const double wd = static_cast<double>(want);
    double err = hz_lsum(hz_l2(f) - wd, hz_l2(g) - wd);                                    
    err = hz_lsum(err, hz_l2(p1) + std::log2(std::exp2(eF) + 4.0) - wd);
    err = hz_lsum(err, hz_l2(p2) + std::log2(std::exp2(eG) + 5.0) - wd);
    err = hz_lsum(err, hz_lsum(hz_l2(f), hz_l2(g)) + tail);
    if (!ci) err = hz_lsum(err, 1.0 - wd);
    return sp_finish(v, err, want);
}

zt_val sp_sici_raw(int kind, const BF& x, std::size_t want) {                    
    const bool neg = x.signbit();
    const BF   ax  = x.abs();
    zt_val r;
    if (kind <= 1 && sp_sici_asym_ok(ax, static_cast<double>(want) + 8.0)) r = sp_sici_asym(kind == 1, ax, want);
    else if (kind == 0 || kind == 2) r = sp_si_series(ax, kind == 2, want);
    else                             r = sp_ci_series(ax, kind == 3, want);
    if (neg && (kind == 0 || kind == 2)) r.v = -r.v;
    return r;
}

std::size_t sp_sici_guard(int kind, const BF& x, std::size_t prec) {
    if (kind >= 2 || sp_sici_asym_ok(x.abs(), static_cast<double>(prec) + 48.0)) return 32;
    return sp_guard(1.45 * sp_dbl(x.abs()));
}

zt_val sp_ei_series(const BF& x, std::size_t want) {                              
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const double lx = hz_l2(x);
    sp_sum a;
    a.add(constants::euler_mascheroni(wc), 1.0, ac, want);
    a.add(ln(x, wc), 1.0, ac, want);
    BF t = x;

    for (std::uint64_t k = 1; k < sp_cap; ++k) {
        const BF term = BF::div(t, BF(k), wc);
        a.add(term, 2.0 * static_cast<double>(k) + 2.0, ac, want);
        t = BF::div(BF::mul(t, x, wc), BF(k + 1), wc);
        const double rho = std::exp2(lx - std::log2(static_cast<double>(k + 1)));

        if (rho < 0.5) {
            const double lt = hz_l2(term) + std::log2(2.0 * rho);
            if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
        }
    }

    return sp_undef();
}

zt_val sp_en_series(const BF& x, std::uint64_t n, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const double lx = hz_l2(x);
    const double wd = static_cast<double>(want);
    BF H = BF::zero();
    for (std::uint64_t j = 1; j < n; ++j) H = BF::add(H, BF(j).reciprocal(ac), ac);
    const BF psi  = BF::sub(H, constants::euler_mascheroni(wc), wc);
    const BF lnx  = ln(x, wc);
    const BF pl   = BF::sub(psi, lnx, wc);
    const double lpl = hz_lsum(hz_l2(psi) + 1.0, hz_l2(lnx)) + 1.0 - wd;                    
    const BF mx = -x;
    sp_sum a;
    BF w = BF::one();                                                                       

    for (std::uint64_t k = 0; k < sp_cap; ++k) {
        if (k > 0) w = BF::div(BF::mul(w, mx, wc), BF(k), wc);

        if (k + 1 == n) {
            a.add(BF::mul(w, pl, wc), 2.0 * static_cast<double>(k) + 2.0, ac, want);
            a.add_abs(hz_l2(w) + lpl);
        } else {
            const BF term = -BF::div(w, BF(static_cast<std::int64_t>(k) - static_cast<std::int64_t>(n) + 1), wc);
            a.add(term, 2.0 * static_cast<double>(k) + 2.0, ac, want);
        }

        const double rho = std::exp2(lx - std::log2(static_cast<double>(k + 1)));

        if (k >= n && rho < 0.5) {
            const double lt = hz_l2(w) + std::log2(2.0 * rho);
            if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
        }
    }

    return sp_undef();
}

bool sp_en_asym_ok(double lx, std::uint64_t n, double target) {
    double lc = 0.0;

    for (std::uint64_t k = 1; k < 4000000; ++k) {
        const double r = std::log2(static_cast<double>(n + k - 1)) - lx;
        if (r >= 0.0) return false;
        lc += r;
        if (lc <= -target) return true;
    }

    return false;
}

zt_val sp_en_asym(const BF& x, std::uint64_t n, std::size_t want) {              
    const BFC wc = sp_ctx(want);
    const BFC ac = sp_ctx(want + 40);
    const BF  ix = x.reciprocal(wc);
    BF c = BF::one(), S = BF::one();
    double E = sp_ninf, tail = sp_ninf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > sp_cap) return sp_undef();
        c = BF::mul(BF::mul(c, BF(n + k - 1), wc), ix, wc);
        const double l = hz_l2(c);
        if (l <= -static_cast<double>(want) - 8.0) { tail = l + 1.0; break; }
        S = (k & 1u) ? BF::sub(S, c, ac) : BF::add(S, c, ac);
        E = hz_lsum(E, l + std::log2(3.0 * static_cast<double>(k) + 2.0));
    }

    const BF v = BF::mul(BF::mul(exp(-x, wc), ix, wc), S, wc);
    const double units = std::exp2(E) + std::exp2(tail + static_cast<double>(want)) + 6.0 + sp_dbl(x);   
    return zt_val{v, std::log2(units) + 1.0};
}

zt_val sp_en_raw(const BF& x, std::uint64_t n, std::size_t want) {
    if (n == 0) {                                                                          
        const BFC wc = sp_ctx(want);
        return zt_val{BF::div(exp(-x, wc), x, wc), std::log2(5.0 + sp_dbl(x.abs())) + 1.0};
    }

    if (sp_en_asym_ok(hz_l2(x), n, static_cast<double>(want) + 8.0)) return sp_en_asym(x, n, want);
    return sp_en_series(x, n, want);
}

std::size_t sp_en_guard(const BF& x, std::uint64_t n, std::size_t prec) {
    if (n == 0 || sp_en_asym_ok(hz_l2(x), n, static_cast<double>(prec) + 48.0)) return 32;
    return sp_guard(2.886 * sp_dbl(x));
}

zt_val sp_ei_raw(const BF& x, std::size_t want) {                                 
    if (!x.signbit()) return sp_ei_series(x, want);
    zt_val r = sp_en_raw(-x, 1, want);                                                     
    r.v = -r.v;
    return r;
}

zt_val sp_li_raw(const BF& x, std::size_t want) {                                 
    const BF     u = ln(x, sp_ctx(want));
    const zt_val r = sp_ei_raw(u, want);
    if (!r.v.is_finite()) return r;
    const double err = hz_lsum(sp_abs(r, want), hz_l2(x) + 1.0 - static_cast<double>(want));
    return sp_finish(r.v, err, want);
}

zt_val sp_lin_series(const BF& y, std::uint64_t n, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const double ly = hz_l2(y);
    const double wd = static_cast<double>(want);
    BF H = BF::zero();
    for (std::uint64_t j = 1; j < n; ++j) H = BF::add(H, BF(j).reciprocal(ac), ac);
    const BF     psi = BF::sub(H, constants::euler_mascheroni(wc), wc);
    const BF     lny = ln(y.abs(), wc);
    const BF     c   = BF::sub(lny, psi, wc);
    const double lc  = hz_lsum(hz_l2(psi) + 1.0, hz_l2(lny)) + 1.0 - wd;
    sp_sum a;
    BF w = BF::one();

    for (std::uint64_t k = 0; k < sp_cap; ++k) {
        if (k > 0) w = BF::div(BF::mul(w, y, wc), BF(k), wc);

        if (k + 1 == n) {
            a.add(BF::mul(w, c, wc), 2.0 * static_cast<double>(k) + 2.0, ac, want);
            a.add_abs(hz_l2(w) + lc);
        } else {
            const BF term = BF::div(w, BF(static_cast<std::int64_t>(k) - static_cast<std::int64_t>(n) + 1), wc);
            a.add(term, 2.0 * static_cast<double>(k) + 3.0, ac, want);
        }

        if (k >= n) {
            const double rho = std::exp2(ly - std::log2(static_cast<double>(k + 1)));

            if (rho < 0.5) {
                const double lt = hz_l2(w) + std::log2(2.0 * rho);
                if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
            }
        }
    }

    return sp_undef();
}

zt_val sp_lin_raw(const BF& x, std::uint64_t n, std::size_t want) {
    const BFC wc = sp_ctx(want);
    const BF  y  = ln(x, wc);
    if (!y.is_finite() || y.is_zero()) return sp_undef();
    const bool   neg = y.signbit();
    const BF     ay  = y.abs();
    const zt_val r   = neg ? sp_en_raw(ay, n, want) : sp_lin_series(y, n, want);
    if (!r.v.is_finite()) return r;
    const BF P = gmdetail::gm_powi(ay, n - 1, wc);
    BF v = BF::div(r.v, P, wc);
    if (neg && (n & 1u)) v = -v;
    const double wd = static_cast<double>(want);
    const double nd = static_cast<double>(n);
    double err = hz_lsum(sp_abs(r, want) - hz_l2(P), hz_l2(v) + std::log2(2.0 * nd + 4.0) - wd);
    err = hz_lsum(err, hz_l2(x) - (nd - 1.0) * hz_l2(ay) + 1.0 - wd);
    return sp_finish(v, err, want);
}

zt_val sp_fact_poly(const BF& z, std::uint64_t m, std::size_t want) {
    const BFC wc = sp_ctx(want);
    const BFC ac = sp_ctx(want + 40);
    sp_sum a;
    BF t = BF(gmdetail::fact_range(1, m + 1)).rounded(wc);
    a.add(t, 1.0, ac, want);

    for (std::uint64_t k = 1; k <= m; ++k) {
        t = BF::div(BF::mul(t, z, wc), BF(k), wc);
        a.add(t, 2.0 * static_cast<double>(k) + 3.0, ac, want);
    }

    return sp_finish(a.s, a.err, want);
}

std::size_t sp_fact_poly_guard(const BF& z, std::uint64_t m) {
    if (!z.signbit()) return 32;
    return sp_guard(2.886 * std::min(sp_dbl(z), static_cast<double>(m)));
}

zt_val sp_linneg_raw(const BF& x, std::uint64_t m, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const double wd = static_cast<double>(want);
    const BF     y  = ln(x, wc);
    const zt_val r  = sp_fact_poly(-y, m, want);
    if (!r.v.is_finite()) return r;
    BF v = BF::mul(x, r.v, wc);
    if (m & 1u) v = -v;
    double err = hz_lsum(sp_abs(r, want) + hz_l2(x), hz_l2(v) + 1.0 - wd);
    err = hz_lsum(err, hz_l2(x) + static_cast<double>(m + 1) * hz_l2(y) + 1.0 - wd);
    return sp_finish(v, err, want);
}

zt_val sp_en_neg_raw(const BF& x, std::uint64_t m, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const zt_val r  = sp_fact_poly(x, m, want);
    if (!r.v.is_finite()) return r;
    const BF P = gmdetail::gm_powi(x, m + 1, wc);
    const BF v = BF::div(BF::mul(exp(-x, wc), r.v, wc), P, wc);
    const double units = std::exp2(r.lost) + 2.0 * static_cast<double>(m) + 8.0;
    return zt_val{v, std::log2(units) + 1.0};
}

void sp_sincospi(const BF& q, const BFC& wc, BF& s, BF& c) {
    const bool odd = q.get_integer_part().is_odd();
    const BF   a   = BF::mul(constants::pi(wc), q.get_fractional_part(), wc);
    s = sin(a, wc);
    c = cos(a, wc);

    if (odd) {
        s = -s;
        c = -c;
    }
}

zt_val sp_fresnel_series(bool sine, const BF& x, std::size_t want) {            
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const BF     y  = BF::mul(constants::pi(wc), hz_xmul(x, x).scaled_pow2(-1), wc);
    const double ly = hz_l2(y);
    sp_sum a;
    BF w = x;

    for (std::uint64_t j = 0; j < sp_cap; ++j) {
        if (j > 0) w = BF::div(BF::mul(w, y, wc), BF(j), wc);

        if ((j & 1u) == (sine ? 1u : 0u)) {
            BF term = BF::div(w, BF(2 * j + 1), wc);
            if ((j >> 1) & 1u) term = -term;
            a.add(term, 5.0 * static_cast<double>(j) + 3.0, ac, want);
        }

        const double rho = std::exp2(ly - std::log2(static_cast<double>(j + 1)));

        if (j >= 1 && rho < 0.5) {
            const double lt = hz_l2(w) + std::log2(2.0 * rho);
            if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
        }
    }

    return sp_undef();
}

bool sp_fresnel_asym_ok(const BF& x, double target) {
    const double lz = 1.6514961294723187 + 2.0 * hz_l2(x);                                 
    double la = 0.0, lb = 0.0;

    for (std::uint64_t k = 1; k < 4000000; ++k) {
        const double kd = static_cast<double>(k);
        const double ra = std::log2((4 * kd - 3) * (4 * kd - 1)) - 2.0 * lz;
        const double rb = std::log2((4 * kd - 1) * (4 * kd + 1)) - 2.0 * lz;
        if (ra >= 0.0 || rb >= 0.0) return false;
        la += ra;
        lb += rb;
        if (std::max(la, lb) <= -target) return true;
    }

    return false;
}

zt_val sp_fresnel_asym(bool sine, const BF& x, std::size_t want) {                
    const BFC wc  = sp_ctx(want);
    const BFC ac  = sp_ctx(want + 40);
    const BF  one = BF::one();
    const BF  pi  = constants::pi(wc);
    const BF  xx  = hz_xmul(x, x);
    const BF  z   = BF::mul(pi, xx, wc);
    const BF  iz2 = BF::mul(z, z, wc).reciprocal(wc);
    BF a = one, b = one, F = one, G = one;
    double eF = sp_ninf, eG = sp_ninf, tail = sp_ninf;

    for (std::uint64_t k = 1; ; ++k) {
        if (k > sp_cap) return sp_undef();
        a = BF::mul(BF::mul(a, BF((4 * k - 3) * (4 * k - 1)), wc), iz2, wc);
        b = BF::mul(BF::mul(b, BF((4 * k - 1) * (4 * k + 1)), wc), iz2, wc);
        const double l = std::max(hz_l2(a), hz_l2(b));
        if (l <= -static_cast<double>(want) - 8.0) { tail = l + 1.0; break; }
        const bool minus = (k & 1u) != 0;
        F  = minus ? BF::sub(F, a, ac) : BF::add(F, a, ac);
        G  = minus ? BF::sub(G, b, ac) : BF::add(G, b, ac);
        eF = hz_lsum(eF, hz_l2(a) + std::log2(7.0 * static_cast<double>(k) + 2.0));
        eG = hz_lsum(eG, hz_l2(b) + std::log2(7.0 * static_cast<double>(k) + 2.0));
    }

    const BF f = BF::div(F, BF::mul(pi, x, wc), wc);                                        
    const BF g = BF::div(G, BF::mul(BF::mul(pi, pi, wc), BF::mul(xx, x, wc), wc), wc);      
    BF s, c;
    sp_sincospi(xx.scaled_pow2(-1), wc, s, c);                                              
    const BF p1 = BF::mul(f, sine ? c : s, wc);
    const BF p2 = BF::mul(g, sine ? s : c, wc);
    const BF half = one.scaled_pow2(-1);
    const BF v = sine ? BF::sub(BF::sub(half, p1, wc), p2, wc) : BF::sub(BF::add(half, p1, wc), p2, wc);                             
    const double wd = static_cast<double>(want);
    double err = hz_lsum(hz_l2(f) + 3.0 - wd, hz_l2(g) + 3.0 - wd);
    err = hz_lsum(err, hz_l2(p1) + std::log2(std::exp2(eF) + 5.0) - wd);
    err = hz_lsum(err, hz_l2(p2) + std::log2(std::exp2(eG) + 9.0) - wd);
    err = hz_lsum(err, hz_lsum(hz_l2(f), hz_l2(g)) + tail);
    err = hz_lsum(err, -1.0 - wd);
    return sp_finish(v, err, want);
}

zt_val sp_fresnel_raw(bool sine, const BF& x, std::size_t want) {
    const BF ax = x.abs();
    zt_val r = sp_fresnel_asym_ok(ax, static_cast<double>(want) + 8.0) ? sp_fresnel_asym(sine, ax, want) : sp_fresnel_series(sine, ax, want);
    if (x.signbit()) r.v = -r.v;                                                           
    return r;
}

std::size_t sp_fresnel_guard(const BF& x, std::size_t prec) {
    if (sp_fresnel_asym_ok(x.abs(), static_cast<double>(prec) + 48.0)) return 32;
    const double xd = sp_dbl(x.abs());
    return sp_guard(2.27 * xd * xd);
}

bool sp_reduce_2pi(const BF& t, std::size_t want, BF& r, bool& reduced) {
    reduced = false;
    if (BF::compare(t.abs(), BF(static_cast<std::uint64_t>(3))) == BF::ordering::less) { r = t; return true; }
    const std::int64_t e = t.get_exp_base2();
    if (e > 60) return false;
    const std::size_t eb = static_cast<std::size_t>(e > 0 ? e : 0);
    const BFC         qc = sp_ctx(eb + 80);
    const BF          q  = BF::mul(t.abs(), constants::inv_pi(qc).scaled_pow2(-1), qc);    
    std::int64_t      j  = static_cast<std::int64_t>(BF::add(q, BF::one().scaled_pow2(-1), qc).get_integer_part().get_lowest_bits());
    if (t.signbit()) j = -j;
    const BFC rc = sp_ctx(want + eb + 28);
    const BF  tp = constants::two_pi(rc);
    const BF  pi = tp.scaled_pow2(-1);
    r = BF::sub(t, BF::mul(BF(j), tp, rc), rc);
    if (BF::compare(r, pi) == BF::ordering::greater)   { ++j; r = BF::sub(r, tp, rc); }
    else if (BF::compare(r, -pi) == BF::ordering::less) { --j; r = BF::add(r, tp, rc); }
    reduced = (j != 0);
    return true;
}

zt_val sp_clausen_raw(const BF& theta, std::uint64_t s, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const double wd = static_cast<double>(want);
    BF   r;
    bool reduced = false;
    if (!sp_reduce_2pi(theta, want, r, reduced)) return sp_undef();
    if (r.is_zero()) return sp_undef();
    const BF     ar = r.abs();
    const double rd = sp_dbl(ar);

    if (s == 1) {                                                                          
        const BF y = sin(r.scaled_pow2(-1), wc).abs().scaled_pow2(1);
        const BF v = -ln(y, wc);
        const double err = std::log2(3.0 + (reduced ? std::ldexp(1.0, -8) / rd : 0.0)) - wd;
        return sp_finish(v, err, want);
    }

    const BF lnr = ln(ar, wc);
    BF H = BF::zero();
    for (std::uint64_t j = 1; j < s; ++j) H = BF::add(H, BF(j).reciprocal(ac), ac);
    const BF     hl   = BF::sub(H, lnr, wc);
    const double lhl  = hz_lsum(hz_l2(H), hz_l2(lnr)) + 1.0 - wd;
    const double lr   = hz_l2(ar);
    const double l2pi = 2.6514961294723187;
    const double lpre = 2.0 + static_cast<double>(s) * lr - l2pi - std::lgamma(static_cast<double>(s) + 1.0) / 0.6931471805599453
                      - std::log2(1.0 - (rd / 6.283185307179586) * (rd / 6.283185307179586));
    const std::uint64_t k0  = s - 1;
    const std::uint64_t par = k0 & 1u;
    sp_sum a;
    BF p = BF::one();                                                                      

    for (std::uint64_t k = 0; k < 8 * static_cast<std::uint64_t>(want) + s + 1000; ++k) {
        if (k > 0) p = BF::div(BF::mul(p, ar, wc), BF(k), wc);
        if ((k & 1u) != par) continue;
        BF term;

        if (k == k0) {
            term = BF::mul(p, hl, wc);
            a.add_abs(hz_l2(p) + lhl);
        } else if (k < k0) {
            term = BF::mul(pldetail::pl_zeta_pos(s - k, wc), p, wc);
        } else {
            term = BF::mul(pldetail::pl_zeta_negodd(k - s, wc), p, wc);
        }

        if ((k / 2) & 1u) term = -term;
        a.add(term, 2.0 * static_cast<double>(k) + 4.0, ac, want);

        if (k > k0 + 1) {                                                                   
            const double lt = lpre + static_cast<double>(k + 2 - s) * (lr - l2pi);
            if (a.done(lt, want)) {
                a.add_abs(lt);
                if (reduced) a.add_abs(std::log2((s == 2 ? std::fabs(std::log(rd)) + 2.0 : 2.0)) - wd - 8.0); 
                BF v = a.s;
                if ((s & 1u) == 0 && r.signbit()) v = -v;                                 
                return sp_finish(v, a.err, want);
            }
        }
    }

    return sp_undef();
}

const std::vector<BigUInt>& sp_cl_poly(std::size_t m) {
    static thread_local std::vector<std::vector<BigUInt>> rows;
    if (rows.empty()) rows.push_back(std::vector<BigUInt>{BigUInt::zero(), BigUInt::one()});

    while (rows.size() <= m) {
        std::vector<BigUInt> b(rows.back().size() + 1, BigUInt::zero());

        {
            const std::vector<BigUInt>& a = rows.back();

            for (std::size_t i = 1; i < a.size(); ++i) {
                if (a[i].is_zero()) continue;
                BigUInt d = a[i];
                d.mul_small_mutable(static_cast<std::uint64_t>(i));
                b[i - 1].add_mutable(d);
                b[i + 1].add_mutable(d);
            }
        }

        rows.push_back(std::move(b));
    }

    return rows[m];
}

zt_val sp_clausen_neg_raw(const BF& theta, std::uint64_t m, std::size_t want) {
    const BFC wc = sp_ctx(want);
    BF   r;
    bool reduced = false;
    if (!sp_reduce_2pi(theta, want, r, reduced)) return sp_undef();
    if (r.is_zero()) return sp_undef();
    const BF u  = r.scaled_pow2(-1);
    const BF su = sin(u, wc);
    const BF cu = cos(u, wc);
    if (su.is_zero()) return sp_undef();
    const BF c  = BF::div(cu, su, wc);
    const BF c2 = BF::mul(c, c, wc);
    const std::vector<BigUInt>& a = sp_cl_poly(static_cast<std::size_t>(m));
    const std::size_t p   = static_cast<std::size_t>((m + 1) & 1u);
    const std::size_t top = (a.size() - 1 - p) / 2;
    BF h = BF::zero();

    for (std::size_t t = top + 1; t-- > 0; ) {
        const BigUInt& coef = a[p + 2 * t];
        h = BF::add(BF::mul(h, c2, wc), BF(coef), wc);
    }

    if (p) h = BF::mul(h, c, wc);
    BF v = h.scaled_pow2(-static_cast<std::int64_t>(m + 1));
    if (((m >> 1) & 1u) != (m & 1u)) v = -v;
    const double sr   = 2.0 * sp_dbl(su) * sp_dbl(cu);
    const double relc = 3.0 + ((reduced && sr > 0.0) ? std::ldexp(1.0, -8) / sr : 0.0);
    const double units = static_cast<double>(m + 1) * relc + 2.0 * static_cast<double>(m) + 8.0;
    return zt_val{v, std::log2(units) + 1.0};
}

zt_val sp_debye_small(const BF& x, std::uint64_t n, std::size_t want) {
    const BFC wc = sp_ctx(want);
    const BFC ac = sp_ctx(want + 40);
    BF xn = BF::one();
    for (std::uint64_t i = 0; i < n; ++i) xn = BF::mul(xn, x, wc);
    const double nd = static_cast<double>(n);
    sp_sum a;
    a.add(BF::div(xn, BF(n), wc), nd + 1.0, ac, want);
    a.add(-BF::div(BF::mul(xn, x, wc), BF(2 * (n + 1)), wc), nd + 3.0, ac, want);
    const BF ro  = BF::div(x, constants::two_pi(wc), wc);
    const BF ro2 = BF::mul(ro, ro, wc);
    const double rho = sp_dbl(ro2);
    BF p = xn;

    for (std::uint64_t j = 1; j < sp_cap; ++j) {
        p = BF::mul(p, ro2, wc);
        BF term = BF::div(BF::mul(pldetail::pl_zeta_pos(2 * j, wc), p, wc).scaled_pow2(1), BF(n + 2 * j), wc);
        if ((j & 1u) == 0) term = -term;
        a.add(term, nd + 4.0 * static_cast<double>(j) + 6.0, ac, want);
        const double lt = hz_l2(term) + std::log2(rho / (1.0 - rho));
        if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
    }

    return sp_undef();
}

zt_val sp_debye_tail_sum(const BF& x, std::uint64_t n, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const BFC    ac = sp_ctx(want + 40);
    const BF     one = BF::one();
    const BF     E   = exp(-x, wc);
    const double eu  = 3.0 + sp_dbl(x);                                                      
    const double Ed  = sp_dbl(E);
    const BF     nf  = BF(gmdetail::fact_range(1, n + 1));
    sp_sum a;
    BF Ek = one;

    for (std::uint64_t k = 1; k < sp_cap; ++k) {
        Ek = BF::mul(Ek, E, wc);
        const BF y = BF::mul(x, BF(k), wc);
        BF h = one;
        for (std::uint64_t j = n; j >= 1; --j) h = BF::add(one, BF::div(BF::mul(h, y, wc), BF(j), wc), wc);
        const BF term = BF::div(BF::mul(BF::mul(Ek, nf, wc), h, wc), BF(hz_upow(BigUInt(k), n + 1)), wc);
        a.add(term, static_cast<double>(k) * eu + 3.0 * static_cast<double>(n) + 6.0, ac, want);
        const double lt = hz_l2(term) + std::log2(Ed / (1.0 - Ed));                          
        if (a.done(lt, want)) { a.add_abs(lt); return sp_finish(a.s, a.err, want); }
    }

    return sp_undef();
}

zt_val sp_debye_raw(int kind, const BF& x, std::uint64_t n, std::size_t want) {
    const BFC    wc = sp_ctx(want);
    const double wd = static_cast<double>(want);
    const BF     Z  = BF::mul(BF(gmdetail::fact_range(1, n + 1)), pldetail::pl_zeta_pos(n + 1, wc), wc);   
    const double lZ = hz_l2(Z) + 1.6 - wd;
    BF I, T;
    double eI, eT;

    if (BF::compare(x, BF(static_cast<std::uint64_t>(3))) != BF::ordering::greater) {
        const zt_val r = sp_debye_small(x, n, want);
        if (!r.v.is_finite()) return r;
        I  = r.v;
        eI = sp_abs(r, want);
        T  = BF::sub(Z, I, wc);
        eT = hz_lsum(hz_lsum(lZ, eI), hz_l2(Z) - wd);
    } else {
        const zt_val r = sp_debye_tail_sum(x, n, want);
        if (!r.v.is_finite()) return r;
        T  = r.v;
        eT = sp_abs(r, want);
        I  = BF::sub(Z, T, wc);
        eI = hz_lsum(hz_lsum(lZ, eT), hz_l2(Z) - wd);
    }

    if (kind == 0) return sp_finish(I, eI, want);
    if (kind == 1) return sp_finish(T, eT, want);
    BF xn = BF::one();
    for (std::uint64_t i = 0; i < n; ++i) xn = BF::mul(xn, x, wc);
    const BF v = BF::div(BF::mul(BF(n), I, wc), xn, wc);
    const double err = hz_lsum(eI + std::log2(static_cast<double>(n)) - hz_l2(xn), hz_l2(v) + std::log2(static_cast<double>(n) + 3.0) - wd);
    return sp_finish(v, err, want);
}

BF sp_neg_constant(const BFC& ctx, BF (*fn)(const BFC&)) {                       
    return hzdetail::hz_drive(32, ctx, [fn](std::size_t want) { return zt_val{-fn(sp_ctx(want)), 1.0}; });
}

} // namespace spdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat fresnel_c(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))       return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero())     return x;
    if (x.is_infinite()) return BigFloat::one(x.signbit()).scaled_pow2(-1);
    return hzdetail::hz_drive(sp_fresnel_guard(x, ctx.precision), ctx, [&](std::size_t want) { return sp_fresnel_raw(false, x, want); });
}

BigFloat fresnel_s(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))       return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero())     return x;
    if (x.is_infinite()) return BigFloat::one(x.signbit()).scaled_pow2(-1);
    return hzdetail::hz_drive(sp_fresnel_guard(x, ctx.precision), ctx, [&](std::size_t want) { return sp_fresnel_raw(true, x, want); });
}

BigFloat sin_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))   return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero()) return x;
    if (x.is_infinite()) return x.signbit() ? sp_neg_constant(ctx, &constants::half_pi) : constants::half_pi(ctx);
    return hzdetail::hz_drive(sp_sici_guard(0, x, ctx.precision), ctx, [&](std::size_t want) { return sp_sici_raw(0, x, want); });
}

BigFloat cos_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))   return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero()) return BigFloat::infinity(true);
    if (x.signbit()) return BigFloat::nan();                                               
    if (x.is_infinite()) return BigFloat::zero();
    return hzdetail::hz_drive(sp_sici_guard(1, x, ctx.precision), ctx, [&](std::size_t want) { return sp_sici_raw(1, x, want); });
}

BigFloat sinh_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))                     return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero() || x.is_infinite()) return x;
    return hzdetail::hz_drive(32, ctx, [&](std::size_t want) { return sp_sici_raw(2, x, want); });
}

BigFloat cosh_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))   return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero()) return BigFloat::infinity(true);
    if (x.signbit()) return BigFloat::nan();                                               
    if (x.is_infinite()) return x;
    return hzdetail::hz_drive(32, ctx, [&](std::size_t want) { return sp_sici_raw(3, x, want); });
}

BigFloat exp_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))       return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero())     return BigFloat::infinity(true);
    if (x.is_infinite()) return x.signbit() ? BigFloat::zero(true) : x;
    return hzdetail::hz_drive(sp_ei_guard(x, ctx.precision), ctx, [&](std::size_t want) { return sp_ei_raw(x, want); });
}

BigFloat exp_integral_generalized(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x)) return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero()) return (n >= 2) ? BigFloat::div(BigFloat::one(), BigFloat(n - 1), ctx) : BigFloat::infinity();
    if (x.is_infinite()) return x.signbit() ? (n <= 0 ? BigFloat::infinity(true) : BigFloat::nan()) : BigFloat::zero();
    if (x.signbit() && n > 0) return BigFloat::nan();

    if (n < 0) {
        const std::uint64_t m = static_cast<std::uint64_t>(-(n + 1)) + 1u;
        if (m > gmdetail::fact_n_cap()) return BigFloat::undefined();
        return hzdetail::hz_drive(sp_fact_poly_guard(x, m), ctx, [&](std::size_t want) { return sp_en_neg_raw(x, m, want); });
    }

    const std::uint64_t un = static_cast<std::uint64_t>(n);
    return hzdetail::hz_drive(sp_en_guard(x.abs(), un, ctx.precision), ctx, [&](std::size_t want) { return sp_en_raw(x, un, want); });
}

BigFloat log_integral(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))       return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (x.is_zero())     return BigFloat::zero();
    if (x.signbit())     return BigFloat::nan();
    if (x.is_infinite()) return x;
    if (BigFloat::compare(x, BigFloat::one()) == BigFloat::ordering::equal) return BigFloat::infinity(true);
    const BigFloat u = ln(x, sp_ctx(64));
    return hzdetail::hz_drive(sp_ei_guard(u, ctx.precision), ctx, [&](std::size_t want) { return sp_li_raw(x, want); });
}

BigFloat log_integral_generalized(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(x))       return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (n == 1)          return log_integral(x, ctx);
    if (x.is_zero())     return BigFloat::zero();
    if (x.signbit())     return BigFloat::nan();
    if (x.is_infinite()) return x;
    if (n == 0)          return x.rounded(ctx);
    const bool at_one = BigFloat::compare(x, BigFloat::one()) == BigFloat::ordering::equal;

    if (n < 0) {
        const std::uint64_t m = static_cast<std::uint64_t>(-(n + 1)) + 1u;
        if (m > gmdetail::fact_n_cap()) return BigFloat::undefined();
        if (at_one) return BigFloat(gmdetail::fact_range(1, m + 1), (m & 1u) != 0).rounded(ctx);
        return hzdetail::hz_drive(sp_linneg_guard(x, m), ctx, [&](std::size_t want) { return sp_linneg_raw(x, m, want); });
    }

    const std::uint64_t un = static_cast<std::uint64_t>(n);
    if (at_one) return (un & 1u) ? BigFloat::infinity(true) : BigFloat::undefined();
    return hzdetail::hz_drive(sp_lin_guard(x, un, ctx.precision), ctx, [&](std::size_t want) { return sp_lin_raw(x, un, want); });
}

BigFloat clausen(const BigFloat& theta, std::int64_t s, const BigFloatContext& ctx) {
    using namespace spdetail;
    if (sp_bad(theta))       return theta.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (theta.is_infinite()) return BigFloat::undefined();

    if (s <= 0) {
        const std::uint64_t m = static_cast<std::uint64_t>(-s);
        if (theta.is_zero()) return (m & 1u) ? BigFloat::infinity(((m >> 1) & 1u) == 0) : BigFloat::undefined();
        return hzdetail::hz_drive(32, ctx, [&](std::size_t want) { return sp_clausen_neg_raw(theta, m, want); });
    }

    const std::uint64_t us = static_cast<std::uint64_t>(s);

    if (theta.is_zero()) {
        if (us == 1)     return BigFloat::infinity();
        if (us % 2 == 0) return theta;
        return riemann_zeta(BigFloat(us), ctx);
    }

    return hzdetail::hz_drive(32, ctx, [&](std::size_t want) { return sp_clausen_raw(theta, us, want); });
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {
namespace spdetail {

BigFloat sp_debye_public(int kind, const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    if (sp_bad(x))   return x.is_nan() ? BigFloat::nan() : BigFloat::undefined();
    if (n < 1)       return BigFloat::undefined();
    if (x.signbit() && !x.is_zero()) return BigFloat::nan();
    const std::uint64_t un = static_cast<std::uint64_t>(n);

    auto total = [&]() {                                                                    
        return hzdetail::hz_drive(32, ctx, [&](std::size_t want) -> zt_val {
            const BFC wc = sp_ctx(want);
            return zt_val{BF::mul(BF(gmdetail::fact_range(1, un + 1)), pldetail::pl_zeta_pos(un + 1, wc), wc), 2.6};
        });
    };

    if (x.is_zero()) {
        if (kind == 0) return BigFloat::zero();
        if (kind == 2) return BigFloat::one();
        return total();
    }

    if (x.is_infinite()) {
        if (kind == 0) return total();
        return BigFloat::zero();
    }

    return hzdetail::hz_drive(32, ctx, [&](std::size_t want) { return sp_debye_raw(kind, x, un, want); });
}

} // namespace spdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo
