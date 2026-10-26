#include "solubility.hpp"

#include "acid_base.hpp"

#include <algorithm>

namespace fizmo {
namespace chemistry {

namespace {

bool cation_is(const IonDescriptor& c, int z, int charge) {
    return !c.polyatomic && c.element && c.element->atomic_number == z && c.charge == charge;
}

bool is_group1_or_ammonium(const IonDescriptor& c) {
    if (c.polyatomic) {
        const std::string_view f(c.formula);
        return f == "NH4" || f == "H3O" || f == "N(CH3)4" || f == "CH3NH3";
    }
    return c.element && c.element->group == 1 && c.element->atomic_number != 1;
}

bool anion_named(const IonDescriptor& a, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (a.name == n) return true;
    }
    return false;
}

std::string clean(std::string_view formula) {
    std::string s(trim(formula));
    const std::size_t p = s.rfind('(');
    if (p != std::string::npos && s.back() == ')') {
        const std::string inside = s.substr(p + 1, s.size() - p - 2);
        if (inside == "aq" || inside == "s" || inside == "l" || inside == "g") s = s.substr(0, p);
    }
    return s;
}

} // namespace

SolubilityResult solubility(const IonDescriptor& c, const IonDescriptor& a) {
    SolubilityResult r;
    const bool g1 = is_group1_or_ammonium(c);
    const bool silver = cation_is(c, 47, 1);
    const bool lead2 = cation_is(c, 82, 2);
    const bool mercury1 = c.polyatomic && c.formula == "Hg2";
    const bool copper1 = cation_is(c, 29, 1);
    const bool thallium1 = cation_is(c, 81, 1);
    const bool group2 = !c.polyatomic && c.element && c.element->group == 2;
    const int z = (!c.polyatomic && c.element) ? c.element->atomic_number : 0;
    if (anion_named(a, { "nitrate", "acetate", "chlorate", "perchlorate", "nitrite", "permanganate", "formate" })) {
        if (silver && a.name == "acetate") {
            r.solubility = Solubility::SlightlySoluble;
            r.rule = "acetates are soluble, but silver acetate only slightly";
            return r;
        }
        if (silver && a.name == "nitrite") {
            r.solubility = Solubility::SlightlySoluble;
            r.rule = "silver nitrite is only slightly soluble";
            return r;
        }
        r.solubility = Solubility::Soluble;
        r.rule = "nitrates, acetates, chlorates and perchlorates are soluble";
        return r;
    }
    if (g1) {
        r.solubility = Solubility::Soluble;
        r.rule = "compounds of group 1 metals and ammonium are soluble";
        return r;
    }
    if (anion_named(a, { "hydrogen carbonate" })) {
        r.solubility = Solubility::Soluble;
        r.rule = "hydrogen carbonates are generally soluble";
        return r;
    }
    if (anion_named(a, { "chloride", "bromide", "iodide" })) {
        if (silver || mercury1 || copper1) {
            r.solubility = Solubility::Insoluble;
            r.rule = "halides are soluble except with Ag+, Pb2+, Hg2^2+ and Cu+";
            return r;
        }
        if (lead2 || thallium1) {
            r.solubility = a.name == "iodide" ? Solubility::Insoluble : Solubility::SlightlySoluble;
            r.rule = "lead(II) chloride and bromide are slightly soluble; lead(II) iodide is insoluble";
            return r;
        }
        r.solubility = Solubility::Soluble;
        r.rule = "chlorides, bromides and iodides are soluble except with Ag+, Pb2+, Hg2^2+ and Cu+";
        return r;
    }
    if (a.name == "fluoride") {
        if (group2 && z != 4) {
            r.solubility = Solubility::Insoluble;
            r.rule = "fluorides of Mg2+, Ca2+, Sr2+ and Ba2+ are insoluble";
            return r;
        }
        if (lead2) {
            r.solubility = Solubility::Insoluble;
            r.rule = "lead(II) fluoride is insoluble";
            return r;
        }
        r.solubility = Solubility::Soluble;
        r.rule = "most fluorides are soluble";
        return r;
    }
    if (anion_named(a, { "sulfate", "hydrogen sulfate" })) {
        if (z == 56 || z == 38 || lead2 || mercury1) {
            r.solubility = Solubility::Insoluble;
            r.rule = "sulfates are soluble except with Ba2+, Sr2+, Pb2+ and Hg2^2+";
            return r;
        }
        if (z == 20 || silver) {
            r.solubility = Solubility::SlightlySoluble;
            r.rule = "calcium and silver sulfate are slightly soluble";
            return r;
        }
        r.solubility = Solubility::Soluble;
        r.rule = "most sulfates are soluble";
        return r;
    }
    if (a.name == "hydroxide") {
        if (z == 56 || z == 38) {
            r.solubility = Solubility::Soluble;
            r.rule = "hydroxides are insoluble except group 1, Ba2+ and Sr2+";
            return r;
        }
        if (z == 20) {
            r.solubility = Solubility::SlightlySoluble;
            r.rule = "calcium hydroxide is slightly soluble";
            return r;
        }
        r.solubility = Solubility::Insoluble;
        r.rule = "hydroxides are insoluble except group 1, NH4+, Ba2+ and Sr2+";
        return r;
    }
    if (a.name == "sulfide" || a.name == "hydrogen sulfide") {
        if (group2) {
            r.solubility = Solubility::Soluble;
            r.rule = "sulfides are insoluble except group 1, group 2 and NH4+";
            return r;
        }
        r.solubility = Solubility::Insoluble;
        r.rule = "sulfides are insoluble except group 1, group 2 and NH4+";
        return r;
    }
    if (a.name == "oxide" || a.name == "peroxide" || a.name == "superoxide") {
        if (group2 && z != 4 && z != 12) {
            r.solubility = Solubility::ReactsWithWater;
            r.rule = "oxides of the heavier group 2 metals react with water to form hydroxides";
            return r;
        }
        r.solubility = Solubility::Insoluble;
        r.rule = "metal oxides are insoluble (except group 1 and heavier group 2, which react with water)";
        return r;
    }
    if (anion_named(a, { "carbonate", "phosphate", "sulfite", "chromate", "oxalate", "metasilicate", "orthosilicate", "arsenate", "hydrogen phosphate", "borate", "fluoride" })) {
        if (a.name == "chromate" && (z == 12 || z == 20)) {
            r.solubility = Solubility::Soluble;
            r.rule = "magnesium and calcium chromate are soluble";
            return r;
        }
        r.solubility = Solubility::Insoluble;
        r.rule = "carbonates, phosphates, sulfites, chromates and oxalates are insoluble except group 1 and NH4+";
        return r;
    }
    if (a.name == "dihydrogen phosphate") {
        r.solubility = Solubility::Soluble;
        r.rule = "dihydrogen phosphates are generally soluble";
        return r;
    }
    if (a.name == "cyanide") {
        r.solubility = group2 ? Solubility::Soluble : Solubility::Insoluble;
        r.rule = "cyanides are insoluble except group 1, group 2 and NH4+";
        return r;
    }
    if (a.name == "thiocyanate") {
        r.solubility = (silver || mercury1 || copper1 || lead2) ? Solubility::Insoluble : Solubility::Soluble;
        r.rule = "thiocyanates are soluble except with Ag+, Hg2^2+, Cu+ and Pb2+";
        return r;
    }
    if (a.name == "nitride" || a.name == "phosphide" || a.name == "carbide" || a.name == "hydride") {
        r.solubility = Solubility::ReactsWithWater;
        r.rule = "ionic nitrides, phosphides, carbides and hydrides react with water";
        return r;
    }
    r.rule = "no general rule covers this pair";
    return r;
}

SolubilityResult solubility_of(std::string_view formula) {
    SolubilityResult r;
    const std::string f = clean(formula);
    if (is_strong_acid(f) || is_weak_acid(f)) {
        r.solubility = Solubility::Soluble;
        r.rule = "acids dissolve in water";
        return r;
    }
    const CompoundName name = name_compound(f);
    if (!name.ok) return r;
    if (name.kind == CompoundKind::Ionic || name.kind == CompoundKind::Base) {
        if (name.ions.size() >= 2) {
            if (name.ions.size() == 3) return solubility(name.ions[0], name.ions[2]);
            return solubility(name.ions[0], name.ions[1]);
        }
    }
    if (name.kind == CompoundKind::Acid) {
        r.solubility = Solubility::Soluble;
        r.rule = "acids dissolve in water";
        return r;
    }
    if (name.kind == CompoundKind::Carbohydrate) {
        r.solubility = Solubility::Soluble;
        r.rule = "small sugars are soluble (many OH groups hydrogen-bond with water)";
        return r;
    }
    return r;
}

bool is_soluble(std::string_view formula) {
    return solubility_of(formula).solubility == Solubility::Soluble;
}

bool is_strong_acid(std::string_view formula) {
    const std::string f = clean(formula);
    static const char* const strong[] = { "HCl", "HBr", "HI", "HNO3", "H2SO4", "HClO4", "HClO3", "HBrO3", "HIO4", "HMnO4", "HBF4", "HPF6", "H2SeO4", "HSbF6", "CF3SO3H", "HSO3F" };
    for (const char* s : strong) {
        if (f == s) return true;
    }
    return false;
}

bool is_strong_base(std::string_view formula) {
    const std::string f = clean(formula);
    static const char* const strong[] = { "LiOH", "NaOH", "KOH", "RbOH", "CsOH", "Ca(OH)2", "Sr(OH)2", "Ba(OH)2" };
    for (const char* s : strong) {
        if (f == s) return true;
    }
    return false;
}

bool is_weak_acid(std::string_view formula) {
    const std::string f = clean(formula);
    if (is_strong_acid(f)) return false;
    if (const AcidConstantEntry* a = find_acid(f)) return !a->strong;
    static const char* const weak[] = { "HF", "HNO2", "HClO", "HClO2", "HCN", "H2CO3", "H3PO4", "H2SO3", "H2S", "CH3COOH", "HCOOH", "H2C2O4", "H3BO3", "HOCl", "C6H5COOH", "C6H8O7", "H2SiO3", "HIO3", "HBrO", "HN3" };
    for (const char* s : weak) {
        if (f == s) return true;
    }
    return false;
}

bool is_weak_base(std::string_view formula) {
    const std::string f = clean(formula);
    if (find_base(f)) return true;
    static const char* const weak[] = { "NH3", "CH3NH2", "C5H5N", "N2H4", "C6H5NH2", "NH4OH", "(CH3)2NH", "(CH3)3N" };
    for (const char* s : weak) {
        if (f == s) return true;
    }
    return false;
}

bool is_strong_electrolyte(std::string_view formula) {
    const std::string f = clean(formula);
    if (is_strong_acid(f) || is_strong_base(f)) return true;
    if (is_weak_acid(f) || is_weak_base(f)) return false;
    const CompoundName name = name_compound(f);
    return name.ok && (name.kind == CompoundKind::Ionic || name.kind == CompoundKind::Base) && solubility_of(f).solubility == Solubility::Soluble;
}

} // namespace chemistry
} // namespace fizmo
