#include "acid_base.hpp"

#include "nomenclature.hpp"

#include <algorithm>
#include <cctype>
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

namespace {

bool same_text(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    }
    return true;
}

std::vector<double> pkas_of(const AcidConstantEntry& a) {
    std::vector<double> out;
    for (int i = 1; i <= a.protons(); ++i) out.push_back(a.pka(i));
    return out;
}

std::vector<AcidBaseSpecies> build_species() {
    std::vector<AcidBaseSpecies> out;
    for (const AcidConstantEntry& a : acid_constants()) {
        const std::string_view name = a.name;
        if (name == "water" || name == "hydronium") continue;
        AcidBaseSpecies s;
        s.name = a.name;
        s.formula = a.formula;
        s.role = AcidBaseRole::Acid;
        s.strong = a.strong;
        s.pka = pkas_of(a);
        s.equivalents = a.protons();
        s.pk = a.pka1;
        const std::string_view f = a.formula;
        if (!f.empty() && f.back() == '+') {
            s.charge_of_most_protonated = 1;
            s.strong_acid_per_mole = 1.0;
            s.kind = "weak acid (cation, as its chloride)";
        } else if (a.strong && a.protons() == 1) {
            s.pka.clear();
            s.strong_acid_per_mole = 1.0;
            s.kind = "strong acid";
        } else if (a.strong) {
            s.kind = "strong acid (polyprotic)";
        } else {
            s.kind = a.protons() > 1 ? "weak acid (polyprotic)" : "weak acid";
        }
        out.push_back(std::move(s));
    }
    struct StrongBase {
        const char* name;
        const char* formula;
        int oh;
    };
    static const StrongBase strong_bases[] = {
        { "sodium hydroxide", "NaOH", 1 }, { "potassium hydroxide", "KOH", 1 }, { "lithium hydroxide", "LiOH", 1 },
        { "rubidium hydroxide", "RbOH", 1 }, { "cesium hydroxide", "CsOH", 1 }, { "calcium hydroxide", "Ca(OH)2", 2 },
        { "strontium hydroxide", "Sr(OH)2", 2 }, { "barium hydroxide", "Ba(OH)2", 2 },
    };
    for (const StrongBase& b : strong_bases) {
        AcidBaseSpecies s;
        s.name = b.name;
        s.formula = b.formula;
        s.role = AcidBaseRole::Base;
        s.strong = true;
        s.kind = "strong base";
        s.equivalents = b.oh;
        s.strong_base_per_mole = b.oh;
        out.push_back(std::move(s));
    }
    for (const BaseConstantEntry& b : base_constants()) {
        AcidBaseSpecies s;
        s.name = b.name;
        s.formula = b.formula;
        s.role = AcidBaseRole::Base;
        s.kind = "weak base";
        s.pka = { b.pka_of_conjugate_acid(WATER_PKW) };
        s.charge_of_most_protonated = 1;
        s.pk = b.pkb;
        out.push_back(std::move(s));
    }
    for (const PolyatomicIonEntry& ion : polyatomic_ions()) {
        if (ion.charge >= 0 || !ion.acid || !*ion.acid) continue;
        const AcidConstantEntry* parent = find_acid(ion.acid);
        if (!parent && ion.acid_formula && *ion.acid_formula) parent = find_acid(ion.acid_formula);
        if (!parent || std::string_view(parent->name) == "water") continue;
        const std::vector<double> p = pkas_of(*parent);
        const int k = -ion.charge;
        if (k > static_cast<int>(p.size())) continue;
        int accepts = 0;
        for (int i = 0; i < k; ++i) {
            if (p[static_cast<std::size_t>(i)] >= 1.5) ++accepts;
        }
        const std::string label = std::string(ion.name) + " (as its sodium salt)";
        if (accepts > 0) {
            AcidBaseSpecies s;
            s.name = ion.name;
            s.formula = ion.formula;
            s.role = AcidBaseRole::Base;
            s.kind = "weak base (anion)";
            s.pka = p;
            s.charge_of_most_protonated = 0;
            s.strong_base_per_mole = k;
            s.equivalents = accepts;
            s.pk = WATER_PKW - p[static_cast<std::size_t>(k - 1)];
            out.push_back(std::move(s));
        }
        if (k < static_cast<int>(p.size()) && p[static_cast<std::size_t>(k)] < 13.5) {
            AcidBaseSpecies s;
            s.name = ion.name;
            s.formula = ion.formula;
            s.role = AcidBaseRole::Acid;
            s.kind = "weak acid (anion, amphoteric)";
            s.pka = p;
            s.charge_of_most_protonated = 0;
            s.strong_base_per_mole = k;
            s.equivalents = static_cast<int>(p.size()) - k;
            s.pk = p[static_cast<std::size_t>(k)];
            out.push_back(std::move(s));
        }
    }
    return out;
}

} // namespace

const std::vector<AcidBaseSpecies>& acid_base_species() {
    static const std::vector<AcidBaseSpecies> list = build_species();
    return list;
}

const AcidBaseSpecies* find_acid_base_species(std::string_view key, int role) {
    while (!key.empty() && key.front() == ' ') key.remove_prefix(1);
    while (!key.empty() && key.back() == ' ') key.remove_suffix(1);
    const auto wanted = [&](const AcidBaseSpecies& s) { return role == 0 || (role > 0 && s.is_acid()) || (role < 0 && s.is_base()); };
    for (const AcidBaseSpecies& s : acid_base_species()) {
        if (wanted(s) && (same_text(s.name, key) || s.formula == key)) return &s;
    }
    for (const AcidBaseSpecies& s : acid_base_species()) {
        if (wanted(s) && same_text(s.formula, key)) return &s;
    }
    return nullptr;
}

void add_acid_base_species(const AcidBaseSpecies& s, double c, std::vector<AcidBaseComponent>& components, double& strong_acid, double& strong_base) {
    if (!(c > 0.0)) return;
    if (!s.pka.empty()) components.push_back(AcidBaseComponent{ c, s.pka, s.charge_of_most_protonated });
    strong_acid += s.strong_acid_per_mole * c;
    strong_base += s.strong_base_per_mole * c;
}

double solution_ph(const AcidBaseSpecies& s, double c, double kw) {
    std::vector<AcidBaseComponent> comps;
    double sa = 0.0, sb = 0.0;
    add_acid_base_species(s, c, comps, sa, sb);
    return equilibrium_ph(comps, sa, sb, kw);
}

double titration_point_ph(const AcidBaseSpecies& analyte, double ca, double va, const AcidBaseSpecies& titrant, double ct, double v, double kw) {
    const double total = va + v;
    if (!(total > 0.0)) throw std::invalid_argument("fizmo::chemistry: volume must be positive");
    std::vector<AcidBaseComponent> comps;
    double sa = 0.0, sb = 0.0;
    add_acid_base_species(analyte, ca * va / total, comps, sa, sb);
    add_acid_base_species(titrant, ct * v / total, comps, sa, sb);
    return equilibrium_ph(comps, sa, sb, kw);
}

TitrationResult titrate(const AcidBaseSpecies& analyte, double ca, double va, const AcidBaseSpecies& titrant, double ct, double vmax, int points, double kw) {
    TitrationResult r;
    if (analyte.role == titrant.role) {
        r.error = analyte.is_acid() ? "titrate an acid with a base" : "titrate a base with an acid";
        return r;
    }
    if (!(ca > 0.0) || !(va > 0.0) || !(ct > 0.0)) {
        r.error = "concentrations and volume must be positive";
        return r;
    }
    r.kind = std::string(analyte.strong ? "strong " : "weak ") + (analyte.is_acid() ? "acid" : "base") + " titrated with a " + (titrant.strong ? "strong " : "weak ") + (titrant.is_acid() ? "acid" : "base");
    const double per = ct * std::max(1, titrant.equivalents);
    const int n = std::max(1, analyte.equivalents);
    for (int k = 1; k <= n; ++k) {
        r.equivalence_volumes.push_back(ca * va * k / per);
        if (!analyte.strong || analyte.pka.size() > 1) r.half_equivalence_volumes.push_back(ca * va * (k - 0.5) / per);
    }
    if (!(vmax > 0.0)) vmax = 1.5 * r.equivalence_volumes.back();
    points = std::max(10, points);
    for (int i = 0; i <= points; ++i) {
        const double v = vmax * i / points;
        r.curve.emplace_back(v, titration_point_ph(analyte, ca, va, titrant, ct, v, kw));
    }
    r.start_ph = r.curve.front().second;
    r.ok = true;
    return r;
}

} // namespace chemistry
} // namespace fizmo
