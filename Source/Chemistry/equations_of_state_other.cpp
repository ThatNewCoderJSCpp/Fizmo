#include "equation_of_state.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

const double R = GAS_CONSTANT;
const double EULER_E = 2.718281828459045235360287;

template <typename F>
double bisect(F f, double lo, double hi, int iterations = 200) {
    double flo = f(lo);
    for (int i = 0; i < iterations; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = f(mid);
        if (fm == 0.0) return mid;
        if ((fm > 0.0) == (flo > 0.0)) {
            lo = mid;
            flo = fm;
        } else {
            hi = mid;
        }
        if (hi - lo <= 1e-15 * std::fabs(hi)) break;
    }
    return 0.5 * (lo + hi);
}

struct LeeKeslerFluid {
    double b1, b2, b3, b4, c1, c2, c3, c4, d1, d2, beta, gamma;

    double z(double tr, double vr) const {
        const double B = b1 - b2 / tr - b3 / (tr * tr) - b4 / (tr * tr * tr);
        const double C = c1 - c2 / tr + c3 / (tr * tr * tr);
        const double D = d1 + d2 / tr;
        const double vr2 = vr * vr;
        return 1.0 + B / vr + C / vr2 + D / (vr2 * vr2 * vr) + c4 / (tr * tr * tr * vr2) * (beta + gamma / vr2) * std::exp(-gamma / vr2);
    }

    double vr(double tr, double pr, bool liquid) const {
        auto f = [&](double v) { return pr * v / tr - z(tr, v); };
        std::vector<double> roots;
        const int n = 2000;
        const double lo = 1e-3, hi = std::max(1e3, 100.0 * tr / pr);
        const double ratio = std::pow(hi / lo, 1.0 / n);
        double v_prev = lo;
        double f_prev = f(v_prev);
        for (int i = 1; i <= n; ++i) {
            const double v = lo * std::pow(ratio, i);
            const double fv = f(v);
            if ((fv > 0.0) != (f_prev > 0.0)) roots.push_back(bisect(f, v_prev, v));
            v_prev = v;
            f_prev = fv;
        }
        if (roots.empty()) return NO_VALUE;
        return liquid ? roots.front() : roots.back();
    }
};

struct LeeKeslerDepartures {
    double z = NO_VALUE;
    double ln_phi = NO_VALUE;
    double enthalpy = NO_VALUE;
    double entropy = NO_VALUE;
};

LeeKeslerDepartures lk_departures(const LeeKeslerFluid& f, double tr, double pr, bool liquid) {
    LeeKeslerDepartures d;
    const double vr = f.vr(tr, pr, liquid);
    if (!has_value(vr)) return d;
    const double tr2 = tr * tr;
    const double tr3 = tr2 * tr;
    const double B = f.b1 - f.b2 / tr - f.b3 / tr2 - f.b4 / tr3;
    const double C = f.c1 - f.c2 / tr + f.c3 / tr3;
    const double D = f.d1 + f.d2 / tr;
    const double vr2 = vr * vr;
    const double vr5 = vr2 * vr2 * vr;
    const double E = f.c4 / (2.0 * tr3 * f.gamma) * (f.beta + 1.0 - (f.beta + 1.0 + f.gamma / vr2) * std::exp(-f.gamma / vr2));
    d.z = pr * vr / tr;
    d.ln_phi = d.z - 1.0 - std::log(d.z) + B / vr + C / (2.0 * vr2) + D / (5.0 * vr5) + E;
    d.enthalpy = tr * (d.z - 1.0 - (f.b2 + 2.0 * f.b3 / tr + 3.0 * f.b4 / tr2) / (tr * vr) - (f.c2 - 3.0 * f.c3 / tr2) / (2.0 * tr * vr2) + f.d2 / (5.0 * tr * vr5) + 3.0 * E);
    d.entropy = std::log(d.z) - (f.b1 + f.b3 / tr2 + 2.0 * f.b4 / tr3) / vr - (f.c1 - 2.0 * f.c3 / tr3) / (2.0 * vr2) - f.d1 / (5.0 * vr5) + 2.0 * E;
    return d;
}

const LeeKeslerFluid LK_SIMPLE{ 0.1181193, 0.265728, 0.154790, 0.030323, 0.0236744, 0.0186984, 0.0, 0.042724, 0.155488e-4, 0.623689e-4, 0.65392, 0.060167 };
const LeeKeslerFluid LK_REFERENCE{ 0.2026579, 0.331511, 0.027655, 0.203488, 0.0313385, 0.0503618, 0.016901, 0.041577, 0.48736e-4, 0.0740336e-4, 1.226, 0.03754 };
const double LK_REFERENCE_OMEGA = 0.3978;

double cs_z(double eta) {
    const double d = 1.0 - eta;
    return (1.0 + eta + eta * eta - eta * eta * eta) / (d * d * d);
}

double cs_dz(double eta) {
    const double h = 1e-7;
    return (cs_z(eta + h) - cs_z(eta - h)) / (2.0 * h);
}

double cs_d2z(double eta) {
    const double h = 1e-5;
    return (cs_z(eta + h) - 2.0 * cs_z(eta) + cs_z(eta - h)) / (h * h);
}

struct BeattieBridgemanPreset {
    const char* name;
    double a0, a, b0, b, c;
};

const BeattieBridgemanPreset BB_PRESETS[] = {
    { "air", 131.8441, 0.01931, 0.04611, -0.001101, 4.34e4 },
    { "argon", 130.7802, 0.02328, 0.03931, 0.0, 5.99e4 },
    { "carbon dioxide", 507.2836, 0.07132, 0.10476, 0.07235, 6.60e5 },
    { "helium", 2.1886, 0.05984, 0.01400, 0.0, 40.0 },
    { "hydrogen", 20.0117, -0.00506, 0.02096, -0.04359, 504.0 },
    { "nitrogen", 136.2315, 0.02617, 0.05046, -0.00691, 4.20e4 },
    { "oxygen", 151.0857, 0.02562, 0.04624, 0.004208, 4.80e4 },
};

struct BwrPreset {
    const char* name;
    double a, a0, b, b0, c, c0, alpha, gamma;
};

const BwrPreset BWR_PRESETS[] = {
    { "n-butane", 190.68, 1021.6, 0.039998, 0.12436, 3.205e7, 1.006e8, 1.101e-3, 0.0340 },
    { "carbon dioxide", 13.86, 277.30, 0.007210, 0.04991, 1.511e6, 1.404e7, 8.470e-5, 0.00539 },
    { "carbon monoxide", 3.71, 135.87, 0.002632, 0.05454, 1.054e5, 8.673e5, 1.350e-4, 0.0060 },
    { "methane", 5.00, 187.91, 0.003380, 0.04260, 2.578e5, 2.286e6, 1.244e-4, 0.0060 },
    { "nitrogen", 2.54, 106.73, 0.002328, 0.04074, 7.379e4, 8.164e5, 1.272e-4, 0.0053 },
};

std::string_view preset_key(std::string_view gas) {
    if (equals_ignore_case(gas, "Ar")) return "argon";
    if (equals_ignore_case(gas, "CO2")) return "carbon dioxide";
    if (equals_ignore_case(gas, "CO")) return "carbon monoxide";
    if (equals_ignore_case(gas, "He")) return "helium";
    if (equals_ignore_case(gas, "H2")) return "hydrogen";
    if (equals_ignore_case(gas, "N2")) return "nitrogen";
    if (equals_ignore_case(gas, "O2")) return "oxygen";
    if (equals_ignore_case(gas, "CH4")) return "methane";
    if (equals_ignore_case(gas, "butane") || equals_ignore_case(gas, "C4H10")) return "n-butane";
    return gas;
}

} // namespace

Dieterici Dieterici::from_critical(double tc, double pc) {
    return Dieterici(4.0 * R * R * tc * tc / (pc * EULER_E * EULER_E), R * tc / (pc * EULER_E * EULER_E));
}

Dieterici Dieterici::from_substance(const Substance& s) {
    const CriticalConstants c = CriticalConstants::of(s);
    return from_critical(c.temperature, c.pressure);
}

double Dieterici::pressure(double t, double v) const {
    return R * t / (v - m_b) * std::exp(-m_a / (R * t * v));
}

CriticalConstants Dieterici::critical_point() const {
    return CriticalConstants{ m_a / (4.0 * R * m_b), m_a / (4.0 * EULER_E * EULER_E * m_b * m_b), 2.0 * m_b, 0.0 };
}

Wohl Wohl::from_critical(double tc, double pc) {
    const double vc = 4.0 * R * tc / (15.0 * pc);
    return Wohl(6.0 * pc * tc * vc * vc, vc / 4.0, 4.0 * pc * tc * tc * vc * vc * vc);
}

Wohl Wohl::from_substance(const Substance& s) {
    const CriticalConstants c = CriticalConstants::of(s);
    return from_critical(c.temperature, c.pressure);
}

double Wohl::pressure(double t, double v) const {
    return R * t / (v - m_b) - m_a / (t * v * (v - m_b)) + m_c / (t * t * v * v * v);
}

CriticalConstants Wohl::critical_point() const {
    const double vc = 4.0 * m_b;
    const double tc = 1.5 * m_c / (m_a * vc);
    return CriticalConstants{ tc, m_a / (6.0 * tc * vc * vc), vc, 0.0 };
}

double Wohl::residual_helmholtz(double t, double v) const {
    return -R * t * std::log(1.0 - m_b / v) - m_a / (t * m_b) * std::log(v / (v - m_b)) + m_c / (2.0 * t * t * v * v);
}

VirialEquation::VirialEquation(Coefficient b, Coefficient c, Coefficient d, Form form) : m_b(std::move(b)), m_c(std::move(c)), m_d(std::move(d)), m_form(form) {
    if (!m_b) throw std::invalid_argument("fizmo::chemistry: the virial equation needs a second virial coefficient");
}

VirialEquation VirialEquation::constant(double b, double c, double d, Form form) {
    return VirialEquation([b](double) { return b; }, [c](double) { return c; }, [d](double) { return d; }, form);
}

double VirialEquation::tsonopoulos_b(const CriticalConstants& c, double t) {
    const double tr = t / c.temperature;
    const double b0 = 0.1445 - 0.330 / tr - 0.1385 / (tr * tr) - 0.0121 / (tr * tr * tr) - 0.000607 / std::pow(tr, 8.0);
    const double b1 = 0.0637 + 0.331 / (tr * tr) - 0.423 / (tr * tr * tr) - 0.008 / std::pow(tr, 8.0);
    return R * c.temperature / c.pressure * (b0 + c.acentric_factor * b1);
}

double VirialEquation::pitzer_abbott_b(const CriticalConstants& c, double t) {
    const double tr = t / c.temperature;
    const double b0 = 0.083 - 0.422 / std::pow(tr, 1.6);
    const double b1 = 0.139 - 0.172 / std::pow(tr, 4.2);
    return R * c.temperature / c.pressure * (b0 + c.acentric_factor * b1);
}

double VirialEquation::orbey_vera_c(const CriticalConstants& c, double t) {
    const double tr = t / c.temperature;
    const double c0 = 0.01407 + 0.02432 / std::pow(tr, 2.8) - 0.00313 / std::pow(tr, 10.5);
    const double c1 = -0.02676 + 0.01770 / std::pow(tr, 2.8) + 0.040 / std::pow(tr, 3.0) - 0.003 / std::pow(tr, 6.0) - 0.00228 / std::pow(tr, 10.5);
    const double scale = R * c.temperature / c.pressure;
    return scale * scale * (c0 + c.acentric_factor * c1);
}

VirialEquation VirialEquation::tsonopoulos(const CriticalConstants& c, bool third, Form form) {
    Coefficient cc = nullptr;
    if (third) cc = [c](double t) { return orbey_vera_c(c, t); };
    return VirialEquation([c](double t) { return tsonopoulos_b(c, t); }, cc, nullptr, form);
}

VirialEquation VirialEquation::pitzer_abbott(const CriticalConstants& c, Form form) {
    return VirialEquation([c](double t) { return pitzer_abbott_b(c, t); }, nullptr, nullptr, form);
}

double VirialEquation::third_virial(double t) const {
    return m_c ? m_c(t) : 0.0;
}

double VirialEquation::pressure(double t, double v) const {
    const double b = m_b(t);
    const double c = m_c ? m_c(t) : 0.0;
    const double d = m_d ? m_d(t) : 0.0;
    if (m_form == Form::Density) return R * t / v * (1.0 + b / v + c / (v * v) + d / (v * v * v));
    const double rt = R * t;
    const double k = (c - b * b) / rt;
    if (std::fabs(k) < 1e-300) return rt / (v - b);
    const double disc = (v - b) * (v - b) - 4.0 * k * rt;
    if (disc < 0.0) return NO_VALUE;
    const double p1 = ((v - b) - std::sqrt(disc)) / (2.0 * k);
    const double p2 = ((v - b) + std::sqrt(disc)) / (2.0 * k);
    const double guess = rt / (v - b);
    return std::fabs(p1 - guess) < std::fabs(p2 - guess) ? p1 : p2;
}

double VirialEquation::minimum_volume(double t) const {
    const double b = std::fabs(m_b(t));
    const double c = m_c ? std::sqrt(std::fabs(m_c(t))) : 0.0;
    return std::max(b, c);
}

std::vector<double> VirialEquation::molar_volumes(double t, double p) const {
    const double rt = R * t;
    const double b = m_b(t);
    if (m_form == Form::Pressure) {
        const double c = m_c ? m_c(t) : 0.0;
        const double z = 1.0 + b * p / rt + (c - b * b) * p * p / (rt * rt);
        if (z <= 0.0) return {};
        return { z * rt / p };
    }
    auto f = [&](double v) { return pressure(t, v) - p; };
    double hi = 10.0 * rt / p + std::fabs(b);
    double fhi = f(hi);
    const double floor = minimum_volume(t);
    for (int i = 0; i < 4000; ++i) {
        const double lo = hi / 1.01;
        if (lo <= floor) break;
        const double flo = f(lo);
        if ((flo > 0.0) != (fhi > 0.0)) return { bisect(f, lo, hi) };
        hi = lo;
        fhi = flo;
    }
    return {};
}

double VirialEquation::residual_helmholtz(double t, double v) const {
    if (m_form == Form::Pressure) return EquationOfState::residual_helmholtz(t, v);
    const double b = m_b(t);
    const double c = m_c ? m_c(t) : 0.0;
    const double d = m_d ? m_d(t) : 0.0;
    return R * t * (b / v + c / (2.0 * v * v) + d / (3.0 * v * v * v));
}

BeattieBridgeman BeattieBridgeman::from_kilopascal_table(double a0, double a, double b0, double b, double c) {
    return BeattieBridgeman(Constants{ a0 * 1e-3, a * 1e-3, b0 * 1e-3, b * 1e-3, c * 1e-3 });
}

bool BeattieBridgeman::has_preset(std::string_view gas) {
    const std::string_view key = preset_key(gas);
    for (const auto& p : BB_PRESETS) {
        if (equals_ignore_case(key, p.name)) return true;
    }
    return false;
}

BeattieBridgeman BeattieBridgeman::preset(std::string_view gas) {
    const std::string_view key = preset_key(gas);
    for (const auto& p : BB_PRESETS) {
        if (equals_ignore_case(key, p.name)) return from_kilopascal_table(p.a0, p.a, p.b0, p.b, p.c);
    }
    throw std::invalid_argument("fizmo::chemistry: no Beattie-Bridgeman constants for that gas");
}

double BeattieBridgeman::pressure(double t, double v) const {
    const double eps = m_k.c / (v * t * t * t);
    const double big_a = m_k.a0 * (1.0 - m_k.a / v);
    const double big_b = m_k.b0 * (1.0 - m_k.b / v);
    return R * t * (1.0 - eps) / (v * v) * (v + big_b) - big_a / (v * v);
}

double BeattieBridgeman::minimum_volume(double) const {
    return 0.25 * m_k.b0;
}

BenedictWebbRubin BenedictWebbRubin::from_kilopascal_table(double a, double a0, double b, double b0, double c, double c0, double alpha, double gamma) {
    return BenedictWebbRubin(Constants{ a * 1e-6, a0 * 1e-3, b * 1e-6, b0 * 1e-3, c * 1e-6, c0 * 1e-3, alpha * 1e-9, gamma * 1e-6 });
}

bool BenedictWebbRubin::has_preset(std::string_view gas) {
    const std::string_view key = preset_key(gas);
    for (const auto& p : BWR_PRESETS) {
        if (equals_ignore_case(key, p.name)) return true;
    }
    return false;
}

BenedictWebbRubin BenedictWebbRubin::preset(std::string_view gas) {
    const std::string_view key = preset_key(gas);
    for (const auto& p : BWR_PRESETS) {
        if (equals_ignore_case(key, p.name)) return from_kilopascal_table(p.a, p.a0, p.b, p.b0, p.c, p.c0, p.alpha, p.gamma);
    }
    throw std::invalid_argument("fizmo::chemistry: no Benedict-Webb-Rubin constants for that gas");
}

double BenedictWebbRubin::pressure(double t, double v) const {
    const double rho = 1.0 / v;
    const double r2 = rho * rho;
    const double r3 = r2 * rho;
    const double rt = R * t;
    return rt * rho + (m_k.b0 * rt - m_k.a0 - m_k.c0 / (t * t)) * r2 + (m_k.b * rt - m_k.a) * r3 + m_k.a * m_k.alpha * r3 * r3 +
           m_k.c * r3 / (t * t) * (1.0 + m_k.gamma * r2) * std::exp(-m_k.gamma * r2);
}

double BenedictWebbRubin::minimum_volume(double) const {
    return 0.25 * m_k.b0;
}

double BenedictWebbRubin::residual_helmholtz(double t, double v) const {
    const double rho = 1.0 / v;
    const double rt = R * t;
    const double u = m_k.gamma * rho * rho;
    double exp_term = 0.0;
    if (m_k.gamma > 0.0) exp_term = m_k.c / (m_k.gamma * t * t) * (1.0 - (1.0 + 0.5 * u) * std::exp(-u));
    return (m_k.b0 * rt - m_k.a0 - m_k.c0 / (t * t)) * rho + 0.5 * (m_k.b * rt - m_k.a) * rho * rho + 0.2 * m_k.a * m_k.alpha * std::pow(rho, 5.0) + exp_term;
}

double LeeKesler::compressibility_simple(double tr, double pr, bool liquid) const {
    const double vr = LK_SIMPLE.vr(tr, pr, liquid);
    return has_value(vr) ? pr * vr / tr : NO_VALUE;
}

double LeeKesler::compressibility_reference(double tr, double pr, bool liquid) const {
    const double vr = LK_REFERENCE.vr(tr, pr, liquid);
    return has_value(vr) ? pr * vr / tr : NO_VALUE;
}

double LeeKesler::generalized_compressibility(double tr, double pr, bool liquid) const {
    const double z0 = compressibility_simple(tr, pr, liquid);
    const double zr = compressibility_reference(tr, pr, liquid);
    return z0 + m_critical.acentric_factor / LK_REFERENCE_OMEGA * (zr - z0);
}

std::vector<double> LeeKesler::molar_volumes(double t, double p) const {
    const double tr = t / m_critical.temperature;
    const double pr = p / m_critical.pressure;
    std::vector<double> out;
    const double zv = generalized_compressibility(tr, pr, false);
    const double zl = generalized_compressibility(tr, pr, true);
    if (has_value(zl) && zl > 0.0) out.push_back(zl * R * t / p);
    if (has_value(zv) && zv > 0.0 && (out.empty() || std::fabs(zv * R * t / p - out.front()) > 1e-9 * out.front())) out.push_back(zv * R * t / p);
    std::sort(out.begin(), out.end());
    return out;
}

bool LeeKesler::use_liquid_root(double t, double p, Root root) const {
    if (root == Root::Liquid) return true;
    if (root == Root::Vapor) return false;
    if (t >= m_critical.temperature) return false;
    return p > saturation_pressure(t);
}

double LeeKesler::combine(double simple, double reference) const {
    return simple + m_critical.acentric_factor / LK_REFERENCE_OMEGA * (reference - simple);
}

double LeeKesler::compressibility(double t, double p, Root root) const {
    const bool liquid = use_liquid_root(t, p, root);
    return generalized_compressibility(t / m_critical.temperature, p / m_critical.pressure, liquid);
}

double LeeKesler::ln_fugacity_coefficient(double t, double p, Root root) const {
    const bool liquid = use_liquid_root(t, p, root);
    const double tr = t / m_critical.temperature, pr = p / m_critical.pressure;
    return combine(lk_departures(LK_SIMPLE, tr, pr, liquid).ln_phi, lk_departures(LK_REFERENCE, tr, pr, liquid).ln_phi);
}

double LeeKesler::residual_enthalpy(double t, double p, Root root) const {
    const bool liquid = use_liquid_root(t, p, root);
    const double tr = t / m_critical.temperature, pr = p / m_critical.pressure;
    return R * m_critical.temperature * combine(lk_departures(LK_SIMPLE, tr, pr, liquid).enthalpy, lk_departures(LK_REFERENCE, tr, pr, liquid).enthalpy);
}

double LeeKesler::residual_entropy(double t, double p, Root root) const {
    const bool liquid = use_liquid_root(t, p, root);
    const double tr = t / m_critical.temperature, pr = p / m_critical.pressure;
    return R * combine(lk_departures(LK_SIMPLE, tr, pr, liquid).entropy, lk_departures(LK_REFERENCE, tr, pr, liquid).entropy);
}

double LeeKesler::residual_gibbs(double t, double p, Root root) const {
    return R * t * ln_fugacity_coefficient(t, p, root);
}

double LeeKesler::saturation_pressure(double t) const {
    if (t >= m_critical.temperature) return NO_VALUE;
    return lee_kesler_vapor_pressure(m_critical, t);
}

double LeeKesler::pressure(double t, double v) const {
    const double tr = t / m_critical.temperature;
    const double vr = v * m_critical.pressure / (R * m_critical.temperature);
    const double z0 = LK_SIMPLE.z(tr, vr);
    const double zr = LK_REFERENCE.z(tr, vr);
    const double z = z0 + m_critical.acentric_factor / LK_REFERENCE_OMEGA * (zr - z0);
    return z * R * t / v;
}

double LeeKesler::minimum_volume(double) const {
    return 0.05 * R * m_critical.temperature / m_critical.pressure;
}

CarnahanStarling CarnahanStarling::from_diameter(double sigma_m) {
    return CarnahanStarling(2.0 / 3.0 * PI * AVOGADRO_CONSTANT * sigma_m * sigma_m * sigma_m);
}

double CarnahanStarling::pressure(double t, double v) const {
    return cs_z(packing_fraction(v)) * R * t / v;
}

double CarnahanStarling::residual_helmholtz(double t, double v) const {
    const double eta = packing_fraction(v);
    const double d = 1.0 - eta;
    return R * t * (4.0 * eta - 3.0 * eta * eta) / (d * d);
}

CarnahanStarlingVanDerWaals CarnahanStarlingVanDerWaals::from_critical(double tc, double pc) {
    auto condition = [](double eta) { return cs_z(eta) - eta * cs_dz(eta) - eta * eta * cs_d2z(eta); };
    const double eta = bisect(condition, 0.01, 0.4);
    const double zc = 0.5 * (cs_z(eta) - eta * cs_dz(eta));
    const double rho = pc / (zc * R * tc);
    const double b = 4.0 * eta / rho;
    const double a = R * tc * (cs_z(eta) + eta * cs_dz(eta)) / (2.0 * rho);
    return CarnahanStarlingVanDerWaals(a, b);
}

CarnahanStarlingVanDerWaals CarnahanStarlingVanDerWaals::from_substance(const Substance& s) {
    const CriticalConstants c = CriticalConstants::of(s);
    return from_critical(c.temperature, c.pressure);
}

double CarnahanStarlingVanDerWaals::pressure(double t, double v) const {
    return CarnahanStarling::pressure(t, v) - m_a / (v * v);
}

double CarnahanStarlingVanDerWaals::residual_helmholtz(double t, double v) const {
    return CarnahanStarling::residual_helmholtz(t, v) - m_a / v;
}

double CarnahanStarlingVanDerWaals::residual_helmholtz_dt(double t, double v) const {
    return CarnahanStarling::residual_helmholtz(t, v) / t;
}

double StiffenedGas::sound_speed(double t, double v, double molar_mass_g) const {
    return std::sqrt(m_gamma * (pressure(t, v) + m_pinf) * v / (molar_mass_g / 1000.0));
}

std::vector<double> CondensedEquationOfState::molar_volumes(double t, double p) const {
    const double v0 = reference_volume(t);
    auto f = [&](double v) { return pressure(t, v) - p; };
    double lo = 1e-3 * v0, hi = 10.0 * v0;
    if ((f(lo) > 0.0) == (f(hi) > 0.0)) return {};
    return { bisect(f, lo, hi) };
}

Tait::Tait(double v0, double b, double c, double p0, double t0, double thermal_expansion)
    : CondensedEquationOfState(v0, (b + p0) / c, 1.0 / c - 1.0, t0, thermal_expansion), m_b(b), m_c(c), m_p0(p0) {}

Tait Tait::from_bulk_modulus(double v0, double k0, double k0_prime, double p0, double t0, double thermal_expansion) {
    const double c = 1.0 / (k0_prime + 1.0);
    return Tait(v0, c * k0 - p0, c, p0, t0, thermal_expansion);
}

double Tait::pressure(double t, double v) const {
    return (m_b + m_p0) * std::exp((1.0 - v / reference_volume(t)) / m_c) - m_b;
}

std::vector<double> Tait::molar_volumes(double t, double p) const {
    const double arg = (m_b + p) / (m_b + m_p0);
    if (arg <= 0.0) return {};
    return { reference_volume(t) * (1.0 - m_c * std::log(arg)) };
}

double Murnaghan::pressure(double t, double v) const {
    return m_k0 / m_k0p * (std::pow(reference_volume(t) / v, m_k0p) - 1.0);
}

std::vector<double> Murnaghan::molar_volumes(double t, double p) const {
    const double arg = 1.0 + m_k0p * p / m_k0;
    if (arg <= 0.0) return {};
    return { reference_volume(t) * std::pow(arg, -1.0 / m_k0p) };
}

double BirchMurnaghan::pressure(double t, double v) const {
    const double x = reference_volume(t) / v;
    const double x13 = std::cbrt(x);
    const double x23 = x13 * x13;
    return 1.5 * m_k0 * (x * x * x13 - x * x23) * (1.0 + 0.75 * (m_k0p - 4.0) * (x23 - 1.0));
}

double Vinet::pressure(double t, double v) const {
    const double f = std::cbrt(v / reference_volume(t));
    return 3.0 * m_k0 * (1.0 - f) / (f * f) * std::exp(1.5 * (m_k0p - 1.0) * (1.0 - f));
}

const char* to_string(EosKind kind) noexcept {
    switch (kind) {
        case EosKind::IdealGas: return "ideal gas";
        case EosKind::VanDerWaals: return "van der Waals";
        case EosKind::Berthelot: return "Berthelot";
        case EosKind::Clausius: return "Clausius";
        case EosKind::Dieterici: return "Dieterici";
        case EosKind::Wohl: return "Wohl";
        case EosKind::RedlichKwong: return "Redlich-Kwong";
        case EosKind::SoaveRedlichKwong: return "Soave-Redlich-Kwong";
        case EosKind::PengRobinson: return "Peng-Robinson";
        case EosKind::PengRobinsonStryjekVera: return "Peng-Robinson-Stryjek-Vera";
        case EosKind::PatelTeja: return "Patel-Teja";
        case EosKind::Virial: return "virial";
        case EosKind::LeeKesler: return "Lee-Kesler";
        case EosKind::CarnahanStarlingVanDerWaals: return "Carnahan-Starling-van der Waals";
        case EosKind::BeattieBridgeman: return "Beattie-Bridgeman";
        default: return "Benedict-Webb-Rubin";
    }
}

std::vector<EosKind> equation_of_state_kinds() {
    return { EosKind::IdealGas, EosKind::VanDerWaals, EosKind::Berthelot, EosKind::Clausius, EosKind::Dieterici, EosKind::Wohl,
             EosKind::RedlichKwong, EosKind::SoaveRedlichKwong, EosKind::PengRobinson, EosKind::PengRobinsonStryjekVera,
             EosKind::PatelTeja, EosKind::Virial, EosKind::LeeKesler, EosKind::CarnahanStarlingVanDerWaals,
             EosKind::BeattieBridgeman, EosKind::BenedictWebbRubin };
}

std::unique_ptr<EquationOfState> make_equation_of_state(EosKind kind, const CriticalConstants& c) {
    switch (kind) {
        case EosKind::IdealGas: return std::make_unique<IdealGas>();
        case EosKind::VanDerWaals: return std::make_unique<VanDerWaals>(VanDerWaals::from_critical(c.temperature, c.pressure));
        case EosKind::Berthelot: return std::make_unique<Berthelot>(Berthelot::from_critical(c.temperature, c.pressure));
        case EosKind::Clausius:
            if (!has_value(c.volume)) throw std::invalid_argument("fizmo::chemistry: the Clausius equation needs a critical volume");
            return std::make_unique<Clausius>(Clausius::from_critical(c.temperature, c.pressure, c.volume));
        case EosKind::Dieterici: return std::make_unique<Dieterici>(Dieterici::from_critical(c.temperature, c.pressure));
        case EosKind::Wohl: return std::make_unique<Wohl>(Wohl::from_critical(c.temperature, c.pressure));
        case EosKind::RedlichKwong: return std::make_unique<RedlichKwong>(c);
        case EosKind::SoaveRedlichKwong: return std::make_unique<SoaveRedlichKwong>(c);
        case EosKind::PengRobinson: return std::make_unique<PengRobinson>(c);
        case EosKind::PengRobinsonStryjekVera: return std::make_unique<PengRobinsonStryjekVera>(c);
        case EosKind::PatelTeja: return std::make_unique<PatelTeja>(c);
        case EosKind::Virial: return std::make_unique<VirialEquation>(VirialEquation::tsonopoulos(c));
        case EosKind::LeeKesler: return std::make_unique<LeeKesler>(c);
        case EosKind::CarnahanStarlingVanDerWaals: return std::make_unique<CarnahanStarlingVanDerWaals>(CarnahanStarlingVanDerWaals::from_critical(c.temperature, c.pressure));
        default: throw std::invalid_argument("fizmo::chemistry: that equation of state needs fitted constants, not critical constants");
    }
}

std::unique_ptr<EquationOfState> make_equation_of_state(EosKind kind, const Substance& s) {
    if (kind == EosKind::BeattieBridgeman) return std::make_unique<BeattieBridgeman>(BeattieBridgeman::preset(s.name));
    if (kind == EosKind::BenedictWebbRubin) return std::make_unique<BenedictWebbRubin>(BenedictWebbRubin::preset(s.name));
    if (kind == EosKind::IdealGas) return std::make_unique<IdealGas>();
    return make_equation_of_state(kind, CriticalConstants::of(s));
}

} // namespace chemistry
} // namespace fizmo
