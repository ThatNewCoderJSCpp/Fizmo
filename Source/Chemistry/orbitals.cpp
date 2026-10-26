#include "orbitals.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

constexpr double BOHR_PM = BOHR_RADIUS / PICOMETER;
const char SUBSHELL_LETTERS[] = { 's', 'p', 'd', 'f', 'g', 'h', 'i' };

double factorial(int n) {
    double f = 1.0;
    for (int i = 2; i <= n; ++i) f *= i;
    return f;
}

double laguerre(int k, int alpha, double x) {
    if (k == 0) return 1.0;
    double l0 = 1.0;
    double l1 = 1.0 + alpha - x;
    for (int i = 1; i < k; ++i) {
        const double l2 = ((2.0 * i + 1.0 + alpha - x) * l1 - (i + alpha) * l0) / (i + 1.0);
        l0 = l1;
        l1 = l2;
    }
    return l1;
}

double legendre(int l, int m, double x) {
    double pmm = 1.0;
    if (m > 0) {
        const double somx2 = std::sqrt(std::max(0.0, (1.0 - x) * (1.0 + x)));
        double fact = 1.0;
        for (int i = 1; i <= m; ++i) {
            pmm *= fact * somx2;
            fact += 2.0;
        }
    }
    if (l == m) return pmm;
    double pmmp1 = x * (2.0 * m + 1.0) * pmm;
    if (l == m + 1) return pmmp1;
    double pll = 0.0;
    for (int ll = m + 2; ll <= l; ++ll) {
        pll = (x * (2.0 * ll - 1.0) * pmmp1 - (ll + m - 1.0) * pmm) / (ll - m);
        pmm = pmmp1;
        pmmp1 = pll;
    }
    return pll;
}

void check(int n, int l, int m) {
    if (n < 1 || l < 0 || l >= n || std::abs(m) > l) throw std::invalid_argument("fizmo::chemistry: quantum numbers must satisfy n >= 1, 0 <= l < n and |m| <= l");
}

struct Rng {
    std::uint64_t s;
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(s >> 11) / 9007199254740992.0;
    }
};

double max_harmonic_square(int l, int m) {
    double best = 0.0;
    for (int i = 0; i <= 180; ++i) {
        const double theta = PI * i / 180.0;
        for (int j = 0; j < 72; ++j) {
            const double phi = 2.0 * PI * j / 72.0;
            const double y = real_spherical_harmonic(l, m, theta, phi);
            best = std::max(best, y * y);
        }
    }
    return best * 1.05;
}

} // namespace

std::string subshell_name(int n, int l) {
    if (l < 0 || l > 6) return std::to_string(n) + "?";
    return std::to_string(n) + SUBSHELL_LETTERS[l];
}

std::string orbital_shape_name(int l, int m) {
    switch (l) {
        case 0: return "";
        case 1: return m == 0 ? "z" : (m > 0 ? "x" : "y");
        case 2:
            switch (m) {
                case 0: return "z\xC2\xB2";
                case 1: return "xz";
                case -1: return "yz";
                case 2: return "x\xC2\xB2-y\xC2\xB2";
                default: return "xy";
            }
        case 3:
            switch (m) {
                case 0: return "z\xC2\xB3";
                case 1: return "xz\xC2\xB2";
                case -1: return "yz\xC2\xB2";
                case 2: return "z(x\xC2\xB2-y\xC2\xB2)";
                case -2: return "xyz";
                case 3: return "x(x\xC2\xB2-3y\xC2\xB2)";
                default: return "y(3x\xC2\xB2-y\xC2\xB2)";
            }
        default: return "m=" + std::to_string(m);
    }
}

std::string orbital_name(int n, int l, int m) {
    const std::string shape = orbital_shape_name(l, m);
    return shape.empty() ? subshell_name(n, l) : subshell_name(n, l) + "_" + shape;
}

int radial_node_count(int n, int l) noexcept {
    return n - l - 1;
}

int angular_node_count(int l) noexcept {
    return l;
}

double slater_effective_n(int n) noexcept {
    switch (n) {
        case 1: return 1.0;
        case 2: return 2.0;
        case 3: return 3.0;
        case 4: return 3.7;
        case 5: return 4.0;
        default: return 4.2;
    }
}

double effective_nuclear_charge(const Element& element, int n, int l, int charge) {
    const ElectronConfiguration c = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    double z = c.effective_nuclear_charge(element.atomic_number, n, l);
    if (!(z > 0.0)) z = std::max(1.0, static_cast<double>(element.atomic_number) - c.electrons() + 1.0);
    return z;
}

double radial_wavefunction(int n, int l, double z_eff, double r, OrbitalModel model) {
    if (r < 0.0) r = 0.0;
    if (model == OrbitalModel::Slater) {
        const double ns = slater_effective_n(n);
        const double zeta = z_eff / ns;
        const double norm = std::pow(2.0 * zeta, ns + 0.5) / std::sqrt(std::tgamma(2.0 * ns + 1.0));
        return norm * std::pow(r, ns - 1.0) * std::exp(-zeta * r);
    }
    const double rho = 2.0 * z_eff * r / n;
    const double norm = std::sqrt(std::pow(2.0 * z_eff / n, 3) * factorial(n - l - 1) / (2.0 * n * factorial(n + l)));
    return norm * std::exp(-rho / 2.0) * std::pow(rho, l) * laguerre(n - l - 1, 2 * l + 1, rho);
}

double radial_probability(int n, int l, double z_eff, double r, OrbitalModel model) {
    const double R = radial_wavefunction(n, l, z_eff, r, model);
    return r * r * R * R;
}

double real_spherical_harmonic(int l, int m, double theta, double phi) {
    const int am = std::abs(m);
    const double norm = std::sqrt((2.0 * l + 1.0) / (4.0 * PI) * factorial(l - am) / factorial(l + am));
    const double p = legendre(l, am, std::cos(theta));
    if (m == 0) return norm * p;
    if (m > 0) return std::sqrt(2.0) * norm * p * std::cos(am * phi);
    return std::sqrt(2.0) * norm * p * std::sin(am * phi);
}

double wavefunction(int n, int l, int m, double z_eff, double x, double y, double z, OrbitalModel model) {
    const double r = std::sqrt(x * x + y * y + z * z);
    if (r < 1e-12) return l == 0 ? radial_wavefunction(n, l, z_eff, 0.0, model) * real_spherical_harmonic(0, 0, 0.0, 0.0) : 0.0;
    const double theta = std::acos(std::max(-1.0, std::min(1.0, z / r)));
    const double phi = std::atan2(y, x);
    return radial_wavefunction(n, l, z_eff, r, model) * real_spherical_harmonic(l, m, theta, phi);
}

double probability_density(int n, int l, int m, double z_eff, double x, double y, double z, OrbitalModel model) {
    const double psi = wavefunction(n, l, m, z_eff, x, y, z, model);
    return psi * psi;
}

double mean_radius_bohr(int n, int l, double z_eff) noexcept {
    return (3.0 * n * n - l * (l + 1.0)) / (2.0 * z_eff);
}

double most_probable_radius_bohr(int n, int l, double z_eff, OrbitalModel model) {
    const double extent = orbital_extent_bohr(n, l, z_eff, 0.999, model);
    double best_r = 0.0, best_p = -1.0;
    const int steps = 4000;
    for (int i = 1; i <= steps; ++i) {
        const double r = extent * i / steps;
        const double p = radial_probability(n, l, z_eff, r, model);
        if (p > best_p) {
            best_p = p;
            best_r = r;
        }
    }
    return best_r;
}

double orbital_extent_bohr(int n, int l, double z_eff, double containment, OrbitalModel model) {
    const double guess = 4.0 * mean_radius_bohr(n, l, z_eff) + 2.0;
    const int steps = 6000;
    double total = 0.0;
    std::vector<double> cum(steps + 1, 0.0);
    double prev = radial_probability(n, l, z_eff, 0.0, model);
    for (int i = 1; i <= steps; ++i) {
        const double r = guess * i / steps;
        const double p = radial_probability(n, l, z_eff, r, model);
        total += 0.5 * (p + prev) * guess / steps;
        cum[i] = total;
        prev = p;
    }
    if (total <= 0.0) return guess;
    for (int i = 1; i <= steps; ++i) {
        if (cum[i] / total >= containment) return guess * i / steps;
    }
    return guess;
}

double orbital_energy_ev(int n, double z_eff) noexcept {
    return -RYDBERG_ENERGY_EV * z_eff * z_eff / (n * n);
}

RadialCurve radial_distribution(int n, int l, double z_eff, double r_max_pm, int points, OrbitalModel model) {
    check(n, l, 0);
    RadialCurve c;
    c.n = n;
    c.l = l;
    c.z_eff = z_eff;
    c.label = subshell_name(n, l);
    points = std::max(16, points);
    c.r_pm.resize(points + 1);
    c.probability.resize(points + 1);
    c.cumulative.resize(points + 1);
    const double dr_pm = r_max_pm / points;
    double total = 0.0;
    double best = -1.0;
    for (int i = 0; i <= points; ++i) {
        const double rpm = dr_pm * i;
        const double p = radial_probability(n, l, z_eff, rpm / BOHR_PM, model) / BOHR_PM;
        c.r_pm[i] = rpm;
        c.probability[i] = p;
        if (i > 0) total += 0.5 * (p + c.probability[i - 1]) * dr_pm;
        c.cumulative[i] = total;
        if (p > best) {
            best = p;
            c.most_probable_pm = rpm;
        }
    }
    if (model == OrbitalModel::Hydrogenic) {
        c.mean_pm = mean_radius_bohr(n, l, z_eff) * BOHR_PM;
        const int k = n - l - 1;
        if (k > 0) {
            const double hi = 4.0 * n * n / z_eff + 10.0;
            const int scan = 20000;
            double prev = laguerre(k, 2 * l + 1, 2.0 * z_eff * 1e-6 / n);
            for (int i = 1; i <= scan; ++i) {
                const double r = hi * i / scan;
                const double v = laguerre(k, 2 * l + 1, 2.0 * z_eff * r / n);
                if ((v > 0) != (prev > 0)) c.node_radii_pm.push_back(r * BOHR_PM);
                prev = v;
            }
        }
    } else {
        double num = 0.0;
        for (int i = 1; i <= points; ++i) num += 0.5 * (c.probability[i] * c.r_pm[i] + c.probability[i - 1] * c.r_pm[i - 1]) * dr_pm;
        c.mean_pm = total > 0 ? num / total : 0.0;
    }
    const double full = 1.0;
    for (int i = 0; i <= points; ++i) {
        if (c.cumulative[i] >= 0.9 * full) {
            c.radius_90_pm = c.r_pm[i];
            break;
        }
    }
    if (c.radius_90_pm == 0.0) c.radius_90_pm = orbital_extent_bohr(n, l, z_eff, 0.9, model) * BOHR_PM;
    c.energy_ev = orbital_energy_ev(n, z_eff);
    return c;
}

RadialCurve radial_distribution(const Element& element, int n, int l, double r_max_pm, int points, OrbitalModel model, int charge) {
    const double z = effective_nuclear_charge(element, n, l, charge);
    RadialCurve c = radial_distribution(n, l, z, r_max_pm, points, model);
    const ElectronConfiguration conf = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    c.occupancy = conf.occupancy(n, l);
    return c;
}

std::vector<RadialCurve> atom_radial_distributions(const Element& element, double r_max_pm, int points, OrbitalModel model, int charge) {
    std::vector<RadialCurve> out;
    const ElectronConfiguration conf = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    for (const Subshell& s : conf.subshells()) {
        if (s.electrons <= 0) continue;
        out.push_back(radial_distribution(element, s.n, s.l, r_max_pm, points, model, charge));
    }
    std::sort(out.begin(), out.end(), [](const RadialCurve& a, const RadialCurve& b) { return a.n != b.n ? a.n < b.n : a.l < b.l; });
    return out;
}

RadialCurve total_radial_density(const Element& element, double r_max_pm, int points, OrbitalModel model, int charge) {
    const std::vector<RadialCurve> parts = atom_radial_distributions(element, r_max_pm, points, model, charge);
    RadialCurve total;
    total.label = std::string(element.symbol) + " (all electrons)";
    if (parts.empty()) return total;
    total.r_pm = parts.front().r_pm;
    total.probability.assign(total.r_pm.size(), 0.0);
    total.cumulative.assign(total.r_pm.size(), 0.0);
    int electrons = 0;
    for (const RadialCurve& c : parts) {
        electrons += c.occupancy;
        for (std::size_t i = 0; i < c.probability.size(); ++i) total.probability[i] += c.occupancy * c.probability[i];
    }
    total.occupancy = electrons;
    double best = -1.0;
    double cum = 0.0;
    double num = 0.0;
    for (std::size_t i = 0; i < total.probability.size(); ++i) {
        if (i > 0) {
            const double dr = total.r_pm[i] - total.r_pm[i - 1];
            cum += 0.5 * (total.probability[i] + total.probability[i - 1]) * dr;
            num += 0.5 * (total.probability[i] * total.r_pm[i] + total.probability[i - 1] * total.r_pm[i - 1]) * dr;
        }
        total.cumulative[i] = cum;
        if (total.probability[i] > best) {
            best = total.probability[i];
            total.most_probable_pm = total.r_pm[i];
        }
    }
    total.mean_pm = cum > 0.0 ? num / cum : 0.0;
    for (std::size_t i = 0; i < total.cumulative.size(); ++i) {
        if (total.cumulative[i] >= 0.9 * electrons) {
            total.radius_90_pm = total.r_pm[i];
            break;
        }
    }
    return total;
}

double suggested_radial_range_pm(const Element& element, int charge) {
    const ElectronConfiguration conf = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    double best = 50.0;
    for (const Subshell& s : conf.subshells()) {
        if (s.electrons <= 0) continue;
        const double z = effective_nuclear_charge(element, s.n, s.l, charge);
        best = std::max(best, orbital_extent_bohr(s.n, s.l, z, 0.995) * BOHR_PM);
    }
    return best * 1.1;
}

std::vector<CloudPoint> sample_orbital(int n, int l, int m, double z_eff, int count, unsigned seed, OrbitalModel model) {
    check(n, l, m);
    std::vector<CloudPoint> out;
    if (count <= 0) return out;
    out.reserve(static_cast<std::size_t>(count));
    const double extent = orbital_extent_bohr(n, l, z_eff, 0.999, model);
    const int table = 2000;
    std::vector<double> cum(table + 1, 0.0);
    double total = 0.0;
    double prev = radial_probability(n, l, z_eff, 0.0, model);
    for (int i = 1; i <= table; ++i) {
        const double r = extent * i / table;
        const double p = radial_probability(n, l, z_eff, r, model);
        total += 0.5 * (p + prev);
        cum[i] = total;
        prev = p;
    }
    Rng rng{ seed * 0x9E3779B97F4A7C15ULL + 7 };
    const double ymax = max_harmonic_square(l, m);
    while (static_cast<int>(out.size()) < count) {
        const double u = rng.next() * total;
        const std::size_t idx = static_cast<std::size_t>(std::lower_bound(cum.begin(), cum.end(), u) - cum.begin());
        const std::size_t i1 = std::min<std::size_t>(std::max<std::size_t>(idx, 1), table);
        const double frac = cum[i1] - cum[i1 - 1] > 0 ? (u - cum[i1 - 1]) / (cum[i1] - cum[i1 - 1]) : 0.5;
        const double r = extent * (static_cast<double>(i1 - 1) + frac) / table;
        double theta = 0.0, phi = 0.0, y = 0.0;
        for (int tries = 0; tries < 1000; ++tries) {
            const double cz = 2.0 * rng.next() - 1.0;
            theta = std::acos(cz);
            phi = 2.0 * PI * rng.next();
            y = real_spherical_harmonic(l, m, theta, phi);
            if (rng.next() * ymax <= y * y) break;
        }
        const double rad = radial_wavefunction(n, l, z_eff, r, model);
        CloudPoint p;
        const double rp = r * BOHR_PM;
        p.x = rp * std::sin(theta) * std::cos(phi);
        p.y = rp * std::sin(theta) * std::sin(phi);
        p.z = rp * std::cos(theta);
        const double psi = rad * y;
        p.density = psi * psi;
        p.sign = psi >= 0 ? 1 : -1;
        p.n = n;
        p.l = l;
        out.push_back(p);
    }
    return out;
}

std::vector<CloudPoint> sample_atom(const Element& element, int count, unsigned seed, OrbitalModel model, int charge, bool valence_only) {
    std::vector<CloudPoint> out;
    const ElectronConfiguration conf = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    const int top = conf.highest_shell();
    int electrons = 0;
    for (const Subshell& s : conf.subshells()) {
        if (s.electrons > 0 && (!valence_only || s.n == top || (s.l >= 2 && s.n == top - 1) || (s.l == 3 && s.n == top - 2))) electrons += s.electrons;
    }
    if (electrons == 0) return out;
    unsigned k = 0;
    for (const Subshell& s : conf.subshells()) {
        if (s.electrons <= 0) continue;
        if (valence_only && !(s.n == top || (s.l >= 2 && s.n == top - 1) || (s.l == 3 && s.n == top - 2))) continue;
        const double z = effective_nuclear_charge(element, s.n, s.l, charge);
        const int share = std::max(1, count * s.electrons / electrons);
        const int orbitals = 2 * s.l + 1;
        for (int m = -s.l; m <= s.l; ++m) {
            const int per = std::max(1, share / orbitals);
            std::vector<CloudPoint> part = sample_orbital(s.n, s.l, m, z, per, seed * 131 + (++k), model);
            out.insert(out.end(), part.begin(), part.end());
        }
    }
    return out;
}

DensitySlice orbital_slice(int n, int l, int m, double z_eff, char axis_a, char axis_b, double extent_pm, int resolution, bool signed_wavefunction, OrbitalModel model) {
    check(n, l, m);
    DensitySlice d;
    d.width = d.height = std::max(8, resolution);
    d.extent_pm = extent_pm;
    d.values.resize(static_cast<std::size_t>(d.width) * d.height);
    double mx = 0.0, mn = 0.0;
    for (int j = 0; j < d.height; ++j) {
        const double b = (1.0 - 2.0 * (j + 0.5) / d.height) * extent_pm / BOHR_PM;
        for (int i = 0; i < d.width; ++i) {
            const double a = (2.0 * (i + 0.5) / d.width - 1.0) * extent_pm / BOHR_PM;
            double x = 0, y = 0, z = 0;
            (axis_a == 'x' ? x : axis_a == 'y' ? y : z) = a;
            (axis_b == 'x' ? x : axis_b == 'y' ? y : z) = b;
            const double psi = wavefunction(n, l, m, z_eff, x, y, z, model);
            const double v = signed_wavefunction ? psi : psi * psi;
            d.values[static_cast<std::size_t>(j) * d.width + i] = v;
            mx = std::max(mx, v);
            mn = std::min(mn, v);
        }
    }
    d.max_value = mx;
    d.min_value = mn;
    return d;
}

DensitySlice atom_slice(const Element& element, double extent_pm, int resolution, OrbitalModel model, int charge) {
    DensitySlice d;
    d.width = d.height = std::max(8, resolution);
    d.extent_pm = extent_pm;
    d.values.assign(static_cast<std::size_t>(d.width) * d.height, 0.0);
    const ElectronConfiguration conf = charge == 0 ? ElectronConfiguration::ground_state(element) : ElectronConfiguration::of_ion(element, charge);
    for (const Subshell& s : conf.subshells()) {
        if (s.electrons <= 0) continue;
        const double z = effective_nuclear_charge(element, s.n, s.l, charge);
        const double per_orbital = static_cast<double>(s.electrons) / (2 * s.l + 1);
        for (int j = 0; j < d.height; ++j) {
            const double b = (1.0 - 2.0 * (j + 0.5) / d.height) * extent_pm / BOHR_PM;
            for (int i = 0; i < d.width; ++i) {
                const double a = (2.0 * (i + 0.5) / d.width - 1.0) * extent_pm / BOHR_PM;
                double sum = 0.0;
                for (int m = -s.l; m <= s.l; ++m) sum += probability_density(s.n, s.l, m, z, a, b, 0.0, model);
                d.values[static_cast<std::size_t>(j) * d.width + i] += per_orbital * sum;
            }
        }
    }
    for (double v : d.values) d.max_value = std::max(d.max_value, v);
    return d;
}

} // namespace chemistry
} // namespace fizmo
