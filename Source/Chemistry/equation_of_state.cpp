#include "equation_of_state.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;
const double SQRT2 = 1.4142135623730950488;

template <typename F>
double brent(F f, double a, double b, double fa, double fb, double tol = 1e-14, int max_iter = 200) {
    if (fa == 0.0) return a;
    if (fb == 0.0) return b;
    if ((fa > 0.0) == (fb > 0.0)) return NO_VALUE;
    double c = a, fc = fa, d = b - a, e = d;
    for (int i = 0; i < max_iter; ++i) {
        if ((fb > 0.0) == (fc > 0.0)) {
            c = a;
            fc = fa;
            d = e = b - a;
        }
        if (std::fabs(fc) < std::fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        const double tol1 = 2.0 * std::numeric_limits<double>::epsilon() * std::fabs(b) + 0.5 * tol * std::fabs(b);
        const double m = 0.5 * (c - b);
        if (std::fabs(m) <= tol1 || fb == 0.0) return b;
        if (std::fabs(e) >= tol1 && std::fabs(fa) > std::fabs(fb)) {
            double p, q, r;
            const double s = fb / fa;
            if (a == c) {
                p = 2.0 * m * s;
                q = 1.0 - s;
            } else {
                q = fa / fc;
                r = fb / fc;
                p = s * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0.0) q = -q;
            else p = -p;
            if (2.0 * p < std::min(3.0 * m * q - std::fabs(tol1 * q), std::fabs(e * q))) {
                e = d;
                d = p / q;
            } else {
                d = m;
                e = d;
            }
        } else {
            d = m;
            e = d;
        }
        a = b;
        fa = fb;
        b += std::fabs(d) > tol1 ? d : (m > 0.0 ? tol1 : -tol1);
        fb = f(b);
    }
    return b;
}

template <typename F>
double brent(F f, double a, double b, double tol = 1e-14) {
    return brent(f, a, b, f(a), f(b), tol);
}

template <typename F>
double simpson(F f, double a, double b, double fa, double fm, double fb, double whole, double tol, int depth) {
    const double m = 0.5 * (a + b);
    const double lm = 0.5 * (a + m);
    const double rm = 0.5 * (m + b);
    const double flm = f(lm);
    const double frm = f(rm);
    const double left = (m - a) / 6.0 * (fa + 4.0 * flm + fm);
    const double right = (b - m) / 6.0 * (fm + 4.0 * frm + fb);
    const double diff = left + right - whole;
    if (depth <= 0 || std::fabs(diff) <= 15.0 * tol) return left + right + diff / 15.0;
    return simpson(f, a, m, fa, flm, fm, left, tol / 2.0, depth - 1) + simpson(f, m, b, fm, frm, fb, right, tol / 2.0, depth - 1);
}

template <typename F>
double integrate(F f, double a, double b, double tol) {
    const double fa = f(a);
    const double fb = f(b);
    const double fm = f(0.5 * (a + b));
    const double whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
    return simpson(f, a, b, fa, fm, fb, whole, tol, 48);
}

std::vector<double> real_cubic_roots(double c2, double c1, double c0) {
    const double q = (3.0 * c1 - c2 * c2) / 9.0;
    const double r = (9.0 * c2 * c1 - 27.0 * c0 - 2.0 * c2 * c2 * c2) / 54.0;
    const double disc = q * q * q + r * r;
    std::vector<double> roots;
    if (disc > 0.0) {
        const double sq = std::sqrt(disc);
        const double s = std::cbrt(r + sq);
        const double t = std::cbrt(r - sq);
        roots.push_back(s + t - c2 / 3.0);
    } else if (q == 0.0) {
        roots.push_back(-c2 / 3.0);
    } else {
        const double theta = std::acos(std::max(-1.0, std::min(1.0, r / std::sqrt(-q * q * q))));
        const double m = 2.0 * std::sqrt(-q);
        roots.push_back(m * std::cos(theta / 3.0) - c2 / 3.0);
        roots.push_back(m * std::cos((theta + 2.0 * PI) / 3.0) - c2 / 3.0);
        roots.push_back(m * std::cos((theta + 4.0 * PI) / 3.0) - c2 / 3.0);
    }
    auto polish = [&](double z) {
        for (int i = 0; i < 4; ++i) {
            const double f = ((z + c2) * z + c1) * z + c0;
            const double df = (3.0 * z + 2.0 * c2) * z + c1;
            if (df == 0.0) break;
            z -= f / df;
        }
        return z;
    };
    double big = roots.front();
    for (double z : roots) {
        if (std::fabs(z) > std::fabs(big)) big = z;
    }
    big = polish(big);
    std::vector<double> out{ big };
    if (big != 0.0) {
        const double pq = c2 + big;
        const double qq = -c0 / big;
        const double dq = pq * pq - 4.0 * qq;
        if (dq >= 0.0) {
            const double sq = std::sqrt(dq);
            const double t = -0.5 * (pq + (pq >= 0.0 ? sq : -sq));
            if (t != 0.0) {
                out.push_back(polish(t));
                out.push_back(polish(qq / t));
            } else {
                out.push_back(0.0);
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

double lower_volume_bound(const EquationOfState& eos, double t) {
    const double vmin = eos.minimum_volume(t);
    return vmin > 0.0 ? vmin : 1e-8;
}

} // namespace

CriticalConstants CriticalConstants::of(const Substance& s) {
    if (!s.has_critical_point()) throw std::invalid_argument(std::string("fizmo::chemistry: no critical constants for ") + s.name);
    return CriticalConstants{ s.critical_temperature, s.critical_pressure, s.critical_volume, value_or(s.acentric_factor, 0.0) };
}

double EquationOfState::minimum_volume(double) const {
    return 0.0;
}

double EquationOfState::dp_dv(double t, double v) const {
    const double vmin = minimum_volume(t);
    const double h = 1e-6 * std::max(v - vmin, 1e-3 * v);
    return (pressure(t, v + h) - pressure(t, v - h)) / (2.0 * h);
}

double EquationOfState::dp_dt(double t, double v) const {
    const double h = 1e-5 * t;
    return (pressure(t + h, v) - pressure(t - h, v)) / (2.0 * h);
}

double EquationOfState::d2p_dv2(double t, double v) const {
    const double vmin = minimum_volume(t);
    const double h = 1e-4 * std::max(v - vmin, 1e-3 * v);
    return (pressure(t, v + h) - 2.0 * pressure(t, v) + pressure(t, v - h)) / (h * h);
}

std::vector<double> EquationOfState::molar_volumes(double t, double p) const {
    std::vector<double> roots;
    if (!(t > 0.0) || !(p > 0.0)) return roots;
    const double vmin = minimum_volume(t);
    const double base = lower_volume_bound(*this, t);
    const double ideal = R * t / p;
    const double umin = vmin > 0.0 ? base * 1e-10 : base;
    const double umax = std::max(100.0 * ideal, 1e4 * base);
    const int n = 1200;
    const double ratio = std::pow(umax / umin, 1.0 / n);
    auto f = [&](double v) { return pressure(t, v) - p; };
    double u_prev = umin;
    double v_prev = vmin + u_prev;
    double f_prev = f(v_prev);
    for (int i = 1; i <= n; ++i) {
        const double u = umin * std::pow(ratio, i);
        const double v = vmin + u;
        const double fv = f(v);
        if (std::isfinite(fv) && std::isfinite(f_prev) && ((fv > 0.0) != (f_prev > 0.0) || fv == 0.0)) {
            const double root = brent(f, v_prev, v, f_prev, fv);
            if (has_value(root) && dp_dv(t, root) < 0.0) roots.push_back(root);
        }
        v_prev = v;
        f_prev = fv;
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end(), [](double a, double b) { return std::fabs(a - b) <= 1e-12 * std::max(a, b); }), roots.end());
    return roots;
}

double EquationOfState::residual_helmholtz(double t, double v) const {
    if (!has_ideal_gas_limit()) return NO_VALUE;
    const double rho = 1.0 / v;
    auto integrand = [&](double r) {
        if (r <= 0.0) return R * t * second_virial(t);
        return (pressure(t, 1.0 / r) - R * t * r) / (r * r);
    };
    return integrate(integrand, 0.0, rho, 1e-11 * R * t);
}

double EquationOfState::residual_helmholtz_dt(double t, double v) const {
    const double h = 1e-4 * t;
    return (residual_helmholtz(t + h, v) - residual_helmholtz(t - h, v)) / (2.0 * h);
}

double EquationOfState::residual_helmholtz_dt2(double t, double v) const {
    const double h = 1e-3 * t;
    return (residual_helmholtz_dt(t + h, v) - residual_helmholtz_dt(t - h, v)) / (2.0 * h);
}

CriticalConstants EquationOfState::critical_point() const {
    auto best_slope = [&](double t, double* v_at) {
        const double base = lower_volume_bound(*this, t);
        const double vmin = minimum_volume(t);
        double best = -1e300, best_v = NO_VALUE;
        const int n = 400;
        const double umin = vmin > 0.0 ? base * 1e-3 : base;
        const double umax = 200.0 * base;
        const double ratio = std::pow(umax / umin, 1.0 / n);
        int best_i = 0;
        for (int i = 0; i <= n; ++i) {
            const double v = vmin + umin * std::pow(ratio, i);
            const double s = dp_dv(t, v);
            if (std::isfinite(s) && s > best) {
                best = s;
                best_v = v;
                best_i = i;
            }
        }
        double lo = vmin + umin * std::pow(ratio, std::max(0, best_i - 1));
        double hi = vmin + umin * std::pow(ratio, std::min(n, best_i + 1));
        const double g = 0.6180339887498949;
        for (int k = 0; k < 80; ++k) {
            const double a = hi - g * (hi - lo);
            const double b = lo + g * (hi - lo);
            if (dp_dv(t, a) > dp_dv(t, b)) hi = b;
            else lo = a;
        }
        best_v = 0.5 * (lo + hi);
        best = dp_dv(t, best_v);
        if (v_at) *v_at = best_v;
        return best;
    };
    double t_lo = NO_VALUE, t_hi = NO_VALUE;
    double prev_t = 1.0;
    double prev = best_slope(prev_t, nullptr);
    for (int i = 1; i <= 60; ++i) {
        const double t = std::pow(10.0, i * 5.0 / 60.0);
        const double s = best_slope(t, nullptr);
        if (prev > 0.0 && s < 0.0) {
            t_lo = prev_t;
            t_hi = t;
            break;
        }
        prev_t = t;
        prev = s;
    }
    if (!has_value(t_lo)) return CriticalConstants{};
    for (int k = 0; k < 100; ++k) {
        const double mid = std::sqrt(t_lo * t_hi);
        if (best_slope(mid, nullptr) > 0.0) t_lo = mid;
        else t_hi = mid;
        if (t_hi / t_lo - 1.0 < 1e-13) break;
    }
    const double tc = std::sqrt(t_lo * t_hi);
    double vc = NO_VALUE;
    best_slope(tc, &vc);
    return CriticalConstants{ tc, pressure(tc, vc), vc, 0.0 };
}

double EquationOfState::second_virial(double t) const {
    if (!has_ideal_gas_limit()) return NO_VALUE;
    const double vref = std::max(lower_volume_bound(*this, t), 1e-5);
    const double rho = 1e-5 / vref;
    auto f = [&](double r) { return (compressibility_at(t, 1.0 / r) - 1.0) / r; };
    return 2.0 * f(rho) - f(2.0 * rho);
}

double EquationOfState::third_virial(double t) const {
    if (!has_ideal_gas_limit()) return NO_VALUE;
    const double vref = std::max(lower_volume_bound(*this, t), 1e-5);
    const double rho = 2e-3 / vref;
    const double b = second_virial(t);
    auto f = [&](double r) { return ((compressibility_at(t, 1.0 / r) - 1.0) / r - b) / r; };
    return 2.0 * f(rho) - f(2.0 * rho);
}

double EquationOfState::compressibility_at(double t, double v) const {
    return pressure(t, v) * v / (R * t);
}

double EquationOfState::stable_root(double t, double, const std::vector<double>& roots) const {
    if (roots.empty()) return NO_VALUE;
    if (roots.size() == 1 || !has_ideal_gas_limit()) return roots.front();
    double best = roots.front();
    double best_g = ln_fugacity_coefficient_at(t, best);
    for (double v : roots) {
        const double g = ln_fugacity_coefficient_at(t, v);
        if (g < best_g) {
            best_g = g;
            best = v;
        }
    }
    return best;
}

double EquationOfState::molar_volume(double t, double p, Root root) const {
    const std::vector<double> roots = molar_volumes(t, p);
    if (roots.empty()) return NO_VALUE;
    if (root == Root::Liquid) return roots.front();
    if (root == Root::Vapor) return roots.back();
    return stable_root(t, p, roots);
}

double EquationOfState::compressibility(double t, double p, Root root) const {
    return p * molar_volume(t, p, root) / (R * t);
}

double EquationOfState::temperature(double p, double v) const {
    auto f = [&](double t) { return pressure(t, v) - p; };
    double lo = 1e-3, hi = 10.0;
    double flo = f(lo), fhi = f(hi);
    int guard = 0;
    while ((flo > 0.0) == (fhi > 0.0) && guard++ < 40) {
        lo = hi;
        flo = fhi;
        hi *= 4.0;
        fhi = f(hi);
    }
    return brent(f, lo, hi, flo, fhi);
}

double EquationOfState::density(double t, double p, double molar_mass_g, Root root) const {
    return molar_mass_g / 1000.0 / molar_volume(t, p, root);
}

double EquationOfState::volume(double moles_, double t, double p, Root root) const {
    return moles_ * molar_volume(t, p, root);
}

double EquationOfState::moles(double p, double total_volume, double t, Root root) const {
    return total_volume / molar_volume(t, p, root);
}

double EquationOfState::pressure_of(double moles_, double t, double total_volume) const {
    return pressure(t, total_volume / moles_);
}

double EquationOfState::ln_fugacity_coefficient_at(double t, double v) const {
    const double z = compressibility_at(t, v);
    return residual_helmholtz(t, v) / (R * t) + z - 1.0 - std::log(z);
}

double EquationOfState::ln_fugacity_coefficient(double t, double p, Root root) const {
    const double v = molar_volume(t, p, root);
    return has_value(v) ? ln_fugacity_coefficient_at(t, v) : NO_VALUE;
}

double EquationOfState::fugacity_coefficient(double t, double p, Root root) const {
    return std::exp(ln_fugacity_coefficient(t, p, root));
}

double EquationOfState::fugacity(double t, double p, Root root) const {
    return p * fugacity_coefficient(t, p, root);
}

double EquationOfState::residual_internal_energy_at(double t, double v) const {
    return residual_helmholtz(t, v) - t * residual_helmholtz_dt(t, v);
}

double EquationOfState::residual_enthalpy_at(double t, double v) const {
    return residual_internal_energy_at(t, v) + pressure(t, v) * v - R * t;
}

double EquationOfState::residual_gibbs_at(double t, double v) const {
    return R * t * ln_fugacity_coefficient_at(t, v);
}

double EquationOfState::residual_entropy_at(double t, double v) const {
    return (residual_enthalpy_at(t, v) - residual_gibbs_at(t, v)) / t;
}

double EquationOfState::residual_cv_at(double t, double v) const {
    return -t * residual_helmholtz_dt2(t, v);
}

double EquationOfState::residual_cp_at(double t, double v) const {
    const double pt = dp_dt(t, v);
    return residual_cv_at(t, v) - t * pt * pt / dp_dv(t, v) - R;
}

double EquationOfState::residual_enthalpy(double t, double p, Root root) const {
    const double v = molar_volume(t, p, root);
    return has_value(v) ? residual_enthalpy_at(t, v) : NO_VALUE;
}

double EquationOfState::residual_entropy(double t, double p, Root root) const {
    const double v = molar_volume(t, p, root);
    return has_value(v) ? residual_entropy_at(t, v) : NO_VALUE;
}

double EquationOfState::residual_gibbs(double t, double p, Root root) const {
    const double v = molar_volume(t, p, root);
    return has_value(v) ? residual_gibbs_at(t, v) : NO_VALUE;
}

double EquationOfState::isothermal_compressibility(double t, double v) const {
    return -1.0 / (v * dp_dv(t, v));
}

double EquationOfState::thermal_expansion(double t, double v) const {
    return -dp_dt(t, v) / (v * dp_dv(t, v));
}

double EquationOfState::bulk_modulus(double t, double v) const {
    return -v * dp_dv(t, v);
}

double EquationOfState::joule_thomson_coefficient(double t, double p, double ideal_cp, Root root) const {
    const double v = molar_volume(t, p, root);
    if (!has_value(v)) return NO_VALUE;
    const double dv_dt = -dp_dt(t, v) / dp_dv(t, v);
    const double cp = ideal_cp + residual_cp_at(t, v);
    return (t * dv_dt - v) / cp;
}

double EquationOfState::heat_capacity_ratio(double t, double p, double ideal_cp, Root root) const {
    const double v = molar_volume(t, p, root);
    if (!has_value(v)) return NO_VALUE;
    const double cp = ideal_cp + residual_cp_at(t, v);
    const double cv = ideal_cp - R + residual_cv_at(t, v);
    return cp / cv;
}

double EquationOfState::speed_of_sound(double t, double p, double ideal_cp, double molar_mass_g, Root root) const {
    const double v = molar_volume(t, p, root);
    if (!has_value(v)) return NO_VALUE;
    const double gamma = heat_capacity_ratio(t, p, ideal_cp, root);
    const double w2 = -v * v * gamma * dp_dv(t, v) / (molar_mass_g / 1000.0);
    return w2 > 0.0 ? std::sqrt(w2) : NO_VALUE;
}

std::pair<double, double> EquationOfState::spinodal_volumes(double t) const {
    const double vmin = minimum_volume(t);
    const double base = lower_volume_bound(*this, t);
    const double umin = vmin > 0.0 ? base * 1e-6 : base;
    const double umax = 1e4 * base + 100.0 * R * t / 1e5;
    const int n = 1500;
    const double ratio = std::pow(umax / umin, 1.0 / n);
    auto g = [&](double v) { return dp_dv(t, v); };
    double liquid = NO_VALUE;
    double v_prev = vmin + umin;
    double g_prev = g(v_prev);
    for (int i = 1; i <= n; ++i) {
        const double v = vmin + umin * std::pow(ratio, i);
        const double gv = g(v);
        if (std::isfinite(gv) && std::isfinite(g_prev) && (gv > 0.0) != (g_prev > 0.0)) {
            const bool minimum = g_prev < 0.0 && gv > 0.0;
            const double r = brent(g, v_prev, v, g_prev, gv, 1e-13);
            if (has_value(r)) {
                if (!has_value(liquid) && minimum) liquid = r;
                else if (has_value(liquid) && !minimum) return { liquid, r };
            }
        }
        v_prev = v;
        g_prev = gv;
    }
    return { NO_VALUE, NO_VALUE };
}

double EquationOfState::saturation_pressure(double t) const {
    if (!has_ideal_gas_limit()) return NO_VALUE;
    const std::pair<double, double> s = spinodal_volumes(t);
    if (!has_value(s.first)) return NO_VALUE;
    const double p_low = pressure(t, s.first);
    const double p_high = pressure(t, s.second);
    if (!(p_high > 0.0)) return NO_VALUE;
    const double v_liquid_max = s.first;
    const double v_vapor_min = s.second;
    auto g = [&](double p) {
        const std::vector<double> roots = molar_volumes(t, p);
        if (roots.size() < 2) return NO_VALUE;
        const double vl = roots.front();
        const double vv = roots.back();
        if (!(vl <= v_liquid_max * (1.0 + 1e-9)) || !(vv >= v_vapor_min * (1.0 - 1e-9))) return NO_VALUE;
        return ln_fugacity_coefficient_at(t, vl) - ln_fugacity_coefficient_at(t, vv);
    };
    double hi = p_high * (1.0 - 1e-9);
    double ghi = g(hi);
    double lo = std::max(p_low * (1.0 + 1e-9), p_high * 1e-3);
    if (p_low > 0.0 && lo < p_low * (1.0 + 1e-9)) lo = p_low * (1.0 + 1e-9);
    double glo = g(lo);
    int guard = 0;
    while (has_value(glo) && glo <= 0.0 && guard++ < 300) {
        const double next = lo / 10.0;
        if (p_low > 0.0 && next <= p_low) break;
        const double gn = g(next);
        if (!has_value(gn)) break;
        lo = next;
        glo = gn;
    }
    guard = 0;
    while (!has_value(glo) && guard++ < 200) {
        lo += (hi - lo) * 0.02;
        glo = g(lo);
    }
    guard = 0;
    while (!has_value(ghi) && guard++ < 200) {
        hi -= (hi - lo) * 0.02;
        ghi = g(hi);
    }
    if (!has_value(glo) || !has_value(ghi) || (glo > 0.0) == (ghi > 0.0)) return NO_VALUE;
    for (int k = 0; k < 200; ++k) {
        const double mid = std::sqrt(lo * hi);
        const double gm = g(mid);
        if (!has_value(gm)) break;
        if ((gm > 0.0) == (glo > 0.0)) {
            lo = mid;
            glo = gm;
        } else {
            hi = mid;
            ghi = gm;
        }
        if (hi / lo - 1.0 < 1e-14) break;
    }
    return std::sqrt(lo * hi);
}

std::pair<double, double> EquationOfState::saturation_volumes(double t) const {
    const double p = saturation_pressure(t);
    if (!has_value(p)) return { NO_VALUE, NO_VALUE };
    const std::vector<double> roots = molar_volumes(t, p);
    if (roots.size() < 2) return { NO_VALUE, NO_VALUE };
    return { roots.front(), roots.back() };
}

double EquationOfState::saturation_temperature(double p) const {
    const CriticalConstants c = critical_point();
    if (!c.known() || p >= c.pressure) return NO_VALUE;
    double hi = c.temperature * (1.0 - 1e-9);
    double lo = c.temperature * 0.2;
    auto f = [&](double t) {
        const double ps = saturation_pressure(t);
        return has_value(ps) ? std::log(ps / p) : NO_VALUE;
    };
    double flo = f(lo);
    while (!has_value(flo) && lo < hi) {
        lo *= 1.1;
        flo = f(lo);
    }
    double fhi = f(hi);
    while (!has_value(fhi) && hi > lo) {
        hi *= 0.9999;
        fhi = f(hi);
    }
    if (!has_value(flo) || !has_value(fhi)) return NO_VALUE;
    if (flo > 0.0) return NO_VALUE;
    if (fhi < 0.0) return NO_VALUE;
    return brent(f, lo, hi, flo, fhi, 1e-12);
}

double EquationOfState::boyle_temperature() const {
    auto f = [&](double t) { return second_virial(t); };
    double lo = 1.0;
    double flo = f(lo);
    for (int i = 1; i <= 50; ++i) {
        const double t = std::pow(10.0, i * 5.0 / 50.0);
        const double ft = f(t);
        if ((flo > 0.0) != (ft > 0.0)) return brent(f, lo, t, flo, ft, 1e-12);
        lo = t;
        flo = ft;
    }
    return NO_VALUE;
}

double EquationOfState::inversion_temperature() const {
    auto f = [&](double t) {
        const double h = 1e-4 * t;
        const double db = (second_virial(t + h) - second_virial(t - h)) / (2.0 * h);
        return t * db - second_virial(t);
    };
    double hi = 1e5;
    double fhi = f(hi);
    for (int i = 49; i >= 0; --i) {
        const double t = std::pow(10.0, i * 5.0 / 50.0);
        const double ft = f(t);
        if ((fhi > 0.0) != (ft > 0.0)) return brent(f, t, hi, ft, fhi, 1e-12);
        hi = t;
        fhi = ft;
    }
    return NO_VALUE;
}

double EquationOfState::compressibility_at_critical() const {
    const CriticalConstants c = critical_point();
    return c.known() && has_value(c.volume) ? c.compressibility() : NO_VALUE;
}

Phase EquationOfState::phase(double t, double p) const {
    if (!has_ideal_gas_limit()) return Phase::Unknown;
    const CriticalConstants c = critical_point();
    if (c.known() && t >= c.temperature) return p >= c.pressure ? Phase::Supercritical : Phase::Gas;
    const double ps = saturation_pressure(t);
    if (!has_value(ps)) return Phase::Gas;
    return p < ps ? Phase::Gas : Phase::Liquid;
}

EosState EquationOfState::state(double t, double p, Root root) const {
    EosState s;
    s.temperature = t;
    s.pressure = p;
    s.molar_volume = molar_volume(t, p, root);
    if (!has_value(s.molar_volume)) return s;
    s.compressibility = p * s.molar_volume / (R * t);
    s.fugacity_coefficient = has_ideal_gas_limit() ? std::exp(ln_fugacity_coefficient_at(t, s.molar_volume)) : NO_VALUE;
    if (root == Root::Liquid) {
        const std::vector<double> roots = molar_volumes(t, p);
        s.phase = roots.size() > 1 ? Phase::Liquid : phase(t, p);
    } else if (root == Root::Vapor) {
        const std::vector<double> roots = molar_volumes(t, p);
        s.phase = roots.size() > 1 ? Phase::Gas : phase(t, p);
    } else {
        s.phase = phase(t, p);
    }
    s.ok = true;
    return s;
}

double EquationOfState::numeric_temperature_derivative(const std::function<double(double)>& f, double t) const {
    const double h = 1e-5 * t;
    return (f(t + h) - f(t - h)) / (2.0 * h);
}

double CubicEquationOfState::da_dt(double t) const {
    const double h = 1e-5 * t;
    return (a(t + h) - a(t - h)) / (2.0 * h);
}

double CubicEquationOfState::d2a_dt2(double t) const {
    const double h = 1e-4 * t;
    return (da_dt(t + h) - da_dt(t - h)) / (2.0 * h);
}

double CubicEquationOfState::pressure(double t, double v) const {
    const double bb = b();
    return R * t / (v - bb) - a(t) / ((v + epsilon() * bb) * (v + sigma() * bb));
}

double CubicEquationOfState::dp_dv(double t, double v) const {
    const double bb = b();
    const double d = (v + epsilon() * bb) * (v + sigma() * bb);
    return -R * t / ((v - bb) * (v - bb)) + a(t) * (2.0 * v + (epsilon() + sigma()) * bb) / (d * d);
}

double CubicEquationOfState::dp_dt(double t, double v) const {
    const double bb = b();
    return R / (v - bb) - da_dt(t) / ((v + epsilon() * bb) * (v + sigma() * bb));
}

std::vector<double> CubicEquationOfState::compressibility_roots(double t, double p) const {
    const double A = reduced_a(t, p);
    const double B = reduced_b(t, p);
    const double e = epsilon();
    const double s = sigma();
    const double c2 = (e + s - 1.0) * B - 1.0;
    const double c1 = A + e * s * B * B - (e + s) * B * (1.0 + B);
    const double c0 = -(A * B + e * s * B * B * (1.0 + B));
    std::vector<double> out;
    for (double z : real_cubic_roots(c2, c1, c0)) {
        if (z > B && std::isfinite(z)) out.push_back(z);
    }
    return out;
}

std::vector<double> CubicEquationOfState::molar_volumes(double t, double p) const {
    std::vector<double> roots;
    if (!(t > 0.0) || !(p > 0.0)) return roots;
    for (double z : compressibility_roots(t, p)) {
        const double v = z * R * t / p;
        if (dp_dv(t, v) < 0.0) roots.push_back(v);
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end(), [](double x, double y) { return std::fabs(x - y) <= 1e-12 * std::max(x, y); }), roots.end());
    return roots;
}

double CubicEquationOfState::attraction_integral(double v) const {
    const double bb = b();
    const double e = epsilon();
    const double s = sigma();
    if (std::fabs(s - e) < 1e-12) return 1.0 / (v + e * bb);
    return std::log((v + s * bb) / (v + e * bb)) / ((s - e) * bb);
}

double CubicEquationOfState::residual_helmholtz(double t, double v) const {
    return -R * t * std::log(1.0 - b() / v) - a(t) * attraction_integral(v);
}

double CubicEquationOfState::residual_helmholtz_dt(double t, double v) const {
    return -R * std::log(1.0 - b() / v) - da_dt(t) * attraction_integral(v);
}

double CubicEquationOfState::residual_helmholtz_dt2(double t, double v) const {
    return -d2a_dt2(t) * attraction_integral(v);
}

double CubicEquationOfState::second_virial(double t) const {
    return b() - a(t) / (R * t);
}

CorrespondingStatesCubic::CorrespondingStatesCubic(const CriticalConstants& c, double omega_a, double omega_b) : m_critical(c) {
    if (!(c.temperature > 0.0) || !(c.pressure > 0.0)) throw std::invalid_argument("fizmo::chemistry: critical temperature and pressure must be positive");
    m_ac = omega_a * R * R * c.temperature * c.temperature / c.pressure;
    m_b = omega_b * R * c.temperature / c.pressure;
}

VanDerWaals::VanDerWaals(double a_, double b_) : m_a(a_) {
    if (a_ < 0.0 || b_ < 0.0) throw std::invalid_argument("fizmo::chemistry: van der Waals constants must not be negative");
    m_b = b_;
}

VanDerWaals VanDerWaals::from_critical(double tc, double pc) {
    return VanDerWaals(27.0 * R * R * tc * tc / (64.0 * pc), R * tc / (8.0 * pc));
}

CriticalConstants VanDerWaals::critical_point() const {
    if (m_a == 0.0 || m_b == 0.0) return CriticalConstants{};
    return CriticalConstants{ 8.0 * m_a / (27.0 * R * m_b), m_a / (27.0 * m_b * m_b), 3.0 * m_b, 0.0 };
}

Berthelot::Berthelot(double a_, double b_) : m_a(a_) {
    m_b = b_;
}

Berthelot Berthelot::from_critical(double tc, double pc) {
    return Berthelot(27.0 * R * R * tc * tc * tc / (64.0 * pc), R * tc / (8.0 * pc));
}

Berthelot Berthelot::from_substance(const Substance& s) {
    const CriticalConstants c = CriticalConstants::of(s);
    return from_critical(c.temperature, c.pressure);
}

CriticalConstants Berthelot::critical_point() const {
    const double tc = std::sqrt(8.0 * m_a / (27.0 * R * m_b));
    return CriticalConstants{ tc, m_a / (27.0 * tc * m_b * m_b), 3.0 * m_b, 0.0 };
}

Clausius::Clausius(double a_, double b_, double c_) : m_a(a_), m_c(c_) {
    m_b = b_;
    if (m_b <= 0.0) throw std::invalid_argument("fizmo::chemistry: the Clausius b must be positive");
}

Clausius Clausius::from_critical(double tc, double pc, double vc) {
    const double a_ = 27.0 * R * R * tc * tc * tc / (64.0 * pc);
    const double b_ = vc - R * tc / (4.0 * pc);
    const double c_ = 3.0 * R * tc / (8.0 * pc) - vc;
    return Clausius(a_, b_, c_);
}

Clausius Clausius::from_substance(const Substance& s) {
    const CriticalConstants c = CriticalConstants::of(s);
    if (!has_value(c.volume)) throw std::invalid_argument("fizmo::chemistry: the Clausius equation needs a critical volume");
    return from_critical(c.temperature, c.pressure, c.volume);
}

RedlichKwong::RedlichKwong(const CriticalConstants& c) : CorrespondingStatesCubic(c, 0.42748023354034140, 0.08664034996495773) {
    if (!has_value(m_critical.volume)) m_critical.volume = R * c.temperature / (3.0 * c.pressure);
}

double RedlichKwong::alpha(double tr) const {
    return 1.0 / std::sqrt(tr);
}

double RedlichKwong::dalpha_dtr(double tr) const {
    return -0.5 / (tr * std::sqrt(tr));
}

double RedlichKwong::d2alpha_dtr2(double tr) const {
    return 0.75 / (tr * tr * std::sqrt(tr));
}

SoaveRedlichKwong::SoaveRedlichKwong(const CriticalConstants& c) : CorrespondingStatesCubic(c, 0.42748023354034140, 0.08664034996495773) {
    const double w = c.acentric_factor;
    m_m = 0.480 + 1.574 * w - 0.176 * w * w;
    if (!has_value(m_critical.volume)) m_critical.volume = R * c.temperature / (3.0 * c.pressure);
}

double SoaveRedlichKwong::alpha(double tr) const {
    const double g = 1.0 + m_m * (1.0 - std::sqrt(tr));
    return g * g;
}

double SoaveRedlichKwong::dalpha_dtr(double tr) const {
    const double s = std::sqrt(tr);
    return -m_m * (1.0 + m_m * (1.0 - s)) / s;
}

double SoaveRedlichKwong::d2alpha_dtr2(double tr) const {
    return m_m * (1.0 + m_m) / (2.0 * tr * std::sqrt(tr));
}

PengRobinson::PengRobinson(const CriticalConstants& c) : CorrespondingStatesCubic(c, 0.45723552892138218, 0.07779607390388845) {
    const double w = c.acentric_factor;
    if (w <= 0.491) m_kappa = 0.37464 + 1.54226 * w - 0.26992 * w * w;
    else m_kappa = 0.379642 + 1.48503 * w - 0.164423 * w * w + 0.016666 * w * w * w;
    if (!has_value(m_critical.volume)) m_critical.volume = 0.30740130869870 * R * c.temperature / c.pressure;
}

double PengRobinson::epsilon() const {
    return 1.0 - SQRT2;
}

double PengRobinson::sigma() const {
    return 1.0 + SQRT2;
}

double PengRobinson::alpha(double tr) const {
    const double g = 1.0 + m_kappa * (1.0 - std::sqrt(tr));
    return g * g;
}

double PengRobinson::dalpha_dtr(double tr) const {
    const double s = std::sqrt(tr);
    return -m_kappa * (1.0 + m_kappa * (1.0 - s)) / s;
}

double PengRobinson::d2alpha_dtr2(double tr) const {
    return m_kappa * (1.0 + m_kappa) / (2.0 * tr * std::sqrt(tr));
}

PengRobinsonStryjekVera::PengRobinsonStryjekVera(const CriticalConstants& c, double kappa1) : PengRobinson(c), m_kappa1(kappa1) {
    const double w = c.acentric_factor;
    m_kappa0 = 0.378893 + 1.4897153 * w - 0.17131848 * w * w + 0.0196554 * w * w * w;
}

double PengRobinsonStryjekVera::kappa_of(double tr) const noexcept {
    if (tr > 0.7) return m_kappa0;
    return m_kappa0 + m_kappa1 * (1.0 + std::sqrt(tr)) * (0.7 - tr);
}

double PengRobinsonStryjekVera::dkappa_of(double tr) const noexcept {
    if (tr > 0.7) return 0.0;
    const double s = std::sqrt(tr);
    return m_kappa1 * ((0.7 - tr) / (2.0 * s) - (1.0 + s));
}

double PengRobinsonStryjekVera::alpha(double tr) const {
    const double g = 1.0 + kappa_of(tr) * (1.0 - std::sqrt(tr));
    return g * g;
}

double PengRobinsonStryjekVera::dalpha_dtr(double tr) const {
    const double s = std::sqrt(tr);
    const double k = kappa_of(tr);
    const double g = 1.0 + k * (1.0 - s);
    return 2.0 * g * (dkappa_of(tr) * (1.0 - s) - k / (2.0 * s));
}

double PengRobinsonStryjekVera::d2alpha_dtr2(double tr) const {
    const double h = 1e-5 * tr;
    return (dalpha_dtr(tr + h) - dalpha_dtr(tr - h)) / (2.0 * h);
}

PatelTeja::PatelTeja(const CriticalConstants& c, double zeta_c, double f) : m_critical(c) {
    const double w = c.acentric_factor;
    const double zc = has_value(zeta_c) ? zeta_c : 0.329032 - 0.076799 * w + 0.0211947 * w * w;
    m_f = has_value(f) ? f : 0.452413 + 1.30982 * w - 0.295937 * w * w;
    const double omega_c = 1.0 - 3.0 * zc;
    double omega_b = NO_VALUE;
    for (double r : real_cubic_roots(2.0 - 3.0 * zc, 3.0 * zc * zc, -zc * zc * zc)) {
        if (r > 0.0 && (!has_value(omega_b) || r < omega_b)) omega_b = r;
    }
    if (!has_value(omega_b)) throw std::invalid_argument("fizmo::chemistry: no Patel-Teja Omega_b for that zeta_c");
    const double omega_a = 3.0 * zc * zc + 3.0 * (1.0 - 2.0 * zc) * omega_b + omega_b * omega_b + omega_c;
    m_ac = omega_a * R * R * c.temperature * c.temperature / c.pressure;
    m_b = omega_b * R * c.temperature / c.pressure;
    m_c = omega_c * R * c.temperature / c.pressure;
    const double k = m_c / m_b;
    const double disc = std::sqrt((1.0 + k) * (1.0 + k) + 4.0 * k);
    m_sigma = 0.5 * ((1.0 + k) + disc);
    m_epsilon = 0.5 * ((1.0 + k) - disc);
    if (!has_value(m_critical.volume)) m_critical.volume = zc * R * c.temperature / c.pressure;
}

double PatelTeja::a(double t) const {
    const double g = 1.0 + m_f * (1.0 - std::sqrt(t / m_critical.temperature));
    return m_ac * g * g;
}

double PatelTeja::da_dt(double t) const {
    const double tr = t / m_critical.temperature;
    const double s = std::sqrt(tr);
    return m_ac * (-m_f * (1.0 + m_f * (1.0 - s)) / s) / m_critical.temperature;
}

double PatelTeja::d2a_dt2(double t) const {
    const double tr = t / m_critical.temperature;
    return m_ac * (m_f * (1.0 + m_f) / (2.0 * tr * std::sqrt(tr))) / (m_critical.temperature * m_critical.temperature);
}

CubicMixture::CubicMixture(std::vector<std::shared_ptr<const CubicEquationOfState>> components, std::vector<double> mole_fractions, std::vector<std::vector<double>> kij)
    : m_components(std::move(components)), m_x(std::move(mole_fractions)), m_kij(std::move(kij)) {
    const std::size_t n = m_components.size();
    if (n == 0) throw std::invalid_argument("fizmo::chemistry: a mixture needs at least one component");
    if (m_x.size() != n) throw std::invalid_argument("fizmo::chemistry: one mole fraction per component");
    for (const auto& c : m_components) {
        if (!c) throw std::invalid_argument("fizmo::chemistry: null mixture component");
        if (std::fabs(c->epsilon() - m_components.front()->epsilon()) > 1e-12 || std::fabs(c->sigma() - m_components.front()->sigma()) > 1e-12) {
            throw std::invalid_argument("fizmo::chemistry: every component of a cubic mixture must use the same cubic family");
        }
    }
    if (m_kij.empty()) m_kij.assign(n, std::vector<double>(n, 0.0));
    if (m_kij.size() != n) throw std::invalid_argument("fizmo::chemistry: kij must be n by n");
    for (const auto& row : m_kij) {
        if (row.size() != n) throw std::invalid_argument("fizmo::chemistry: kij must be n by n");
    }
    set_mole_fractions(m_x);
}

std::string CubicMixture::name() const {
    return m_components.front()->name() + " mixture";
}

void CubicMixture::set_mole_fractions(std::vector<double> x) {
    if (x.size() != m_components.size()) throw std::invalid_argument("fizmo::chemistry: one mole fraction per component");
    double sum = 0.0;
    for (double v : x) {
        if (v < 0.0) throw std::invalid_argument("fizmo::chemistry: mole fractions must not be negative");
        sum += v;
    }
    if (!(sum > 0.0)) throw std::invalid_argument("fizmo::chemistry: mole fractions must not all be zero");
    for (double& v : x) v /= sum;
    m_x = std::move(x);
    update_b();
}

void CubicMixture::update_b() {
    m_b = 0.0;
    for (std::size_t i = 0; i < m_components.size(); ++i) m_b += m_x[i] * m_components[i]->b();
}

double CubicMixture::interaction(std::size_t i, std::size_t j) const {
    return m_kij.at(i).at(j);
}

void CubicMixture::set_interaction(std::size_t i, std::size_t j, double kij) {
    m_kij.at(i).at(j) = kij;
    m_kij.at(j).at(i) = kij;
}

double CubicMixture::aij(double t, std::size_t i, std::size_t j) const {
    return (1.0 - m_kij[i][j]) * std::sqrt(m_components[i]->a(t) * m_components[j]->a(t));
}

double CubicMixture::a(double t) const {
    double sum = 0.0;
    for (std::size_t i = 0; i < m_components.size(); ++i) {
        for (std::size_t j = 0; j < m_components.size(); ++j) sum += m_x[i] * m_x[j] * aij(t, i, j);
    }
    return sum;
}

double CubicMixture::da_dt(double t) const {
    double sum = 0.0;
    for (std::size_t i = 0; i < m_components.size(); ++i) {
        const double ai = m_components[i]->a(t);
        const double dai = m_components[i]->da_dt(t);
        for (std::size_t j = 0; j < m_components.size(); ++j) {
            const double aj = m_components[j]->a(t);
            const double daj = m_components[j]->da_dt(t);
            const double root = std::sqrt(ai * aj);
            if (root > 0.0) sum += m_x[i] * m_x[j] * (1.0 - m_kij[i][j]) * (dai * aj + ai * daj) / (2.0 * root);
        }
    }
    return sum;
}

double CubicMixture::d2a_dt2(double t) const {
    const double h = 1e-4 * t;
    return (da_dt(t + h) - da_dt(t - h)) / (2.0 * h);
}

CriticalConstants CubicMixture::critical_point() const {
    return EquationOfState::critical_point();
}

CriticalConstants CubicMixture::pseudo_critical() const {
    std::vector<CriticalConstants> cs;
    for (const auto& c : m_components) cs.push_back(c->critical_point());
    return kay_pseudo_critical(cs, m_x);
}

std::vector<double> CubicMixture::ln_fugacity_coefficients(double t, double p, Root root) const {
    const double v = molar_volume(t, p, root);
    std::vector<double> out(m_components.size(), NO_VALUE);
    if (!has_value(v)) return out;
    const double z = p * v / (R * t);
    const double bb = b();
    const double beta = bb * p / (R * t);
    const double am = a(t);
    const double q = am / (bb * R * t);
    const double e = epsilon();
    const double s = sigma();
    const double iz = std::fabs(s - e) < 1e-12 ? beta / (z + e * beta) : std::log((z + s * beta) / (z + e * beta)) / (s - e);
    for (std::size_t i = 0; i < m_components.size(); ++i) {
        double sum = 0.0;
        for (std::size_t j = 0; j < m_components.size(); ++j) sum += m_x[j] * aij(t, i, j);
        const double bi = m_components[i]->b() / bb;
        out[i] = bi * (z - 1.0) - std::log(z - beta) - q * iz * (2.0 * sum / am - bi);
    }
    return out;
}

std::vector<double> CubicMixture::fugacities(double t, double p, Root root) const {
    std::vector<double> ln = ln_fugacity_coefficients(t, p, root);
    for (std::size_t i = 0; i < ln.size(); ++i) ln[i] = m_x[i] * p * std::exp(ln[i]);
    return ln;
}

double ideal_gas_pressure(double moles_, double t, double volume_) noexcept {
    return moles_ * R * t / volume_;
}

double ideal_gas_volume(double moles_, double t, double p) noexcept {
    return moles_ * R * t / p;
}

double ideal_gas_temperature(double moles_, double p, double volume_) noexcept {
    return p * volume_ / (moles_ * R);
}

double ideal_gas_moles(double p, double volume_, double t) noexcept {
    return p * volume_ / (R * t);
}

double ideal_gas_density(double p, double t, double molar_mass_g) noexcept {
    return p * molar_mass_g / 1000.0 / (R * t);
}

double ideal_gas_molar_mass(double density_, double p, double t) noexcept {
    return density_ * R * t / p * 1000.0;
}

double combined_gas_law(double p1, double v1, double t1, double p2, double t2) noexcept {
    return p1 * v1 * t2 / (t1 * p2);
}

double van_der_waals_pressure(double moles_, double t, double volume_, double a_, double b_) noexcept {
    return moles_ * R * t / (volume_ - moles_ * b_) - a_ * moles_ * moles_ / (volume_ * volume_);
}

CriticalConstants kay_pseudo_critical(const std::vector<CriticalConstants>& components, const std::vector<double>& mole_fractions) {
    if (components.size() != mole_fractions.size()) throw std::invalid_argument("fizmo::chemistry: one mole fraction per component");
    CriticalConstants out{ 0.0, 0.0, 0.0, 0.0 };
    double sum = 0.0;
    for (double x : mole_fractions) sum += x;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const double x = mole_fractions[i] / sum;
        out.temperature += x * components[i].temperature;
        out.pressure += x * components[i].pressure;
        out.volume += x * value_or(components[i].volume, NO_VALUE);
        out.acentric_factor += x * components[i].acentric_factor;
    }
    return out;
}

double rackett_volume(const CriticalConstants& c, double t, double zra) {
    const double z = has_value(zra) ? zra : 0.29056 - 0.08775 * c.acentric_factor;
    const double tr = std::min(t / c.temperature, 1.0);
    return R * c.temperature / c.pressure * std::pow(z, 1.0 + std::pow(1.0 - tr, 2.0 / 7.0));
}

double wilson_k_value(const CriticalConstants& c, double t, double p) {
    return c.pressure / p * std::exp(5.373 * (1.0 + c.acentric_factor) * (1.0 - c.temperature / t));
}

double wilson_vapor_pressure(const CriticalConstants& c, double t) {
    return c.pressure * std::exp(5.373 * (1.0 + c.acentric_factor) * (1.0 - c.temperature / t));
}

double lee_kesler_vapor_pressure(const CriticalConstants& c, double t) {
    const double tr = t / c.temperature;
    const double tr6 = std::pow(tr, 6.0);
    const double f0 = 5.92714 - 6.09648 / tr - 1.28862 * std::log(tr) + 0.169347 * tr6;
    const double f1 = 15.2518 - 15.6875 / tr - 13.4721 * std::log(tr) + 0.43577 * tr6;
    return c.pressure * std::exp(f0 + c.acentric_factor * f1);
}

double acentric_factor_from_vapor_pressure(double p_sat_at_tr_07, double pc) {
    return -std::log10(p_sat_at_tr_07 / pc) - 1.0;
}

} // namespace chemistry
} // namespace fizmo
