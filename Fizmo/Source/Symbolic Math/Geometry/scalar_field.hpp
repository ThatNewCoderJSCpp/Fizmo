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
    cas::Expression derivative()        const { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_;  }
    cas::Expression second_derivative() const { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(derivative(), xv_)); return *cache_dxx_; }

    cas::Expression nth_derivative(unsigned int n) const {
        if (n == 0) return f_;
        if (n == 1) return derivative();
        if (n == 2) return second_derivative();
        return cas::DIFFERENTIATE_ITERATIVE(f_, xv_, n);
    }

public:
    cas::Expression gradient()  const { return derivative(); }
    cas::Expression laplacian() const { return second_derivative(); }

    ScalarField1D tangent_line(double a) const {
        double f_a  = evaluate(a);
        double fp_a = EVALUATE(derivative(), {{ xv_, a }});
        cas::Expression result = cas::Const(f_a) + cas::Const(fp_a) * (cas::VARIABLE(xv_) - cas::Const(a));
        return ScalarField1D(SIMPLIFY(result), xv_);
    }

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
    vectors::SymbolicVector2 unit_tangent() const {
        cas::Expression fp = derivative();
        cas::Expression s  = SQRT(cas::Const(1.0) + fp * fp);
        return { cas::Const(1.0) / s, fp / s };
    }

    vectors::SymbolicVector2 unit_normal() const {
        cas::Expression fp = derivative();
        cas::Expression s  = SQRT(cas::Const(1.0) + fp * fp);
        return { -fp / s, cas::Const(1.0) / s };
    }

    cas::Expression curvature() const {
        cas::Expression fp  = derivative();
        cas::Expression fpp = second_derivative();
        cas::Expression denom = SQRT(cas::Const(1.0) + fp * fp);   
        return fpp / (denom * denom * denom);             
    }

    cas::Expression curvature_radius() const {
        cas::Expression fp  = derivative();
        cas::Expression fpp = second_derivative();
        cas::Expression num = SQRT(cas::Const(1.0) + fp * fp);
        return (num * num * num) / ABS(fpp);              
    }

    ScalarField1D normal_line(double a) const {
        double f_a  = evaluate(a);
        double fp_a = EVALUATE(derivative(), {{ xv_, a }});
        cas::Expression result;

        if (std::abs(fp_a) <= constants::middle_epsilon()) {
            result = cas::Const(f_a);
        } else {
            double m_n = -1.0 / fp_a;
            result = cas::Const(f_a) + cas::Const(m_n) * (cas::VARIABLE(xv_) - cas::Const(a));
        }

        return ScalarField1D(SIMPLIFY(result), xv_);
    }

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
    ScalarField2D& operator=(const ScalarField2D& o) { f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; cache_dx_.reset(); cache_dy_.reset(); cache_dxx_.reset(); cache_dyy_.reset(); cache_dxy_.reset(); cache_dyx_.reset(); return *this; }

public:
    const cas::Expression&  expression()  const { return f_;  }
    const std::string& xvar()        const { return xv_; }
    const std::string& yvar()        const { return yv_; }
    
public:
    cas::Expression partial_x()  const { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_;  }
    cas::Expression partial_y()  const { if (!cache_dy_)  cache_dy_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_)); return *cache_dy_;  }
    cas::Expression partial_xx() const { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }
    cas::Expression partial_yy() const { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }
    cas::Expression partial_xy() const { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }
    cas::Expression partial_yx() const { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }
    
public:
    vectors::SymbolicVector2 gradient()             const { return { partial_x(), partial_y() }; }
    cas::Expression laplacian()            const { return partial_xx() + partial_yy(); }
    cas::Expression hessian_discriminant() const { return (partial_xx() * partial_yy()) - (partial_xy() * partial_yx()); }

    ScalarField2D tangent_plane(double a, double b) const {
        double f_ab  = evaluate(a, b);
        double fx_ab = EVALUATE(partial_x(), {{ xv_, a }, { yv_, b }});
        double fy_ab = EVALUATE(partial_y(), {{ xv_, a }, { yv_, b }});

        cas::Expression result = cas::Const(f_ab)
            + cas::Const(fx_ab) * (cas::VARIABLE(xv_) - cas::Const(a))
            + cas::Const(fy_ab) * (cas::VARIABLE(yv_) - cas::Const(b));
        return ScalarField2D(SIMPLIFY(result), xv_, yv_);
    }

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
    cas::Expression line_integrand(const ParametricCurve2D& curve) const {
        cas::Expression f_on_curve = parameterize(curve);
        cas::Expression speed = curve.speed();
        return f_on_curve * speed;
    }

    cas::Expression line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param = "t") const {
        cas::Expression f_on_curve = parameterize(x_t, y_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression speed = SQRT(xp * xp + yp * yp);
        return f_on_curve * speed;
    }

    cas::Expression line_integrand(const vectors::SymbolicVector2& r, const std::string& param = "t") const { return line_integrand(r.x, r.y, param); }

public:
    integration::IntegrationResult line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = line_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = line_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const vectors::SymbolicVector2& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, param, t0, t1, layer);
    }

public:
    cas::Expression surface_area_integrand() const {
        cas::Expression fx = partial_x();
        cas::Expression fy = partial_y();
        return SQRT(cas::Const(1.0) + fx * fx + fy * fy);
    }

    vectors::SymbolicVector3 surface_normal() const { return { -partial_x(), -partial_y(), cas::Const(1.0) }; }

public:
    // integrate 1 dA
    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            cas::Const(1.0),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            [](double, double) -> double { return 1.0; },
            outer_lo, outer_hi,
            std::move(inner_lo),
            std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult2D region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            cas::Const(1.0),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult2D region_area(const Region& region, integration::IntegrationConfig2D cfg = {}) const {
        auto f = [&region](double x, double y) -> double {
            return region.contains(x, y) ? 1.0 : 0.0;
        };
        return integration::Integrator2D::integrate(
            f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            cfg
        );
    }

public:
    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            surface_area_integrand(),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        cas::Expression sa = surface_area_integrand();
        const std::string& ov = outer_var;
        const std::string& iv = inner_var;

        auto f2d = [sa, ov, iv](double o, double i) -> double {
            return sa.evaluate({{ ov, o }, { iv, i }});
        };

        return integration::Integrator2D::integrate(
            f2d,
            outer_lo, outer_hi,
            std::move(inner_lo),
            std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult2D surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            surface_area_integrand(),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult2D surface_area(const Region& region, integration::IntegrationConfig2D cfg = {}) const {
        cas::Expression sa = surface_area_integrand();
        const std::string& xn = xv_;
        const std::string& yn = yv_;
        auto f = [sa, &xn, &yn, &region](double x, double y) -> double {
            return region.contains(x, y) ? sa.evaluate({{ xn, x }, { yn, y }}) : 0.0;
        };
        return integration::Integrator2D::integrate(
            f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            cfg
        );
    }

public:
    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, std::move(inner_lo), std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult2D double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult2D double_integral(const Region& region, integration::IntegrationConfig2D cfg = {}) const {
        const cas::Expression& expr = f_;
        const std::string& xn = xv_;
        const std::string& yn = yv_;
        auto f = [&expr, &xn, &yn, &region](double x, double y) -> double {
            return region.contains(x, y) ? expr.evaluate({{ xn, x }, { yn, y }}) : 0.0;
        };
        return integration::Integrator2D::integrate(f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            cfg
        );
    }
    
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
    ScalarField3D& operator=(const ScalarField3D& o) { f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; cache_dx_.reset(); cache_dy_.reset(); cache_dz_.reset(); cache_dxx_.reset(); cache_dyy_.reset(); cache_dzz_.reset(); cache_dxy_.reset(); cache_dxz_.reset(); cache_dyx_.reset(); cache_dyz_.reset(); cache_dzx_.reset(); cache_dzy_.reset(); return *this; }
    
public:
    const cas::Expression&  expression()  const { return f_;  }
    const std::string& xvar()        const { return xv_; }
    const std::string& yvar()        const { return yv_; }
    const std::string& zvar()        const { return zv_; }

public:
    cas::Expression partial_x()  const { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_));          return *cache_dx_;  }
    cas::Expression partial_y()  const { if (!cache_dy_)  cache_dy_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_));          return *cache_dy_;  }
    cas::Expression partial_z()  const { if (!cache_dz_)  cache_dz_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, zv_));          return *cache_dz_;  }
    cas::Expression partial_xx() const { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }
    cas::Expression partial_yy() const { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }
    cas::Expression partial_zz() const { if (!cache_dzz_) cache_dzz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), zv_)); return *cache_dzz_; }
    cas::Expression partial_xy() const { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }
    cas::Expression partial_xz() const { if (!cache_dxz_) cache_dxz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), zv_)); return *cache_dxz_; }
    cas::Expression partial_yx() const { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }
    cas::Expression partial_yz() const { if (!cache_dyz_) cache_dyz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), zv_)); return *cache_dyz_; }
    cas::Expression partial_zx() const { if (!cache_dzx_) cache_dzx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), xv_)); return *cache_dzx_; }
    cas::Expression partial_zy() const { if (!cache_dzy_) cache_dzy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), yv_)); return *cache_dzy_; }

public:
    vectors::SymbolicVector3 gradient()  const { return { partial_x(), partial_y(), partial_z() }; }
    cas::Expression laplacian()      const { return partial_xx() + partial_yy() + partial_zz(); }

    ScalarField3D tangent_plane(double a, double b, double c) const {
        double f_abc  = evaluate(a, b, c);
        double fx_abc = EVALUATE(partial_x(), {{ xv_, a }, { yv_, b }, { zv_, c }});
        double fy_abc = EVALUATE(partial_y(), {{ xv_, a }, { yv_, b }, { zv_, c }});
        double fz_abc = EVALUATE(partial_z(), {{ xv_, a }, { yv_, b }, { zv_, c }});

        cas::Expression result = cas::Const(f_abc)
            + cas::Const(fx_abc) * (cas::VARIABLE(xv_) - cas::Const(a))
            + cas::Const(fy_abc) * (cas::VARIABLE(yv_) - cas::Const(b))
            + cas::Const(fz_abc) * (cas::VARIABLE(zv_) - cas::Const(c));

        return ScalarField3D(SIMPLIFY(result), xv_, yv_, zv_);
    }

    matrices::SymbolicMatrix3x3 hessian() const {
        matrices::SymbolicMatrix3x3 H;
        H.get_data()[0][0] = partial_xx(); H.get_data()[0][1] = partial_xy(); H.get_data()[0][2] = partial_xz();
        H.get_data()[1][0] = partial_yx(); H.get_data()[1][1] = partial_yy(); H.get_data()[1][2] = partial_yz();
        H.get_data()[2][0] = partial_zx(); H.get_data()[2][1] = partial_zy(); H.get_data()[2][2] = partial_zz();
        return H;
    }

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
    cas::Expression line_integrand(const ParametricCurve& curve) const {
        cas::Expression f_on_curve = parameterize(curve);
        cas::Expression speed = curve.speed();
        return f_on_curve * speed;
    }

    cas::Expression line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param = "t") const {
        cas::Expression f_on_curve = parameterize(x_t, y_t, z_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression zp = cas::DIFFERENTIATE(z_t, param);
        cas::Expression speed = SQRT(xp * xp + yp * yp + zp * zp);
        return f_on_curve * speed;
    }

    cas::Expression line_integrand(const vectors::SymbolicVector3& r, const std::string& param = "t") const { return line_integrand(r.x, r.y, r.z, param); }

public:
    integration::IntegrationResult line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = line_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        cas::Expression integrand = line_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

    integration::IntegrationResult line_integral(const vectors::SymbolicVector3& r, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer = {}) const {
        return line_integral(r.x, r.y, r.z, param, t0, t1, layer);
    }

public:
    cas::Expression surface_area_integrand() const {
        cas::Expression fx = partial_x();
        cas::Expression fy = partial_y();
        cas::Expression fz = partial_z();
        return SQRT(cas::Const(1.0) + fx * fx + fy * fy + fz * fz);
    }

    cas::Expression surface_integrand(const ParametricSurface& surface) const {
        cas::Expression f_on_surface = parameterize(surface);
        return f_on_surface * surface.area_element();
    }

    vectors::SymbolicVector3 surface_normal() const { return gradient(); }

public:
    // Integrate 1 dV
    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            cas::Const(1.0),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            [](double, double, double) -> double { return 1.0; },
            outer_lo, outer_hi,
            std::move(middle_lo), std::move(middle_hi),
            std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult3D region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            cas::Const(1.0),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult3D region_volume(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const {
        auto f = [&region](double x, double y, double z) -> double {
            return region.contains(x, y, z) ? 1.0 : 0.0;
        };
        return integration::Integrator3D::integrate(
            f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            integration::IntegrationBound2(region.z_range.lower.value),
            integration::IntegrationBound2(region.z_range.upper.value),
            cfg
        );
    }

public:
    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            surface_area_integrand(),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        cas::Expression sa = surface_area_integrand();
        const std::string& on = outer_var;
        const std::string& mn = middle_var;
        const std::string& in_ = inner_var;
        auto f = [sa, on, mn, in_](double o, double m, double i) -> double {
            return sa.evaluate({{ on, o }, { mn, m }, { in_, i }});
        };
        return integration::Integrator3D::integrate(
            f,
            outer_lo, outer_hi,
            std::move(middle_lo), std::move(middle_hi),
            std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult3D surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            surface_area_integrand(),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult3D surface_area(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const {
        cas::Expression sa = surface_area_integrand();
        const std::string& xn = xv_;
        const std::string& yn = yv_;
        const std::string& zn = zv_;
        auto f = [sa, &xn, &yn, &zn, &region](double x, double y, double z) -> double {
            return region.contains(x, y, z) ? sa.evaluate({{ xn, x }, { yn, y }, { zn, z }}) : 0.0;
        };
        return integration::Integrator3D::integrate(
            f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            integration::IntegrationBound2(region.z_range.lower.value),
            integration::IntegrationBound2(region.z_range.upper.value),
            cfg
        );
    }

public:
    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, std::move(middle_lo), std::move(middle_hi),
            inner_var,  std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

    integration::IntegrationResult3D triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg = {}
    ) const {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

    integration::IntegrationResult3D triple_integral(const Region3D& region, integration::IntegrationConfig3D cfg = {}) const {
        const cas::Expression& expr = f_;
        const std::string& xn = xv_;
        const std::string& yn = yv_;
        const std::string& zn = zv_;
        auto f = [&expr, &xn, &yn, &zn, &region](double x, double y, double z) -> double {
            return region.contains(x, y, z) ? expr.evaluate({{ xn, x }, { yn, y }, { zn, z }}) : 0.0;
        };
        return integration::Integrator3D::integrate(f,
            region.x_range.lower.value, region.x_range.upper.value,
            integration::IntegrationBound(region.y_range.lower.value),
            integration::IntegrationBound(region.y_range.upper.value),
            integration::IntegrationBound2(region.z_range.lower.value),
            integration::IntegrationBound2(region.z_range.upper.value),
            cfg
        );
    }

public:
    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            surface_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        cas::Expression integrand = surface_integrand(surface);
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

    integration::IntegrationResult2D surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg = {}
    ) const {
        return integration::Integrator2D::integrate(
            surface_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

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
        : f_(std::move(f)), xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

    ScalarField4D(const ScalarField4D& o) : f_(o.f_), xv_(o.xv_), yv_(o.yv_), zv_(o.zv_), wv_(o.wv_) {}

    ScalarField4D& operator=(const ScalarField4D& o) {
        f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; wv_ = o.wv_;
        cache_dx_.reset(); cache_dy_.reset(); cache_dz_.reset(); cache_dw_.reset();
        cache_dxx_.reset(); cache_dyy_.reset(); cache_dzz_.reset(); cache_dww_.reset();
        cache_dxy_.reset(); cache_dxz_.reset(); cache_dxw_.reset();
        cache_dyx_.reset(); cache_dyz_.reset(); cache_dyw_.reset();
        cache_dzx_.reset(); cache_dzy_.reset(); cache_dzw_.reset();
        cache_dwx_.reset(); cache_dwy_.reset(); cache_dwz_.reset();
        return *this;
    }

public:
    const cas::Expression&  expression() const { return f_;  }
    const std::string& xvar()      const { return xv_; }
    const std::string& yvar()      const { return yv_; }
    const std::string& zvar()      const { return zv_; }
    const std::string& wvar()      const { return wv_; }

public:
    cas::Expression partial_x() const { if (!cache_dx_) cache_dx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_; }
    cas::Expression partial_y() const { if (!cache_dy_) cache_dy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_)); return *cache_dy_; }
    cas::Expression partial_z() const { if (!cache_dz_) cache_dz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, zv_)); return *cache_dz_; }
    cas::Expression partial_w() const { if (!cache_dw_) cache_dw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, wv_)); return *cache_dw_; }

    cas::Expression partial_xx() const { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }
    cas::Expression partial_yy() const { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }
    cas::Expression partial_zz() const { if (!cache_dzz_) cache_dzz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), zv_)); return *cache_dzz_; }
    cas::Expression partial_ww() const { if (!cache_dww_) cache_dww_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), wv_)); return *cache_dww_; }

    cas::Expression partial_xy() const { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }
    cas::Expression partial_xz() const { if (!cache_dxz_) cache_dxz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), zv_)); return *cache_dxz_; }
    cas::Expression partial_xw() const { if (!cache_dxw_) cache_dxw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), wv_)); return *cache_dxw_; }
    cas::Expression partial_yx() const { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }
    cas::Expression partial_yz() const { if (!cache_dyz_) cache_dyz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), zv_)); return *cache_dyz_; }
    cas::Expression partial_yw() const { if (!cache_dyw_) cache_dyw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), wv_)); return *cache_dyw_; }
    cas::Expression partial_zx() const { if (!cache_dzx_) cache_dzx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), xv_)); return *cache_dzx_; }
    cas::Expression partial_zy() const { if (!cache_dzy_) cache_dzy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), yv_)); return *cache_dzy_; }
    cas::Expression partial_zw() const { if (!cache_dzw_) cache_dzw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), wv_)); return *cache_dzw_; }
    cas::Expression partial_wx() const { if (!cache_dwx_) cache_dwx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), xv_)); return *cache_dwx_; }
    cas::Expression partial_wy() const { if (!cache_dwy_) cache_dwy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), yv_)); return *cache_dwy_; }
    cas::Expression partial_wz() const { if (!cache_dwz_) cache_dwz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), zv_)); return *cache_dwz_; }

public:
    vectors::SymbolicVector4 gradient() const { return { partial_x(), partial_y(), partial_z(), partial_w() }; }
    cas::Expression laplacian()     const { return partial_xx() + partial_yy() + partial_zz() + partial_ww(); }

    matrices::SymbolicMatrix4x4 hessian() const {
        matrices::SymbolicMatrix4x4 H;
        auto& d = H.get_data();
        d[0][0] = partial_xx(); d[0][1] = partial_xy(); d[0][2] = partial_xz(); d[0][3] = partial_xw();
        d[1][0] = partial_yx(); d[1][1] = partial_yy(); d[1][2] = partial_yz(); d[1][3] = partial_yw();
        d[2][0] = partial_zx(); d[2][1] = partial_zy(); d[2][2] = partial_zz(); d[2][3] = partial_zw();
        d[3][0] = partial_wx(); d[3][1] = partial_wy(); d[3][2] = partial_wz(); d[3][3] = partial_ww();
        return H;
    }

    ScalarField4D tangent_hyperplane(double a, double b, double c, double d) const {
        double f_val  = evaluate(a, b, c, d);
        double fx_val = EVALUATE(partial_x(), {{ xv_, a }, { yv_, b }, { zv_, c }, { wv_, d }});
        double fy_val = EVALUATE(partial_y(), {{ xv_, a }, { yv_, b }, { zv_, c }, { wv_, d }});
        double fz_val = EVALUATE(partial_z(), {{ xv_, a }, { yv_, b }, { zv_, c }, { wv_, d }});
        double fw_val = EVALUATE(partial_w(), {{ xv_, a }, { yv_, b }, { zv_, c }, { wv_, d }});

        cas::Expression result = cas::Const(f_val)
            + cas::Const(fx_val) * (cas::VARIABLE(xv_) - cas::Const(a))
            + cas::Const(fy_val) * (cas::VARIABLE(yv_) - cas::Const(b))
            + cas::Const(fz_val) * (cas::VARIABLE(zv_) - cas::Const(c))
            + cas::Const(fw_val) * (cas::VARIABLE(wv_) - cas::Const(d));

        return ScalarField4D(SIMPLIFY(result), xv_, yv_, zv_, wv_);
    }

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

    ScalarFieldN& operator=(const ScalarFieldN& o) {
        f_ = o.f_; 
        vars_ = o.vars_;
        cache_partials_->clear(); 
        cache_hessian_.reset(); 
        cache_laplacian_.reset();
        return *this;
    }

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

    ScalarFieldN tangent_plane(const std::vector<double>& point) const {
        assert(point.size() == dimension());
        std::unordered_map<std::string, double> eval_map;
        for (std::size_t i = 0; i < dimension(); ++i) eval_map[vars_[i]] = point[i];
        double f_p = EVALUATE(f_, eval_map);
        cas::Expression result = cas::Const(f_p);

        for (std::size_t i = 0; i < dimension(); ++i) {
            double dfi = EVALUATE(partial(i), eval_map);
            if (std::abs(dfi) <= constants::middle_epsilon()) continue;
            result = result + cas::Const(dfi) * (cas::VARIABLE(vars_[i]) - cas::Const(point[i]));
        }

        return ScalarFieldN(SIMPLIFY(result), vars_);
    }

    ScalarFieldN tangent_plane(std::initializer_list<double> point) const { return tangent_plane(std::vector<double>(point)); }

    matrices::SymbolicMatrixN hessian() const {
        if (!cache_hessian_) {
            std::size_t n = dimension();
            matrices::SymbolicMatrixN H(n);

            for (std::size_t i = 0; i < n; ++i)
                for (std::size_t j = 0; j < n; ++j)
                    H.at(i, j) = partial2(i, j);

            cache_hessian_ = std::make_unique<matrices::SymbolicMatrixN>(std::move(H));
        }

        return *cache_hessian_;
    }

    cas::Expression laplacian() const {
        if (!cache_laplacian_) {
            cas::Expression lap = partial2(0, 0);
            for (std::size_t i = 1; i < dimension(); ++i) lap = lap + partial2(i, i);
            cache_laplacian_ = std::make_unique<cas::Expression>(std::move(lap));
        }

        return *cache_laplacian_;
    }

public:
    cas::Expression directional_derivative_unit(const vectors::SymbolicVectorN& d_) const {
        assert(d_.dimension() == dimension());
        const auto d = d_.unit_vector();
        cas::Expression dd = d[0] * partial(0);
        for (std::size_t i = 1; i < dimension(); ++i) dd = dd + d[i] * partial(i);
        return dd;
    }

    cas::Expression directional_derivative(const vectors::SymbolicVectorN& d) const {
        assert(d.dimension() == dimension());
        cas::Expression dd = d[0] * partial(0);
        for (std::size_t i = 1; i < dimension(); ++i) dd = dd + d[i] * partial(i);
        return dd;
    }

    cas::Expression directional_derivative(const std::vector<cas::Expression>& d) const { return directional_derivative(vectors::SymbolicVectorN(d)); }

public:
    double evaluate(const std::unordered_map<std::string, double>& vals)        const { return f_.evaluate(vals); }
    double evaluate(std::initializer_list<std::pair<std::string, double>> vals) const { return f_.evaluate(vals); }

    double evaluate(const std::vector<double>& vals) const {
        std::unordered_map<std::string, double> m;
        for (std::size_t i = 0; i < std::min(vals.size(), vars_.size()); ++i) m[vars_[i]] = vals[i];
        return f_.evaluate(m);
    }

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

    void ensure_partials() const {
        if (!cache_partials_) {
            auto p = std::make_shared<std::vector<cas::Expression>>();
            p->reserve(vars_.size());
            for (auto& v : vars_) p->push_back(cas::DIFFERENTIATE(f_, v));
            cache_partials_ = std::move(p);
        }
    }
};

inline ScalarFieldN operator+(const cas::Expression& s, const ScalarFieldN& f) { return f + s; }
inline ScalarFieldN operator*(const cas::Expression& s, const ScalarFieldN& f) { return f * s; }
inline ScalarFieldN operator+(double s,            const ScalarFieldN& f) { return f + s; }
inline ScalarFieldN operator*(double s,            const ScalarFieldN& f) { return f * s; }

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_SCALAR_FIELDS_HPP