#ifndef FIZMO_MATH_EVALUATOR_CLASS_HPP
#define FIZMO_MATH_EVALUATOR_CLASS_HPP

#include "expression.hpp"
#include "../../Basic/constants.hpp"

#include "../Math Impl/gamma.hpp"
#include "../Math Impl/error.hpp"
#include "../Math Impl/lambert_w.hpp"
#include "../Math Impl/chebyshev.hpp"
#include "../Math Impl/exp_log_integral.hpp"
#include "../Math Impl/trig_integrals.hpp"
#include "../Math Impl/fresnel.hpp"
#include "../Math Impl/spence.hpp"
#include "../Math Impl/rogers.hpp"
#include "../Math Impl/gudermannian.hpp"
#include "../Math Impl/fibonacci_lucas.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

inline double eval_node(const MathExpressionNode* n, const EvalContext& ctx) {
    if (is_invalid(n)) { return std::numeric_limits<double>::quiet_NaN(); }

    switch (n->type) {
        case NodeType::Constant: return n->constant;

        case NodeType::Variable: {
            auto id = n->variable.var_id;

            // Variable not in table 
            if (id >= ctx.values.size()) {
                switch (ctx.policy.missing_in_table) {
                    case MissingVariablePolicy::UseDefaultValue: return ctx.policy.default_value;
                    case MissingVariablePolicy::ReturnNaN: return std::numeric_limits<double>::quiet_NaN();
                    case MissingVariablePolicy::ThrowException: throw std::runtime_error("Variable ID out of range");
                }
            }

            // Variable exists but not assigned
            if (!ctx.assigned[id]) {
                switch (ctx.policy.missing_in_input) {
                    case MissingVariablePolicy::UseDefaultValue: return ctx.policy.default_value;
                    case MissingVariablePolicy::ReturnNaN: return std::numeric_limits<double>::quiet_NaN();
                    case MissingVariablePolicy::ThrowException: throw std::runtime_error("Variable not assigned");
                }
            }

            return ctx.values[id];
        }

        case NodeType::PositiveInfinity:  return  std::numeric_limits<double>::infinity();
        case NodeType::NegativeInfinity:  return -std::numeric_limits<double>::infinity();

        case NodeType::NaN:               
        case NodeType::Undefined:          
        case NodeType::Indeterminate:     
        case NodeType::Invalid:           
            return std::numeric_limits<double>::quiet_NaN();

        case NodeType::Add: return eval_node(n->binary.left, ctx) + eval_node(n->binary.right, ctx);
        case NodeType::Subtract: return eval_node(n->binary.left, ctx) - eval_node(n->binary.right, ctx);
        case NodeType::Multiply: return eval_node(n->binary.left, ctx) * eval_node(n->binary.right, ctx);

        case NodeType::Divide: {
            double num = eval_node(n->binary.left,  ctx);
            double den = eval_node(n->binary.right, ctx);
            if (den == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return num / den;
        }

        case NodeType::Modulo: {
            double a = eval_node(n->binary.left,  ctx);
            double b = eval_node(n->binary.right, ctx);
            if (b == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::fmod(a, b);
        }

        case NodeType::Negate: return -eval_node(n->unary.child, ctx);
        case NodeType::Floor: return std::floor(eval_node(n->unary.child, ctx));
        case NodeType::Ceil: return std::ceil(eval_node(n->unary.child, ctx));
        case NodeType::Round: return std::round(eval_node(n->unary.child, ctx));
        case NodeType::Truncate: return std::trunc(eval_node(n->unary.child, ctx));

        case NodeType::FractionalPart: {
            double v = eval_node(n->unary.child, ctx);
            return v - std::trunc(v);
        }

        case NodeType::IntegerPart: return std::trunc(eval_node(n->unary.child, ctx));
        case NodeType::AbsoluteValue: return std::abs(eval_node(n->unary.child, ctx));

        case NodeType::Sign: {
            double v = eval_node(n->unary.child, ctx);
            if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
            if (v > 0.0) return 1.0;
            if (v < 0.0) return -1.0;
            return 0.0;
        }

        case NodeType::UnitStep: {
            double v = eval_node(n->unary.child, ctx);
            if (std::isnan(v)) return std::numeric_limits<double>::quiet_NaN();
            return v >= 0.0 ? 1.0 : 0.0;
        }

        case NodeType::Combination: {
            double nn = eval_node(n->binary.left,  ctx);
            double kk = eval_node(n->binary.right, ctx);
            if (std::isnan(nn) || std::isnan(kk)) return std::numeric_limits<double>::quiet_NaN();
            double num = std::tgamma(nn + 1.0);
            double den = std::tgamma(kk + 1.0) * std::tgamma(nn - kk + 1.0);
            if (den == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return num / den;
        }

        case NodeType::Permutation: {
            double nn = eval_node(n->binary.left,  ctx);
            double kk = eval_node(n->binary.right, ctx);
            if (std::isnan(nn) || std::isnan(kk)) return std::numeric_limits<double>::quiet_NaN();
            double den = std::tgamma(nn - kk + 1.0);
            if (den == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::tgamma(nn + 1.0) / den;
        }

        case NodeType::NaturalExp: return std::exp(eval_node(n->unary.child, ctx));

        case NodeType::Power: {
            double base = eval_node(n->power.base,     ctx);
            double exp  = eval_node(n->power.exponent, ctx);
            if (base == 0.0 && exp == 0.0) return std::numeric_limits<double>::quiet_NaN();
            if (base == 0.0 && exp < 0.0)  return std::numeric_limits<double>::quiet_NaN();
            return std::pow(base, exp);
        }

        case NodeType::Root: {
            double radicand = eval_node(n->binary.left,  ctx);
            double degree   = eval_node(n->binary.right, ctx);
            if (degree == 0.0) return std::numeric_limits<double>::quiet_NaN();
            if (radicand < 0.0) {
                double intpart;
                if (std::modf(degree, &intpart) != 0.0) return std::numeric_limits<double>::quiet_NaN(); 
                if (std::fmod(intpart, 2.0) == 0.0) return std::numeric_limits<double>::quiet_NaN(); 
                return -std::pow(-radicand, 1.0 / degree);
            }
            return std::pow(radicand, 1.0 / degree);
        }

        case NodeType::Sqrt: {
            double v = eval_node(n->unary.child, ctx);
            if (v < 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::sqrt(v);
        }

        case NodeType::Cbrt: return std::cbrt(eval_node(n->unary.child, ctx));

        case NodeType::NaturalLog: {
            double v = eval_node(n->unary.child, ctx);
            if (v <  0.0) return std::numeric_limits<double>::quiet_NaN();    
            if (v == 0.0) return -std::numeric_limits<double>::infinity();  
            return std::log(v);
        }

        case NodeType::Log: {
            double arg  = eval_node(n->binary.left,  ctx);
            double base = eval_node(n->binary.right, ctx);
            if (base <= 0.0 || base == 1.0) return std::numeric_limits<double>::quiet_NaN();  
            if (arg  <  0.0) return std::numeric_limits<double>::quiet_NaN();                  
            if (arg  == 0.0) return -std::numeric_limits<double>::infinity();               
            return std::log(arg) / std::log(base);
        }

        // Trigonometric
        case NodeType::Sin: return std::sin(eval_node(n->unary.child, ctx));

        case NodeType::Sinc: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0) return 1.0;
            return std::sin(v) / v;
        }

        case NodeType::NormalSinc: {
            double v = eval_node(n->unary.child, ctx) * constants::pi();
            if (v == 0.0) return 1.0;
            return std::sin(v) / v;
        }

        case NodeType::Cos: return std::cos(eval_node(n->unary.child, ctx));

        case NodeType::Tan: {
            double v = eval_node(n->unary.child, ctx);
            double c = std::cos(v);
            if (c == 0.0) return std::numeric_limits<double>::quiet_NaN();  
            return std::tan(v);
        }

        case NodeType::Csc: {
            double v = eval_node(n->unary.child, ctx);
            double s = std::sin(v);
            if (s == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return 1.0 / s;
        }

        case NodeType::Sec: {
            double v = eval_node(n->unary.child, ctx);
            double c = std::cos(v);
            if (c == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return 1.0 / c;
        }

        case NodeType::Cot: {
            double v = eval_node(n->unary.child, ctx);
            double s = std::sin(v);
            if (s == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::cos(v) / s;
        }

        case NodeType::Arcsin: {
            double v = eval_node(n->unary.child, ctx);
            if (v < -1.0 || v > 1.0) return std::numeric_limits<double>::quiet_NaN();
            return std::asin(v);
        }

        case NodeType::Arccos: {
            double v = eval_node(n->unary.child, ctx);
            if (v < -1.0 || v > 1.0) return std::numeric_limits<double>::quiet_NaN();
            return std::acos(v);
        }

        case NodeType::Arctan: return std::atan(eval_node(n->unary.child, ctx));

        case NodeType::Arccsc: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0 || (v > -1.0 && v < 1.0)) return std::numeric_limits<double>::quiet_NaN();
            return std::asin(1.0 / v);
        }

        case NodeType::Arcsec: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0 || (v > -1.0 && v < 1.0)) return std::numeric_limits<double>::quiet_NaN();
            return std::acos(1.0 / v);
        }

        case NodeType::Arccot: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0) return constants::pi_2();
            return std::atan(1.0 / v);
        }

        // Hyperbolic
        case NodeType::Sinh: return std::sinh(eval_node(n->unary.child, ctx));
        case NodeType::Cosh: return std::cosh(eval_node(n->unary.child, ctx));
        case NodeType::Tanh: return std::tanh(eval_node(n->unary.child, ctx));
        
        case NodeType::Csch: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return 1.0 / std::sinh(v);
        }

        case NodeType::Sech: { double v = eval_node(n->unary.child, ctx); return 1.0 / std::cosh(v); }
        
        case NodeType::Coth: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return 1.0 / std::tanh(v);
        }

        // Inverse hyperbolic
        case NodeType::Arcsinh: { double v = eval_node(n->unary.child, ctx); return std::asinh(v); }
        
        case NodeType::Arccosh: {
            double v = eval_node(n->unary.child, ctx);
            if (v < 1.0) return std::numeric_limits<double>::quiet_NaN();  
            return std::acosh(v);
        }

        case NodeType::Arctanh: {
            double v = eval_node(n->unary.child, ctx);
            if (v <= -1.0 || v >= 1.0) return std::numeric_limits<double>::quiet_NaN();  
            return std::atanh(v);
        }

        case NodeType::Arccsch: {
            double v = eval_node(n->unary.child, ctx);
            if (v == 0.0) return std::numeric_limits<double>::quiet_NaN();
            return std::asinh(1.0 / v);
        }

        case NodeType::Arcsech: {
            double v = eval_node(n->unary.child, ctx);
            if (v <= 0.0 || v > 1.0) return std::numeric_limits<double>::quiet_NaN();  
            return std::acosh(1.0 / v);
        }

        case NodeType::Arccoth: {
            double v = eval_node(n->unary.child, ctx);
            if (v >= -1.0 && v <= 1.0) return std::numeric_limits<double>::quiet_NaN();  
            return std::atanh(1.0 / v);
        }

        case NodeType::Erf:  return std::erf (eval_node(n->unary.child, ctx));
        case NodeType::Erfc: return std::erfc(eval_node(n->unary.child, ctx));

        case NodeType::ErfGeneralized: {
            double a = eval_node(n->binary.left,  ctx);
            double b = eval_node(n->binary.right, ctx);
            return std::erf(a) - std::erf(b);
        }
        
        case NodeType::ErfcGeneralized: {
            double a = eval_node(n->binary.left,  ctx);
            double b = eval_node(n->binary.right, ctx);
            return std::erfc(a) - std::erfc(b);
        }

        case NodeType::Erfi:        return erfi(eval_node(n->unary.child, ctx));
        case NodeType::InverseErf:  return inverse_erf(eval_node(n->unary.child, ctx));
        case NodeType::InverseErfc: return inverse_erfc(eval_node(n->unary.child, ctx));
        case NodeType::InverseErfi: return inverse_erfi(eval_node(n->unary.child, ctx));

        case NodeType::Gamma: return std::tgamma(eval_node(n->unary.child, ctx)); 
        case NodeType::Factorial: return std::tgamma(eval_node(n->unary.child, ctx) + 1.0); 
        case NodeType::Digamma: return digamma(eval_node(n->unary.child, ctx));
        case NodeType::Trigamma: return trigamma(eval_node(n->unary.child, ctx));
        case NodeType::BetaFunction: return beta_function(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));
        case NodeType::Polygamma: return polygamma(eval_node(n->binary.left, ctx), static_cast<int>(eval_node(n->binary.right, ctx)));

        case NodeType::LambertW: {
            double z = eval_node(n->binary.left,  ctx);   
            double k = eval_node(n->binary.right, ctx);  
            int branch = static_cast<int>(k);

            if (branch == 0)
                return lambert_w0(z);
            else if (branch == -1)
                return lambert_wn1(z);
            else
                return std::numeric_limits<double>::quiet_NaN();
        }

        case NodeType::ChebyshevU: return chebyshev_u(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));
        case NodeType::ChebyshevT: return chebyshev_u(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));

        case NodeType::ExponentialIntegral: return exp_integral(eval_node(n->unary.child, ctx));
        case NodeType::LogarithmicIntegral: return log_integral(eval_node(n->unary.child, ctx));
        case NodeType::ExponentialIntegralGeneralized: return exp_integral(eval_node(n->binary.left, ctx), eval_node(n->binary.left, ctx)); 
        case NodeType::LogarithmicIntegralGeneralized: return log_integral(eval_node(n->binary.left, ctx), eval_node(n->binary.left, ctx)); 

        case NodeType::SinIntegral:  return sin_integral (eval_node(n->unary.child, ctx));
        case NodeType::CosIntegral:  return cos_integral (eval_node(n->unary.child, ctx));
        case NodeType::SinhIntegral: return sinh_integral(eval_node(n->unary.child, ctx));
        case NodeType::CoshIntegral: return cosh_integral(eval_node(n->unary.child, ctx));

        case NodeType::FresnelS: return fresnel_s(eval_node(n->unary.child, ctx));
        case NodeType::FresnelC: return fresnel_c(eval_node(n->unary.child, ctx));

        case NodeType::RiemannZeta: return riemann_zeta(eval_node(n->unary.child, ctx));

        case NodeType::Dilogarithm:  return dilogarithm(eval_node(n->unary.child, ctx));
        case NodeType::Trilogarithm: return trilogarithm(eval_node(n->unary.child, ctx));
        case NodeType::Polylog:      return polylog(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));

        case NodeType::SpenceFunction: return spence_function(eval_node(n->unary.child, ctx));
        case NodeType::SpenceIntegral: return spence_integral(eval_node(n->unary.child, ctx));
        
        case NodeType::RogersL:  return rogers_function(eval_node(n->unary.child, ctx));
        case NodeType::RogersLR: return rogers_dilogarithm(eval_node(n->unary.child, ctx));

        case NodeType::Gudermannian:        return gudermannian(eval_node(n->unary.child, ctx));
        case NodeType::InverseGudermannian: return inverse_gudermannian(eval_node(n->unary.child, ctx)); 

        case NodeType::FibonacciSequence:   return fibonacci(eval_node(n->unary.child, ctx));
        case NodeType::LucasSequence:       return lucas(eval_node(n->unary.child, ctx));
        case NodeType::FibonacciPolynomial: return fibonacci_polynomial(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));
        case NodeType::LucasPolynomial:     return lucas_polynomial(eval_node(n->binary.left, ctx), eval_node(n->binary.right, ctx));

        default:
            return std::nan("");
    }
}

inline double evaluate(MathExpression e, const EvalContext& ctx) {
    return eval_node(e.get(), ctx);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_EVALUATOR_CLASS_HPP