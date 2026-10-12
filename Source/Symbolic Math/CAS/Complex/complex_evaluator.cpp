#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

ComplexD complex_nan() { return ComplexD(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()); }

ComplexD complex_inf() { return ComplexD(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()); }

ComplexD eval_complex_node(const ComplexMathExpressionNode* n, const ComplexEvalContext& ctx) {
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
            return fizmo::math::pow(base, exp, branch);
        }

        case ComplexNodeType::Root: {
            ComplexD radicand = eval_complex_node(n->binary.left,  ctx);
            ComplexD degree   = eval_complex_node(n->binary.right, ctx);
            return fizmo::math::nth_root(radicand, degree, branch);
        }

        case ComplexNodeType::Sqrt:       return fizmo::math::nth_root(eval_complex_node(n->unary.child, ctx), 2, branch);
        case ComplexNodeType::Cbrt:       return fizmo::math::nth_root(eval_complex_node(n->unary.child, ctx), 3, branch);
        case ComplexNodeType::NaturalExp: return fizmo::math::exp(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::NaturalLog: return fizmo::math::ln(eval_complex_node(n->unary.child, ctx), branch);

        case ComplexNodeType::Log: {
            ComplexD arg  = eval_complex_node(n->binary.left,  ctx);
            ComplexD base = eval_complex_node(n->binary.right, ctx);
            return fizmo::math::log(arg, base, branch);
        }

        case ComplexNodeType::Sin:  return fizmo::math::sin(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cos:  return fizmo::math::cos(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Tan:  return fizmo::math::tan(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Csc:  return fizmo::math::csc(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Sec:  return fizmo::math::sec(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cot:  return fizmo::math::cot(eval_complex_node(n->unary.child, ctx));

        case ComplexNodeType::Arcsin:  return fizmo::math::arcsin(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccos:  return fizmo::math::arccos(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arctan:  return fizmo::math::arctan(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccsc:  return fizmo::math::arccsc(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arcsec:  return fizmo::math::arcsec(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccot:  return fizmo::math::arccot(eval_complex_node(n->unary.child, ctx), branch);

        case ComplexNodeType::Sinh:  return fizmo::math::sinh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Cosh:  return fizmo::math::cosh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Tanh:  return fizmo::math::tanh(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Csch:  return fizmo::math::csch(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Sech:  return fizmo::math::sech(eval_complex_node(n->unary.child, ctx));
        case ComplexNodeType::Coth:  return fizmo::math::coth(eval_complex_node(n->unary.child, ctx));

        case ComplexNodeType::Arcsinh:  return fizmo::math::arcsinh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccosh:  return fizmo::math::arccosh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arctanh:  return fizmo::math::arctanh(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccsch:  return fizmo::math::arccsch(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arcsech:  return fizmo::math::arcsech(eval_complex_node(n->unary.child, ctx), branch);
        case ComplexNodeType::Arccoth:  return fizmo::math::arccoth(eval_complex_node(n->unary.child, ctx), branch);

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

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo
