#include "gas_stoichiometry.hpp"

#include "periodic_table.hpp"
#include "substance.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace chemistry {

namespace {

bool known(double v) {
    return v == v && std::isfinite(v);
}

const Substance* substance_for(const Formula& f) {
    for (const std::string& key : { f.hill(), f.to_string() }) {
        if (key.empty()) continue;
        if (const Substance* s = find_substance(key)) return s;
    }
    return nullptr;
}

const Element* single_element(const Formula& f) {
    const auto parts = f.composition();
    if (parts.size() != 1) return nullptr;
    return parts.front().first;
}

std::string text_of(const Formula& f) {
    std::string s = f.display_string(false);
    return s.empty() ? f.to_string() : s;
}

double pressure_or(double v, double fallback) {
    return known(v) && v > 0.0 ? v : fallback;
}

void cap_condensables(std::vector<GasSpeciesState>& gases, GasVesselMode mode, double t, double p, double v) {
    for (GasSpeciesState& g : gases) g.condensed_moles = 0.0;
    if (mode == GasVesselMode::Rigid) {
        for (GasSpeciesState& g : gases) {
            if (!known(g.vapor_pressure) || !(v > 0.0)) continue;
            const double most = g.vapor_pressure * v / (GAS_CONSTANT * t);
            if (g.moles > most) {
                g.condensed_moles = g.moles - most;
                g.moles = most;
            }
        }
        return;
    }
    if (!(p > 0.0)) return;
    std::vector<bool> capped(gases.size(), false);
    for (int pass = 0; pass < 20; ++pass) {
        double free_moles = 0.0, share = 0.0;
        for (std::size_t i = 0; i < gases.size(); ++i) {
            if (capped[i]) share += gases[i].vapor_pressure / p;
            else free_moles += gases[i].moles + gases[i].condensed_moles;
        }
        if (share >= 1.0) break;
        const double total = free_moles / (1.0 - share);
        bool changed = false;
        for (std::size_t i = 0; i < gases.size(); ++i) {
            GasSpeciesState& g = gases[i];
            const double all = g.moles + g.condensed_moles;
            if (capped[i]) {
                const double vapor = std::min(all, g.vapor_pressure / p * total);
                g.moles = vapor;
                g.condensed_moles = all - vapor;
                continue;
            }
            if (!known(g.vapor_pressure) || !(total > 0.0)) continue;
            if (all / total * p > g.vapor_pressure) {
                capped[i] = true;
                changed = true;
            }
        }
        if (!changed) break;
    }
}

void fill_pressures(std::vector<GasSpeciesState>& gases, const std::vector<VanDerWaalsConstants>& vdw, GasVesselMode mode, GasModel model, double t, double p, double v, double& total_moles, double& total_pressure, double& total_volume) {
    total_moles = 0.0;
    for (const GasSpeciesState& g : gases) total_moles += g.moles;
    std::vector<double> x(gases.size(), 0.0);
    for (std::size_t i = 0; i < gases.size(); ++i) {
        x[i] = total_moles > 0.0 ? gases[i].moles / total_moles : 0.0;
        gases[i].mole_fraction = x[i];
    }
    const VanDerWaalsConstants mix = model == GasModel::VanDerWaals ? van_der_waals_mixture(vdw, x) : VanDerWaalsConstants{};
    if (mode == GasVesselMode::Rigid) {
        total_volume = v;
        total_pressure = total_moles > 0.0 ? gas_pressure(total_moles, v, t, model, mix) : 0.0;
    } else {
        total_pressure = p;
        total_volume = total_moles > 0.0 ? gas_volume(total_moles, p, t, model, mix) : 0.0;
    }
    for (std::size_t i = 0; i < gases.size(); ++i) {
        gases[i].partial_pressure = x[i] * total_pressure;
        gases[i].volume = total_moles > 0.0 ? x[i] * total_volume : 0.0;
    }
}

} // namespace

const char* to_string(GasAmountKind k) noexcept {
    switch (k) {
        case GasAmountKind::Mass: return "mass";
        case GasAmountKind::Moles: return "moles";
        case GasAmountKind::Particles: return "particles";
        case GasAmountKind::SolutionVolume: return "solution volume";
        case GasAmountKind::GasVolume: return "gas volume";
        case GasAmountKind::PartialPressure: return "partial pressure";
        case GasAmountKind::CollectedOverWater: return "gas collected over water";
    }
    return "amount";
}

VanDerWaalsConstants van_der_waals_constants(double tc, double pc) noexcept {
    VanDerWaalsConstants c;
    if (!known(tc) || !known(pc) || tc <= 0.0 || pc <= 0.0) return c;
    c.a = 27.0 * GAS_CONSTANT * GAS_CONSTANT * tc * tc / (64.0 * pc);
    c.b = GAS_CONSTANT * tc / (8.0 * pc);
    return c;
}

VanDerWaalsConstants van_der_waals_mixture(const std::vector<VanDerWaalsConstants>& parts, const std::vector<double>& x) noexcept {
    VanDerWaalsConstants c;
    double sa = 0.0, b = 0.0;
    for (std::size_t i = 0; i < parts.size() && i < x.size(); ++i) {
        if (!parts[i].ok()) continue;
        sa += x[i] * std::sqrt(parts[i].a);
        b += x[i] * parts[i].b;
    }
    if (sa > 0.0 && b > 0.0) {
        c.a = sa * sa;
        c.b = b;
    }
    return c;
}

double water_vapor_pressure(double t) noexcept {
    if (!(t > 0.0)) return NO_VALUE;
    if (const Substance* w = find_substance("H2O")) {
        const double p = w->vapor_pressure(t);
        if (known(p)) return p;
    }
    const double c = t - 273.15;
    return 610.94 * std::exp(17.625 * c / (c + 243.04));
}

GasBehavior gas_behavior(const Formula& f, double t) {
    GasBehavior g;
    const Phase marked = f.phase();
    if (marked == Phase::Solid || marked == Phase::Liquid || marked == Phase::Aqueous) {
        g.source = "marked as not a gas";
        return g;
    }
    double tc = NO_VALUE, pc = NO_VALUE, bp = NO_VALUE, pvap = NO_VALUE;
    bool standard_gas = false;
    if (const Substance* s = substance_for(f)) {
        tc = s->critical_temperature;
        pc = s->critical_pressure;
        bp = s->boiling_point;
        standard_gas = s->standard_state == Phase::Gas;
        if (!known(tc) || t < tc) pvap = s->vapor_pressure(t);
        g.source = "substance data";
    } else if (const Element* e = single_element(f)) {
        tc = e->critical_temperature;
        pc = e->critical_pressure;
        bp = e->boiling_point;
        standard_gas = e->standard_state == Phase::Gas;
        if (known(bp) && known(e->vaporization_heat) && (!known(tc) || t < tc)) pvap = ATMOSPHERE * std::exp(-e->vaporization_heat * 1000.0 / GAS_CONSTANT * (1.0 / t - 1.0 / bp));
        g.source = "element data";
    }
    g.boiling_point = bp;
    g.critical_temperature = tc;
    g.vdw = van_der_waals_constants(tc, pc);
    const bool volatile_enough = standard_gas || (known(bp) && bp < 650.0);
    g.can_be_gas = marked == Phase::Gas || volatile_enough;
    if (g.can_be_gas && known(pvap) && pvap > 0.0 && (!known(tc) || t < tc)) {
        g.condensable = true;
        g.vapor_pressure = pvap;
    }
    if (g.source.empty()) g.source = marked == Phase::Gas ? "marked (g)" : "no data";
    return g;
}

double gas_pressure(double n, double v, double t, GasModel model, const VanDerWaalsConstants& c) {
    if (!(v > 0.0)) return NO_VALUE;
    if (model == GasModel::VanDerWaals && c.ok() && v > n * c.b) return n * GAS_CONSTANT * t / (v - n * c.b) - c.a * n * n / (v * v);
    return n * GAS_CONSTANT * t / v;
}

double gas_moles(double p, double v, double t, GasModel model, const VanDerWaalsConstants& c) {
    if (!(t > 0.0) || !(v > 0.0)) return NO_VALUE;
    double n = p * v / (GAS_CONSTANT * t);
    if (model != GasModel::VanDerWaals || !c.ok()) return n;
    for (int i = 0; i < 100; ++i) {
        const double f = gas_pressure(n, v, t, model, c) - p;
        const double h = std::max(1e-12, n * 1e-6);
        const double d = (gas_pressure(n + h, v, t, model, c) - gas_pressure(n - h, v, t, model, c)) / (2.0 * h);
        if (!(std::fabs(d) > 0.0)) break;
        double next = n - f / d;
        if (next <= 0.0) next = 0.5 * n;
        if (next * c.b >= v) next = 0.5 * (n + v / c.b);
        if (std::fabs(next - n) < 1e-14 * std::max(1.0, n)) {
            n = next;
            break;
        }
        n = next;
    }
    return n;
}

double gas_volume(double n, double p, double t, GasModel model, const VanDerWaalsConstants& c) {
    if (!(p > 0.0)) return NO_VALUE;
    double v = n * GAS_CONSTANT * t / p;
    if (model != GasModel::VanDerWaals || !c.ok() || !(n > 0.0)) return v;
    for (int i = 0; i < 100; ++i) {
        const double f = gas_pressure(n, v, t, model, c) - p;
        const double h = std::max(1e-15, v * 1e-7);
        const double d = (gas_pressure(n, v + h, t, model, c) - gas_pressure(n, v - h, t, model, c)) / (2.0 * h);
        if (!(std::fabs(d) > 0.0)) break;
        double next = v - f / d;
        if (next <= n * c.b) next = 0.5 * (v + n * c.b);
        if (std::fabs(next - v) < 1e-14 * v) {
            v = next;
            break;
        }
        v = next;
    }
    return v;
}

double gas_known_moles(const GasKnown& k, const Formula& f, const GasVessel& vessel, std::string* step) {
    const double t = pressure_or(k.temperature, vessel.temperature);
    const double m = f.molar_mass();
    const GasBehavior b = gas_behavior(f, t);
    const VanDerWaalsConstants c = vessel.model == GasModel::VanDerWaals ? b.vdw : VanDerWaalsConstants{};
    const std::string name = text_of(f);
    const std::string model = vessel.model == GasModel::VanDerWaals && c.ok() ? " (van der Waals)" : "";
    double n = NO_VALUE;
    std::string how;
    switch (k.kind) {
        case GasAmountKind::Mass:
            n = k.value * 1000.0 / m;
            how = format_number(k.value * 1000.0, 5) + " g \xC3\xB7 " + format_number(m, 5) + " g/mol";
            break;
        case GasAmountKind::Moles:
            n = k.value;
            how = "given";
            break;
        case GasAmountKind::Particles:
            n = k.value / AVOGADRO_CONSTANT;
            how = format_number(k.value, 5) + " particles \xC3\xB7 N\xE2\x82\x90";
            break;
        case GasAmountKind::SolutionVolume:
            n = k.value * k.molarity;
            how = format_number(k.value * 1000.0, 5) + " L \xC3\x97 " + format_number(k.molarity / 1000.0, 5) + " M";
            break;
        case GasAmountKind::GasVolume: {
            const double p = pressure_or(k.pressure, vessel.pressure);
            n = gas_moles(p, k.value, t, vessel.model, c);
            how = "n = PV/RT" + model + " with P = " + format_number(p / ATMOSPHERE, 5) + " atm, V = " + format_number(k.value * 1000.0, 5) + " L, T = " + format_number(t, 5) + " K";
            break;
        }
        case GasAmountKind::PartialPressure: {
            const double v = pressure_or(k.volume, vessel.volume);
            if (!known(v)) {
                if (step) *step = name + ": a partial pressure needs the volume it's in";
                return NO_VALUE;
            }
            n = gas_moles(k.value, v, t, vessel.model, c);
            how = "n = P\xE1\xB5\xA2V/RT" + model + " with P\xE1\xB5\xA2 = " + format_number(k.value / ATMOSPHERE, 5) + " atm in " + format_number(v * 1000.0, 5) + " L at " + format_number(t, 5) + " K";
            break;
        }
        case GasAmountKind::CollectedOverWater: {
            const double p = pressure_or(k.pressure, vessel.pressure);
            const double pw = water_vapor_pressure(t);
            const double dry = p - pw;
            if (!(dry > 0.0)) {
                if (step) *step = name + ": the water's vapor pressure is above the total pressure";
                return NO_VALUE;
            }
            n = gas_moles(dry, k.value, t, vessel.model, c);
            how = "P(" + name + ") = P\xE2\x82\x9C\xE2\x82\x92\xE2\x82\x9C\xE2\x82\x90\xE2\x82\x97 \xE2\x88\x92 P(H\xE2\x82\x82O) = " + format_number(p / ATMOSPHERE, 5) + " \xE2\x88\x92 " + format_number(pw / ATMOSPHERE, 4) + " atm = " + format_number(dry / ATMOSPHERE, 5) + " atm, then n = PV/RT" + model;
            break;
        }
    }
    if (step) *step = name + ": " + format_number(n, 5) + " mol (" + how + ")";
    return n;
}

GasStoichiometryResult solve_gas_stoichiometry(const Reaction& input, const std::vector<GasKnown>& knowns, const GasVessel& vessel) {
    GasStoichiometryResult out;
    Reaction r = input;
    try {
        if (!r.is_balanced()) r.balance();
    } catch (const std::exception& ex) {
        out.message = std::string("couldn't balance the reaction: ") + ex.what();
        return out;
    }
    if (vessel.mode == GasVesselMode::Rigid && !(known(vessel.volume) && vessel.volume > 0.0)) {
        out.message = "a closed container needs its volume";
        return out;
    }
    std::vector<const ReactionTerm*> terms;
    for (const ReactionTerm& t : r.reactants()) terms.push_back(&t);
    for (const ReactionTerm& t : r.products()) terms.push_back(&t);
    const std::size_t nr = r.reactants().size();
    out.given_moles.assign(terms.size(), NO_VALUE);
    std::vector<StoichiometryKnown> moles;
    for (const GasKnown& k : knowns) {
        if (k.species >= terms.size()) continue;
        std::string step;
        const double n = gas_known_moles(k, terms[k.species]->formula, vessel, &step);
        if (!step.empty()) out.steps.push_back(step);
        if (!known(n)) continue;
        out.given_moles[k.species] = n;
        moles.push_back(StoichiometryKnown{ k.species, AmountKind::Moles, n, NO_VALUE });
    }
    if (moles.empty()) {
        out.message = "give an amount for at least one substance";
        return out;
    }
    out.stoichiometry = solve_stoichiometry(r, moles, vessel.temperature, vessel.pressure);
    if (!out.stoichiometry.ok) {
        out.message = out.stoichiometry.message;
        return out;
    }
    for (const std::string& s : out.stoichiometry.steps) {
        if (s.find("(from moles)") == std::string::npos) out.steps.push_back(s);
    }
    const double t = vessel.temperature;
    std::vector<VanDerWaalsConstants> vdw_before, vdw_after;
    const bool collect = vessel.over_water && vessel.mode == GasVesselMode::Open;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const StoichiometrySpecies& sp = out.stoichiometry.species[i];
        const GasBehavior b = gas_behavior(terms[i]->formula, t);
        if (!b.can_be_gas) continue;
        const bool is_water = terms[i]->formula.hill() == "H2O";
        if (collect && is_water) continue;
        GasSpeciesState g;
        g.formula = sp.formula;
        g.reactant = i < nr;
        g.vapor_pressure = b.condensable ? b.vapor_pressure : NO_VALUE;
        const double start = i < nr ? (known(out.given_moles[i]) ? out.given_moles[i] : sp.reacted_moles) : 0.0;
        if (i < nr && known(start) && start > 0.0) {
            GasSpeciesState before = g;
            before.moles = start;
            out.before.push_back(before);
            vdw_before.push_back(b.vdw);
        }
        if (known(sp.final_moles) && sp.final_moles > 1e-15) {
            g.moles = sp.final_moles;
            out.after.push_back(g);
            vdw_after.push_back(b.vdw);
        }
    }
    if (vessel.inert_moles > 0.0) {
        GasSpeciesState g;
        g.formula = vessel.inert_name;
        g.inert = true;
        g.moles = vessel.inert_moles;
        out.before.push_back(g);
        out.after.push_back(g);
        vdw_before.push_back(VanDerWaalsConstants{});
        vdw_after.push_back(VanDerWaalsConstants{});
    }
    double p_total = vessel.pressure;
    if (collect) {
        out.water_vapor = water_vapor_pressure(t);
        p_total = vessel.pressure - out.water_vapor;
        out.dry_gas_pressure = p_total;
        if (!(p_total > 0.0)) {
            out.message = "the water's vapor pressure is above the total pressure at this temperature";
            return out;
        }
    }
    cap_condensables(out.before, vessel.mode, t, p_total, vessel.volume);
    cap_condensables(out.after, vessel.mode, t, p_total, vessel.volume);
    fill_pressures(out.before, vdw_before, vessel.mode, vessel.model, t, p_total, vessel.volume, out.moles_before, out.pressure_before, out.volume_before);
    fill_pressures(out.after, vdw_after, vessel.mode, vessel.model, t, p_total, vessel.volume, out.moles_after, out.pressure_after, out.volume_after);
    if (collect) {
        out.pressure_before = vessel.pressure;
        out.pressure_after = vessel.pressure;
        out.notes.push_back("collected over water: the gas is saturated with water vapor at " + format_number(out.water_vapor / ATMOSPHERE, 4) + " atm, so the dry gas makes up only " + format_number(p_total / ATMOSPHERE, 5) + " atm of the " + format_number(vessel.pressure / ATMOSPHERE, 5) + " atm (Dalton's law)");
    }
    for (const GasSpeciesState& g : out.after) {
        if (g.condensed_moles > 1e-12) out.notes.push_back(g.formula + ": only " + format_number(g.moles, 4) + " mol can stay as vapor (its vapor pressure is " + format_number(g.vapor_pressure / ATMOSPHERE, 4) + " atm here); the other " + format_number(g.condensed_moles, 4) + " mol condenses");
    }
    if (vessel.mode == GasVesselMode::Rigid && known(out.pressure_before) && out.pressure_before > 0.0 && known(out.pressure_after)) {
        out.notes.push_back("in the closed container the pressure goes from " + format_number(out.pressure_before / ATMOSPHERE, 5) + " atm to " + format_number(out.pressure_after / ATMOSPHERE, 5) + " atm because the moles of gas go from " + format_number(out.moles_before, 5) + " to " + format_number(out.moles_after, 5));
    }
    if (vessel.model == GasModel::VanDerWaals) out.notes.push_back("real-gas (van der Waals) corrections use a and b from each gas's critical point; gases without data are treated as ideal");
    out.ok = true;
    return out;
}

} // namespace chemistry
} // namespace fizmo
