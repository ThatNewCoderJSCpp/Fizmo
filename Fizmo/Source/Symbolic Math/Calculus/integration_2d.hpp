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

    IntegrationBound(cas::Expression expr, const std::string& outer_var) : is_constant_(false) {
        auto vars = expr.variable_names();
        bool depends = false;
        for (auto& v : vars) { if (v == outer_var) { depends = true; break; }}

        if (!depends) {
            fn_ = [](double) { return 0.0; };
            is_constant_ = true;
            constant_val_ = 0.0;
        } else {
            std::string ov = outer_var;
            fn_ = [expr, ov](double t) -> double { return expr.evaluate({{ ov, t }}); };
        }
    }

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
    ) {
        IntegrationResult2D result;
        result.outer_method = cfg.outer.technique;
        result.inner_method = cfg.inner.technique;
        std::uint64_t inner_evals = 0;
        const auto& inner_layer = cfg.inner;

        auto outer_func = [&](double x) -> double {
            double ya = inner_lo(x);
            double yb = inner_hi(x);
            if (std::abs(ya - yb) <= constants::middle_epsilon()) return 0.0;
            auto inner_func = [&f, x](double y) -> double { return f(x, y); };
            auto ir = Integrator::integrate(inner_func, ya, yb, inner_layer);
            inner_evals += ir.function_evaluations;
            if (!ir.converged) result.converged = false;
            return ir.value;
        };

        auto outer_result = Integrator::integrate(outer_func, outer_a, outer_b, cfg.outer);
        result.value = outer_result.value;
        result.converged = result.converged && outer_result.converged;
        result.outer_evaluations = outer_result.function_evaluations;
        result.inner_evaluations_total = inner_evals;
        return result;
    }

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, IntegrationBound inner_lo, IntegrationBound inner_hi,
        IntegrationConfig2D cfg = {}
    ) {
        auto f2d = [&f, &outer_var, &inner_var](double x, double y) -> double { return f.evaluate({{ outer_var, x }, { inner_var, y }}); };
        return integrate(f2d, outer_a, outer_b, std::move(inner_lo), std::move(inner_hi), cfg);
    }

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, double inner_a, double inner_b,
        IntegrationConfig2D cfg = {}
    ) {
        return integrate(
            f, outer_var, outer_a, outer_b,
            inner_var,
            IntegrationBound(inner_a),
            IntegrationBound(inner_b), cfg
        );
    }

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        IntegrationConfig2D cfg = {}
    ) {
        return integrate(
            f, outer_var, outer_a, outer_b,
            inner_var,
            IntegrationBound(inner_lo_expr, outer_var),
            IntegrationBound(inner_hi_expr, outer_var), cfg
        );
    }

    static IntegrationResult2D integrate(
        std::function<double(double, double)> f,
        const Region& region,
        IntegrationConfig2D cfg = {}
    ) {
        auto masked = [f = std::move(f), &region](double x, double y) -> double {
            return region.contains(x, y) ? f(x, y) : 0.0;
        };

        return integrate(
            std::move(masked),
            region.x_range.lower.value, region.x_range.upper.value,
            IntegrationBound(region.y_range.lower.value),
            IntegrationBound(region.y_range.upper.value),
            cfg
        );
    }

    static IntegrationResult2D integrate(
        const cas::Expression& f,
        const std::string& outer_var,
        const std::string& inner_var,
        const Region& region,
        IntegrationConfig2D cfg = {}
    ) {
        auto f2d = [&f, &outer_var, &inner_var, &region](double x, double y) -> double {
            return region.contains(x, y) ? f.evaluate({{ outer_var, x }, { inner_var, y }}) : 0.0;
        };

        return integrate(
            f2d,
            region.x_range.lower.value, region.x_range.upper.value,
            IntegrationBound(region.y_range.lower.value),
            IntegrationBound(region.y_range.upper.value),
            cfg
        );
    }
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_2D_HPP