#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace geometry {

auto ScalarField1D::derivative() const -> cas::Expression { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_;  }

auto ScalarField1D::second_derivative() const -> cas::Expression { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(derivative(), xv_)); return *cache_dxx_; }

auto ScalarField1D::nth_derivative(unsigned int n) const -> cas::Expression {
        if (n == 0) return f_;
        if (n == 1) return derivative();
        if (n == 2) return second_derivative();
        return cas::DIFFERENTIATE_ITERATIVE(f_, xv_, n);
    }

auto ScalarField1D::tangent_line(double a) const -> ScalarField1D {
        double f_a  = evaluate(a);
        double fp_a = EVALUATE(derivative(), {{ xv_, a }});
        cas::Expression result = cas::Const(f_a) + cas::Const(fp_a) * (cas::VARIABLE(xv_) - cas::Const(a));
        return ScalarField1D(SIMPLIFY(result), xv_);
    }

auto ScalarField1D::unit_tangent() const -> vectors::SymbolicVector2 {
        cas::Expression fp = derivative();
        cas::Expression s  = SQRT(cas::Const(1.0) + fp * fp);
        return { cas::Const(1.0) / s, fp / s };
    }

auto ScalarField1D::unit_normal() const -> vectors::SymbolicVector2 {
        cas::Expression fp = derivative();
        cas::Expression s  = SQRT(cas::Const(1.0) + fp * fp);
        return { -fp / s, cas::Const(1.0) / s };
    }

auto ScalarField1D::curvature() const -> cas::Expression {
        cas::Expression fp  = derivative();
        cas::Expression fpp = second_derivative();
        cas::Expression denom = SQRT(cas::Const(1.0) + fp * fp);   
        return fpp / (denom * denom * denom);             
    }

auto ScalarField1D::curvature_radius() const -> cas::Expression {
        cas::Expression fp  = derivative();
        cas::Expression fpp = second_derivative();
        cas::Expression num = SQRT(cas::Const(1.0) + fp * fp);
        return (num * num * num) / ABS(fpp);              
    }

auto ScalarField1D::normal_line(double a) const -> ScalarField1D {
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

auto ScalarField2D::operator=(const ScalarField2D& o) -> ScalarField2D& { f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; cache_dx_.reset(); cache_dy_.reset(); cache_dxx_.reset(); cache_dyy_.reset(); cache_dxy_.reset(); cache_dyx_.reset(); return *this; }

auto ScalarField2D::partial_x() const -> cas::Expression { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_;  }

auto ScalarField2D::partial_y() const -> cas::Expression { if (!cache_dy_)  cache_dy_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_)); return *cache_dy_;  }

auto ScalarField2D::partial_xx() const -> cas::Expression { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }

auto ScalarField2D::partial_yy() const -> cas::Expression { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }

auto ScalarField2D::partial_xy() const -> cas::Expression { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }

auto ScalarField2D::partial_yx() const -> cas::Expression { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }

auto ScalarField2D::tangent_plane(double a, double b) const -> ScalarField2D {
        double f_ab  = evaluate(a, b);
        double fx_ab = EVALUATE(partial_x(), {{ xv_, a }, { yv_, b }});
        double fy_ab = EVALUATE(partial_y(), {{ xv_, a }, { yv_, b }});

        cas::Expression result = cas::Const(f_ab)
            + cas::Const(fx_ab) * (cas::VARIABLE(xv_) - cas::Const(a))
            + cas::Const(fy_ab) * (cas::VARIABLE(yv_) - cas::Const(b));
        return ScalarField2D(SIMPLIFY(result), xv_, yv_);
    }

auto ScalarField2D::line_integrand(const ParametricCurve2D& curve) const -> cas::Expression {
        cas::Expression f_on_curve = parameterize(curve);
        cas::Expression speed = curve.speed();
        return f_on_curve * speed;
    }

auto ScalarField2D::line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param) const -> cas::Expression {
        cas::Expression f_on_curve = parameterize(x_t, y_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression speed = SQRT(xp * xp + yp * yp);
        return f_on_curve * speed;
    }

auto ScalarField2D::line_integral(const ParametricCurve2D& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = line_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto ScalarField2D::line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = line_integrand(x_t, y_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto ScalarField2D::surface_area_integrand() const -> cas::Expression {
        cas::Expression fx = partial_x();
        cas::Expression fy = partial_y();
        return SQRT(cas::Const(1.0) + fx * fx + fy * fy);
    }

auto ScalarField2D::region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            cas::Const(1.0),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

auto ScalarField2D::region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            [](double, double) -> double { return 1.0; },
            outer_lo, outer_hi,
            std::move(inner_lo),
            std::move(inner_hi),
            cfg
        );
    }

auto ScalarField2D::region_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            cas::Const(1.0),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

auto ScalarField2D::region_area(const Region& region, integration::IntegrationConfig2D cfg) const -> integration::IntegrationResult2D {
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

auto ScalarField2D::surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            surface_area_integrand(),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

auto ScalarField2D::surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
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

auto ScalarField2D::surface_area(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            surface_area_integrand(),
            outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

auto ScalarField2D::surface_area(const Region& region, integration::IntegrationConfig2D cfg) const -> integration::IntegrationResult2D {
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

auto ScalarField2D::double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, inner_lo, inner_hi,
            cfg
        );
    }

auto ScalarField2D::double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, std::move(inner_lo), std::move(inner_hi),
            cfg
        );
    }

auto ScalarField2D::double_integral(
        const std::string& outer_var, double outer_lo, double outer_hi,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            f_, outer_var, outer_lo, outer_hi,
            inner_var, inner_lo_expr, inner_hi_expr,
            cfg
        );
    }

auto ScalarField2D::double_integral(const Region& region, integration::IntegrationConfig2D cfg) const -> integration::IntegrationResult2D {
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

auto ScalarField3D::operator=(const ScalarField3D& o) -> ScalarField3D& { f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; cache_dx_.reset(); cache_dy_.reset(); cache_dz_.reset(); cache_dxx_.reset(); cache_dyy_.reset(); cache_dzz_.reset(); cache_dxy_.reset(); cache_dxz_.reset(); cache_dyx_.reset(); cache_dyz_.reset(); cache_dzx_.reset(); cache_dzy_.reset(); return *this; }

auto ScalarField3D::partial_x() const -> cas::Expression { if (!cache_dx_)  cache_dx_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_));          return *cache_dx_;  }

auto ScalarField3D::partial_y() const -> cas::Expression { if (!cache_dy_)  cache_dy_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_));          return *cache_dy_;  }

auto ScalarField3D::partial_z() const -> cas::Expression { if (!cache_dz_)  cache_dz_  = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, zv_));          return *cache_dz_;  }

auto ScalarField3D::partial_xx() const -> cas::Expression { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }

auto ScalarField3D::partial_yy() const -> cas::Expression { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }

auto ScalarField3D::partial_zz() const -> cas::Expression { if (!cache_dzz_) cache_dzz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), zv_)); return *cache_dzz_; }

auto ScalarField3D::partial_xy() const -> cas::Expression { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }

auto ScalarField3D::partial_xz() const -> cas::Expression { if (!cache_dxz_) cache_dxz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), zv_)); return *cache_dxz_; }

auto ScalarField3D::partial_yx() const -> cas::Expression { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }

auto ScalarField3D::partial_yz() const -> cas::Expression { if (!cache_dyz_) cache_dyz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), zv_)); return *cache_dyz_; }

auto ScalarField3D::partial_zx() const -> cas::Expression { if (!cache_dzx_) cache_dzx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), xv_)); return *cache_dzx_; }

auto ScalarField3D::partial_zy() const -> cas::Expression { if (!cache_dzy_) cache_dzy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), yv_)); return *cache_dzy_; }

auto ScalarField3D::tangent_plane(double a, double b, double c) const -> ScalarField3D {
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

auto ScalarField3D::hessian() const -> matrices::SymbolicMatrix3x3 {
        matrices::SymbolicMatrix3x3 H;
        H.get_data()[0][0] = partial_xx(); H.get_data()[0][1] = partial_xy(); H.get_data()[0][2] = partial_xz();
        H.get_data()[1][0] = partial_yx(); H.get_data()[1][1] = partial_yy(); H.get_data()[1][2] = partial_yz();
        H.get_data()[2][0] = partial_zx(); H.get_data()[2][1] = partial_zy(); H.get_data()[2][2] = partial_zz();
        return H;
    }

auto ScalarField3D::line_integrand(const ParametricCurve& curve) const -> cas::Expression {
        cas::Expression f_on_curve = parameterize(curve);
        cas::Expression speed = curve.speed();
        return f_on_curve * speed;
    }

auto ScalarField3D::line_integrand(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param) const -> cas::Expression {
        cas::Expression f_on_curve = parameterize(x_t, y_t, z_t);
        cas::Expression xp = cas::DIFFERENTIATE(x_t, param);
        cas::Expression yp = cas::DIFFERENTIATE(y_t, param);
        cas::Expression zp = cas::DIFFERENTIATE(z_t, param);
        cas::Expression speed = SQRT(xp * xp + yp * yp + zp * zp);
        return f_on_curve * speed;
    }

auto ScalarField3D::line_integral(const ParametricCurve& curve, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = line_integrand(curve);
        return integration::Integrator::integrate(integrand, curve.param(), t0, t1, layer);
    }

auto ScalarField3D::line_integral(const cas::Expression& x_t, const cas::Expression& y_t, const cas::Expression& z_t, const std::string& param, double t0, double t1, const integration::IntegrationLayer& layer) const -> integration::IntegrationResult {
        cas::Expression integrand = line_integrand(x_t, y_t, z_t, param);
        return integration::Integrator::integrate(integrand, param, t0, t1, layer);
    }

auto ScalarField3D::surface_area_integrand() const -> cas::Expression {
        cas::Expression fx = partial_x();
        cas::Expression fy = partial_y();
        cas::Expression fz = partial_z();
        return SQRT(cas::Const(1.0) + fx * fx + fy * fy + fz * fz);
    }

auto ScalarField3D::surface_integrand(const ParametricSurface& surface) const -> cas::Expression {
        cas::Expression f_on_surface = parameterize(surface);
        return f_on_surface * surface.area_element();
    }

auto ScalarField3D::region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            cas::Const(1.0),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

auto ScalarField3D::region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            [](double, double, double) -> double { return 1.0; },
            outer_lo, outer_hi,
            std::move(middle_lo), std::move(middle_hi),
            std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

auto ScalarField3D::region_volume(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            cas::Const(1.0),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

auto ScalarField3D::region_volume(const Region3D& region, integration::IntegrationConfig3D cfg) const -> integration::IntegrationResult3D {
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

auto ScalarField3D::surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            surface_area_integrand(),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

auto ScalarField3D::surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
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

auto ScalarField3D::surface_area(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            surface_area_integrand(),
            outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

auto ScalarField3D::surface_area(const Region3D& region, integration::IntegrationConfig3D cfg) const -> integration::IntegrationResult3D {
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

auto ScalarField3D::triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, double middle_lo, double middle_hi,
        const std::string& inner_var,  double inner_lo,  double inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo, middle_hi,
            inner_var,  inner_lo,  inner_hi,
            cfg
        );
    }

auto ScalarField3D::triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var, integration::IntegrationBound middle_lo, integration::IntegrationBound middle_hi,
        const std::string& inner_var,  integration::IntegrationBound2 inner_lo, integration::IntegrationBound2 inner_hi,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, std::move(middle_lo), std::move(middle_hi),
            inner_var,  std::move(inner_lo),  std::move(inner_hi),
            cfg
        );
    }

auto ScalarField3D::triple_integral(
        const std::string& outer_var,  double outer_lo,  double outer_hi,
        const std::string& middle_var,
        const cas::Expression& middle_lo_expr,
        const cas::Expression& middle_hi_expr,
        const std::string& inner_var,
        const cas::Expression& inner_lo_expr,
        const cas::Expression& inner_hi_expr,
        integration::IntegrationConfig3D cfg 
) const -> integration::IntegrationResult3D {
        return integration::Integrator3D::integrate(
            f_, outer_var,  outer_lo,  outer_hi,
            middle_var, middle_lo_expr, middle_hi_expr,
            inner_var,  inner_lo_expr,  inner_hi_expr,
            cfg
        );
    }

auto ScalarField3D::triple_integral(const Region3D& region, integration::IntegrationConfig3D cfg) const -> integration::IntegrationResult3D {
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

auto ScalarField3D::surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, double inner_lo, double inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            surface_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

auto ScalarField3D::surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, integration::IntegrationBound inner_lo, integration::IntegrationBound inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
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

auto ScalarField3D::surface_integral(
        const ParametricSurface& surface,
        const std::string& outer, double outer_lo, double outer_hi,
        const std::string& inner, const cas::Expression& inner_lo, const cas::Expression& inner_hi,
        integration::IntegrationConfig2D cfg 
) const -> integration::IntegrationResult2D {
        return integration::Integrator2D::integrate(
            surface_integrand(surface), outer, outer_lo, outer_hi, inner, inner_lo, inner_hi, cfg
        );
    }

ScalarField4D::ScalarField4D(cas::Expression f,
                  std::string xvar, std::string yvar,
                  std::string zvar, std::string wvar) : f_(std::move(f)), xv_(std::move(xvar)), yv_(std::move(yvar)),
          zv_(std::move(zvar)), wv_(std::move(wvar)) {}

auto ScalarField4D::operator=(const ScalarField4D& o) -> ScalarField4D& {
        f_ = o.f_; xv_ = o.xv_; yv_ = o.yv_; zv_ = o.zv_; wv_ = o.wv_;
        cache_dx_.reset(); cache_dy_.reset(); cache_dz_.reset(); cache_dw_.reset();
        cache_dxx_.reset(); cache_dyy_.reset(); cache_dzz_.reset(); cache_dww_.reset();
        cache_dxy_.reset(); cache_dxz_.reset(); cache_dxw_.reset();
        cache_dyx_.reset(); cache_dyz_.reset(); cache_dyw_.reset();
        cache_dzx_.reset(); cache_dzy_.reset(); cache_dzw_.reset();
        cache_dwx_.reset(); cache_dwy_.reset(); cache_dwz_.reset();
        return *this;
    }

auto ScalarField4D::partial_x() const -> cas::Expression { if (!cache_dx_) cache_dx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, xv_)); return *cache_dx_; }

auto ScalarField4D::partial_y() const -> cas::Expression { if (!cache_dy_) cache_dy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, yv_)); return *cache_dy_; }

auto ScalarField4D::partial_z() const -> cas::Expression { if (!cache_dz_) cache_dz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, zv_)); return *cache_dz_; }

auto ScalarField4D::partial_w() const -> cas::Expression { if (!cache_dw_) cache_dw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(f_, wv_)); return *cache_dw_; }

auto ScalarField4D::partial_xx() const -> cas::Expression { if (!cache_dxx_) cache_dxx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), xv_)); return *cache_dxx_; }

auto ScalarField4D::partial_yy() const -> cas::Expression { if (!cache_dyy_) cache_dyy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), yv_)); return *cache_dyy_; }

auto ScalarField4D::partial_zz() const -> cas::Expression { if (!cache_dzz_) cache_dzz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), zv_)); return *cache_dzz_; }

auto ScalarField4D::partial_ww() const -> cas::Expression { if (!cache_dww_) cache_dww_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), wv_)); return *cache_dww_; }

auto ScalarField4D::partial_xy() const -> cas::Expression { if (!cache_dxy_) cache_dxy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), yv_)); return *cache_dxy_; }

auto ScalarField4D::partial_xz() const -> cas::Expression { if (!cache_dxz_) cache_dxz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), zv_)); return *cache_dxz_; }

auto ScalarField4D::partial_xw() const -> cas::Expression { if (!cache_dxw_) cache_dxw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_x(), wv_)); return *cache_dxw_; }

auto ScalarField4D::partial_yx() const -> cas::Expression { if (!cache_dyx_) cache_dyx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), xv_)); return *cache_dyx_; }

auto ScalarField4D::partial_yz() const -> cas::Expression { if (!cache_dyz_) cache_dyz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), zv_)); return *cache_dyz_; }

auto ScalarField4D::partial_yw() const -> cas::Expression { if (!cache_dyw_) cache_dyw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_y(), wv_)); return *cache_dyw_; }

auto ScalarField4D::partial_zx() const -> cas::Expression { if (!cache_dzx_) cache_dzx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), xv_)); return *cache_dzx_; }

auto ScalarField4D::partial_zy() const -> cas::Expression { if (!cache_dzy_) cache_dzy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), yv_)); return *cache_dzy_; }

auto ScalarField4D::partial_zw() const -> cas::Expression { if (!cache_dzw_) cache_dzw_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_z(), wv_)); return *cache_dzw_; }

auto ScalarField4D::partial_wx() const -> cas::Expression { if (!cache_dwx_) cache_dwx_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), xv_)); return *cache_dwx_; }

auto ScalarField4D::partial_wy() const -> cas::Expression { if (!cache_dwy_) cache_dwy_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), yv_)); return *cache_dwy_; }

auto ScalarField4D::partial_wz() const -> cas::Expression { if (!cache_dwz_) cache_dwz_ = std::make_unique<cas::Expression>(cas::DIFFERENTIATE(partial_w(), zv_)); return *cache_dwz_; }

auto ScalarField4D::hessian() const -> matrices::SymbolicMatrix4x4 {
        matrices::SymbolicMatrix4x4 H;
        auto& d = H.get_data();
        d[0][0] = partial_xx(); d[0][1] = partial_xy(); d[0][2] = partial_xz(); d[0][3] = partial_xw();
        d[1][0] = partial_yx(); d[1][1] = partial_yy(); d[1][2] = partial_yz(); d[1][3] = partial_yw();
        d[2][0] = partial_zx(); d[2][1] = partial_zy(); d[2][2] = partial_zz(); d[2][3] = partial_zw();
        d[3][0] = partial_wx(); d[3][1] = partial_wy(); d[3][2] = partial_wz(); d[3][3] = partial_ww();
        return H;
    }

auto ScalarField4D::tangent_hyperplane(double a, double b, double c, double d) const -> ScalarField4D {
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

auto ScalarFieldN::operator=(const ScalarFieldN& o) -> ScalarFieldN& {
        f_ = o.f_; 
        vars_ = o.vars_;
        cache_partials_->clear(); 
        cache_hessian_.reset(); 
        cache_laplacian_.reset();
        return *this;
    }

auto ScalarFieldN::tangent_plane(const std::vector<double>& point) const -> ScalarFieldN {
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

auto ScalarFieldN::hessian() const -> matrices::SymbolicMatrixN {
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

auto ScalarFieldN::laplacian() const -> cas::Expression {
        if (!cache_laplacian_) {
            cas::Expression lap = partial2(0, 0);
            for (std::size_t i = 1; i < dimension(); ++i) lap = lap + partial2(i, i);
            cache_laplacian_ = std::make_unique<cas::Expression>(std::move(lap));
        }

        return *cache_laplacian_;
    }

auto ScalarFieldN::directional_derivative_unit(const vectors::SymbolicVectorN& d_) const -> cas::Expression {
        assert(d_.dimension() == dimension());
        const auto d = d_.unit_vector();
        cas::Expression dd = d[0] * partial(0);
        for (std::size_t i = 1; i < dimension(); ++i) dd = dd + d[i] * partial(i);
        return dd;
    }

auto ScalarFieldN::directional_derivative(const vectors::SymbolicVectorN& d) const -> cas::Expression {
        assert(d.dimension() == dimension());
        cas::Expression dd = d[0] * partial(0);
        for (std::size_t i = 1; i < dimension(); ++i) dd = dd + d[i] * partial(i);
        return dd;
    }

auto ScalarFieldN::evaluate(const std::vector<double>& vals) const -> double {
        std::unordered_map<std::string, double> m;
        for (std::size_t i = 0; i < std::min(vals.size(), vars_.size()); ++i) m[vars_[i]] = vals[i];
        return f_.evaluate(m);
    }

auto ScalarFieldN::ensure_partials() const -> void {
        if (!cache_partials_) {
            auto p = std::make_shared<std::vector<cas::Expression>>();
            p->reserve(vars_.size());
            for (auto& v : vars_) p->push_back(cas::DIFFERENTIATE(f_, v));
            cache_partials_ = std::move(p);
        }
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
