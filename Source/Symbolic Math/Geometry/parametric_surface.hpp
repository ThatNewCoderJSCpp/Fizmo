#ifndef FIZMO_MATH_PARAMETRIC_SURFACE_HPP
#define FIZMO_MATH_PARAMETRIC_SURFACE_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../Linalg/vectors.hpp"
#include "../Calculus/integrator.hpp"
#include "../Calculus/integration_2d.hpp"
#include "../Common/secondary_convenience.hpp"
#include "frenet_frame.hpp"

#include <string>
#include <memory>
#include <iostream>
#include <sstream>

namespace fizmo {
namespace math {
namespace geometry {

class ParametricSurface {
public:
    ParametricSurface(vectors::SymbolicVector3 r, std::string u = "u", std::string v = "v") : r_(std::move(r)), u_(std::move(u)), v_(std::move(v)) {}

    ParametricSurface(cas::Expression x_uv, cas::Expression y_uv, cas::Expression z_uv, std::string u = "u", std::string v = "v")
        : r_(std::move(x_uv), std::move(y_uv), std::move(z_uv)),
          u_(std::move(u)), v_(std::move(v)) {}

public:
    const vectors::SymbolicVector3& r() const { return r_; }
    const std::string& u_var()          const { return u_; }
    const std::string& v_var()          const { return v_; }

public:
    const vectors::SymbolicVector3& r_u() const;

    const vectors::SymbolicVector3& r_v() const;

    const vectors::SymbolicVector3& r_uu() const;

    const vectors::SymbolicVector3& r_uv() const;

    const vectors::SymbolicVector3& r_vv() const;

public:
    const vectors::SymbolicVector3& normal() const;

    const vectors::SymbolicVector3& u_cross_v()    const { return normal();              }
          vectors::SymbolicVector3  v_cross_u()    const { return -normal();             }
          vectors::SymbolicVector3  unit_normal()  const { return normal().normalized(); }
    cas::Expression area_element() const { return normal().magnitude();  }

    cas::Expression E() const { return r_u().dot(r_u()); }
    cas::Expression F() const { return r_u().dot(r_v()); }
    cas::Expression G() const { return r_v().dot(r_v()); }

    cas::Expression e() const { return r_uu().dot(unit_normal()); }
    cas::Expression f() const { return r_uv().dot(unit_normal()); }
    cas::Expression g() const { return r_vv().dot(unit_normal()); }

    cas::Expression gaussian_curvature() const {
        return (e() * g() - f() * f()) / (E() * G() - F() * F());
    }

    cas::Expression mean_curvature() const {
        return (e() * G() - cas::Const(2.0) * f() * F() + g() * E()) / (cas::Const(2.0) * (E() * G() - F() * F()));
    }

    const matrices::SymbolicMatrixNM& jacobian() const;

    matrices::SymbolicMatrix2x2 metric() const {
        return {
            E(), F(),
            F(), G()
        };
    }

    cas::Expression metric_determinant() const {
        return E()*G() - F()*F();
    }

public:
    Vector3D<double> evaluate(double u_val, double v_val) const {
        return r_.evaluate({{ u_, u_val }, { v_, v_val }});
    }

    Vector3D<double> normal_at(double u_val, double v_val) const {
        return normal().evaluate({{ u_, u_val }, { v_, v_val }});
    }

public:
    cas::Expression parameterize(const cas::Expression& f, const std::string& xv, const std::string& yv, const std::string& zv) const {
        return f.substitute({{ xv, r_.x }, { yv, r_.y }, { zv, r_.z }});
    }

    vectors::SymbolicVector3 parameterize(const vectors::SymbolicVector3& F, const std::string& xv, const std::string& yv, const std::string& zv) const {
        return F.substitute({{ xv, r_.x }, { yv, r_.y }, { zv, r_.z }});
    }

public:
    cas::Expression scalar_integrand(const cas::Expression& f, const std::string& xv, const std::string& yv, const std::string& zv) const {
        cas::Expression f_on_surface = parameterize(f, xv, yv, zv);
        return f_on_surface * area_element();
    }

    cas::Expression flux_integrand(const vectors::SymbolicVector3& F, const std::string& xv, const std::string& yv, const std::string& zv) const;

public:
    integration::IntegrationResult2D surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_area(
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D scalar_surface_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D flux_integral(
        const vectors::SymbolicVector3& F,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

public:
    ParametricSurface substitute(const std::string& var, double val) const {
        return { r_.substitute(var, val), u_, v_ };
    }

    ParametricSurface substitute(const std::string& var, const cas::Expression& expr) const {
        return { r_.substitute(var, expr), u_, v_ };
    }

    ParametricSurface simplify() const {
        return { r_.simplify(), u_, v_ };
    }

    ParametricSurface full_simplify(const cas::RewriterConfig& cfg = {}) const {
        return { r_.full_simplify(cfg), u_, v_ };
    }

public:
    ParametricCurve u_curve(double u_val) const { return ParametricCurve(r_.substitute(u_, u_val), v_); }
    ParametricCurve v_curve(double v_val) const { return ParametricCurve(r_.substitute(v_, v_val), u_); }

public:
    friend std::ostream& operator<<(std::ostream& os, const ParametricSurface& s) {
        os << "Parametric surface:\n"
           << "    r(" << s.u_ << ", " << s.v_ << ") = " << s.r_.full_simplify() << "\n"
           << "    r_" << s.u_ << " = " << s.r_u().full_simplify() << "\n"
           << "    r_" << s.v_ << " = " << s.r_v().full_simplify() << "\n"
           << "    n = r_" << s.u_ << " × r_" << s.v_ << " = " << s.normal().full_simplify();
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }

private:
    vectors::SymbolicVector3 r_;
    std::string u_, v_;

    mutable std::unique_ptr<matrices::SymbolicMatrixNM> jac_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   ru_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   rv_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   ruu_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   ruv_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   rvv_;
    mutable std::unique_ptr<vectors::SymbolicVector3>   normal_;
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_PARAMETRIC_SURFACE_HPP