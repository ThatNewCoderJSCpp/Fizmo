#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace geometry {

auto ParametricSurface::r_u() const -> const vectors::SymbolicVector3& {
        if (!ru_) ru_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(u_));
        return *ru_;
    }

auto ParametricSurface::r_v() const -> const vectors::SymbolicVector3& {
        if (!rv_) rv_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(v_));
        return *rv_;
    }

auto ParametricSurface::r_uu() const -> const vectors::SymbolicVector3& {
        if (!ruu_) ruu_ = std::make_unique<vectors::SymbolicVector3>(r_u().differentiate(u_));
        return *ruu_;
    }

auto ParametricSurface::r_uv() const -> const vectors::SymbolicVector3& {
        if (!ruv_) ruv_ = std::make_unique<vectors::SymbolicVector3>(r_u().differentiate(v_));
        return *ruv_;
    }

auto ParametricSurface::r_vv() const -> const vectors::SymbolicVector3& {
        if (!rvv_) rvv_ = std::make_unique<vectors::SymbolicVector3>(r_v().differentiate(v_));
        return *rvv_;
    }

auto ParametricSurface::normal() const -> const vectors::SymbolicVector3& {
        if (!normal_) normal_ = std::make_unique<vectors::SymbolicVector3>(r_u().cross(r_v()));
        return *normal_;
    }

auto ParametricSurface::jacobian() const -> const matrices::SymbolicMatrixNM& {
        if (!jac_) {
            jac_ = std::make_unique<matrices::SymbolicMatrixNM>(matrices::SymbolicMatrixNM{
                {r_u().x, r_v().x},
                {r_u().y, r_v().y},
                {r_u().z, r_v().z}
            });
        }
        return *jac_;
    }

auto ParametricSurface::flux_integrand(const vectors::SymbolicVector3& F, const std::string& xv, const std::string& yv, const std::string& zv) const -> cas::Expression {
        vectors::SymbolicVector3 F_on_surface = parameterize(F, xv, yv, zv);
        const auto& n = normal();
        return F_on_surface.x * n.x + F_on_surface.y * n.y + F_on_surface.z * n.z;
    }

auto ParametricSurface::surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            area_element(), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ParametricSurface::surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        cas::Expression ae = area_element();
        const std::string& ov = outer;
        const std::string& iv = inner;

        auto f2d = [ae, ov, iv](double o, double i) -> double {
            return ae.evaluate({{ ov, o }, { iv, i }});
        };

        return integration::Integrator2D::integrate(
            f2d, outer_lo, outer_hi,
            std::move(inner_lo), std::move(inner_hi), cfg
        );
    }

auto ParametricSurface::surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            area_element(), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ParametricSurface::scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            scalar_integrand(f, xv, yv, zv), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ParametricSurface::scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        cas::Expression integrand = scalar_integrand(f, xv, yv, zv);
        const std::string& ov = outer;
        const std::string& iv = inner;

        auto f2d = [integrand, ov, iv](double o, double i) -> double {
            return integrand.evaluate({{ ov, o }, { iv, i }});
        };

        return integration::Integrator2D::integrate(
            f2d, outer_lo, outer_hi,
            std::move(inner_lo), std::move(inner_hi), cfg
        );
    }

auto ParametricSurface::scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            scalar_integrand(f, xv, yv, zv), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ParametricSurface::flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            flux_integrand(F, xv, yv, zv), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ParametricSurface::flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        cas::Expression integrand = flux_integrand(F, xv, yv, zv);
        const std::string& ov = outer;
        const std::string& iv = inner;

        auto f2d = [integrand, ov, iv](double o, double i) -> double {
            return integrand.evaluate({{ ov, o }, { iv, i }});
        };

        return integration::Integrator2D::integrate(
            f2d, outer_lo, outer_hi,
            std::move(inner_lo), std::move(inner_hi), cfg
        );
    }

auto ParametricSurface::flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            flux_integrand(F, xv, yv, zv), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
