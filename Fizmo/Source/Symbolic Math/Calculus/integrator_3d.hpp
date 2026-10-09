#ifndef FIZMO_MATH_INTEGRATOR_3D_HPP
#define FIZMO_MATH_INTEGRATOR_3D_HPP

#include "integration_2d.hpp"
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

struct IntegrationConfig3D {
    IntegrationLayer outer  = {};
    IntegrationLayer middle = {};
    IntegrationLayer inner  = {};
};

struct IntegrationResult3D {
    double value = 0.0;
    bool   converged = true;
    std::uint64_t outer_evaluations = 0;
    std::uint64_t middle_evaluations_total = 0;
    std::uint64_t inner_evaluations_total = 0;
    IntegrationTechnique outer_method  = IntegrationTechnique::GaussKronrod;
    IntegrationTechnique middle_method = IntegrationTechnique::GaussKronrod;
    IntegrationTechnique inner_method  = IntegrationTechnique::GaussKronrod;
    explicit operator double() const noexcept { return value; }
};

class IntegrationBound2 {
public:
    IntegrationBound2(double val) : fn_([val](double, double) { return val; }), is_constant_(true), constant_val_(val) {}
    IntegrationBound2(std::function<double(double, double)> fn) : fn_(std::move(fn)), is_constant_(false) {}

    IntegrationBound2(cas::Expression expr, const std::string& outer_var, const std::string& middle_var);

    double operator()(double outer_val, double middle_val) const { return fn_(outer_val, middle_val); }
    bool   is_constant() const { return is_constant_; }
    double constant_value() const { return constant_val_; }

private:
    std::function<double(double, double)> fn_;
    bool   is_constant_ = false;
    double constant_val_ = 0.0;
};

class Integrator3D {
public:
    static IntegrationResult3D integrate(
        std::function<double(double, double, double)> f,
        double outer_a, double outer_b,
        IntegrationBound middle_lo,
        IntegrationBound middle_hi,
        IntegrationBound2 inner_lo,
        IntegrationBound2 inner_hi,
        IntegrationConfig3D cfg = {}
    );

    static IntegrationResult3D integrate(
        const cas::Expression& f,
        const std::string& outer_var,  double outer_a,  double outer_b,
        const std::string& middle_var, double middle_a,  double middle_b,
        const std::string& inner_var,  double inner_a,   double inner_b,
        IntegrationConfig3D cfg = {}
    );

    static IntegrationResult3D integrate(
        const cas::Expression& f,
        const std::string& outer_var,  double outer_a,  double outer_b,
        const std::string& middle_var, IntegrationBound middle_lo, IntegrationBound middle_hi,
        const std::string& inner_var,  IntegrationBound2 inner_lo, IntegrationBound2 inner_hi,
        IntegrationConfig3D cfg = {}
    );

    static IntegrationResult3D integrate(
        const cas::Expression& f,
        const std::string& outer_var,  double outer_a,  double outer_b,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        IntegrationConfig3D cfg = {}
    );

    static IntegrationResult3D integrate(
        std::function<double(double, double, double)> f,
        const Region3D& region,
        IntegrationConfig3D cfg = {}
    );

    static IntegrationResult3D integrate(
        const cas::Expression& f,
        const std::string& outer_var,
        const std::string& middle_var,
        const std::string& inner_var,
        const Region3D& region,
        IntegrationConfig3D cfg = {}
    );
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_3D_HPP