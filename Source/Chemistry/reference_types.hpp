#ifndef FIZMO_CHEMISTRY_REFERENCE_TYPES_HPP
#define FIZMO_CHEMISTRY_REFERENCE_TYPES_HPP

#include "common.hpp"

#include <cstdint>

namespace fizmo {
namespace chemistry {

struct BondEnergyEntry {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t order = 1;
    double energy = NO_VALUE;
};

struct BondLengthEntry {
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t order = 1;
    double length = NO_VALUE;
};

struct ReductionPotentialEntry {
    const char* couple = "";
    const char* half_reaction = "";
    int electrons = 1;
    double potential = NO_VALUE;
};

struct AcidConstantEntry {
    const char* name = "";
    const char* formula = "";
    double pka1 = NO_VALUE;
    double pka2 = NO_VALUE;
    double pka3 = NO_VALUE;
    bool strong = false;

    constexpr int protons() const noexcept { return pka3 == pka3 ? 3 : (pka2 == pka2 ? 2 : 1); }
    constexpr double pka(int n) const noexcept { return n == 1 ? pka1 : (n == 2 ? pka2 : (n == 3 ? pka3 : NO_VALUE)); }
};

struct PolyatomicIonEntry {
    const char* formula = "";
    int charge = 0;
    const char* name = "";
    const char* other_names = "";
    const char* acid = "";
    const char* acid_formula = "";
    const char* category = "";
    const char* smiles = "";
};

struct CommonCompoundEntry {
    const char* formula = "";
    const char* name = "";
    const char* other_names = "";
    const char* category = "";
    const char* smiles = "";
};

struct BaseConstantEntry {
    const char* name = "";
    const char* formula = "";
    double pkb = NO_VALUE;

    constexpr double pka_of_conjugate_acid(double pkw = 14.0) const noexcept { return pkw - pkb; }
};

struct MadelungEntry {
    const char* structure = "";
    const char* example = "";
    double constant = NO_VALUE;
    int ions_per_formula = 2;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_REFERENCE_TYPES_HPP
