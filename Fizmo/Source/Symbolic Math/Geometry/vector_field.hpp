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
    ScalarField1D divergence() const {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField1D>(cas::DIFFERENTIATE(f_, xv_), xv_);
        return *cache_div_;
    }

    cas::Expression jacobian() const { return cas::DIFFERENTIATE(f_, xv_); }

    VectorField1D laplacian() const {
        if (!cache_lap_) {
            cas::Expression lap = cas::DIFFERENTIATE(cas::DIFFERENTIATE(f_, xv_), xv_);
            cache_lap_ = std::make_unique<VectorField1D>(std::move(lap), xv_);
        }

        return *cache_lap_;
    }

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
    cas::Expression flow_integrand(const cas::Expression& x_t, const std::string& param = "t") const {
        cas::Expression F_on_curve = parameterize(x_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        return F_on_curve * xp;
    }

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(x_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

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
    VectorField2D& operator=(const VectorField2D& o) { v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; cache_div_.reset(); cache_curl_.reset(); cache_jac_.reset(); cache_lap_.reset(); return *this; }
    
public:
    static VectorField2D from_potential(const ScalarField2D& sf)  { return VectorField2D(sf); }
    
public:
    const vectors::SymbolicVector2& vector() const { return v_; }
    const cas::Expression&          fx()     const { return v_.x; }
    const cas::Expression&          fy()     const { return v_.y; }
    const std::string&              xvar()   const { return xv_;  }
    const std::string&              yvar()   const { return yv_;  }

public:
    ScalarField2D divergence() const {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField2D>(cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_), xv_, yv_);
        return *cache_div_;
    }

    ScalarField2D curl() const {
        if (!cache_curl_) cache_curl_ = std::make_unique<ScalarField2D>(cas::DIFFERENTIATE(v_.y, xv_) - cas::DIFFERENTIATE(v_.x, yv_), xv_, yv_);
        return *cache_curl_;
    }

    bool is_conservative() const {
        cas::Expression dP_dy = cas::DIFFERENTIATE(v_.x, yv_);
        cas::Expression dQ_dx = cas::DIFFERENTIATE(v_.y, xv_);
        return dP_dy.symbolic_equals(dQ_dx);
    }

    matrices::SymbolicMatrix2x2 jacobian() const {
        if (!cache_jac_) {
            matrices::SymbolicMatrix2x2 J;
            J.get_data()[0][0] = cas::DIFFERENTIATE(v_.x, xv_); J.get_data()[0][1] = cas::DIFFERENTIATE(v_.x, yv_);
            J.get_data()[1][0] = cas::DIFFERENTIATE(v_.y, xv_); J.get_data()[1][1] = cas::DIFFERENTIATE(v_.y, yv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix2x2>(J);
        }

        return *cache_jac_;
    }

    VectorField2D laplacian() const {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_);
            };
            cache_lap_ = std::make_unique<VectorField2D>(vectors::SymbolicVector2{ lap(v_.x), lap(v_.y) }, xv_, yv_);
        }

        return *cache_lap_;
    }

    ScalarField2D dot(const VectorField2D& g) const { return ScalarField2D(v_.x * g.v_.x + v_.y * g.v_.y, xv_, yv_); }

public:
    VectorField2D directional_derivative(const cas::Expression& dx, const cas::Expression& dy) const {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_);
        };

        return VectorField2D({ dd(v_.x), dd(v_.y) }, xv_, yv_);
    }

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
    integration::IntegrationResult line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, param, t0, t1, layer);
    }

public:
    cas::Expression flow_integrand(const ParametricCurve2D& curve) const {
        auto F_on_curve = parameterize(curve);        
        auto& rp = curve.r_prime();                   
        return F_on_curve.dot(rp);                    
    }

    cas::Expression flux_integrand(const ParametricCurve2D& curve) const {
        auto F_on_curve = parameterize(curve);         
        auto& rp = curve.r_prime();                   
        return F_on_curve.x * rp.y - F_on_curve.y * rp.x;  
    }

    cas::Expression flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const {
        auto F_on_curve = parameterize(x_t, y_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        return F_on_curve.x * xp + F_on_curve.y * yp;
    }

    cas::Expression flux_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const {
        auto F_on_curve = parameterize(x_t, y_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        return F_on_curve.x * yp - F_on_curve.y * xp;
    }

    cas::Expression flow_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return flow_integrand(r.x, r.y, param); }
    cas::Expression flux_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return flux_integrand(r.x, r.y, param); }

public:
    integration::IntegrationResult flow_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult flow_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return flow_integral(r.x, r.y, param, t0, t1, layer);
    }

    integration::IntegrationResult flux_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flux_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult flux_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flux_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

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
    VectorField3D(cas::Expression fx, cas::Expression fy, cas::Expression fz, std::string xvar = "x", std::string yvar = "y", std::string zvar = "z") : v_({ std::move(fx), std::move(fy), std::move(fz) }) , xv_(std::move(xvar)), yv_(std::move(yvar)), zv_(std::move(zvar)) {}
    VectorField3D(vectors::SymbolicVector3 v, std::string xvar = "x", std::string yvar = "y", std::string zvar = "z") : v_(std::move(v)), xv_(std::move(xvar)), yv_(std::move(yvar)), zv_(std::move(zvar)) {}
    VectorField3D(const VectorField3D& o) : v_(o.v_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_) {}
    explicit VectorField3D(const ScalarField3D& sf) : v_(sf.gradient()), xv_(sf.xvar()), yv_(sf.yvar()), zv_(sf.zvar()) {}
    VectorField3D& operator=(const VectorField3D& o) { v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; cache_div_.reset(); cache_curl_.reset(); cache_jac_.reset(); cache_lap_.reset(); return *this; }
    
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
    ScalarField3D divergence() const {
        if (!cache_div_) cache_div_ = std::make_unique<ScalarField3D>(cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_) + cas::DIFFERENTIATE(v_.z, zv_), xv_, yv_, zv_);
        return *cache_div_;
    }

    VectorField3D curl() const {
        if (!cache_curl_) cache_curl_ = std::make_unique<VectorField3D>(vectors::SymbolicVector3{
            cas::DIFFERENTIATE(v_.z, yv_) - cas::DIFFERENTIATE(v_.y, zv_),
            cas::DIFFERENTIATE(v_.x, zv_) - cas::DIFFERENTIATE(v_.z, xv_),
            cas::DIFFERENTIATE(v_.y, xv_) - cas::DIFFERENTIATE(v_.x, yv_)
        }, xv_, yv_, zv_);
        return *cache_curl_;
    }

    bool is_conservative() const {
        cas::Expression dP_dy = cas::DIFFERENTIATE(v_.x, yv_);
        cas::Expression dQ_dx = cas::DIFFERENTIATE(v_.y, xv_);
        if (!dP_dy.symbolic_equals(dQ_dx)) return false;
        cas::Expression dP_dz = cas::DIFFERENTIATE(v_.x, zv_);
        cas::Expression dR_dx = cas::DIFFERENTIATE(v_.z, xv_);
        if (!dP_dz.symbolic_equals(dR_dx)) return false;
        cas::Expression dQ_dz = cas::DIFFERENTIATE(v_.y, zv_);
        cas::Expression dR_dy = cas::DIFFERENTIATE(v_.z, yv_);
        return dQ_dz.symbolic_equals(dR_dy);
    }

    matrices::SymbolicMatrix3x3 jacobian() const {
        if (!cache_jac_) {
            matrices::SymbolicMatrix3x3 J;
            J.get_data()[0][0] = cas::DIFFERENTIATE(v_.x, xv_); J.get_data()[0][1] = cas::DIFFERENTIATE(v_.x, yv_); J.get_data()[0][2] = cas::DIFFERENTIATE(v_.x, zv_);
            J.get_data()[1][0] = cas::DIFFERENTIATE(v_.y, xv_); J.get_data()[1][1] = cas::DIFFERENTIATE(v_.y, yv_); J.get_data()[1][2] = cas::DIFFERENTIATE(v_.y, zv_);
            J.get_data()[2][0] = cas::DIFFERENTIATE(v_.z, xv_); J.get_data()[2][1] = cas::DIFFERENTIATE(v_.z, yv_); J.get_data()[2][2] = cas::DIFFERENTIATE(v_.z, zv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix3x3>(J);
        }

        return *cache_jac_;
    }

    VectorField3D laplacian() const {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_) + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, zv_), zv_);
            };
            cache_lap_ = std::make_unique<VectorField3D>(vectors::SymbolicVector3{ lap(v_.x), lap(v_.y), lap(v_.z) }, xv_, yv_, zv_);
        }

        return *cache_lap_;
    }

    ScalarField3D dot(const VectorField3D& g) const { return ScalarField3D(v_.x * g.v_.x + v_.y * g.v_.y + v_.z * g.v_.z, xv_, yv_, zv_); }

    VectorField3D cross(const VectorField3D& g) const {
        return VectorField3D(
            {
                v_.y * g.v_.z - v_.z * g.v_.y,
                v_.z * g.v_.x - v_.x * g.v_.z,
                v_.x * g.v_.y - v_.y * g.v_.x
            },
            xv_, yv_, zv_
        );
    }

public:
    VectorField3D directional_derivative(const cas::Expression& dx, const cas::Expression& dy, const cas::Expression& dz) const {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_) + dz * cas::DIFFERENTIATE(f, zv_);
        };
        return VectorField3D({ dd(v_.x), dd(v_.y), dd(v_.z) }, xv_, yv_, zv_);
    }

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

    cas::Expression flow_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const {
        auto F_on_curve = parameterize(x_t, y_t, z_t, param);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression zp = cas::DIFFERENTIATE(z_t, param);
        return F_on_curve.x * xp + F_on_curve.y * yp + F_on_curve.z * zp;
    }

    cas::Expression flow_integrand(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return flow_integrand(r.x, r.y, r.z, param); }

    cas::Expression flux_integrand(const ParametricSurface& surface) const {
        vectors::SymbolicVector3 F_on_surface = v_.substitute({
            { xv_, surface.r().x },
            { yv_, surface.r().y },
            { zv_, surface.r().z }
        });
        const auto& n = surface.normal();
        return F_on_surface.x * n.x + F_on_surface.y * n.y + F_on_surface.z * n.z;
    }

public:
    integration::IntegrationResult line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    integration::IntegrationResult flow_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult flow_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = flow_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult flow_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return flow_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            flux_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        cas::Expression integrand = flux_integrand(surface);
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

    integration::IntegrationResult2D flux_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            flux_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

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
        : v_({ std::move(fx), std::move(fy), std::move(fz), std::move(fw) }),
          xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

    VectorField4D(vectors::SymbolicVector4 v,
                  std::string xvar = "x", std::string yvar = "y",
                  std::string zvar = "z", std::string wvar = "w")
        : v_(std::move(v)), xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

    VectorField4D(const VectorField4D& o) : v_(o.v_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_), wv_(o.wv_) {}
    explicit VectorField4D(const ScalarField4D& sf) : v_(sf.gradient()), xv_(sf.xvar()), yv_(sf.yvar()), zv_(sf.zvar()), wv_(sf.wvar()) {}

    VectorField4D& operator=(const VectorField4D& o) {
        v_ = o.v_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; wv_ = o.wv_;
        cache_div_.reset(); cache_jac_.reset(); cache_lap_.reset();
        return *this;
    }

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
    ScalarField4D divergence() const {
        if (!cache_div_) {
            cache_div_ = std::make_unique<ScalarField4D>(
                cas::DIFFERENTIATE(v_.x, xv_) + cas::DIFFERENTIATE(v_.y, yv_) +
                cas::DIFFERENTIATE(v_.z, zv_) + cas::DIFFERENTIATE(v_.w, wv_),
                xv_, yv_, zv_, wv_
            );
        }
        return *cache_div_;
    }

    matrices::SymbolicMatrix4x4 jacobian() const {
        if (!cache_jac_) {
            matrices::SymbolicMatrix4x4 J;
            auto& d = J.get_data();
            d[0][0] = cas::DIFFERENTIATE(v_.x, xv_); d[0][1] = cas::DIFFERENTIATE(v_.x, yv_);
            d[0][2] = cas::DIFFERENTIATE(v_.x, zv_); d[0][3] = cas::DIFFERENTIATE(v_.x, wv_);
            d[1][0] = cas::DIFFERENTIATE(v_.y, xv_); d[1][1] = cas::DIFFERENTIATE(v_.y, yv_);
            d[1][2] = cas::DIFFERENTIATE(v_.y, zv_); d[1][3] = cas::DIFFERENTIATE(v_.y, wv_);
            d[2][0] = cas::DIFFERENTIATE(v_.z, xv_); d[2][1] = cas::DIFFERENTIATE(v_.z, yv_);
            d[2][2] = cas::DIFFERENTIATE(v_.z, zv_); d[2][3] = cas::DIFFERENTIATE(v_.z, wv_);
            d[3][0] = cas::DIFFERENTIATE(v_.w, xv_); d[3][1] = cas::DIFFERENTIATE(v_.w, yv_);
            d[3][2] = cas::DIFFERENTIATE(v_.w, zv_); d[3][3] = cas::DIFFERENTIATE(v_.w, wv_);
            cache_jac_ = std::make_unique<matrices::SymbolicMatrix4x4>(J);
        }
        return *cache_jac_;
    }

    VectorField4D laplacian() const {
        if (!cache_lap_) {
            auto lap = [&](const cas::Expression& f) {
                return cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, xv_), xv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, yv_), yv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, zv_), zv_)
                     + cas::DIFFERENTIATE(cas::DIFFERENTIATE(f, wv_), wv_);
            };
            cache_lap_ = std::make_unique<VectorField4D>(
                vectors::SymbolicVector4{ lap(v_.x), lap(v_.y), lap(v_.z), lap(v_.w) },
                xv_, yv_, zv_, wv_
            );
        }
        return *cache_lap_;
    }

    ScalarField4D dot(const VectorField4D& g) const {
        return ScalarField4D(
            v_.x * g.v_.x + v_.y * g.v_.y + v_.z * g.v_.z + v_.w * g.v_.w,
            xv_, yv_, zv_, wv_
        );
    }

    bool is_conservative() const {
        const cas::Expression* comps[4] = { &v_.x, &v_.y, &v_.z, &v_.w };
        const std::string* vars[4] = { &xv_,  &yv_,  &zv_,  &wv_  };
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = i + 1; j < 4; ++j)
                if (!cas::DIFFERENTIATE(*comps[i], *vars[j]).symbolic_equals(
                     cas::DIFFERENTIATE(*comps[j], *vars[i])))
                    return false;
        return true;
    }

public:
    VectorField4D directional_derivative(const cas::Expression& dx, const cas::Expression& dy,
                                         const cas::Expression& dz, const cas::Expression& dw) const {
        auto dd = [&](const cas::Expression& f) {
            return dx * cas::DIFFERENTIATE(f, xv_) + dy * cas::DIFFERENTIATE(f, yv_)
                 + dz * cas::DIFFERENTIATE(f, zv_) + dw * cas::DIFFERENTIATE(f, wv_);
        };
        return VectorField4D({ dd(v_.x), dd(v_.y), dd(v_.z), dd(v_.w) }, xv_, yv_, zv_, wv_);
    }

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
    VectorFieldN(vectors::SymbolicVectorN v, std::vector<std::string> vars) : v_(std::move(v)), vars_(std::move(vars)) {
        if (v_.dimension() != vars_.size())
            throw std::invalid_argument("VectorFieldN: component count ("
                + std::to_string(v_.dimension()) + ") != variable count (" + std::to_string(vars_.size()) + ")");
    }

    VectorFieldN(std::vector<cas::Expression> components, std::vector<std::string> vars) : VectorFieldN(vectors::SymbolicVectorN(std::move(components)), std::move(vars)) {}
    VectorFieldN(const VectorFieldN& o) : v_(o.v_), vars_(o.vars_) {}
    explicit VectorFieldN(const ScalarFieldN& sf) : v_(sf.gradient()), vars_(sf.vars()) {}

    VectorFieldN& operator=(const VectorFieldN& o) {
        v_ = o.v_; vars_ = o.vars_;
        cache_div_.reset(); cache_jac_.reset(); cache_lap_.reset();
        return *this;
    }

public:
    static VectorFieldN from_potential(const ScalarFieldN& sf) { return VectorFieldN(sf); }

public:
    std::size_t dimension()                         const noexcept { return vars_.size(); }
    const vectors::SymbolicVectorN& vec()           const { return v_; }
    const cas::Expression& component(std::size_t i) const { return v_[i]; }
    const std::vector<std::string>& vars()          const noexcept { return vars_; }
    const std::string& var(std::size_t i)           const { return vars_.at(i); }

public:
    ScalarFieldN divergence() const {
        if (!cache_div_) {
            cas::Expression d = cas::DIFFERENTIATE(v_[0], vars_[0]);
            for (std::size_t i = 1; i < dimension(); ++i) d = d + cas::DIFFERENTIATE(v_[i], vars_[i]);
            cache_div_ = std::make_unique<ScalarFieldN>(std::move(d), vars_);
        }

        return *cache_div_;
    }

    matrices::SymbolicMatrixN jacobian() const {
        if (!cache_jac_) {
            std::size_t n = dimension();
            matrices::SymbolicMatrixN J(n);

            for (std::size_t i = 0; i < n; ++i)
                for (std::size_t j = 0; j < n; ++j)
                    J.at(i, j) = cas::DIFFERENTIATE(v_[i], vars_[j]);

            cache_jac_ = std::make_unique<matrices::SymbolicMatrixN>(std::move(J));
        }
        return *cache_jac_;
    }

    VectorFieldN laplacian() const {
        if (!cache_lap_) {
            std::size_t n = dimension();
            vectors::SymbolicVectorN lap(n);

            for (std::size_t i = 0; i < n; ++i) {
                cas::Expression l = cas::DIFFERENTIATE(cas::DIFFERENTIATE(v_[i], vars_[0]), vars_[0]);
                for (std::size_t j = 1; j < n; ++j) l = l + cas::DIFFERENTIATE(cas::DIFFERENTIATE(v_[i], vars_[j]), vars_[j]);
                lap[i] = std::move(l);
            }

            cache_lap_ = std::make_unique<VectorFieldN>(std::move(lap), vars_);
        }
        return *cache_lap_;
    }

    VectorFieldN curl() const {
        if (dimension() == 3) {
            return VectorFieldN(vectors::SymbolicVectorN({
                cas::DIFFERENTIATE(v_[2], vars_[1]) - cas::DIFFERENTIATE(v_[1], vars_[2]),
                cas::DIFFERENTIATE(v_[0], vars_[2]) - cas::DIFFERENTIATE(v_[2], vars_[0]),
                cas::DIFFERENTIATE(v_[1], vars_[0]) - cas::DIFFERENTIATE(v_[0], vars_[1])
            }), vars_);
        }
        if (dimension() == 7) {
            const auto& F = v_;
            const auto& x = vars_;

            return VectorFieldN(vectors::SymbolicVectorN({
            /* e0 */ cas::DIFFERENTIATE(F[3],x[1]) - cas::DIFFERENTIATE(F[1],x[3])
                    + cas::DIFFERENTIATE(F[6],x[2]) - cas::DIFFERENTIATE(F[2],x[6])
                    + cas::DIFFERENTIATE(F[5],x[4]) - cas::DIFFERENTIATE(F[4],x[5]),

            /* e1 */ cas::DIFFERENTIATE(F[4],x[2]) - cas::DIFFERENTIATE(F[2],x[4])
                    + cas::DIFFERENTIATE(F[0],x[3]) - cas::DIFFERENTIATE(F[3],x[0])
                    + cas::DIFFERENTIATE(F[6],x[5]) - cas::DIFFERENTIATE(F[5],x[6]),

            /* e2 */ cas::DIFFERENTIATE(F[5],x[3]) - cas::DIFFERENTIATE(F[3],x[5])
                    + cas::DIFFERENTIATE(F[1],x[4]) - cas::DIFFERENTIATE(F[4],x[1])
                    + cas::DIFFERENTIATE(F[0],x[6]) - cas::DIFFERENTIATE(F[6],x[0]),

            /* e3 */ cas::DIFFERENTIATE(F[6],x[4]) - cas::DIFFERENTIATE(F[4],x[6])
                    + cas::DIFFERENTIATE(F[2],x[5]) - cas::DIFFERENTIATE(F[5],x[2])
                    + cas::DIFFERENTIATE(F[1],x[0]) - cas::DIFFERENTIATE(F[0],x[1]),

            /* e4 */ cas::DIFFERENTIATE(F[0],x[5]) - cas::DIFFERENTIATE(F[5],x[0])
                    + cas::DIFFERENTIATE(F[3],x[6]) - cas::DIFFERENTIATE(F[6],x[3])
                    + cas::DIFFERENTIATE(F[2],x[1]) - cas::DIFFERENTIATE(F[1],x[2]),

            /* e5 */ cas::DIFFERENTIATE(F[1],x[6]) - cas::DIFFERENTIATE(F[6],x[1])
                    + cas::DIFFERENTIATE(F[4],x[0]) - cas::DIFFERENTIATE(F[0],x[4])
                    + cas::DIFFERENTIATE(F[3],x[2]) - cas::DIFFERENTIATE(F[2],x[3]),

            /* e6 */ cas::DIFFERENTIATE(F[2],x[0]) - cas::DIFFERENTIATE(F[0],x[2])
                    + cas::DIFFERENTIATE(F[5],x[1]) - cas::DIFFERENTIATE(F[1],x[5])
                    + cas::DIFFERENTIATE(F[4],x[3]) - cas::DIFFERENTIATE(F[3],x[4])
            }), vars_);
        }
        throw std::domain_error("VectorFieldN::curl requires dimension 3 or 7");
    }

    ScalarFieldN dot(const VectorFieldN& g) const {
        assert(dimension() == g.dimension());
        cas::Expression d = v_[0] * g.v_[0];
        for (std::size_t i = 1; i < dimension(); ++i) d = d + v_[i] * g.v_[i];
        return ScalarFieldN(std::move(d), vars_);
    }

    VectorFieldN cross(const VectorFieldN& g) const {
        if (dimension() == 3) {
            return VectorFieldN(vectors::SymbolicVectorN({
                v_[1] * g.v_[2] - v_[2] * g.v_[1],
                v_[2] * g.v_[0] - v_[0] * g.v_[2],
                v_[0] * g.v_[1] - v_[1] * g.v_[0]
            }), vars_);
        }
        if (dimension() == 7) {
            const auto& a = v_;
            const auto& b = g.v_;
            return VectorFieldN(vectors::SymbolicVectorN({
                /* e0 */ a[1]*b[3] - a[3]*b[1] + a[2]*b[6] - a[6]*b[2] + a[4]*b[5] - a[5]*b[4],
                /* e1 */ a[2]*b[4] - a[4]*b[2] + a[3]*b[0] - a[0]*b[3] + a[5]*b[6] - a[6]*b[5],
                /* e2 */ a[3]*b[5] - a[5]*b[3] + a[4]*b[1] - a[1]*b[4] + a[6]*b[0] - a[0]*b[6],
                /* e3 */ a[4]*b[6] - a[6]*b[4] + a[5]*b[2] - a[2]*b[5] + a[0]*b[1] - a[1]*b[0],
                /* e4 */ a[5]*b[0] - a[0]*b[5] + a[6]*b[3] - a[3]*b[6] + a[1]*b[2] - a[2]*b[1],
                /* e5 */ a[6]*b[1] - a[1]*b[6] + a[0]*b[4] - a[4]*b[0] + a[2]*b[3] - a[3]*b[2],
                /* e6 */ a[0]*b[2] - a[2]*b[0] + a[1]*b[5] - a[5]*b[1] + a[3]*b[4] - a[4]*b[3]
            }), vars_);
        }
        throw std::domain_error("VectorFieldN::cross requires dimension 3 or 7");
    }

    bool is_conservative() const {
        std::size_t n = dimension();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                cas::Expression dFi_dxj = cas::DIFFERENTIATE(v_[i], vars_[j]);
                cas::Expression dFj_dxi = cas::DIFFERENTIATE(v_[j], vars_[i]);
                if (!dFi_dxj.symbolic_equals(dFj_dxi)) return false;
            }
        }
        return true;
    }

public:
    VectorFieldN directional_derivative(const vectors::SymbolicVectorN& d) const {
        assert(d.dimension() == dimension());
        std::size_t n = dimension();
        vectors::SymbolicVectorN result(n);

        for (std::size_t i = 0; i < n; ++i) {
            cas::Expression dd = d[0] * cas::DIFFERENTIATE(v_[i], vars_[0]);
            for (std::size_t j = 1; j < n; ++j)
                dd = dd + d[j] * cas::DIFFERENTIATE(v_[i], vars_[j]);
            result[i] = std::move(dd);
        }

        return VectorFieldN(std::move(result), vars_);
    }
    
    VectorFieldN directional_derivative_unit(const vectors::SymbolicVectorN& d_) const { return directional_derivative(d_.unit_vector()); }
    VectorFieldN directional_derivative(const std::vector<cas::Expression>& d)   const { return directional_derivative(vectors::SymbolicVectorN(d)); }

public:
    std::vector<double> evaluate(const std::unordered_map<std::string, double>& vals)        const { return v_.evaluate(vals); }
    std::vector<double> evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return v_.evaluate(vals); }

    std::vector<double> evaluate(const std::vector<double>& vals) const {
        std::unordered_map<std::string, double> m;
        for (std::size_t i = 0; i < std::min(vals.size(), vars_.size()); ++i) m[vars_[i]] = vals[i];
        return v_.evaluate(m);
    }

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