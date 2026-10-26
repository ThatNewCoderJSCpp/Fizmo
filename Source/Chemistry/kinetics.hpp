#ifndef FIZMO_CHEMISTRY_KINETICS_HPP
#define FIZMO_CHEMISTRY_KINETICS_HPP

#include "common.hpp"
#include "constants.hpp"

#include <string>
#include <vector>

namespace fizmo {
namespace chemistry {

double arrhenius(double pre_exponential, double activation_energy, double t) noexcept;
double modified_arrhenius(double pre_exponential, double temperature_exponent, double activation_energy, double t, double t_ref = 1.0) noexcept;
double activation_energy(double k1, double t1, double k2, double t2) noexcept;
double pre_exponential_factor(double k, double activation_energy, double t) noexcept;
double rate_constant_at(double k1, double t1, double t2, double activation_energy) noexcept;
double eyring(double delta_h_activation, double delta_s_activation, double t, double transmission = 1.0) noexcept;
double eyring_from_gibbs(double delta_g_activation, double t, double transmission = 1.0) noexcept;
double collision_rate_constant(double cross_section, double reduced_mass_u, double t, double activation_energy = 0.0, double steric_factor = 1.0) noexcept;
double collision_cross_section(double diameter_a, double diameter_b) noexcept;
double q10(double k1, double k2, double t1, double t2) noexcept;
double rate_from_q10(double k1, double q10_value, double t1, double t2) noexcept;

double zero_order_concentration(double c0, double k, double t) noexcept;
double first_order_concentration(double c0, double k, double t) noexcept;
double second_order_concentration(double c0, double k, double t) noexcept;
double nth_order_concentration(double c0, double k, double t, double order) noexcept;
double mixed_second_order_concentration(double a0, double b0, double k, double t) noexcept;
double zero_order_half_life(double c0, double k) noexcept;
double first_order_half_life(double k) noexcept;
double second_order_half_life(double c0, double k) noexcept;
double nth_order_half_life(double c0, double k, double order) noexcept;
double rate_constant_from_half_life(double half_life, double c0 = 1.0, double order = 1.0) noexcept;
double first_order_time_to(double c0, double c, double k) noexcept;
double consecutive_intermediate(double a0, double k1, double k2, double t) noexcept;
double consecutive_product(double a0, double k1, double k2, double t) noexcept;
double reversible_first_order(double a0, double kf, double kr, double t) noexcept;
double michaelis_menten(double substrate, double v_max, double k_m) noexcept;
double competitive_inhibition(double substrate, double inhibitor, double v_max, double k_m, double k_i) noexcept;
double lineweaver_burk_slope(double v_max, double k_m) noexcept;
double rate_law(double k, const std::vector<double>& concentrations, const std::vector<double>& orders);
double overall_order(const std::vector<double>& orders) noexcept;

struct ElementaryStep {
    std::vector<std::pair<std::size_t, double>> reactants;
    std::vector<std::pair<std::size_t, double>> products;
    double forward_rate = 0.0;
    double reverse_rate = 0.0;
    std::string label;

    double forward(const std::vector<double>& c) const;
    double reverse(const std::vector<double>& c) const;
    double net(const std::vector<double>& c) const { return forward(c) - reverse(c); }
};

class ReactionNetwork {
public:
    explicit ReactionNetwork(std::size_t species = 0) : m_species(species) {}

    std::size_t species() const noexcept { return m_species; }
    void set_species(std::size_t n) noexcept { m_species = n; }
    std::size_t add(ElementaryStep step);
    std::size_t add(std::vector<std::pair<std::size_t, double>> reactants, std::vector<std::pair<std::size_t, double>> products, double kf, double kr = 0.0);
    const std::vector<ElementaryStep>& steps() const noexcept { return m_steps; }
    std::vector<ElementaryStep>& steps() noexcept { return m_steps; }

    void derivatives(const std::vector<double>& concentrations, std::vector<double>& rates) const;
    std::vector<double> derivatives(const std::vector<double>& concentrations) const;
    std::vector<double> step_rates(const std::vector<double>& concentrations) const;
    std::vector<double> simulate(std::vector<double> concentrations, double time, int steps) const;
    void set_temperature_dependence(std::size_t step, double pre_exponential, double activation_energy, double t);

private:
    std::size_t m_species;
    std::vector<ElementaryStep> m_steps;
};

} // namespace chemistry
} // namespace fizmo

#endif // FIZMO_CHEMISTRY_KINETICS_HPP
