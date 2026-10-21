#ifndef FIZMO_MULTIPRECISION_FAST_FLOAT_DETAIL_HPP
#define FIZMO_MULTIPRECISION_FAST_FLOAT_DETAIL_HPP

#include "bigfloat_bridge.hpp"

#include <cmath>
#include <cstdint>
#include <type_traits>

#ifndef FIZMO_FAST_FLOAT128
#define FIZMO_FAST_FLOAT128 1
#endif

namespace fizmo {
namespace math {
namespace fwfast {

enum class fop {
    sin, cos, tan, csc, sec, cot,
    sinh, cosh, tanh, csch, sech, coth,
    arcsin, arccos, arctan, arccot, arcsec, arccsc,
    arcsinh, arccosh, arctanh, arccoth, arcsech, arccsch,
    exp, expm1, pow2, pow10,
    log, log1p, log2, log10,
    sqrt, cbrt,
    sinc, normalized_sinc, gudermannian, inv_gudermannian,
    erf, erfc, gamma, log_abs_gamma
};

enum class fop2 { atan2, pow };

inline double two_sum(double a, double b, double& e) noexcept {
    const double s  = a + b;
    const double bb = s - a;
    e = (a - (s - bb)) + (b - bb);
    return s;
}

inline double quick_two_sum(double a, double b, double& e) noexcept {
    const double s = a + b;
    e = b - (s - a);
    return s;
}

inline void split(double a, double& hi, double& lo) noexcept {
    const double splitter = 134217729.0;
    if (a > 6.69692879491417e+299 || a < -6.69692879491417e+299) {
        a *= 3.7252902984619140625e-09;
        const double t = splitter * a;
        hi = t - (t - a);
        lo = a - hi;
        hi *= 268435456.0;
        lo *= 268435456.0;
    } else {
        const double t = splitter * a;
        hi = t - (t - a);
        lo = a - hi;
    }
}

inline double two_prod(double a, double b, double& e) noexcept {
#if defined(__FMA__) || defined(__ARM_FEATURE_FMA) || defined(__aarch64__) || defined(_M_ARM64)
    const double p = a * b;
    e = std::fma(a, b, -p);
    return p;
#else
    const double p = a * b;
    double ah, al, bh, bl;
    split(a, ah, al);
    split(b, bh, bl);
    e = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return p;
#endif
}

inline void three_sum(double& a, double& b, double& c) noexcept {
    double t2, t3;
    const double t1 = two_sum(a, b, t2);
    a = two_sum(c, t1, t3);
    b = two_sum(t2, t3, c);
}

struct dd {
    double x0 = 0.0;
    double x1 = 0.0;

    dd() noexcept = default;
    dd(double a) noexcept : x0(a), x1(0.0) {}
    dd(double a, double b) noexcept : x0(a), x1(b) {}

    double lead() const noexcept { return x0; }
};

inline dd operator+(const dd& a, const dd& b) noexcept {
    double s2, t2;
    double s1 = two_sum(a.x0, b.x0, s2);
    const double t1 = two_sum(a.x1, b.x1, t2);
    s2 += t1;
    s1 = quick_two_sum(s1, s2, s2);
    s2 += t2;
    s1 = quick_two_sum(s1, s2, s2);
    return dd(s1, s2);
}

inline dd operator-(const dd& a) noexcept { return dd(-a.x0, -a.x1); }
inline dd operator-(const dd& a, const dd& b) noexcept { return a + (-b); }

inline dd operator*(const dd& a, const dd& b) noexcept {
    double p2;
    double p1 = two_prod(a.x0, b.x0, p2);
    p2 += a.x0 * b.x1 + a.x1 * b.x0;
    p1 = quick_two_sum(p1, p2, p2);
    return dd(p1, p2);
}

inline dd operator*(const dd& a, double b) noexcept {
    double p2;
    double p1 = two_prod(a.x0, b, p2);
    p2 += a.x1 * b;
    p1 = quick_two_sum(p1, p2, p2);
    return dd(p1, p2);
}

inline dd operator/(const dd& a, const dd& b) noexcept {
    double q1 = a.x0 / b.x0;
    dd r = a - b * q1;
    double q2 = r.x0 / b.x0;
    r = r - b * q2;
    const double q3 = r.x0 / b.x0;
    q1 = quick_two_sum(q1, q2, q2);
    return dd(q1, q2) + dd(q3);
}

inline dd ldexp(const dd& a, int e) noexcept { return dd(std::ldexp(a.x0, e), std::ldexp(a.x1, e)); }

inline dd sqrt(const dd& a) noexcept {
    if (a.x0 <= 0.0) return dd(0.0);
    const double x  = 1.0 / std::sqrt(a.x0);
    const double ax = a.x0 * x;
    const dd axd(ax);
    return axd + dd((a - axd * axd).x0 * (x * 0.5));
}

struct qd {
    double x[4] = { 0.0, 0.0, 0.0, 0.0 };

    qd() noexcept = default;
    qd(double a) noexcept : x{ a, 0.0, 0.0, 0.0 } {}
    qd(double a, double b, double c, double d) noexcept : x{ a, b, c, d } {}

    double lead() const noexcept { return x[0]; }
};

inline void renorm(double& c0, double& c1, double& c2, double& c3) noexcept {
    double s0, s1, s2 = 0.0, s3 = 0.0;
    if (std::isinf(c0)) return;
    s0 = quick_two_sum(c2, c3, c3);
    s0 = quick_two_sum(c1, s0, c2);
    c0 = quick_two_sum(c0, s0, c1);
    s0 = c0;
    s1 = c1;
    if (s1 != 0.0) {
        s1 = quick_two_sum(s1, c2, s2);
        if (s2 != 0.0) s2 = quick_two_sum(s2, c3, s3);
        else s1 = quick_two_sum(s1, c3, s2);
    } else {
        s0 = quick_two_sum(s0, c2, s1);
        if (s1 != 0.0) s1 = quick_two_sum(s1, c3, s2);
        else s0 = quick_two_sum(s0, c3, s1);
    }
    c0 = s0;
    c1 = s1;
    c2 = s2;
    c3 = s3;
}

inline void renorm(double& c0, double& c1, double& c2, double& c3, double& c4) noexcept {
    double s0, s1, s2 = 0.0, s3 = 0.0;
    if (std::isinf(c0)) return;
    s0 = quick_two_sum(c3, c4, c4);
    s0 = quick_two_sum(c2, s0, c3);
    s0 = quick_two_sum(c1, s0, c2);
    c0 = quick_two_sum(c0, s0, c1);
    s0 = c0;
    s1 = c1;
    if (s1 != 0.0) {
        s1 = quick_two_sum(s1, c2, s2);
        if (s2 != 0.0) {
            s2 = quick_two_sum(s2, c3, s3);
            if (s3 != 0.0) s3 += c4;
            else s2 = quick_two_sum(s2, c4, s3);
        } else {
            s1 = quick_two_sum(s1, c3, s2);
            if (s2 != 0.0) s2 = quick_two_sum(s2, c4, s3);
            else s1 = quick_two_sum(s1, c4, s2);
        }
    } else {
        s0 = quick_two_sum(s0, c2, s1);
        if (s1 != 0.0) {
            s1 = quick_two_sum(s1, c3, s2);
            if (s2 != 0.0) s2 = quick_two_sum(s2, c4, s3);
            else s1 = quick_two_sum(s1, c4, s2);
        } else {
            s0 = quick_two_sum(s0, c3, s1);
            if (s1 != 0.0) s1 = quick_two_sum(s1, c4, s2);
            else s0 = quick_two_sum(s0, c4, s1);
        }
    }
    c0 = s0;
    c1 = s1;
    c2 = s2;
    c3 = s3;
}

inline double quick_three_accum(double& a, double& b, double c) noexcept {
    double s = two_sum(b, c, b);
    s = two_sum(a, s, a);
    const bool za = a != 0.0;
    const bool zb = b != 0.0;
    if (za && zb) return s;
    if (!zb) {
        b = a;
        a = s;
    } else {
        a = s;
    }
    return 0.0;
}

inline qd operator+(const qd& a, const qd& b) noexcept {
    int i = 0, j = 0, k = 0;
    double u, v, t, s;
    double x[4] = { 0.0, 0.0, 0.0, 0.0 };
    if (std::fabs(a.x[i]) > std::fabs(b.x[j])) u = a.x[i++];
    else u = b.x[j++];
    if (std::fabs(a.x[i]) > std::fabs(b.x[j])) v = a.x[i++];
    else v = b.x[j++];
    u = quick_two_sum(u, v, v);
    while (k < 4) {
        if (i >= 4 && j >= 4) {
            x[k] = u;
            if (k < 3) x[++k] = v;
            break;
        }
        if (i >= 4) t = b.x[j++];
        else if (j >= 4) t = a.x[i++];
        else if (std::fabs(a.x[i]) > std::fabs(b.x[j])) t = a.x[i++];
        else t = b.x[j++];
        s = quick_three_accum(u, v, t);
        if (s != 0.0) x[k++] = s;
    }
    for (k = i; k < 4; ++k) x[3] += a.x[k];
    for (k = j; k < 4; ++k) x[3] += b.x[k];
    renorm(x[0], x[1], x[2], x[3]);
    return qd(x[0], x[1], x[2], x[3]);
}

inline qd operator-(const qd& a) noexcept { return qd(-a.x[0], -a.x[1], -a.x[2], -a.x[3]); }
inline qd operator-(const qd& a, const qd& b) noexcept { return a + (-b); }

inline qd operator*(const qd& a, const qd& b) noexcept {
    double q0, q1, q2, q3, q4, q5, q6, q7, q8, q9;
    double p0 = two_prod(a.x[0], b.x[0], q0);
    double p1 = two_prod(a.x[0], b.x[1], q1);
    double p2 = two_prod(a.x[1], b.x[0], q2);
    double p3 = two_prod(a.x[0], b.x[2], q3);
    double p4 = two_prod(a.x[1], b.x[1], q4);
    double p5 = two_prod(a.x[2], b.x[0], q5);
    three_sum(p1, p2, q0);
    three_sum(p2, q1, q2);
    three_sum(p3, p4, p5);
    double t0, t1;
    double s0 = two_sum(p2, p3, t0);
    double s1 = two_sum(q1, p4, t1);
    double s2 = q2 + p5;
    s1 = two_sum(s1, t0, t0);
    s2 += (t0 + t1);
    double p6 = two_prod(a.x[0], b.x[3], q6);
    double p7 = two_prod(a.x[1], b.x[2], q7);
    double p8 = two_prod(a.x[2], b.x[1], q8);
    double p9 = two_prod(a.x[3], b.x[0], q9);
    q0 = two_sum(q0, q3, q3);
    q4 = two_sum(q4, q5, q5);
    p6 = two_sum(p6, p7, p7);
    p8 = two_sum(p8, p9, p9);
    t0 = two_sum(q0, q4, t1);
    t1 += (q3 + q5);
    double r1;
    const double r0 = two_sum(p6, p8, r1);
    r1 += (p7 + p9);
    q3 = two_sum(t0, r0, q4);
    q4 += (t1 + r1);
    t0 = two_sum(q3, s1, t1);
    t1 += q4;
    t1 += a.x[1] * b.x[3] + a.x[2] * b.x[2] + a.x[3] * b.x[1] + q6 + q7 + q8 + q9 + s2;
    renorm(p0, p1, s0, t0, t1);
    return qd(p0, p1, s0, t0);
}

inline qd operator*(const qd& a, double b) noexcept { return a * qd(b); }

inline qd operator/(const qd& a, const qd& b) noexcept {
    double q0 = a.x[0] / b.x[0];
    qd r = a - b * q0;
    double q1 = r.x[0] / b.x[0];
    r = r - b * q1;
    double q2 = r.x[0] / b.x[0];
    r = r - b * q2;
    double q3 = r.x[0] / b.x[0];
    r = r - b * q3;
    double q4 = r.x[0] / b.x[0];
    renorm(q0, q1, q2, q3, q4);
    return qd(q0, q1, q2, q3);
}

inline qd ldexp(const qd& a, int e) noexcept {
    return qd(std::ldexp(a.x[0], e), std::ldexp(a.x[1], e), std::ldexp(a.x[2], e), std::ldexp(a.x[3], e));
}

inline qd sqrt(const qd& a) noexcept {
    if (a.x[0] <= 0.0) return qd(0.0);
    qd r(1.0 / std::sqrt(a.x[0]));
    const qd h = ldexp(a, -1);
    for (int i = 0; i < 3; ++i) r = r + r * (qd(0.5) - h * (r * r));
    return a * r;
}

template <class W> struct wt;

template <> struct wt<dd> {
    static constexpr int parts      = 2;
    static constexpr int newton     = 1;
    static constexpr int sin_terms  = 15;
    static constexpr int exp_terms  = 10;
    static constexpr int exp_halves = 8;
};

template <> struct wt<qd> {
    static constexpr int parts      = 4;
    static constexpr int newton     = 2;
    static constexpr int sin_terms  = 27;
    static constexpr int exp_terms  = 15;
    static constexpr int exp_halves = 12;
};

struct split6 {
    double p[6];
};

inline const split6& c_half_pi() {
    static const split6 c{ { 0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54, -0x1.f1976b7ed8fbcp-110, 0x1.4cf98e804177dp-164, 0x1.31d89cd9128a5p-218, 0x1.0f31c6809bbdfp-276 } };
    return c;
}

inline const split6& c_pi() {
    static const split6 c{ { 0x1.921fb54442d18p+1, 0x1.1a62633145c07p-53, -0x1.f1976b7ed8fbcp-109, 0x1.4cf98e804177dp-163, 0x1.31d89cd9128a5p-217, 0x1.0f31c6809bbdfp-275 } };
    return c;
}

inline const split6& c_ln2() {
    static const split6 c{ { 0x1.62e42fefa39efp-1, 0x1.abc9e3b39803fp-56, 0x1.7b57a079a1934p-111, -0x1.ace93a4ebe5d1p-165, -0x1.23a2a82ea0c24p-219, 0x1.d881b7aeb2615p-274 } };
    return c;
}

inline const split6& c_ln10() {
    static const split6 c{ { 0x1.26bb1bbb55516p+1, -0x1.f48ad494ea3e9p-53, -0x1.9ebae3ae0260cp-107, -0x1.2d10378be1cf1p-161, 0x1.0403e05ae52c6p-215, -0x1.fa509cafdf466p-271 } };
    return c;
}

inline const split6& c_log2_e() {
    static const split6 c{ { 0x1.71547652b82fep+0, 0x1.777d0ffda0d24p-56, -0x1.60bb8a5442ab9p-110, -0x1.4b52d3ba6d74dp-166, 0x1.9a342648fbc39p-220, -0x1.e0455744994eep-274 } };
    return c;
}

inline const split6& c_log10_e() {
    static const split6 c{ { 0x1.bcb7b1526e50ep-2, 0x1.95355baaafad3p-57, 0x1.ee191f71a3012p-112, 0x1.7268808e8fcb5p-167, 0x1.3de3a94f1d509p-223, 0x1.df42805e7e524p-278 } };
    return c;
}

inline dd make(const split6& c, dd*) noexcept { return dd(c.p[0], c.p[1]); }
inline qd make(const split6& c, qd*) noexcept { return qd(c.p[0], c.p[1], c.p[2], c.p[3]); }

template <class W>
W konst(const split6& c) noexcept { return make(c, static_cast<W*>(nullptr)); }

template <class W>
W reduce(const W& x, double k, const split6& c) noexcept {
    W r = x;
    for (int i = 0; i < wt<W>::parts + 2; ++i) {
        double e;
        const double h = two_prod(k, c.p[i], e);
        r = r - W(h);
        r = r - W(e);
    }
    return r;
}

template <class W>
struct tables {
    W inv_fact[64];

    tables() {
        inv_fact[0] = W(1.0);
        for (int i = 1; i < 64; ++i) inv_fact[i] = inv_fact[i - 1] / W(static_cast<double>(i));
    }

    static const tables& get() {
        static const tables t;
        return t;
    }
};

template <class W>
W sin_taylor(const W& r) noexcept {
    const W* f = tables<W>::get().inv_fact;
    const W u = r * r;
    const int n = wt<W>::sin_terms;
    W p = n % 2 ? -f[2 * n + 1] : f[2 * n + 1];
    for (int j = n - 1; j >= 0; --j) p = (j % 2 ? -f[2 * j + 1] : f[2 * j + 1]) + u * p;
    return r * p;
}

template <class W>
W cos_taylor(const W& r) noexcept {
    const W* f = tables<W>::get().inv_fact;
    const W u = r * r;
    const int n = wt<W>::sin_terms;
    W p = n % 2 ? -f[2 * n] : f[2 * n];
    for (int j = n - 1; j >= 0; --j) p = (j % 2 ? -f[2 * j] : f[2 * j]) + u * p;
    return p;
}

enum class want { sin, cos, both };

template <class W>
bool sincos(const W& x, W& s, W& c, want w = want::both) noexcept {
    const double a = x.lead();
    if (!(std::fabs(a) < 0x1p40)) return false;
    const double k = std::nearbyint(a * 0.63661977236758134308);
    const W r = k == 0.0 ? x : reduce(x, k, c_half_pi());
    long long q = static_cast<long long>(k) % 4;
    if (q < 0) q += 4;
    const bool swap = q % 2 == 1;
    const bool need_ss = w == want::both || ((w == want::sin) != swap);
    const bool need_cc = w == want::both || ((w == want::cos) != swap);
    const W ss = need_ss ? sin_taylor(r) : W(0.0);
    const W cc = need_cc ? cos_taylor(r) : W(0.0);
    switch (q) {
        case 0: s = ss; c = cc; break;
        case 1: s = cc; c = -ss; break;
        case 2: s = -ss; c = -cc; break;
        default: s = -cc; c = ss; break;
    }
    return true;
}

template <class W>
W expm1_small(const W& r0) noexcept {
    const W* f = tables<W>::get().inv_fact;
    const int m = wt<W>::exp_halves;
    const W r = ldexp(r0, -m);
    const int n = wt<W>::exp_terms;
    W p = f[n];
    for (int i = n - 1; i >= 1; --i) p = f[i] + r * p;
    W s = r * p;
    for (int i = 0; i < m; ++i) s = s * (s + W(2.0));
    return s;
}

template <class W>
bool exp(const W& x, W& out) noexcept {
    const double a = x.lead();
    if (!(a < 709.0 && a > -700.0)) return false;
    const double k = std::nearbyint(a * 1.4426950408889634074);
    const W r = k == 0.0 ? x : reduce(x, k, c_ln2());
    out = ldexp(expm1_small(r) + W(1.0), static_cast<int>(k));
    return true;
}

template <class W>
bool expm1(const W& x, W& out) noexcept {
    const double a = x.lead();
    if (std::fabs(a) < 0.34) {
        out = expm1_small(x);
        return true;
    }
    W e;
    if (!exp(x, e)) return false;
    out = e - W(1.0);
    return true;
}

inline dd down(const qd& a) noexcept { return dd(a.x[0], a.x[1]); }
inline qd up(const dd& a) noexcept { return qd(a.x0, a.x1, 0.0, 0.0); }

template <class W>
bool log1p_of(const W& d, W& out) noexcept;

template <class W>
bool atan2(const W& y, const W& x, W& out) noexcept;

template <class W>
bool log1p_of(const W& d, W& out) noexcept {
    const double a = d.lead();
    if (!(a > -1.0)) return false;
    int steps = wt<W>::newton;
    W y;
    if constexpr (std::is_same<W, qd>::value) {
        dd yd;
        if (!log1p_of(down(d), yd)) return false;
        y = up(yd);
        steps = 1;
    }
    if (std::fabs(a) < 0.5) {
        if constexpr (!std::is_same<W, qd>::value) y = W(std::log1p(a));
        for (int i = 0; i < steps; ++i) {
            W e;
            if (!expm1(-y, e)) return false;
            y = y + (d * (e + W(1.0)) + e);
        }
        out = y;
        return true;
    }
    const W x = W(1.0) + d;
    if (!(x.lead() > 0.0)) return false;
    if constexpr (!std::is_same<W, qd>::value) y = W(std::log(x.lead()));
    for (int i = 0; i < steps; ++i) {
        W e;
        if (!exp(-y, e)) return false;
        y = y + (x * e - W(1.0));
    }
    out = y;
    return true;
}

template <class W>
bool log(const W& x, W& out) noexcept {
    if (!(x.lead() > 0.0)) return false;
    return log1p_of(x - W(1.0), out);
}

template <class W>
bool atan(const W& x, W& out) noexcept {
    int steps = wt<W>::newton;
    W y;
    if constexpr (std::is_same<W, qd>::value) {
        dd yd;
        if (!atan(down(x), yd)) return false;
        y = up(yd);
        steps = 1;
    } else {
        y = W(std::atan(x.lead()));
    }
    for (int i = 0; i < steps; ++i) {
        W s, c;
        if (!sincos(y, s, c)) return false;
        y = y + (x * c - s) * c;
    }
    out = y;
    return true;
}

template <class W>
bool atan2(const W& y, const W& x, W& out) noexcept {
    const double ya = y.lead();
    const double xa = x.lead();
    if (xa == 0.0 && ya == 0.0) return false;
    if (xa == 0.0) {
        out = ya > 0.0 ? konst<W>(c_half_pi()) : -konst<W>(c_half_pi());
        return true;
    }
    if (ya == 0.0) {
        if (xa > 0.0) return false;
        out = konst<W>(c_pi());
        return true;
    }
    const W r = sqrt(x * x + y * y);
    const W xx = x / r;
    const W yy = y / r;
    int steps = wt<W>::newton;
    W z;
    if constexpr (std::is_same<W, qd>::value) {
        dd zd;
        if (!atan2(down(y), down(x), zd)) return false;
        z = up(zd);
        steps = 1;
    } else {
        z = W(std::atan2(ya, xa));
    }
    for (int i = 0; i < steps; ++i) {
        W s, c;
        if (!sincos(z, s, c)) return false;
        if (std::fabs(xx.lead()) > std::fabs(yy.lead())) z = z + (yy - s) / c;
        else z = z - (xx - c) / s;
    }
    out = z;
    return true;
}

template <class W>
W one_minus_sq(const W& x) noexcept { return (W(1.0) - x) * (W(1.0) + x); }

template <class W>
bool asin(const W& x, W& out) noexcept {
    if (!(std::fabs(x.lead()) < 1.0)) return false;
    return atan2(x, sqrt(one_minus_sq(x)), out);
}

template <class W>
bool acos(const W& x, W& out) noexcept {
    if (!(std::fabs(x.lead()) < 1.0)) return false;
    return atan2(sqrt(one_minus_sq(x)), x, out);
}

template <class W>
bool sinh(const W& x, W& out) noexcept {
    const double a = x.lead();
    if (std::fabs(a) < 0.34) {
        const W e = expm1_small(x);
        out = ldexp(e + e / (e + W(1.0)), -1);
        return true;
    }
    W e;
    if (!exp(x, e)) return false;
    out = ldexp(e - W(1.0) / e, -1);
    return true;
}

template <class W>
bool cosh(const W& x, W& out) noexcept {
    W e;
    if (!exp(x, e)) return false;
    out = ldexp(e + W(1.0) / e, -1);
    return true;
}

template <class W>
bool tanh(const W& x, W& out) noexcept {
    const double a = x.lead();
    if (std::fabs(a) > 200.0) return false;
    W e;
    if (!expm1(ldexp(x, 1), e)) return false;
    out = e / (e + W(2.0));
    return true;
}

template <class W>
bool asinh(const W& x, W& out) noexcept {
    const double a = x.lead();
    const bool neg = a < 0.0;
    const W ax = neg ? -x : x;
    W r;
    if (std::fabs(a) > 0x1p200) {
        if (!log(ax, r)) return false;
        r = r + konst<W>(c_ln2());
    } else {
        const W a2 = ax * ax;
        if (!log1p_of(ax + a2 / (W(1.0) + sqrt(a2 + W(1.0))), r)) return false;
    }
    out = neg ? -r : r;
    return true;
}

template <class W>
bool acosh(const W& x, W& out) noexcept {
    if (!(x.lead() >= 1.0)) return false;
    const W t = x - W(1.0);
    if (!(t.lead() > 0.0)) return false;
    if (x.lead() > 0x1p200) {
        if (!log(x, out)) return false;
        out = out + konst<W>(c_ln2());
        return true;
    }
    return log1p_of(t + sqrt(ldexp(t, 1) + t * t), out);
}

template <class W>
bool atanh(const W& x, W& out) noexcept {
    if (!(std::fabs(x.lead()) < 1.0)) return false;
    if (!log1p_of(ldexp(x, 1) / (W(1.0) - x), out)) return false;
    out = ldexp(out, -1);
    return true;
}

template <class W>
bool cbrt(const W& x, W& out) noexcept {
    const double a = x.lead();
    W y(std::cbrt(a));
    for (int i = 0; i < wt<W>::newton + 1; ++i) {
        const W y2 = y * y;
        y = y - (y2 * y - x) / (W(3.0) * y2);
    }
    out = y;
    return true;
}

template <class W>
bool unary_w(fop o, const W& x, W& r) noexcept {
    W s, c, t;
    switch (o) {
        case fop::sin:
            if (!sincos(x, s, c, want::sin)) return false;
            r = s;
            return true;
        case fop::cos:
            if (!sincos(x, s, c, want::cos)) return false;
            r = c;
            return true;
        case fop::tan:
            if (!sincos(x, s, c)) return false;
            r = s / c;
            return true;
        case fop::csc:
            if (!sincos(x, s, c, want::sin)) return false;
            r = W(1.0) / s;
            return true;
        case fop::sec:
            if (!sincos(x, s, c, want::cos)) return false;
            r = W(1.0) / c;
            return true;
        case fop::cot:
            if (!sincos(x, s, c)) return false;
            r = c / s;
            return true;
        case fop::sinh: return sinh(x, r);
        case fop::cosh: return cosh(x, r);
        case fop::tanh: return tanh(x, r);
        case fop::csch:
            if (!sinh(x, t)) return false;
            r = W(1.0) / t;
            return true;
        case fop::sech:
            if (!cosh(x, t)) return false;
            r = W(1.0) / t;
            return true;
        case fop::coth:
            if (!tanh(x, t)) return false;
            r = W(1.0) / t;
            return true;
        case fop::arcsin: return asin(x, r);
        case fop::arccos: return acos(x, r);
        case fop::arctan: return atan(x, r);
        case fop::arccot:
            if (!atan(W(1.0) / x, t)) return false;
            r = x.lead() > 0.0 ? t : konst<W>(c_pi()) + t;
            return true;
        case fop::arcsec:
            if (!(std::fabs(x.lead()) > 1.0)) return false;
            return acos(W(1.0) / x, r);
        case fop::arccsc:
            if (!(std::fabs(x.lead()) > 1.0)) return false;
            return asin(W(1.0) / x, r);
        case fop::arcsinh: return asinh(x, r);
        case fop::arccosh: return acosh(x, r);
        case fop::arctanh: return atanh(x, r);
        case fop::arccoth:
            if (!(std::fabs(x.lead()) > 1.0)) return false;
            return atanh(W(1.0) / x, r);
        case fop::arcsech:
            if (!(x.lead() > 0.0 && x.lead() < 1.0)) return false;
            return acosh(W(1.0) / x, r);
        case fop::arccsch: return asinh(W(1.0) / x, r);
        case fop::exp: return exp(x, r);
        case fop::expm1: return expm1(x, r);
        case fop::pow2: return exp(x * konst<W>(c_ln2()), r);
        case fop::pow10: return exp(x * konst<W>(c_ln10()), r);
        case fop::log: return log(x, r);
        case fop::log1p: return log1p_of(x, r);
        case fop::log2:
            if (!log(x, t)) return false;
            r = t * konst<W>(c_log2_e());
            return true;
        case fop::log10:
            if (!log(x, t)) return false;
            r = t * konst<W>(c_log10_e());
            return true;
        case fop::sqrt:
            if (!(x.lead() > 0.0)) return false;
            r = sqrt(x);
            return true;
        case fop::cbrt: return cbrt(x, r);
        case fop::sinc:
            if (!sincos(x, s, c, want::sin)) return false;
            r = s / x;
            return true;
        case fop::normalized_sinc: {
            const double a = x.lead();
            if (!(std::fabs(a) < 0x1p30) || std::nearbyint(a) == a) return false;
            const double k = std::nearbyint(a * 2.0);
            const W f = x - W(k * 0.5);
            const W px = f * konst<W>(c_pi());
            if (!sincos(px, s, c)) return false;
            long long q = static_cast<long long>(k) % 4;
            if (q < 0) q += 4;
            const W v = q == 0 ? s : q == 1 ? c : q == 2 ? -s : -c;
            r = v / (x * konst<W>(c_pi()));
            return true;
        }
        case fop::gudermannian:
            if (!tanh(ldexp(x, -1), t)) return false;
            if (!atan(t, r)) return false;
            r = ldexp(r, 1);
            return true;
        case fop::inv_gudermannian:
            if (!(std::fabs(x.lead()) < 1.5)) return false;
            if (!sincos(x, s, c)) return false;
            return asinh(s / c, r);
        default:
            return false;
    }
}

inline bool tail_zero(const dd& w) noexcept { return w.x1 == 0.0; }
inline bool tail_zero(const qd& w) noexcept { return w.x[1] == 0.0 && w.x[2] == 0.0 && w.x[3] == 0.0; }

template <class W>
bool binary_w(fop2 o, const W& a, const W& b, W& r) noexcept {
    switch (o) {
        case fop2::atan2: return atan2(a, b, r);
        case fop2::pow: {
            const double bl = b.lead();
            if (std::fabs(bl) <= 1024.0 && std::nearbyint(bl) == bl && tail_zero(b) && a.lead() != 0.0) {
                W acc(1.0);
                W p = a;
                for (long long n = static_cast<long long>(std::fabs(bl)); n != 0; n >>= 1) {
                    if (n & 1) acc = acc * p;
                    if (n > 1) p = p * p;
                    if (!std::isfinite(acc.lead()) || !std::isfinite(p.lead())) return false;
                }
                r = bl < 0.0 ? W(1.0) / acc : acc;
                return true;
            }
            if (!(a.lead() > 0.0)) return false;
            W l;
            if (!log(a, l)) return false;
            return exp(b * l, r);
        }
    }
    return false;
}

inline bool unary_double(fop o, double x, double& r) noexcept {
    switch (o) {
        case fop::sin: r = std::sin(x); break;
        case fop::cos: r = std::cos(x); break;
        case fop::tan: r = std::tan(x); break;
        case fop::csc: r = 1.0 / std::sin(x); break;
        case fop::sec: r = 1.0 / std::cos(x); break;
        case fop::cot: r = 1.0 / std::tan(x); break;
        case fop::sinh: r = std::sinh(x); break;
        case fop::cosh: r = std::cosh(x); break;
        case fop::tanh: r = std::tanh(x); break;
        case fop::csch: r = 1.0 / std::sinh(x); break;
        case fop::sech: r = 1.0 / std::cosh(x); break;
        case fop::coth: r = 1.0 / std::tanh(x); break;
        case fop::arcsin: if (!(std::fabs(x) < 1.0)) return false; r = std::asin(x); break;
        case fop::arccos: if (!(std::fabs(x) < 1.0)) return false; r = std::acos(x); break;
        case fop::arctan: r = std::atan(x); break;
        case fop::arccot: r = x > 0.0 ? std::atan(1.0 / x) : 3.14159265358979323846 + std::atan(1.0 / x); break;
        case fop::arcsec: if (!(std::fabs(x) > 1.0)) return false; r = std::acos(1.0 / x); break;
        case fop::arccsc: if (!(std::fabs(x) > 1.0)) return false; r = std::asin(1.0 / x); break;
        case fop::arcsinh: r = std::asinh(x); break;
        case fop::arccosh: if (!(x > 1.0)) return false; r = std::acosh(x); break;
        case fop::arctanh: if (!(std::fabs(x) < 1.0)) return false; r = std::atanh(x); break;
        case fop::arccoth: if (!(std::fabs(x) > 1.0)) return false; r = std::atanh(1.0 / x); break;
        case fop::arcsech: if (!(x > 0.0 && x < 1.0)) return false; r = std::acosh(1.0 / x); break;
        case fop::arccsch: r = std::asinh(1.0 / x); break;
        case fop::exp: r = std::exp(x); break;
        case fop::expm1: r = std::expm1(x); break;
        case fop::pow2: r = std::exp2(x); break;
        case fop::pow10: r = std::pow(10.0, x); break;
        case fop::log: if (!(x > 0.0)) return false; r = std::log(x); break;
        case fop::log1p: if (!(x > -1.0)) return false; r = std::log1p(x); break;
        case fop::log2: if (!(x > 0.0)) return false; r = std::log2(x); break;
        case fop::log10: if (!(x > 0.0)) return false; r = std::log10(x); break;
        case fop::sqrt: if (!(x > 0.0)) return false; r = std::sqrt(x); break;
        case fop::cbrt: r = std::cbrt(x); break;
        case fop::sinc: r = std::sin(x) / x; break;
        case fop::normalized_sinc: {
            if (std::nearbyint(x) == x || !(std::fabs(x) < 0x1p30)) return false;
            const double k = std::nearbyint(x * 2.0);
            const double f = (x - k * 0.5) * 3.14159265358979323846;
            long long q = static_cast<long long>(k) % 4;
            if (q < 0) q += 4;
            const double v = q == 0 ? std::sin(f) : q == 1 ? std::cos(f) : q == 2 ? -std::sin(f) : -std::cos(f);
            r = v / (x * 3.14159265358979323846);
            break;
        }
        case fop::gudermannian: r = 2.0 * std::atan(std::tanh(x * 0.5)); break;
        case fop::inv_gudermannian: if (!(std::fabs(x) < 1.5)) return false; r = std::asinh(std::tan(x)); break;
        case fop::erf: r = std::erf(x); break;
        case fop::erfc: r = std::erfc(x); break;
        case fop::gamma: if (x <= 0.0 && std::nearbyint(x) == x) return false; r = std::tgamma(x); break;
        case fop::log_abs_gamma: if (x <= 0.0 && std::nearbyint(x) == x) return false; r = std::lgamma(x); break;
        default: return false;
    }
    return std::isfinite(r);
}

inline bool binary_double(fop2 o, double a, double b, double& r) noexcept {
    switch (o) {
        case fop2::atan2:
            if (a == 0.0 && b >= 0.0) return false;
            r = std::atan2(a, b);
            break;
        case fop2::pow:
            if (a == 0.0) return false;
            if (!(a > 0.0) && std::nearbyint(b) != b) return false;
            r = std::pow(a, b);
            break;
    }
    return std::isfinite(r);
}

template <class F> struct level : std::integral_constant<int, 0> {};
template <multiprecision::sign S> struct level<multiprecision::floatmp<32, 24, S>> : std::integral_constant<int, 1> {};
template <multiprecision::sign S> struct level<multiprecision::floatmp<64, 53, S>> : std::integral_constant<int, 2> {};
#if FIZMO_FAST_FLOAT128
template <multiprecision::sign S> struct level<multiprecision::floatmp<128, 112, S>> : std::integral_constant<int, 3> {};
#endif

template <class F>
constexpr bool enabled = level<F>::value != 0;

inline bool worth_it(int lvl, fop o) noexcept {
    if (lvl != 3) return true;
    switch (o) {
        case fop::sin: case fop::cos: case fop::tan: case fop::csc: case fop::sec: case fop::cot:
        case fop::sinh: case fop::cosh: case fop::tanh: case fop::csch: case fop::sech: case fop::coth:
        case fop::arcsin: case fop::arccos: case fop::arctan: case fop::arccot: case fop::arcsec: case fop::arccsc:
        case fop::arctanh: case fop::arccoth:
        case fop::exp: case fop::expm1: case fop::pow2: case fop::pow10:
        case fop::sinc: case fop::normalized_sinc: case fop::gudermannian:
            return true;
        default:
            return false;
    }
}

inline bool worth_it(int lvl, fop2 o) noexcept {
    return lvl != 3 || o == fop2::atan2;
}

template <class S>
long long as_ll(const S& s) noexcept {
    return s.is_negative() ? -static_cast<long long>((-s).get_lowest_bits()) : static_cast<long long>(s.get_lowest_bits());
}

template <class C>
constexpr bool small_c = multiprecision::fdetail::small_format<C::math_bits, C::mantissa_bits>::value;

template <class C>
using small_ops_of = multiprecision::fdetail::small_ops<C::math_bits, C::mantissa_bits>;

inline void split_double(double v, std::uint64_t& m, long long& e) noexcept {
    int ex = 0;
    const double f = std::frexp(std::fabs(v), &ex);
    m = static_cast<std::uint64_t>(std::ldexp(f, 53));
    e = static_cast<long long>(ex) - 53;
}

template <class C>
bool small_from_parts(const double* c, int n, C& out) noexcept {
    using ops = small_ops_of<C>;
    if (!std::isfinite(c[0]) || c[0] == 0.0) return false;
    const bool neg = c[0] < 0.0;
    std::uint64_t mh = 0;
    long long eh = 0;
    split_double(c[0], mh, eh);
    std::uint64_t S = mh << 10;
    const long long unit = eh - 10;
    bool sticky = false;
    for (int i = 1; i < n; ++i) {
        if (c[i] == 0.0) continue;
        if (!std::isfinite(c[i])) return false;
        std::uint64_t ml = 0;
        long long el = 0;
        split_double(c[i], ml, el);
        const long long sh = el - unit;
        std::uint64_t q = 0;
        bool rem = false;
        if (sh >= 0) {
            if (sh > 9) return false;
            q = ml << sh;
        } else if (-sh >= 64) {
            rem = true;
        } else {
            q = ml >> (-sh);
            rem = (ml & multiprecision::fdetail::sfmath::low_mask(static_cast<int>(-sh))) != 0;
        }
        if ((c[i] < 0.0) == neg) {
            S += q;
            sticky = sticky || rem;
        } else {
            S -= q;
            if (rem) {
                S -= 1;
                sticky = true;
            }
        }
    }
    const multiprecision::fdetail::sf_value v = ops::pack(neg, S, unit, sticky);
    out = C::from_raw(v.neg, typename C::store_t(v.bits));
    return out.is_finite() && !out.is_zero();
}

template <class C>
int to_parts(const C& x, double* out) noexcept {
    if constexpr (small_c<C>) {
        using ops = small_ops_of<C>;
        std::uint64_t m = 0;
        long long e = 0;
        ops::unpack(x.get_bits().get_lowest_bits(), m, e);
        const double sgn = x.is_negative() ? -1.0 : 1.0;
        if (ops::precision <= 53) {
            out[0] = sgn * std::ldexp(static_cast<double>(m), static_cast<int>(e));
            return 1;
        }
        const int extra = ops::precision - 53;
        const std::uint64_t low = m & multiprecision::fdetail::sfmath::low_mask(extra);
        out[0] = sgn * std::ldexp(static_cast<double>(m - low), static_cast<int>(e));
        out[1] = sgn * std::ldexp(static_cast<double>(low), static_cast<int>(e));
        return low ? 2 : 1;
    }
    const auto fr = x.frexp();
    auto m = fr.first;
    const long long base = as_ll(fr.second) - static_cast<long long>(C::mantissa_bits);
    const double sgn = x.is_negative() ? -1.0 : 1.0;
    int n = 0;
    while (!m.is_zero() && n < 4) {
        const long long L = static_cast<long long>(m.highest_bit()) + 1;
        const long long take = L < 53 ? L : 53;
        const auto top = m >> static_cast<std::size_t>(L - take);
        out[n++] = sgn * std::ldexp(static_cast<double>(top.get_lowest_bits()), static_cast<int>(base + L - take));
        m = m - (top << static_cast<std::size_t>(L - take));
    }
    return n;
}

template <class C>
bool from_parts(const double* c, int n, C& out) noexcept {
    if constexpr (small_c<C>) {
        if (n <= 2) return small_from_parts(c, n, out);
    }
    using wide_t   = typename C::wide_t;
    using sstore_t = typename C::sstore_t;
    if (!std::isfinite(c[0]) || c[0] == 0.0) return false;
    const bool neg = c[0] < 0.0;
    int e0 = 0;
    std::frexp(c[0], &e0);
    const long long unit = static_cast<long long>(e0) - static_cast<long long>(2 * C::math_bits - 3);
    wide_t pos, sub;
    bool sticky = false;
    for (int i = 0; i < n; ++i) {
        const double v = neg ? -c[i] : c[i];
        if (v == 0.0) continue;
        if (!std::isfinite(v)) return false;
        int ei = 0;
        const double fm = std::frexp(std::fabs(v), &ei);
        const std::uint64_t mi = static_cast<std::uint64_t>(std::ldexp(fm, 53));
        const long long sh = static_cast<long long>(ei) - 53 - unit;
        wide_t w;
        if (sh >= 0) {
            if (sh + 53 >= static_cast<long long>(2 * C::math_bits)) return false;
            w = wide_t(mi) << static_cast<std::size_t>(sh);
        } else if (-sh >= 64) {
            sticky = true;
            continue;
        } else {
            const std::uint64_t mask = (std::uint64_t(1) << static_cast<unsigned>(-sh)) - 1u;
            if (mi & mask) sticky = true;
            w = wide_t(mi >> static_cast<unsigned>(-sh));
        }
        if (v > 0.0) pos = pos + w;
        else sub = sub + w;
    }
    if (sub > pos) return false;
    out = C::from_wide(neg, pos - sub, sstore_t(unit), sticky);
    return out.is_finite() && !out.is_zero();
}

template <class C>
bool in_range(const C& x, long long lo, long long hi) noexcept {
    if constexpr (small_c<C>) {
        using ops = small_ops_of<C>;
        if (!x.is_finite() || x.is_zero()) return false;
        const std::uint64_t b = x.get_bits().get_lowest_bits();
        const long long e = static_cast<long long>(b >> ops::mb) - ops::bias;
        return e >= lo && e <= hi;
    }
    if (!x.is_finite() || x.is_zero()) return false;
    const long long e = as_ll(x.frexp().second);
    return e >= lo && e <= hi;
}

inline dd make_w(const double* p, int n, dd*) noexcept {
    dd r(p[0]);
    for (int i = 1; i < n; ++i) r = r + dd(p[i]);
    return r;
}

inline qd make_w(const double* p, int n, qd*) noexcept {
    qd r(p[0]);
    for (int i = 1; i < n; ++i) r = r + qd(p[i]);
    return r;
}

inline int parts_of(const dd& w, double* out) noexcept {
    out[0] = w.x0;
    out[1] = w.x1;
    return 2;
}

inline int parts_of(const qd& w, double* out) noexcept {
    for (int i = 0; i < 4; ++i) out[i] = w.x[i];
    return 4;
}

template <class W, class C>
bool finish_w(const W& w, C& out) noexcept {
    const double a = std::fabs(w.lead());
    if (!(a > 0x1p-960 && a < 0x1p1000)) return false;
    double p[4];
    const int n = parts_of(w, p);
    return from_parts(p, n, out);
}

template <class F>
bool unary(fop o, const F& x, F& out) noexcept {
    if constexpr (!enabled<F>) {
        (void)o;
        (void)x;
        (void)out;
        return false;
    } else {
        if (!worth_it(level<F>::value, o)) return false;
        using C = multiprecision::floatmp<F::math_bits, F::mantissa_bits, multiprecision::sign::is_signed>;
        const C& xc = x.as_signed_core();
        C rc;
        if constexpr (level<F>::value == 1) {
            if (!xc.is_finite() || xc.is_zero()) return false;
            double p[4];
            to_parts(xc, p);
            double r = 0.0;
            if (!unary_double(o, p[0], r)) return false;
            const double a = std::fabs(r);
            if (!(a > 0x1p-120 && a < 0x1p126)) return false;
            if (!from_parts(&r, 1, rc)) return false;
        } else {
            using W = typename std::conditional<level<F>::value == 2, dd, qd>::type;
            if (!in_range(xc, -900, 900)) return false;
            double p[4];
            const int n = to_parts(xc, p);
            W r;
            if (!unary_w(o, make_w(p, n, static_cast<W*>(nullptr)), r)) return false;
            if (!finish_w(r, rc)) return false;
        }
        if (!std::is_same<F, C>::value && rc.is_negative()) return false;
        out = F(rc);
        return true;
    }
}

template <class F>
bool binary(fop2 o, const F& a, const F& b, F& out) noexcept {
    if constexpr (!enabled<F>) {
        (void)o;
        (void)a;
        (void)b;
        (void)out;
        return false;
    } else {
        if (!worth_it(level<F>::value, o)) return false;
        using C = multiprecision::floatmp<F::math_bits, F::mantissa_bits, multiprecision::sign::is_signed>;
        const C& ac = a.as_signed_core();
        const C& bc = b.as_signed_core();
        C rc;
        if constexpr (level<F>::value == 1) {
            if (!ac.is_finite() || !bc.is_finite()) return false;
            double pa[4] = { 0.0, 0.0, 0.0, 0.0 }, pb[4] = { 0.0, 0.0, 0.0, 0.0 };
            if (!ac.is_zero()) to_parts(ac, pa);
            if (!bc.is_zero()) to_parts(bc, pb);
            double r = 0.0;
            if (!binary_double(o, pa[0], pb[0], r)) return false;
            const double m = std::fabs(r);
            if (!(m > 0x1p-120 && m < 0x1p126)) return false;
            if (!from_parts(&r, 1, rc)) return false;
        } else {
            using W = typename std::conditional<level<F>::value == 2, dd, qd>::type;
            const bool az = ac.is_zero();
            const bool bz = bc.is_zero();
            if (!ac.is_finite() || !bc.is_finite()) return false;
            if (!az && !in_range(ac, -900, 900)) return false;
            if (!bz && !in_range(bc, -900, 900)) return false;
            double pa[4] = { 0.0, 0.0, 0.0, 0.0 }, pb[4] = { 0.0, 0.0, 0.0, 0.0 };
            const int na = az ? 1 : to_parts(ac, pa);
            const int nb = bz ? 1 : to_parts(bc, pb);
            W r;
            if (!binary_w(o, make_w(pa, na, static_cast<W*>(nullptr)), make_w(pb, nb, static_cast<W*>(nullptr)), r)) return false;
            if (!finish_w(r, rc)) return false;
        }
        if (!std::is_same<F, C>::value && rc.is_negative()) return false;
        out = F(rc);
        return true;
    }
}

} // namespace fwfast
} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FAST_FLOAT_DETAIL_HPP
