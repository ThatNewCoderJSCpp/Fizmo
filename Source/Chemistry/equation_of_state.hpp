#ifndef FIZMO_CHEMISTRY_EQUATION_OF_STATE_HPP
#define FIZMO_CHEMISTRY_EQUATION_OF_STATE_HPP

#include "common.hpp"
#include "constants.hpp"
#include "substance.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fizmo {
namespace chemistry {

enum class Root : std::uint8_t {
    Stable,
    Vapor,
    Liquid
};

struct CriticalConstants {
    double temperature = NO_VALUE;
    double pressure = NO_VALUE;
    double volume = NO_VALUE;
    double acentric_factor = 0.0;

    constexpr double compressibility() const noexcept { return pressure * volume / (GAS_CONSTANT * temperature); }
    constexpr bool known() const noexcept { return temperature == temperature && pressure == pressure; }
    static CriticalConstants of(const Substance& s);
};

struct EosState {
    double temperature = NO_VALUE;
    double pressure = NO_VALUE;
    double molar_volume = NO_VALUE;
    double compressibility = NO_VALUE;
    double fugacity_coefficient = NO_VALUE;
    Phase phase = Phase::Unknown;
    bool ok = false;

    double density(double molar_mass_g) const noexcept { return molar_mass_g / 1000.0 / molar_volume; }
    double fugacity() const noexcept { return fugacity_coefficient * pressure; }
};

class EquationOfState {
public:
    virtual ~EquationOfState() = default;

    virtual std::string name() const = 0;
    virtual std::unique_ptr<EquationOfState> clone() const = 0;
    virtual double pressure(double t, double v) const = 0;
    virtual double minimum_volume(double t) const;
    virtual bool has_ideal_gas_limit() const { return true; }

    virtual double dp_dv(double t, double v) const;
    virtual double dp_dt(double t, double v) const;
    virtual double d2p_dv2(double t, double v) const;
    virtual std::vector<double> molar_volumes(double t, double p) const;

    virtual double residual_helmholtz(double t, double v) const;
    virtual double residual_helmholtz_dt(double t, double v) const;
    virtual double residual_helmholtz_dt2(double t, double v) const;

    virtual CriticalConstants critical_point() const;
    virtual double second_virial(double t) const;
    virtual double third_virial(double t) const;

    double compressibility_at(double t, double v) const;
    double molar_volume(double t, double p, Root root = Root::Stable) const;
    virtual double compressibility(double t, double p, Root root = Root::Stable) const;
    double temperature(double p, double v) const;
    double density(double t, double p, double molar_mass_g, Root root = Root::Stable) const;
    double volume(double moles, double t, double p, Root root = Root::Stable) const;
    double moles(double p, double total_volume, double t, Root root = Root::Stable) const;
    double pressure_of(double moles, double t, double total_volume) const;

    double ln_fugacity_coefficient_at(double t, double v) const;
    virtual double ln_fugacity_coefficient(double t, double p, Root root = Root::Stable) const;
    double fugacity_coefficient(double t, double p, Root root = Root::Stable) const;
    double fugacity(double t, double p, Root root = Root::Stable) const;

    double residual_internal_energy_at(double t, double v) const;
    double residual_enthalpy_at(double t, double v) const;
    double residual_entropy_at(double t, double v) const;
    double residual_gibbs_at(double t, double v) const;
    double residual_cv_at(double t, double v) const;
    double residual_cp_at(double t, double v) const;
    virtual double residual_enthalpy(double t, double p, Root root = Root::Stable) const;
    virtual double residual_entropy(double t, double p, Root root = Root::Stable) const;
    virtual double residual_gibbs(double t, double p, Root root = Root::Stable) const;

    double isothermal_compressibility(double t, double v) const;
    double thermal_expansion(double t, double v) const;
    double bulk_modulus(double t, double v) const;
    double joule_thomson_coefficient(double t, double p, double ideal_cp, Root root = Root::Stable) const;
    double speed_of_sound(double t, double p, double ideal_cp, double molar_mass_g, Root root = Root::Stable) const;
    double heat_capacity_ratio(double t, double p, double ideal_cp, Root root = Root::Stable) const;

    std::pair<double, double> spinodal_volumes(double t) const;
    virtual double saturation_pressure(double t) const;
    double saturation_temperature(double p) const;
    std::pair<double, double> saturation_volumes(double t) const;
    double boyle_temperature() const;
    double inversion_temperature() const;
    double compressibility_at_critical() const;

    EosState state(double t, double p, Root root = Root::Stable) const;
    Phase phase(double t, double p) const;

protected:
    double numeric_temperature_derivative(const std::function<double(double)>& f, double t) const;
    double stable_root(double t, double p, const std::vector<double>& roots) const;
};

class IdealGas : public EquationOfState {
public:
    std::string name() const override { return "ideal gas"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<IdealGas>(*this); }
    double pressure(double t, double v) const override { return GAS_CONSTANT * t / v; }
    double minimum_volume(double) const override { return 0.0; }
    double dp_dv(double t, double v) const override { return -GAS_CONSTANT * t / (v * v); }
    double dp_dt(double, double v) const override { return GAS_CONSTANT / v; }
    double d2p_dv2(double t, double v) const override { return 2.0 * GAS_CONSTANT * t / (v * v * v); }
    std::vector<double> molar_volumes(double t, double p) const override { return { GAS_CONSTANT * t / p }; }
    double residual_helmholtz(double, double) const override { return 0.0; }
    double residual_helmholtz_dt(double, double) const override { return 0.0; }
    double residual_helmholtz_dt2(double, double) const override { return 0.0; }
    CriticalConstants critical_point() const override { return CriticalConstants{}; }
    double second_virial(double) const override { return 0.0; }
    double third_virial(double) const override { return 0.0; }
};

class CubicEquationOfState : public EquationOfState {
public:
    virtual double a(double t) const = 0;
    virtual double da_dt(double t) const;
    virtual double d2a_dt2(double t) const;
    virtual double b() const { return m_b; }
    virtual double epsilon() const = 0;
    virtual double sigma() const = 0;

    double pressure(double t, double v) const override;
    double minimum_volume(double) const override { return b(); }
    double dp_dv(double t, double v) const override;
    double dp_dt(double t, double v) const override;
    std::vector<double> molar_volumes(double t, double p) const override;
    double residual_helmholtz(double t, double v) const override;
    double residual_helmholtz_dt(double t, double v) const override;
    double residual_helmholtz_dt2(double t, double v) const override;
    double second_virial(double t) const override;

    std::vector<double> compressibility_roots(double t, double p) const;
    double reduced_a(double t, double p) const { return a(t) * p / (GAS_CONSTANT * GAS_CONSTANT * t * t); }
    double reduced_b(double t, double p) const { return b() * p / (GAS_CONSTANT * t); }

protected:
    double attraction_integral(double v) const;
    double m_b = 0.0;
};

class CorrespondingStatesCubic : public CubicEquationOfState {
public:
    CorrespondingStatesCubic(const CriticalConstants& c, double omega_a, double omega_b);
    const CriticalConstants& critical_constants() const noexcept { return m_critical; }
    CriticalConstants critical_point() const override { return m_critical; }
    double a(double t) const override { return m_ac * alpha(t / m_critical.temperature); }
    double da_dt(double t) const override { return m_ac * dalpha_dtr(t / m_critical.temperature) / m_critical.temperature; }
    double d2a_dt2(double t) const override { return m_ac * d2alpha_dtr2(t / m_critical.temperature) / (m_critical.temperature * m_critical.temperature); }
    double ac() const noexcept { return m_ac; }
    virtual double alpha(double tr) const = 0;
    virtual double dalpha_dtr(double tr) const = 0;
    virtual double d2alpha_dtr2(double tr) const = 0;

protected:
    CriticalConstants m_critical;
    double m_ac = 0.0;
};

class VanDerWaals : public CubicEquationOfState {
public:
    VanDerWaals(double a, double b);
    static VanDerWaals from_critical(double tc, double pc);
    static VanDerWaals from_critical(const CriticalConstants& c) { return from_critical(c.temperature, c.pressure); }
    static VanDerWaals from_substance(const Substance& s) { return from_critical(CriticalConstants::of(s)); }
    std::string name() const override { return "van der Waals"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<VanDerWaals>(*this); }
    double a(double) const override { return m_a; }
    double da_dt(double) const override { return 0.0; }
    double d2a_dt2(double) const override { return 0.0; }
    double epsilon() const override { return 0.0; }
    double sigma() const override { return 0.0; }
    CriticalConstants critical_point() const override;
    double a_constant() const noexcept { return m_a; }
    double b_constant() const noexcept { return m_b; }
    double a_in_liter_atm() const noexcept { return m_a / (LITER * LITER * ATMOSPHERE); }
    double b_in_liters() const noexcept { return m_b / LITER; }
    double boyle_temperature_exact() const noexcept { return m_a / (GAS_CONSTANT * m_b); }
    double inversion_temperature_low_pressure() const noexcept { return 2.0 * m_a / (GAS_CONSTANT * m_b); }
    double reduced_pressure(double tr, double vr) const noexcept { return 8.0 * tr / (3.0 * vr - 1.0) - 3.0 / (vr * vr); }

private:
    double m_a = 0.0;
};

class Berthelot : public CubicEquationOfState {
public:
    Berthelot(double a, double b);
    static Berthelot from_critical(double tc, double pc);
    static Berthelot from_substance(const Substance& s);
    std::string name() const override { return "Berthelot"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Berthelot>(*this); }
    double a(double t) const override { return m_a / t; }
    double da_dt(double t) const override { return -m_a / (t * t); }
    double d2a_dt2(double t) const override { return 2.0 * m_a / (t * t * t); }
    double epsilon() const override { return 0.0; }
    double sigma() const override { return 0.0; }
    CriticalConstants critical_point() const override;

private:
    double m_a = 0.0;
};

class Clausius : public CubicEquationOfState {
public:
    Clausius(double a, double b, double c);
    static Clausius from_critical(double tc, double pc, double vc);
    static Clausius from_substance(const Substance& s);
    std::string name() const override { return "Clausius"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Clausius>(*this); }
    double a(double t) const override { return m_a / t; }
    double da_dt(double t) const override { return -m_a / (t * t); }
    double d2a_dt2(double t) const override { return 2.0 * m_a / (t * t * t); }
    double epsilon() const override { return m_c / m_b; }
    double sigma() const override { return m_c / m_b; }

private:
    double m_a = 0.0;
    double m_c = 0.0;
};

class RedlichKwong : public CorrespondingStatesCubic {
public:
    explicit RedlichKwong(const CriticalConstants& c);
    RedlichKwong(double tc, double pc) : RedlichKwong(CriticalConstants{ tc, pc }) {}
    static RedlichKwong from_substance(const Substance& s) { return RedlichKwong(CriticalConstants::of(s)); }
    std::string name() const override { return "Redlich-Kwong"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<RedlichKwong>(*this); }
    double epsilon() const override { return 0.0; }
    double sigma() const override { return 1.0; }
    double alpha(double tr) const override;
    double dalpha_dtr(double tr) const override;
    double d2alpha_dtr2(double tr) const override;
};

class SoaveRedlichKwong : public CorrespondingStatesCubic {
public:
    explicit SoaveRedlichKwong(const CriticalConstants& c);
    SoaveRedlichKwong(double tc, double pc, double omega) : SoaveRedlichKwong(CriticalConstants{ tc, pc, NO_VALUE, omega }) {}
    static SoaveRedlichKwong from_substance(const Substance& s) { return SoaveRedlichKwong(CriticalConstants::of(s)); }
    std::string name() const override { return "Soave-Redlich-Kwong"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<SoaveRedlichKwong>(*this); }
    double epsilon() const override { return 0.0; }
    double sigma() const override { return 1.0; }
    double alpha(double tr) const override;
    double dalpha_dtr(double tr) const override;
    double d2alpha_dtr2(double tr) const override;
    double m() const noexcept { return m_m; }

private:
    double m_m = 0.0;
};

class PengRobinson : public CorrespondingStatesCubic {
public:
    explicit PengRobinson(const CriticalConstants& c);
    PengRobinson(double tc, double pc, double omega) : PengRobinson(CriticalConstants{ tc, pc, NO_VALUE, omega }) {}
    static PengRobinson from_substance(const Substance& s) { return PengRobinson(CriticalConstants::of(s)); }
    std::string name() const override { return "Peng-Robinson"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<PengRobinson>(*this); }
    double epsilon() const override;
    double sigma() const override;
    double alpha(double tr) const override;
    double dalpha_dtr(double tr) const override;
    double d2alpha_dtr2(double tr) const override;
    double kappa() const noexcept { return m_kappa; }

protected:
    double m_kappa = 0.0;
};

class PengRobinsonStryjekVera : public PengRobinson {
public:
    PengRobinsonStryjekVera(const CriticalConstants& c, double kappa1 = 0.0);
    static PengRobinsonStryjekVera from_substance(const Substance& s, double kappa1 = 0.0) { return PengRobinsonStryjekVera(CriticalConstants::of(s), kappa1); }
    std::string name() const override { return "Peng-Robinson-Stryjek-Vera"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<PengRobinsonStryjekVera>(*this); }
    double alpha(double tr) const override;
    double dalpha_dtr(double tr) const override;
    double d2alpha_dtr2(double tr) const override;

private:
    double kappa_of(double tr) const noexcept;
    double dkappa_of(double tr) const noexcept;
    double m_kappa0 = 0.0;
    double m_kappa1 = 0.0;
};

class PatelTeja : public CubicEquationOfState {
public:
    PatelTeja(const CriticalConstants& c, double zeta_c = NO_VALUE, double f = NO_VALUE);
    static PatelTeja from_substance(const Substance& s) { return PatelTeja(CriticalConstants::of(s)); }
    std::string name() const override { return "Patel-Teja"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<PatelTeja>(*this); }
    double a(double t) const override;
    double da_dt(double t) const override;
    double d2a_dt2(double t) const override;
    double epsilon() const override { return m_epsilon; }
    double sigma() const override { return m_sigma; }
    double c() const noexcept { return m_c; }
    CriticalConstants critical_point() const override { return m_critical; }

private:
    CriticalConstants m_critical;
    double m_ac = 0.0;
    double m_f = 0.0;
    double m_c = 0.0;
    double m_epsilon = 0.0;
    double m_sigma = 0.0;
};

class CubicMixture : public CubicEquationOfState {
public:
    CubicMixture(std::vector<std::shared_ptr<const CubicEquationOfState>> components, std::vector<double> mole_fractions,
                 std::vector<std::vector<double>> kij = {});
    std::string name() const override;
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<CubicMixture>(*this); }
    double a(double t) const override;
    double da_dt(double t) const override;
    double d2a_dt2(double t) const override;
    double epsilon() const override { return m_components.front()->epsilon(); }
    double sigma() const override { return m_components.front()->sigma(); }
    CriticalConstants critical_point() const override;

    std::size_t size() const noexcept { return m_components.size(); }
    const std::vector<double>& mole_fractions() const noexcept { return m_x; }
    void set_mole_fractions(std::vector<double> x);
    double interaction(std::size_t i, std::size_t j) const;
    void set_interaction(std::size_t i, std::size_t j, double kij);
    std::vector<double> ln_fugacity_coefficients(double t, double p, Root root = Root::Stable) const;
    std::vector<double> fugacities(double t, double p, Root root = Root::Stable) const;
    CriticalConstants pseudo_critical() const;

private:
    double aij(double t, std::size_t i, std::size_t j) const;
    void update_b();
    std::vector<std::shared_ptr<const CubicEquationOfState>> m_components;
    std::vector<double> m_x;
    std::vector<std::vector<double>> m_kij;
};

class Dieterici : public EquationOfState {
public:
    Dieterici(double a, double b) : m_a(a), m_b(b) {}
    static Dieterici from_critical(double tc, double pc);
    static Dieterici from_substance(const Substance& s);
    std::string name() const override { return "Dieterici"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Dieterici>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double) const override { return m_b; }
    CriticalConstants critical_point() const override;
    double second_virial(double t) const override { return m_b - m_a / (GAS_CONSTANT * t); }

private:
    double m_a;
    double m_b;
};

class Wohl : public EquationOfState {
public:
    Wohl(double a, double b, double c) : m_a(a), m_b(b), m_c(c) {}
    static Wohl from_critical(double tc, double pc);
    static Wohl from_substance(const Substance& s);
    std::string name() const override { return "Wohl"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Wohl>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double) const override { return m_b; }
    double residual_helmholtz(double t, double v) const override;
    CriticalConstants critical_point() const override;
    double second_virial(double t) const override { return m_b - m_a / (GAS_CONSTANT * t * t); }

private:
    double m_a;
    double m_b;
    double m_c;
};

class VirialEquation : public EquationOfState {
public:
    using Coefficient = std::function<double(double)>;
    enum class Form : std::uint8_t { Density, Pressure };

    VirialEquation(Coefficient b, Coefficient c = nullptr, Coefficient d = nullptr, Form form = Form::Density);
    static VirialEquation constant(double b, double c = 0.0, double d = 0.0, Form form = Form::Density);
    static VirialEquation tsonopoulos(const CriticalConstants& c, bool third = true, Form form = Form::Density);
    static VirialEquation pitzer_abbott(const CriticalConstants& c, Form form = Form::Pressure);
    static VirialEquation from_substance(const Substance& s, Form form = Form::Density) { return tsonopoulos(CriticalConstants::of(s), true, form); }
    static double tsonopoulos_b(const CriticalConstants& c, double t);
    static double pitzer_abbott_b(const CriticalConstants& c, double t);
    static double orbey_vera_c(const CriticalConstants& c, double t);

    std::string name() const override { return m_form == Form::Density ? "virial (density)" : "virial (pressure)"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<VirialEquation>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double t) const override;
    std::vector<double> molar_volumes(double t, double p) const override;
    double residual_helmholtz(double t, double v) const override;
    double second_virial(double t) const override { return m_b ? m_b(t) : 0.0; }
    double third_virial(double t) const override;
    Form form() const noexcept { return m_form; }

private:
    Coefficient m_b;
    Coefficient m_c;
    Coefficient m_d;
    Form m_form;
};

class BeattieBridgeman : public EquationOfState {
public:
    struct Constants {
        double a0 = 0.0;
        double a = 0.0;
        double b0 = 0.0;
        double b = 0.0;
        double c = 0.0;
    };
    explicit BeattieBridgeman(const Constants& k) : m_k(k) {}
    static BeattieBridgeman from_kilopascal_table(double a0, double a, double b0, double b, double c);
    static bool has_preset(std::string_view gas);
    static BeattieBridgeman preset(std::string_view gas);
    std::string name() const override { return "Beattie-Bridgeman"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<BeattieBridgeman>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double t) const override;
    double second_virial(double t) const override { return m_k.b0 - m_k.a0 / (GAS_CONSTANT * t) - m_k.c / (t * t * t); }
    const Constants& constants() const noexcept { return m_k; }

private:
    Constants m_k;
};

class BenedictWebbRubin : public EquationOfState {
public:
    struct Constants {
        double a = 0.0;
        double a0 = 0.0;
        double b = 0.0;
        double b0 = 0.0;
        double c = 0.0;
        double c0 = 0.0;
        double alpha = 0.0;
        double gamma = 0.0;
    };
    explicit BenedictWebbRubin(const Constants& k) : m_k(k) {}
    static BenedictWebbRubin from_kilopascal_table(double a, double a0, double b, double b0, double c, double c0, double alpha, double gamma);
    static bool has_preset(std::string_view gas);
    static BenedictWebbRubin preset(std::string_view gas);
    std::string name() const override { return "Benedict-Webb-Rubin"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<BenedictWebbRubin>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double t) const override;
    double residual_helmholtz(double t, double v) const override;
    double second_virial(double t) const override { return m_k.b0 - (m_k.a0 + m_k.c0 / (t * t)) / (GAS_CONSTANT * t); }
    const Constants& constants() const noexcept { return m_k; }

private:
    Constants m_k;
};

class LeeKesler : public EquationOfState {
public:
    explicit LeeKesler(const CriticalConstants& c) : m_critical(c) {}
    static LeeKesler from_substance(const Substance& s) { return LeeKesler(CriticalConstants::of(s)); }
    std::string name() const override { return "Lee-Kesler"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<LeeKesler>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double t) const override;
    std::vector<double> molar_volumes(double t, double p) const override;
    CriticalConstants critical_point() const override { return m_critical; }
    double compressibility(double t, double p, Root root = Root::Stable) const override;
    double ln_fugacity_coefficient(double t, double p, Root root = Root::Stable) const override;
    double residual_enthalpy(double t, double p, Root root = Root::Stable) const override;
    double residual_entropy(double t, double p, Root root = Root::Stable) const override;
    double residual_gibbs(double t, double p, Root root = Root::Stable) const override;
    double saturation_pressure(double t) const override;
    double compressibility_simple(double tr, double pr, bool liquid = false) const;
    double compressibility_reference(double tr, double pr, bool liquid = false) const;
    double generalized_compressibility(double tr, double pr, bool liquid = false) const;

private:
    bool use_liquid_root(double t, double p, Root root) const;
    double combine(double simple, double reference) const;
    CriticalConstants m_critical;
};

class CarnahanStarling : public EquationOfState {
public:
    explicit CarnahanStarling(double b) : m_b(b) {}
    static CarnahanStarling from_diameter(double sigma_m);
    std::string name() const override { return "Carnahan-Starling hard spheres"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<CarnahanStarling>(*this); }
    double pressure(double t, double v) const override;
    double minimum_volume(double) const override { return m_b / 4.0; }
    double residual_helmholtz(double t, double v) const override;
    CriticalConstants critical_point() const override { return CriticalConstants{}; }
    double second_virial(double) const override { return m_b; }
    double packing_fraction(double v) const noexcept { return m_b / (4.0 * v); }

protected:
    double m_b;
};

class CarnahanStarlingVanDerWaals : public CarnahanStarling {
public:
    CarnahanStarlingVanDerWaals(double a, double b) : CarnahanStarling(b), m_a(a) {}
    static CarnahanStarlingVanDerWaals from_critical(double tc, double pc);
    static CarnahanStarlingVanDerWaals from_substance(const Substance& s);
    std::string name() const override { return "Carnahan-Starling-van der Waals"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<CarnahanStarlingVanDerWaals>(*this); }
    double pressure(double t, double v) const override;
    double residual_helmholtz(double t, double v) const override;
    double residual_helmholtz_dt(double t, double v) const override;
    CriticalConstants critical_point() const override { return EquationOfState::critical_point(); }
    double second_virial(double t) const override { return m_b - m_a / (GAS_CONSTANT * t); }

private:
    double m_a;
};

class NobleAbel : public EquationOfState {
public:
    explicit NobleAbel(double covolume) : m_b(covolume) {}
    std::string name() const override { return "Noble-Abel"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<NobleAbel>(*this); }
    double pressure(double t, double v) const override { return GAS_CONSTANT * t / (v - m_b); }
    double minimum_volume(double) const override { return m_b; }
    std::vector<double> molar_volumes(double t, double p) const override { return { GAS_CONSTANT * t / p + m_b }; }
    double residual_helmholtz(double t, double v) const override { return -GAS_CONSTANT * t * std::log(1.0 - m_b / v); }
    double residual_helmholtz_dt(double, double v) const override { return -GAS_CONSTANT * std::log(1.0 - m_b / v); }
    double residual_helmholtz_dt2(double, double) const override { return 0.0; }
    CriticalConstants critical_point() const override { return CriticalConstants{}; }
    double second_virial(double) const override { return m_b; }

private:
    double m_b;
};

class StiffenedGas : public EquationOfState {
public:
    StiffenedGas(double gamma, double p_infinity, double cv_molar) : m_gamma(gamma), m_pinf(p_infinity), m_cv(cv_molar) {}
    static StiffenedGas water() { return StiffenedGas(6.1, 2.0e8, 75.3 / 6.1); }
    std::string name() const override { return "stiffened gas"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<StiffenedGas>(*this); }
    double pressure(double t, double v) const override { return (m_gamma - 1.0) * m_cv * t / v - m_pinf; }
    double minimum_volume(double) const override { return 0.0; }
    std::vector<double> molar_volumes(double t, double p) const override { return { (m_gamma - 1.0) * m_cv * t / (p + m_pinf) }; }
    bool has_ideal_gas_limit() const override { return m_pinf == 0.0 && (m_gamma - 1.0) * m_cv == GAS_CONSTANT; }
    CriticalConstants critical_point() const override { return CriticalConstants{}; }
    double sound_speed(double t, double v, double molar_mass_g) const;

private:
    double m_gamma;
    double m_pinf;
    double m_cv;
};

class CondensedEquationOfState : public EquationOfState {
public:
    CondensedEquationOfState(double v0, double k0, double k0_prime, double t0 = STANDARD_TEMPERATURE, double thermal_expansion = 0.0)
        : m_v0(v0), m_k0(k0), m_k0p(k0_prime), m_t0(t0), m_alpha(thermal_expansion) {}
    bool has_ideal_gas_limit() const override { return false; }
    double minimum_volume(double t) const override { return 1e-6 * reference_volume(t); }
    std::vector<double> molar_volumes(double t, double p) const override;
    double residual_helmholtz(double, double) const override { return NO_VALUE; }
    double residual_helmholtz_dt(double, double) const override { return NO_VALUE; }
    double residual_helmholtz_dt2(double, double) const override { return NO_VALUE; }
    CriticalConstants critical_point() const override { return CriticalConstants{}; }
    double second_virial(double) const override { return NO_VALUE; }
    double third_virial(double) const override { return NO_VALUE; }
    double reference_volume(double t) const noexcept { return m_v0 * (1.0 + m_alpha * (t - m_t0)); }
    double reference_bulk_modulus() const noexcept { return m_k0; }
    double reference_bulk_modulus_derivative() const noexcept { return m_k0p; }

protected:
    double m_v0;
    double m_k0;
    double m_k0p;
    double m_t0;
    double m_alpha;
};

class Tait : public CondensedEquationOfState {
public:
    Tait(double v0, double b, double c, double p0 = ATMOSPHERE, double t0 = STANDARD_TEMPERATURE, double thermal_expansion = 0.0);
    static Tait from_bulk_modulus(double v0, double k0, double k0_prime, double p0 = ATMOSPHERE, double t0 = STANDARD_TEMPERATURE, double thermal_expansion = 0.0);
    std::string name() const override { return "Tait"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Tait>(*this); }
    double pressure(double t, double v) const override;
    std::vector<double> molar_volumes(double t, double p) const override;

private:
    double m_b;
    double m_c;
    double m_p0;
};

class Murnaghan : public CondensedEquationOfState {
public:
    using CondensedEquationOfState::CondensedEquationOfState;
    std::string name() const override { return "Murnaghan"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Murnaghan>(*this); }
    double pressure(double t, double v) const override;
    std::vector<double> molar_volumes(double t, double p) const override;
};

class BirchMurnaghan : public CondensedEquationOfState {
public:
    using CondensedEquationOfState::CondensedEquationOfState;
    std::string name() const override { return "Birch-Murnaghan (third order)"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<BirchMurnaghan>(*this); }
    double pressure(double t, double v) const override;
};

class Vinet : public CondensedEquationOfState {
public:
    using CondensedEquationOfState::CondensedEquationOfState;
    std::string name() const override { return "Vinet"; }
    std::unique_ptr<EquationOfState> clone() const override { return std::make_unique<Vinet>(*this); }
    double pressure(double t, double v) const override;
};

enum class EosKind : std::uint8_t {
    IdealGas,
    VanDerWaals,
    Berthelot,
    Clausius,
    Dieterici,
    Wohl,
    RedlichKwong,
    SoaveRedlichKwong,
    PengRobinson,
    PengRobinsonStryjekVera,
    PatelTeja,
    Virial,
    LeeKesler,
    CarnahanStarlingVanDerWaals,
    BeattieBridgeman,
    BenedictWebbRubin
};

const char* to_string(EosKind kind) noexcept;
std::unique_ptr<EquationOfState> make_equation_of_state(EosKind kind, const CriticalConstants& c);
std::unique_ptr<EquationOfState> make_equation_of_state(EosKind kind, const Substance& s);
std::vector<EosKind> equation_of_state_kinds();

double ideal_gas_pressure(double moles, double t, double volume) noexcept;
double ideal_gas_volume(double moles, double t, double p) noexcept;
double ideal_gas_temperature(double moles, double p, double volume) noexcept;
double ideal_gas_moles(double p, double volume, double t) noexcept;
double ideal_gas_density(double p, double t, double molar_mass_g) noexcept;
double ideal_gas_molar_mass(double density, double p, double t) noexcept;
double combined_gas_law(double p1, double v1, double t1, double p2, double t2) noexcept;
double van_der_waals_pressure(double moles, double t, double volume, double a, double b) noexcept;

CriticalConstants kay_pseudo_critical(const std::vector<CriticalConstants>& components, const std::vector<double>& mole_fractions);
double rackett_volume(const CriticalConstants& c, double t, double zra = NO_VALUE);
double wilson_k_value(const CriticalConstants& c, double t, double p);
double wilson_vapor_pressure(const CriticalConstants& c, double t);
double lee_kesler_vapor_pressure(const CriticalConstants& c, double t);
double acentric_factor_from_vapor_pressure(double p_sat_at_tr_07, double pc);

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_EQUATION_OF_STATE_HPP
