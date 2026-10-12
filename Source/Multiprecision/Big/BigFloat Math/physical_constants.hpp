#ifndef FIZMO_MULTIPRECISION_BIG_PHYSICAL_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_BIG_PHYSICAL_CONSTANTS_HPP

#include "big_float_consts.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace constants {

enum class mass_unit : std::uint8_t { kg, MeV, dalton };

enum class particle : std::uint8_t {
    electron,
    muon,
    tau,
    proton,
    neutron,
    deuteron,
    triton,
    helion,
    alpha_particle,
    w_boson,
    z_boson,
    higgs_boson,
    up_quark,
    down_quark,
    strange_quark,
    charm_quark,
    bottom_quark,
    top_quark,
    charged_pion,
    neutral_pion,
    charged_kaon,
    neutral_kaon,
    eta_meson,
    lambda_baryon,
    sigma_plus,
    sigma_zero,
    sigma_minus,
    xi_zero,
    xi_minus,
    omega_minus,
    count
};

template <class T>
struct PhysicalConstant {
    T           value{};
    T           uncertainty{};
    T           relative_uncertainty{};
    bool        exact  = false;
    const char* name   = "";
    const char* symbol = "";
    const char* unit   = "";
};

template <class T>
struct ParticleProperties {
    particle            id   = particle::electron;
    const char*         name = "";
    PhysicalConstant<T> mass_kg;
    PhysicalConstant<T> mass_mev;
    PhysicalConstant<T> mass_dalton;
    bool                has_g_factor = false;
    PhysicalConstant<T> g_factor;
};

namespace pcdetail {
struct access;
} // namespace pcdetail

class ConstantDefinition {
private:
    struct Factor {
        std::uint32_t atom;
        int           power;
    };

    BigInt              m_num;
    BigInt              m_den;
    std::vector<Factor> m_factors;
    bool                m_defined = true;
    const char*         m_name    = "";
    const char*         m_symbol  = "";
    const char*         m_unit    = "";

    friend struct pcdetail::access;

public:
    ConstantDefinition();

    static ConstantDefinition integer(std::int64_t value);
    static ConstantDefinition ratio(std::int64_t num, std::int64_t den);
    static ConstantDefinition decimal(std::int64_t mantissa, int exp10);
    static ConstantDefinition measured(std::int64_t mantissa, int exp10, std::uint64_t uncertainty, int uncertainty_exp10);
    static ConstantDefinition pi();
    static ConstantDefinition undefined();

    ConstantDefinition labeled(const char* name, const char* symbol, const char* unit) const;

    const char* name() const noexcept { return m_name; }
    const char* symbol() const noexcept { return m_symbol; }
    const char* unit() const noexcept { return m_unit; }
    bool is_defined() const noexcept { return m_defined; }
    bool is_exact() const;

    ConstantDefinition pow(int k) const;
    ConstantDefinition inverse() const { return pow(-1); }
    ConstantDefinition squared() const { return pow(2); }

    BigFloat value(const BigFloatContext& ctx) const;
    BigFloat value() const { return value(BigFloatContext::current()); }
    BigFloat relative_uncertainty(const BigFloatContext& ctx) const;
    BigFloat relative_uncertainty() const { return relative_uncertainty(BigFloatContext::current()); }
    PhysicalConstant<BigFloat> evaluate(const BigFloatContext& ctx) const;
    PhysicalConstant<BigFloat> evaluate() const { return evaluate(BigFloatContext::current()); }

    template <class F> F value() const;
    template <class F> PhysicalConstant<F> evaluate() const;

    friend ConstantDefinition operator*(const ConstantDefinition& a, const ConstantDefinition& b);
    friend ConstantDefinition operator/(const ConstantDefinition& a, const ConstantDefinition& b);
    friend ConstantDefinition operator*(const ConstantDefinition& a, std::int64_t b) { return a * integer(b); }
    friend ConstantDefinition operator*(std::int64_t a, const ConstantDefinition& b) { return integer(a) * b; }
    friend ConstantDefinition operator/(const ConstantDefinition& a, std::int64_t b) { return a / integer(b); }
    friend ConstantDefinition operator/(std::int64_t a, const ConstantDefinition& b) { return integer(a) / b; }
};

PhysicalConstant<BigFloat> operator*(const PhysicalConstant<BigFloat>& a, const PhysicalConstant<BigFloat>& b);
PhysicalConstant<BigFloat> operator/(const PhysicalConstant<BigFloat>& a, const PhysicalConstant<BigFloat>& b);
PhysicalConstant<BigFloat> operator*(const PhysicalConstant<BigFloat>& a, const BigFloat& b);
PhysicalConstant<BigFloat> operator*(const BigFloat& a, const PhysicalConstant<BigFloat>& b);
PhysicalConstant<BigFloat> operator/(const PhysicalConstant<BigFloat>& a, const BigFloat& b);
PhysicalConstant<BigFloat> pow(const PhysicalConstant<BigFloat>& a, int k);

const ConstantDefinition& speed_of_light();
const ConstantDefinition& planck_constant();
const ConstantDefinition& reduced_planck_constant();
const ConstantDefinition& boltzmann_constant();
const ConstantDefinition& gravitational_constant();
const ConstantDefinition& cosmological_constant();
const ConstantDefinition& avogadro_constant();
const ConstantDefinition& first_radiation_constant();
const ConstantDefinition& first_radiation_constant_spectral();
const ConstantDefinition& second_radiation_constant();
const ConstantDefinition& astronomical_unit();
const ConstantDefinition& parsec();
const ConstantDefinition& wien_wavelength_constant();
const ConstantDefinition& wien_frequency_constant();
const ConstantDefinition& wien_entropy_constant();
const ConstantDefinition& elementary_charge();
const ConstantDefinition& stefan_boltzmann_constant();
const ConstantDefinition& conductance_quantum();
const ConstantDefinition& von_klitzing_constant();
const ConstantDefinition& josephson_constant();
const ConstantDefinition& magnetic_flux_quantum();
const ConstantDefinition& fine_structure_constant();
const ConstantDefinition& vacuum_permeability();
const ConstantDefinition& vacuum_impedance();
const ConstantDefinition& vacuum_permittivity();
const ConstantDefinition& coulomb_constant();
const ConstantDefinition& quantum_of_circulation();
const ConstantDefinition& nuclear_magneton();
const ConstantDefinition& bohr_magneton();
const ConstantDefinition& bohr_radius();
const ConstantDefinition& rydberg_constant();
const ConstantDefinition& rydberg_energy();
const ConstantDefinition& hartree_energy();
const ConstantDefinition& fermi_coupling_constant();
const ConstantDefinition& molar_gas_constant();
const ConstantDefinition& faraday_constant();
const ConstantDefinition& molar_planck_constant();
const ConstantDefinition& weak_mixing_angle();
const ConstantDefinition& weinberg_angle();
const ConstantDefinition& atomic_mass_constant();
const ConstantDefinition& electron_mass();
const ConstantDefinition& proton_mass();
const ConstantDefinition& neutron_mass();

inline const ConstantDefinition& c() { return speed_of_light(); }
inline const ConstantDefinition& h() { return planck_constant(); }
inline const ConstantDefinition& hbar() { return reduced_planck_constant(); }
inline const ConstantDefinition& k_B() { return boltzmann_constant(); }
inline const ConstantDefinition& G() { return gravitational_constant(); }
inline const ConstantDefinition& N_A() { return avogadro_constant(); }
inline const ConstantDefinition& alpha() { return fine_structure_constant(); }
inline const ConstantDefinition& mu_0() { return vacuum_permeability(); }
inline const ConstantDefinition& epsilon_0() { return vacuum_permittivity(); }
inline const ConstantDefinition& m_e() { return electron_mass(); }
inline const ConstantDefinition& m_p() { return proton_mass(); }
inline const ConstantDefinition& m_n() { return neutron_mass(); }
inline const ConstantDefinition& m_u() { return atomic_mass_constant(); }

const char* name(particle p) noexcept;
const char* unit(mass_unit u) noexcept;
bool        has_g_factor(particle p) noexcept;

const ConstantDefinition& mass(particle p, mass_unit u);
const ConstantDefinition& g_factor(particle p);

ParticleProperties<BigFloat> properties(particle p, const BigFloatContext& ctx);
inline ParticleProperties<BigFloat> properties(particle p) { return properties(p, BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_PHYSICAL_CONSTANTS_HPP
