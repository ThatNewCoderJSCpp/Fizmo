#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace igdetail {

using tracked::BF;
using tracked::BFC;
using tracked::RV;
using tracked::ninf;
using tracked::pinf;

constexpr std::uint64_t term_cap = 20000000;
constexpr double        ln2      = 0.69314718055994530942;

double dl(const BF& x) { return tracked::l2(x); }

double dbl(const BF& x) {
    const double d = x.to_double();
    if (std::isinf(d)) return d > 0 ? 1.0e300 : -1.0e300;
    return d;
}

bool less(const BF& a, const BF& b) { return BF::compare(a, b) == BF::ordering::less; }

bool greater(const BF& a, const BF& b) { return BF::compare(a, b) == BF::ordering::greater; }

bool equal(const BF& a, const BF& b) { return BF::compare(a, b) == BF::ordering::equal; }

BF big(std::uint64_t k) { return BF(k); }

struct Sum {
    BF     s;
    double err = pinf;
    bool   ok  = false;
};

Sum gamma_series(const BF& a, const BF& x, std::size_t w) {
    const std::size_t wp  = w + 12;
    const BFC         c   = tracked::ctx(wp);
    const double      lx  = dl(x);
    BF                t   = BF::one();
    BF                s   = BF::one();
    double            acc = 1.0;
    Sum               out;

    for (std::uint64_t k = 0; k < term_cap; ++k) {
        const BF den = tracked::exact_add(a, big(k + 1));
        t = BF::div(BF::mul(t, x, c), den, c);
        s = BF::add(s, t, c);
        const double lt = dl(t);
        acc = tracked::lsum(acc, lt + std::log2(2.0 * static_cast<double>(k) + 4.0));
        const BF     den2 = tracked::exact_add(a, big(k + 2));
        if (!greater(den2, x)) continue;
        const double r = std::exp2(lx - dl(den2)) * (1.0 + 1.0e-12);
        if (!(r < 1.0)) continue;
        const double tail = lt + std::log2(r / (1.0 - r));
        if (t.is_zero() || tail < dl(s) - static_cast<double>(wp) - 2.0) {
            out.s   = s;
            out.err = tracked::lsum(acc - static_cast<double>(wp) + 0.1, dl(s) + std::log2(static_cast<double>(k) + 2.0) - static_cast<double>(wp), t.is_zero() ? ninf : tail);
            out.ok  = true;
            return out;
        }
    }

    return out;
}

double tail_factor(double ad, double xd, double kd) {
    const double m = ad - kd - 1.0;
    if (m <= 0.0) return 0.0;
    const double gap = xd - m;
    if (!(gap > 0.0)) return 1.0e301;
    return std::log2(xd / gap) + 1.0e-9;
}

Sum gamma_asym(const BF& a, const BF& x, std::size_t w) {
    const std::size_t wp    = w + 12;
    const BFC         c     = tracked::ctx(wp);
    const double      ad    = dbl(a);
    const double      xd    = dbl(x) * (1.0 - 1.0e-15);
    BF                u     = BF::one();
    BF                A     = BF::one();
    double            acc   = 1.0;
    double            amax  = 0.0;
    double            lprev = 0.0;
    Sum               out;

    for (std::uint64_t k = 1; k < term_cap; ++k) {
        const BF num = tracked::exact_sub(a, big(k));
        u = BF::div(BF::mul(u, num, c), x, c);
        const double lu = dl(u);
        const double kd = static_cast<double>(k);
        const double lf = tail_factor(ad, xd, kd);

        if (lf < 1.0e300 && (u.is_zero() || lu + lf < dl(A) - static_cast<double>(wp) - 2.0)) {
            out.s   = A;
            out.err = tracked::lsum(u.is_zero() ? ninf : lu + lf, acc - static_cast<double>(wp) + 0.1, amax + std::log2(kd + 1.0) - static_cast<double>(wp));
            out.ok  = true;
            return out;
        }

        if (kd > ad + 1.0 && lu >= lprev) return out;
        A = BF::add(A, u, c);
        acc = tracked::lsum(acc, lu + std::log2(2.0 * kd + 2.0));
        const double la = dl(A);
        if (la > amax) amax = la;
        lprev = lu;
    }

    return out;
}

bool asym_plan(double ad, double lx, std::size_t w, double& n) {
    double l    = 0.0;
    double prev = 0.0;
    const double target = -(static_cast<double>(w) + 16.0);
    const double xd     = std::exp2(std::min(lx, 1000.0));

    for (std::uint64_t k = 1; k < 4000000; ++k) {
        const double kd = static_cast<double>(k);
        const double d  = std::fabs(ad - kd);
        if (d == 0.0) {
            n = kd;
            return true;
        }
        l += std::log2(d) - lx;
        const double lf = tail_factor(ad, xd, kd);
        if (lf < 1.0e300 && l + lf < target) {
            n = kd;
            return true;
        }
        if (kd > ad + 1.0 && (l >= prev || d >= xd)) return false;
        prev = l;
    }

    return false;
}

double series_cost(double ad, double xd, std::size_t w) {
    const double bits = static_cast<double>(w) + 16.0;
    const double peak = xd > ad ? xd - ad : 0.0;
    return peak + std::sqrt(2.0 * std::max(xd, 1.0) * bits * ln2) + 2.0 * bits;
}

double extra_bits(double magnitude) {
    if (!(magnitude > 1.0)) return 8.0;
    return std::ceil(std::log2(magnitude)) + 8.0;
}

double gamma_magnitude(const BF& a, const BF& x) {
    const double ad = dbl(a);
    const double lx = dl(x) * ln2;
    const double xd = dbl(x);
    const double lg = (ad > 0.0 && ad < 1.0e300) ? std::fabs(std::lgamma(ad + 1.0)) : 1.0e300;
    return std::fabs(ad * lx) + std::fabs(xd) + lg + 2.0;
}

RV exp_or_zero(const RV& L, std::size_t w) {
    if (L.ok() && L.v.signbit() && dl(L.v) > 46.0) return RV(BF::zero(), ninf);
    return tracked::exp(L, w);
}

RV gamma_exact_impl(const BF& a, const BF& x, std::size_t w, bool upper) {
    const double ad = dbl(a);
    const double xd = dbl(x);
    const double lx = dl(x);
    const std::size_t wp = w + static_cast<std::size_t>(extra_bits(gamma_magnitude(a, x)));
    const std::size_t wr = w + 4;
    bool use_asym = false;

    if (xd > ad) {
        double n = 0.0;
        use_asym = asym_plan(ad, lx, wr, n) && n < series_cost(ad, xd, wr);
    }

    if (use_asym) {
        const Sum A = gamma_asym(a, x, wr);

        if (A.ok) {
            const RV am1 = tracked::exact(tracked::exact_sub(a, BF::one()));
            const RV L   = tracked::sub(tracked::sub(tracked::mul(am1, tracked::ln(tracked::exact(x), wp), wp), tracked::exact(x), wp),
                                        tracked::log_gamma(tracked::exact(a), wp), wp);
            const RV Q   = tracked::mul(exp_or_zero(L, wr), RV(A.s, A.err), wr);
            if (Q.ok() && Q.v.is_zero() && Q.e == ninf) return upper ? Q : tracked::exact(BF::one());
            return upper ? Q : tracked::one_minus(Q, wr);
        }
    }

    const Sum S = gamma_series(a, x, wr);
    if (!S.ok) return RV(BF::nan(), pinf);
    const RV a1 = tracked::exact(tracked::exact_add(a, BF::one()));
    const RV L  = tracked::sub(tracked::sub(tracked::mul(tracked::exact(a), tracked::ln(tracked::exact(x), wp), wp), tracked::exact(x), wp),
                               tracked::log_gamma(a1, wp), wp);
    const RV P  = tracked::mul(exp_or_zero(L, wr), RV(S.s, S.err), wr);
    if (P.ok() && P.v.is_zero() && P.e == ninf) return upper ? tracked::exact(BF::one()) : P;
    return upper ? tracked::one_minus(P, wr) : P;
}

struct BetaSide {
    BF   p;
    BF   q;
    BF   s;
    BF   t;
    bool direct;
};

Sum beta_series(const BF& x, const BF& a, const BF& b, std::size_t w) {
    const std::size_t wp  = w + 12;
    const BFC         c   = tracked::ctx(wp);
    const BF          ab  = tracked::exact_add(a, b);
    const BF          a1  = tracked::exact_add(a, BF::one());
    const bool        bge = !less(b, BF::one());
    const double      lx  = dl(x);
    const double      xr  = std::exp2(lx) * (1.0 + 1.0e-12);
    BF                t   = BF::one();
    BF                s   = BF::one();
    double            acc = 1.0;
    Sum               out;

    for (std::uint64_t k = 0; k < term_cap; ++k) {
        const BF num = tracked::exact_add(ab, big(k));
        const BF den = tracked::exact_add(a1, big(k));
        t = BF::div(BF::mul(BF::mul(t, num, c), x, c), den, c);
        s = BF::add(s, t, c);
        const double lt = dl(t);
        acc = tracked::lsum(acc, lt + std::log2(3.0 * static_cast<double>(k) + 6.0));
        const double rn = std::exp2(dl(tracked::exact_add(ab, big(k + 1))) + lx - dl(tracked::exact_add(a1, big(k + 1)))) * (1.0 + 1.0e-12);
        const double R  = bge ? rn : xr;
        if (!(R < 1.0)) continue;
        const double tail = lt + std::log2(R / (1.0 - R));
        if (t.is_zero() || tail < dl(s) - static_cast<double>(wp) - 2.0) {
            out.s   = s;
            out.err = tracked::lsum(acc - static_cast<double>(wp) + 0.1, dl(s) + std::log2(static_cast<double>(k) + 2.0) - static_cast<double>(wp), t.is_zero() ? ninf : tail);
            out.ok  = true;
            return out;
        }
    }

    return out;
}

double beta_cost(double lx, double ad, double bd, std::size_t w) {
    const double bits = static_cast<double>(w) + 16.0;
    const double x    = std::exp2(lx);
    if (!(x < 1.0)) return 1.0e300;
    const double peak = std::max(0.0, ((ad + bd) * x - ad - 1.0) / (1.0 - x));
    const double nl   = -lx;
    return peak + 2.0 * std::sqrt((peak + 1.0) * bits) + bits / std::max(nl, 1.0e-300);
}

RV beta_exact_impl(const BF& x, const BF& a, const BF& b, std::size_t w, bool complement) {
    const BF     y      = tracked::exact_sub(BF::one(), x);
    const double ad     = dbl(a);
    const double bd     = dbl(b);
    const bool   direct = beta_cost(dl(x), ad, bd, w) <= beta_cost(dl(y), bd, ad, w);
    const BF&    p      = direct ? x : y;
    const BF&    q      = direct ? y : x;
    const BF&    s      = direct ? a : b;
    const BF&    t      = direct ? b : a;
    const std::size_t wr = w + 4;
    const double sd     = dbl(s);
    const double td     = dbl(t);
    const double mag    = std::fabs(sd * dl(p) * ln2) + std::fabs(td * dl(q) * ln2) + std::fabs(std::log(std::max(sd, 1.0e-300)))
                        + std::fabs(std::lgamma(std::min(sd, 1.0e300))) + std::fabs(std::lgamma(std::min(td, 1.0e300)))
                        + std::fabs(std::lgamma(std::min(sd + td, 1.0e300))) + 2.0;
    const std::size_t wp = w + static_cast<std::size_t>(extra_bits(mag));
    const Sum F = beta_series(p, s, t, wr);
    if (!F.ok) return RV(BF::nan(), pinf);
    const RV S  = tracked::exact(s);
    const RV T  = tracked::exact(t);
    const RV L1 = tracked::mul(S, tracked::ln(tracked::exact(p), wp), wp);
    const RV L2 = tracked::mul(T, tracked::ln(tracked::exact(q), wp), wp);
    const RV L  = tracked::sub(tracked::sub(tracked::add(L1, L2, wp), tracked::ln(S, wp), wp), tracked::log_beta(S, T, wp), wp);
    const RV V  = tracked::mul(exp_or_zero(L, wr), RV(F.s, F.err), wr);
    const bool want_v = direct != complement;
    if (V.ok() && V.v.is_zero() && V.e == ninf) return want_v ? V : tracked::exact(BF::one());
    return want_v ? V : tracked::one_minus(V, wr);
}

} // namespace igdetail

namespace tracked {

double log2_gamma_density(const BF& a, const BF& x) {
    const std::size_t w   = 64 + static_cast<std::size_t>(igdetail::extra_bits(igdetail::gamma_magnitude(a, x)));
    const BFC         c   = ctx(w);
    const BF          am1 = exact_sub(a, BF::one());
    const BF          L   = BF::sub(BF::sub(BF::mul(am1, math::ln(x, c), c), x, c), math::log_gamma(a, c), c);
    return igdetail::dbl(L) / igdetail::ln2;
}

double log2_beta_density(const BF& x, const BF& a, const BF& b) {
    const BF          y   = exact_sub(BF::one(), x);
    const double      ad  = igdetail::dbl(a);
    const double      bd  = igdetail::dbl(b);
    const double      mag = std::fabs(ad * l2(x)) + std::fabs(bd * l2(y)) + std::fabs(std::lgamma(std::min(ad + bd, 1.0e300))) + 2.0;
    const std::size_t w   = 64 + static_cast<std::size_t>(igdetail::extra_bits(mag));
    const BFC         c   = ctx(w);
    const BF          A   = BF::mul(exact_sub(a, BF::one()), math::ln(x, c), c);
    const BF          B   = BF::mul(exact_sub(b, BF::one()), math::ln(y, c), c);
    const BF          L   = BF::sub(BF::add(A, B, c), math::log_beta(a, b, c), c);
    return igdetail::dbl(L) / igdetail::ln2;
}

RV gamma_pq_exact(const BF& a, const BF& x, std::size_t w, bool upper) {
    if (x.is_zero()) return exact(upper ? BF::one() : BF::zero());
    if (x.is_infinite()) return exact(upper ? BF::zero() : BF::one());
    return igdetail::gamma_exact_impl(a, x, w, upper);
}

RV gamma_pq(const BF& a, const RV& x, std::size_t w, bool upper) {
    if (!x.ok()) return failed();
    if (x.e == ninf) return gamma_pq_exact(a, x.v, w, upper);
    if (!certain_positive(x)) return failed();
    const RV     core = gamma_pq_exact(a, x.v, w, upper);
    if (!core.ok()) return core;
    const double eps  = std::exp2(x.e);
    const double xd   = igdetail::dbl(x.v);
    const double xl   = xd - eps;
    if (!(xl > 0.0)) return failed();
    const double g = std::fabs((igdetail::dbl(a) - 1.0) / xl) + 1.0;
    if (g * eps > 0.5) return failed();
    return RV(core.v, lsum(core.e, log2_gamma_density(a, x.v) + 0.75 + x.e));
}

RV beta_i_exact(const BF& x, const BF& a, const BF& b, std::size_t w, bool complement) {
    if (x.is_zero()) return exact(complement ? BF::one() : BF::zero());
    if (igdetail::equal(x, BF::one())) return exact(complement ? BF::zero() : BF::one());
    if (igdetail::equal(a, b) && igdetail::equal(x, BF::one().scaled_pow2(-1))) return exact(BF::one().scaled_pow2(-1));
    return igdetail::beta_exact_impl(x, a, b, w, complement);
}

RV beta_i(const RV& x, const BF& a, const BF& b, std::size_t w, bool complement) {
    if (!x.ok()) return failed();
    if (x.e == ninf) return beta_i_exact(x.v, a, b, w, complement);
    if (!certain_positive(x)) return failed();
    const RV y = one_minus(x, w + 16);
    if (!certain_positive(y)) return failed();
    const RV core = beta_i_exact(x.v, a, b, w, complement);
    if (!core.ok()) return core;
    const double eps = std::exp2(x.e);
    const double xl  = igdetail::dbl(x.v) - eps;
    const double yl  = igdetail::dbl(y.v) - std::exp2(y.e) - eps;
    if (!(xl > 0.0) || !(yl > 0.0)) return failed();
    const double g = std::fabs((igdetail::dbl(a) - 1.0) / xl) + std::fabs((igdetail::dbl(b) - 1.0) / yl);
    if (g * eps > 0.5) return failed();
    return RV(core.v, lsum(core.e, log2_beta_density(x.v, a, b) + 0.75 + x.e));
}

} // namespace tracked

namespace igdetail {

bool bad(const BF& v) { return v.is_unordered(); }

BF bad_of(const BF& a) { return a.is_nan() ? BF::nan() : BF::undefined(); }

bool positive_finite(const BF& v) { return v.is_finite() && !v.signbit() && !v.is_zero(); }

BF exp_lowp(const BF& v) { return math::exp(v, tracked::ctx(64)); }

BF gamma_density_bf(const BF& a, const BF& x, std::size_t w) {
    const std::size_t wp  = w + static_cast<std::size_t>(extra_bits(gamma_magnitude(a, x)));
    const BFC         c   = tracked::ctx(wp);
    const BF          am1 = tracked::exact_sub(a, BF::one());
    const BF          L   = BF::sub(BF::sub(BF::mul(am1, math::ln(x, c), c), x, c), math::log_gamma(a, c), c);
    return math::exp(L, tracked::ctx(w));
}

BF beta_density_bf(const BF& x, const BF& a, const BF& b, std::size_t w) {
    const BF          y   = tracked::exact_sub(BF::one(), x);
    const double      ad  = dbl(a);
    const double      bd  = dbl(b);
    const double      mag = std::fabs(ad * dl(x)) + std::fabs(bd * dl(y)) + std::fabs(std::lgamma(std::min(ad + bd, 1.0e300))) + 2.0;
    const std::size_t wp  = w + static_cast<std::size_t>(extra_bits(mag));
    const BFC         c   = tracked::ctx(wp);
    const BF          A   = BF::mul(tracked::exact_sub(a, BF::one()), math::ln(x, c), c);
    const BF          B   = BF::mul(tracked::exact_sub(b, BF::one()), math::ln(y, c), c);
    return math::exp(BF::sub(BF::add(A, B, c), math::log_beta(a, b, c), c), tracked::ctx(w));
}

double normal_guess(double p) {
    const double pp = p < 0.5 ? p : 1.0 - p;
    const double t  = std::sqrt(-2.0 * std::log(pp));
    double       x  = (2.30753 + t * 0.27061) / (1.0 + t * (0.99229 + t * 0.04481)) - t;
    return p < 0.5 ? x : -x;
}

double log_target(const BF& t) { return dl(t) * ln2; }

BF gamma_guess(const BF& a, const BF& target, bool upper) {
    const double ad = dbl(a);
    const double lt = log_target(target);
    const double lp = upper ? std::log1p(-std::exp(lt)) : lt;
    const double lq = upper ? lt : std::log1p(-std::exp(lt));
    double       lx = 0.0;

    if (ad > 1.0) {
        const double p   = std::exp(lp);
        const double z   = (lp < -700.0 || lq < -700.0) ? (lp < lq ? -std::sqrt(-2.0 * lp) : std::sqrt(-2.0 * lq)) : normal_guess(p);
        const double cub = 1.0 - 1.0 / (9.0 * ad) + z / (3.0 * std::sqrt(ad));
        lx = (cub > 0.0) ? std::log(ad) + 3.0 * std::log(cub) : std::log(ad) - 3.0;
        if (lq < -50.0 && cub > 0.0) lx = std::max(lx, std::log(std::max(1.0, -lq + (ad - 1.0) * std::log(std::max(-lq, 1.0)))));
    } else {
        const double t = 1.0 - ad * (0.253 + ad * 0.12);
        if (lp < std::log(t)) lx = (lp - std::log(t)) / ad;
        else                  lx = std::log(std::max(1.0e-300, 1.0 - std::log(std::max(1.0e-300, std::exp(lq) / (1.0 - t)))));
    }

    if (!std::isfinite(lx)) lx = 0.0;
    return exp_lowp(BF(lx));
}

BF gamma_rough(const BF& a, const BF& target, bool upper) {
    BF u = math::ln(gamma_guess(a, target, upper), tracked::ctx(64));
    const BF lt = math::ln(target, tracked::ctx(64));

    for (int it = 0; it < 400; ++it) {
        const BF x = exp_lowp(u);
        const RV G = tracked::gamma_pq_exact(a, x, 64, upper);
        if (bad(G.v)) break;
        double step;

        if (G.v.is_zero()) {
            step = upper ? -1.0 : 1.0;
        } else {
            const double f    = dbl(BF::sub(math::ln(G.v, tracked::ctx(64)), lt, tracked::ctx(64)));
            const double ld   = tracked::log2_gamma_density(a, x) + dl(x) - dl(G.v);
            const double dfdu = (upper ? -1.0 : 1.0) * std::exp2(std::min(ld, 1000.0));
            step = f / dfdu;
        }

        if (!std::isfinite(step)) break;
        if (step > 1.0) step = 1.0;
        if (step < -1.0) step = -1.0;
        u = BF::sub(u, BF(step), tracked::ctx(64));
        if (std::fabs(step) < 1.0e-14) break;
    }

    return exp_lowp(u);
}

BF beta_from_logit(const BF& u, std::size_t w) {
    const BFC c = tracked::ctx(w + 8);
    const BF  e = math::exp(-u, c);
    return BF::div(BF::one(), BF::add(BF::one(), e, c), tracked::ctx(w));
}

BF beta_guess_logit(const BF& a, const BF& b, const BF& target, bool complement) {
    const double ad = dbl(a);
    const double bd = dbl(b);
    const double lt = log_target(target);
    const double lp = complement ? std::log1p(-std::exp(lt)) : lt;
    const double lq = complement ? lt : std::log1p(-std::exp(lt));
    double       lx, ly;

    if (ad >= 1.0 && bd >= 1.0 && lp > -700.0 && lq > -700.0) {
        const double p  = std::exp(lp);
        const double x  = -normal_guess(p);
        const double al = (x * x - 3.0) / 6.0;
        const double h  = 2.0 / (1.0 / (2.0 * ad - 1.0) + 1.0 / (2.0 * bd - 1.0));
        const double ww = x * std::sqrt(al + h) / h - (1.0 / (2.0 * bd - 1.0) - 1.0 / (2.0 * ad - 1.0)) * (al + 5.0 / 6.0 - 2.0 / (3.0 * h));
        const double xx = ad / (ad + bd * std::exp(2.0 * ww));
        lx = std::log(xx);
        ly = std::log1p(-xx);
    } else {
        const double lna = std::log(ad / (ad + bd));
        const double lnb = std::log(bd / (ad + bd));
        const double lt1 = ad * lna - std::log(ad);
        const double lu1 = bd * lnb - std::log(bd);
        const double lw  = std::max(lt1, lu1) + std::log1p(std::exp(std::min(lt1, lu1) - std::max(lt1, lu1)));
        if (lp < lt1 - lw) {
            lx = (std::log(ad) + lw + lp) / ad;
            ly = std::log1p(-std::exp(lx));
        } else {
            ly = (std::log(bd) + lw + lq) / bd;
            lx = std::log1p(-std::exp(ly));
        }
    }

    double u = lx - ly;
    if (!std::isfinite(u)) u = 0.0;
    return BF(u);
}

BF beta_rough(const BF& a, const BF& b, const BF& target, bool complement) {
    BF       u  = beta_guess_logit(a, b, target, complement);
    const BF lt = math::ln(target, tracked::ctx(64));

    for (int it = 0; it < 400; ++it) {
        const BF x = beta_from_logit(u, 64);
        if (x.is_zero() || equal(x, BF::one())) break;
        const RV G = tracked::beta_i_exact(x, a, b, 64, complement);
        if (bad(G.v)) break;
        double step;

        if (G.v.is_zero()) {
            step = complement ? -1.0 : 1.0;
        } else {
            const BF     y    = tracked::exact_sub(BF::one(), x);
            const double f    = dbl(BF::sub(math::ln(G.v, tracked::ctx(64)), lt, tracked::ctx(64)));
            const double ld   = tracked::log2_beta_density(x, a, b) + dl(x) + dl(y) - dl(G.v);
            const double dfdu = (complement ? -1.0 : 1.0) * std::exp2(std::min(ld, 1000.0));
            step = f / dfdu;
        }

        if (!std::isfinite(step)) break;
        if (step > 2.0) step = 2.0;
        if (step < -2.0) step = -2.0;
        u = BF::sub(u, BF(step), tracked::ctx(64));
        if (std::fabs(step) < 1.0e-14) break;
    }

    return beta_from_logit(u, 64);
}

template <class Eval, class Dens, class Check>
BF solve(BF xs, const BF& target, bool decreasing, const BFC& ctx, Eval&& eval, Dens&& dens, Check&& clamp) {
    std::size_t done = 40;

    return tracked::drive(ctx, [&](std::size_t w) -> RV {
        while (done < w + 8) {
            const std::size_t wk = std::min<std::size_t>(2 * done + 16, w + 32);
            const RV          G  = eval(xs, wk);
            if (!G.ok()) return tracked::failed();
            const BF d = dens(xs, wk);
            if (d.is_zero() || !d.is_finite()) return tracked::failed();
            const BFC c    = tracked::ctx(wk);
            BF        step = BF::div(BF::sub(G.v, target, c), d, c);
            if (decreasing) step = -step;
            xs   = clamp(xs, BF::sub(xs, step, c));
            done = std::min<std::size_t>(2 * done, wk - 16);
        }

        const RV G = eval(xs, w + 8);
        if (!G.ok()) return tracked::failed();
        const BF     d  = dens(xs, 64);
        const BF     f  = tracked::exact_sub(G.v, target);
        const double lf = tracked::lsum(dl(f), G.e);
        const double ld = dl(d);
        if (!std::isfinite(ld)) return tracked::failed();
        const double lerr = lf - ld;
        if (lerr > dl(xs) - 20.0) {
            done = 40;
            return tracked::failed();
        }
        return RV(xs, lerr + 1.0);
    });
}

BF gamma_inverse(const BF& a, const BF& t, bool upper, const BFC& ctx) {
    const BF half = BF::one().scaled_pow2(-1);
    bool     side = upper;
    BF       tgt  = t;

    if (greater(t, half)) {
        side = !upper;
        tgt  = tracked::exact_sub(BF::one(), t);
    }

    const BF x0 = gamma_rough(a, tgt, side);
    return solve(x0, tgt, side, ctx,
        [&](const BF& x, std::size_t w) { return tracked::gamma_pq_exact(a, x, w, side); },
        [&](const BF& x, std::size_t w) { return gamma_density_bf(a, x, w); },
        [](const BF& old, const BF& nx) { return (nx.signbit() || nx.is_zero()) ? old.scaled_pow2(-1) : nx; });
}

BF beta_inverse_core(const BF& a, const BF& b, const BF& t, bool complement, const BFC& ctx) {
    const BF half = BF::one().scaled_pow2(-1);
    bool     side = complement;
    BF       tgt  = t;

    if (greater(t, half)) {
        side = !complement;
        tgt  = tracked::exact_sub(BF::one(), t);
    }

    const BF x0 = beta_rough(a, b, tgt, side);
    return solve(x0, tgt, side, ctx,
        [&](const BF& x, std::size_t w) { return tracked::beta_i_exact(x, a, b, w, side); },
        [&](const BF& x, std::size_t w) { return beta_density_bf(x, a, b, w); },
        [](const BF& old, const BF& nx) {
            if (nx.signbit() || nx.is_zero()) return old.scaled_pow2(-1);
            if (!less(nx, BF::one())) return BF::div(BF::add(old, BF::one(), tracked::ctx(old.significand().bit_length() + 8)), BF(2), tracked::ctx(old.significand().bit_length() + 8));
            return nx;
        });
}

BF beta_inverse(const BF& a, const BF& b, const BF& t, bool complement, const BFC& ctx) {
    const BF     half = BF::one().scaled_pow2(-1);
    const bool   flip = greater(t, half);
    const BF     tgt  = flip ? tracked::exact_sub(BF::one(), t) : t;
    const bool   side = flip ? !complement : complement;
    const double u    = dbl(beta_guess_logit(a, b, tgt, side));
    if (!(u > 0.0)) return beta_inverse_core(a, b, t, complement, ctx);

    return tracked::drive(ctx, [&](std::size_t w) {
        const BF yw = beta_inverse_core(b, a, t, !complement, tracked::ctx(w));
        if (bad(yw) || yw.is_zero()) return tracked::failed();
        return RV(tracked::exact_sub(BF::one(), yw), dl(yw) - static_cast<double>(w) + 0.5);
    });
}

RV fib_eval(const BF& x, std::size_t w, bool luc) {
    const double      lx = dl(x);
    const std::size_t wp = w + 8 + static_cast<std::size_t>(std::max(0.0, lx + 1.0));
    const RV          s5 = tracked::sqrt(tracked::exact(BF(5)), wp);
    const RV          ph = tracked::scale2(tracked::add(tracked::exact(BF::one()), s5, wp), -1);
    const RV          E  = tracked::mul(tracked::exact(x), tracked::ln(ph, wp), wp);
    const RV          A  = exp_or_zero(E, wp);
    const RV          B  = tracked::mul(tracked::cospi(tracked::exact(x), wp), exp_or_zero(tracked::neg(E), wp), wp);
    if (luc) return tracked::add(A, B, w + 4);
    return tracked::div(tracked::sub(A, B, w + 4), s5, w + 4);
}

BF gamma_fn(const BF& a, std::size_t w) { return math::gamma(a, tracked::ctx(w)); }

RV with_gamma(const RV& r, const BF& a, std::size_t w) {
    const BF g = gamma_fn(a, w);
    return tracked::mul(r, tracked::rounded(g, w), w);
}

bool negative_integer(const BF& a, std::uint64_t& n) {
    if (!a.is_integer() || !a.signbit()) return false;
    const BF m = a.abs();
    if (greater(m, BF(static_cast<std::uint64_t>(1) << 40))) return false;
    n = static_cast<std::uint64_t>(dbl(m));
    return true;
}

RV upper_negative(const BF& a, const BF& x, std::size_t w) {
    std::uint64_t n = 0;
    const std::size_t wr = w + 4;

    if (a.is_zero() || negative_integer(a, n)) {
        const BF E = math::exp_integral_generalized(x, static_cast<std::int64_t>(n + 1), tracked::ctx(wr));
        if (n == 0) return tracked::rounded(E, wr);
        const BF P = math::pow(x, BigInt(-static_cast<std::int64_t>(n)), tracked::ctx(wr));
        return tracked::mul(tracked::rounded(E, wr), tracked::rounded(P, wr), wr);
    }

    const double steps = std::ceil(-dbl(a));
    if (!(steps < 100000.0)) return RV(BF::nan(), pinf);
    const std::uint64_t m  = static_cast<std::uint64_t>(steps);
    const std::size_t   wp = wr + 16 + static_cast<std::size_t>(extra_bits(gamma_magnitude(a.abs(), x)));
    const BF            top = tracked::exact_add(a, big(m));
    RV G = with_gamma(tracked::gamma_pq_exact(top, x, wp, true), top, wp);
    const RV lx = tracked::ln(tracked::exact(x), wp);

    for (std::uint64_t j = m; j-- > 0;) {
        const BF s  = tracked::exact_add(a, big(j));
        const RV ex = exp_or_zero(tracked::sub(tracked::mul(tracked::exact(s), lx, wp), tracked::exact(x), wp), wp);
        G = tracked::div(tracked::sub(G, ex, wp), tracked::exact(s), wp);
    }

    return G;
}

} // namespace igdetail

BigFloat log1p(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (x.is_infinite()) return x.signbit() ? BF::nan() : x;
    if (x.is_zero()) return x;
    const int cmp = static_cast<int>(BF::compare(x, BF::one(true)));
    if (cmp < 0) return BF::nan();
    if (cmp == 0) return BF::infinity(true);
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::ln1p(tracked::exact(x), w); });
}

BigFloat expm1(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (x.is_infinite()) return x.signbit() ? BF::one(true) : x;
    if (x.is_zero()) return x;
    if (x.signbit() && dl(x) > 46.0) return tracked::drive(ctx, [&](std::size_t) { return RV(BF::one(true), -1.0e18); });
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::expm1(tracked::exact(x), w); });
}

BigFloat fibonacci(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (x.is_infinite()) return BF::nan();

    if (x.is_integer() && dl(x) < 20.0) {
        const std::int64_t n = static_cast<std::int64_t>(dbl(x));
        return BF(multiprecision::sequences::fibonacci(n)).rounded(ctx);
    }

    return tracked::drive(ctx, [&](std::size_t w) { return fib_eval(x, w, false); });
}

BigFloat lucas(const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (x.is_infinite()) return BF::nan();

    if (x.is_integer() && dl(x) < 20.0) {
        const std::int64_t n = static_cast<std::int64_t>(dbl(x));
        return BF(multiprecision::sequences::lucas(n)).rounded(ctx);
    }

    return tracked::drive(ctx, [&](std::size_t w) { return fib_eval(x, w, true); });
}

BigFloat regularized_gamma_p(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(x)) return bad_of(x);
    if (!positive_finite(a) || x.signbit()) return BF::nan();
    if (x.is_zero()) return BF::zero();
    if (x.is_infinite()) return BF::one();
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::gamma_pq_exact(a, x, w, false); });
}

BigFloat regularized_gamma_q(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(x)) return bad_of(x);
    if (!positive_finite(a) || x.signbit()) return BF::nan();
    if (x.is_zero()) return BF::one();
    if (x.is_infinite()) return BF::zero();
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::gamma_pq_exact(a, x, w, true); });
}

BigFloat lower_incomplete_gamma(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(x)) return bad_of(x);
    if (!a.is_finite() || x.signbit()) return BF::nan();
    if (x.is_zero()) return a.signbit() || a.is_zero() ? BF::nan() : BF::zero();
    std::uint64_t n = 0;
    if (a.is_zero() || negative_integer(a, n)) return BF::infinity();
    if (x.is_infinite()) return math::gamma(a, ctx);

    if (a.signbit()) {
        return tracked::drive(ctx, [&](std::size_t w) {
            const RV g = tracked::rounded(gamma_fn(a, w + 8), w + 8);
            return tracked::sub(g, upper_negative(a, x, w + 8), w + 4);
        });
    }

    return tracked::drive(ctx, [&](std::size_t w) { return with_gamma(tracked::gamma_pq_exact(a, x, w + 4, false), a, w + 4); });
}

BigFloat upper_incomplete_gamma(const BigFloat& a, const BigFloat& x, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(x)) return bad_of(x);
    if (!a.is_finite() || x.signbit()) return BF::nan();
    if (x.is_infinite()) return BF::zero();
    if (x.is_zero()) return positive_finite(a) ? math::gamma(a, ctx) : BF::infinity();
    if (a.signbit() || a.is_zero()) return tracked::drive(ctx, [&](std::size_t w) { return upper_negative(a, x, w); });
    return tracked::drive(ctx, [&](std::size_t w) { return with_gamma(tracked::gamma_pq_exact(a, x, w + 4, true), a, w + 4); });
}

BigFloat inverse_regularized_gamma_p(const BigFloat& a, const BigFloat& p, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(p)) return bad_of(p);
    if (!positive_finite(a) || p.signbit() || greater(p, BF::one())) return BF::nan();
    if (p.is_zero()) return BF::zero();
    if (equal(p, BF::one())) return BF::infinity();
    return gamma_inverse(a, p, false, ctx);
}

BigFloat inverse_regularized_gamma_q(const BigFloat& a, const BigFloat& q, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(a)) return bad_of(a);
    if (bad(q)) return bad_of(q);
    if (!positive_finite(a) || q.signbit() || greater(q, BF::one())) return BF::nan();
    if (q.is_zero()) return BF::infinity();
    if (equal(q, BF::one())) return BF::zero();
    return gamma_inverse(a, q, true, ctx);
}

namespace igdetail {

bool beta_domain(const BF& x, const BF& a, const BF& b) {
    return positive_finite(a) && positive_finite(b) && x.is_finite() && !x.signbit() && !greater(x, BF::one());
}

} // namespace igdetail

BigFloat regularized_beta(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (bad(a)) return bad_of(a);
    if (bad(b)) return bad_of(b);
    if (!beta_domain(x, a, b)) return BF::nan();
    if (x.is_zero() || equal(x, BF::one()) || (equal(a, b) && equal(x, BF::one().scaled_pow2(-1)))) return tracked::beta_i_exact(x, a, b, 64, false).v.rounded(ctx);
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::beta_i_exact(x, a, b, w, false); });
}

BigFloat regularized_beta_complement(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (bad(a)) return bad_of(a);
    if (bad(b)) return bad_of(b);
    if (!beta_domain(x, a, b)) return BF::nan();
    if (x.is_zero() || equal(x, BF::one()) || (equal(a, b) && equal(x, BF::one().scaled_pow2(-1)))) return tracked::beta_i_exact(x, a, b, 64, true).v.rounded(ctx);
    return tracked::drive(ctx, [&](std::size_t w) { return tracked::beta_i_exact(x, a, b, w, true); });
}

BigFloat incomplete_beta(const BigFloat& x, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(x)) return bad_of(x);
    if (bad(a)) return bad_of(a);
    if (bad(b)) return bad_of(b);
    if (!beta_domain(x, a, b)) return BF::nan();
    if (x.is_zero()) return BF::zero();
    if (equal(x, BF::one())) return math::beta(a, b, ctx);
    return tracked::drive(ctx, [&](std::size_t w) {
        const RV I = tracked::beta_i_exact(x, a, b, w + 4, false);
        return tracked::mul(I, tracked::rounded(math::beta(a, b, tracked::ctx(w + 4)), w + 4), w + 4);
    });
}

BigFloat inverse_regularized_beta(const BigFloat& p, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(p)) return bad_of(p);
    if (bad(a)) return bad_of(a);
    if (bad(b)) return bad_of(b);
    if (!beta_domain(p, a, b)) return BF::nan();
    if (p.is_zero()) return BF::zero();
    if (equal(p, BF::one())) return BF::one();
    if (equal(a, b) && equal(p, BF::one().scaled_pow2(-1))) return BF::one().scaled_pow2(-1);
    return beta_inverse(a, b, p, false, ctx);
}

BigFloat inverse_regularized_beta_complement(const BigFloat& q, const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
    using namespace igdetail;
    if (bad(q)) return bad_of(q);
    if (bad(a)) return bad_of(a);
    if (bad(b)) return bad_of(b);
    if (!beta_domain(q, a, b)) return BF::nan();
    if (q.is_zero()) return BF::one();
    if (equal(q, BF::one())) return BF::zero();
    if (equal(a, b) && equal(q, BF::one().scaled_pow2(-1))) return BF::one().scaled_pow2(-1);
    return beta_inverse(a, b, q, true, ctx);
}

} // namespace math
} // namespace fizmo
