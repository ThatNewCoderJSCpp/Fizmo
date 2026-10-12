#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace tracked {

BFC ctx(std::size_t w) { return BFC(BFC::clamp_precision(w), RoundingMode::nearest_even); }

double l2(const BF& x) { return cxsf::l2(x); }

double l2(const RV& x) { return cxsf::l2(x.v); }

double lsum(double a, double b) {
    if (a == pinf || b == pinf) return pinf;
    return cxsf::lsum(a, b);
}

double lsum(double a, double b, double c) { return lsum(lsum(a, b), c); }

double lsum(double a, double b, double c, double d) { return lsum(lsum(a, b), lsum(c, d)); }

double rel(const RV& x) {
    if (x.e == ninf) return ninf;
    if (x.v.is_zero()) return pinf;
    return x.e - l2(x.v);
}

double to_double(const BF& x) { return x.to_double(); }

BF exact_add(const BF& a, const BF& b) { return BF::add(a, b, BFC(BFC::max_prec, RoundingMode::nearest_even)); }

BF exact_sub(const BF& a, const BF& b) { return BF::sub(a, b, BFC(BFC::max_prec, RoundingMode::nearest_even)); }

namespace {

double round_err(const BF& v, std::size_t w) { return l2(v) - static_cast<double>(w); }

bool small_rel(const RV& a, double limit) { return a.e == ninf || rel(a) < limit; }

double psi_bound(const RV& a) {
    const double ad = std::fabs(to_double(a.v));
    if (!(ad > 0.0)) return pinf;
    const double spread = (a.e == ninf) ? 0.0 : std::exp2(a.e);
    const double lo     = ad - spread;
    const double hi     = ad + spread;
    if (!(lo > 0.0)) return pinf;
    const double ln_lo = std::fabs(std::log(lo));
    const double ln_hi = std::fabs(std::log(hi));
    return std::log2((ln_lo > ln_hi ? ln_lo : ln_hi) + 1.0 / lo + 1.0);
}

} // namespace

RV rounded(const BF& v, std::size_t w) {
    const BF r = v.rounded(ctx(w));
    if (BF::compare(r, v) == BF::ordering::equal) return RV(r, ninf);
    return RV(r, round_err(r, w));
}

RV add(const RV& a, const RV& b, std::size_t w) {
    if (!a.ok() || !b.ok()) return failed();
    const BF v = BF::add(a.v, b.v, ctx(w));
    return RV(v, lsum(a.e, b.e, round_err(v, w)));
}

RV sub(const RV& a, const RV& b, std::size_t w) {
    if (!a.ok() || !b.ok()) return failed();
    const BF v = BF::sub(a.v, b.v, ctx(w));
    return RV(v, lsum(a.e, b.e, round_err(v, w)));
}

RV mul(const RV& a, const RV& b, std::size_t w) {
    if (!a.ok() || !b.ok()) return failed();
    const BF v = BF::mul(a.v, b.v, ctx(w));
    return RV(v, lsum(l2(a.v) + b.e, l2(b.v) + a.e, a.e + b.e, round_err(v, w)));
}

RV div(const RV& a, const RV& b, std::size_t w) {
    if (!a.ok() || !b.ok() || b.v.is_zero() || !small_rel(b, -2.0)) return failed();
    const BF     v  = BF::div(a.v, b.v, ctx(w));
    const double rb = rel(b);
    return RV(v, lsum(a.e - l2(b.v) + 0.5, l2(v) + rb + 0.5, round_err(v, w)));
}

RV neg(const RV& a) { return RV(-a.v, a.e); }

RV abs(const RV& a) { return RV(a.v.abs(), a.e); }

RV scale2(const RV& a, std::int64_t k) { return RV(a.v.scaled_pow2(k), a.e == ninf ? ninf : a.e + static_cast<double>(k)); }

RV sqr(const RV& a, std::size_t w) { return mul(a, a, w); }

RV sqrt(const RV& a, std::size_t w) {
    if (!a.ok()) return failed();
    if (a.v.is_zero() && a.e == ninf) return a;
    if (a.v.signbit() || a.v.is_zero() || !small_rel(a, -2.0)) return failed();
    const BF v = math::sqrt(a.v, ctx(w));
    return RV(v, lsum(l2(v) + rel(a), round_err(v, w)));
}

RV exp(const RV& a, std::size_t w) {
    if (!a.ok() || a.e > -2.0) return failed();
    const BF v = math::exp(a.v, ctx(w));
    if (!v.is_finite()) return RV(v, ninf);
    return RV(v, lsum(l2(v) + a.e + 0.5, round_err(v, w)));
}

RV ln(const RV& a, std::size_t w) {
    if (!a.ok() || a.v.signbit() || a.v.is_zero() || !small_rel(a, -2.0)) return failed();
    const BF v = math::ln(a.v, ctx(w));
    return RV(v, lsum(rel(a) + 0.5, round_err(v, w)));
}

RV ln1p(const RV& a, std::size_t w) {
    if (!a.ok()) return failed();
    if (a.v.is_zero()) return RV(a.v, a.e == ninf ? ninf : a.e + 0.5);
    const double la = l2(a.v);
    const double wd = static_cast<double>(w);

    if (la < -0.5 * wd - 4.0) {
        const BFC c = ctx(w + 4);
        const BF  h = BF::mul(a.v, a.v, c).scaled_pow2(-1);
        const BF  v = BF::sub(a.v, h, ctx(w));
        return RV(v, lsum(a.e + 0.5, 3.0 * la, round_err(v, w) + 0.1));
    }

    const BF u = exact_add(BF::one(), a.v);
    if (u.signbit() || u.is_zero()) return failed();
    const double lu = l2(u);
    if (a.e != ninf && a.e - lu > -2.0) return failed();
    const BF v = math::ln(u, ctx(w));
    return RV(v, lsum(a.e == ninf ? ninf : a.e - lu + 0.5, round_err(v, w)));
}

RV expm1(const RV& a, std::size_t w) {
    if (!a.ok() || a.e > -2.0) return failed();
    if (a.v.is_zero()) return RV(a.v, a.e == ninf ? ninf : a.e + 1.2);
    const double la = l2(a.v);

    if (la < -1.0) {
        const std::size_t wp  = w + 8;
        const BFC         c   = ctx(wp);
        BF                t   = a.v;
        BF                s   = a.v;
        double            acc = la;

        for (std::uint64_t k = 2; k < 100000; ++k) {
            t = BF::div(BF::mul(t, a.v, c), BF(k), c);
            const double lt = l2(t);
            s = BF::add(s, t, c);
            acc = lsum(acc, lt + std::log2(2.0 * static_cast<double>(k) + 2.0));
            if (t.is_zero() || lt < l2(s) - static_cast<double>(wp) - 4.0) {
                const BF     v   = s.rounded(ctx(w));
                const double sum = lsum(acc, l2(s) + std::log2(static_cast<double>(k) + 1.0)) - static_cast<double>(wp);
                return RV(v, lsum(lt + 1.0, sum, a.e == ninf ? ninf : a.e + 1.2, round_err(v, w)));
            }
        }

        return failed();
    }

    const RV E = exp(a, w + 4);
    if (!E.ok() || !E.v.is_finite()) return E;
    const BF v = BF::sub(E.v, BF::one(), ctx(w));
    return RV(v, lsum(E.e, round_err(v, w)));
}

RV pow(const RV& x, const RV& y, std::size_t w) {
    if (!x.ok() || !y.ok()) return failed();
    if (x.v.is_zero() || x.v.signbit()) return failed();
    const std::size_t extra = static_cast<std::size_t>(std::max(0.0, l2(y.v) + std::log2(std::fabs(l2(x.v)) + 2.0))) + 8;
    const RV          L     = ln(x, w + extra);
    return exp(mul(y, L, w + extra), w);
}

RV recip(const RV& a, std::size_t w) { return div(exact(BF::one()), a, w); }

RV one_minus(const RV& a, std::size_t w) { return sub(exact(BF::one()), a, w); }

RV pi(std::size_t w) {
    const BF v = multiprecision::constants::pi(ctx(w));
    return RV(v, round_err(v, w));
}

RV log_gamma(const RV& a, std::size_t w) {
    if (!a.ok() || a.v.signbit() || a.v.is_zero() || !small_rel(a, -2.0)) return failed();
    const BF v = math::log_gamma(a.v, ctx(w));
    if (!v.is_finite()) return RV(v, ninf);
    const double slope = (a.e == ninf) ? ninf : psi_bound(a);
    return RV(v, lsum(round_err(v, w), slope + a.e + 0.5));
}

RV log_beta(const RV& a, const RV& b, std::size_t w) {
    if (!a.ok() || !b.ok()) return failed();
    if (a.v.signbit() || a.v.is_zero() || b.v.signbit() || b.v.is_zero()) return failed();
    if (!small_rel(a, -2.0) || !small_rel(b, -2.0)) return failed();
    const BF v = math::log_beta(a.v, b.v, ctx(w));
    if (!v.is_finite()) return RV(v, ninf);
    double err = round_err(v, w);

    if (a.e != ninf || b.e != ninf) {
        const RV     s     = add(a, b, w);
        const double slope = lsum(psi_bound(a), psi_bound(b), psi_bound(s), psi_bound(s));
        err = lsum(err, slope + lsum(a.e, b.e) + 0.5);
    }

    return RV(v, err);
}

RV erfc(const RV& z, std::size_t w) {
    if (!z.ok()) return failed();
    const BF v = math::erfc(z.v, ctx(w));
    if (z.e == ninf) return RV(v, round_err(v, w));
    const double zd  = std::fabs(to_double(z.v));
    const double d   = std::exp2(z.e);
    const double m   = zd > d ? zd - d : 0.0;
    const double lsl = 0.1765 - m * m * 1.4426950408889634;
    return RV(v, lsum(round_err(v, w), lsl + z.e + 0.5));
}

RV cospi(const RV& x, std::size_t w) {
    if (!x.ok()) return failed();
    BF s, c;
    cxsf::sincospi(x.v, w + 4, s, c);
    const BF v = c.rounded(ctx(w));
    return RV(v, lsum(-static_cast<double>(w) - 2.0, round_err(v, w), x.e == ninf ? ninf : x.e + 1.66));
}

RV through(const RV& a, const BF& value, double l2_slope, std::size_t w) {
    if (!a.ok()) return failed();
    return RV(value, lsum(round_err(value, w), a.e == ninf ? ninf : l2_slope + a.e + 0.5));
}

bool certain_positive(const RV& a) { return a.ok() && !a.v.signbit() && !a.v.is_zero() && l2(a.v) > a.e + 0.5; }

bool certain_negative(const RV& a) { return a.ok() && a.v.signbit() && !a.v.is_zero() && l2(a.v) > a.e + 0.5; }

bool separated(const RV& a, const RV& b) {
    if (!a.ok() || !b.ok()) return false;
    const BF d = exact_sub(a.v, b.v);
    return !d.is_zero() && l2(d) > lsum(a.e, b.e) + 0.5;
}

BF finish(const RV& r, const BFC& c, bool last, bool& done) {
    if (r.e == pinf) {
        done = false;
        return BF::nan();
    }

    if (r.v.is_nan() || r.v.is_undefined()) {
        done = true;
        return r.v;
    }

    if (!r.v.is_finite()) {
        done = true;
        return r.v;
    }

    BF out;
    done = cxsf::settle(r.v, r.e, c, last, out);
    return out;
}

} // namespace tracked
} // namespace math
} // namespace fizmo
