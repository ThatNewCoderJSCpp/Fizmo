#ifndef FIZMO_MATH_INTEGRATOR_2D_HPP
#define FIZMO_MATH_INTEGRATOR_2D_HPP

#include "integrator.hpp"
#include "../CAS/expression_wrapper.hpp"
#include "../Common/main_convenience.hpp"
#include "../../Basic/constants.hpp"

#include <string>
#include <functional>
#include <stdexcept>

namespace fizmo {
namespace math {
namespace integration {

struct IntegrationConfig2D {
    IntegrationLayer outer = {};
    IntegrationLayer inner = {};
};

struct IntegrationResult2D {
    double value = 0.0;
    bool   converged = true;
    std::uint64_t outer_evaluations = 0;
    std::uint64_t inner_evaluations_total = 0;
    IntegrationTechnique outer_method = IntegrationTechnique::GaussKronrod;
    IntegrationTechnique inner_method = IntegrationTechnique::GaussKronrod;
    explicit operator double() const noexcept { return value; }
};

class IntegrationBound {
public:
    IntegrationBound(double val) : fn_([val](double) { return val; }), is_constant_(true), constant_val_(val) {}
    IntegrationBound(std::function<double(double)> fn) : fn_(std::move(fn)), is_constant_(false) {}

    IntegrationBound(cas::Expression expr, const std::string& outer_var);

    double operator()(double outer_val) const { return fn_(outer_val); }
    bool   is_constant() const { return is_constant_; }
    double constant_value() const { return constant_val_; }

private:
    std::function<double(double)> fn_;
    bool   is_constant_ = false;
    double constant_val_ = 0.0;
};

class Integrator2D {
public:
    static IntegrationResult2D integrate(
        std::function<double(double, double)> f,
        double outer_a, double outer_b,
        IntegrationBound inner_lo,
        IntegrationBound inner_hi,
        IntegrationConfig2D cfg = {}
    );

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, IntegrationBound inner_lo, IntegrationBound inner_hi,
        IntegrationConfig2D cfg = {}
    );

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, double inner_a, double inner_b,
        IntegrationConfig2D cfg = {}
    );

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        IntegrationConfig2D cfg = {}
    );

    static IntegrationResult2D integrate(
        std::function<double(double, double)> f,
        const Region& region,
        IntegrationConfig2D cfg = {}
    );

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var,
        const std::string& inner_var,
        const Region& region,
        IntegrationConfig2D cfg = {}
    );
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_2D_HPP