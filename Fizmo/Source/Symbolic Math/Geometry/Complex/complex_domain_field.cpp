#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace geometry {

auto ComplexDomainField::decompose(int branch) const -> std::pair<cas::Expression, cas::Expression> {
        auto it = cache_uv_.find(branch);
        if (it != cache_uv_.end()) return it->second;
        DecompCache dc;
        auto result = decompose_node(f_.node(), dc, branch);
        cache_uv_.emplace(branch, result);
        return result;
    }

auto ComplexDomainField::magnitude_field(int branch) const -> ScalarField2D {
        auto pair = decompose(branch);
        return ScalarField2D(cas::SQRT(cas::POW(pair.first, cas::Const(2.0)) + cas::POW(pair.second, cas::Const(2.0))), xv_, yv_);
    }

auto ComplexDomainField::magnitude_sq_field(int branch) const -> ScalarField2D {
        auto pair = decompose(branch);
        return ScalarField2D(cas::POW(pair.first, cas::Const(2.0)) + cas::POW(pair.second, cas::Const(2.0)), xv_, yv_);
    }

auto ComplexDomainField::argument_field(int branch) const -> ScalarField2D {
        auto pair = decompose(branch);
        cas::Expression arg = cas::ATAN2(pair.second, pair.first);
        if (branch != 0) arg = arg + cas::Const(2.0 * constants::pi() * static_cast<double>(branch));
        return ScalarField2D(arg, xv_, yv_);
    }

auto ComplexDomainField::log_magnitude_field(int branch) const -> ScalarField2D {
        auto pair = decompose(branch);
        return ScalarField2D(0.5 * cas::LN(cas::POW(pair.first, cas::Const(2.0)) + cas::POW(pair.second, cas::Const(2.0))), xv_, yv_);
    }

auto ComplexDomainField::projection_field(ComplexProjection proj, int branch) const -> ScalarField2D {
        switch (proj) {
            case ComplexProjection::RealPart:          return real_part_field(branch);
            case ComplexProjection::ImaginaryPart:     return imag_part_field(branch);
            case ComplexProjection::Magnitude:         return magnitude_field(branch);
            case ComplexProjection::MagnitudeSquared:  return magnitude_sq_field(branch);
            case ComplexProjection::Argument:          return argument_field(branch);
            case ComplexProjection::LogMagnitude:      return log_magnitude_field(branch);
            default:                                   return real_part_field(branch);
        }
    }

auto ComplexDomainField::evaluate(double x, double y, int branch) const -> ComplexD {
        cas::complex_symbols::ComplexEvaluationPolicy pol;
        pol.branch = branch;
        return f_.evaluate({ { var_, ComplexD(x, y) } }, pol);
    }

auto ComplexDomainField::numerical_map(ComplexProjection proj, int branch) const -> std::function<double(double, double)> {
        ComplexDomainField cap = *this;
        return [cap, proj, branch](double x, double y) { return cap.evaluate(x, y, proj, branch); };
    }

auto ComplexDomainField::complex_map(int branch) const -> std::function<ComplexD(double, double)> {
        ComplexDomainField cap = *this;
        return [cap, branch](double x, double y) { return cap.evaluate(x, y, branch); };
    }

auto ComplexDomainField::domain_color_at(double x, double y, int branch) const -> DomainColorSample {
        ComplexD w = evaluate(x, y, branch);
        double mag = w.magnitude();
        return { x, y, w.real(), w.imaginary(), mag, w.argument(branch), std::log(mag) };
    }

auto ComplexDomainField::domain_color_grid(
        double x0, double x1, std::size_t nx,
        double y0, double y1, std::size_t ny,
        int branch 
) const -> std::vector<DomainColorSample> {
        std::vector<DomainColorSample> grid;
        grid.reserve(nx * ny);
        double dx = (nx > 1) ? (x1 - x0) / double(nx - 1) : 0.0;
        double dy = (ny > 1) ? (y1 - y0) / double(ny - 1) : 0.0;

        for (std::size_t j = 0; j < ny; ++j)
            for (std::size_t i = 0; i < nx; ++i)
                grid.push_back(domain_color_at(x0 + i * dx, y0 + j * dy, branch));

        return grid;
    }

auto ComplexDomainField::cauchy_riemann_residuals(int branch) const -> std::pair<cas::Expression, cas::Expression> {
        auto pair = decompose(branch);
        ScalarField2D U(pair.first, xv_, yv_);
        ScalarField2D V(pair.second, xv_, yv_);
        return { U.partial_x() - V.partial_y(), U.partial_y() + V.partial_x() };
    }

auto ComplexDomainField::cauchy_riemann_at(double x, double y, int branch, double tol) const -> CauchyRiemannSample {
        auto pair = decompose(branch);
        ScalarField2D U(pair.first, xv_, yv_);
        ScalarField2D V(pair.second, xv_, yv_);
        const auto& ux_e = U.partial_x();
        const auto& uy_e = U.partial_y();
        const auto& vx_e = V.partial_x();
        const auto& vy_e = V.partial_y();
        double ux = ux_e.evaluate({{ xv_, x }, { yv_, y }});
        double uy = uy_e.evaluate({{ xv_, x }, { yv_, y }});
        double vx = vx_e.evaluate({{ xv_, x }, { yv_, y }});
        double vy = vy_e.evaluate({{ xv_, x }, { yv_, y }});
        double r1 = std::abs(ux - vy);
        double r2 = std::abs(uy + vx);
        return { x, y, ux, uy, vx, vy, r1, r2, (r1 < tol && r2 < tol) };
    }

auto ComplexDomainField::is_holomorphic_symbolic(int branch) const -> bool {
        auto pair = cauchy_riemann_residuals(branch);
        cas::Expression zero = cas::Const(0.0);
        return cas::SIMPLIFY(pair.first).symbolic_equals(zero) && cas::SIMPLIFY(pair.second).symbolic_equals(zero);
    }

auto ComplexDomainField::contour_integral(
        const ParametricCurve2D& curve,
        double t0, double t1,
        int branch,
        const integration::IntegrationLayer& layer 
) const -> ComplexIntegrationResult {
        const auto& r  = curve.r();
        const auto& rp = curve.r_prime();
        const std::string& par = curve.param();
        ComplexDomainField cap = *this;

        auto real_int = [&](double t) -> double {
            double x = r.x.evaluate({{ par, t }}), y = r.y.evaluate({{ par, t }});
            double xp = rp.x.evaluate({{ par, t }}), yp = rp.y.evaluate({{ par, t }});
            ComplexD w = cap.evaluate(x, y, branch);
            return w.real() * xp - w.imaginary() * yp;
        };

        auto imag_int = [&](double t) -> double {
            double x = r.x.evaluate({{ par, t }}), y = r.y.evaluate({{ par, t }});
            double xp = rp.x.evaluate({{ par, t }}), yp = rp.y.evaluate({{ par, t }});
            ComplexD w = cap.evaluate(x, y, branch);
            return w.real() * yp + w.imaginary() * xp;
        };

        auto rr = integration::Integrator::integrate(real_int, t0, t1, layer);
        auto ri = integration::Integrator::integrate(imag_int, t0, t1, layer);
        return { ComplexD(rr.value, ri.value), rr.error_estimate, ri.error_estimate, rr.converged && ri.converged, rr.function_evaluations + ri.function_evaluations };
    }

auto ComplexDomainField::contour_integral_circle(
        double cx, double cy, double r,
        int branch,
        const integration::IntegrationLayer& layer 
) const -> ComplexIntegrationResult {
        cas::Expression t = cas::VARIABLE("t");
        cas::Expression x_t = cas::Const(cx) + cas::Const(r) * cas::COS(t);
        cas::Expression y_t = cas::Const(cy) + cas::Const(r) * cas::SIN(t);
        geometry::ParametricCurve2D circle(x_t, y_t, "t");
        return contour_integral(circle, 0.0, 2.0 * constants::pi(), branch, layer);
    }

auto ComplexDomainField::residue_at(double cx, double cy, double r, int branch, const integration::IntegrationLayer& layer) const -> ComplexD {
        auto res = contour_integral_circle(cx, cy, r, branch, layer);
        const double inv2pi = constants::reciprocal_two_pi();
        return ComplexD(res.value.imaginary() * inv2pi, -res.value.real() * inv2pi);
    }

auto ComplexDomainField::winding_number(const geometry::ParametricCurve2D& curve, double t0, double t1, std::size_t n_samples, int branch) const -> double {
        const auto& r = curve.r();
        const std::string& par = curve.param();
        double dt = (t1 - t0) / double(n_samples);
        double total = 0.0;

        auto eval_at = [&](double t) -> ComplexD {
            double x = r.x.evaluate({{ par, t }});
            double y = r.y.evaluate({{ par, t }});
            return evaluate(x, y, branch);
        };

        ComplexD prev = eval_at(t0);

        for (std::size_t i = 1; i <= n_samples; ++i) {
            ComplexD cur = eval_at(t0 + i * dt);
            double da = cur.argument() - prev.argument();
            if (da >  constants::pi()) da -= 2.0 * constants::pi();
            if (da < -constants::pi()) da += 2.0 * constants::pi();
            total += da;
            prev = cur;
        }

        return total * constants::reciprocal_two_pi();
    }

auto ComplexDomainField::winding_number_circle(double cx, double cy, double r, std::size_t n_samples, int branch) const -> double {
        cas::Expression t = cas::VARIABLE("t");
        cas::Expression x_t = cas::Const(cx) + cas::Const(r) * cas::COS(t);
        cas::Expression y_t = cas::Const(cy) + cas::Const(r) * cas::SIN(t);
        ParametricCurve2D circle(x_t, y_t, "t");
        return winding_number(circle, 0.0, 2.0 * constants::pi(), n_samples, branch);
    }

auto ComplexDomainField::project(const ComplexD& w, ComplexProjection p, int branch) -> double {
        switch (p) {
            case ComplexProjection::RealPart:         return w.real();
            case ComplexProjection::ImaginaryPart:    return w.imaginary();
            case ComplexProjection::Magnitude:        return w.magnitude();
            case ComplexProjection::MagnitudeSquared: return w.magnitude_squared();
            case ComplexProjection::Argument:         return w.argument(branch);
            case ComplexProjection::LogMagnitude:     return std::log(w.magnitude());
            default:                                  return w.real();
        }
    }

auto ComplexDomainField::p_div(const std::pair<cas::Expression, cas::Expression>& a, const std::pair<cas::Expression, cas::Expression>& b) -> std::pair<cas::Expression, cas::Expression> {
        cas::Expression denom = b.first * b.first + b.second * b.second;
        return { (a.first * b.first  + a.second * b.second) / denom, (a.second * b.first - a.first  * b.second) / denom };
    }

auto ComplexDomainField::p_recip(const std::pair<cas::Expression, cas::Expression>& a) -> std::pair<cas::Expression, cas::Expression> {
        cas::Expression denom = a.first * a.first + a.second * a.second;
        return { a.first / denom, -a.second / denom };
    }

auto ComplexDomainField::p_exp(const std::pair<cas::Expression, cas::Expression>& a) -> std::pair<cas::Expression, cas::Expression> {
        cas::Expression eu = cas::EXP(a.first);
        return { eu * cas::COS(a.second), eu * cas::SIN(a.second) };
    }

auto ComplexDomainField::p_ln(const std::pair<cas::Expression, cas::Expression>& a, int branch) -> std::pair<cas::Expression, cas::Expression> {
        cas::Expression arg = cas::ATAN2(a.second, a.first);
        if (branch != 0) arg = arg + cas::Const(2.0 * constants::pi() * static_cast<double>(branch));
        return { 0.5 * cas::LN(a.first * a.first + a.second * a.second), arg };
    }

auto ComplexDomainField::p_asin(const std::pair<cas::Expression, cas::Expression>& z, int branch) -> std::pair<cas::Expression, cas::Expression> {
        auto i  = p_const(0, 1);
        auto iz = p_mul(i, z);
        auto sq = p_sqrt(p_sub(p_const(1, 0), p_mul(z, z)), branch);
        auto lg = p_ln(p_add(iz, sq), branch);
        return { lg.second, -lg.first };
    }

auto ComplexDomainField::p_atan(const std::pair<cas::Expression, cas::Expression>& z, int branch) -> std::pair<cas::Expression, cas::Expression> {
        auto i   = p_const(0, 1);
        auto num = p_add(i, z);
        auto den = p_sub(i, z);
        auto lg  = p_ln(p_div(num, den), branch);
        return { -0.5 * lg.second, 0.5 * lg.first };
    }

auto ComplexDomainField::p_acosh(const std::pair<cas::Expression, cas::Expression>& z, int branch) -> std::pair<cas::Expression, cas::Expression> {
        auto one = p_const(1, 0);
        auto r1  = p_sqrt(p_add(z, one), branch);
        auto r2  = p_sqrt(p_sub(z, one), branch);
        return p_ln(p_add(z, p_mul(r1, r2)), branch);
    }

auto ComplexDomainField::p_atanh(const std::pair<cas::Expression, cas::Expression>& z, int branch) -> std::pair<cas::Expression, cas::Expression> {
        auto one = p_const(1, 0);
        auto lg  = p_ln(p_div(p_add(one, z), p_sub(one, z)), branch);
        return { 0.5 * lg.first, 0.5 * lg.second };
    }

auto ComplexDomainField::decompose_node(const cas::complex_symbols::ComplexMathExpressionNode* n, DecompCache& dc, int branch) const -> std::pair<cas::Expression, cas::Expression> {
        if (!n) return p_zero();
        auto it = dc.find(n);
        if (it != dc.end()) return it->second;
        std::pair<cas::Expression, cas::Expression> result;

        switch (n->type) {
            case cas::complex_symbols::ComplexNodeType::Constant:
                result = p_const(n->constant.real, n->constant.imag);
                break;

            case cas::complex_symbols::ComplexNodeType::Variable:
                result = { cas::VARIABLE(xv_), cas::VARIABLE(yv_) };
                break;

            case cas::complex_symbols::ComplexNodeType::Infinity:
            case cas::complex_symbols::ComplexNodeType::NaN:
            case cas::complex_symbols::ComplexNodeType::Undefined:
            case cas::complex_symbols::ComplexNodeType::Indeterminate:
            case cas::complex_symbols::ComplexNodeType::Invalid:
                result = { cas::Const(std::numeric_limits<double>::quiet_NaN()), cas::Const(std::numeric_limits<double>::quiet_NaN()) };
                break;

            case cas::complex_symbols::ComplexNodeType::Add:
                result = p_add(decompose_node(n->binary.left, dc, branch), decompose_node(n->binary.right, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Subtract:
                result = p_sub(decompose_node(n->binary.left, dc, branch), decompose_node(n->binary.right, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Multiply:
                result = p_mul(decompose_node(n->binary.left, dc, branch), decompose_node(n->binary.right, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Divide:
                result = p_div(decompose_node(n->binary.left, dc, branch), decompose_node(n->binary.right, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Negate:
                result = p_neg(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Power:
                result = p_pow(decompose_node(n->power.base, dc, branch), decompose_node(n->power.exponent, dc, branch), branch);
                break;

            case cas::complex_symbols::ComplexNodeType::Root: {
                auto rad = decompose_node(n->binary.left,  dc, branch);
                auto deg = decompose_node(n->binary.right, dc, branch);
                result = p_pow(rad, p_recip(deg), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Sqrt:
                result = p_sqrt(decompose_node(n->unary.child, dc, branch), branch);
                break;

            case cas::complex_symbols::ComplexNodeType::Cbrt:
                result = p_pow(decompose_node(n->unary.child, dc, branch), p_const(1.0 / 3.0, 0), branch);
                break;

            case cas::complex_symbols::ComplexNodeType::NaturalExp:
                result = p_exp(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::NaturalLog:
                result = p_ln(decompose_node(n->unary.child, dc, branch), branch);
                break;

            case cas::complex_symbols::ComplexNodeType::Log: {
                auto arg  = p_ln(decompose_node(n->binary.left,  dc, branch), branch);
                auto base = p_ln(decompose_node(n->binary.right, dc, branch), branch);
                result = p_div(arg, base);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Sin:
                result = p_sin(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Cos:
                result = p_cos(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Tan: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_div(p_sin(z), p_cos(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Csc: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_recip(p_sin(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Sec: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_recip(p_cos(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Cot: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_div(p_cos(z), p_sin(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arcsin:  result = p_asin(decompose_node(n->unary.child, dc, branch), branch);  break;
            case cas::complex_symbols::ComplexNodeType::Arccos:  result = p_acos(decompose_node(n->unary.child, dc, branch), branch);  break;
            case cas::complex_symbols::ComplexNodeType::Arctan:  result = p_atan(decompose_node(n->unary.child, dc, branch), branch);  break;

            case cas::complex_symbols::ComplexNodeType::Arccsc: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_asin(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arcsec: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_acos(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arccot: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_atan(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Sinh:
                result = p_sinh(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Cosh:
                result = p_cosh(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Tanh: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_div(p_sinh(z), p_cosh(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Csch: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_recip(p_sinh(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Sech: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_recip(p_cosh(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Coth: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_div(p_cosh(z), p_sinh(z));
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arcsinh:  result = p_asinh(decompose_node(n->unary.child, dc, branch), branch);  break;
            case cas::complex_symbols::ComplexNodeType::Arccosh:  result = p_acosh(decompose_node(n->unary.child, dc, branch), branch);  break;
            case cas::complex_symbols::ComplexNodeType::Arctanh:  result = p_atanh(decompose_node(n->unary.child, dc, branch), branch);  break;

            case cas::complex_symbols::ComplexNodeType::Arccsch: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_asinh(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arcsech: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_acosh(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Arccoth: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = p_atanh(p_recip(z), branch);
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Conjugate:
                result = p_conj(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::RealPart: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = { z.first, cas::Const(0) };
                break;
            }

            case cas::complex_symbols::ComplexNodeType::ImaginaryPart: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = { z.second, cas::Const(0) };
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Magnitude: {
                auto z = decompose_node(n->unary.child, dc, branch);
                result = { cas::SQRT(z.first * z.first + z.second * z.second), cas::Const(0) };
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Argument: {
                auto z = decompose_node(n->unary.child, dc, branch);
                cas::Expression arg = cas::ATAN2(z.second, z.first);
                if (branch != 0) arg = arg + cas::Const(2.0 * constants::pi() * static_cast<double>(branch));
                result = { arg, cas::Const(0) };
                break;
            }

            case cas::complex_symbols::ComplexNodeType::Reciprocal:
                result = p_recip(decompose_node(n->unary.child, dc, branch));
                break;

            case cas::complex_symbols::ComplexNodeType::Sign: {
                auto z = decompose_node(n->unary.child, dc, branch);
                cas::Expression mag = cas::SQRT(z.first * z.first + z.second * z.second);
                result = { z.first / mag, z.second / mag };
                break;
            }

            default:
                result = p_zero();
                break;
        }

        dc[n] = result;
        return result;
    }

} // namespace geometry
} // namespace math
} // namespace fizmo
