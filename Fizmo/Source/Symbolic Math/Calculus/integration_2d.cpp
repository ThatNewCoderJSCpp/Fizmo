#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace integration {

IntegrationBound::IntegrationBound(cas::Expression expr, const std::string& outer_var) : is_constant_(false) {
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

auto Integrator2D::integrate(
        std::function<double(double, double)> f,
        double outer_a, double outer_b,
        IntegrationBound inner_lo,
        IntegrationBound inner_hi,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
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

auto Integrator2D::integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, IntegrationBound inner_lo, IntegrationBound inner_hi,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
        auto f2d = [&f, &outer_var, &inner_var](double x, double y) -> double { return f.evaluate({{ outer_var, x }, { inner_var, y }}); };
        return integrate(f2d, outer_a, outer_b, std::move(inner_lo), std::move(inner_hi), cfg);
    }

auto Integrator2D::integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var, double inner_a, double inner_b,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
        return integrate(
            f, outer_var, outer_a, outer_b,
            inner_var,
            IntegrationBound(inner_a),
            IntegrationBound(inner_b), cfg
        );
    }

auto Integrator2D::integrate(
        const cas::Expression& f,
        const std::string& outer_var, double outer_a, double outer_b,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
        return integrate(
            f, outer_var, outer_a, outer_b,
            inner_var,
            IntegrationBound(inner_lo_expr, outer_var),
            IntegrationBound(inner_hi_expr, outer_var), cfg
        );
    }

auto Integrator2D::integrate(
        std::function<double(double, double)> f,
        const Region& region,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
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

auto Integrator2D::integrate(
        const cas::Expression& f,
        const std::string& outer_var,
        const std::string& inner_var,
        const Region& region,
        IntegrationConfig2D cfg 
) -> IntegrationResult2D {
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

} // namespace integration
} // namespace math
} // namespace fizmo
