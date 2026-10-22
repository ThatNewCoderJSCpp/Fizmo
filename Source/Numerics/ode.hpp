#ifndef FIZMO_NUMERICS_ODE_HPP
#define FIZMO_NUMERICS_ODE_HPP

#include "dense.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace numerics {

template <class T>
struct OdeScalar {
    static double to_double(const T& v) { return static_cast<double>(v); }
    static T from_double(double v) { return static_cast<T>(v); }
};

template <class T>
using OdeSystem = std::function<void(const T& t, const std::vector<T>& y, std::vector<T>& dydt)>;

template <class T>
using OdeEventFunction = std::function<T(const T& t, const std::vector<T>& y)>;

template <class T>
struct OdeEvent {
    OdeEventFunction<T> function;
    int direction = 0;
    bool terminal = false;
};

template <class T>
struct OdeEventHit {
    std::size_t event = 0;
    T t = T(0);
    std::vector<T> y;
};

enum class OdeMethod {
    Euler,
    Midpoint,
    Heun,
    Rk4,
    BackwardEuler,
    Trapezoid,
    Bdf2,
    ImplicitMidpoint
};

template <class T>
struct OdeOptions {
    T relative_tolerance = T(1e-8);
    T absolute_tolerance = T(1e-10);
    T initial_step = T(0);
    T min_step = T(0);
    T max_step = T(0);
    std::size_t max_steps = 1000000;
    std::vector<OdeEvent<T>> events;
    std::function<bool()> cancelled;
};

template <class T>
struct OdeSolution {
    std::vector<T> t;
    std::vector<std::vector<T>> y;
    std::vector<std::vector<T>> dydt;
    std::vector<OdeEventHit<T>> events;
    bool ok = true;
    bool stopped_by_event = false;
    std::string message;
    std::size_t steps = 0;
    std::size_t rejected = 0;
    std::size_t evaluations = 0;

    std::size_t size() const { return t.size(); }
    bool empty() const { return t.empty(); }
    const std::vector<T>& back() const { return y.back(); }

    std::vector<T> at(const T& time) const {
        if (t.empty()) return {};
        const bool forward = t.size() < 2 || t.back() >= t.front();
        auto before = [&](const T& a, const T& b) { return forward ? a < b : a > b; };
        if (!before(t.front(), time)) return y.front();
        if (!before(time, t.back())) return y.back();
        std::size_t lo = 0, hi = t.size() - 1;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            if (before(time, t[mid])) hi = mid;
            else lo = mid;
        }
        return interpolate(lo, time);
    }

    std::vector<T> interpolate(std::size_t i, const T& time) const {
        const T h = t[i + 1] - t[i];
        if (h == T(0)) return y[i];
        const T s = (time - t[i]) / h;
        const T s2 = s * s;
        const T s3 = s2 * s;
        const T h00 = T(2) * s3 - T(3) * s2 + T(1);
        const T h10 = s3 - T(2) * s2 + s;
        const T h01 = T(-2) * s3 + T(3) * s2;
        const T h11 = s3 - s2;
        std::vector<T> out(y[i].size());
        const bool have = dydt.size() == y.size();
        for (std::size_t k = 0; k < out.size(); ++k) {
            if (have) out[k] = h00 * y[i][k] + h10 * h * dydt[i][k] + h01 * y[i + 1][k] + h11 * h * dydt[i + 1][k];
            else out[k] = (T(1) - s) * y[i][k] + s * y[i + 1][k];
        }
        return out;
    }

    std::vector<T> component(std::size_t k) const {
        std::vector<T> out;
        out.reserve(y.size());
        for (const auto& v : y) out.push_back(v[k]);
        return out;
    }
};

namespace odedetail {

template <class T>
T magnitude(const T& v) {
    return v < T(0) ? -v : v;
}

template <class T>
void axpy(std::vector<T>& out, const std::vector<T>& y, const T& h, const std::vector<T>& k) {
    out.resize(y.size());
    for (std::size_t i = 0; i < y.size(); ++i) out[i] = y[i] + h * k[i];
}

template <class T>
bool finite(const std::vector<T>& v) {
    for (const T& a : v) {
        const double d = OdeScalar<T>::to_double(a);
        if (!std::isfinite(d)) return false;
    }
    return true;
}

template <class T>
bool event_crossed(int direction, const T& a, const T& b) {
    if (direction >= 0 && a < T(0) && b >= T(0)) return true;
    if (direction <= 0 && a > T(0) && b <= T(0)) return true;
    return false;
}

template <class T>
bool check_events(const OdeOptions<T>& opt, OdeSolution<T>& sol, std::vector<T>& previous_g, const T& t1, const std::vector<T>& y1) {
    if (opt.events.empty()) return false;
    const std::size_t last = sol.t.size() - 1;
    bool stop = false;
    T stop_at = t1;
    std::vector<T> stop_y;
    for (std::size_t e = 0; e < opt.events.size(); ++e) {
        const T gb = opt.events[e].function(t1, y1);
        const T ga = previous_g[e];
        previous_g[e] = gb;
        if (!event_crossed(opt.events[e].direction, ga, gb)) continue;
        T a = sol.t[last - 1];
        T b = sol.t[last];
        T fa = ga;
        T fb = gb;
        T root = b;
        std::vector<T> yr = y1;
        for (int it = 0; it < 80; ++it) {
            T m = (a * fb - b * fa) / (fb - fa);
            if (!(magnitude(fb - fa) > T(0)) || !((m > a && m < b) || (m < a && m > b))) m = (a + b) / T(2);
            yr = sol.interpolate(last - 1, m);
            const T fm = opt.events[e].function(m, yr);
            root = m;
            if (fm == T(0) || magnitude(b - a) <= T(1e-14) * (T(1) + magnitude(b))) break;
            if ((fa < T(0)) == (fm < T(0))) {
                a = m;
                fa = fm;
            } else {
                b = m;
                fb = fm;
            }
        }
        sol.events.push_back({ e, root, yr });
        if (opt.events[e].terminal) {
            const bool earlier = sol.t[last] >= sol.t[last - 1] ? root < stop_at : root > stop_at;
            if (!stop || earlier) {
                stop = true;
                stop_at = root;
                stop_y = yr;
            }
        }
    }
    if (stop) {
        sol.t.back() = stop_at;
        sol.y.back() = stop_y;
        sol.stopped_by_event = true;
    }
    return stop;
}

template <class T>
bool implicit_stage(const OdeSystem<T>& f, const T& t, const std::vector<T>& base, const T& c, std::vector<T>& y, std::size_t& evaluations) {
    VectorFunction<T> g = [&](const std::vector<T>& x, std::vector<T>& out) {
        std::vector<T> d;
        f(t, x, d);
        ++evaluations;
        out.resize(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) out[i] = x[i] - base[i] - c * d[i];
    };
    const NewtonResult<T> r = newton_dense<T>(g, y, nullptr, T(1e-12), 30);
    return r.converged || r.residual < T(1e-8);
}

} // namespace odedetail

template <class T>
void rk4_step(const OdeSystem<T>& f, const T& t, const std::vector<T>& y, const T& h, std::vector<T>& out) {
    std::vector<T> k1, k2, k3, k4, tmp;
    f(t, y, k1);
    odedetail::axpy(tmp, y, h / T(2), k1);
    f(t + h / T(2), tmp, k2);
    odedetail::axpy(tmp, y, h / T(2), k2);
    f(t + h / T(2), tmp, k3);
    odedetail::axpy(tmp, y, h, k3);
    f(t + h, tmp, k4);
    out.resize(y.size());
    for (std::size_t i = 0; i < y.size(); ++i) out[i] = y[i] + h / T(6) * (k1[i] + T(2) * k2[i] + T(2) * k3[i] + k4[i]);
}

template <class T>
OdeSolution<T> solve_fixed(const OdeSystem<T>& f, const T& t0, const std::vector<T>& y0, const T& t1, std::size_t steps, OdeMethod method = OdeMethod::Rk4, const OdeOptions<T>& options = OdeOptions<T>()) {
    OdeSolution<T> sol;
    steps = std::max<std::size_t>(1, steps);
    const T h = (t1 - t0) / T(static_cast<double>(steps));
    std::vector<T> y = y0;
    std::vector<T> d;
    f(t0, y, d);
    ++sol.evaluations;
    sol.t.push_back(t0);
    sol.y.push_back(y);
    sol.dydt.push_back(d);
    std::vector<T> g(options.events.size());
    for (std::size_t e = 0; e < options.events.size(); ++e) g[e] = options.events[e].function(t0, y);
    std::vector<T> prev = y;
    std::vector<T> next, k1, k2, tmp;
    for (std::size_t s = 0; s < steps; ++s) {
        if (options.cancelled && options.cancelled()) {
            sol.ok = false;
            sol.message = "cancelled";
            break;
        }
        const T t = t0 + h * T(static_cast<double>(s));
        switch (method) {
        case OdeMethod::Euler:
            odedetail::axpy(next, y, h, d);
            break;
        case OdeMethod::Midpoint:
            odedetail::axpy(tmp, y, h / T(2), d);
            f(t + h / T(2), tmp, k1);
            ++sol.evaluations;
            odedetail::axpy(next, y, h, k1);
            break;
        case OdeMethod::Heun:
            odedetail::axpy(tmp, y, h, d);
            f(t + h, tmp, k1);
            ++sol.evaluations;
            next.resize(y.size());
            for (std::size_t i = 0; i < y.size(); ++i) next[i] = y[i] + h / T(2) * (d[i] + k1[i]);
            break;
        case OdeMethod::Rk4:
            rk4_step(f, t, y, h, next);
            sol.evaluations += 4;
            break;
        case OdeMethod::BackwardEuler:
            odedetail::axpy(next, y, h, d);
            if (!odedetail::implicit_stage(f, t + h, y, h, next, sol.evaluations)) sol.ok = false;
            break;
        case OdeMethod::Trapezoid: {
            std::vector<T> base;
            odedetail::axpy(base, y, h / T(2), d);
            odedetail::axpy(next, y, h, d);
            if (!odedetail::implicit_stage(f, t + h, base, h / T(2), next, sol.evaluations)) sol.ok = false;
            break;
        }
        case OdeMethod::ImplicitMidpoint: {
            std::vector<T> mid;
            odedetail::axpy(mid, y, h / T(2), d);
            VectorFunction<T> gm = [&](const std::vector<T>& x, std::vector<T>& out) {
                std::vector<T> dd;
                f(t + h / T(2), x, dd);
                ++sol.evaluations;
                out.resize(x.size());
                for (std::size_t i = 0; i < x.size(); ++i) out[i] = x[i] - y[i] - h / T(2) * dd[i];
            };
            if (!newton_dense<T>(gm, mid, nullptr, T(1e-12), 30).converged) sol.ok = false;
            next.resize(y.size());
            for (std::size_t i = 0; i < y.size(); ++i) next[i] = T(2) * mid[i] - y[i];
            break;
        }
        case OdeMethod::Bdf2: {
            if (s == 0) {
                odedetail::axpy(next, y, h, d);
                if (!odedetail::implicit_stage(f, t + h, y, h, next, sol.evaluations)) sol.ok = false;
            } else {
                std::vector<T> base(y.size());
                for (std::size_t i = 0; i < y.size(); ++i) base[i] = (T(4) * y[i] - prev[i]) / T(3);
                odedetail::axpy(next, y, h, d);
                if (!odedetail::implicit_stage(f, t + h, base, T(2) * h / T(3), next, sol.evaluations)) sol.ok = false;
            }
            break;
        }
        }
        if (!sol.ok) {
            sol.message = "the implicit solve did not converge";
            break;
        }
        prev = y;
        y = next;
        f(t + h, y, d);
        ++sol.evaluations;
        sol.t.push_back(s + 1 == steps ? t1 : t + h);
        sol.y.push_back(y);
        sol.dydt.push_back(d);
        ++sol.steps;
        if (!odedetail::finite(y)) {
            sol.ok = false;
            sol.message = "the solution stopped being finite";
            break;
        }
        if (odedetail::check_events(options, sol, g, sol.t.back(), y)) break;
    }
    return sol;
}

template <class T>
OdeSolution<T> solve_adaptive(const OdeSystem<T>& f, const T& t0, const std::vector<T>& y0, const T& t1, const OdeOptions<T>& opt = OdeOptions<T>()) {
    using S = OdeScalar<T>;
    static const double c2 = 1.0 / 5, c3 = 3.0 / 10, c4 = 4.0 / 5, c5 = 8.0 / 9;
    static const double a21 = 1.0 / 5;
    static const double a31 = 3.0 / 40, a32 = 9.0 / 40;
    static const double a41 = 44.0 / 45, a42 = -56.0 / 15, a43 = 32.0 / 9;
    static const double a51 = 19372.0 / 6561, a52 = -25360.0 / 2187, a53 = 64448.0 / 6561, a54 = -212.0 / 729;
    static const double a61 = 9017.0 / 3168, a62 = -355.0 / 33, a63 = 46732.0 / 5247, a64 = 49.0 / 176, a65 = -5103.0 / 18656;
    static const double b1 = 35.0 / 384, b3 = 500.0 / 1113, b4 = 125.0 / 192, b5 = -2187.0 / 6784, b6 = 11.0 / 84;
    static const double e1 = 71.0 / 57600, e3 = -71.0 / 16695, e4 = 71.0 / 1920, e5 = -17253.0 / 339200, e6 = 22.0 / 525, e7 = -1.0 / 40;
    OdeSolution<T> sol;
    const std::size_t n = y0.size();
    const double span = S::to_double(t1 - t0);
    const double dir = span < 0.0 ? -1.0 : 1.0;
    std::vector<T> y = y0;
    std::vector<T> k1, k2, k3, k4, k5, k6, k7, tmp(n), ynew(n);
    f(t0, y, k1);
    ++sol.evaluations;
    sol.t.push_back(t0);
    sol.y.push_back(y);
    sol.dydt.push_back(k1);
    if (span == 0.0) return sol;
    std::vector<T> g(opt.events.size());
    for (std::size_t e = 0; e < opt.events.size(); ++e) g[e] = opt.events[e].function(t0, y);
    const double rtol = S::to_double(opt.relative_tolerance);
    const double atol = S::to_double(opt.absolute_tolerance);
    double hmax = S::to_double(opt.max_step);
    if (!(hmax > 0.0)) hmax = std::fabs(span);
    const double hmin = std::max(S::to_double(opt.min_step), 1e-14 * std::fabs(span));
    double h = S::to_double(opt.initial_step);
    if (!(h > 0.0)) {
        double y0n = 0.0, f0n = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double sc = atol + rtol * std::fabs(S::to_double(y[i]));
            y0n = std::max(y0n, std::fabs(S::to_double(y[i])) / sc);
            f0n = std::max(f0n, std::fabs(S::to_double(k1[i])) / sc);
        }
        h = (y0n < 1e-5 || f0n < 1e-5) ? 1e-6 : 0.01 * y0n / f0n;
        h = std::min(h, std::fabs(span) * 0.01);
    }
    h = std::min(h, hmax);
    T t = t0;
    double err_prev = 1e-4;
    while (sol.steps + sol.rejected < opt.max_steps) {
        if (opt.cancelled && opt.cancelled()) {
            sol.ok = false;
            sol.message = "cancelled";
            return sol;
        }
        const double remaining = S::to_double(t1 - t) * dir;
        if (remaining <= 0.0) break;
        bool last = false;
        if (h >= remaining) {
            h = remaining;
            last = true;
        }
        const T hh = S::from_double(h * dir);
        auto stage = [&](std::vector<T>& out, std::initializer_list<std::pair<double, const std::vector<T>*>> terms) {
            for (std::size_t i = 0; i < n; ++i) {
                T acc = y[i];
                for (const auto& tm : terms) acc += hh * S::from_double(tm.first) * (*tm.second)[i];
                out[i] = acc;
            }
        };
        stage(tmp, { { a21, &k1 } });
        f(t + hh * S::from_double(c2), tmp, k2);
        stage(tmp, { { a31, &k1 }, { a32, &k2 } });
        f(t + hh * S::from_double(c3), tmp, k3);
        stage(tmp, { { a41, &k1 }, { a42, &k2 }, { a43, &k3 } });
        f(t + hh * S::from_double(c4), tmp, k4);
        stage(tmp, { { a51, &k1 }, { a52, &k2 }, { a53, &k3 }, { a54, &k4 } });
        f(t + hh * S::from_double(c5), tmp, k5);
        stage(tmp, { { a61, &k1 }, { a62, &k2 }, { a63, &k3 }, { a64, &k4 }, { a65, &k5 } });
        f(t + hh, tmp, k6);
        stage(ynew, { { b1, &k1 }, { b3, &k3 }, { b4, &k4 }, { b5, &k5 }, { b6, &k6 } });
        f(t + hh, ynew, k7);
        sol.evaluations += 6;
        double err = 0.0;
        bool bad = false;
        for (std::size_t i = 0; i < n; ++i) {
            const T ei = hh * (S::from_double(e1) * k1[i] + S::from_double(e3) * k3[i] + S::from_double(e4) * k4[i] + S::from_double(e5) * k5[i] + S::from_double(e6) * k6[i] + S::from_double(e7) * k7[i]);
            const double yi = std::max(std::fabs(S::to_double(y[i])), std::fabs(S::to_double(ynew[i])));
            const double sc = atol + rtol * yi;
            const double r = std::fabs(S::to_double(ei)) / sc;
            if (!std::isfinite(r)) bad = true;
            err = std::max(err, r);
        }
        if (bad) err = 1e10;
        if (err <= 1.0) {
            t = last ? t1 : t + hh;
            y = ynew;
            k1 = k7;
            sol.t.push_back(t);
            sol.y.push_back(y);
            sol.dydt.push_back(k1);
            ++sol.steps;
            if (odedetail::check_events(opt, sol, g, t, y)) return sol;
            if (last) return sol;
            double fac = err > 0.0 ? 0.9 * std::pow(err, -0.7 / 5.0) * std::pow(err_prev, 0.4 / 5.0) : 5.0;
            fac = std::clamp(fac, 0.2, 5.0);
            h = std::min(h * fac, hmax);
            err_prev = std::max(err, 1e-4);
        } else {
            ++sol.rejected;
            const double fac = std::max(0.2, 0.9 * std::pow(err, -0.2));
            h *= fac;
            if (h < hmin) {
                sol.ok = false;
                sol.message = "the step size became too small (the equation may be singular or very stiff here)";
                return sol;
            }
        }
    }
    if (S::to_double(t1 - t) * dir > 0.0) {
        sol.ok = false;
        sol.message = "too many steps";
    }
    return sol;
}

template <class T>
OdeSolution<T> solve_stiff(const OdeSystem<T>& f, const T& t0, const std::vector<T>& y0, const T& t1, const OdeOptions<T>& opt = OdeOptions<T>()) {
    using S = OdeScalar<T>;
    OdeSolution<T> sol;
    const std::size_t n = y0.size();
    const double span = S::to_double(t1 - t0);
    const double dir = span < 0.0 ? -1.0 : 1.0;
    std::vector<T> y = y0;
    std::vector<T> d;
    f(t0, y, d);
    ++sol.evaluations;
    sol.t.push_back(t0);
    sol.y.push_back(y);
    sol.dydt.push_back(d);
    if (span == 0.0) return sol;
    std::vector<T> g(opt.events.size());
    for (std::size_t e = 0; e < opt.events.size(); ++e) g[e] = opt.events[e].function(t0, y);
    const double rtol = S::to_double(opt.relative_tolerance);
    const double atol = S::to_double(opt.absolute_tolerance);
    double hmax = S::to_double(opt.max_step);
    if (!(hmax > 0.0)) hmax = std::fabs(span);
    double h = S::to_double(opt.initial_step);
    if (!(h > 0.0)) h = std::fabs(span) * 1e-3;
    const double hmin = std::max(S::to_double(opt.min_step), 1e-14 * std::fabs(span));
    T t = t0;
    auto trapezoid = [&](const T& from, const std::vector<T>& ya, const std::vector<T>& da, const T& step, std::vector<T>& out) {
        std::vector<T> base;
        odedetail::axpy(base, ya, step / T(2), da);
        odedetail::axpy(out, ya, step, da);
        return odedetail::implicit_stage(f, from + step, base, step / T(2), out, sol.evaluations);
    };
    while (sol.steps + sol.rejected < opt.max_steps) {
        if (opt.cancelled && opt.cancelled()) {
            sol.ok = false;
            sol.message = "cancelled";
            return sol;
        }
        const double remaining = S::to_double(t1 - t) * dir;
        if (remaining <= 0.0) break;
        bool last = false;
        if (h >= remaining) {
            h = remaining;
            last = true;
        }
        const T hh = S::from_double(h * dir);
        std::vector<T> big, half, half_d, two;
        const bool ok1 = trapezoid(t, y, d, hh, big);
        bool ok2 = trapezoid(t, y, d, hh / T(2), half);
        if (ok2) {
            f(t + hh / T(2), half, half_d);
            ++sol.evaluations;
            ok2 = trapezoid(t + hh / T(2), half, half_d, hh / T(2), two);
        }
        double err = 1e10;
        if (ok1 && ok2) {
            err = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                const double sc = atol + rtol * std::max(std::fabs(S::to_double(y[i])), std::fabs(S::to_double(two[i])));
                err = std::max(err, std::fabs(S::to_double(two[i] - big[i])) / 3.0 / sc);
            }
            if (!std::isfinite(err)) err = 1e10;
        }
        if (err <= 1.0) {
            t = last ? t1 : t + hh;
            y = two;
            f(t, y, d);
            ++sol.evaluations;
            sol.t.push_back(t);
            sol.y.push_back(y);
            sol.dydt.push_back(d);
            ++sol.steps;
            if (odedetail::check_events(opt, sol, g, t, y)) return sol;
            if (last) return sol;
            const double fac = err > 0.0 ? std::clamp(0.9 * std::pow(err, -1.0 / 3.0), 0.2, 4.0) : 4.0;
            h = std::min(h * fac, hmax);
        } else {
            ++sol.rejected;
            h *= err >= 1e9 ? 0.25 : std::max(0.2, 0.9 * std::pow(err, -1.0 / 3.0));
            if (h < hmin) {
                sol.ok = false;
                sol.message = "the step size became too small";
                return sol;
            }
        }
    }
    if (S::to_double(t1 - t) * dir > 0.0) {
        sol.ok = false;
        sol.message = "too many steps";
    }
    return sol;
}

template <class T>
using AccelerationFunction = std::function<void(const T& t, const std::vector<T>& position, const std::vector<T>& velocity, std::vector<T>& acceleration)>;

enum class SymplecticMethod { Euler, Verlet, Yoshida4 };

template <class T>
OdeSolution<T> solve_symplectic(const AccelerationFunction<T>& accel, const T& t0, const std::vector<T>& q0, const std::vector<T>& v0, const T& t1, std::size_t steps, SymplecticMethod method = SymplecticMethod::Verlet, const OdeOptions<T>& options = OdeOptions<T>()) {
    OdeSolution<T> sol;
    steps = std::max<std::size_t>(1, steps);
    const std::size_t n = q0.size();
    const T h = (t1 - t0) / T(static_cast<double>(steps));
    std::vector<T> q = q0, v = v0, a;
    auto pack = [&](const std::vector<T>& qq, const std::vector<T>& vv, const std::vector<T>& aa, const T& tt) {
        std::vector<T> y(2 * n), dy(2 * n);
        for (std::size_t i = 0; i < n; ++i) {
            y[i] = qq[i];
            y[n + i] = vv[i];
            dy[i] = vv[i];
            dy[n + i] = aa[i];
        }
        sol.t.push_back(tt);
        sol.y.push_back(y);
        sol.dydt.push_back(dy);
    };
    accel(t0, q, v, a);
    ++sol.evaluations;
    pack(q, v, a, t0);
    std::vector<T> g(options.events.size());
    for (std::size_t e = 0; e < options.events.size(); ++e) g[e] = options.events[e].function(t0, sol.y.back());
    auto drift = [&](const T& c) {
        for (std::size_t i = 0; i < n; ++i) q[i] += c * h * v[i];
    };
    auto kick = [&](const T& tt, const T& c) {
        accel(tt, q, v, a);
        ++sol.evaluations;
        for (std::size_t i = 0; i < n; ++i) v[i] += c * h * a[i];
    };
    for (std::size_t s = 0; s < steps; ++s) {
        if (options.cancelled && options.cancelled()) {
            sol.ok = false;
            sol.message = "cancelled";
            break;
        }
        const T t = t0 + h * T(static_cast<double>(s));
        if (method == SymplecticMethod::Euler) {
            kick(t, T(1));
            drift(T(1));
        } else if (method == SymplecticMethod::Verlet) {
            for (std::size_t i = 0; i < n; ++i) v[i] += h / T(2) * a[i];
            drift(T(1));
            kick(t + h, T(1) / T(2));
        } else {
            const double cbrt2 = std::cbrt(2.0);
            const T w1 = OdeScalar<T>::from_double(1.0 / (2.0 - cbrt2));
            const T w0 = OdeScalar<T>::from_double(-cbrt2 / (2.0 - cbrt2));
            const T c1 = w1 / T(2), c2 = (w0 + w1) / T(2);
            drift(c1);
            kick(t + c1 * h, w1);
            drift(c2);
            kick(t + (c1 + c2) * h, w0);
            drift(c2);
            kick(t + (c1 + c2 + c2) * h, w1);
            drift(c1);
        }
        const T tn = s + 1 == steps ? t1 : t + h;
        accel(tn, q, v, a);
        ++sol.evaluations;
        pack(q, v, a, tn);
        ++sol.steps;
        if (!odedetail::finite(q) || !odedetail::finite(v)) {
            sol.ok = false;
            sol.message = "the solution stopped being finite";
            break;
        }
        if (odedetail::check_events(options, sol, g, tn, sol.y.back())) break;
    }
    return sol;
}

template <class T>
using ImplicitOdeFunction = std::function<T(const T& x, const std::vector<T>& derivatives)>;

template <class T>
OdeSystem<T> implicit_order_system(const ImplicitOdeFunction<T>& F, std::size_t order) {
    auto guess = std::make_shared<T>(T(0));
    return [F, order, guess](const T& x, const std::vector<T>& y, std::vector<T>& dy) {
        dy.resize(order);
        for (std::size_t i = 0; i + 1 < order; ++i) dy[i] = y[i + 1];
        std::vector<T> all(order + 1);
        for (std::size_t i = 0; i < order; ++i) all[i] = y[i];
        T top = *guess;
        auto value = [&](const T& v) {
            all[order] = v;
            return F(x, all);
        };
        T fv = value(top);
        for (int it = 0; it < 60; ++it) {
            const T scale = odedetail::magnitude(top);
            const T hstep = T(1e-7) * (T(1) + scale);
            const T slope = (value(top + hstep) - fv) / hstep;
            if (slope == T(0)) break;
            const T step = fv / slope;
            top = top - step;
            fv = value(top);
            if (odedetail::magnitude(step) <= T(1e-13) * (T(1) + odedetail::magnitude(top))) break;
        }
        *guess = top;
        dy[order - 1] = top;
    };
}

} // namespace numerics
} // namespace fizmo

#endif // FIZMO_NUMERICS_ODE_HPP
