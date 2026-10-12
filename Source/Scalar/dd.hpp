#ifndef FIZMO_SCALAR_DD_HPP
#define FIZMO_SCALAR_DD_HPP

#include "scalar.hpp"

namespace fizmo {
namespace scalar {
namespace dd {

constexpr double U = detail::fp_consts<double>::u;
constexpr double kTiny = detail::fp_consts<double>::tiny;
constexpr double kHuge = detail::fp_consts<double>::huge;

struct DD {
    double hi = 0.0;
    double lo = 0.0;
    double err = 0.0;
    bool   ok = true;
};

inline double mag(const DD& a) { return std::fabs(a.hi) + std::fabs(a.lo); }

inline void two_sum(double a, double b, double& s, double& e) {
    s = a + b;
    const double bb = s - a;
    e = (a - (s - bb)) + (b - bb);
}

inline bool product_ok(double a, double b, double p) {
    if (!std::isfinite(p) || std::fabs(a) > kHuge || std::fabs(b) > kHuge) return false;
    if (p == 0.0) return a == 0.0 || b == 0.0;
    return std::fabs(p) >= kTiny;
}

inline DD exact(double x) {
    DD r;
    r.hi = x;
    r.ok = std::isfinite(x);
    return r;
}

inline bool is_exact_zero(const DD& a) { return a.ok && a.hi == 0.0 && a.lo == 0.0 && a.err == 0.0; }

inline DD neg(const DD& a) {
    DD r = a;
    r.hi = -a.hi;
    r.lo = -a.lo;
    return r;
}

inline DD add(const DD& a, const DD& b) {
    DD r;
    r.ok = a.ok && b.ok;
    if (!r.ok) return r;
    if (is_exact_zero(b)) return a;
    if (is_exact_zero(a)) return b;
    double s, e;
    two_sum(a.hi, b.hi, s, e);
    const double t1 = e + a.lo;
    const double t2 = t1 + b.lo;
    two_sum(s, t2, r.hi, r.lo);
    r.err = a.err + b.err + 2.0 * U * (std::fabs(t1) + std::fabs(t2)) + 4.0 * std::numeric_limits<double>::denorm_min();
    r.ok = std::isfinite(r.hi) && std::isfinite(r.err);
    return r;
}

inline DD sub(const DD& a, const DD& b) { return add(a, neg(b)); }

inline DD mul(const DD& a, const DD& b) {
    DD r;
    r.ok = a.ok && b.ok;
    if (!r.ok) return r;

    if (is_exact_zero(a) || is_exact_zero(b)) {
        r.hi = (std::signbit(a.hi) != std::signbit(b.hi)) ? -0.0 : 0.0;
        return r;
    }

    const double p = a.hi * b.hi;
    if (!product_ok(a.hi, b.hi, p)) { r.ok = false; return r; }
    const double e = detail::product_error(a.hi, b.hi, p);
    const double t1 = a.hi * b.lo;
    const double t2 = a.lo * b.hi;
    const double t3 = t1 + t2;
    const double t4 = e + t3;
    two_sum(p, t4, r.hi, r.lo);
    r.err = 2.0 * U * (std::fabs(t1) + std::fabs(t2) + std::fabs(t3) + std::fabs(t4)) + 2.0 * std::fabs(a.lo * b.lo)
          + mag(a) * b.err + mag(b) * a.err + a.err * b.err + 8.0 * std::numeric_limits<double>::denorm_min();
    r.ok = std::isfinite(r.hi) && std::isfinite(r.err);
    return r;
}

inline DD div(const DD& a, const DD& b) {
    DD r;
    r.ok = a.ok && b.ok && b.hi != 0.0;
    if (!r.ok) return r;

    if (is_exact_zero(a)) {
        r.hi = (std::signbit(a.hi) != std::signbit(b.hi)) ? -0.0 : 0.0;
        return r;
    }

    const double bmin = std::fabs(b.hi) - std::fabs(b.lo) - b.err;
    if (!(bmin > kTiny)) { r.ok = false; return r; }
    const double q1 = a.hi / b.hi;
    if (!std::isfinite(q1)) { r.ok = false; return r; }
    const DD rem = sub(a, mul(exact(q1), b));
    if (!rem.ok) { r.ok = false; return r; }
    const double q2 = rem.hi / b.hi;
    two_sum(q1, q2, r.hi, r.lo);
    r.err = (rem.err + std::fabs(rem.lo)) / bmin + std::fabs(rem.hi) * (std::fabs(b.lo) + b.err) / (bmin * std::fabs(b.hi)) + 2.0 * U * std::fabs(q2)
          + 4.0 * std::numeric_limits<double>::denorm_min();
    r.err *= 1.0 + 8.0 * U;
    r.ok = std::isfinite(r.hi) && std::isfinite(r.err);
    return r;
}

inline DD sqrt(const DD& a) {
    DD r;
    r.ok = a.ok && a.hi > kTiny && a.err < 0.25 * a.hi;
    if (!r.ok) return r;
    const double y = std::sqrt(a.hi);
    const double p = y * y;
    const double e = detail::product_error(y, y, p);
    const double t = (a.hi - p) - e;
    const double d = t + a.lo;
    const double y2 = d / (2.0 * y);
    two_sum(y, y2, r.hi, r.lo);
    const double dd = std::fabs(d) + a.err + 2.0 * U * (std::fabs(t) + std::fabs(d));
    r.err = (2.0 * U * (std::fabs(t) + std::fabs(d)) + a.err) / (2.0 * y) * 1.5 + 2.0 * U * std::fabs(y2) + dd * dd / (y * y * y);
    r.ok = std::isfinite(r.hi) && std::isfinite(r.err);
    return r;
}

template <class T>
bool round_to(const DD& v, T& out) {
    if (!v.ok) return false;

    if (v.hi == 0.0 && v.lo == 0.0 && v.err == 0.0) {
        out = static_cast<T>(v.hi);
        return true;
    }

    const T c = static_cast<T>(v.hi + v.lo);
    if (!std::isfinite(c) || !(std::fabs(c) > detail::fp_consts<T>::tiny)) return false;
    const double res1 = v.hi - static_cast<double>(c);
    const double res = res1 + v.lo;
    const double bound = std::fabs(res) + v.err + 2.0 * U * (std::fabs(res1) + std::fabs(res));
    return bound < 0.5 * static_cast<double>(detail::min_spacing(c)) * (1.0 - 1.0 / 1048576.0) ? (out = c, true) : false;
}

struct Constants {
    double p1, p2, p3, p3t, tail;
    DD     pi_180;
    DD     inv_fact[32];
    double inv_fact_err[32];
};

inline const Constants& constants() {
    static const Constants c = [] {
        Constants k{};
        const BigFloatContext w = context(400);
        const BigFloat half_pi = multiprecision::constants::half_pi(w);
        BigFloat rem = half_pi;
        const auto take = [&](std::size_t bits) {
            const double v = rem.rounded(context(bits)).to_double();
            rem = BigFloat::sub(rem, BigFloat(v), w);
            return v;
        };
        k.p1 = take(33);
        k.p2 = take(33);
        k.p3 = take(33);
        k.p3t = take(53);
        k.tail = 2.0 * std::fabs(rem.to_double()) + 1e-60;

        const BigFloat p180 = BigFloat::div(multiprecision::constants::pi(w), BigFloat(180), w);
        k.pi_180.hi = p180.to_double();
        const BigFloat p180r = BigFloat::sub(p180, BigFloat(k.pi_180.hi), w);
        k.pi_180.lo = p180r.to_double();
        k.pi_180.err = 2.0 * std::fabs(BigFloat::sub(p180r, BigFloat(k.pi_180.lo), w).to_double()) + 1e-60;

        BigFloat f = BigFloat::one();

        for (int n = 0; n < 32; ++n) {
            if (n > 0) f = BigFloat::div(f, BigFloat(n), w);
            DD v;
            v.hi = f.to_double();
            const BigFloat r1 = BigFloat::sub(f, BigFloat(v.hi), w);
            v.lo = r1.to_double();
            v.err = 2.0 * std::fabs(BigFloat::sub(r1, BigFloat(v.lo), w).to_double()) + 1e-300;
            k.inv_fact[n] = v;
        }

        return k;
    }();

    return c;
}

struct CosSin {
    DD   cos;
    DD   sin;
    bool ok = false;
};

inline CosSin reduced_cos_sin(const DD& r) {
    const Constants& k = constants();
    CosSin out;
    const double ar = mag(r) + r.err;
    if (!(ar < 0.8)) return out;
    const DD z = mul(r, r);
    int terms = 1;
    double tail_s = 1.0;
    double tail_c = 1.0;

    for (; terms <= 14; ++terms) {
        tail_s = 1.0;
        tail_c = 1.0;
        for (int n = 1; n <= 2 * terms + 3; ++n) tail_s *= ar / n;
        for (int n = 1; n <= 2 * terms + 2; ++n) tail_c *= ar / n;
        if (tail_c < 7.7e-34 && tail_s < 7.7e-34 * ar) break;
    }

    if (terms > 14) return out;
    const int split = terms < 5 ? terms : 5;
    const double zd = z.hi;
    const double dz = std::fabs(z.lo) + z.err;
    double ts = k.inv_fact[2 * terms + 1].hi;
    double tc = k.inv_fact[2 * terms].hi;
    double sum_s = std::fabs(ts);
    double sum_c = std::fabs(tc);

    for (int j = terms - 1; j >= split; --j) {
        ts = k.inv_fact[2 * j + 1].hi - ts * zd;
        tc = k.inv_fact[2 * j].hi - tc * zd;
        sum_s += std::fabs(ts);
        sum_c += std::fabs(tc);
    }

    DD s = exact(ts);
    DD c = exact(tc);
    s.err = 4.0 * U * sum_s + 2.0 * dz * std::fabs(ts) * (terms - split + 1) + 4.0 * U * k.inv_fact[2 * split + 1].hi;
    c.err = 4.0 * U * sum_c + 2.0 * dz * std::fabs(tc) * (terms - split + 1) + 4.0 * U * k.inv_fact[2 * split].hi;
    if (split == terms) {
        s = k.inv_fact[2 * terms + 1];
        c = k.inv_fact[2 * terms];
    }

    for (int j = split - 1; j >= 0; --j) {
        s = add(mul(s, neg(z)), k.inv_fact[2 * j + 1]);
        c = add(mul(c, neg(z)), k.inv_fact[2 * j]);
    }

    s = mul(s, r);
    s.err += 1.01 * tail_s;
    c.err += 1.01 * tail_c;
    out.cos = c;
    out.sin = s;
    out.ok = s.ok && c.ok;
    return out;
}

inline CosSin cos_sin(const DD& x) {
    const Constants& k = constants();
    CosSin out;
    if (!x.ok) return out;

    if (x.hi == 0.0 && x.lo == 0.0 && x.err == 0.0) {
        out.cos = exact(1.0);
        out.sin = exact(x.hi);
        out.ok = true;
        return out;
    }

    const double kq = std::nearbyint(x.hi * 0.63661977236758134308);
    if (!(std::fabs(kq) < 1048576.0)) return out;
    const double t = x.hi - kq * k.p1;
    DD r;
    r.hi = t;
    r.lo = x.lo;
    r.err = x.err;
    double s, e;
    two_sum(r.hi, r.lo, s, e);
    r.hi = s;
    r.lo = e;
    r = sub(r, exact(kq * k.p2));
    r = sub(r, exact(kq * k.p3));
    const double pt = kq * k.p3t;
    DD ptd;
    ptd.hi = pt;
    ptd.lo = kq == 0.0 ? 0.0 : detail::product_error(kq, k.p3t, pt);
    if (kq != 0.0 && !product_ok(kq, k.p3t, pt)) ptd.err = std::fabs(pt) * 4.0 * U;
    r = sub(r, ptd);
    r.err += std::fabs(kq) * k.tail;

    const CosSin red = reduced_cos_sin(r);
    if (!red.ok) return out;
    const long long q = ((static_cast<long long>(kq) % 4) + 4) % 4;

    switch (q) {
        case 0: out.cos = red.cos; out.sin = red.sin; break;
        case 1: out.cos = neg(red.sin); out.sin = red.cos; break;
        case 2: out.cos = neg(red.cos); out.sin = neg(red.sin); break;
        default: out.cos = red.sin; out.sin = neg(red.cos); break;
    }

    out.ok = true;
    return out;
}

inline CosSin cos_sin(double angle, bool degrees) {
    CosSin out;
    if (!std::isfinite(angle)) return out;
    if (!degrees) return cos_sin(exact(angle));

    const double d = std::fmod(angle, 360.0);

    if (d == std::nearbyint(d) && std::fmod(d, 30.0) == 0.0) {
        const long long m = ((static_cast<long long>(d) % 360) + 360) % 360;
        const double h = 0.5;
        const DD r3 = mul(exact(0.5), sqrt(exact(3.0)));
        const DD zero = exact(m == 0 ? (angle == 0.0 ? angle : 0.0) : 0.0);
        out.ok = r3.ok;

        switch (m) {
            case 0:   out.cos = exact(1.0); out.sin = zero; break;
            case 30:  out.cos = r3; out.sin = exact(h); break;
            case 60:  out.cos = exact(h); out.sin = r3; break;
            case 90:  out.cos = exact(0.0); out.sin = exact(1.0); break;
            case 120: out.cos = exact(-h); out.sin = r3; break;
            case 150: out.cos = neg(r3); out.sin = exact(h); break;
            case 180: out.cos = exact(-1.0); out.sin = exact(0.0); break;
            case 210: out.cos = neg(r3); out.sin = exact(-h); break;
            case 240: out.cos = exact(-h); out.sin = neg(r3); break;
            case 270: out.cos = exact(0.0); out.sin = exact(-1.0); break;
            case 300: out.cos = exact(h); out.sin = neg(r3); break;
            default:  out.cos = r3; out.sin = exact(-h); break;
        }

        return out;
    }

    return cos_sin(mul(exact(d), constants().pi_180));
}

template <class T>
struct eligible : std::integral_constant<bool, std::is_same<T, float>::value || std::is_same<T, double>::value> {};

template <class T>
bool exact_input(const T& x) { return std::isfinite(x) && (x == T(0) || std::fabs(x) >= detail::fp_consts<double>::tiny); }

template <class T>
bool half_angle(const T& angle, double& out) {
    if (!std::isfinite(angle)) return false;
    out = static_cast<double>(angle) * 0.5;
    return angle == T(0) || std::fabs(out) >= std::numeric_limits<double>::min();
}

template <class T>
bool fast_cos_sin(const T& angle, bool degrees, T& c, T& s) {
    if (!std::isfinite(angle)) return false;
    const CosSin cs = cos_sin(static_cast<double>(angle), degrees);
    return cs.ok && round_to(cs.cos, c) && round_to(cs.sin, s);
}

template <class T>
bool fast_scaled_cos_sin(const T& magnitude, const T& angle, bool degrees, T& x, T& y) {
    if (!exact_input(magnitude) || !std::isfinite(angle)) return false;
    const CosSin cs = cos_sin(static_cast<double>(angle), degrees);
    if (!cs.ok) return false;
    const DD m = exact(static_cast<double>(magnitude));
    return round_to(mul(m, cs.cos), x) && round_to(mul(m, cs.sin), y);
}

template <class T>
bool fast_axis_angle(const T& ax, const T& ay, const T& az, const T& angle, bool degrees, std::array<T, 4>& out) {
    double h;
    if (!exact_input(ax) || !exact_input(ay) || !exact_input(az) || !half_angle(angle, h)) return false;
    const CosSin cs = cos_sin(h, degrees);
    if (!cs.ok) return false;
    const DD a[3] = { exact(static_cast<double>(ax)), exact(static_cast<double>(ay)), exact(static_cast<double>(az)) };
    const DD len = sqrt(add(add(mul(a[0], a[0]), mul(a[1], a[1])), mul(a[2], a[2])));
    if (!len.ok) return false;
    const DD k = div(cs.sin, len);
    return round_to(cs.cos, out[0]) && round_to(mul(a[0], k), out[1]) && round_to(mul(a[1], k), out[2]) && round_to(mul(a[2], k), out[3]);
}

template <class T>
bool fast_euler(const T& roll, const T& pitch, const T& yaw, bool degrees, std::array<T, 4>& out) {
    double hr, hp, hy;
    if (!half_angle(roll, hr) || !half_angle(pitch, hp) || !half_angle(yaw, hy)) return false;
    const CosSin a = cos_sin(hr, degrees);
    const CosSin b = cos_sin(hp, degrees);
    const CosSin d = cos_sin(hy, degrees);
    if (!a.ok || !b.ok || !d.ok) return false;
    const auto m3 = [](const DD& u, const DD& v, const DD& t) { return mul(mul(u, v), t); };
    return round_to(add(m3(a.cos, b.cos, d.cos), m3(a.sin, b.sin, d.sin)), out[0])
        && round_to(sub(m3(a.sin, b.cos, d.cos), m3(a.cos, b.sin, d.sin)), out[1])
        && round_to(add(m3(a.cos, b.sin, d.cos), m3(a.sin, b.cos, d.sin)), out[2])
        && round_to(sub(m3(a.cos, b.cos, d.sin), m3(a.sin, b.sin, d.cos)), out[3]);
}

template <class T>
bool fast_axis_rotation(const T& ax, const T& ay, const T& az, const T& angle, bool degrees, std::array<T, 9>& out) {
    if (!exact_input(ax) || !exact_input(ay) || !exact_input(az) || !std::isfinite(angle)) return false;
    const CosSin cs = cos_sin(static_cast<double>(angle), degrees);
    if (!cs.ok) return false;
    const DD a[3] = { exact(static_cast<double>(ax)), exact(static_cast<double>(ay)), exact(static_cast<double>(az)) };
    const DD l2 = add(add(mul(a[0], a[0]), mul(a[1], a[1])), mul(a[2], a[2]));
    const DD len = sqrt(l2);
    const DD ic = sub(exact(1.0), cs.cos);
    if (!len.ok || !ic.ok) return false;
    const DD sx = div(mul(a[0], cs.sin), len);
    const DD sy = div(mul(a[1], cs.sin), len);
    const DD sz = div(mul(a[2], cs.sin), len);

    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t k = 0; k < 3; ++k) {
            DD e = mul(div(mul(a[r], a[k]), l2), ic);
            if (r == k) e = add(e, cs.cos);
            if (r == 0 && k == 1) e = sub(e, sz);
            if (r == 0 && k == 2) e = add(e, sy);
            if (r == 1 && k == 0) e = add(e, sz);
            if (r == 1 && k == 2) e = sub(e, sx);
            if (r == 2 && k == 0) e = sub(e, sy);
            if (r == 2 && k == 1) e = add(e, sx);
            if (!round_to(e, out[r * 3 + k])) return false;
        }
    }

    return true;
}

} // namespace dd
} // namespace scalar
} // namespace fizmo

#endif // FIZMO_SCALAR_DD_HPP
