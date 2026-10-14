#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::max(std::min(l, 1000.0), -1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

struct Res {
    CX     v;
    double e  = ninf;
    bool   ok = true;
};

Res bad() {
    Res r;
    r.ok = false;
    return r;
}

CX third(const CX& a, std::size_t w) { return div(a, BF(static_cast<std::int64_t>(3)), w); }

Res rf_core(const CX& x0, const CX& y0, const CX& z0, std::size_t w) {
    const std::size_t wb = w + 16;
    const CX A0 = third(add(add(x0, y0, wb + 8), z0, wb + 8), wb);
    const double lm = std::max({l2(sub(A0, x0, wb)), l2(sub(A0, y0, wb)), l2(sub(A0, z0, wb))});
    CX x = x0, y = y0, z = z0, A = A0;
    std::int64_t n = 0;
    const double target = -(static_cast<double>(wb) + 4.0) / 6.0 - 2.0;
    for (;;) {
        const double lA = l2(A);
        if (lA == ninf) return bad();
        if (lm == ninf || lm - 2.0 * static_cast<double>(n) - lA <= target) break;
        if (n > 4000) return bad();
        const CX sx = sqrt(x, wb), sy = sqrt(y, wb), sz = sqrt(z, wb);
        const CX lam = add(add(mul(sx, sy, wb), mul(sy, sz, wb), wb), mul(sz, sx, wb), wb);
        x = scale2(add(x, lam, wb), -2);
        y = scale2(add(y, lam, wb), -2);
        z = scale2(add(z, lam, wb), -2);
        A = scale2(add(A, lam, wb), -2);
        ++n;
    }
    const CX t = scale2(recip(A, wb), -2 * n);
    const CX X = mul(sub(A0, x0, wb), t, wb);
    const CX Y = mul(sub(A0, y0, wb), t, wb);
    const CX Z = neg(add(X, Y, wb));
    const CX E2 = sub(mul(X, Y, wb), sqr(Z, wb), wb);
    const CX E3 = mul(mul(X, Y, wb), Z, wb);
    CX P(BF(static_cast<std::int64_t>(9240)));
    P = sub(P, mul(E2, BF(static_cast<std::int64_t>(924)), wb), wb);
    P = add(P, mul(sqr(E2, wb), BF(static_cast<std::int64_t>(385)), wb), wb);
    P = add(P, mul(E3, BF(static_cast<std::int64_t>(660)), wb), wb);
    P = sub(P, mul(mul(E2, E3, wb), BF(static_cast<std::int64_t>(630)), wb), wb);
    P = div(P, BF(static_cast<std::int64_t>(9240)), wb);
    Res r;
    r.v = div(P, sqrt(A, wb), wb);
    const double lr = lm == ninf ? ninf : lm - 2.0 * static_cast<double>(n) - l2(A);
    r.e = l2(r.v) + lsum(lr == ninf ? ninf : 6.0 * (lr + 2.0) + 1.0, std::log2(24.0 * static_cast<double>(n) + 64.0) - static_cast<double>(wb));
    return r;
}

Res rc_core(const CX& x, const CX& y, std::size_t w) { return rf_core(x, y, y, w); }

Res rc_one_plus(const CX& e, std::size_t w) {
    const double le = l2(e);
    if (le > -4.0) return rc_core(CX(BF::one()), add_real(e, BF::one(), w + 8), w);
    const std::size_t wb = w + 8;
    Res r;
    if (le == ninf) {
        r.v = CX(BF::one());
        r.e = ninf;
        return r;
    }
    const CX me = neg(e);
    CX   p(BF::one());
    CX   sum = p;
    std::uint64_t k = 1;
    for (;; ++k) {
        p = mul(p, me, wb);
        const CX t = div(p, BF(2 * k + 1), wb);
        sum = add(sum, t, wb);
        if (l2(t) + le < -static_cast<double>(wb) - 2.0) break;
    }
    r.v = sum;
    r.e = lsum(static_cast<double>(k + 1) * le + 1.0, std::log2(4.0 * static_cast<double>(k) + 8.0) - static_cast<double>(wb));
    return r;
}

Res rd_core(const CX& x0, const CX& y0, const CX& z0, std::size_t w) {
    const std::size_t wb = w + 16;
    const CX A0 = div(add(add(x0, y0, wb + 8), mul(z0, BF(static_cast<std::int64_t>(3)), wb + 8), wb + 8), BF(static_cast<std::int64_t>(5)), wb);
    const double lm = std::max({l2(sub(A0, x0, wb)), l2(sub(A0, y0, wb)), l2(sub(A0, z0, wb))});
    CX x = x0, y = y0, z = z0, A = A0;
    CX S;
    Term T;
    std::int64_t n = 0;
    const double target = -(static_cast<double>(wb) + 4.0) / 6.0 - 2.0;
    for (;;) {
        const double lA = l2(A);
        if (lA == ninf) return bad();
        if (lm == ninf || lm - 2.0 * static_cast<double>(n) - lA <= target) break;
        if (n > 4000) return bad();
        const CX sx = sqrt(x, wb), sy = sqrt(y, wb), sz = sqrt(z, wb);
        const CX lam = add(add(mul(sx, sy, wb), mul(sy, sz, wb), wb), mul(sz, sx, wb), wb);
        const CX den = mul(sz, add(z, lam, wb), wb);
        if (is_zero(den)) return bad();
        const CX term = scale2(recip(den, wb), -2 * n);
        S = add(S, term, wb);
        T.add(l2(term));
        x = scale2(add(x, lam, wb), -2);
        y = scale2(add(y, lam, wb), -2);
        z = scale2(add(z, lam, wb), -2);
        A = scale2(add(A, lam, wb), -2);
        ++n;
    }
    const CX t  = scale2(recip(A, wb), -2 * n);
    const CX X  = mul(sub(A0, x0, wb), t, wb);
    const CX Y  = mul(sub(A0, y0, wb), t, wb);
    const CX Z  = third(neg(add(X, Y, wb)), wb);
    const CX XY = mul(X, Y, wb);
    const CX Z2 = sqr(Z, wb);
    const CX E2 = sub(XY, mul(Z2, BF(static_cast<std::int64_t>(6)), wb), wb);
    const CX E3 = mul(sub(mul(XY, BF(static_cast<std::int64_t>(3)), wb), mul(Z2, BF(static_cast<std::int64_t>(8)), wb), wb), Z, wb);
    const CX E4 = mul(mul(sub(XY, Z2, wb), Z2, wb), BF(static_cast<std::int64_t>(3)), wb);
    const CX E5 = mul(mul(XY, Z2, wb), Z, wb);
    CX P(BF(static_cast<std::int64_t>(24024)));
    P = sub(P, mul(E2, BF(static_cast<std::int64_t>(5148)), wb), wb);
    P = add(P, mul(sqr(E2, wb), BF(static_cast<std::int64_t>(2457)), wb), wb);
    P = add(P, mul(E3, BF(static_cast<std::int64_t>(4004)), wb), wb);
    P = sub(P, mul(mul(E2, E3, wb), BF(static_cast<std::int64_t>(4158)), wb), wb);
    P = sub(P, mul(E4, BF(static_cast<std::int64_t>(3276)), wb), wb);
    P = add(P, mul(E5, BF(static_cast<std::int64_t>(2772)), wb), wb);
    P = div(P, BF(static_cast<std::int64_t>(24024)), wb);
    const CX sA = sqrt(A, wb);
    const CX v1 = scale2(div(P, mul(A, sA, wb), wb), -2 * n);
    const CX v2 = mul(S, BF(static_cast<std::int64_t>(3)), wb);
    Res r;
    r.v = add(v1, v2, wb);
    const double lr = lm == ninf ? ninf : lm - 2.0 * static_cast<double>(n) - l2(A);
    const double e1 = l2(v1) + lsum(lr == ninf ? ninf : 6.0 * (lr + 2.0) + 1.0, std::log2(24.0 * static_cast<double>(n) + 64.0) - static_cast<double>(wb));
    const double e2 = T.l2sum + 1.6 + std::log2(16.0 * static_cast<double>(n) + 32.0) - static_cast<double>(wb);
    r.e = lsum(lsum(e1, e2), l2(r.v) - static_cast<double>(wb) + 2.0);
    return r;
}

using cd = std::complex<double>;

constexpr int plan_steps = 14;

struct RJPlan {
    std::vector<std::array<cd, 4>> roots;
    std::vector<cd>                w;
    std::vector<cd>                at;
};

cd to_cd(const CX& z) { return cd(to_d(z.re), to_d(z.im)); }

cd nearest_root(const cd& v, const cd& ref) {
    const cd r = std::sqrt(v);
    return std::abs(-r - ref) < std::abs(r - ref) ? -r : r;
}

RJPlan plan_at(const std::array<cd, 4>& args, const RJPlan* prev) {
    RJPlan out;
    const cd x = args[0], y = args[1], z = args[2], p = args[3];
    const cd delta = (p - x) * (p - y) * (p - z);
    std::array<cd, 4> cur = args;
    for (int m = 0; m < plan_steps; ++m) {
        std::array<cd, 4> r;
        for (int i = 0; i < 4; ++i) r[i] = prev ? nearest_root(cur[i], prev->roots[m][i]) : std::sqrt(cur[i]);
        const cd lam = r[0] * r[1] + r[1] * r[2] + r[2] * r[0];
        const cd d   = (r[3] + r[0]) * (r[3] + r[1]) * (r[3] + r[2]);
        const cd e   = delta * std::pow(4.0, -3.0 * m) / (d * d);
        cd wv = std::sqrt(e);
        if (prev && std::abs(-wv - prev->w[m]) < std::abs(wv - prev->w[m])) wv = -wv;
        cd a = (wv == cd(0.0)) ? cd(0.0) : std::atan(wv);
        if (prev) a += std::nearbyint((prev->at[m] - a).real() / 3.141592653589793) * 3.141592653589793;
        out.roots.push_back(r);
        out.w.push_back(wv);
        out.at.push_back(a);
        for (auto& c : cur) c = (c + lam) / 4.0;
    }
    return out;
}

bool plan_close(const RJPlan& a, const RJPlan& b) {
    for (int m = 0; m < plan_steps; ++m) {
        for (int i = 0; i < 4; ++i) {
            if (std::abs(a.roots[m][i] - b.roots[m][i]) > 0.3 * std::abs(b.roots[m][i]) + 1e-300) return false;
        }
        if (std::abs(a.at[m] - b.at[m]) > 0.3) return false;
        if (!std::isfinite(a.at[m].real()) || !std::isfinite(a.at[m].imag())) return false;
    }
    return true;
}

bool rj_plan(const CX& x, const CX& y, const CX& z, const CX& p, RJPlan& plan) {
    const std::array<CX, 4> in{x, y, z, p};
    double lmax = ninf;
    for (const CX& v : in) lmax = std::max(lmax, l2(v));
    if (lmax == ninf) return false;
    const double scale = std::exp2(-std::ceil(lmax));
    std::array<double, 4> rad, th;
    for (int i = 0; i < 4; ++i) {
        const cd v = to_cd(in[i]) * scale;
        rad[i] = std::abs(v);
        th[i]  = (v.imag() == 0.0 && v.real() < 0.0) ? (in[i].im.signbit() ? -3.141592653589793 : 3.141592653589793) : std::arg(v);
    }
    auto path = [&](double t) {
        std::array<cd, 4> a;
        for (int i = 0; i < 4; ++i) a[i] = ((1.0 - t) + t * rad[i]) * std::exp(cd(0.0, th[i] * t));
        return a;
    };
    RJPlan cur = plan_at(path(0.0), nullptr);
    double t = 0.0, h = 1.0 / 64.0;
    int    guard = 0;
    while (t < 1.0) {
        if (++guard > 200000) return false;
        const double t1 = std::min(1.0, t + h);
        RJPlan cand = plan_at(path(t1), &cur);
        if (plan_close(cand, cur) || h < 1e-12) {
            cur = std::move(cand);
            t   = t1;
            h   = std::min(2.0 * h, 1.0 / 16.0);
        } else {
            h *= 0.5;
        }
    }
    const double rs = std::sqrt(1.0 / scale);
    for (auto& r : cur.roots) {
        for (auto& v : r) v *= rs;
    }
    plan = std::move(cur);
    return true;
}

Res rj_core(const CX& x0, const CX& y0, const CX& z0, const CX& p0, std::size_t w, const RJPlan* plan = nullptr) {
    const std::size_t wb = w + 16;
    const CX A0 = div(add(add(add(x0, y0, wb + 8), z0, wb + 8), scale2(p0, 1), wb + 8), BF(static_cast<std::int64_t>(5)), wb);
    const CX delta = mul(mul(sub(p0, x0, wb + 8), sub(p0, y0, wb + 8), wb + 8), sub(p0, z0, wb + 8), wb);
    const double lm = std::max({l2(sub(A0, x0, wb)), l2(sub(A0, y0, wb)), l2(sub(A0, z0, wb)), l2(sub(A0, p0, wb))});
    CX x = x0, y = y0, z = z0, p = p0, A = A0;
    CX S;
    double eS = ninf;
    std::array<cd, 4> prev_roots{};
    std::int64_t n = 0;
    const double target = -(static_cast<double>(wb) + 4.0) / 6.0 - 2.0;
    for (;;) {
        const double lA = l2(A);
        if (lA == ninf) return bad();
        if (lm == ninf || lm - 2.0 * static_cast<double>(n) - lA <= target) break;
        if (n > 4000) return bad();
        std::array<CX, 4> rt{sqrt(x, wb), sqrt(y, wb), sqrt(z, wb), sqrt(p, wb)};
        if (plan) {
            for (int i = 0; i < 4; ++i) {
                const cd ref = n < plan_steps ? plan->roots[static_cast<std::size_t>(n)][i] : prev_roots[i];
                const cd v   = to_cd(rt[i]);
                if (std::abs(-v - ref) < std::abs(v - ref)) rt[i] = neg(rt[i]);
                prev_roots[i] = to_cd(rt[i]);
            }
        }
        const CX& sx = rt[0];
        const CX& sy = rt[1];
        const CX& sz = rt[2];
        const CX& sp = rt[3];
        const CX lam = add(add(mul(sx, sy, wb), mul(sy, sz, wb), wb), mul(sz, sx, wb), wb);
        const CX d   = mul(mul(add(sp, sx, wb), add(sp, sy, wb), wb), add(sp, sz, wb), wb);
        if (is_zero(d)) return bad();
        const CX e   = scale2(div(delta, sqr(d, wb), wb), -6 * n);
        Res rc = rc_one_plus(e, wb);
        if (!rc.ok) return bad();
        if (plan && n < plan_steps && !is_zero(e)) {
            const CX  we  = sqrt(e, wb);
            const cd  wd  = to_cd(we);
            const cd  wt  = plan->w[static_cast<std::size_t>(n)];
            const int sig = std::abs(wd - wt) <= std::abs(-wd - wt) ? 1 : -1;
            const double k = std::nearbyint((plan->at[static_cast<std::size_t>(n)] - static_cast<double>(sig) * std::atan(wd)).real() / 3.141592653589793);
            if (k != 0.0) {
                const CX corr = div(CX(BF::mul(pi(wb + 8), BF(static_cast<std::int64_t>(sig * k)), wctx(wb + 8))), we, wb);
                rc.v = add(rc.v, corr, wb);
                rc.e = lsum(rc.e, l2(corr) - static_cast<double>(wb) + 2.0);
            }
        }
        const CX term = scale2(div(rc.v, d, wb), -2 * n);
        S  = add(S, term, wb);
        eS = lsum(eS, lsum(rc.e - l2(d) - 2.0 * static_cast<double>(n), l2(term) - static_cast<double>(wb) + 4.0));
        x = scale2(add(x, lam, wb), -2);
        y = scale2(add(y, lam, wb), -2);
        z = scale2(add(z, lam, wb), -2);
        p = scale2(add(p, lam, wb), -2);
        A = scale2(add(A, lam, wb), -2);
        ++n;
    }
    const CX t  = scale2(recip(A, wb), -2 * n);
    const CX X  = mul(sub(A0, x0, wb), t, wb);
    const CX Y  = mul(sub(A0, y0, wb), t, wb);
    const CX Z  = mul(sub(A0, z0, wb), t, wb);
    const CX P  = scale2(neg(add(add(X, Y, wb), Z, wb)), -1);
    const CX XYZ = mul(mul(X, Y, wb), Z, wb);
    const CX P2 = sqr(P, wb);
    const CX E2 = sub(add(add(mul(X, Y, wb), mul(X, Z, wb), wb), mul(Y, Z, wb), wb), mul(P2, BF(static_cast<std::int64_t>(3)), wb), wb);
    const CX E3 = add(add(XYZ, scale2(mul(E2, P, wb), 1), wb), scale2(mul(P2, P, wb), 2), wb);
    const CX E4 = mul(add(add(scale2(XYZ, 1), mul(E2, P, wb), wb), mul(mul(P2, P, wb), BF(static_cast<std::int64_t>(3)), wb), wb), P, wb);
    const CX E5 = mul(XYZ, P2, wb);
    CX Q(BF(static_cast<std::int64_t>(24024)));
    Q = sub(Q, mul(E2, BF(static_cast<std::int64_t>(5148)), wb), wb);
    Q = add(Q, mul(sqr(E2, wb), BF(static_cast<std::int64_t>(2457)), wb), wb);
    Q = add(Q, mul(E3, BF(static_cast<std::int64_t>(4004)), wb), wb);
    Q = sub(Q, mul(mul(E2, E3, wb), BF(static_cast<std::int64_t>(4158)), wb), wb);
    Q = sub(Q, mul(E4, BF(static_cast<std::int64_t>(3276)), wb), wb);
    Q = add(Q, mul(E5, BF(static_cast<std::int64_t>(2772)), wb), wb);
    Q = div(Q, BF(static_cast<std::int64_t>(24024)), wb);
    const CX sA = sqrt(A, wb);
    const CX v1 = scale2(div(Q, mul(A, sA, wb), wb), -2 * n);
    const CX v2 = mul(S, BF(static_cast<std::int64_t>(6)), wb);
    Res r;
    r.v = add(v1, v2, wb);
    const double lr = lm == ninf ? ninf : lm - 2.0 * static_cast<double>(n) - l2(A);
    const double e1 = l2(v1) + lsum(lr == ninf ? ninf : 6.0 * (lr + 2.0) + 1.0, std::log2(24.0 * static_cast<double>(n) + 64.0) - static_cast<double>(wb));
    r.e = lsum(lsum(e1, eS + 2.6), l2(r.v) - static_cast<double>(wb) + 2.0);
    return r;
}

bool rj_valid(const CX& x, const CX& y, const CX& z, const CX& p) {
    auto nonneg = [](const CX& t) { return !t.re.signbit() || t.re.is_zero(); };
    auto eq = [](const CX& a, const CX& b) { return BF::compare(a.re, b.re) == BF::ordering::equal && BF::compare(a.im, b.im) == BF::ordering::equal; };
    auto conj_eq = [](const CX& a, const CX& b) {
        return BF::compare(a.re, b.re) == BF::ordering::equal && BF::compare(a.im, -b.im) == BF::ordering::equal;
    };
    if (nonneg(x) && nonneg(y) && nonneg(z) && !p.re.signbit() && !p.re.is_zero()) return true;
    if (eq(x, p) || eq(y, p) || eq(z, p)) return true;
    if (!p.im.is_zero() || !p.re.signbit()) {
        if (x.im.is_zero() && nonneg(x) && conj_eq(y, z)) return true;
        if (y.im.is_zero() && nonneg(y) && conj_eq(x, z)) return true;
        if (z.im.is_zero() && nonneg(z) && conj_eq(x, y)) return true;
    }
    return false;
}

Res rj_eval(const CX& x, const CX& y, const CX& z, const CX& p, std::size_t w) {
    if (rj_valid(x, y, z, p)) return rj_core(x, y, z, p, w);
    RJPlan plan;
    if (!rj_plan(x, y, z, p, plan)) return bad();
    return rj_core(x, y, z, p, w, &plan);
}

Approx from(const Res& r) {
    if (!r.ok) return approx_undefined();
    return approx(r.v, r.e);
}

bool all_real(std::initializer_list<const BC*> v) {
    for (const BC* p : v) {
        if (!p->imaginary().is_zero()) return false;
    }
    return true;
}

bool bad_input(std::initializer_list<const BC*> v) {
    for (const BC* p : v) {
        if (p->is_undefined() || !p->is_finite()) return true;
    }
    return false;
}

bool usable(const BF& r) { return !r.is_unordered() && r.is_finite(); }

} // namespace

BC carlson_rf(const BC& x, const BC& y, const BC& z, const BCC& ctx) {
    if (bad_input({&x, &y, &z})) return BC::undefined();
    if (all_real({&x, &y, &z})) {
        const BF r = math::carlson_rf(x.real(), y.real(), z.real(), wctx(32));
        if (usable(r)) return BC(math::carlson_rf(x.real(), y.real(), z.real(), ctx.real()), BF::zero());
    }
    const CX X = cx(x), Y = cx(y), Z = cx(z);
    if (static_cast<int>(is_zero(X)) + static_cast<int>(is_zero(Y)) + static_cast<int>(is_zero(Z)) > 1) return BC::undefined();
    return drive(ctx, [&](std::size_t w) { return from(rf_core(X, Y, Z, w)); });
}

BC carlson_rc(const BC& x, const BC& y, const BCC& ctx) {
    if (bad_input({&x, &y})) return BC::undefined();
    if (all_real({&x, &y})) {
        const BF r = math::carlson_rc(x.real(), y.real(), wctx(32));
        if (usable(r)) return BC(math::carlson_rc(x.real(), y.real(), ctx.real()), BF::zero());
    }
    const CX X = cx(x), Y = cx(y);
    if (is_zero(Y)) return BC::undefined();
    return drive(ctx, [&](std::size_t w) { return from(rc_core(X, Y, w)); });
}

BC carlson_rd(const BC& x, const BC& y, const BC& z, const BCC& ctx) {
    if (bad_input({&x, &y, &z})) return BC::undefined();
    if (all_real({&x, &y, &z})) {
        const BF r = math::carlson_rd(x.real(), y.real(), z.real(), wctx(32));
        if (usable(r)) return BC(math::carlson_rd(x.real(), y.real(), z.real(), ctx.real()), BF::zero());
    }
    const CX X = cx(x), Y = cx(y), Z = cx(z);
    if (is_zero(Z) || (is_zero(X) && is_zero(Y))) return BC::undefined();
    return drive(ctx, [&](std::size_t w) { return from(rd_core(X, Y, Z, w)); });
}

BC carlson_rj(const BC& x, const BC& y, const BC& z, const BC& p, const BCC& ctx) {
    if (bad_input({&x, &y, &z, &p})) return BC::undefined();
    if (all_real({&x, &y, &z, &p})) {
        const BF r = math::carlson_rj(x.real(), y.real(), z.real(), p.real(), wctx(32));
        if (usable(r)) return BC(math::carlson_rj(x.real(), y.real(), z.real(), p.real(), ctx.real()), BF::zero());
    }
    const CX X = cx(x), Y = cx(y), Z = cx(z), P = cx(p);
    if (is_zero(P) || static_cast<int>(is_zero(X)) + static_cast<int>(is_zero(Y)) + static_cast<int>(is_zero(Z)) > 1) return BC::undefined();
    return drive(ctx, [&](std::size_t w) { return from(rj_eval(X, Y, Z, P, w)); });
}

namespace {

Approx rg_raw(CX x, CX y, CX z, std::size_t w) {
    const int zeros = static_cast<int>(is_zero(x)) + static_cast<int>(is_zero(y)) + static_cast<int>(is_zero(z));
    const std::size_t wb = w + 16;
    if (zeros == 2) {
        const CX nz = !is_zero(x) ? x : (!is_zero(y) ? y : z);
        const CX v  = scale2(sqrt(nz, wb), -1);
        return approx(v, l2(v) - static_cast<double>(wb) + 1.0);
    }
    if (zeros == 1 && is_zero(z)) std::swap(x, z);
    const Res f = rf_core(x, y, z, wb);
    const Res d = rd_core(x, y, z, wb);
    if (!f.ok || !d.ok) return approx_undefined();
    const CX t1 = mul(z, f.v, wb);
    const CX q  = third(mul(sub(x, z, wb + 8), sub(y, z, wb + 8), wb), wb);
    const CX t2 = mul(q, d.v, wb);
    const CX t3 = div(mul(sqrt(x, wb), sqrt(y, wb), wb), sqrt(z, wb), wb);
    const CX v  = scale2(add(sub(t1, t2, wb), t3, wb), -1);
    double e = lsum(f.e + l2(z), d.e + l2(q));
    e = lsum(e, lsum(lsum(l2(t1), l2(t2)), l2(t3)) - static_cast<double>(wb) + 3.0);
    return approx(v, e - 1.0);
}

} // namespace

BC carlson_rg(const BC& x, const BC& y, const BC& z, const BCC& ctx) {
    if (bad_input({&x, &y, &z})) return BC::undefined();
    if (all_real({&x, &y, &z})) {
        const BF r = math::carlson_rg(x.real(), y.real(), z.real(), wctx(32));
        if (usable(r)) return BC(math::carlson_rg(x.real(), y.real(), z.real(), ctx.real()), BF::zero());
    }
    const CX X = cx(x), Y = cx(y), Z = cx(z);
    if (is_zero(X) && is_zero(Y) && is_zero(Z)) return BC::zero();
    return drive(ctx, [&](std::size_t w) { return rg_raw(X, Y, Z, w); });
}

namespace {

CX one_minus_exact(const CX& m) {
    const std::int64_t hi = std::max<std::int64_t>(2, m.re.is_zero() ? 2 : m.re.get_exp_base2() + 2);
    const std::int64_t lo = m.re.is_zero() ? 0 : std::min<std::int64_t>(0, m.re.exponent());
    return CX(BF::sub(BF::one(), m.re, wctx(static_cast<std::size_t>(hi - lo + 8))), m.im.with_sign(!m.im.signbit()));
}

Approx k_raw(const CX& m, std::size_t w) { return from(rf_core(CX(), one_minus_exact(m), CX(BF::one()), w)); }

Approx e_raw(const CX& m, std::size_t w) {
    const std::size_t wb = w + 8;
    const CX y = one_minus_exact(m);
    const Res f = rf_core(CX(), y, CX(BF::one()), wb);
    const Res d = rd_core(CX(), y, CX(BF::one()), wb);
    if (!f.ok || !d.ok) return approx_undefined();
    const CX t = third(mul(m, d.v, wb), wb);
    const CX v = sub(f.v, t, wb);
    return approx(v, lsum(lsum(f.e, d.e + l2(m) - 1.5), lsum(l2(f.v), l2(t)) - static_cast<double>(wb) + 2.0));
}

Approx pi_raw(const CX& n, const CX& m, std::size_t w) {
    const std::size_t wb = w + 8;
    const CX y = one_minus_exact(m);
    const CX p = one_minus_exact(n);
    const Res f = rf_core(CX(), y, CX(BF::one()), wb);
    const Res j = rj_eval(CX(), y, CX(BF::one()), p, wb);
    if (!f.ok || !j.ok) return approx_undefined();
    const CX t = third(mul(n, j.v, wb), wb);
    const CX v = add(f.v, t, wb);
    return approx(v, lsum(lsum(f.e, j.e + l2(n) - 1.5), lsum(l2(f.v), l2(t)) - static_cast<double>(wb) + 2.0));
}

enum class inc { f, e, pi };

Approx incomplete(inc kind, const CX& phi, const CX& n, const CX& m, std::size_t w) {
    const std::size_t wb = w + 16;
    const double xr = to_d(phi.re);
    const double d  = std::nearbyint(xr / 3.141592653589793);
    CX ph = phi;
    const bool away = std::fabs(xr) > 1.5707963267948966;
    std::int64_t di = 0;
    if (away) {
        di = static_cast<std::int64_t>(d);
        const std::size_t wp = wb + 64 + bits_of(std::log2(std::fabs(d) + 1.0)) + bits_of(l2(phi.re));
        ph = CX(BF::sub(phi.re, BF::mul(BF(di), pi(wp + 8), wctx(wp)), wctx(wp)), phi.im);
    }
    const std::size_t wt = wb + bits_of(std::fabs(l2(ph.im))) + 8;
    const CX s  = sin(ph, wt);
    const CX c  = cos(ph, wt);
    const CX c2 = sqr(c, wb + 8);
    const CX s2 = sqr(s, wb + 8);
    const CX y  = sub(CX(BF::one()), mul(m, s2, wb + 8), wb + 8);
    const Res f = rf_core(c2, y, CX(BF::one()), wb);
    if (!f.ok) return approx_undefined();
    CX     v = mul(s, f.v, wb);
    double e = f.e + l2(s);
    if (kind == inc::e) {
        const Res dd = rd_core(c2, y, CX(BF::one()), wb);
        if (!dd.ok) return approx_undefined();
        const CX coef = third(mul(mul(m, s2, wb), s, wb), wb);
        const CX t    = mul(coef, dd.v, wb);
        v = sub(v, t, wb);
        e = lsum(e, dd.e + l2(coef));
        e = lsum(e, l2(t) - static_cast<double>(wb) + 3.0);
    } else if (kind == inc::pi) {
        const CX pp = sub(CX(BF::one()), mul(n, s2, wb + 8), wb + 8);
        const Res jj = rj_eval(c2, y, CX(BF::one()), pp, wb);
        if (!jj.ok) return approx_undefined();
        const CX coef = third(mul(mul(n, s2, wb), s, wb), wb);
        const CX t    = mul(coef, jj.v, wb);
        v = add(v, t, wb);
        e = lsum(e, jj.e + l2(coef));
        e = lsum(e, l2(t) - static_cast<double>(wb) + 3.0);
    }
    e = lsum(e, l2(v) - static_cast<double>(wb) + 3.0);
    e = lsum(e, -l2(y) * 0.5 + l2(phi) - static_cast<double>(wb) - 40.0);
    if (away) {
        Approx P = kind == inc::f ? k_raw(m, wb) : (kind == inc::e ? e_raw(m, wb) : pi_raw(n, m, wb));
        if (P.undefined) return P;
        const CX q = mul(P.v, BF(2 * di), wb);
        v = add(v, q, wb);
        e = lsum(e, lsum(P.er, P.ei) + std::log2(2.0 * std::fabs(d)) + 0.5);
        e = lsum(e, l2(v) - static_cast<double>(wb) + 3.0);
    }
    return approx(v, e);
}

} // namespace

BC elliptic_k(const BC& m, const BCC& ctx) {
    if (bad_input({&m})) return BC::undefined();
    if (all_real({&m}) && BF::compare(m.real(), BF::one()) == BF::ordering::less) return BC(math::elliptic_k(m.real(), ctx.real()), BF::zero());
    if (all_real({&m}) && BF::compare(m.real(), BF::one()) == BF::ordering::equal) return BC::undefined();
    const CX M = cx(m);
    return drive(ctx, [&](std::size_t w) { return k_raw(M, w); });
}

BC elliptic_e(const BC& m, const BCC& ctx) {
    if (bad_input({&m})) return BC::undefined();
    if (all_real({&m}) && BF::compare(m.real(), BF::one()) != BF::ordering::greater) return BC(math::elliptic_e(m.real(), ctx.real()), BF::zero());
    const CX M = cx(m);
    return drive(ctx, [&](std::size_t w) { return e_raw(M, w); });
}

BC elliptic_pi(const BC& n, const BC& m, const BCC& ctx) {
    if (bad_input({&n, &m})) return BC::undefined();
    if (all_real({&n, &m})) {
        const BF r = math::elliptic_pi(n.real(), m.real(), wctx(32));
        if (usable(r) && BF::compare(m.real(), BF::one()) == BF::ordering::less && BF::compare(n.real(), BF::one()) == BF::ordering::less) {
            return BC(math::elliptic_pi(n.real(), m.real(), ctx.real()), BF::zero());
        }
    }
    const CX N = cx(n), M = cx(m);
    return drive(ctx, [&](std::size_t w) { return pi_raw(N, M, w); });
}

BC elliptic_f(const BC& phi, const BC& m, const BCC& ctx) {
    if (bad_input({&phi, &m})) return BC::undefined();
    if (all_real({&phi, &m})) {
        const BF r = math::elliptic_f(phi.real(), m.real(), wctx(32));
        if (usable(r) && BF::compare(m.real(), BF::one()) != BF::ordering::greater) return BC(math::elliptic_f(phi.real(), m.real(), ctx.real()), BF::zero());
    }
    if (phi.is_zero()) return BC::zero();
    const CX P = cx(phi), M = cx(m);
    return drive(ctx, [&](std::size_t w) { return incomplete(inc::f, P, CX(), M, w); });
}

BC elliptic_e(const BC& phi, const BC& m, const BCC& ctx) {
    if (bad_input({&phi, &m})) return BC::undefined();
    if (all_real({&phi, &m})) {
        const BF r = math::elliptic_e(phi.real(), m.real(), wctx(32));
        if (usable(r) && BF::compare(m.real(), BF::one()) != BF::ordering::greater) return BC(math::elliptic_e(phi.real(), m.real(), ctx.real()), BF::zero());
    }
    if (phi.is_zero()) return BC::zero();
    const CX P = cx(phi), M = cx(m);
    return drive(ctx, [&](std::size_t w) { return incomplete(inc::e, P, CX(), M, w); });
}

BC elliptic_pi(const BC& phi, const BC& n, const BC& m, const BCC& ctx) {
    if (bad_input({&phi, &n, &m})) return BC::undefined();
    if (all_real({&phi, &n, &m})) {
        const BF r = math::elliptic_pi(phi.real(), n.real(), m.real(), wctx(32));
        if (usable(r) && BF::compare(m.real(), BF::one()) != BF::ordering::greater) return BC(math::elliptic_pi(phi.real(), n.real(), m.real(), ctx.real()), BF::zero());
    }
    if (phi.is_zero()) return BC::zero();
    const CX P = cx(phi), N = cx(n), M = cx(m);
    return drive(ctx, [&](std::size_t w) { return incomplete(inc::pi, P, N, M, w); });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
