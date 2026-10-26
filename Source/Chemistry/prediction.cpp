#include "prediction.hpp"

#include "crystal.hpp"
#include "nomenclature.hpp"
#include "units.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace fizmo {
namespace chemistry {

namespace {

const int ACTIVITY[] = { 3, 19, 56, 38, 20, 11, 12, 13, 25, 30, 24, 26, 48, 27, 28, 50, 82, 1, 29, 80, 47, 78, 79 };

int aqueous_charge(const Element& e) {
    switch (e.atomic_number) {
        case 26: case 50: case 82: case 29: case 80: case 25: case 27: case 28: case 30: case 48: return 2;
        case 24: return 3;
        case 47: return 1;
        default: break;
    }
    const std::vector<int> q = likely_cation_charges(e);
    return q.empty() ? 1 : q.front();
}

struct Info {
    std::string text;
    std::string clean;
    Formula formula;
    Phase phase = Phase::Unknown;
    CompoundName name;
    const Element* element = nullptr;
    bool metal_element = false;
    bool nonmetal_element = false;
    bool noble = false;
    bool water = false;
    bool oxygen = false;
    bool hydrogen = false;
    bool organic = false;
    bool acid = false;
    bool strong_acid = false;
    bool base = false;
    bool ammonia = false;
    bool ionic = false;
    bool metal_oxide = false;
    bool nonmetal_oxide = false;
    bool halogen = false;
};

std::string strip_coefficient(std::string s) {
    s = std::string(trim(s));
    std::size_t i = 0;
    while (i < s.size() && (is_ascii_digit(s[i]) || s[i] == '.' || s[i] == ' ')) ++i;
    if (i > 0 && i < s.size() && (is_ascii_upper(s[i]) || s[i] == '(' || s[i] == '[')) return s.substr(i);
    return s;
}

std::string without_phase(const std::string& s, Phase& phase) {
    const Formula f = Formula::parse(s);
    phase = f.phase();
    std::string t(trim(s));
    for (const char* tag : { "(aq)", "(s)", "(l)", "(g)" }) {
        const std::string_view tv(tag);
        if (t.size() > tv.size() && std::string_view(t).substr(t.size() - tv.size()) == tv) return std::string(trim(std::string_view(t).substr(0, t.size() - tv.size())));
    }
    return t;
}

bool composition_is(const Formula& f, std::initializer_list<std::pair<int, double>> parts) {
    const auto comp = f.composition();
    if (comp.size() != parts.size()) return false;
    for (const auto& [z, n] : parts) {
        if (std::fabs(f.count(z) - n) > 1e-9) return false;
    }
    return true;
}

Info classify(const std::string& text) {
    Info r;
    r.text = text;
    r.clean = without_phase(strip_coefficient(text), r.phase);
    r.formula = Formula::parse(r.clean);
    const auto comp = r.formula.composition();
    if (comp.size() == 1) {
        r.element = comp.front().first;
        r.metal_element = r.element->is_metal();
        r.nonmetal_element = !r.metal_element;
        r.noble = r.element->is_noble_gas();
        r.oxygen = r.element->atomic_number == 8 && std::fabs(comp.front().second - 2.0) < 1e-9;
        r.hydrogen = r.element->atomic_number == 1;
        r.halogen = r.element->group == 17;
    }
    r.water = composition_is(r.formula, { { 1, 2.0 }, { 8, 1.0 } });
    r.ammonia = composition_is(r.formula, { { 7, 1.0 }, { 1, 3.0 } });
    r.strong_acid = is_strong_acid(r.clean);
    r.acid = r.strong_acid || is_weak_acid(r.clean);
    r.name = name_compound(r.clean, r.phase);
    if (!r.acid && r.name.kind == CompoundKind::Acid && r.name.ions.size() == 2) r.acid = true;
    if (r.acid && r.name.ions.size() < 2) {
        for (const PolyatomicIonEntry& p : polyatomic_ions()) {
            if (p.charge < 0 && r.clean == p.acid_formula && (p.charge == -1 || std::string_view(p.name).find("hydrogen") == std::string_view::npos)) {
                const int protons = -p.charge;
                bool fits = true;
                try {
                    Formula check = Formula::parse(p.formula);
                    check += Formula::parse("H" + std::to_string(protons));
                    fits = check.same_composition(r.formula);
                } catch (...) {
                    fits = false;
                }
                if (!fits) continue;
                r.name.ions = { monatomic_ion(PeriodicTable::element(1), 1), polyatomic_ion(p) };
                break;
            }
        }
    }
    r.base = r.name.kind == CompoundKind::Base || r.ammonia;
    r.ionic = (r.name.kind == CompoundKind::Ionic || r.name.kind == CompoundKind::Base) && r.name.ions.size() >= 2;
    const bool has_o = r.formula.contains(8);
    if (has_o && comp.size() == 2 && !r.water) {
        const Element* other = comp[0].first->atomic_number == 8 ? comp[1].first : comp[0].first;
        if (other->is_metal()) r.metal_oxide = true;
        else if (other->atomic_number != 1) r.nonmetal_oxide = true;
    }
    r.organic = r.formula.contains(6) && r.formula.contains(1) && !r.ionic && !r.acid;
    if (r.organic) {
        for (const auto& [e, n] : comp) {
            const int z = e->atomic_number;
            if (z != 6 && z != 1 && z != 8 && z != 7 && z != 16 && z != 9 && z != 17 && z != 35 && z != 53) r.organic = false;
        }
    }
    if (r.clean == "CH3COOH" || r.clean == "HCOOH" || r.clean == "C2H5COOH") {
        r.acid = true;
        r.organic = false;
    }
    return r;
}

bool is_gas_formula(const std::string& f) {
    static const char* const gases[] = { "H2", "O2", "N2", "F2", "Cl2", "CO2", "CO", "SO2", "NO", "NO2", "N2O", "NH3", "H2S", "CH4", "C2H6", "C3H8", "C2H4", "C2H2", "He", "Ne", "Ar", "Kr", "Xe", "Rn", "O3", "PH3", "HCN", "SiH4", "B2H6" };
    for (const char* g : gases) {
        if (f == g) return true;
    }
    return false;
}

std::string with_phase(const std::string& formula, bool aqueous) {
    if (formula == "H2O") return "H2O(l)";
    if (formula == "Br2" || formula == "Hg") return formula + "(l)";
    if (formula == "I2" || formula == "S" || formula == "S8" || formula == "P4" || formula == "C" || formula == "P4O10" || formula == "SiO2" || formula == "C6H12O6" || formula == "C12H22O11") return formula + "(s)";
    if (formula == "SO3") return formula + "(g)";
    if (is_gas_formula(formula)) return formula + "(g)";
    if (formula == "HCl" || formula == "HBr" || formula == "HI" || formula == "HF") return formula + (aqueous ? "(aq)" : "(g)");
    if (is_strong_acid(formula) || is_weak_acid(formula) || is_strong_base(formula)) return formula + (aqueous ? "(aq)" : "");
    try {
        const Formula f = Formula::parse(formula);
        const auto comp = f.composition();
        if (comp.size() == 1 && comp.front().first->is_metal()) return formula + "(s)";
    } catch (...) {
        return formula;
    }
    const CompoundName n = name_compound(formula);
    if ((n.kind == CompoundKind::Ionic || n.kind == CompoundKind::Base) && n.ions.size() >= 2) {
        if (!aqueous) return formula + "(s)";
        const SolubilityResult s = n.ions.size() == 3 ? solubility(n.ions[0], n.ions[2]) : solubility(n.ions[0], n.ions[1]);
        if (s.solubility == Solubility::Soluble) return formula + "(aq)";
        return formula + "(s)";
    }
    if (aqueous && (n.kind == CompoundKind::Carbohydrate)) return formula + "(aq)";
    return formula;
}

std::string reactant_text(const Info& i, bool aqueous) {
    if (i.phase != Phase::Unknown) return i.clean + "(" + phase_label(i.phase) + ")";
    return with_phase(i.clean, aqueous);
}

std::string ionic_formula_of(const IonDescriptor& c, const IonDescriptor& a) {
    return combine_ions(c, a).formula;
}

struct Builder {
    std::vector<std::string> left;
    std::vector<std::string> right;
    bool reversible = false;
};

PredictedReaction finish(const Builder& b, ReactionType type, const std::string& kind, const std::string& why) {
    PredictedReaction out;
    std::string eq;
    for (std::size_t i = 0; i < b.left.size(); ++i) eq += (i ? " + " : "") + b.left[i];
    eq += b.reversible ? " <=> " : " -> ";
    for (std::size_t i = 0; i < b.right.size(); ++i) eq += (i ? " + " : "") + b.right[i];
    try {
        Reaction r = Reaction::parse(eq);
        r.set_reversible(b.reversible);
        r.balance();
        out.reaction = r;
        out.ok = r.is_balanced();
    } catch (const std::exception& ex) {
        out.ok = false;
        out.explanation = std::string("predicted ") + eq + " but couldn't balance it: " + ex.what();
        return out;
    }
    out.type = type;
    out.kind = kind;
    out.explanation = why;
    const IonicEquation ie = ionic_equations(out.reaction);
    out.complete_ionic = ie.complete;
    out.net_ionic = ie.net;
    out.spectators = ie.spectators;
    return out;
}

PredictedReaction none(const std::string& why) {
    PredictedReaction out;
    out.ok = true;
    out.no_reaction = true;
    out.kind = "no reaction";
    out.explanation = why;
    return out;
}

std::string lower_name(const Element& e) {
    std::string s = e.name;
    for (char& c : s) c = ascii_lower(c);
    return s;
}

std::string covalent_binary(const Element& a, const Element& b) {
    const Element* pos = &a;
    const Element* neg = &b;
    if (value_or(a.electronegativity_pauling, 2.0) > value_or(b.electronegativity_pauling, 2.0)) std::swap(pos, neg);
    int qp = 0;
    for (const OxidationStateEntry& s : pos->oxidation_states()) {
        if (s.state > 0 && s.common) {
            qp = s.state;
            break;
        }
    }
    if (pos->atomic_number == 6 && neg->atomic_number == 8) qp = 4;
    if (pos->atomic_number == 16 && neg->atomic_number == 8) qp = 4;
    if (pos->atomic_number == 15 && neg->atomic_number == 8) qp = 5;
    if (pos->atomic_number == 7 && neg->atomic_number == 8) qp = 2;
    if (pos->atomic_number == 1) qp = 1;
    if (qp <= 0) qp = pos->highest_oxidation_state() > 0 ? pos->highest_oxidation_state() : 1;
    int qn = -likely_anion_charge(*neg);
    if (qn <= 0) qn = 1;
    int g = std::gcd(qp, qn);
    const int np = qn / g, nn = qp / g;
    if (pos->atomic_number == 1) return std::string(pos->symbol) + (np > 1 ? std::to_string(np) : "") + neg->symbol + (nn > 1 ? std::to_string(nn) : "");
    if (pos->atomic_number == 15 && neg->atomic_number == 8) return "P4O10";
    return std::string(pos->symbol) + (np > 1 ? std::to_string(np) : "") + neg->symbol + (nn > 1 ? std::to_string(nn) : "");
}

PredictedReaction combustion(const Info& fuel, const Info& o2) {
    Builder b;
    b.left = { reactant_text(fuel, false), "O2(g)" };
    b.right = { "CO2(g)", "H2O(l)" };
    if (fuel.formula.contains(7)) b.right.push_back("N2(g)");
    if (fuel.formula.contains(16)) b.right.push_back("SO2(g)");
    if (fuel.formula.contains(17)) b.right.push_back("HCl(g)");
    if (!fuel.formula.contains(6)) b.right.erase(b.right.begin());
    (void)o2;
    return finish(b, ReactionType::Combustion, "combustion", "A compound of carbon and hydrogen burns in oxygen to make carbon dioxide and water (complete combustion). With too little oxygen, carbon monoxide or soot forms instead.");
}

PredictedReaction two_elements(const Info& a, const Info& b) {
    const Element& ea = *a.element;
    const Element& eb = *b.element;
    if (a.noble || b.noble) {
        const Info& other = a.noble ? b : a;
        const Info& gas = a.noble ? a : b;
        if (gas.element->atomic_number == 54 && other.element->atomic_number == 9) {
            Builder bl;
            bl.left = { "Xe(g)", "F2(g)" };
            bl.right = { "XeF2(s)" };
            return finish(bl, ReactionType::Synthesis, "synthesis", "Xenon is one of the few noble gases that reacts, and only with very reactive fluorine (XeF4 and XeF6 form with more fluorine).");
        }
        return none(std::string(gas.element->name) + " is a noble gas with a full valence shell, so it doesn't react.");
    }
    Builder bl;
    bl.left = { reactant_text(a, false), reactant_text(b, false) };
    if (ea.is_metal() != eb.is_metal()) {
        const Element& metal = ea.is_metal() ? ea : eb;
        const Element& nonmetal = ea.is_metal() ? eb : ea;
        int qa = likely_anion_charge(nonmetal);
        if (qa == 0) qa = -1;
        int qc = likely_cation_charges(metal).front();
        if (metal.atomic_number == 26 && nonmetal.atomic_number == 8) qc = 3;
        if (metal.atomic_number == 26 && nonmetal.group == 17) qc = 3;
        const IonDescriptor cat = monatomic_ion(metal, qc);
        const IonDescriptor an = monatomic_ion(nonmetal, qa);
        std::string product = ionic_formula_of(cat, an);
        if (nonmetal.atomic_number == 8 && metal.group == 1 && metal.atomic_number >= 19) {
            product = metal.atomic_number == 19 || metal.atomic_number == 37 || metal.atomic_number == 55 ? std::string(metal.symbol) + "O2" : product;
        }
        bl.right = { product + "(s)" };
        const CompoundName n = name_compound(product);
        std::string why = std::string(metal.name) + " (a metal) gives electrons to " + lower_name(nonmetal) + " (a nonmetal), making the ionic compound " + n.name + ".";
        if (nonmetal.atomic_number == 8 && metal.group == 1 && metal.atomic_number >= 19) why += " The heavier alkali metals burn to superoxides like this one.";
        return finish(bl, ReactionType::Synthesis, "synthesis", why);
    }
    if (ea.is_metal() && eb.is_metal()) return none("Two metals don't react to form a compound; melted together they form an alloy (try the metallic sandbox).");
    const int za = ea.atomic_number, zb = eb.atomic_number;
    const auto has = [&](int x, int y) { return (za == x && zb == y) || (za == y && zb == x); };
    if (has(1, 8)) {
        bl.right = { "H2O(l)" };
        return finish(bl, ReactionType::Synthesis, "synthesis", "Hydrogen burns in oxygen to make water; it's very exothermic.");
    }
    if (has(1, 7)) {
        bl.right = { "NH3(g)" };
        bl.reversible = true;
        return finish(bl, ReactionType::Synthesis, "synthesis (Haber process)", "Nitrogen and hydrogen reach an equilibrium with ammonia; high pressure and an iron catalyst push it toward NH3.");
    }
    if (has(7, 8)) {
        bl.right = { "NO(g)" };
        bl.reversible = true;
        return finish(bl, ReactionType::Synthesis, "synthesis", "Nitrogen and oxygen only combine at very high temperatures (lightning, engines), reaching an equilibrium with nitric oxide.");
    }
    if (has(6, 8)) {
        bl.right = { "CO2(g)" };
        return finish(bl, ReactionType::Combustion, "combustion", "Carbon burns to carbon dioxide (carbon monoxide with limited oxygen).");
    }
    if (has(16, 8)) {
        bl.right = { "SO2(g)" };
        return finish(bl, ReactionType::Combustion, "combustion", "Sulfur burns with a blue flame to sulfur dioxide.");
    }
    if (ea.atomic_number == zb) return none("An element doesn't react with itself.");
    const std::string product = covalent_binary(ea, eb);
    const CompoundName n = name_compound(product);
    bl.right = { with_phase(product, false) };
    return finish(bl, ReactionType::Synthesis, "synthesis", "Two nonmetals share electrons to make the molecular compound " + n.name + ".");
}

PredictedReaction metal_with_water(const Info& metal, const Info& water) {
    const Element& m = *metal.element;
    (void)water;
    const bool cold = m.group == 1 || m.atomic_number == 20 || m.atomic_number == 38 || m.atomic_number == 56;
    if (cold) {
        Builder b;
        b.left = { std::string(m.symbol) + "(s)", "H2O(l)" };
        const IonDescriptor cat = monatomic_ion(m, m.group == 1 ? 1 : 2);
        const std::string hydroxide = ionic_formula_of(cat, polyatomic_ion(*find_polyatomic_ion("hydroxide")));
        b.right = { with_phase(hydroxide, true), "H2(g)" };
        return finish(b, ReactionType::SingleReplacement, "single replacement", std::string(m.name) + " is reactive enough to push hydrogen out of water, making a hydroxide and hydrogen gas.");
    }
    if (m.atomic_number == 12 || m.atomic_number == 13 || m.atomic_number == 30 || m.atomic_number == 26) {
        Builder b;
        const IonDescriptor cat = monatomic_ion(m, m.atomic_number == 13 ? 3 : 2);
        b.left = { std::string(m.symbol) + "(s)", "H2O(g)" };
        const std::string oxide = ionic_formula_of(cat, monatomic_ion(PeriodicTable::element(8), -2));
        b.right = { oxide + "(s)", "H2(g)" };
        PredictedReaction r = finish(b, ReactionType::SingleReplacement, "single replacement", std::string(m.name) + " doesn't react with cold water, but with steam it forms an oxide and hydrogen.");
        r.notes.push_back("only with steam (hot water vapor)");
        return r;
    }
    return none(std::string(m.name) + " isn't active enough to react with water.");
}

PredictedReaction metal_with_acid(const Info& metal, const Info& acid) {
    const Element& m = *metal.element;
    if (acid.name.ions.size() < 2) return none("couldn't split the acid into ions");
    const IonDescriptor anion = acid.name.ions[1];
    if (acid.clean == "HNO3") {
        if (metal_activity_rank(m) > metal_activity_rank(PeriodicTable::element(1)) && m.atomic_number != 79 && m.atomic_number != 78) {
            Builder b;
            b.left = { std::string(m.symbol) + "(s)", "HNO3(aq)" };
            const IonDescriptor cat = monatomic_ion(m, aqueous_charge(m));
            b.right = { with_phase(ionic_formula_of(cat, anion), true), "NO(g)", "H2O(l)" };
            return finish(b, ReactionType::Redox, "redox", "Nitric acid is an oxidizing acid: even metals below hydrogen dissolve, and the nitrate is reduced to nitric oxide (dilute acid; concentrated acid gives NO2).");
        }
    }
    if (metal_activity_rank(m) >= metal_activity_rank(PeriodicTable::element(1))) return none(std::string(m.name) + " is below hydrogen in the activity series, so it can't replace the hydrogen in " + acid.clean + ".");
    Builder b;
    b.left = { std::string(m.symbol) + "(s)", acid.clean + "(aq)" };
    const IonDescriptor cat = monatomic_ion(m, aqueous_charge(m));
    b.right = { with_phase(ionic_formula_of(cat, anion), true), "H2(g)" };
    return finish(b, ReactionType::SingleReplacement, "single replacement", std::string(m.name) + " is above hydrogen in the activity series, so it replaces the hydrogen in the acid and hydrogen gas bubbles off.");
}

PredictedReaction element_with_salt(const Info& el, const Info& salt) {
    const Element& e = *el.element;
    if (salt.name.ions.size() < 2) return none("couldn't split " + salt.clean + " into ions");
    const IonDescriptor cat = salt.name.ions.front();
    const IonDescriptor an = salt.name.ions.back();
    if (e.is_metal()) {
        if (cat.polyatomic || !cat.element) return none("the cation isn't a metal, so there's nothing to replace");
        if (!displaces(e, *cat.element)) return none(std::string(e.name) + " is less active than " + lower_name(*cat.element) + ", so it can't replace it (no reaction).");
        Builder b;
        b.left = { std::string(e.symbol) + "(s)", reactant_text(salt, true) };
        const IonDescriptor newcat = monatomic_ion(e, aqueous_charge(e));
        const std::string freed = cat.element->is_diatomic() ? std::string(cat.element->symbol) + "2" : std::string(cat.element->symbol);
        b.right = { with_phase(ionic_formula_of(newcat, an), true), with_phase(freed, false) };
        if (b.right.back() == freed) b.right.back() = freed + "(s)";
        return finish(b, ReactionType::SingleReplacement, "single replacement", std::string(e.name) + " is more active than " + lower_name(*cat.element) + ", so it gives electrons to the " + cat.text() + " ions and takes their place.");
    }
    if (e.group == 17) {
        if (an.polyatomic || !an.element || an.element->group != 17) return none("a halogen only replaces another halide ion");
        if (!displaces(e, *an.element)) return none(std::string(e.name) + " is less reactive than " + lower_name(*an.element) + ", so no reaction.");
        Builder b;
        b.left = { std::string(e.symbol) + "2" + (e.atomic_number == 35 ? "(l)" : e.atomic_number == 53 ? "(s)" : "(g)"), reactant_text(salt, true) };
        const IonDescriptor newan = monatomic_ion(e, -1);
        std::string freed = std::string(an.element->symbol) + "2";
        b.right = { with_phase(ionic_formula_of(cat, newan), true), freed + (an.element->atomic_number == 35 ? "(l)" : an.element->atomic_number == 53 ? "(s)" : "(g)") };
        return finish(b, ReactionType::SingleReplacement, "single replacement", std::string(e.name) + " is a stronger oxidizer than " + lower_name(*an.element) + ", so it takes the electrons from the halide ions.");
    }
    return none("no single-replacement rule covers " + el.clean + " with " + salt.clean);
}

std::optional<IonDescriptor> anion_of(const Info& i) {
    if (i.name.ions.size() >= 2) return i.name.ions.back();
    return std::nullopt;
}

std::optional<IonDescriptor> cation_of(const Info& i) {
    if (i.name.ions.size() >= 2) return i.name.ions.front();
    return std::nullopt;
}

bool anion_is(const IonDescriptor& a, const char* name) {
    return a.name == name;
}

PredictedReaction double_replacement(const Info& x, const Info& y) {
    auto c1 = cation_of(x), a1 = anion_of(x), c2 = cation_of(y), a2 = anion_of(y);
    if (!c1 || !a1 || !c2 || !a2) return none("couldn't split both compounds into ions");
    Builder b;
    b.left = { reactant_text(x, true), reactant_text(y, true) };
    std::vector<std::string> notes;
    ReactionType type = ReactionType::DoubleReplacement;
    std::string kind = "double replacement";
    std::string why;
    bool driving = false;
    const auto product_of = [&](const IonDescriptor& c, const IonDescriptor& a) {
        const bool proton = c.element && c.element->atomic_number == 1 && !c.polyatomic;
        if (proton && anion_is(a, "hydroxide")) {
            driving = true;
            type = ReactionType::AcidBase;
            kind = "neutralization";
            return std::vector<std::string>{ "H2O(l)" };
        }
        if (proton && (anion_is(a, "carbonate") || anion_is(a, "hydrogen carbonate"))) {
            driving = true;
            notes.push_back("the carbonic acid formed breaks down into water and carbon dioxide (fizzing)");
            return std::vector<std::string>{ "H2O(l)", "CO2(g)" };
        }
        if (proton && (anion_is(a, "sulfite") || anion_is(a, "hydrogen sulfite"))) {
            driving = true;
            notes.push_back("the sulfurous acid formed breaks down into water and sulfur dioxide");
            return std::vector<std::string>{ "H2O(l)", "SO2(g)" };
        }
        if (proton && (anion_is(a, "sulfide") || anion_is(a, "hydrogen sulfide"))) {
            driving = true;
            notes.push_back("hydrogen sulfide gas forms (rotten-egg smell)");
            return std::vector<std::string>{ "H2S(g)" };
        }
        if (c.polyatomic && c.formula == "NH4" && anion_is(a, "hydroxide")) {
            driving = true;
            notes.push_back("ammonium hydroxide breaks down into ammonia gas and water");
            return std::vector<std::string>{ "NH3(g)", "H2O(l)" };
        }
        const IonicCombination combo = combine_ions(c, a);
        std::string f = combo.formula;
        if (proton) {
            const bool strong = is_strong_acid(f);
            if (!strong) {
                driving = true;
                notes.push_back(f + " is a weak acid, so it stays mostly as molecules");
            }
            return std::vector<std::string>{ f + "(aq)" };
        }
        const std::string ph = with_phase(f, true);
        if (ph.size() >= 3 && ph.substr(ph.size() - 3) == "(s)") {
            driving = true;
            notes.push_back(f + " is insoluble, so it precipitates as a solid");
        }
        return std::vector<std::string>{ ph };
    };
    for (const std::string& s : product_of(*c1, *a2)) b.right.push_back(s);
    for (const std::string& s : product_of(*c2, *a1)) b.right.push_back(s);
    if (!driving) {
        PredictedReaction r = none("All the possible products stay dissolved as ions, so nothing actually happens (no precipitate, gas or water forms).");
        r.notes.push_back("mixing just gives a solution of " + c1->text() + ", " + a1->text() + ", " + c2->text() + " and " + a2->text());
        return r;
    }
    if (kind == "neutralization") why = "An acid and a base neutralize each other: H+ and OH- make water, and the other two ions form a salt.";
    else why = "The two compounds swap partners.";
    for (const std::string& n : notes) why += " " + std::string(1, static_cast<char>(ascii_upper(n[0]))) + n.substr(1) + ".";
    PredictedReaction r = finish(b, type, kind, why);
    r.notes = notes;
    return r;
}

PredictedReaction decomposition(const Info& x) {
    Builder b;
    b.left = { reactant_text(x, false) };
    const std::string f = x.clean;
    if (x.water) {
        b.right = { "H2(g)", "O2(g)" };
        return finish(b, ReactionType::Decomposition, "decomposition (electrolysis)", "Water only splits into hydrogen and oxygen when electricity (electrolysis) or very high heat drives it.");
    }
    if (f == "H2O2") {
        b.right = { "H2O(l)", "O2(g)" };
        return finish(b, ReactionType::Decomposition, "decomposition", "Hydrogen peroxide slowly breaks down into water and oxygen (fast with a catalyst like MnO2).");
    }
    if (f == "H2CO3") {
        b.right = { "H2O(l)", "CO2(g)" };
        b.reversible = true;
        return finish(b, ReactionType::Decomposition, "decomposition", "Carbonic acid is unstable and exists in equilibrium with water and carbon dioxide.");
    }
    if (f == "NH4Cl") {
        b.right = { "NH3(g)", "HCl(g)" };
        b.reversible = true;
        return finish(b, ReactionType::Decomposition, "decomposition", "Heating ammonium chloride splits it into ammonia and hydrogen chloride, which recombine on cooling.");
    }
    if (f == "NH4NO3") {
        b.right = { "N2O(g)", "H2O(g)" };
        return finish(b, ReactionType::Decomposition, "decomposition", "Gently heated ammonium nitrate gives nitrous oxide and water.");
    }
    if (f == "(NH4)2CO3") {
        b.right = { "NH3(g)", "H2O(g)", "CO2(g)" };
        return finish(b, ReactionType::Decomposition, "decomposition", "Ammonium carbonate (smelling salts) breaks down into ammonia, water and carbon dioxide.");
    }
    if (x.name.hydrate_waters > 0) {
        std::string base = f.substr(0, f.find_first_of("*.\xC2"));
        b.right = { base + "(s)", "H2O(g)" };
        return finish(b, ReactionType::Decomposition, "dehydration", "Heating a hydrate drives off its water of crystallization.");
    }
    if (x.ionic && x.name.ions.size() >= 2) {
        const IonDescriptor c = x.name.ions.front();
        const IonDescriptor a = x.name.ions.back();
        const IonDescriptor oxide = monatomic_ion(PeriodicTable::element(8), -2);
        if (anion_is(a, "carbonate") && c.element) {
            if (c.element->group == 1 && c.element->atomic_number != 3) return none("Group 1 carbonates (except lithium) don't decompose at ordinary heating temperatures.");
            b.right = { ionic_formula_of(c, oxide) + "(s)", "CO2(g)" };
            return finish(b, ReactionType::Decomposition, "decomposition", "Heating a metal carbonate drives off carbon dioxide and leaves the metal oxide (like limestone becoming quicklime).");
        }
        if (anion_is(a, "hydrogen carbonate")) {
            b.right = { ionic_formula_of(c, polyatomic_ion(*find_polyatomic_ion("carbonate"))) + "(s)", "H2O(g)", "CO2(g)" };
            return finish(b, ReactionType::Decomposition, "decomposition", "Hydrogen carbonates break down when heated into the carbonate, water and carbon dioxide (why baking soda makes cakes rise).");
        }
        if (anion_is(a, "chlorate")) {
            b.right = { ionic_formula_of(c, monatomic_ion(PeriodicTable::element(17), -1)) + "(s)", "O2(g)" };
            return finish(b, ReactionType::Decomposition, "decomposition", "Heated chlorates release oxygen and leave the chloride.");
        }
        if (anion_is(a, "nitrate") && c.element) {
            if (c.element->group == 1 && c.element->atomic_number != 3) {
                b.right = { ionic_formula_of(c, polyatomic_ion(*find_polyatomic_ion("nitrite"))) + "(s)", "O2(g)" };
                return finish(b, ReactionType::Decomposition, "decomposition", "Group 1 nitrates lose oxygen on heating to give nitrites.");
            }
            b.right = { ionic_formula_of(c, oxide) + "(s)", "NO2(g)", "O2(g)" };
            return finish(b, ReactionType::Decomposition, "decomposition", "Most metal nitrates break down into the metal oxide, brown nitrogen dioxide and oxygen.");
        }
        if (anion_is(a, "hydroxide") && c.element && c.element->group != 1) {
            b.right = { ionic_formula_of(c, oxide) + "(s)", "H2O(g)" };
            return finish(b, ReactionType::Decomposition, "decomposition", "Heating a metal hydroxide drives off water and leaves the oxide.");
        }
        if (!c.polyatomic && !a.polyatomic && c.element && a.element) {
            const std::string metal = c.element->is_diatomic() ? std::string(c.element->symbol) + "2" : std::string(c.element->symbol);
            const std::string nonmetal = a.element->is_diatomic() ? std::string(a.element->symbol) + "2" : std::string(a.element->symbol);
            b.right = { with_phase(metal, false), with_phase(nonmetal, false) };
            return finish(b, ReactionType::Decomposition, "decomposition (electrolysis)", "A binary ionic compound can be split into its elements by electrolysis of the melt.");
        }
    }
    const auto comp = x.formula.composition();
    if (comp.size() == 2 && !x.organic) {
        std::vector<std::string> elems;
        for (const auto& [e, n] : comp) elems.push_back(e->is_diatomic() ? std::string(e->symbol) + "2" : std::string(e->symbol));
        b.right = { with_phase(elems[0], false), with_phase(elems[1], false) };
        return finish(b, ReactionType::Decomposition, "decomposition", "A binary compound breaks down into its elements (this usually needs a lot of energy).");
    }
    if (x.organic) return none("An organic compound on its own needs oxygen to react; add O2 for combustion.");
    return none("no decomposition rule covers " + f);
}

std::string ester_from(const std::string& acid, const std::string& alcohol) {
    const std::size_t a = acid.find("COOH");
    const std::size_t o = alcohol.rfind("OH");
    if (a == std::string::npos || o == std::string::npos || a + 4 != acid.size() || o + 2 != alcohol.size()) return std::string();
    return acid.substr(0, a) + "COO" + alcohol.substr(0, o);
}

} // namespace

std::string PredictedReaction::equation(bool unicode) const {
    if (no_reaction) return "no reaction";
    return unicode ? reaction.to_unicode_string() : reaction.to_string();
}

int metal_activity_rank(const Element& e) noexcept {
    int i = 0;
    for (int z : ACTIVITY) {
        if (z == e.atomic_number) return i;
        ++i;
    }
    if (e.group == 1) return 0;
    if (e.group == 2) return 4;
    if (e.is_lanthanide()) return 6;
    if (e.atomic_number == 46 || e.atomic_number == 77 || e.atomic_number == 76 || e.atomic_number == 44 || e.atomic_number == 45) return 21;
    return 12;
}

int halogen_activity_rank(const Element& e) noexcept {
    switch (e.atomic_number) {
        case 9: return 0;
        case 17: return 1;
        case 35: return 2;
        case 53: return 3;
        case 85: return 4;
        default: return 9;
    }
}

bool displaces(const Element& free_element, const Element& combined) noexcept {
    if (free_element.group == 17 && combined.group == 17) return halogen_activity_rank(free_element) < halogen_activity_rank(combined);
    return metal_activity_rank(free_element) < metal_activity_rank(combined);
}

std::vector<const Element*> metal_activity_series() {
    std::vector<const Element*> out;
    for (int z : ACTIVITY) out.push_back(&PeriodicTable::element(z));
    return out;
}

std::vector<std::string> split_reactants(std::string_view text) {
    std::vector<std::string> out;
    std::string t(trim(text));
    for (const char* arrow : { "->", "=>", "<=>", "<->", "\xE2\x86\x92", "\xE2\x87\x8C", "=" }) {
        const std::size_t p = t.find(arrow);
        if (p != std::string::npos) {
            t = t.substr(0, p);
            break;
        }
    }
    if (t.find(" + ") != std::string::npos) {
        std::size_t start = 0;
        while (true) {
            const std::size_t p = t.find(" + ", start);
            out.push_back(std::string(trim(std::string_view(t).substr(start, p == std::string::npos ? std::string::npos : p - start))));
            if (p == std::string::npos) break;
            start = p + 3;
        }
    } else {
        std::size_t start = 0;
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (t[i] != '+') continue;
            if (i > 0 && t[i - 1] == '^') continue;
            if (i + 1 < t.size() && (is_ascii_upper(t[i + 1]) || is_ascii_digit(t[i + 1]) || t[i + 1] == '(' || t[i + 1] == '[')) {
                out.push_back(std::string(trim(std::string_view(t).substr(start, i - start))));
                start = i + 1;
            }
        }
        out.push_back(std::string(trim(std::string_view(t).substr(start))));
    }
    out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& s) { return s.empty(); }), out.end());
    return out;
}

PredictedReaction predict_products(std::string_view reactants) {
    return predict_products(split_reactants(reactants));
}

PredictedReaction predict_products(const std::vector<std::string>& texts) {
    PredictedReaction out;
    if (texts.empty()) {
        out.explanation = "type at least one reactant";
        return out;
    }
    std::vector<Info> r;
    try {
        for (const std::string& t : texts) r.push_back(classify(t));
    } catch (const std::exception& ex) {
        out.explanation = std::string("couldn't read a reactant: ") + ex.what();
        return out;
    }
    if (r.size() == 1) return decomposition(r[0]);
    if (r.size() == 2) {
        Info a = r[0], b = r[1];
        const auto either = [&](auto pred) { return pred(a) || pred(b); };
        const auto pick = [&](auto pred) -> std::pair<Info, Info> { return pred(a) ? std::make_pair(a, b) : std::make_pair(b, a); };
        if (either([](const Info& i) { return i.oxygen; })) {
            auto [o2, other] = pick([](const Info& i) { return i.oxygen; });
            if (other.organic || (other.formula.contains(6) && other.formula.contains(1) && !other.ionic)) return combustion(other, o2);
        }
        if (a.element && b.element) return two_elements(a, b);
        if (either([](const Info& i) { return i.element != nullptr; })) {
            auto [el, other] = pick([](const Info& i) { return i.element != nullptr; });
            if (el.metal_element && other.water) return metal_with_water(el, other);
            if (el.metal_element && other.acid) return metal_with_acid(el, other);
            if ((el.metal_element || el.halogen) && other.ionic) return element_with_salt(el, other);
            if (el.oxygen && other.formula.contains(1) && other.formula.composition().size() == 1) return two_elements(el, other);
            return none("no rule covers " + el.clean + " with " + other.clean + "; try entering both sides in the balancer");
        }
        if (either([](const Info& i) { return i.water; })) {
            auto [water, other] = pick([](const Info& i) { return i.water; });
            if (other.metal_oxide) {
                const IonDescriptor c = other.name.ions.front();
                if (c.element && (c.element->group == 1 || (c.element->group == 2 && c.element->atomic_number >= 20))) {
                    Builder bl;
                    bl.left = { reactant_text(other, false), "H2O(l)" };
                    bl.right = { with_phase(combine_ions(c, polyatomic_ion(*find_polyatomic_ion("hydroxide"))).formula, true) };
                    return finish(bl, ReactionType::Synthesis, "synthesis", "Basic oxides of active metals react with water to make hydroxides (bases).");
                }
                return none(other.clean + " is a metal oxide that doesn't dissolve or react with water.");
            }
            if (other.nonmetal_oxide) {
                static const struct {
                    const char* oxide;
                    const char* acid;
                    bool reversible;
                } table[] = { { "CO2", "H2CO3", true }, { "SO2", "H2SO3", true }, { "SO3", "H2SO4", false }, { "N2O5", "HNO3", false }, { "P4O10", "H3PO4", false }, { "N2O3", "HNO2", false }, { "Cl2O7", "HClO4", false }, { "SeO3", "H2SeO4", false } };
                for (const auto& row : table) {
                    if (other.clean == row.oxide) {
                        Builder bl;
                        bl.left = { reactant_text(other, false), "H2O(l)" };
                        bl.right = { std::string(row.acid) + "(aq)" };
                        bl.reversible = row.reversible;
                        return finish(bl, ReactionType::Synthesis, "synthesis", "Nonmetal oxides are acidic: with water they form an oxyacid.");
                    }
                }
                if (other.clean == "NO2") {
                    Builder bl;
                    bl.left = { "NO2(g)", "H2O(l)" };
                    bl.right = { "HNO3(aq)", "NO(g)" };
                    return finish(bl, ReactionType::Redox, "redox (disproportionation)", "Nitrogen dioxide disproportionates in water into nitric acid and nitric oxide.");
                }
            }
            if (other.acid) {
                Builder bl;
                if (other.name.ions.size() < 2) return none("couldn't split the acid into ions");
                const IonDescriptor an = other.name.ions.back();
                bl.left = { other.clean + (other.strong_acid ? "(aq)" : "(l)"), "H2O(l)" };
                bl.right = { "H3O^+(aq)", an.text() + "(aq)" };
                bl.reversible = !other.strong_acid;
                if (an.charge < -1) bl.right[1] = combine_ions(monatomic_ion(PeriodicTable::element(1), 1), an).formula;
                if (an.charge < -1) {
                    std::string hx;
                    for (const PolyatomicIonEntry& p : polyatomic_ions()) {
                        if (p.charge == an.charge + 1 && std::string_view(p.acid) == other.name.name) {
                            hx = ion_formula_text(p, false);
                            break;
                        }
                    }
                    if (!hx.empty()) bl.right[1] = hx + "(aq)";
                }
                return finish(bl, ReactionType::AcidBase, other.strong_acid ? "ionization (strong acid)" : "ionization (weak acid)", other.strong_acid ? "A strong acid gives its proton to water completely." : "A weak acid gives only a small fraction of its protons to water, so it's an equilibrium.");
            }
            if (other.ammonia || is_weak_base(other.clean)) {
                Builder bl;
                bl.left = { other.clean + "(aq)", "H2O(l)" };
                bl.right = { other.ammonia ? "NH4^+(aq)" : other.clean + "H^+(aq)", "OH^-(aq)" };
                bl.reversible = true;
                return finish(bl, ReactionType::AcidBase, "ionization (weak base)", "A weak base takes a proton from water, making hydroxide ions; it's an equilibrium.");
            }
            if (other.ionic) {
                Builder bl;
                bl.left = { reactant_text(other, false) };
                const IonDescriptor c = other.name.ions.front();
                const IonDescriptor an = other.name.ions.back();
                const SolubilityResult s = solubility(c, an);
                if (s.solubility != Solubility::Soluble) return none(other.clean + " is " + to_string(s.solubility) + " in water (" + s.rule + ").");
                const IonicCombination combo = combine_ions(c, an);
                bl.left.back() = other.clean + "(s)";
                bl.right = { c.text() + "(aq)", an.text() + "(aq)" };
                (void)combo;
                return finish(bl, ReactionType::Other, "dissolving", "A soluble ionic compound separates into its ions in water.");
            }
            return none(other.clean + " doesn't react with water by any rule here.");
        }
        if (a.acid && b.ammonia) std::swap(a, b);
        if (a.ammonia && b.acid) {
            Builder bl;
            if (b.name.ions.size() < 2) return none("couldn't split the acid into ions");
            const IonDescriptor nh4 = polyatomic_ion(*find_polyatomic_ion("ammonium"));
            bl.left = { "NH3(aq)", b.clean + "(aq)" };
            bl.right = { with_phase(combine_ions(nh4, b.name.ions.back()).formula, true) };
            return finish(bl, ReactionType::AcidBase, "neutralization", "Ammonia takes a proton from the acid, forming an ammonium salt.");
        }
        if ((a.metal_oxide && b.acid) || (b.metal_oxide && a.acid)) {
            const Info& ox = a.metal_oxide ? a : b;
            const Info& ac = a.metal_oxide ? b : a;
            Builder bl;
            bl.left = { reactant_text(ox, false), ac.clean + "(aq)" };
            bl.right = { with_phase(combine_ions(ox.name.ions.front(), ac.name.ions.back()).formula, true), "H2O(l)" };
            return finish(bl, ReactionType::AcidBase, "neutralization", "A basic metal oxide neutralizes an acid, making a salt and water.");
        }
        if ((a.nonmetal_oxide && b.base) || (b.nonmetal_oxide && a.base)) {
            const Info& ox = a.nonmetal_oxide ? a : b;
            const Info& ba = a.nonmetal_oxide ? b : a;
            static const struct {
                const char* oxide;
                const char* anion;
            } table[] = { { "CO2", "carbonate" }, { "SO2", "sulfite" }, { "SO3", "sulfate" }, { "N2O5", "nitrate" }, { "P4O10", "phosphate" } };
            for (const auto& row : table) {
                if (ox.clean == row.oxide && ba.name.ions.size() >= 2) {
                    Builder bl;
                    bl.left = { reactant_text(ox, false), reactant_text(ba, true) };
                    bl.right = { with_phase(combine_ions(ba.name.ions.front(), polyatomic_ion(*find_polyatomic_ion(row.anion))).formula, true), "H2O(l)" };
                    return finish(bl, ReactionType::AcidBase, "neutralization", "An acidic oxide reacts with a base to make a salt and water.");
                }
            }
        }
        if ((a.metal_oxide && b.nonmetal_oxide) || (b.metal_oxide && a.nonmetal_oxide)) {
            const Info& mo = a.metal_oxide ? a : b;
            const Info& no = a.metal_oxide ? b : a;
            static const struct {
                const char* oxide;
                const char* anion;
            } table[] = { { "CO2", "carbonate" }, { "SO2", "sulfite" }, { "SO3", "sulfate" }, { "SiO2", "metasilicate" }, { "P4O10", "phosphate" } };
            for (const auto& row : table) {
                if (no.clean == row.oxide) {
                    Builder bl;
                    bl.left = { reactant_text(mo, false), reactant_text(no, false) };
                    bl.right = { combine_ions(mo.name.ions.front(), polyatomic_ion(*find_polyatomic_ion(row.anion))).formula + "(s)" };
                    return finish(bl, ReactionType::Synthesis, "synthesis", "A basic oxide and an acidic oxide combine directly into a salt.");
                }
            }
        }
        if ((a.ionic || a.acid) && (b.ionic || b.acid)) return double_replacement(a, b);
        const auto is_alcohol = [](const Info& i) { return i.organic && i.clean.size() > 2 && i.clean.substr(i.clean.size() - 2) == "OH" && i.clean.find("COOH") == std::string::npos; };
        const auto is_carboxylic = [](const Info& i) { return i.clean.size() > 4 && i.clean.substr(i.clean.size() - 4) == "COOH"; };
        if ((is_alcohol(a) && is_carboxylic(b)) || (is_alcohol(b) && is_carboxylic(a))) {
            const Info& acid = is_carboxylic(a) ? a : b;
            const Info& alc = is_carboxylic(a) ? b : a;
            const std::string ester = ester_from(acid.clean, alc.clean);
            if (!ester.empty()) {
                Builder bl;
                bl.left = { acid.clean + "(l)", alc.clean + "(l)" };
                bl.right = { ester + "(l)", "H2O(l)" };
                bl.reversible = true;
                PredictedReaction pr = finish(bl, ReactionType::Other, "esterification", "A carboxylic acid and an alcohol join, losing water, to make an ester (with an acid catalyst; it's an equilibrium).");
                pr.notes.push_back("ester: " + name_compound(ester).name);
                return pr;
            }
        }
    }
    if (r.size() == 3) {
        int fuel = -1;
        bool o2 = false;
        for (std::size_t i = 0; i < r.size(); ++i) {
            if (r[i].oxygen) o2 = true;
            else if (r[i].organic) fuel = static_cast<int>(i);
        }
        if (o2 && fuel >= 0) return combustion(r[static_cast<std::size_t>(fuel)], r[0]);
    }
    out.explanation = "None of the reaction rules here match these reactants. Enter both sides in the full balancer instead.";
    return out;
}

IonicEquation ionic_equations(const Reaction& reaction) {
    IonicEquation out;
    struct Piece {
        std::string text;
        double count;
    };
    const auto expand = [](const ReactionTerm& t) {
        std::vector<Piece> pieces;
        const std::string disp = t.formula.display_string(false);
        const std::string pretty = t.formula.display_string(true);
        bool split = false;
        if (t.formula.phase() == Phase::Aqueous && t.formula.charge() == 0) {
            if (is_strong_acid(disp) || is_strong_base(disp) || is_soluble(disp)) {
                const CompoundName n = name_compound(disp);
                if (n.ions.size() >= 2) {
                    const IonDescriptor& c = n.ions.front();
                    const IonDescriptor& a = n.ions.back();
                    if (n.ions.size() == 2) {
                        const IonicCombination combo = combine_ions(c, a);
                        if (combo.cation_count > 0 && combo.anion_count > 0) {
                            pieces.push_back({ c.text(true) + "(aq)", t.coefficient * combo.cation_count });
                            pieces.push_back({ a.text(true) + "(aq)", t.coefficient * combo.anion_count });
                            split = true;
                        }
                    }
                }
            }
        }
        if (!split) {
            std::string s = pretty;
            if (t.formula.phase() != Phase::Unknown) s += std::string("(") + phase_label(t.formula.phase()) + ")";
            pieces.push_back({ s, t.coefficient });
        }
        return pieces;
    };
    std::map<std::string, double> left, right;
    std::vector<std::string> order_left, order_right;
    for (const ReactionTerm& t : reaction.reactants()) {
        for (const Piece& p : expand(t)) {
            if (!left.count(p.text)) order_left.push_back(p.text);
            left[p.text] += p.count;
        }
    }
    for (const ReactionTerm& t : reaction.products()) {
        for (const Piece& p : expand(t)) {
            if (!right.count(p.text)) order_right.push_back(p.text);
            right[p.text] += p.count;
        }
    }
    const char* arrow = reaction.reversible() ? " \xE2\x87\x8C " : " \xE2\x86\x92 ";
    const auto side_text = [](const std::vector<std::string>& order, const std::map<std::string, double>& side) {
        std::string s;
        for (const std::string& k : order) {
            const double c = side.at(k);
            if (c <= 1e-9) continue;
            if (!s.empty()) s += " + ";
            if (std::fabs(c - 1.0) > 1e-9) s += format_number(c, 6) + " ";
            s += k;
        }
        return s;
    };
    out.complete = side_text(order_left, left) + arrow + side_text(order_right, right);
    for (auto& [k, v] : left) {
        auto it = right.find(k);
        if (it == right.end()) continue;
        const double m = std::min(v, it->second);
        if (m > 1e-9) {
            out.spectators.push_back(k);
            v -= m;
            it->second -= m;
        }
    }
    double g = 0.0;
    bool integral = true;
    for (const auto* side : { &left, &right }) {
        for (const auto& [k, v] : *side) {
            if (v <= 1e-9) continue;
            if (std::fabs(v - std::round(v)) > 1e-9) integral = false;
            g = g == 0.0 ? std::round(v) : static_cast<double>(std::gcd(static_cast<long long>(std::round(g)), static_cast<long long>(std::round(v))));
        }
    }
    if (integral && g > 1.0) {
        for (auto* side : { &left, &right }) {
            for (auto& [k, v] : *side) v /= g;
        }
    }
    const std::string l = side_text(order_left, left), r = side_text(order_right, right);
    if (l.empty() || r.empty()) {
        out.no_net_reaction = true;
        out.net = "no net reaction";
    } else {
        out.net = l + arrow + r;
    }
    return out;
}

} // namespace chemistry
} // namespace fizmo
