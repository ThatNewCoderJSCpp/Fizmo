#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace integration {

IntegrationBound2::IntegrationBound2(cas::Expression expr, const std::string& outer_var, const std::string& middle_var) : is_constant_(false) {
    auto vars = expr.variable_names();
    bool depends = false;
    for (auto& v : vars) { if (v == outer_var || v == middle_var) { depends = true; break; } }

    if (!depends) {
        fn_ = [](double, double) { return 0.0; };
        is_constant_ = true;
        constant_val_ = 0.0;
    } else {
        std::string ov = outer_var;
        std::string mv = middle_var;
        fn_ = [expr, ov, mv](double x, double y) -> double { return expr.evaluate({{ ov, x }, { mv, y }}); };
    }
}

auto Integrator3D::integrate(
    std::function<double(double, double, double)> f,
    double outer_a, double outer_b,
    IntegrationBound middle_lo,
    IntegrationBound middle_hi,
    IntegrationBound2 inner_lo,
    IntegrationBound2 inner_hi,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    IntegrationResult3D result;
    result.outer_method  = cfg.outer.technique;
    result.middle_method = cfg.middle.technique;
    result.inner_method  = cfg.inner.technique;
    std::uint64_t middle_evals = 0;
    std::uint64_t inner_evals  = 0;
    const auto& middle_layer = cfg.middle;
    const auto& inner_layer  = cfg.inner;

    auto outer_func = [&](double x) -> double {
        double ya = middle_lo(x);
        double yb = middle_hi(x);
        if (std::abs(ya - yb) <= constants::middle_epsilon()) return 0.0;

        auto middle_func = [&](double y) -> double {
            double za = inner_lo(x, y);
            double zb = inner_hi(x, y);
            if (std::abs(za - zb) <= constants::middle_epsilon()) return 0.0;
            auto inner_func = [&f, x, y](double z) -> double { return f(x, y, z); };
            auto ir = Integrator::integrate(inner_func, za, zb, inner_layer);
            inner_evals += ir.function_evaluations;
            if (!ir.converged) result.converged = false;
            return ir.value;
        };

        auto mr = Integrator::integrate(middle_func, ya, yb, middle_layer);
        middle_evals += mr.function_evaluations;
        if (!mr.converged) result.converged = false;
        return mr.value;
    };

    auto outer_result = Integrator::integrate(outer_func, outer_a, outer_b, cfg.outer);
    result.value = outer_result.value;
    result.converged = result.converged && outer_result.converged;
    result.outer_evaluations = outer_result.function_evaluations;
    result.middle_evaluations_total = middle_evals;
    result.inner_evaluations_total  = inner_evals;
    return result;
}

auto Integrator3D::integrate(
    const cas::Expression& f,
    const std::string& outer_var,  double outer_a,  double outer_b,
    const std::string& middle_var, double middle_a,  double middle_b,
    const std::string& inner_var,  double inner_a,   double inner_b,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    return integrate(
        f,
        outer_var,  outer_a,  outer_b,
        middle_var, IntegrationBound(middle_a), IntegrationBound(middle_b),
        inner_var,  IntegrationBound2(inner_a), IntegrationBound2(inner_b),
        cfg
    );
}

auto Integrator3D::integrate(
    const cas::Expression& f,
    const std::string& outer_var,  double outer_a,  double outer_b,
    const std::string& middle_var, IntegrationBound middle_lo, IntegrationBound middle_hi,
    const std::string& inner_var,  IntegrationBound2 inner_lo, IntegrationBound2 inner_hi,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    auto f3d = [&f, &outer_var, &middle_var, &inner_var](double x, double y, double z) -> double {
        return f.evaluate({{ outer_var, x }, { middle_var, y }, { inner_var, z }});
    };
    return integrate(
        f3d, outer_a, outer_b,
        std::move(middle_lo), std::move(middle_hi),
        std::move(inner_lo),  std::move(inner_hi),
        cfg
    );
}

auto Integrator3D::integrate(
    const cas::Expression& f,
    const std::string& outer_var,  double outer_a,  double outer_b,
    const std::string& middle_var,
    const cas::Expression& middle_lo_expr,
    const cas::Expression& middle_hi_expr,
    const std::string& inner_var,
    const cas::Expression& inner_lo_expr,
    const cas::Expression& inner_hi_expr,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    return integrate(
        f,
        outer_var,  outer_a,  outer_b,
        middle_var,
        IntegrationBound(middle_lo_expr, outer_var),
        IntegrationBound(middle_hi_expr, outer_var),
        inner_var,
        IntegrationBound2(inner_lo_expr, outer_var, middle_var),
        IntegrationBound2(inner_hi_expr, outer_var, middle_var),
        cfg
    );
}

auto Integrator3D::integrate(
    std::function<double(double, double, double)> f,
    const Region3D& region,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    auto masked = [f = std::move(f), &region](double x, double y, double z) -> double {
        return region.contains(x, y, z) ? f(x, y, z) : 0.0;
    };

    return integrate(
        std::move(masked),
        region.x_range.lower.value, region.x_range.upper.value,
        IntegrationBound(region.y_range.lower.value),
        IntegrationBound(region.y_range.upper.value),
        IntegrationBound2(region.z_range.lower.value),
        IntegrationBound2(region.z_range.upper.value),
        cfg
    );
}

auto Integrator3D::integrate(
    const cas::Expression& f,
    const std::string& outer_var,
    const std::string& middle_var,
    const std::string& inner_var,
    const Region3D& region,
    IntegrationConfig3D cfg 
) -> IntegrationResult3D {
    auto f3d = [&f, &outer_var, &middle_var, &inner_var, &region](double x, double y, double z) -> double {
        return region.contains(x, y, z) ? f.evaluate({{ outer_var, x }, { middle_var, y }, { inner_var, z }}) : 0.0;
    };

    return integrate(
        f3d,
        region.x_range.lower.value, region.x_range.upper.value,
        IntegrationBound(region.y_range.lower.value),
        IntegrationBound(region.y_range.upper.value),
        IntegrationBound2(region.z_range.lower.value),
        IntegrationBound2(region.z_range.upper.value),
        cfg
    );
}

} // namespace integration
} // namespace math
} // namespace fizmo
