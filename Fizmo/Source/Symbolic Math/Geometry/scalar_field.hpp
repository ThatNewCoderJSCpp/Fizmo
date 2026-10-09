#ifndef FIZMO_SCALAR_FIELDS_HPP
#define FIZMO_SCALAR_FIELDS_HPP

#include "../Calculus/integrator_3d.hpp"
#include "parametric_surface.hpp"

namespace fizmo {
namespace math {
namespace geometry {

class ScalarField1D {
public:
    ScalarField1D(cas::Expression f, std::string xvar = "x") : f_(std::move(f)), xv_(std::move(xvar)) {}
    ScalarField1D(const ScalarField1D& o) : f_(o.f_), xv_(o.xv_) {}

    ScalarField1D& operator=(const ScalarField1D& o) {
        f_ = o.f_; xv_ = o.xv_;
        cache_dx_.reset(); cache_dxx_.reset();
        return *this;
    }

public:
    const cas::Expression&  expression() const { return f_;  }
    const std::string& xvar()       const { return xv_; }

public:
    cas::Expression derivative()        const;
    cas::Expression second_derivative() const;

    cas::Expression nth_derivative(unsigned int n) const;

public:
    cas::Expression gradient()  const { return derivative(); }
    cas::Expression laplacian() const { return second_derivative(); }

    ScalarField1D tangent_line(double a) const;

public:
    double evaluate(double x) const { return f_.evaluate({{ xv_, x }}); }
    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals); }
    ScalarField1D substitute(const std::string& var, double val)          const { return { SUBSTITUTE(f_, var, val), xv_ }; }
    ScalarField1D substitute(const std::string& var, const cas::Expression& r) const { return { SUBSTITUTE(f_, var, r),   xv_ }; }

    ScalarField1D substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, xv_ };
    }

    ScalarField1D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, xv_ };
    }

    ScalarField1D simplify()                                         const { return { cas::SIMPLIFY(f_),           xv_ }; }
    ScalarField1D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { cas::FULL_SIMPLIFY(f_, cfg), xv_ }; }
    ScalarField1D rewrite(const cas::RewriterConfig& cfg = {})       const { return { cas::REWRITE(f_, cfg),       xv_ }; }

public:
    ScalarField1D operator+(const ScalarField1D& o) const { return { f_ + o.f_, xv_ }; }
    ScalarField1D operator-(const ScalarField1D& o) const { return { f_ - o.f_, xv_ }; }
    ScalarField1D operator*(const ScalarField1D& o) const { return { f_ * o.f_, xv_ }; }
    ScalarField1D operator/(const ScalarField1D& o) const { return { f_ / o.f_, xv_ }; }
    ScalarField1D operator-()                       const { return { -f_,       xv_ }; }
    ScalarField1D operator+(const cas::Expression& s)    const { return { f_ + s,    xv_ }; }
    ScalarField1D operator-(const cas::Expression& s)    const { return { f_ - s,    xv_ }; }
    ScalarField1D operator*(const cas::Expression& s)    const { return { f_ * s,    xv_ }; }
    ScalarField1D operator/(const cas::Expression& s)    const { return { f_ / s,    xv_ }; }
    ScalarField1D operator+(double s)               const { return { f_ + s,    xv_ }; }
    ScalarField1D operator-(double s)               const { return { f_ - s,    xv_ }; }
    ScalarField1D operator*(double s)               const { return { f_ * s,    xv_ }; }
    ScalarField1D operator/(double s)               const { return { f_ / s,    xv_ }; }

public:
    integration::IntegrationResult integral(double a, double b, const integration::IntegrationLayer& layer = {}) const { return integration::Integrator::integrate(f_, xv_, a, b, layer); }
    integration::IntegrationResult integral(const Interval& iv, const integration::IntegrationLayer& layer = {}) const { return integration::Integrator::integrate(f_, xv_, iv, layer); }

    double average_value(double a, double b, const integration::IntegrationLayer& layer = {}) const {
        auto result = integral(a, b, layer);
        return result.value / (b - a);
    }

    double average_slope(double a, double b) const { return (evaluate(b) - evaluate(a)) / (b - a); }
    integration::IntegrationResult arclength(double a, double b, const integration::IntegrationLayer& layer = {}) const { return integration::Integrator::integrate(arclength_integrand(), xv_, a, b, layer); }

    cas::Expression arclength_integrand() const {
        cas::Expression fp = derivative();
        return SQRT(cas::Const(1.0) + fp * fp);
    }

public:
    vectors::SymbolicVector2 unit_tangent() const;

    vectors::SymbolicVector2 unit_normal() const;

    cas::Expression curvature() const;

    cas::Expression curvature_radius() const;

    ScalarField1D normal_line(double a) const;

public:
    cas::Expression parameterize(const cas::Expression& x_t) const { return SUBSTITUTE(f_, xv_, x_t); }

public:
    friend std::ostream& operator<<(std::ostream& os, const ScalarField1D& sf) {
        os << "f(" << sf.xv_ << ") = " << sf.f_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression f_;
    mutable std::unique_ptr<cas::Expression> cache_dx_, cache_dxx_;
    std::string xv_;
};

inline ScalarField1D operator+(const cas::Expression& s, const ScalarField1D& f) { return f + s; }
inline ScalarField1D operator*(const cas::Expression& s, const ScalarField1D& f) { return f * s; }
inline ScalarField1D operator+(double s,            const ScalarField1D& f) { return f + s; }
inline ScalarField1D operator*(double s,            const ScalarField1D& f) { return f * s; }

class ScalarField2D {
public:
    ScalarField2D(cas::Expression f, std::string xvar = "x", std::string yvar = "y") : f_(std::move(f)) , xv_(std::move(xvar)) , yv_(std::move(yvar)) {}
    ScalarField2D(const ScalarField2D& o) : f_(o.f_), xv_(o.xv_), yv_(o.yv_) {}
    ScalarField2D& operator=(const ScalarField2D& o);

public:
    const cas::Expression&  expression()  const { return f_;  }
    const std::string& xvar()        const { return xv_; }
    const std::string& yvar()        const { return yv_; }
    
public:
    cas::Expression partial_x()  const;
    cas::Expression partial_y()  const;
    cas::Expression partial_xx() const;
    cas::Expression partial_yy() const;
    cas::Expression partial_xy() const;
    cas::Expression partial_yx() const;
    
public:
    vectors::SymbolicVector2 gradient()             const { return { partial_x(), partial_y() }; }
    cas::Expression laplacian()            const { return partial_xx() + partial_yy(); }
    cas::Expression hessian_discriminant() const { return (partial_xx() * partial_yy()) - (partial_xy() * partial_yx()); }

    ScalarField2D tangent_plane(double a, double b) const;

public:
    cas::Expression directional_derivative(const cas::Expression& dx, const cas::Expression& dy) const { return dx * partial_x() + dy * partial_y(); }
    cas::Expression directional_derivative_unit(const vectors::SymbolicVector2& d) const { const auto d_n = d.unit_vector(); return directional_derivative(d_n.x, d_n.y); }
    cas::Expression directional_derivative(const vectors::SymbolicVector2& d) const { return directional_derivative(d.x, d.y); }

public:
    cas::Expression parameterize(const ParametricCurve2D& curve) const {
        return f_.substitute({
            { xv_, curve.r().x },
            { yv_, curve.r().y }
        });
    }

    cas::Expression parameterize(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const {
        return f_.substitute({
            { xv_, x_t },
            { yv_, y_t }
        });
    }

    cas::Expression parameterize(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return parameterize(r.x, r.y, param); }

public:
    cas::Expression line_integrand(const ParametricCurve2D& curve) const;

    cas::Expression line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const;

    cas::Expression line_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return line_integrand(r.x, r.y, param); }

public:
    integration::IntegrationResult line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, param, t0, t1, layer);
    }

public:
    cas::Expression surface_area_integrand() const;

    vectors::SymbolicVector3 surface_normal() const { return { -partial_x(), -partial_y(), cas::Const(1.0) }; }

public:
    // integrate 1 dA
    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D region_area(const Region& region, integration::IntegrationConfig2D cfg = {}) const;

public:
    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_area(const Region& region, integration::IntegrationConfig2D cfg = {}) const;

public:
    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D double_integral(const Region& region, integration::IntegrationConfig2D cfg = {}) const;
    
public:
    double evaluate(double x, double y) const { 
        return f_.evaluate({
            { xv_, x }, 
            { yv_, y }
        });  
    }

    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals);                      }
    ScalarField2D substitute(const std::string& var, double val)                const { return { SUBSTITUTE(f_, var, val), xv_, yv_ }; }
    ScalarField2D substitute(const std::string& var, const cas::Expression& r)  const { return { SUBSTITUTE(f_, var, r),   xv_, yv_ }; }

    ScalarField2D substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, xv_, yv_ };
    }

    ScalarField2D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, xv_, yv_ };
    }

    ScalarField2D simplify()                                                              const { return { SIMPLIFY(f_),           xv_, yv_ }; }
    ScalarField2D full_simplify(const cas::RewriterConfig& cfg = {})  const { return { FULL_SIMPLIFY(f_, cfg), xv_, yv_ }; }
    ScalarField2D rewrite(const cas::RewriterConfig& cfg = {})        const { return { REWRITE(f_, cfg),       xv_, yv_ }; }

public:
    ScalarField2D operator+(const ScalarField2D& o) const { return { f_ + o.f_, xv_, yv_ }; }
    ScalarField2D operator-(const ScalarField2D& o) const { return { f_ - o.f_, xv_, yv_ }; }
    ScalarField2D operator*(const ScalarField2D& o) const { return { f_ * o.f_, xv_, yv_ }; }
    ScalarField2D operator/(const ScalarField2D& o) const { return { f_ / o.f_, xv_, yv_ }; }
    ScalarField2D operator-()                       const { return { -f_,       xv_, yv_ }; }
    ScalarField2D operator+(const cas::Expression& s)    const { return { f_ + s,    xv_, yv_ }; }
    ScalarField2D operator-(const cas::Expression& s)    const { return { f_ - s,    xv_, yv_ }; }
    ScalarField2D operator*(const cas::Expression& s)    const { return { f_ * s,    xv_, yv_ }; }
    ScalarField2D operator/(const cas::Expression& s)    const { return { f_ / s,    xv_, yv_ }; }
    ScalarField2D operator+(double s)               const { return { f_ + s,    xv_, yv_ }; }
    ScalarField2D operator-(double s)               const { return { f_ - s,    xv_, yv_ }; }
    ScalarField2D operator*(double s)               const { return { f_ * s,    xv_, yv_ }; }
    ScalarField2D operator/(double s)               const { return { f_ / s,    xv_, yv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const ScalarField2D& sf) {
        os << "f(" << sf.xv_ << ", " << sf.yv_ << ") = " << sf.f_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression  f_;
    mutable std::unique_ptr<cas::Expression> cache_dx_, cache_dy_;
    mutable std::unique_ptr<cas::Expression> cache_dxx_, cache_dyy_, cache_dxy_, cache_dyx_;
    std::string xv_, yv_;
};

inline ScalarField2D operator+(const cas::Expression& s, const ScalarField2D& f) { return f + s; }
inline ScalarField2D operator*(const cas::Expression& s, const ScalarField2D& f) { return f * s; }
inline ScalarField2D operator+(double s,            const ScalarField2D& f) { return f + s; }
inline ScalarField2D operator*(double s,            const ScalarField2D& f) { return f * s; }

class ScalarField3D {
public:
    ScalarField3D(cas::Expression f, std::string xvar = "x", std::string yvar = "y", std::string zvar = "z") : f_(std::move(f)), xv_(std::move(xvar)), yv_(std::move(yvar)), zv_(std::move(zvar)) {}
    ScalarField3D(const ScalarField3D& o) : f_(o.f_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_) {}
    ScalarField3D& operator=(const ScalarField3D& o);
    
public:
    const cas::Expression&  expression()  const { return f_;  }
    const std::string& xvar()        const { return xv_; }
    const std::string& yvar()        const { return yv_; }
    const std::string& zvar()        const { return zv_; }

public:
    cas::Expression partial_x()  const;
    cas::Expression partial_y()  const;
    cas::Expression partial_z()  const;
    cas::Expression partial_xx() const;
    cas::Expression partial_yy() const;
    cas::Expression partial_zz() const;
    cas::Expression partial_xy() const;
    cas::Expression partial_xz() const;
    cas::Expression partial_yx() const;
    cas::Expression partial_yz() const;
    cas::Expression partial_zx() const;
    cas::Expression partial_zy() const;

public:
    vectors::SymbolicVector3 gradient()  const { return { partial_x(), partial_y(), partial_z() }; }
    cas::Expression laplacian()      const { return partial_xx() + partial_yy() + partial_zz(); }

    ScalarField3D tangent_plane(double a, double b, double c) const;

    matrices::SymbolicMatrix3x3 hessian() const;

public:
    cas::Expression directional_derivative(const cas::Expression& dx, const cas::Expression& dy, const cas::Expression& dz) const { return dx * partial_x() + dy * partial_y() + dz * partial_z(); }

    cas::Expression directional_derivative_unit(const vectors::SymbolicVector3& d) const { 
        const auto d_n = d.unit_vector(); 
        return directional_derivative(d_n.x, d_n.y, d_n.z); 
    }
    
    cas::Expression directional_derivative(const vectors::SymbolicVector3& d) const { return directional_derivative(d.x, d.y, d.z); }

public:
    cas::Expression parameterize(const ParametricCurve& curve) const {
        return f_.substitute({
            { xv_, curve.r().x },
            { yv_, curve.r().y },
            { zv_, curve.r().z }
        });
    }

    cas::Expression parameterize(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const {
        return f_.substitute({
            { xv_, x_t },
            { yv_, y_t },
            { zv_, z_t }
        });
    }

    cas::Expression parameterize(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return parameterize(r.x, r.y, r.z, param); }

    cas::Expression parameterize(const ParametricSurface& surface) const {
        return f_.substitute({
            { xv_, surface.r().x },
            { yv_, surface.r().y },
            { zv_, surface.r().z }
        });
    }

public:
    cas::Expression line_integrand(const ParametricCurve& curve) const;

    cas::Expression line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const;

    cas::Expression line_integrand(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return line_integrand(r.x, r.y, r.z, param); }

public:
    integration::IntegrationResult line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const;

    integration::IntegrationResult line_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    cas::Expression surface_area_integrand() const;

    cas::Expression surface_integrand(const ParametricSurface& surface) const;

    vectors::SymbolicVector3 surface_normal() const { return gradient(); }

public:
    // Integrate 1 dV
    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D region_volume(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const;

public:
    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D surface_area(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const;

public:
    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const;

    integration::IntegrationResult3D triple_integral(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const;

public:
    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const;

public:
    double evaluate(double x, double y, double z) const { 
        return f_.evaluate({
            { xv_, x }, 
            { yv_, y }, 
            { zv_, z }
        }); 
    }

    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals);                           }
    ScalarField3D substitute(const std::string& var, double val)                const { return { SUBSTITUTE(f_, var, val), xv_, yv_, zv_ }; }
    ScalarField3D substitute(const std::string& var, const cas::Expression& r)       const { return { SUBSTITUTE(f_, var, r),   xv_, yv_, zv_ }; }

    ScalarField3D substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, xv_, yv_, zv_ };
    }

    ScalarField3D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, xv_, yv_, zv_ };
    }

    ScalarField3D simplify()                                                             const { return { SIMPLIFY(f_),           xv_, yv_, zv_ }; }
    ScalarField3D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { FULL_SIMPLIFY(f_, cfg), xv_, yv_, zv_ }; }
    ScalarField3D rewrite(const cas::RewriterConfig& cfg = {})       const { return { REWRITE(f_, cfg),       xv_, yv_, zv_ }; }

public:
    ScalarField3D operator+(const ScalarField3D& o) const { return { f_ + o.f_, xv_, yv_, zv_ }; }
    ScalarField3D operator-(const ScalarField3D& o) const { return { f_ - o.f_, xv_, yv_, zv_ }; }
    ScalarField3D operator*(const ScalarField3D& o) const { return { f_ * o.f_, xv_, yv_, zv_ }; }
    ScalarField3D operator/(const ScalarField3D& o) const { return { f_ / o.f_, xv_, yv_, zv_ }; }
    ScalarField3D operator-()                       const { return { -f_,       xv_, yv_, zv_ }; }
    ScalarField3D operator+(const cas::Expression& s)    const { return { f_ + s,    xv_, yv_, zv_ }; }
    ScalarField3D operator-(const cas::Expression& s)    const { return { f_ - s,    xv_, yv_, zv_ }; }
    ScalarField3D operator*(const cas::Expression& s)    const { return { f_ * s,    xv_, yv_, zv_ }; }
    ScalarField3D operator/(const cas::Expression& s)    const { return { f_ / s,    xv_, yv_, zv_ }; }
    ScalarField3D operator+(double s)               const { return { f_ + s,    xv_, yv_, zv_ }; }
    ScalarField3D operator-(double s)               const { return { f_ - s,    xv_, yv_, zv_ }; }
    ScalarField3D operator*(double s)               const { return { f_ * s,    xv_, yv_, zv_ }; }
    ScalarField3D operator/(double s)               const { return { f_ / s,    xv_, yv_, zv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const ScalarField3D& sf) {
        os << "f(" << sf.xv_ << ", " << sf.yv_ << ", " << sf.zv_ << ") = " << sf.f_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression  f_;
    mutable std::unique_ptr<cas::Expression> cache_dx_, cache_dy_, cache_dz_;
    mutable std::unique_ptr<cas::Expression> cache_dxx_, cache_dyy_, cache_dzz_;
    mutable std::unique_ptr<cas::Expression> cache_dxy_, cache_dxz_, cache_dyx_, cache_dyz_, cache_dzx_, cache_dzy_;
    std::string xv_, yv_, zv_;
};

inline ScalarField3D operator+(const cas::Expression& s, const ScalarField3D& f) { return f + s; }
inline ScalarField3D operator*(const cas::Expression& s, const ScalarField3D& f) { return f * s; }
inline ScalarField3D operator+(double s,            const ScalarField3D& f) { return f + s; }
inline ScalarField3D operator*(double s,            const ScalarField3D& f) { return f * s; }

class ScalarField4D {
public:
    ScalarField4D(cas::Expression f,
                  std::string xvar = "x", std::string yvar = "y",
                  std::string zvar = "z", std::string wvar = "w")
;

    ScalarField4D(const ScalarField4D& o) : f_(o.f_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_), wv_(o.wv_) {}

    ScalarField4D& operator=(const ScalarField4D& o);

public:
    const cas::Expression&  expression() const { return f_;  }
    const std::string& xvar()      const { return xv_; }
    const std::string& yvar()      const { return yv_; }
    const std::string& zvar()      const { return zv_; }
    const std::string& wvar()      const { return wv_; }

public:
    cas::Expression partial_x() const;
    cas::Expression partial_y() const;
    cas::Expression partial_z() const;
    cas::Expression partial_w() const;

    cas::Expression partial_xx() const;
    cas::Expression partial_yy() const;
    cas::Expression partial_zz() const;
    cas::Expression partial_ww() const;

    cas::Expression partial_xy() const;
    cas::Expression partial_xz() const;
    cas::Expression partial_xw() const;
    cas::Expression partial_yx() const;
    cas::Expression partial_yz() const;
    cas::Expression partial_yw() const;
    cas::Expression partial_zx() const;
    cas::Expression partial_zy() const;
    cas::Expression partial_zw() const;
    cas::Expression partial_wx() const;
    cas::Expression partial_wy() const;
    cas::Expression partial_wz() const;

public:
    vectors::SymbolicVector4 gradient() const { return { partial_x(), partial_y(), partial_z(), partial_w() }; }
    cas::Expression laplacian()     const { return partial_xx() + partial_yy() + partial_zz() + partial_ww(); }

    matrices::SymbolicMatrix4x4 hessian() const;

    ScalarField4D tangent_hyperplane(double a, double b, double c, double d) const;

public:
    cas::Expression directional_derivative(const cas::Expression& dx, const cas::Expression& dy, const cas::Expression& dz, const cas::Expression& dw) const {
        return dx * partial_x() + dy * partial_y() + dz * partial_z() + dw * partial_w();
    }

    cas::Expression directional_derivative(const vectors::SymbolicVector4& d) const {
        return directional_derivative(d.x, d.y, d.z, d.w);
    }

    cas::Expression directional_derivative_unit(const vectors::SymbolicVector4& d) const {
        const auto d_n = d.unit_vector();
        return directional_derivative(d_n.x, d_n.y, d_n.z, d_n.w);
    }

public:
    double evaluate(double x_, double y_, double z_, double w_) const {
        return f_.evaluate({{ xv_, x_ }, { yv_, y_ }, { zv_, z_ }, { wv_, w_ }});
    }
    double evaluate(const std::unordered_map<std::string, double>& vals)        const { return f_.evaluate(vals); }
    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals); }

    ScalarField4D substitute(const std::string& var, double val) const {
        return { SUBSTITUTE(f_, var, val), xv_, yv_, zv_, wv_ };
    }
    ScalarField4D substitute(const std::string& var, const cas::Expression& r) const {
        return { SUBSTITUTE(f_, var, r), xv_, yv_, zv_, wv_ };
    }

    ScalarField4D substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, xv_, yv_, zv_, wv_ };
    }

    ScalarField4D substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, xv_, yv_, zv_, wv_ };
    }

    ScalarField4D simplify()                                                             const { return { SIMPLIFY(f_),           xv_, yv_, zv_, wv_ }; }
    ScalarField4D full_simplify(const cas::RewriterConfig& cfg = {}) const { return { FULL_SIMPLIFY(f_, cfg), xv_, yv_, zv_, wv_ }; }
    ScalarField4D rewrite(const cas::RewriterConfig& cfg = {})       const { return { REWRITE(f_, cfg),       xv_, yv_, zv_, wv_ }; }

public:
    ScalarField4D operator+(const ScalarField4D& o) const { return { f_ + o.f_, xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator-(const ScalarField4D& o) const { return { f_ - o.f_, xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator*(const ScalarField4D& o) const { return { f_ * o.f_, xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator/(const ScalarField4D& o) const { return { f_ / o.f_, xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator-()                       const { return { -f_,       xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator+(const cas::Expression& s)    const { return { f_ + s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator-(const cas::Expression& s)    const { return { f_ - s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator*(const cas::Expression& s)    const { return { f_ * s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator/(const cas::Expression& s)    const { return { f_ / s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator+(double s)               const { return { f_ + s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator-(double s)               const { return { f_ - s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator*(double s)               const { return { f_ * s,    xv_, yv_, zv_, wv_ }; }
    ScalarField4D operator/(double s)               const { return { f_ / s,    xv_, yv_, zv_, wv_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const ScalarField4D& sf) {
        os << "f(" << sf.xv_ << ", " << sf.yv_ << ", " << sf.zv_ << ", " << sf.wv_ << ") = " << sf.f_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression f_;
    mutable std::unique_ptr<cas::Expression> cache_dx_, cache_dy_, cache_dz_, cache_dw_;
    mutable std::unique_ptr<cas::Expression> cache_dxx_, cache_dyy_, cache_dzz_, cache_dww_;
    mutable std::unique_ptr<cas::Expression> cache_dxy_, cache_dxz_, cache_dxw_;
    mutable std::unique_ptr<cas::Expression> cache_dyx_, cache_dyz_, cache_dyw_;
    mutable std::unique_ptr<cas::Expression> cache_dzx_, cache_dzy_, cache_dzw_;
    mutable std::unique_ptr<cas::Expression> cache_dwx_, cache_dwy_, cache_dwz_;
    std::string xv_, yv_, zv_, wv_;
};

inline ScalarField4D operator+(const cas::Expression& s, const ScalarField4D& f) { return f + s; }
inline ScalarField4D operator*(const cas::Expression& s, const ScalarField4D& f) { return f * s; }
inline ScalarField4D operator+(double s,            const ScalarField4D& f) { return f + s; }
inline ScalarField4D operator*(double s,            const ScalarField4D& f) { return f * s; }

class ScalarFieldN {
public:
    ScalarFieldN(cas::Expression f, std::vector<std::string> vars) : f_(std::move(f)), vars_(std::move(vars)) {}
    ScalarFieldN(const ScalarFieldN& o) : f_(o.f_), vars_(o.vars_) {}

    ScalarFieldN& operator=(const ScalarFieldN& o);

public:
    const cas::Expression& expression()         const { return f_; }
    const std::string& var(std::size_t i)  const { return vars_.at(i); }
    std::size_t dimension()                const noexcept { return vars_.size(); }
    const std::vector<std::string>& vars() const noexcept { return vars_; }

public:
    cas::Expression partial(std::size_t i) const {
        ensure_partials();
        return (*cache_partials_)[i];
    }

    cas::Expression partial(const std::string& var) const { return cas::DIFFERENTIATE(f_, var); }
    cas::Expression partial2(std::size_t i, std::size_t j) const { return cas::DIFFERENTIATE(partial(i), vars_[j]); }

public:
    vectors::SymbolicVectorN gradient() const {
        ensure_partials();
        return vectors::SymbolicVectorN(*cache_partials_);
    }

    ScalarFieldN tangent_plane(const std::vector<double>& point) const;

    ScalarFieldN tangent_plane(std::initializer_list<double> point) const { return tangent_plane(std::vector<double>(point)); }

    matrices::SymbolicMatrixN hessian() const;

    cas::Expression laplacian() const;

public:
    cas::Expression directional_derivative_unit(const vectors::SymbolicVectorN& d_) const;

    cas::Expression directional_derivative(const vectors::SymbolicVectorN& d) const;

    cas::Expression directional_derivative(const std::vector<cas::Expression>& d) const { return directional_derivative(vectors::SymbolicVectorN(d)); }

public:
    double evaluate(const std::unordered_map<std::string, double>& vals)        const { return f_.evaluate(vals); }
    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals); }

    double evaluate(const std::vector<double>& vals) const;

    ScalarFieldN substitute(const std::string& var, double val)             const { return { SUBSTITUTE(f_, var, val),  vars_ }; }
    ScalarFieldN substitute(const std::string& var, const cas::Expression& repl) const { return { SUBSTITUTE(f_, var, repl), vars_ }; }

    ScalarFieldN substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        auto sf = f_;
        for (auto& [n, v] : pairs) sf = SUBSTITUTE(sf, n, v);
        return { sf, vars_ };
    }

    ScalarFieldN substitute(std::initializer_list<std::pair<std::string, cas::Expression>> pairs) const {
        auto sf = f_;
        for (auto& [n, r] : pairs) sf = SUBSTITUTE(sf, n, r);
        return { sf, vars_ };
    }

    ScalarFieldN simplify()                                                             const { return { SIMPLIFY(f_),           vars_ }; }
    ScalarFieldN full_simplify(const cas::RewriterConfig& cfg = {}) const { return { FULL_SIMPLIFY(f_, cfg), vars_ }; }
    ScalarFieldN rewrite(const cas::RewriterConfig& cfg = {})       const { return { REWRITE(f_, cfg),       vars_ }; }

public:
    ScalarFieldN operator+(const ScalarFieldN& o) const { return { f_ + o.f_, vars_ }; }
    ScalarFieldN operator-(const ScalarFieldN& o) const { return { f_ - o.f_, vars_ }; }
    ScalarFieldN operator*(const ScalarFieldN& o) const { return { f_ * o.f_, vars_ }; }
    ScalarFieldN operator/(const ScalarFieldN& o) const { return { f_ / o.f_, vars_ }; }
    ScalarFieldN operator-()                      const { return { -f_,       vars_ }; }
    ScalarFieldN operator+(const cas::Expression& s)   const { return { f_ + s,    vars_ }; }
    ScalarFieldN operator-(const cas::Expression& s)   const { return { f_ - s,    vars_ }; }
    ScalarFieldN operator*(const cas::Expression& s)   const { return { f_ * s,    vars_ }; }
    ScalarFieldN operator/(const cas::Expression& s)   const { return { f_ / s,    vars_ }; }
    ScalarFieldN operator+(double s)              const { return { f_ + s,    vars_ }; }
    ScalarFieldN operator-(double s)              const { return { f_ - s,    vars_ }; }
    ScalarFieldN operator*(double s)              const { return { f_ * s,    vars_ }; }
    ScalarFieldN operator/(double s)              const { return { f_ / s,    vars_ }; }

public:
    friend std::ostream& operator<<(std::ostream& os, const ScalarFieldN& sf) {
        os << "f(";

        for (std::size_t i = 0; i < sf.vars_.size(); ++i) {
            os << sf.vars_[i];
            if (i + 1 < sf.vars_.size()) os << ", ";
        }

        os << ") = " << sf.f_.full_simplify();
        return os;
    }

    std::string to_string() const { std::ostringstream os; os << *this; return os.str(); }

private:
    cas::Expression f_;
    std::vector<std::string> vars_;
    mutable std::shared_ptr<std::vector<cas::Expression>> cache_partials_;
    mutable std::unique_ptr<matrices::SymbolicMatrixN>         cache_hessian_;
    mutable std::unique_ptr<cas::Expression>              cache_laplacian_;

    void ensure_partials() const;
};

inline ScalarFieldN operator+(const cas::Expression& s, const ScalarFieldN& f) { return f + s; }
inline ScalarFieldN operator*(const cas::Expression& s, const ScalarFieldN& f) { return f * s; }
inline ScalarFieldN operator+(double s,            const ScalarFieldN& f) { return f + s; }
inline ScalarFieldN operator*(double s,            const ScalarFieldN& f) { return f * s; }

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_SCALAR_FIELDS_HPP