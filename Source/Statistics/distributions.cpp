#include "fizmo_library.hpp"
#include "internal.hpp"

namespace fizmo {
namespace statistics {
namespace core {
namespace ddetail {

namespace T = ::fizmo::math::tracked;
namespace M = ::fizmo::math;

using T::BF;
using T::BFC;
using T::ninf;
using T::pinf;

RV X(const BF& v) { return T::exact(v); }

BF half() { return BF::one().scaled_pow2(-1); }

BF exact_mul(const BF& a, const BF& b) { return BF::mul(a, b, BFC(BFC::max_prec, math::RoundingMode::nearest_even)); }

BF exact_add(const BF& a, const BF& b) { return T::exact_add(a, b); }

BF exact_sub(const BF& a, const BF& b) { return T::exact_sub(a, b); }

int cmp(const BF& a, const BF& b) {
    const BF::ordering o = BF::compare(a, b);
    if (o == BF::ordering::less) return -1;
    if (o == BF::ordering::greater) return 1;
    if (o == BF::ordering::equal) return 0;
    return 2;
}

int sign_of(const RV& x) {
    if (x.v.is_zero() && x.e == ninf) return 0;
    if (T::certain_positive(x)) return 1;
    if (T::certain_negative(x)) return -1;
    return 2;
}

double dbl(const BF& v) {
    const double d = v.to_double();
    if (std::isinf(d)) return d > 0 ? 1.0e300 : -1.0e300;
    return d;
}

bool is_int(const BF& v) { return v.is_finite() && v.is_integer(); }

BF floor_of(const BF& v) { return M::floor(v); }

BF ceil_of(const BF& v) { return M::ceiling(v); }

RV exp_of(const std::function<RV(std::size_t)>& logf, std::size_t w) {
    const RV     est = logf(64);
    const double mag = est.ok() ? std::max(0.0, T::l2(est.v)) : 0.0;
    const RV     L   = logf(w + 8 + static_cast<std::size_t>(mag));
    if (!L.ok()) return T::failed();
    if (L.v.signbit() && T::l2(L.v) > 46.0) return X(BF::zero());
    return T::exp(L, w + 2);
}

RV sqrt2(std::size_t w) { return T::sqrt(X(BF(2)), w); }

RV phi(const RV& z, bool upper, std::size_t w) {
    const double zd = std::fabs(dbl(z.v));
    const std::size_t wi = w + 8 + static_cast<std::size_t>(std::log2(1.0 + zd * zd));
    const RV u = T::div(upper ? z : T::neg(z), sqrt2(wi), wi);
    return T::scale2(T::erfc(u, wi), -1);
}

RV student(const BF& nu, const RV& t, bool upper, std::size_t w) {
    if (t.v.is_zero() && t.e == ninf) return X(half());
    const int s = sign_of(t);
    if (s == 2) return T::failed();
    const bool        pos = s > 0;
    const std::size_t wi  = w + 8;
    const RV          N   = X(nu);
    const RV          t2  = T::sqr(t, wi);
    const RV          den = T::add(N, t2, wi);
    const BF          h   = half();
    const BF          nu2 = nu.scaled_pow2(-1);
    RV tail, other;

    if (cmp(t2.v, nu) < 0) {
        const RV y = T::div(t2, den, wi);
        tail  = T::scale2(T::beta_i(y, h, nu2, wi, true), -1);
        other = T::scale2(T::add(X(BF::one()), T::beta_i(y, h, nu2, wi, false), wi), -1);
    } else {
        const RV x = T::div(N, den, wi);
        tail  = T::scale2(T::beta_i(x, nu2, h, wi, false), -1);
        other = T::one_minus(tail, wi);
    }

    return (pos == upper) ? tail : other;
}

BF nct_cdf_bf(const BF& t, const BF& nu, const BF& delta, std::size_t w) {
    const bool   neg   = t.signbit() && !t.is_zero();
    const BF     tt    = t.abs();
    const BF     del   = neg ? -delta : delta;
    const double dd    = dbl(del);
    const double lam_d = dd * dd;
    const double xd    = tt.is_zero() ? 0.0 : dbl(tt) * dbl(tt) / (dbl(tt) * dbl(tt) + dbl(nu));
    const double loss  = (xd > 0.0 && xd < 1.0) ? (lam_d * 0.5 + 10.0 * std::sqrt(lam_d) + 10.0) * std::log2(1.0 / xd) : 0.0;
    const std::size_t wp = w + 64 + static_cast<std::size_t>(std::min(loss, 8192.0));
    const BFC c = T::ctx(wp);
    const BF  base = BF::div(M::erfc(BF::div(del, M::sqrt(BF(2), c), c), c), BF(2), c);

    if (tt.is_zero()) return neg ? BF::sub(BF::one(), base, c) : base;

    const BF t2     = BF::mul(tt, tt, c);
    const BF x      = BF::div(t2, BF::add(t2, nu, c), c);
    const BF lambda = BF::mul(del, del, c);
    BF       p      = BF::mul(half(), M::exp(BF::mul(lambda, -half(), c), c), c);
    BF       q      = BF::mul(BF::mul(M::sqrt(BF::div(BF(2), multiprecision::constants::pi(c), c), c), p, c), del, c);
    BF       s      = BF::sub(half(), p, c);
    BF       a      = half();
    const BF b      = nu.scaled_pow2(-1);
    const BF rxb    = M::pow(BF::sub(BF::one(), x, c), b, c);
    const BF albeta = BF::sub(BF::add(BF::mul(half(), M::ln(multiprecision::constants::pi(c), c), c), M::log_gamma(b, c), c),
                              M::log_gamma(BF::add(half(), b, c), c), c);
    BF       xodd   = M::regularized_beta(x, a, b, c);
    BF       godd   = BF::mul(BF(2), BF::mul(rxb, M::exp(BF::sub(BF::mul(a, M::ln(x, c), c), albeta, c), c), c), c);
    BF       xeven  = BF::sub(BF::one(), rxb, c);
    BF       geven  = BF::mul(BF::mul(b, x, c), rxb, c);
    BF       tnc    = BF::add(BF::mul(p, xodd, c), BF::mul(q, xeven, c), c);
    const double target = -static_cast<double>(w) - 16.0;

    for (std::uint64_t en = 1; en < 2000000; ++en) {
        a     = BF::add(a, BF::one(), c);
        xodd  = BF::sub(xodd, godd, c);
        xeven = BF::sub(xeven, geven, c);
        godd  = BF::div(BF::mul(godd, BF::mul(x, BF::sub(BF::add(a, b, c), BF::one(), c), c), c), a, c);
        geven = BF::div(BF::mul(geven, BF::mul(x, BF::sub(BF::add(a, b, c), half(), c), c), c), BF::add(a, half(), c), c);
        p     = BF::div(BF::mul(p, lambda, c), BF(2 * en), c);
        q     = BF::div(BF::mul(q, lambda, c), BF(2 * en + 1), c);
        s     = BF::sub(s, p, c);
        tnc   = BF::add(tnc, BF::add(BF::mul(p, xodd, c), BF::mul(q, xeven, c), c), c);
        const BF     errbd = BF::mul(BF(2), BF::mul(s, BF::sub(xodd, godd, c), c), c).abs();
        const double le    = T::l2(errbd);
        if (static_cast<double>(en) > lam_d * 0.5 && (errbd.is_zero() || le < target + std::min(0.0, T::l2(tnc)))) break;
    }

    tnc = BF::add(tnc, base, c);
    if (neg) tnc = BF::sub(BF::one(), tnc, c);
    return tnc;
}

RV nct(const BF& nu, const BF& delta, const RV& t, bool upper, std::size_t w) {
    const BF v = nct_cdf_bf(t.v, nu, delta, w + 8);
    const BF r = upper ? BF::sub(BF::one(), v, T::ctx(w + 8)) : v;
    return RV(r.rounded(T::ctx(w)), T::l2(r) - static_cast<double>(w) + 1.0);
}

RV bound(bool value_one) { return X(value_one ? BF::one() : BF::zero()); }

RV continuous_cdf(const Dist& d, const RV& x, bool upper, std::size_t w) {
    const std::size_t wi = w + 8;

    switch (d.kind) {
        case Kind::normal: {
            const RV z = T::div(T::sub(x, X(d.a), wi), X(d.b), wi);
            return phi(z, upper, w);
        }

        case Kind::student_t:
            return student(d.a, x, upper, w);

        case Kind::chi_squared: {
            const int s = sign_of(x);
            if (s == 2) return T::failed();
            if (s <= 0) return bound(upper);
            return T::gamma_pq(d.a.scaled_pow2(-1), T::scale2(x, -1), w, upper);
        }

        case Kind::fisher_f: {
            const int s = sign_of(x);
            if (s == 2) return T::failed();
            if (s <= 0) return bound(upper);
            const RV d1x = T::mul(X(d.a), x, wi);
            const RV y   = T::div(d1x, T::add(d1x, X(d.b), wi), wi);
            return T::beta_i(y, d.a.scaled_pow2(-1), d.b.scaled_pow2(-1), w, upper);
        }

        case Kind::exponential: {
            const int s = sign_of(x);
            if (s == 2) return T::failed();
            if (s <= 0) return bound(upper);
            const RV arg = T::neg(T::mul(X(d.a), x, wi));
            return upper ? T::exp(arg, w) : T::neg(T::expm1(arg, w));
        }

        case Kind::uniform: {
            const int lo = sign_of(T::sub(x, X(d.a), wi));
            const int hi = sign_of(T::sub(x, X(d.b), wi));
            if (lo == 2 || hi == 2) return T::failed();
            if (lo <= 0) return bound(upper);
            if (hi >= 0) return bound(!upper);
            const RV width = X(exact_sub(d.b, d.a));
            return upper ? T::div(T::sub(X(d.b), x, wi), width, w) : T::div(T::sub(x, X(d.a), wi), width, w);
        }

        case Kind::gamma: {
            const int s = sign_of(x);
            if (s == 2) return T::failed();
            if (s <= 0) return bound(upper);
            return T::gamma_pq(d.a, T::div(x, X(d.b), wi), w, upper);
        }

        case Kind::beta: {
            const int lo = sign_of(x);
            const int hi = sign_of(T::sub(x, X(BF::one()), wi));
            if (lo == 2 || hi == 2) return T::failed();
            if (lo <= 0) return bound(upper);
            if (hi >= 0) return bound(!upper);
            return T::beta_i(x, d.a, d.b, w, upper);
        }

        case Kind::noncentral_t:
            return nct(d.a, d.b, x, upper, w);

        default:
            return RV(BF::nan(), pinf);
    }
}

BF lgamma_bf(const BF& v, std::size_t w) { return M::log_gamma(v, T::ctx(w)); }

RV lg(const BF& v, std::size_t w) { return T::log_gamma(X(v), w); }

RV ln_x(const BF& v, std::size_t w) { return T::ln(X(v), w); }

struct Support {
    BF   lo;
    BF   hi;
    bool bounded_hi;
};

Support support(const Dist& d) {
    switch (d.kind) {
        case Kind::binomial:          return {BF::zero(), d.a, true};
        case Kind::geometric:         return {BF::one(), BF::infinity(), false};
        case Kind::poisson:           return {BF::zero(), BF::infinity(), false};
        case Kind::negative_binomial: return {BF::zero(), BF::infinity(), false};
        case Kind::hypergeometric: {
            const BF lo = exact_sub(exact_add(d.c, d.b), d.a);
            const BF hi = cmp(d.c, d.b) < 0 ? d.c : d.b;
            return {lo.signbit() ? BF::zero() : lo, hi, true};
        }
        default: return {BF::zero(), BF::zero(), false};
    }
}

std::uint64_t u64(const BF& v) { return static_cast<std::uint64_t>(dbl(v)); }

BF hyper_sum(const Dist& d, std::uint64_t from, std::uint64_t to, const BFC& c) {
    const std::uint64_t N = u64(d.a), K = u64(d.b), n = u64(d.c);
    if (from > to) return BF::zero();
    BigInt ck  = M::combination(BigInt(K), static_cast<std::int64_t>(from));
    BigInt cnk = M::combination(BigInt(N - K), static_cast<std::int64_t>(n - from));
    BigInt acc = ck * cnk;

    for (std::uint64_t j = from; j < to; ++j) {
        ck  = ck * BigInt(K - j) / BigInt(j + 1);
        cnk = cnk * BigInt(n - j) / BigInt(N - K - n + j + 1);
        acc = acc + ck * cnk;
    }

    const BigInt total = M::combination(BigInt(N), static_cast<std::int64_t>(n));
    return BF::div(BF(acc), BF(total), c);
}

RV discrete_cdf(const Dist& d, const BF& kin, bool upper, std::size_t w) {
    const BF      k  = floor_of(kin);
    const Support sp = support(d);
    if (cmp(k, sp.lo) < 0) return bound(upper);
    if (sp.bounded_hi && cmp(k, sp.hi) >= 0) return bound(!upper);
    const BF k1 = exact_add(k, BF::one());

    switch (d.kind) {
        case Kind::binomial: {
            if (d.b.is_zero()) return bound(!upper);
            if (cmp(d.b, BF::one()) == 0) return bound(upper);
            const BF q = exact_sub(BF::one(), d.b);
            return T::beta_i_exact(q, exact_sub(d.a, k), k1, w, upper);
        }

        case Kind::geometric: {
            if (cmp(d.a, BF::one()) == 0) return bound(!upper);
            const std::size_t wi = w + 8 + static_cast<std::size_t>(std::max(0.0, T::l2(k)));
            const RV          L  = T::mul(X(k), T::ln1p(X(-d.a), wi), wi);
            return upper ? T::exp(L, w) : T::neg(T::expm1(L, w));
        }

        case Kind::poisson:
            if (d.a.is_zero()) return bound(!upper);
            return T::gamma_pq_exact(k1, d.a, w, !upper);

        case Kind::negative_binomial:
            if (cmp(d.b, BF::one()) == 0) return bound(!upper);
            return T::beta_i_exact(d.b, d.a, k1, w, upper);

        case Kind::hypergeometric: {
            const BFC           c  = T::ctx(w + 4);
            const std::uint64_t lo = u64(sp.lo), hi = u64(sp.hi), kk = u64(k);
            const bool          low_side = (kk - lo) <= (hi - kk);
            BF v;
            if (low_side) {
                v = hyper_sum(d, lo, kk, c);
                if (upper) v = BF::sub(BF::one(), v, T::ctx(w + 4));
            } else {
                v = hyper_sum(d, kk + 1, hi, c);
                if (!upper) v = BF::sub(BF::one(), v, T::ctx(w + 4));
            }
            return RV(v, T::l2(v) - static_cast<double>(w) + 1.0);
        }

        default:
            return RV(BF::nan(), pinf);
    }
}

RV cdf_rv(const Dist& d, const RV& x, bool upper, std::size_t w) {
    if (discrete(d)) return discrete_cdf(d, x.v, upper, w);
    return continuous_cdf(d, x, upper, w);
}

RV log_pmf(const Dist& d, const BF& k, std::size_t w) {
    const std::size_t wi = w;

    switch (d.kind) {
        case Kind::binomial: {
            const BF n = d.a;
            const BF p = d.b;
            const BF nk = exact_sub(n, k);
            const RV c = T::sub(T::sub(lg(exact_add(n, BF::one()), wi), lg(exact_add(k, BF::one()), wi), wi), lg(exact_add(nk, BF::one()), wi), wi);
            const RV a = k.is_zero() ? X(BF::zero()) : T::mul(X(k), ln_x(p, wi), wi);
            const RV b = nk.is_zero() ? X(BF::zero()) : T::mul(X(nk), T::ln1p(X(-p), wi), wi);
            return T::add(T::add(c, a, wi), b, wi);
        }

        case Kind::geometric: {
            const BF km1 = exact_sub(k, BF::one());
            const RV b   = km1.is_zero() ? X(BF::zero()) : T::mul(X(km1), T::ln1p(X(-d.a), wi), wi);
            return T::add(ln_x(d.a, wi), b, wi);
        }

        case Kind::poisson: {
            const RV a = k.is_zero() ? X(BF::zero()) : T::mul(X(k), ln_x(d.a, wi), wi);
            return T::sub(T::sub(a, X(d.a), wi), lg(exact_add(k, BF::one()), wi), wi);
        }

        case Kind::negative_binomial: {
            const RV c = T::sub(T::sub(lg(exact_add(k, d.a), wi), lg(exact_add(k, BF::one()), wi), wi), lg(d.a, wi), wi);
            const RV a = T::mul(X(d.a), ln_x(d.b, wi), wi);
            const RV b = k.is_zero() ? X(BF::zero()) : T::mul(X(k), T::ln1p(X(-d.b), wi), wi);
            return T::add(T::add(c, a, wi), b, wi);
        }

        default:
            return RV(BF::nan(), pinf);
    }
}

RV discrete_pmf(const Dist& d, const BF& k, std::size_t w) {
    if (!is_int(k)) return X(BF::zero());
    const Support sp = support(d);
    if (cmp(k, sp.lo) < 0 || (sp.bounded_hi && cmp(k, sp.hi) > 0)) return X(BF::zero());

    switch (d.kind) {
        case Kind::binomial:
            if (d.b.is_zero()) return X(k.is_zero() ? BF::one() : BF::zero());
            if (cmp(d.b, BF::one()) == 0) return X(cmp(k, d.a) == 0 ? BF::one() : BF::zero());
            break;
        case Kind::geometric:
            if (cmp(d.a, BF::one()) == 0) return X(cmp(k, BF::one()) == 0 ? BF::one() : BF::zero());
            break;
        case Kind::poisson:
            if (d.a.is_zero()) return X(k.is_zero() ? BF::one() : BF::zero());
            break;
        case Kind::negative_binomial:
            if (cmp(d.b, BF::one()) == 0) return X(k.is_zero() ? BF::one() : BF::zero());
            break;
        case Kind::hypergeometric: {
            const std::uint64_t kk = u64(k);
            const BF v = hyper_sum(d, kk, kk, T::ctx(w + 4));
            return RV(v, T::l2(v) - static_cast<double>(w) + 1.0);
        }
        default:
            break;
    }

    return exp_of([&](std::size_t wl) { return log_pmf(d, k, wl); }, w);
}

RV log_density(const Dist& d, const RV& x, std::size_t w) {
    const std::size_t wi = w;

    switch (d.kind) {
        case Kind::normal: {
            const RV z = T::div(T::sub(x, X(d.a), wi), X(d.b), wi);
            const RV l = T::add(ln_x(d.b, wi), T::scale2(T::ln(T::scale2(T::pi(wi), 1), wi), -1), wi);
            return T::neg(T::add(T::scale2(T::sqr(z, wi), -1), l, wi));
        }

        case Kind::student_t: {
            const BF nu  = d.a;
            const RV top = T::sub(lg(exact_add(nu, BF::one()).scaled_pow2(-1), wi), lg(nu.scaled_pow2(-1), wi), wi);
            const RV nrm = T::scale2(T::add(ln_x(nu, wi), T::ln(T::pi(wi), wi), wi), -1);
            const RV tail = T::mul(X(exact_add(nu, BF::one()).scaled_pow2(-1)), T::ln1p(T::div(T::sqr(x, wi), X(nu), wi), wi), wi);
            return T::sub(T::sub(top, nrm, wi), tail, wi);
        }

        case Kind::chi_squared: {
            const BF k2 = d.a.scaled_pow2(-1);
            const RV a  = T::mul(X(exact_sub(k2, BF::one())), T::ln(x, wi), wi);
            const RV b  = T::add(T::scale2(x, -1), T::add(T::mul(X(k2), ln_x(BF(2), wi), wi), lg(k2, wi), wi), wi);
            return T::sub(a, b, wi);
        }

        case Kind::fisher_f: {
            const BF d1 = d.a, d2 = d.b;
            const RV d1x = T::mul(X(d1), x, wi);
            const RV a   = T::mul(X(d1.scaled_pow2(-1)), T::ln(d1x, wi), wi);
            const RV b   = T::mul(X(d2.scaled_pow2(-1)), ln_x(d2, wi), wi);
            const RV c   = T::mul(X(exact_add(d1, d2).scaled_pow2(-1)), T::ln(T::add(d1x, X(d2), wi), wi), wi);
            const RV e   = T::add(T::ln(x, wi), T::log_beta(X(d1.scaled_pow2(-1)), X(d2.scaled_pow2(-1)), wi), wi);
            return T::sub(T::sub(T::add(a, b, wi), c, wi), e, wi);
        }

        case Kind::exponential:
            return T::sub(ln_x(d.a, wi), T::mul(X(d.a), x, wi), wi);

        case Kind::gamma: {
            const RV a = T::mul(X(exact_sub(d.a, BF::one())), T::ln(x, wi), wi);
            const RV b = T::add(T::div(x, X(d.b), wi), T::add(lg(d.a, wi), T::mul(X(d.a), ln_x(d.b, wi), wi), wi), wi);
            return T::sub(a, b, wi);
        }

        case Kind::beta: {
            const RV a = T::mul(X(exact_sub(d.a, BF::one())), T::ln(x, wi), wi);
            const RV b = T::mul(X(exact_sub(d.b, BF::one())), T::ln1p(T::neg(x), wi), wi);
            return T::sub(T::add(a, b, wi), T::log_beta(X(d.a), X(d.b), wi), wi);
        }

        default:
            return RV(BF::nan(), pinf);
    }
}

RV edge_density(const BF& shape, const BF& at_one) {
    const int c = cmp(shape, BF::one());
    if (c < 0) return X(BF::infinity());
    if (c == 0) return X(at_one);
    return X(BF::zero());
}

RV continuous_pdf(const Dist& d, const BF& x, std::size_t w) {
    switch (d.kind) {
        case Kind::chi_squared:
            if (x.signbit() && !x.is_zero()) return X(BF::zero());
            if (x.is_zero()) return edge_density(d.a.scaled_pow2(-1), half());
            break;
        case Kind::fisher_f:
            if (x.signbit() && !x.is_zero()) return X(BF::zero());
            if (x.is_zero()) return edge_density(d.a.scaled_pow2(-1), BF::one());
            break;
        case Kind::exponential:
            if (x.signbit() && !x.is_zero()) return X(BF::zero());
            break;
        case Kind::uniform:
            if (cmp(x, d.a) < 0 || cmp(x, d.b) > 0) return X(BF::zero());
            return T::div(X(BF::one()), X(exact_sub(d.b, d.a)), w);
        case Kind::gamma:
            if (x.signbit() && !x.is_zero()) return X(BF::zero());
            if (x.is_zero()) {
                if (cmp(d.a, BF::one()) == 0) return T::div(X(BF::one()), X(d.b), w);
                return edge_density(d.a, BF::one());
            }
            break;
        case Kind::beta:
            if ((x.signbit() && !x.is_zero()) || cmp(x, BF::one()) > 0) return X(BF::zero());
            if (x.is_zero()) {
                if (cmp(d.a, BF::one()) == 0) return X(d.b);
                return edge_density(d.a, BF::one());
            }
            if (cmp(x, BF::one()) == 0) {
                if (cmp(d.b, BF::one()) == 0) return X(d.a);
                return edge_density(d.b, BF::one());
            }
            break;
        case Kind::noncentral_t: {
            const BFC c = T::ctx(w + 32);
            if (x.is_zero()) {
                const std::size_t wd = 3 * w + 64;
                const BF dx = BF::one().scaled_pow2(-static_cast<std::int64_t>(w + 16));
                const BF lo = nct_cdf_bf(-dx, d.a, d.b, wd);
                const BF hi = nct_cdf_bf(dx, d.a, d.b, wd);
                const BF v  = BF::div(BF::sub(hi, lo, T::ctx(wd)), dx.scaled_pow2(1), c);
                return RV(v.rounded(T::ctx(w)), T::l2(v) - static_cast<double>(w) + 1.0);
            }
            const BF nu2 = exact_add(d.a, BF(2));
            const BF sc  = M::sqrt(BF::add(BF::one(), BF::div(BF(2), d.a, c), c), c);
            const BF f2  = nct_cdf_bf(BF::mul(x, sc, c), nu2, d.b, w + 32);
            const BF f1  = nct_cdf_bf(x, d.a, d.b, w + 32);
            const BF v   = BF::mul(BF::div(d.a, x, c), BF::sub(f2, f1, c), c);
            return RV(v.rounded(T::ctx(w)), T::l2(v) - static_cast<double>(w) + 1.0);
        }
        default:
            break;
    }

    return exp_of([&](std::size_t wl) { return log_density(d, X(x), wl); }, w);
}

RV pdf_rv(const Dist& d, const BF& x, std::size_t w) {
    if (discrete(d)) return discrete_pmf(d, x, w);
    return continuous_pdf(d, x, w);
}

RV rounded_of(const BF& v, std::size_t w) { return T::rounded(v, w); }

RV student_upper_quantile(const BF& nu, const BF& q, std::size_t w) {
    const int c = cmp(q, half());
    if (c == 0) return X(BF::zero());
    if (c > 0) return T::neg(student_upper_quantile(nu, exact_sub(BF::one(), q), w));
    const std::size_t wi  = w + 8;
    const BFC         ci  = T::ctx(wi);
    const BF          h   = half();
    const BF          q2  = q.scaled_pow2(1);
    const RV          x   = rounded_of(M::inverse_regularized_beta(q2, nu.scaled_pow2(-1), h, ci), wi);
    const RV          y   = rounded_of(M::inverse_regularized_beta(exact_sub(BF::one(), q2), h, nu.scaled_pow2(-1), ci), wi);
    return T::sqrt(T::div(T::mul(X(nu), y, wi), x, wi), w);
}

RV continuous_quantile(const Dist& d, const BF& p, bool upper, std::size_t w) {
    const std::size_t wi = w + 8;
    const BFC         ci = T::ctx(wi);
    const BF          q1 = exact_sub(BF::one(), p);

    switch (d.kind) {
        case Kind::normal: {
            const RV r = rounded_of(M::inv_erfc(p.scaled_pow2(1), ci), wi);
            const RV z = T::mul(upper ? r : T::neg(r), sqrt2(wi), wi);
            return T::add(X(d.a), T::mul(X(d.b), z, wi), w);
        }

        case Kind::student_t:
            return student_upper_quantile(d.a, upper ? p : q1, w);

        case Kind::chi_squared: {
            const BF k2 = d.a.scaled_pow2(-1);
            const BF v  = upper ? M::inverse_regularized_gamma_q(k2, p, T::ctx(w)) : M::inverse_regularized_gamma_p(k2, p, T::ctx(w));
            return RV(v.scaled_pow2(1), v.is_finite() ? T::l2(v) + 1.0 - static_cast<double>(w) : ninf);
        }

        case Kind::fisher_f: {
            const BF a = d.a.scaled_pow2(-1), b = d.b.scaled_pow2(-1);
            const BF lo = upper ? q1 : p;
            const RV x  = rounded_of(M::inverse_regularized_beta(lo, a, b, ci), wi);
            const RV y  = rounded_of(M::inverse_regularized_beta(exact_sub(BF::one(), lo), b, a, ci), wi);
            const RV r  = T::div(X(d.b), X(d.a), wi);
            if (y.v.is_zero()) return X(BF::infinity());
            return T::mul(r, T::div(x, y, wi), w);
        }

        case Kind::exponential: {
            const RV L = upper ? T::ln(X(p), wi) : T::ln1p(X(-p), wi);
            return T::neg(T::div(L, X(d.a), w));
        }

        case Kind::uniform: {
            const RV width = X(exact_sub(d.b, d.a));
            if (upper) return T::sub(X(d.b), T::mul(X(p), width, wi), w);
            return T::add(X(d.a), T::mul(X(p), width, wi), w);
        }

        case Kind::gamma: {
            const BF v = upper ? M::inverse_regularized_gamma_q(d.a, p, ci) : M::inverse_regularized_gamma_p(d.a, p, ci);
            return T::mul(rounded_of(v, wi), X(d.b), w);
        }

        case Kind::beta: {
            const BF v = upper ? M::inverse_regularized_beta_complement(p, d.a, d.b, T::ctx(w)) : M::inverse_regularized_beta(p, d.a, d.b, T::ctx(w));
            return RV(v, T::l2(v) - static_cast<double>(w));
        }

        case Kind::noncentral_t: {
            const BFC c = T::ctx(w + 16);
            BF        lo = BF(-1), hi = BF(1);
            auto      f  = [&](const BF& t) { const BF v = nct_cdf_bf(t, d.a, d.b, w + 16); return upper ? BF::sub(BF::one(), v, c) : v; };
            const int dir = upper ? -1 : 1;
            for (int i = 0; i < 2000 && cmp(f(lo), p) * dir > 0; ++i) lo = BF::mul(lo, BF(2), c);
            for (int i = 0; i < 2000 && cmp(f(hi), p) * dir < 0; ++i) hi = BF::mul(hi, BF(2), c);
            for (std::size_t i = 0; i < w + 64; ++i) {
                const BF mid = BF::div(BF::add(lo, hi, c), BF(2), c);
                if (cmp(f(mid), p) * dir < 0) lo = mid; else hi = mid;
            }
            const BF v = BF::div(BF::add(lo, hi, c), BF(2), c);
            return RV(v.rounded(T::ctx(w)), T::l2(v) - static_cast<double>(w) + 1.0);
        }

        default:
            return RV(BF::nan(), pinf);
    }
}

int compare_cdf(const Dist& d, const BF& k, const BF& p, bool upper) {
    for (std::size_t w = 64;; w *= 2) {
        const RV F = discrete_cdf(d, k, upper, w);
        if (!F.ok()) {
            if (w > (1u << 16)) return 0;
            continue;
        }
        const BF diff = exact_sub(F.v, p);
        if (F.e == ninf) return cmp(diff, BF::zero());
        if (!diff.is_zero() && T::l2(diff) > F.e + 0.5) return diff.signbit() ? -1 : 1;
        if (w > (1u << 16)) return 0;
    }
}

bool satisfied(const Dist& d, const BF& k, const BF& p, bool upper) {
    const int c = compare_cdf(d, k, p, upper);
    return upper ? c <= 0 : c >= 0;
}

BF guess_k(const Dist& d, const BF& p, bool upper) {
    const double mu = dbl(mean(d, T::ctx(64)));
    const double sd = std::sqrt(std::max(0.0, dbl(variance(d, T::ctx(64)))));
    const double pd = std::min(std::max(dbl(p), 1.0e-300), 1.0 - 1.0e-16);
    const double z  = dbl(M::inv_erfc(BF(2.0 * (upper ? pd : 1.0 - pd)), T::ctx(64))) * 1.4142135623730951;
    const double g  = std::floor(mu + sd * z);
    return std::isfinite(g) ? BF(g) : BF::zero();
}

RV discrete_quantile(const Dist& d, const BF& p, bool upper) {
    const Support sp = support(d);
    const BF      one = BF::one();
    if (upper ? cmp(p, one) >= 0 : p.is_zero()) return X(sp.lo);
    if (upper ? p.is_zero() : cmp(p, one) >= 0) return X(sp.bounded_hi ? sp.hi : BF::infinity());
    BF k = guess_k(d, p, upper);
    if (cmp(k, sp.lo) < 0) k = sp.lo;
    if (sp.bounded_hi && cmp(k, sp.hi) > 0) k = sp.hi;
    BF lo, hi;

    if (satisfied(d, k, p, upper)) {
        hi = k;
        BF step = BF::one();
        for (;;) {
            BF cand = exact_sub(hi, step);
            if (cmp(cand, sp.lo) < 0) cand = exact_sub(sp.lo, BF::one());
            if (cmp(cand, sp.lo) < 0 || !satisfied(d, cand, p, upper)) {
                lo = cand;
                break;
            }
            hi   = cand;
            step = exact_add(step, step);
        }
    } else {
        lo = k;
        BF step = BF::one();
        for (int i = 0; i < 4000; ++i) {
            BF cand = exact_add(lo, step);
            if (sp.bounded_hi && cmp(cand, sp.hi) > 0) cand = sp.hi;
            if (satisfied(d, cand, p, upper)) {
                hi = cand;
                break;
            }
            lo   = cand;
            step = exact_add(step, step);
            if (i == 3999) return X(BF::infinity());
        }
    }

    while (cmp(exact_sub(hi, lo), BF::one()) > 0) {
        const BF mid = floor_of(exact_add(lo, hi).scaled_pow2(-1));
        if (cmp(mid, sp.lo) >= 0 && satisfied(d, mid, p, upper)) hi = mid; else lo = mid;
    }

    return X(hi);
}

BF ratio(const BF& num, const BF& den, const BFC& c) { return BF::div(num, den, c); }

} // namespace ddetail

using BF  = BigFloat;
using BFC = BigFloatContext;

bool discrete(const Dist& d) {
    switch (d.kind) {
        case Kind::binomial:
        case Kind::geometric:
        case Kind::poisson:
        case Kind::hypergeometric:
        case Kind::negative_binomial:
            return true;
        default:
            return false;
    }
}

bool valid(const Dist& d) {
    using namespace ddetail;
    auto pos  = [](const BF& v) { return v.is_finite() && !v.signbit() && !v.is_zero(); };
    auto prob = [](const BF& v) { return v.is_finite() && !v.signbit() && cmp(v, BF::one()) <= 0; };
    auto nn   = [](const BF& v) { return is_int(v) && !v.signbit(); };

    switch (d.kind) {
        case Kind::normal:            return d.a.is_finite() && pos(d.b);
        case Kind::student_t:         return pos(d.a);
        case Kind::chi_squared:       return pos(d.a);
        case Kind::fisher_f:          return pos(d.a) && pos(d.b);
        case Kind::exponential:       return pos(d.a);
        case Kind::uniform:           return d.a.is_finite() && d.b.is_finite() && cmp(d.a, d.b) < 0;
        case Kind::gamma:             return pos(d.a) && pos(d.b);
        case Kind::beta:              return pos(d.a) && pos(d.b);
        case Kind::noncentral_t:      return pos(d.a) && d.b.is_finite();
        case Kind::binomial:          return nn(d.a) && prob(d.b);
        case Kind::geometric:         return prob(d.a) && !d.a.is_zero();
        case Kind::poisson:           return d.a.is_finite() && !d.a.signbit();
        case Kind::hypergeometric:    return nn(d.a) && nn(d.b) && nn(d.c) && cmp(d.b, d.a) <= 0 && cmp(d.c, d.a) <= 0;
        case Kind::negative_binomial: return pos(d.a) && prob(d.b) && !d.b.is_zero();
    }

    return false;
}

BigFloat pdf(const Dist& d, const BigFloat& x, const BigFloatContext& ctx) {
    if (!valid(d) || x.is_nan() || x.is_undefined()) return BF::nan();
    if (x.is_infinite()) return BF::zero();
    return math::tracked::drive(ctx, [&](std::size_t w) { return ddetail::pdf_rv(d, x, w); });
}

BigFloat log_pdf(const Dist& d, const BigFloat& x, const BigFloatContext& ctx) {
    const BigFloat v = pdf(d, x, BFC(ctx.precision + 64, ctx.rounding_mode));
    if (!v.is_finite() || v.is_zero()) return v.is_zero() ? BF::infinity(true) : v;
    return math::ln(v, ctx);
}

BigFloat cdf(const Dist& d, const BigFloat& x, bool upper, const BigFloatContext& ctx) {
    if (!valid(d) || x.is_nan() || x.is_undefined()) return BF::nan();
    if (x.is_infinite()) return (x.signbit() != upper) ? BF::zero() : BF::one();
    return math::tracked::drive(ctx, [&](std::size_t w) { return ddetail::cdf_rv(d, ddetail::X(x), upper, w); });
}

BigFloat between(const Dist& d, const BigFloat& lo_in, const BigFloat& hi_in, const BigFloatContext& ctx) {
    using namespace ddetail;
    if (!valid(d) || lo_in.is_nan() || hi_in.is_nan() || lo_in.is_undefined() || hi_in.is_undefined()) return BF::nan();
    BF lo = lo_in, hi = hi_in;

    if (discrete(d)) {
        lo = lo.is_infinite() ? lo : exact_sub(ceil_of(lo), BF::one());
        hi = hi.is_infinite() ? hi : floor_of(hi);
    }

    if (cmp(lo, hi) >= 0) return BF::zero();
    if (lo.is_infinite() && lo.signbit()) return cdf(d, hi, false, ctx);
    if (hi.is_infinite() && !hi.signbit()) return cdf(d, lo, true, ctx);
    const double fh = dbl(cdf_rv(d, X(hi), false, 64).v);
    const double sl = dbl(cdf_rv(d, X(lo), true, 64).v);
    const int    mode = (fh <= 0.5) ? 0 : ((sl <= 0.5) ? 1 : 2);

    return math::tracked::drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 4;
        if (mode == 0) return T::sub(cdf_rv(d, X(hi), false, wi), cdf_rv(d, X(lo), false, wi), w);
        if (mode == 1) return T::sub(cdf_rv(d, X(lo), true, wi), cdf_rv(d, X(hi), true, wi), w);
        return T::sub(T::one_minus(cdf_rv(d, X(lo), false, wi), wi), cdf_rv(d, X(hi), true, wi), w);
    });
}

BigFloat outside(const Dist& d, const BigFloat& lo_in, const BigFloat& hi_in, const BigFloatContext& ctx) {
    using namespace ddetail;
    if (!valid(d) || lo_in.is_nan() || hi_in.is_nan() || lo_in.is_undefined() || hi_in.is_undefined()) return BF::nan();
    BF lo = lo_in, hi = hi_in;

    if (discrete(d)) {
        lo = lo.is_infinite() ? lo : exact_sub(ceil_of(lo), BF::one());
        hi = hi.is_infinite() ? hi : floor_of(hi);
    }

    if (cmp(lo, hi) >= 0) return BF::one();
    if (lo.is_infinite() && lo.signbit()) return cdf(d, hi, true, ctx);
    if (hi.is_infinite() && !hi.signbit()) return cdf(d, lo, false, ctx);

    return math::tracked::drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 4;
        return T::add(cdf_rv(d, X(lo), false, wi), cdf_rv(d, X(hi), true, wi), w);
    });
}

BigFloat quantile(const Dist& d, const BigFloat& p, bool upper, const BigFloatContext& ctx) {
    using namespace ddetail;
    if (!valid(d) || p.is_nan() || p.is_undefined()) return BF::nan();
    if (p.signbit() && !p.is_zero()) return BF::nan();
    if (cmp(p, BF::one()) > 0) return BF::nan();
    if (discrete(d)) return discrete_quantile(d, p, upper).v;
    const bool at_lo = upper ? cmp(p, BF::one()) == 0 : p.is_zero();
    const bool at_hi = upper ? p.is_zero() : cmp(p, BF::one()) == 0;

    if (at_lo || at_hi) {
        switch (d.kind) {
            case Kind::uniform: return at_lo ? d.a : d.b;
            case Kind::beta:    return at_lo ? BF::zero() : BF::one();
            case Kind::chi_squared:
            case Kind::fisher_f:
            case Kind::exponential:
            case Kind::gamma:   return at_lo ? BF::zero() : BF::infinity();
            default:            return BF::infinity(at_lo);
        }
    }

    if (d.kind == Kind::chi_squared) {
        const BF k2 = d.a.scaled_pow2(-1);
        return (upper ? math::inverse_regularized_gamma_q(k2, p, ctx) : math::inverse_regularized_gamma_p(k2, p, ctx)).scaled_pow2(1);
    }
    if (d.kind == Kind::beta) return upper ? math::inverse_regularized_beta_complement(p, d.a, d.b, ctx) : math::inverse_regularized_beta(p, d.a, d.b, ctx);
    if (d.kind == Kind::noncentral_t) return continuous_quantile(d, p, upper, ctx.precision + 8).v.rounded(ctx);
    return math::tracked::drive(ctx, [&](std::size_t w) { return continuous_quantile(d, p, upper, w); });
}

BigFloat mean(const Dist& d, const BigFloatContext& ctx) {
    using namespace ddetail;
    if (!valid(d)) return BF::nan();

    switch (d.kind) {
        case Kind::normal:      return d.a.rounded(ctx);
        case Kind::student_t:   return cmp(d.a, BF::one()) > 0 ? BF::zero() : BF::nan();
        case Kind::chi_squared: return d.a.rounded(ctx);
        case Kind::fisher_f:    return cmp(d.b, BF(2)) > 0 ? ratio(d.b, exact_sub(d.b, BF(2)), ctx) : BF::infinity();
        case Kind::exponential: return ratio(BF::one(), d.a, ctx);
        case Kind::uniform:     return exact_add(d.a, d.b).scaled_pow2(-1).rounded(ctx);
        case Kind::gamma:       return BF::mul(d.a, d.b, ctx);
        case Kind::beta:        return ratio(d.a, exact_add(d.a, d.b), ctx);
        case Kind::noncentral_t: {
            if (cmp(d.a, BF::one()) <= 0) return BF::nan();
            return math::tracked::drive(ctx, [&](std::size_t w) {
                const RV g = T::exp(T::sub(lg(exact_sub(d.a, BF::one()).scaled_pow2(-1), w + 8), lg(d.a.scaled_pow2(-1), w + 8), w + 8), w + 4);
                return T::mul(T::mul(X(d.b), T::sqrt(X(d.a.scaled_pow2(-1)), w + 4), w + 4), g, w);
            });
        }
        case Kind::binomial:    return BF::mul(d.a, d.b, ctx);
        case Kind::geometric:   return ratio(BF::one(), d.a, ctx);
        case Kind::poisson:     return d.a.rounded(ctx);
        case Kind::hypergeometric:    return ratio(exact_mul(d.c, d.b), d.a, ctx);
        case Kind::negative_binomial: return ratio(exact_mul(d.a, exact_sub(BF::one(), d.b)), d.b, ctx);
    }

    return BF::nan();
}

BigFloat variance(const Dist& d, const BigFloatContext& ctx) {
    using namespace ddetail;
    if (!valid(d)) return BF::nan();

    switch (d.kind) {
        case Kind::normal:      return exact_mul(d.b, d.b).rounded(ctx);
        case Kind::student_t:
            if (cmp(d.a, BF(2)) > 0) return ratio(d.a, exact_sub(d.a, BF(2)), ctx);
            return cmp(d.a, BF::one()) > 0 ? BF::infinity() : BF::nan();
        case Kind::chi_squared: return d.a.scaled_pow2(1).rounded(ctx);
        case Kind::fisher_f: {
            if (cmp(d.b, BF(4)) <= 0) return cmp(d.b, BF(2)) > 0 ? BF::infinity() : BF::nan();
            const BF d2m2 = exact_sub(d.b, BF(2));
            const BF num  = exact_mul(exact_mul(exact_mul(d.b, d.b), BF(2)), exact_sub(exact_add(d.a, d.b), BF(2)));
            const BF den  = exact_mul(exact_mul(d.a, exact_mul(d2m2, d2m2)), exact_sub(d.b, BF(4)));
            return ratio(num, den, ctx);
        }
        case Kind::exponential: return ratio(BF::one(), exact_mul(d.a, d.a), ctx);
        case Kind::uniform: {
            const BF w = exact_sub(d.b, d.a);
            return ratio(exact_mul(w, w), BF(12), ctx);
        }
        case Kind::gamma:       return exact_mul(d.a, exact_mul(d.b, d.b)).rounded(ctx);
        case Kind::beta: {
            const BF s = exact_add(d.a, d.b);
            return ratio(exact_mul(d.a, d.b), exact_mul(exact_mul(s, s), exact_add(s, BF::one())), ctx);
        }
        case Kind::noncentral_t: {
            if (cmp(d.a, BF(2)) <= 0) return cmp(d.a, BF::one()) > 0 ? BF::infinity() : BF::nan();
            const BigFloatContext hc(ctx.precision + 64, ctx.rounding_mode);
            const BF m  = mean(d, hc);
            const BF t1 = BF::div(BF::mul(d.a, BF::add(BF::one(), BF::mul(d.b, d.b, hc), hc), hc), exact_sub(d.a, BF(2)), hc);
            return BF::sub(t1, BF::mul(m, m, hc), ctx);
        }
        case Kind::binomial:    return exact_mul(exact_mul(d.a, d.b), exact_sub(BF::one(), d.b)).rounded(ctx);
        case Kind::geometric:   return ratio(exact_sub(BF::one(), d.a), exact_mul(d.a, d.a), ctx);
        case Kind::poisson:     return d.a.rounded(ctx);
        case Kind::hypergeometric: {
            const BF N = d.a, K = d.b, n = d.c;
            if (cmp(N, BF::one()) <= 0) return BF::zero();
            const BF num = exact_mul(exact_mul(exact_mul(n, K), exact_sub(N, K)), exact_sub(N, n));
            const BF den = exact_mul(exact_mul(N, N), exact_sub(N, BF::one()));
            return ratio(num, den, ctx);
        }
        case Kind::negative_binomial:
            return ratio(exact_mul(d.a, exact_sub(BF::one(), d.b)), exact_mul(d.b, d.b), ctx);
    }

    return BF::nan();
}

BigFloat standard_deviation(const Dist& d, const BigFloatContext& ctx) {
    if (!valid(d)) return BF::nan();
    if (d.kind == Kind::normal) return d.b.rounded(ctx);

    if (d.kind == Kind::noncentral_t) {
        const BigFloat v = variance(d, BFC(ctx.precision + 32, ctx.rounding_mode));
        return v.is_finite() ? math::sqrt(v, ctx) : v;
    }

    const BigFloat v = variance(d, BFC(64, math::RoundingMode::nearest_even));
    if (!v.is_finite()) return v;
    if (v.is_zero()) return v;

    return math::tracked::drive(ctx, [&](std::size_t w) {
        const BigFloat vv = variance(d, BFC(w + 16, math::RoundingMode::nearest_even));
        return ddetail::T::sqrt(ddetail::T::rounded(vv, w + 16), w);
    });
}

} // namespace core
} // namespace statistics
} // namespace fizmo
