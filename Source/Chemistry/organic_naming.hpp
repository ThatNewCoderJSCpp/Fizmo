#ifndef FIZMO_CHEMISTRY_ORGANIC_NAMING_HPP
#define FIZMO_CHEMISTRY_ORGANIC_NAMING_HPP

#include "molecule.hpp"

#include <string>
#include <string_view>

namespace fizmo {
namespace chemistry {

struct OrganicName {
    bool ok = false;
    std::string name;
    std::string parent;
    std::string principal_group;
    std::string reason;
};

OrganicName name_organic_molecule(const Molecule& molecule);
std::string iupac_name(const Molecule& molecule);
bool molecule_from_condensed(std::string_view text, Molecule& out, std::string* reason = nullptr);
std::string iupac_name_from_condensed(std::string_view text);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ORGANIC_NAMING_HPP
