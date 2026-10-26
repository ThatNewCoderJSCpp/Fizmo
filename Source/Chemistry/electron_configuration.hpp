#ifndef FIZMO_CHEMISTRY_ELECTRON_CONFIGURATION_HPP
#define FIZMO_CHEMISTRY_ELECTRON_CONFIGURATION_HPP

#include "periodic_table.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace chemistry {

struct QuantumNumbers {
    int n = 0;
    int l = 0;
    int ml = 0;
    double ms = 0.5;
};

class ElectronConfiguration {
public:
    ElectronConfiguration() = default;
    explicit ElectronConfiguration(std::vector<Subshell> subshells);

    static ElectronConfiguration ground_state(const Element& element);
    static ElectronConfiguration of_ion(const Element& element, int charge);
    static ElectronConfiguration aufbau(int electrons);
    static ElectronConfiguration parse(std::string_view text);
    static std::vector<std::pair<int, int>> madelung_order(int max_n = 8);

    const std::vector<Subshell>& subshells() const noexcept { return m_subshells; }
    int electrons() const noexcept;
    int occupancy(int n, int l) const noexcept;
    int shell_occupancy(int n) const noexcept;
    std::vector<int> shells() const;
    int highest_shell() const noexcept;
    int unpaired_electrons() const noexcept;
    bool is_paramagnetic() const noexcept { return unpaired_electrons() > 0; }
    bool is_closed_shell() const noexcept;
    int open_subshell_count() const noexcept;
    bool is_noble_gas_like() const noexcept;

    bool add_electron();
    bool remove_electron();
    ElectronConfiguration ionized(int charge) const;

    TermSymbol hund_term() const;
    QuantumNumbers last_electron() const;
    int parity() const noexcept;

    double slater_shielding(int n, int l) const noexcept;
    double effective_nuclear_charge(int atomic_number, int n, int l) const noexcept;
    double effective_nuclear_charge(int atomic_number) const noexcept;

    std::string to_string() const;
    std::string to_noble_gas_string() const;
    std::string to_unicode_string(bool noble_gas_core = true) const;
    std::string shell_string() const;

    bool operator==(const ElectronConfiguration& o) const noexcept { return m_subshells == o.m_subshells; }
    bool operator!=(const ElectronConfiguration& o) const noexcept { return !(*this == o); }

private:
    void normalize();
    std::vector<Subshell> m_subshells;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_ELECTRON_CONFIGURATION_HPP
