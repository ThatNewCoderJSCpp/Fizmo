#ifndef FIZMO_CHEMISTRY_CONSTANTS_HPP
#define FIZMO_CHEMISTRY_CONSTANTS_HPP

namespace fizmo {
namespace chemistry {

inline constexpr double PI = 3.141592653589793238462643383279502884;
inline constexpr double LN2 = 0.693147180559945309417232121458176568;
inline constexpr double LN10 = 2.302585092994045684017991454684364208;

inline constexpr double SPEED_OF_LIGHT = 299792458.0;
inline constexpr double PLANCK_CONSTANT = 6.62607015e-34;
inline constexpr double REDUCED_PLANCK_CONSTANT = 1.054571817646156e-34;
inline constexpr double ELEMENTARY_CHARGE = 1.602176634e-19;
inline constexpr double BOLTZMANN_CONSTANT = 1.380649e-23;
inline constexpr double AVOGADRO_CONSTANT = 6.02214076e23;
inline constexpr double GAS_CONSTANT = AVOGADRO_CONSTANT * BOLTZMANN_CONSTANT;
inline constexpr double FARADAY_CONSTANT = AVOGADRO_CONSTANT * ELEMENTARY_CHARGE;
inline constexpr double VACUUM_PERMITTIVITY = 8.8541878188e-12;
inline constexpr double COULOMB_CONSTANT = 1.0 / (4.0 * PI * VACUUM_PERMITTIVITY);
inline constexpr double FINE_STRUCTURE_CONSTANT = 7.2973525643e-3;
inline constexpr double STEFAN_BOLTZMANN_CONSTANT = 5.670374419184429e-8;
inline constexpr double GRAVITATIONAL_ACCELERATION = 9.80665;

inline constexpr double ATOMIC_MASS_CONSTANT = 1.66053906892e-27;
inline constexpr double MOLAR_MASS_CONSTANT = 1.00000000105e-3;
inline constexpr double ATOMIC_MASS_UNIT_MEV = 931.49410372;
inline constexpr double ELECTRON_MASS = 9.1093837139e-31;
inline constexpr double PROTON_MASS = 1.67262192595e-27;
inline constexpr double NEUTRON_MASS = 1.67492750056e-27;
inline constexpr double ELECTRON_MASS_U = 5.485799090441e-4;
inline constexpr double PROTON_MASS_U = 1.0072764665789;
inline constexpr double NEUTRON_MASS_U = 1.00866491606;
inline constexpr double HYDROGEN_ATOM_MASS_U = 1.007825031898;
inline constexpr double ELECTRON_MASS_MEV = 0.51099895069;
inline constexpr double BOHR_RADIUS = 5.29177210544e-11;
inline constexpr double HARTREE_ENERGY = 4.3597447222060e-18;
inline constexpr double RYDBERG_CONSTANT = 10973731.568157;
inline constexpr double RYDBERG_ENERGY_EV = 13.605693122990;
inline constexpr double DEBYE = 3.33564095198152e-30;
inline constexpr double NUCLEAR_MAGNETON = 5.0507837393e-27;

inline constexpr double ELECTRONVOLT = ELEMENTARY_CHARGE;
inline constexpr double EV_PER_PARTICLE_TO_J_PER_MOL = ELEMENTARY_CHARGE * AVOGADRO_CONSTANT;
inline constexpr double EV_PER_PARTICLE_TO_KJ_PER_MOL = EV_PER_PARTICLE_TO_J_PER_MOL / 1000.0;
inline constexpr double CALORIE = 4.184;
inline constexpr double ANGSTROM = 1e-10;
inline constexpr double PICOMETER = 1e-12;
inline constexpr double LITER = 1e-3;

inline constexpr double STANDARD_PRESSURE = 100000.0;
inline constexpr double ATMOSPHERE = 101325.0;
inline constexpr double BAR = 100000.0;
inline constexpr double TORR = ATMOSPHERE / 760.0;
inline constexpr double MMHG = 133.322387415;
inline constexpr double PSI = 6894.757293168361;
inline constexpr double STANDARD_TEMPERATURE = 298.15;
inline constexpr double ZERO_CELSIUS = 273.15;
inline constexpr double STP_TEMPERATURE = 273.15;
inline constexpr double STP_PRESSURE = 100000.0;
inline constexpr double STANDARD_CONCENTRATION = 1000.0;
inline constexpr double STANDARD_MOLALITY = 1.0;
inline constexpr double WATER_ION_PRODUCT = 1.0116e-14;
inline constexpr double WATER_PKW = 13.995;

constexpr double ev_to_kj_per_mol(double ev) noexcept { return ev * EV_PER_PARTICLE_TO_KJ_PER_MOL; }
constexpr double kj_per_mol_to_ev(double kj) noexcept { return kj / EV_PER_PARTICLE_TO_KJ_PER_MOL; }
constexpr double ev_to_joules(double ev) noexcept { return ev * ELECTRONVOLT; }
constexpr double joules_to_ev(double j) noexcept { return j / ELECTRONVOLT; }
constexpr double kcal_to_kj(double kcal) noexcept { return kcal * CALORIE; }
constexpr double kj_to_kcal(double kj) noexcept { return kj / CALORIE; }
constexpr double hartree_to_ev(double eh) noexcept { return eh * HARTREE_ENERGY / ELECTRONVOLT; }
constexpr double wavenumber_to_ev(double per_cm) noexcept { return per_cm * 100.0 * PLANCK_CONSTANT * SPEED_OF_LIGHT / ELECTRONVOLT; }
constexpr double ev_to_wavenumber(double ev) noexcept { return ev * ELECTRONVOLT / (100.0 * PLANCK_CONSTANT * SPEED_OF_LIGHT); }

constexpr double celsius_to_kelvin(double c) noexcept { return c + ZERO_CELSIUS; }
constexpr double kelvin_to_celsius(double k) noexcept { return k - ZERO_CELSIUS; }
constexpr double fahrenheit_to_kelvin(double f) noexcept { return (f - 32.0) * 5.0 / 9.0 + ZERO_CELSIUS; }
constexpr double kelvin_to_fahrenheit(double k) noexcept { return (k - ZERO_CELSIUS) * 9.0 / 5.0 + 32.0; }

constexpr double atm_to_pa(double atm) noexcept { return atm * ATMOSPHERE; }
constexpr double pa_to_atm(double pa) noexcept { return pa / ATMOSPHERE; }
constexpr double bar_to_pa(double bar) noexcept { return bar * BAR; }
constexpr double pa_to_bar(double pa) noexcept { return pa / BAR; }
constexpr double torr_to_pa(double torr) noexcept { return torr * TORR; }
constexpr double pa_to_torr(double pa) noexcept { return pa / TORR; }
constexpr double kpa_to_pa(double kpa) noexcept { return kpa * 1000.0; }
constexpr double mpa_to_pa(double mpa) noexcept { return mpa * 1e6; }
constexpr double psi_to_pa(double psi) noexcept { return psi * PSI; }

constexpr double liters_to_m3(double l) noexcept { return l * LITER; }
constexpr double m3_to_liters(double m3) noexcept { return m3 / LITER; }
constexpr double ml_to_m3(double ml) noexcept { return ml * 1e-6; }
constexpr double cm3_per_mol_to_m3_per_mol(double v) noexcept { return v * 1e-6; }
constexpr double m3_per_mol_to_cm3_per_mol(double v) noexcept { return v * 1e6; }
constexpr double l_per_mol_to_m3_per_mol(double v) noexcept { return v * 1e-3; }

constexpr double pm_to_m(double pm) noexcept { return pm * PICOMETER; }
constexpr double angstrom_to_pm(double a) noexcept { return a * 100.0; }
constexpr double pm_to_angstrom(double pm) noexcept { return pm / 100.0; }
constexpr double bohr_to_pm(double a0) noexcept { return a0 * BOHR_RADIUS / PICOMETER; }
constexpr double cubic_bohr_to_cubic_angstrom(double v) noexcept { return v * 0.148184711; }

constexpr double debye_to_coulomb_meters(double d) noexcept { return d * DEBYE; }
constexpr double coulomb_meters_to_debye(double cm) noexcept { return cm / DEBYE; }

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_CONSTANTS_HPP
