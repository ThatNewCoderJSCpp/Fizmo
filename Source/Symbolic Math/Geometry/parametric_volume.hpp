#ifndef FIZMO_MATH_PARAMETRIC_VOLUME_HPP
#define FIZMO_MATH_PARAMETRIC_VOLUME_HPP

#include "../CAS/expression_wrapper.hpp"
#include "../Linalg/vectors.hpp"
#include "../Linalg/matrices.hpp"
#include "../Calculus/integrator_3d.hpp"
#include "../Common/secondary_convenience.hpp"

#include <string>
#include <memory>
#include <sstream>

namespace fizmo {
namespace math {
namespace geometry {

class ParametricVolume {
public:
    ParametricVolume(vectors::SymbolicVector3 r,
                     std::string u = "u",
                     std::string v = "v",
                     std::string w = "w")
        : r_(std::move(r)), u_(std::move(u)), v_(std::move(v)), w_(std::move(w)) {}

    ParametricVolume(cas::Expression x_uvw,
                     cas::Expression y_uvw,
                     cas::Expression z_uvw,
                     std::string u = "u",
                     std::string v = "v",
                     std::string w = "w")
;

public:
    const vectors::SymbolicVector3& r() const { return r_; }
    const std::string& u_var() const { return u_; }
    const std::string& v_var() const { return v_; }
    const std::string& w_var() const { return w_; }

public:
    const vectors::SymbolicVector3& r_u() const;

    const vectors::SymbolicVector3& r_v() const;

    const vectors::SymbolicVector3& r_w() const;

public:
    const matrices::SymbolicMatrixNM& jacobian() const;

    cas::Expression jacobian_determinant() const { return jacobian().determinant(); }
    cas::Expression volume_element() const { return ABS(jacobian_determinant()); }

public:
    Vector3D<double> evaluate(double u_val, double v_val, double w_val) const {
        return r_.evaluate({{ u_, u_val }, { v_, v_val }, { w_, w_val }});
    }

public:
    cas::Expression parameterize(
        const cas::Expression& f,
        const std::string& xv,
        const std::string& yv,
        const std::string& zv
    ) const {
        return f.substitute({{ xv, r_.x }, { yv, r_.y }, { zv, r_.z }});
    }

    vectors::SymbolicVector3 parameterize(
        const vectors::SymbolicVector3& F,
        const std::string& xv,
        const std::string& yv,
        const std::string& zv
    ) const {
        return F.substitute({{ xv, r_.x }, { yv, r_.y }, { zv, r_.z }});
    }

public:
    cas::Expression scalar_integrand(
        const cas::Expression& f,
        const std::string& xv,
        const std::string& yv,
        const std::string& zv
    ) const {
        cas::Expression f_on_vol = parameterize(f, xv, yv, zv);
        return f_on_vol * volume_element();
    }

public:
    integration::IntegrationResult3D scalar_volume_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer,  double outer_lo,  double outer_hi,
        const std::string& middle, double middle_lo, double middle_hi,
        const std::string& inner,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D scalar_volume_integral(
        const cas::Expression& f,
        const std::string& xv, const std::string& yv, const std::string& zv,
        const std::string& outer,  double outer_lo,  double outer_hi,
        const std::string& middle, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

public:
    ParametricVolume substitute(const std::string& var, double val) const {
        return { r_.substitute(var, val), u_, v_, w_ };
    }

    ParametricVolume substitute(const std::string& var, const cas::Expression& expr) const {
        return { r_.substitute(var, expr), u_, v_, w_ };
    }

    ParametricVolume simplify() const {
        return { r_.simplify(), u_, v_, w_ };
    }

    ParametricVolume full_simplify(const cas::RewriterConfig& cfg = {}) const {
        return { r_.full_simplify(cfg), u_, v_, w_ };
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const ParametricVolume& s) {
        os << "Parametric volume:\n"
           << "    r(" << s.u_ << ", " << s.v_ << ", " << s.w_ << ") = " << s.r_.full_simplify() << "\n"
           << "    r_u = " << s.r_u().full_simplify() << "\n"
           << "    r_v = " << s.r_v().full_simplify() << "\n"
           << "    r_w = " << s.r_w().full_simplify() << "\n"
           << "    det(J) = " << s.jacobian_determinant().full_simplify();
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }

private:
    vectors::SymbolicVector3 r_;
    std::string u_, v_, w_;

    mutable std::unique_ptr<matrices::SymbolicMatrixNM> jac_;
    mutable std::unique_ptr<vectors::SymbolicVector3> ru_;
    mutable std::unique_ptr<vectors::SymbolicVector3> rv_;
    mutable std::unique_ptr<vectors::SymbolicVector3> rw_;
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_PARAMETRIC_VOLUME_HPP