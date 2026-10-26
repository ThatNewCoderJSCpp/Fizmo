#include "acid_base.hpp"

#include <cmath>
#include <stdexcept>

namespace fizmo {
namespace chemistry {

namespace {

double charge_balance(double h, const std::vector<AcidBaseComponent>& components, double strong_acid, double strong_base, double kw) {
    double positive = h + strong_base;
    double negative = kw / h + strong_acid;
    for (const AcidBaseComponent& c : components) {
        const std::vector<double> f = species_fractions(-std::log10(h), c.pka);
        for (std::size_t k = 0; k < f.size(); ++k) {
            const double z = c.charge_of_most_protonated - static_cast<double>(k);
            if (z > 0.0) positive += z * f[k] * c.concentration;
            else negative += -z * f[k] * c.concentration;
        }
    }
    return positive - negative;
}

} // namespace

double ph(double h) noexcept {
    return -std::log10(h);
}

double hydronium(double p) noexcept {
    return std::pow(10.0, -p);
}

double poh(double oh) noexcept {
    return -std::log10(oh);
}

double ph_from_poh(double p, double pkw) noexcept {
    return pkw - p;
}

double ka_from_pka(double pka) noexcept {
    return std::pow(10.0, -pka);
}

double pka_from_ka(double ka) noexcept {
    return -std::log10(ka);
}

double conjugate_pk(double pk, double pkw) noexcept {
    return pkw - pk;
}

double water_ion_product(double t) noexcept {
    const double pkw = 4470.99 / t - 6.0875 + 0.01706 * t;
    return std::pow(10.0, -pkw);
}

double strong_acid_ph(double c, double kw) noexcept {
    return -std::log10(0.5 * (c + std::sqrt(c * c + 4.0 * kw)));
}

double strong_base_ph(double c, double kw) noexcept {
    const double oh = 0.5 * (c + std::sqrt(c * c + 4.0 * kw));
    return -std::log10(kw / oh);
}

double weak_acid_ph(double c, double ka, double kw) noexcept {
    return equilibrium_ph({ AcidBaseComponent{ c, { pka_from_ka(ka) }, 0 } }, 0.0, 0.0, kw);
}

double weak_base_ph(double c, double kb, double kw) noexcept {
    const double pka = -std::log10(kw / kb);
    return equilibrium_ph({ AcidBaseComponent{ c, { pka }, 1 } }, 0.0, 0.0, kw);
}

double henderson_hasselbalch(double pka, double base, double acid) noexcept {
    return pka + std::log10(base / acid);
}

double buffer_ratio(double target_ph, double pka) noexcept {
    return std::pow(10.0, target_ph - pka);
}

double buffer_capacity(double total, double ka, double h, double kw) noexcept {
    return LN10 * (kw / h + h + total * ka * h / ((ka + h) * (ka + h)));
}

double percent_ionization(double c, double ka) noexcept {
    const double x = 0.5 * (-ka + std::sqrt(ka * ka + 4.0 * ka * c));
    return 100.0 * x / c;
}

std::vector<double> species_fractions(double ph_value, const std::vector<double>& pka) {
    const double h = std::pow(10.0, -ph_value);
    const std::size_t n = pka.size();
    std::vector<double> terms(n + 1);
    double log_term = 0.0;
    std::vector<double> logs(n + 1);
    logs[0] = static_cast<double>(n) * std::log(h);
    for (std::size_t k = 1; k <= n; ++k) {
        log_term += -pka[k - 1] * LN10;
        logs[k] = static_cast<double>(n - k) * std::log(h) + log_term;
    }
    double peak = logs[0];
    for (double v : logs) peak = std::max(peak, v);
    double sum = 0.0;
    for (std::size_t k = 0; k <= n; ++k) {
        terms[k] = std::exp(logs[k] - peak);
        sum += terms[k];
    }
    for (double& t : terms) t /= sum;
    return terms;
}

double average_protons_bound(double ph_value, const std::vector<double>& pka) {
    const std::vector<double> f = species_fractions(ph_value, pka);
    double n = 0.0;
    for (std::size_t k = 0; k < f.size(); ++k) n += static_cast<double>(pka.size() - k) * f[k];
    return n;
}

double isoelectric_point(double pka_acid, double pka_base) noexcept {
    return 0.5 * (pka_acid + pka_base);
}

double equilibrium_ph(const std::vector<AcidBaseComponent>& components, double strong_acid, double strong_base, double kw) {
    double lo = -3.0, hi = 17.0;
    auto f = [&](double p) { return charge_balance(std::pow(10.0, -p), components, strong_acid, strong_base, kw); };
    double flo = f(lo);
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = f(mid);
        if ((fm > 0.0) == (flo > 0.0)) {
            lo = mid;
            flo = fm;
        } else {
            hi = mid;
        }
        if (hi - lo < 1e-13) break;
    }
    return 0.5 * (lo + hi);
}

double titration_ph(double acid_moles, const std::vector<double>& pka, double titrant_moles, double total_volume, bool titrant_is_base, double kw) {
    if (total_volume <= 0.0) throw std::invalid_argument("fizmo::chemistry: volume must be positive");
    const double c = acid_moles / total_volume;
    const double t = titrant_moles / total_volume;
    if (titrant_is_base) return equilibrium_ph({ AcidBaseComponent{ c, pka, 0 } }, 0.0, t, kw);
    return equilibrium_ph({ AcidBaseComponent{ c, pka, static_cast<int>(pka.size()) } }, t, 0.0, kw);
}

std::vector<std::pair<double, double>> titration_curve(double acid_concentration, double acid_volume, const std::vector<double>& pka, double titrant_concentration, double max_volume, int points, bool titrant_is_base) {
    std::vector<std::pair<double, double>> out;
    const double moles = acid_concentration * acid_volume;
    for (int i = 0; i <= points; ++i) {
        const double v = max_volume * i / points;
        out.emplace_back(v, titration_ph(moles, pka, titrant_concentration * v, acid_volume + v, titrant_is_base));
    }
    return out;
}

double equivalence_volume(double analyte_moles, double titrant_concentration, int protons) noexcept {
    return analyte_moles * protons / titrant_concentration;
}

} // namespace chemistry
} // namespace fizmo
