#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace geometry {

ParametricVolume::ParametricVolume(cas::Expression x_uvw,
                     cas::Expression y_uvw,
                     cas::Expression z_uvw,
                     std::string u,
                     std::string v,
                     std::string w) : r_(std::move(x_uvw), std::move(y_uvw), std::move(z_uvw)),
          u_(std::move(u)), v_(std::move(v)), w_(std::move(w)) {}

auto ParametricVolume::r_u() const -> const vectors::SymbolicVector3& {
        if (!ru_) ru_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(u_));
        return *ru_;
    }

auto ParametricVolume::r_v() const -> const vectors::SymbolicVector3& {
        if (!rv_) rv_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(v_));
        return *rv_;
    }

auto ParametricVolume::r_w() const -> const vectors::SymbolicVector3& {
        if (!rw_) rw_ = std::make_unique<vectors::SymbolicVector3>(r_.differentiate(w_));
        return *rw_;
    }

auto ParametricVolume::jacobian() const -> const matrices::SymbolicMatrixNM& {
        if (!jac_) {
            jac_ = std::make_unique<matrices::SymbolicMatrixNM>(matrices::SymbolicMatrixNM{
                { r_u().x, r_v().x, r_w().x },
                { r_u().y, r_v().y, r_w().y },
                { r_u().z, r_v().z, r_w().z }
            });
        }
        return *jac_;
    }

auto ParametricVolume::scalar_volume_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer,  double outer_lo,  double outer_hi,
        const std::string& middle, double middle_lo, double middle_hi,
        const std::string& inner,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            scalar_integrand(f, xv, yv, zv),
            outer, outer_lo, outer_hi,
            middle, middle_lo, middle_hi,
            inner, inner_lo, inner_hi,
            cfg
        );
    }

auto ParametricVolume::scalar_volume_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer,  double outer_lo,  double outer_hi,
        const std::string& middle, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        cas::Expression integrand = scalar_integrand(f, xv, yv, zv);
        const std::string& ov = outer;
        const std::string& mv = middle;
        const std::string& iv = inner;

        auto f3d = [integrand, ov, mv, iv](double o, double m, double i) -> double {
            return integrand.evaluate({{ ov, o }, { mv, m }, { iv, i }});
        };

        return integration::Integrator3D::integrate(
            f3d,
            outer_lo, outer_hi,
            std::move(middle_lo), std::move(middle_hi),
            std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
