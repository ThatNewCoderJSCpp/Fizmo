#ifndef FIZMO_COMPLEX_EVALUATOR_HPP
#define FIZMO_COMPLEX_EVALUATOR_HPP

#include "complex_expression.hpp"

#include "../../../Complex/trig.hpp"
#include "../../../Complex/htrig.hpp"
#include "../../../Complex/inv_trig.hpp"
#include "../../../Complex/inv_htrig.hpp"
#include "../../../Complex/log_pow_root.hpp"
#include "../../../Complex/util.hpp"

#include <limits>
#include <stdexcept>

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

inline ComplexD complex_nan() { return ComplexD(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()); }
inline ComplexD complex_inf() { return ComplexD(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()); }

inline ComplexD eval_complex_node(const ComplexMathExpressionNode* n, const ComplexEvalContext& ctx) {
    if (is_invalid(n)) return complex_nan();

    const int branch = ctx.policy.branch;

    switch (n->type) {
        case ComplexNodeType::Constant:
            return ComplexD(n->constant.real, n->constant.imag);

        case ComplexNodeType::Variable: {
            auto id = n->variable.var_id;

            if (id >= ctx.values.size()) {
                switch (ctx.policy.missing_in_table) {
                    case ComplexMissingVariablePolicy::UseDefaultValue: return ctx.policy.default_value;
                    case ComplexMissingVariablePolicy::ReturnNaN:       return complex_nan();
                    case ComplexMissingVariablePolicy::ThrowException:  throw std::runtime_error("Complex variable ID out of range");
                }
            }

            if (!ctx.assigned[id]) {
                switch (ctx.policy.missing_in_input) {
                    case ComplexMissingVariablePolicy::UseDefaultValue: return ctx.policy.default_value;
                    case ComplexMissingVariablePolicy::ReturnNaN:       return complex_nan();
                    case ComplexMissingVariablePolicy::ThrowException:  throw std::runtime_error("Complex variable not assigned");
                }
            }

            return ctx.values[id];
        }

        case ComplexNodeType::Infinity: return complex_inf();

        case ComplexNodeType::NaN:
        case ComplexNodeType::Undefined:
        case ComplexNodeType::Indeterminate:
        case ComplexNodeType::Invalid:
            return complex_nan();

        case ComplexNodeType::Add:      return eval_complex_node(n->binary.left, ctx) + eval_complex_node(n->binary.right, ctx);
        case ComplexNodeType::Subtract: return eval_complex_node(n->binary.left, ctx) - eval_complex_node(n->binary.right, ctx);
        case ComplexNodeType::Multiply: return eval_complex_node(n->binary.left, ctx) * eval_complex_node(n->binary.right, ctx);

        case ComplexNodeType::Divide: {
            ComplexD num = eval_complex_node(n->binary.left,  ctx);
            ComplexD den = eval_complex_node(n->binary.right, ctx);
            if (den.magnitude_squared() == 0.0) return complex_nan();
            return num / den;
        }

        case ComplexNodeType::Negate: return -eval_complex_node(n->unary.child, ctx);

        case ComplexNodeType::Power: {
            ComplexD base = eval_complex_node(n->power.base, ctx);
            ComplexD exp  = eval_complex_node(n->power.exponent, ctx);
            return cx::pow(base, exp, branch);
        }

        case ComplexNodeType::Root: {
            ComplexD radicand = eval_complex_node(n->binary.left,  ctx);
            ComplexD degree   = eval_complex_node(n->binary.right, ctx);
            return cx::nth_root(radicand, degree, branch);
        }

        case ComplexNodeType::Sqrt:       return cx::sqrt(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Cbrt:       return cx::cbrt(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::NaturalExp: return cx::exp(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::NaturalLog: return cx::ln(eval_complex_node(n->unary.child, ctx), branch);

        case ComplexNodeType::Log: {
            ComplexD arg  = eval_complex_node(n->binary.left,  ctx);
            ComplexD base = eval_complex_node(n->binary.right, ctx);
            return cx::log(arg, base, branch);
        }

        case ComplexNodeType::Sin:  return cx::sin(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cos:  return cx::cos(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Tan:  return cx::tan(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Csc:  return cx::csc(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Sec:  return cx::sec(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cot:  return cx::cot(eval_complex_node(n->unary.child, ctx));

        case ComplexNodeType::Arcsin:  return cx::asin(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccos:  return cx::acos(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arctan:  return cx::atan(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccsc:  return cx::acsc(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arcsec:  return cx::asec(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccot:  return cx::acot(eval_complex_node(n->unary.child, ctx), branch);

        case ComplexNodeType::Sinh:  return cx::sinh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cosh:  return cx::cosh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Tanh:  return cx::tanh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Csch:  return cx::csch(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Sech:  return cx::sech(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Coth:  return cx::coth(eval_complex_node(n->unary.child, ctx));

        case ComplexNodeType::Arcsinh:  return cx::asinh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccosh:  return cx::acosh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arctanh:  return cx::atanh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccsch:  return cx::acsch(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arcsech:  return cx::asech(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccoth:  return cx::acoth(eval_complex_node(n->unary.child, ctx), branch);

        case ComplexNodeType::Conjugate: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            return z.conjugate();
        }

        case ComplexNodeType::RealPart: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            return ComplexD(z.real(), 0.0);
        }

        case ComplexNodeType::ImaginaryPart: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            return ComplexD(z.imaginary(), 0.0);
        }

        case ComplexNodeType::Magnitude: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            return ComplexD(z.magnitude(), 0.0);
        }

        case ComplexNodeType::Argument: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            return ComplexD(z.argument(branch), 0.0);
        }

        case ComplexNodeType::Reciprocal: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            if (z.magnitude_squared() == 0.0) return complex_nan();
            return z.reciprocal();
        }

        case ComplexNodeType::Sign: {
            ComplexD z = eval_complex_node(n->unary.child, ctx);
            double mag = z.magnitude();
            if (mag == 0.0) return ComplexD(0.0, 0.0);
            return z / mag;
        }

        default: return complex_nan();
    }
}

inline ComplexD evaluate(ComplexMathExpression e, const ComplexEvalContext& ctx) { return eval_complex_node(e.get(), ctx); }

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_EVALUATOR_HPP