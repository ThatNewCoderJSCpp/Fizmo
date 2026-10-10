#ifndef FIZMO_MATH_IMPLICIT_CURVE_HPP
#define FIZMO_MATH_IMPLICIT_CURVE_HPP

#include "scalar_field.hpp"
#include <utility>
#include <cmath>

namespace fizmo { 
namespace math { 
namespace geometry {

class ImplicitCurve {
    ScalarField2D F_;
public:
    explicit ImplicitCurve(cas::Expression F, std::string xvar = "x", std::string yvar = "y") : F_(std::move(F), std::move(xvar), std::move(yvar)) {}
    explicit ImplicitCurve(ScalarField2D F) : F_(std::move(F)) {}

    const cas::Expression& expression() const { return F_.expression(); }
    const std::string&     xvar()       const { return F_.xvar(); }
    const std::string&     yvar()       const { return F_.yvar(); }
    const ScalarField2D&   field()      const { return F_; }

    cas::Expression dy_dx() const { return cas::SIMPLIFY(-(F_.partial_x() / F_.partial_y())); }
    cas::Expression dx_dy() const { return cas::SIMPLIFY(-(F_.partial_y() / F_.partial_x())); }

    cas::Expression second_deriv_num() const {
        auto Fx = F_.partial_x();
        auto Fy = F_.partial_y();
        auto Fxx = cas::DIFFERENTIATE(Fx, xvar());
        auto Fyy = cas::DIFFERENTIATE(Fy, yvar());
        auto Fxy = cas::DIFFERENTIATE(Fx, yvar());
        auto& M = F_.expression().manager();
        auto K  = [&](double v){ return cas::Expression(v, M); };
        return Fxx*cas::POW(Fy, 2.0) - K(2.0)*Fxy*Fx*Fy + Fyy*cas::POW(Fx, 2.0);
    }

    cas::Expression d2y_dx2() const {
        return cas::SIMPLIFY(-(second_deriv_num() / cas::POW(F_.partial_y(), 3.0)));
    }

    cas::Expression d2x_dy2() const {
        return cas::SIMPLIFY(-(second_deriv_num() / cas::POW(F_.partial_x(), 3.0)));
    }

    cas::Expression nth_implicit_derivative(
        const std::string& indep, const std::string& dep, unsigned int n
    ) const {
        assert(n >= 1 && "nth_implicit_derivative: order must be >= 1");
        auto F_i = cas::DIFFERENTIATE(F_.expression(), indep);
        auto F_d = cas::DIFFERENTIATE(F_.expression(), dep);
        const cas::Expression d1 = cas::SIMPLIFY(-(F_i / F_d));   
        cas::Expression D = d1;
        
        for (unsigned k = 1; k < n; ++k) {
            D = cas::SIMPLIFY(cas::DIFFERENTIATE(D, indep) + cas::DIFFERENTIATE(D, dep) * d1);
        }

        return D;
    }

    cas::Expression nth_dy_dx(unsigned int n) const { return nth_implicit_derivative(xvar(), yvar(), n); }
    cas::Expression nth_dx_dy(unsigned int n) const { return nth_implicit_derivative(yvar(), xvar(), n); }

    vectors::SymbolicVector2 gradient() const { return F_.gradient(); }   

    vector2d gradient_at(double x, double y) const {
        return { 
            F_.partial_x().evaluate({{xvar(), x}, {yvar(), y}}),
            F_.partial_y().evaluate({{xvar(), x}, {yvar(), y}}) 
        };
    }

    vector2d normal_at(double x, double y) const { return gradient_at(x, y); }

    vector2d tangent_at(double x, double y) const {
        auto gradient = gradient_at(x, y);      
        return { -gradient.y, gradient.x };
    }

    bool on_curve(
        double x, double y,
        double atol = constants::middle_epsilon(),
        double rtol = constants::epsilon()
    ) const {
        double val = std::abs(F_.evaluate(x, y));
        return val <= atol || val <= rtol * std::max(1.0, val);
    }

    bool is_regular_at(
        double x, double y, 
        double atol = constants::middle_epsilon(),
        double rtol = constants::epsilon()
    ) const {
        auto gradient = gradient_at(x, y);
        const double g2 = gradient.magnitude_squared();
        return (g2 > atol) || (g2 > rtol * std::max(1.0, g2));         
    }

    bool is_singular_at(
        double x, double y, 
        double atol = constants::middle_epsilon(),
        double rtol = constants::epsilon()
    ) const {
        return !is_regular_at(x, y, atol, rtol);
    }

    enum class SolvableFor { Both = 0, YofX, XofY, Neither };

    SolvableFor solvable_at(
        double x, double y, 
        double atol = constants::middle_epsilon(),
        double rtol = constants::epsilon()
    ) const {
        auto gradient = gradient_at(x, y);
        bool fx = (std::abs(gradient.x) > atol) || (std::abs(gradient.x) > rtol * std::max(1.0, std::abs(gradient.x)));
        bool fy = (std::abs(gradient.y) > atol) || (std::abs(gradient.y) > rtol * std::max(1.0, std::abs(gradient.y)));
        if (fx && fy) return SolvableFor::Both;
        if (fy)       return SolvableFor::YofX;   
        if (fx)       return SolvableFor::XofY;   
        return SolvableFor::Neither;              
    }

    friend std::ostream& operator<<(std::ostream& os, const ImplicitCurve& c) {
        os << c.F_.expression().full_simplify() << " = 0";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

} // namespace geometry
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_IMPLICIT_CURVE_HPP