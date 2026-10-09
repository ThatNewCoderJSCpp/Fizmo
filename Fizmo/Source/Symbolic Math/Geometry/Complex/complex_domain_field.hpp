#ifndef FIZMO_COMPLEX_DOMAIN_FIELD_HPP
#define FIZMO_COMPLEX_DOMAIN_FIELD_HPP

#include "../../CAS/Complex/complex_expression_wrapper.hpp"
#include "../frenet_frame.hpp"
#include "../../Calculus/integrator.hpp"
#include "../scalar_field.hpp"
#include "../../Common/main_convenience.hpp"
#include "../../../Basic/constants.hpp"

#include <functional>
#include <cmath>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace fizmo {
namespace math {
namespace geometry {

enum class ComplexProjection : std::uint8_t {
    RealPart = 0,       // Re(f(z))
    ImaginaryPart,      // Im(f(z))
    Magnitude,          // |f(z)|
    MagnitudeSquared,   // |f(z)|^2
    Argument,           // arg(f(z))
    LogMagnitude,       // ln|f(z)|
};

struct DomainColorSample {
    double x, y;
    double re, im;
    double magnitude, argument, log_magnitude;
};

struct CauchyRiemannSample {
    double x, y;
    double du_dx, du_dy, dv_dx, dv_dy;
    double cr1_residual, cr2_residual;
    bool   satisfies;
};

struct ComplexIntegrationResult {
    BasicComplex<double> value;
    double real_error   = 0.0;
    double imag_error   = 0.0;
    bool   converged    = false;
    std::uint64_t evals = 0;
};

class ComplexDomainField {
public:
    ComplexDomainField(
        cas::ComplexExpression f,
        std::string var  = "z",
        std::string xvar = "x",
        std::string yvar = "y"
    ) : f_(std::move(f)), var_(std::move(var)), xv_(std::move(xvar)), yv_(std::move(yvar)) {}

    ComplexDomainField(const ComplexDomainField&) = default;
    ComplexDomainField& operator=(const ComplexDomainField&) = default;

    const cas::ComplexExpression& expression() const { return f_;   }
    const std::string& var()                   const { return var_; }
    const std::string& xvar()                  const { return xv_;  }
    const std::string& yvar()                  const { return yv_;  }

    // f(z) -> ( u(x,y) , v(x,y) ) 
    std::pair<cas::Expression, cas::Expression> decompose(int branch = 0) const;

    ScalarField2D real_part_field(int branch = 0) const {
        auto pair = decompose(branch);
        return ScalarField2D(pair.first, xv_, yv_);
    }

    ScalarField2D imag_part_field(int branch = 0) const {
        auto pair = decompose(branch);
        return ScalarField2D(pair.second, xv_, yv_);
    }

    ScalarField2D magnitude_field(int branch = 0) const;

    ScalarField2D magnitude_sq_field(int branch = 0) const;
    
    ScalarField2D argument_field(int branch = 0) const;

    ScalarField2D log_magnitude_field(int branch = 0) const;

    ScalarField2D projection_field(ComplexProjection proj, int branch = 0) const;

    ComplexD evaluate(double x, double y, int branch = 0) const;

    ComplexD evaluate(const ComplexD& z, int branch = 0) const {
        return evaluate(z.real(), z.imaginary(), branch);
    }

    double evaluate(double x, double y, ComplexProjection proj, int branch = 0) const {
        return project(evaluate(x, y, branch), proj, branch);
    }

    std::function<double(double, double)> numerical_map(ComplexProjection proj, int branch = 0) const;

    std::function<ComplexD(double, double)> complex_map(int branch = 0) const;

    DomainColorSample domain_color_at(double x, double y, int branch = 0) const;

    std::vector<DomainColorSample> domain_color_grid(
        double x0, double x1, std::size_t nx,
        double y0, double y1, std::size_t ny,
        int branch = 0
    ) const;

    std::pair<cas::Expression, cas::Expression> cauchy_riemann_residuals(int branch = 0) const;

    CauchyRiemannSample cauchy_riemann_at(double x, double y, int branch = 0, double tol = constants::middle_epsilon()) const;

    bool is_holomorphic_at(double x, double y, int branch = 0, double tol = constants::middle_epsilon()) const {
        return cauchy_riemann_at(x, y, tol, branch).satisfies;
    }

    bool is_holomorphic_symbolic(int branch = 0) const;

    ComplexIntegrationResult contour_integral(
        const ParametricCurve2D& curve,
        double t0, double t1,
        int branch = 0,
        const integration::IntegrationLayer& layer = {}
    ) const;

    ComplexIntegrationResult contour_integral_circle(
        double cx, double cy, double r,
        int branch = 0,
        const integration::IntegrationLayer& layer = {}
    ) const;

    ComplexD residue_at(double cx, double cy, double r = 1e-3, int branch = 0, const integration::IntegrationLayer& layer = {}) const;

    ComplexD residue_at(const ComplexD& z0, double r = 1e-3, int branch = 0, const integration::IntegrationLayer& layer = {}) const {
        return residue_at(z0.real(), z0.imaginary(), r, branch, layer);
    }

    double winding_number(const geometry::ParametricCurve2D& curve, double t0, double t1, std::size_t n_samples = 1024, int branch = 0) const;

    double winding_number_circle(double cx, double cy, double r, std::size_t n_samples = 1024, int branch = 0) const;

    ComplexDomainField operator-() const { return { -f_, var_, xv_, yv_ }; }
    ComplexDomainField operator+(const ComplexDomainField& o) const { return { f_ + o.f_, var_, xv_, yv_ }; }
    ComplexDomainField operator-(const ComplexDomainField& o) const { return { f_ - o.f_, var_, xv_, yv_ }; }
    ComplexDomainField operator*(const ComplexDomainField& o) const { return { f_ * o.f_, var_, xv_, yv_ }; }
    ComplexDomainField operator/(const ComplexDomainField& o) const { return { f_ / o.f_, var_, xv_, yv_ }; }

    ComplexDomainField operator+(const ComplexD& c) const { return { f_ + c, var_, xv_, yv_ }; }
    ComplexDomainField operator-(const ComplexD& c) const { return { f_ - c, var_, xv_, yv_ }; }
    ComplexDomainField operator*(const ComplexD& c) const { return { f_ * c, var_, xv_, yv_ }; }
    ComplexDomainField operator/(const ComplexD& c) const { return { f_ / c, var_, xv_, yv_ }; }

    ComplexDomainField operator+(double s) const { return { f_ + s, var_, xv_, yv_ }; }
    ComplexDomainField operator-(double s) const { return { f_ - s, var_, xv_, yv_ }; }
    ComplexDomainField operator*(double s) const { return { f_ * s, var_, xv_, yv_ }; }
    ComplexDomainField operator/(double s) const { return { f_ / s, var_, xv_, yv_ }; }

    friend ComplexDomainField operator+(const ComplexD& c, const ComplexDomainField& f) { return f + c; }
    friend ComplexDomainField operator*(const ComplexD& c, const ComplexDomainField& f) { return f * c; }
    friend ComplexDomainField operator+(double s,          const ComplexDomainField& f) { return f + s; }
    friend ComplexDomainField operator*(double s,          const ComplexDomainField& f) { return f * s; }

    friend std::ostream& operator<<(std::ostream& os, const ComplexDomainField& cdf) {
        os << "f(" << cdf.var_ << ") = " << cdf.f_;
        return os;
    }

    std::string to_string() const { std::ostringstream ss; ss << *this; return ss.str(); }

private:
    using DecompCache = std::unordered_map<const cas::complex_symbols::ComplexMathExpressionNode*, std::pair<cas::Expression, cas::Expression>>;

    static double project(const ComplexD& w, ComplexProjection p, int branch = 0);

    static std::pair<cas::Expression, cas::Expression> p_const(double r, double i) { return { cas::Const(r), cas::Const(i) }; }
    static std::pair<cas::Expression, cas::Expression> p_zero() { return p_const(0, 0); }

    static std::pair<cas::Expression, cas::Expression> p_add(const std::pair<cas::Expression, cas::Expression>& a, const std::pair<cas::Expression, cas::Expression>& b) {
        return { a.first + b.first, a.second + b.second };
    }

    static std::pair<cas::Expression, cas::Expression> p_sub(const std::pair<cas::Expression, cas::Expression>& a, const std::pair<cas::Expression, cas::Expression>& b) {
        return { a.first - b.first, a.second - b.second };
    }

    static std::pair<cas::Expression, cas::Expression> p_neg(const std::pair<cas::Expression, cas::Expression>& a) {
        return { -a.first, -a.second };
    }

    static std::pair<cas::Expression, cas::Expression> p_mul(const std::pair<cas::Expression, cas::Expression>& a, const std::pair<cas::Expression, cas::Expression>& b) {
        return { a.first * b.first - a.second * b.second, a.first * b.second + a.second * b.first };
    }

    static std::pair<cas::Expression, cas::Expression> p_div(const std::pair<cas::Expression, cas::Expression>& a, const std::pair<cas::Expression, cas::Expression>& b);

    static std::pair<cas::Expression, cas::Expression> p_conj(const std::pair<cas::Expression, cas::Expression>& a) {
        return { a.first, -a.second };
    }

    static std::pair<cas::Expression, cas::Expression> p_recip(const std::pair<cas::Expression, cas::Expression>& a);

    static std::pair<cas::Expression, cas::Expression> p_exp(const std::pair<cas::Expression, cas::Expression>& a);

    static std::pair<cas::Expression, cas::Expression> p_ln(const std::pair<cas::Expression, cas::Expression>& a, int branch = 0);

    static std::pair<cas::Expression, cas::Expression> p_sqrt(const std::pair<cas::Expression, cas::Expression>& a, int branch = 0) {
        return p_exp(p_mul(p_const(0.5, 0), p_ln(a, branch)));
    }

    static std::pair<cas::Expression, cas::Expression> p_pow(const std::pair<cas::Expression, cas::Expression>& base, const std::pair<cas::Expression, cas::Expression>& exp, int branch = 0) {
        return p_exp(p_mul(exp, p_ln(base, branch)));
    }

    static std::pair<cas::Expression, cas::Expression> p_sin(const std::pair<cas::Expression, cas::Expression>& a) {
        return { cas::SIN(a.first) * cas::COSH(a.second), cas::COS(a.first) * cas::SINH(a.second) };
    }

    static std::pair<cas::Expression, cas::Expression> p_cos(const std::pair<cas::Expression, cas::Expression>& a) {
        return { cas::COS(a.first) * cas::COSH(a.second), -cas::SIN(a.first) * cas::SINH(a.second) };
    }

    static std::pair<cas::Expression, cas::Expression> p_sinh(const std::pair<cas::Expression, cas::Expression>& a) {
        return { cas::SINH(a.first) * cas::COS(a.second), cas::COSH(a.first) * cas::SIN(a.second) };
    }

    static std::pair<cas::Expression, cas::Expression> p_cosh(const std::pair<cas::Expression, cas::Expression>& a) {
        return { cas::COSH(a.first) * cas::COS(a.second), cas::SINH(a.first) * cas::SIN(a.second) };
    }

    static std::pair<cas::Expression, cas::Expression> p_asin(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0);

    static std::pair<cas::Expression, cas::Expression> p_acos(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0) {
        auto as = p_asin(z, branch);
        return { cas::Const(constants::pi_2()) - as.first, -as.second };
    }

    static std::pair<cas::Expression, cas::Expression> p_atan(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0);

    static std::pair<cas::Expression, cas::Expression> p_asinh(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0) {
        auto sq = p_sqrt(p_add(p_mul(z, z), p_const(1, 0)), branch);
        return p_ln(p_add(z, sq), branch);
    }

    static std::pair<cas::Expression, cas::Expression> p_acosh(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0);

    static std::pair<cas::Expression, cas::Expression> p_atanh(const std::pair<cas::Expression, cas::Expression>& z, int branch = 0);

    std::pair<cas::Expression, cas::Expression> decompose_node(const cas::complex_symbols::ComplexMathExpressionNode* n, DecompCache& dc, int branch) const;

private:
    cas::ComplexExpression f_;
    std::string var_, xv_, yv_;
    mutable std::unordered_map<int, std::pair<cas::Expression, cas::Expression>> cache_uv_;
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_DOMAIN_FIELD_HPP