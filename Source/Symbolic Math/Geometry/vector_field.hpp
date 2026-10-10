#ifndef FIZMO_VECTOR_FIELDS_HPP
#define FIZMO_VECTOR_FIELDS_HPP

#include <string>
#include <sstream>
#include <ostream>
#include <initializer_list>
#include <utility>

#include "../CAS/expression_wrapper.hpp"
#include "../Common/main_convenience.hpp"
#include "../Linalg/vectors.hpp"
#include "../Linalg/matrices.hpp"
#include "scalar_field.hpp"
#include "../Calculus/integrator.hpp"
#include "parametric_surface.hpp"

namespace fizmo {
namespace math {
namespace geometry {

class VectorField1D {
public:
    VectorField1D(cas::Expression fx, std::string xvar = "x") : f_(std::move(fx)), xv_(std::move(xvar)) {}
    VectorField1D(const VectorField1D& o) : f_(o.f_), xv_(o.xv_) {}
    explicit VectorField1D(const ScalarField1D& sf) : f_(sf.gradient()), xv_(sf.xvar()) {}

    VectorField1D& operator=(const VectorField1D& o) {
        f_ = o.f_; xv_ = o.xv_;
        cache_div_.reset(); cache_lap_.reset();
        return *this;
    }

public:
    static VectorField1D from_potential(const ScalarField1D& sf) { return VectorField1D(sf); }

public:
    const cas::Expression&  fx()   const { return f_;  }
    const std::string&      xvar() const { return xv_; }

public:
    ScalarField1D divergence() const;

    cas::Expression jacobian() const { return cas::DIFFERENTIATE(f_, xv_); }

    VectorField1D laplacian() const;

    ScalarField1D dot(const VectorField1D& g) const { return ScalarField1D(f_ * g.f_, xv_); }
    VectorField1D directional_derivative(const cas::Expression& d) const { return VectorField1D(d * cas::DIFFERENTIATE(f_, xv_), xv_); }

public:
    double evaluate(double x) const { return f_.evaluate({{ xv_, x }}); }
    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals); }
    VectorField1D substitute(const std::string& var, double val)                const { return { SUBSTITUTE(f_, var, val), xv_ }; }
    VectorField1D substitute(const std::string& var, const cas::Expression& r)  const { return { SUBSTITUTE(f_, var, r),   xv_ }; }

    VectorField1D substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, xv_ };
    }

    VectorField1D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, xv_ };
    }

public:
    cas::Expression parameterize(const cas::Expression& x_t) const { return SUBSTITUTE(f_, xv_, x_t); }

public:
    cas::Expression flow_integrand(const cas::Expression& x_t, const std::string& param = "t") const;

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(double a, double b, const integration::IntegrationLayer& layer = {}) const { return integration::Integrator::integrate(f_, xv_, a, b, layer); }
    integration::IntegrationResult line_integral(const Interval& iv, const integration::IntegrationLayer& layer = {}) const { return integration::Integrator::integrate(f_, xv_, iv, layer); }

public:
    VectorField1D operator+(const VectorField1D& o) const { return { f_ + o.f_, xv_ }; }
    VectorField1D operator-(const VectorField1D& o) const { return { f_ - o.f_, xv_ }; }
    VectorField1D operator-()                       const { return { -f_,       xv_ }; }
    VectorField1D operator*(const cas::Expression& s)    const { return { f_ * s,    xv_ }; }
    VectorField1D operator*(double s)               const { return { f_ * s,    xv_ }; }
    VectorField1D operator/(const cas::Expression& s)    const { return { f_ / s,    xv_ }; }
    VectorField1D operator/(double s)               const { return { f_ / s,    xv_ }; }
    VectorField1D operator*(const ScalarField1D& s) const { return { f_ * s.expression(), xv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const VectorField1D& vf) {
        os << "F(" << vf.xv_ << ") = <" << vf.f_.full_simplify() << ">";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression f_;
    mutable std::unique_ptr<ScalarField1D> cache_div_;
    mutable std::unique_ptr<VectorField1D> cache_lap_;
    std::string xv_;
};

inline VectorField1D operator*(const cas::Expression& s, const VectorField1D& f) { return f * s; }
inline VectorField1D operator*(double                 s, const VectorField1D& f) { return f * s; }
inline VectorField1D operator*(const ScalarField1D&   s, const VectorField1D& f) { return f * s; }

class VectorField2D {
public:
    VectorField2D(cas::Expression fx, cas::Expression fy, std::string xvar = "x", std::string yvar = "y") : v_({ std::move(fx), std::move(fy) }) , xv_(std::move(xvar)), yv_(std::move(yvar)) {}
    VectorField2D(vectors::SymbolicVector2 v, std::string xvar = "x", std::string yvar = "y") : v_(std::move(v)), xv_(std::move(xvar)), yv_(std::move(yvar)) {}
    VectorField2D(const VectorField2D& o) : v_(o.v_), xv_(o.xv_), yv_(o.yv_) {}
    explicit VectorField2D(const ScalarField2D& sf) : v_(sf.gradient()), xv_(sf.xvar()), yv_(sf.yvar()) {}
    VectorField2D& operator=(const VectorField2D& o);
    
public:
    static VectorField2D from_potential(const ScalarField2D& sf)  { return VectorField2D(sf); }
    
public:
    const vectors::SymbolicVector2& vector() const { return v_; }
    const cas::Expression&          fx()     const { return v_.x; }
    const cas::Expression&          fy()     const { return v_.y; }
    const std::string&              xvar()   const { return xv_;  }
    const std::string&              yvar()   const { return yv_;  }

public:
    ScalarField2D divergence() const;

    ScalarField2D curl() const;

    bool is_conservative() const;

    matrices::SymbolicMatrix2x2 jacobian() const;

    VectorField2D laplacian() const;

    ScalarField2D dot(const VectorField2D& g) const { return ScalarField2D(v_.x * g.v_.x + v_.y * g.v_.y, xv_, yv_); }

public:
    VectorField2D directional_derivative(const cas::Expression& dx, const cas::Expression& dy) const;

    VectorField2D directional_derivative(const vectors::SymbolicVector2& d) const { return directional_derivative(d.x, d.y); }
    
    VectorField2D directional_derivative_unit(const vectors::SymbolicVector2& d) const {
        const auto d_n = d.unit_vector();
        return directional_derivative(d_n.x, d_n.y);
    }

public:
    vectors::SymbolicVector2 parameterize(const ParametricCurve2D& curve) const {
        return v_.substitute({
            { xv_, curve.r().x },
            { yv_, curve.r().y }
        });
    }

    vectors::SymbolicVector2 parameterize(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const { 
        return v_.substitute({
            { xv_, x_t },
            { yv_, y_t }
        });
    }

    vectors::SymbolicVector2 parameterize(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return parameterize(r.x, r.y, param); }

public:
    integration::IntegrationResult line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, param, t0, t1, layer);
    }

public:
    cas::Expression flow_integrand(const ParametricCurve2D& curve) const {
        auto F_on_curve = parameterize(curve);        
        auto& rp = curve.r_prime();                   
        return F_on_curve.dot(rp);                    
    }

    cas::Expression flux_integrand(const ParametricCurve2D& curve) const;

    cas::Expression flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const;

    cas::Expression flux_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const;

    cas::Expression flow_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return flow_integrand(r.x, r.y, param); }
    cas::Expression flux_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return flux_integrand(r.x, r.y, param); }

public:
    integration::IntegrationResult flow_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flow_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return flow_integral(r.x, r.y, param, t0, t1, layer);
    }

    integration::IntegrationResult flux_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flux_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flux_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return flux_integral(r.x, r.y, param, t0, t1, layer);
    }

public:
    Vector2D<double, double> evaluate(double x, double y) const { 
        return v_.evaluate({
            { xv_, x }, 
            { yv_, y }
        }); 
    }

    Vector2D<double, double> evaluate(std::initializer_list<std::pair<std::string, double>> vals)  const { return   v_.evaluate(vals);                   }
    VectorField2D substitute(const std::string& var, double val)                                   const { return { v_.substitute(var, val), xv_, yv_ }; }
    VectorField2D substitute(const std::string& var, const cas::Expression& r)                     const { return { v_.substitute(var, r),   xv_, yv_ }; }
    VectorField2D substitute(std::initializer_list<std::pair<std::string, double>> pairs)          const { return { v_.substitute(pairs),    xv_, yv_ }; }
    VectorField2D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const { return { v_.substitute(pairs),    xv_, yv_ }; }
    VectorField2D simplify()                                                                       const { return { v_.simplify(),           xv_, yv_ }; }
    VectorField2D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { v_.full_simplify(cfg),   xv_, yv_ }; }
    VectorField2D rewrite(const cas::RewriterConfig& cfg = {})       const { return { v_.rewrite(cfg),         xv_, yv_ }; }

public:
    VectorField2D operator+(const VectorField2D& o)   const { return { v_ + o.v_,           xv_, yv_ }; }
    VectorField2D operator-(const VectorField2D& o)   const { return { v_ - o.v_,           xv_, yv_ }; }
    VectorField2D operator-()                         const { return { -v_,                 xv_, yv_ }; }
    VectorField2D operator*(const cas::Expression& s) const { return { v_ * s,              xv_, yv_ }; }
    VectorField2D operator*(double s)                 const { return { v_ * s,              xv_, yv_ }; }
    VectorField2D operator/(const cas::Expression& s) const { return { v_ / s,              xv_, yv_ }; }
    VectorField2D operator/(double s)                 const { return { v_ / s,              xv_, yv_ }; }
    VectorField2D operator*(const ScalarField2D& s)   const { return { v_ * s.expression(), xv_, yv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const VectorField2D& vf) {
        os << "F(" << vf.xv_ << ", " << vf.yv_ << ") = <\n"
           << "    " << vf.v_.x.full_simplify() << ",\n"
           << "    " << vf.v_.y.full_simplify() << "\n>";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    vectors::SymbolicVector2 v_;
    mutable std::unique_ptr<ScalarField2D>               cache_div_;
    mutable std::unique_ptr<ScalarField2D>               cache_curl_;
    mutable std::unique_ptr<matrices::SymbolicMatrix2x2> cache_jac_;
    mutable std::unique_ptr<VectorField2D>               cache_lap_;
    std::string xv_, yv_;
};

inline VectorField2D operator*(const cas::Expression& s, const VectorField2D& f) { return f * s; }
inline VectorField2D operator*(double                 s, const VectorField2D& f) { return f * s; }
inline VectorField2D operator*(const ScalarField2D&   s, const VectorField2D& f) { return f * s; }

class VectorField3D {
public:
    VectorField3D(cas::Expression fx, cas::Expression fy, cas::Expression fz, std::string xvar = "x", std::string yvar = "y", std::string zvar = "z");
    VectorField3D(vectors::SymbolicVector3 v, std::string xvar = "x", std::string yvar = "y", std::string zvar = "z") : v_(std::move(v)), xv_(std::move(xvar)), yv_(std::move(yvar)), zv_(std::move(zvar)) {}
    VectorField3D(const VectorField3D& o) : v_(o.v_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_) {}
    explicit VectorField3D(const ScalarField3D& sf) : v_(sf.gradient()), xv_(sf.xvar()), yv_(sf.yvar()), zv_(sf.zvar()) {}
    VectorField3D& operator=(const VectorField3D& o);
    
public:
    static VectorField3D from_potential(const ScalarField3D& sf) { return VectorField3D(sf); }
    
public:
    const vectors::SymbolicVector3& vector() const { return v_;  }
    const cas::Expression&          fx()     const { return v_.x; }
    const cas::Expression&          fy()     const { return v_.y; }
    const cas::Expression&          fz()     const { return v_.z; }
    const std::string&              xvar()   const { return xv_;  }
    const std::string&              yvar()   const { return yv_;  }
    const std::string&              zvar()   const { return zv_;  }
    
public:
    ScalarField3D divergence() const;

    VectorField3D curl() const;

    bool is_conservative() const;

    matrices::SymbolicMatrix3x3 jacobian() const;

    VectorField3D laplacian() const;

    ScalarField3D dot(const VectorField3D& g) const { return ScalarField3D(v_.x * g.v_.x + v_.y * g.v_.y + v_.z * g.v_.z, xv_, yv_, zv_); }

    VectorField3D cross(const VectorField3D& g) const;

public:
    VectorField3D directional_derivative(const cas::Expression& dx, const cas::Expression& dy, const cas::Expression& dz) const;

    VectorField3D directional_derivative(const vectors::SymbolicVector3& d) const { return directional_derivative(d.x, d.y, d.z); }

    VectorField3D directional_derivative_unit(const vectors::SymbolicVector3& d) const {
        const auto d_n = d.unit_vector();
        return directional_derivative(d_n.x, d_n.y, d_n.z);
    }

public:
    vectors::SymbolicVector3 parameterize(const ParametricCurve& curve) const {
        return v_.substitute({
            { xv_, curve.r().x },
            { yv_, curve.r().y },
            { zv_, curve.r().z }
        });
    }

    vectors::SymbolicVector3 parameterize(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const {
        return v_.substitute({
            { xv_, x_t },
            { yv_, y_t },
            { zv_, z_t }
        });
    }

    vectors::SymbolicVector3 parameterize(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return parameterize(r.x, r.y, r.z, param); }

    vectors::SymbolicVector3 parameterize(const ParametricSurface& surface) const {
        return surface.parameterize(v_, xv_, yv_, zv_);
    }

public:
    cas::Expression flow_integrand(const ParametricCurve& curve) const {
        auto F_on_curve = parameterize(curve);
        auto& rp = curve.r_prime();
        return F_on_curve.dot(rp);
    }

    cas::Expression flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const;

    cas::Expression flow_integrand(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return flow_integrand(r.x, r.y, r.z, param); }

    cas::Expression flux_integrand(const ParametricSurface& surface) const;

public:
    integration::IntegrationResult line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    integration::IntegrationResult flow_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult flow_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return flow_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

public:
    Vector3D<double, double> evaluate(double x, double y, double z) const { 
        return v_.evaluate({
            { xv_, x }, 
            { yv_, y }, 
            { zv_, z }
        }); 
    }

    Vector3D<double, double> evaluate(std::initializer_list<std::pair<std::string, double>> vals)  const { return   v_.evaluate(vals);                        }
    VectorField3D substitute(const std::string& var, double val)                                   const { return { v_.substitute(var, val), xv_, yv_, zv_ }; }
    VectorField3D substitute(const std::string& var, const cas::Expression& r)                     const { return { v_.substitute(var, r),   xv_, yv_, zv_ }; }
    VectorField3D substitute(std::initializer_list<std::pair<std::string, double>> pairs)          const { return { v_.substitute(pairs),    xv_, yv_, zv_ }; }
    VectorField3D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const { return { v_.substitute(pairs),    xv_, yv_, zv_ }; }
    VectorField3D simplify()                                                                       const { return { v_.simplify(),           xv_, yv_, zv_ }; }
    VectorField3D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { v_.full_simplify(cfg),   xv_, yv_, zv_ }; }
    VectorField3D rewrite(const cas::RewriterConfig& cfg = {})       const { return { v_.rewrite(cfg),         xv_, yv_, zv_ }; }

public:
    VectorField3D operator+(const VectorField3D& o)   const { return { v_ + o.v_,           xv_, yv_, zv_ }; }
    VectorField3D operator-(const VectorField3D& o)   const { return { v_ - o.v_,           xv_, yv_, zv_ }; }
    VectorField3D operator-()                         const { return { -v_,                 xv_, yv_, zv_ }; }
    VectorField3D operator*(const cas::Expression& s) const { return { v_ * s,              xv_, yv_, zv_ }; }
    VectorField3D operator*(double s)                 const { return { v_ * s,              xv_, yv_, zv_ }; }
    VectorField3D operator/(const cas::Expression& s) const { return { v_ / s,              xv_, yv_, zv_ }; }
    VectorField3D operator/(double s)                 const { return { v_ / s,              xv_, yv_, zv_ }; }
    VectorField3D operator*(const ScalarField3D& s)   const { return { v_ * s.expression(), xv_, yv_, zv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const VectorField3D& vf) {
        os << "F(" << vf.xv_ << ", " << vf.yv_ << ", " << vf.zv_ << ") = <\n"
           << "    " << vf.v_.x.full_simplify() << ",\n"
           << "    " << vf.v_.y.full_simplify() << ",\n"
           << "    " << vf.v_.z.full_simplify() << "\n>";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    vectors::SymbolicVector3 v_;
    mutable std::unique_ptr<ScalarField3D>               cache_div_;
    mutable std::unique_ptr<VectorField3D>               cache_curl_;
    mutable std::unique_ptr<matrices::SymbolicMatrix3x3> cache_jac_;
    mutable std::unique_ptr<VectorField3D>               cache_lap_;
    std::string     xv_, yv_, zv_;
};

inline VectorField3D operator*(const cas::Expression& s, const VectorField3D& f) { return f * s; }
inline VectorField3D operator*(double                 s, const VectorField3D& f) { return f * s; }
inline VectorField3D operator*(const ScalarField3D&   s, const VectorField3D& f) { return f * s; }

class VectorField4D {
public:
    VectorField4D(cas::Expression fx, cas::Expression fy, cas::Expression fz, cas::Expression fw,
                  std::string xvar = "x", std::string yvar = "y",
                  std::string zvar = "z", std::string wvar = "w")
;

    VectorField4D(vectors::SymbolicVector4 v,
                  std::string xvar = "x", std::string yvar = "y",
                  std::string zvar = "z", std::string wvar = "w")
;

    VectorField4D(const VectorField4D& o) : v_(o.v_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_), wv_(o.wv_) {}
    explicit VectorField4D(const ScalarField4D& sf) : v_(sf.gradient()), xv_(sf.xvar()), yv_(sf.yvar()), zv_(sf.zvar()), wv_(sf.wvar()) {}

    VectorField4D& operator=(const VectorField4D& o);

public:
    static VectorField4D from_potential(const ScalarField4D& sf) { return VectorField4D(sf); }

public:
    const vectors::SymbolicVector4& vector() const { return v_;   }
    const cas::Expression&          fx()     const { return v_.x; }
    const cas::Expression&          fy()     const { return v_.y; }
    const cas::Expression&          fz()     const { return v_.z; }
    const cas::Expression&          fw()     const { return v_.w; }
    const std::string&              xvar()   const { return xv_;  }
    const std::string&              yvar()   const { return yv_;  }
    const std::string&              zvar()   const { return zv_;  }
    const std::string&              wvar()   const { return wv_;  }

public:
    ScalarField4D divergence() const;

    matrices::SymbolicMatrix4x4 jacobian() const;

    VectorField4D laplacian() const;

    ScalarField4D dot(const VectorField4D& g) const {
        return ScalarField4D(
            v_.x * g.v_.x + v_.y * g.v_.y + v_.z * g.v_.z + v_.w * g.v_.w,
            xv_, yv_, zv_, wv_
        );
    }

    bool is_conservative() const;

public:
    VectorField4D directional_derivative(const cas::Expression& dx, const cas::Expression& dy,
                                         const cas::Expression& dz, const cas::Expression& dw) const;

    VectorField4D directional_derivative(const vectors::SymbolicVector4& d) const {
        return directional_derivative(d.x, d.y, d.z, d.w);
    }

    VectorField4D directional_derivative_unit(const vectors::SymbolicVector4& d) const {
        const auto d_n = d.unit_vector();
        return directional_derivative(d_n.x, d_n.y, d_n.z, d_n.w);
    }

public:
    vector4d evaluate(const std::unordered_map<std::string, double>& vals) const {
        return v_.evaluate(vals);
    }

    vector4d evaluate(std::initializer_list<std::pair<std::string, double>> vals) const {
        return v_.evaluate(vals);
    }

    vector4d evaluate(double x_, double y_, double z_, double w_) const {
        return v_.evaluate({{ xv_, x_ }, { yv_, y_ }, { zv_, z_ }, { wv_, w_ }});
    }

public:
    VectorField4D substitute(const std::string& var, double val)                                   const { return { v_.substitute(var, val), xv_, yv_, zv_, wv_ }; }
    VectorField4D substitute(const std::string& var, const cas::Expression& r)                     const { return { v_.substitute(var, r),   xv_, yv_, zv_, wv_ }; }
    VectorField4D substitute(std::initializer_list<std::pair<std::string, double>> pairs)          const { return { v_.substitute(pairs),    xv_, yv_, zv_, wv_ }; }
    VectorField4D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const { return { v_.substitute(pairs),    xv_, yv_, zv_, wv_ }; }
    VectorField4D simplify()                                                                       const { return { v_.simplify(),           xv_, yv_, zv_, wv_ }; }
    VectorField4D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { v_.full_simplify(cfg),   xv_, yv_, zv_, wv_ }; }
    VectorField4D rewrite(const cas::RewriterConfig& cfg = {})       const { return { v_.rewrite(cfg),         xv_, yv_, zv_, wv_ }; }

public:
    VectorField4D operator+(const VectorField4D& o)   const { return { v_ + o.v_,           xv_, yv_, zv_, wv_ }; }
    VectorField4D operator-(const VectorField4D& o)   const { return { v_ - o.v_,           xv_, yv_, zv_, wv_ }; }
    VectorField4D operator-()                         const { return { -v_,                 xv_, yv_, zv_, wv_ }; }
    VectorField4D operator*(const cas::Expression& s) const { return { v_ * s,              xv_, yv_, zv_, wv_ }; }
    VectorField4D operator*(double s)                 const { return { v_ * s,              xv_, yv_, zv_, wv_ }; }
    VectorField4D operator/(const cas::Expression& s) const { return { v_ / s,              xv_, yv_, zv_, wv_ }; }
    VectorField4D operator/(double s)                 const { return { v_ / s,              xv_, yv_, zv_, wv_ }; }
    VectorField4D operator*(const ScalarField4D& s)   const { return { v_ * s.expression(), xv_, yv_, zv_, wv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const VectorField4D& vf) {
        os << "F(" << vf.xv_ << ", " << vf.yv_ << ", " << vf.zv_ << ", " << vf.wv_ << ") = <\n"
           << "    " << vf.v_.x.full_simplify() << ",\n"
           << "    " << vf.v_.y.full_simplify() << ",\n"
           << "    " << vf.v_.z.full_simplify() << ",\n"
           << "    " << vf.v_.w.full_simplify() << "\n>";
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    vectors::SymbolicVector4 v_;
    mutable std::unique_ptr<ScalarField4D>               cache_div_;
    mutable std::unique_ptr<matrices::SymbolicMatrix4x4> cache_jac_;
    mutable std::unique_ptr<VectorField4D>               cache_lap_;
    std::string xv_, yv_, zv_, wv_;
};

inline VectorField4D operator*(const cas::Expression& s, const VectorField4D& f) { return f * s; }
inline VectorField4D operator*(double                 s, const VectorField4D& f) { return f * s; }
inline VectorField4D operator*(const ScalarField4D&   s, const VectorField4D& f) { return f * s; }

class VectorFieldN {
public:
    VectorFieldN(vectors::SymbolicVectorN v, std::vector<std::string> vars);

    VectorFieldN(std::vector<cas::Expression> components, std::vector<std::string> vars) : VectorFieldN(vectors::SymbolicVectorN(std::move(components)), std::move(vars)) {}
    VectorFieldN(const VectorFieldN& o) : v_(o.v_), vars_(o.vars_) {}
    explicit VectorFieldN(const ScalarFieldN& sf) : v_(sf.gradient()), vars_(sf.vars()) {}

    VectorFieldN& operator=(const VectorFieldN& o);

public:
    static VectorFieldN from_potential(const ScalarFieldN& sf) { return VectorFieldN(sf); }

public:
    std::size_t dimension()                         const noexcept { return vars_.size(); }
    const vectors::SymbolicVectorN& vec()           const { return v_; }
    const cas::Expression& component(std::size_t i) const { return v_[i]; }
    const std::vector<std::string>& vars()          const noexcept { return vars_; }
    const std::string& var(std::size_t i)           const { return vars_.at(i); }

public:
    ScalarFieldN divergence() const;

    matrices::SymbolicMatrixN jacobian() const;

    VectorFieldN laplacian() const;

    VectorFieldN curl() const;

    ScalarFieldN dot(const VectorFieldN& g) const;

    VectorFieldN cross(const VectorFieldN& g) const;

    bool is_conservative() const;

public:
    VectorFieldN directional_derivative(const vectors::SymbolicVectorN& d) const;
    
    VectorFieldN directional_derivative_unit(const vectors::SymbolicVectorN& d_) const { return directional_derivative(d_.unit_vector()); }
    VectorFieldN directional_derivative(const std::vector<cas::Expression>& d)   const { return directional_derivative(vectors::SymbolicVectorN(d)); }

public:
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals)        const { return v_.evaluate(vals); }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return v_.evaluate(vals); }

    std::vector<double> evaluate(const std::vector<double>& vals) const;

    VectorFieldN substitute(const std::string& var, double val)                          const { return { v_.substitute(var, val),  vars_ }; }
    VectorFieldN substitute(const std::string& var, const cas::Expression& repl)         const { return { v_.substitute(var, repl), vars_ }; }
    VectorFieldN substitute(std::initializer_list<std::pair<std::string, double>> pairs) const { return { v_.substitute(pairs),     vars_ }; }
    VectorFieldN simplify()                                                              const { return { v_.simplify(),            vars_ }; }
    VectorFieldN full_simplify(const cas::RewriterConfig& cfg = {}) const { return { v_.full_simplify(cfg),    vars_ }; }
    VectorFieldN rewrite(const cas::RewriterConfig& cfg = {})       const { return { v_.rewrite(cfg),          vars_ }; }

public:
    VectorFieldN operator+(const VectorFieldN& o)    const { return { v_ + o.v_, vars_ }; }
    VectorFieldN operator-(const VectorFieldN& o)    const { return { v_ - o.v_, vars_ }; }
    VectorFieldN operator-()                         const { return { -v_,       vars_ }; }
    VectorFieldN operator*(const cas::Expression& s) const { return { v_ * s,    vars_ }; }
    VectorFieldN operator*(double s)                 const { return { v_ * s,    vars_ }; }
    VectorFieldN operator/(const cas::Expression& s) const { return { v_ / s,    vars_ }; }
    VectorFieldN operator/(double s)                 const { return { v_ / s,    vars_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const VectorFieldN& vf) {
        os << "F(";

        for (std::size_t i = 0; i < vf.vars_.size(); ++i) {
            os << vf.vars_[i];
            if (i + 1 < vf.vars_.size()) os << ", ";
        }

        os << ") = " << vf.v_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    vectors::SymbolicVectorN v_;
    std::vector<std::string> vars_;
    mutable std::unique_ptr<ScalarFieldN>              cache_div_;
    mutable std::unique_ptr<matrices::SymbolicMatrixN> cache_jac_;
    mutable std::unique_ptr<VectorFieldN>              cache_lap_;
};

inline VectorFieldN operator*(const cas::Expression& s, const VectorFieldN& f) { return f * s; }
inline VectorFieldN operator*(double s,                 const VectorFieldN& f) { return f * s; }

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_VECTOR_FIELDS_HPP