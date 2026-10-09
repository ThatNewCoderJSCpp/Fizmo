#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

auto MathExpressionSimplifier::dominant_poison(MathExpressionNode* a, MathExpressionNode* b) const -> MathExpressionNode* {
    auto rank = [](NodeType t) -> int {
        if (t == NodeType::Undefined)     return 3;
        if (t == NodeType::NaN)           return 2;
        if (t == NodeType::Indeterminate) return 1;
        return 0;
    };
    int ra = rank(a->type), rb = rank(b->type);
    if (ra == 0 && rb == 0) return nullptr;
    return (ra >= rb) ? a : b;
}

bool MathExpressionSimplifier::is_square(MathExpressionNode* n, NodeType fn, MathExpressionNode*& inner) const {
    if (n->type != NodeType::Power) return false;
    if (!is_const(n->power.exponent, 2.0)) return false;
    if (n->power.base->type != fn) return false;
    inner = n->power.base->unary.child;
    return true;
}

auto MathExpressionSimplifier::simplify_node(MathExpressionNode* n) -> MathExpression {
    switch (n->type) {
        case NodeType::Constant:
        case NodeType::Variable:
        case NodeType::Indeterminate:
        case NodeType::PositiveInfinity:
        case NodeType::NegativeInfinity:
        case NodeType::NaN:
        case NodeType::Undefined:
        case NodeType::Invalid:
            return MathExpression(n);

        case NodeType::AppliedFunction: return simplify_applied(n);

        case NodeType::Negate:     return simplify_negate(n);
        case NodeType::Add:        return simplify_add(n);
        case NodeType::Subtract:   return simplify_subtract(n);
        case NodeType::Multiply:   return simplify_multiply(n);
        case NodeType::Divide:     return simplify_divide(n);
        case NodeType::Sign:       return simplify_sign(n);
        case NodeType::UnitStep:   return simplify_unit_step(n);
        case NodeType::Power:      return simplify_power(n);
        case NodeType::NaturalExp: return simplify_exp(n);
        case NodeType::NaturalLog: return simplify_ln(n);
        case NodeType::Log:        return simplify_log(n);
        case NodeType::Sqrt:       return simplify_sqrt(n);
        case NodeType::Cbrt:       return simplify_cbrt(n);
        case NodeType::Sinc:       return simplify_sinc(n);
        case NodeType::NormalSinc: return simplify_normalsinc(n);
        case NodeType::Erf:
        case NodeType::Erfc:       
            return simplify_erf_erfc(n);
        case NodeType::Erfi:       return simplify_erfi(n); 
        case NodeType::InverseErf: 
        case NodeType::InverseErfc: 
        case NodeType::InverseErfi:  
            return simplify_inverse_erf_family(n); 
        case NodeType::ErfGeneralized:
        case NodeType::ErfcGeneralized: 
            return simplify_erf_generalized(n);
        case NodeType::Gamma:
        case NodeType::Factorial: 
            return simplify_gamma_factorial(n);
        case NodeType::Digamma:
        case NodeType::Trigamma:     
            return simplify_digamma_trigamma(n);
        case NodeType::BetaFunction: 
            return simplify_beta(n);
        case NodeType::LambertW:     
            return simplify_lambert_w(n);
        case NodeType::ChebyshevU:   
        case NodeType::ChebyshevT:   
            return simplify_chebyshev(n);
        case NodeType::ExponentialIntegral:            
            return simplify_exp_integral(n);
        case NodeType::LogarithmicIntegral:            
            return simplify_log_integral(n);
        case NodeType::ExponentialIntegralGeneralized: 
            return simplify_exp_integral_gen(n);
        case NodeType::LogarithmicIntegralGeneralized: 
            return simplify_log_integral_gen(n);
        case NodeType::BellPolynomial: 
            return simplify_bell(n);
        case NodeType::SinIntegral:
        case NodeType::CosIntegral:
        case NodeType::SinhIntegral:
        case NodeType::CoshIntegral:
            return simplify_sin_cos_integral(n);
        case NodeType::FresnelS:
        case NodeType::FresnelC:
            return simplify_fresnel(n);
        case NodeType::Dilogarithm:
        case NodeType::Trilogarithm:
            return simplify_di_tri_log(n);
        case NodeType::Polylog:
            return simplify_polylog(n);
        case NodeType::RiemannZeta:
            return simplify_riemann(n);
        case NodeType::SpenceFunction:
        case NodeType::SpenceIntegral:
            return simplify_spence(n);
        case NodeType::RogersL:
        case NodeType::RogersLR:
            return simplify_spence(n);
        case NodeType::Gudermannian:
        case NodeType::InverseGudermannian:
            return simplify_gudermannian(n);
        case NodeType::Combination: 
            return simplify_combination(n);
        case NodeType::Permutation: 
            return simplify_permutation(n);
        case NodeType::FibonacciSequence:
        case NodeType::LucasSequence:
            return simplify_fib_lucas_sequence(n);
        case NodeType::FibonacciPolynomial:
        case NodeType::LucasPolynomial:
            return simplify_fib_lucas_poly(n);
                
        default: 
            return simplify_generic(n);
    }
}

auto MathExpressionSimplifier::rebuild_unary(NodeType t, MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get())) {
        EvalContext ctx;
        MathExpression tmp = mgr_.unary(t, child);
        double v = eval_node(tmp.get(), ctx);
        if (!std::isnan(v) && !std::isinf(v)) return konst(v);
        if (std::isinf(v)) return v > 0 ? pos_inf() : neg_inf();
        return undef();
    }
    return mgr_.unary(t, child);
}

auto MathExpressionSimplifier::rebuild_binary(NodeType t, MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_const(l.get()) && is_const(r.get())) {
        EvalContext ctx;
        MathExpression tmp = mgr_.binary(t, l, r);
        double v = eval_node(tmp.get(), ctx);
        if (!std::isnan(v) && !std::isinf(v)) return konst(v);
        if (std::isinf(v)) return v > 0 ? pos_inf() : neg_inf();
        return undef();
    }
    return mgr_.binary(t, l, r);
}

auto MathExpressionSimplifier::simplify_generic(MathExpressionNode* n) -> MathExpression {
    if (NodeKeyHash::is_unary(n->type))  return rebuild_unary(n->type, n);
    if (NodeKeyHash::is_binary(n->type)) return rebuild_binary(n->type, n);
    return MathExpression(n);
}

auto MathExpressionSimplifier::simplify_negate(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))           return MathExpression(child.get());
    if (is_const(child.get()))            return konst(-child.get()->constant);
    if (is_infinite(child.get()))         return negate_inf(child.get());
    if (child.get()->type == NodeType::Negate) return MathExpression(child.get()->unary.child); 
    return mgr_.unary(NodeType::Negate, child);
}

auto MathExpressionSimplifier::simplify_add(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        if (is_infinite(l.get()) && is_infinite(r.get())) {
            return inf_sign(l.get()) == inf_sign(r.get()) ? MathExpression(l.get()) : indet();
        }
        return is_infinite(l.get()) ? MathExpression(l.get()) : MathExpression(r.get());
    }

    if (is_const(l.get()) && is_const(r.get())) return konst(l.get()->constant + r.get()->constant);
    if (is_const(l.get(), 0.0)) return r;
    if (is_const(r.get(), 0.0)) return l;

    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if ((is_square(l.get(), NodeType::Sin, i1) && is_square(r.get(), NodeType::Cos, i2) && i1 == i2) ||
            (is_square(l.get(), NodeType::Cos, i1) && is_square(r.get(), NodeType::Sin, i2) && i1 == i2))
            return konst(1.0);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_square(l.get(), NodeType::Tan, inner) && is_const(r.get(), 1.0)) return square(NodeType::Sec, inner);
        if (is_square(r.get(), NodeType::Tan, inner) && is_const(l.get(), 1.0)) return square(NodeType::Sec, inner);
        if (is_square(l.get(), NodeType::Cot, inner) && is_const(r.get(), 1.0)) return square(NodeType::Csc, inner);
        if (is_square(r.get(), NodeType::Cot, inner) && is_const(l.get(), 1.0)) return square(NodeType::Csc, inner);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_square(l.get(), NodeType::Sinh, inner) && is_const(r.get(), 1.0)) return square(NodeType::Cosh, inner);
        if (is_square(r.get(), NodeType::Sinh, inner) && is_const(l.get(), 1.0)) return square(NodeType::Cosh, inner);
        if (is_square(l.get(), NodeType::Coth, inner) && is_const(r.get(), 1.0)) return square(NodeType::Csch, inner);
        if (is_square(r.get(), NodeType::Coth, inner) && is_const(l.get(), 1.0)) return square(NodeType::Csch, inner);
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cos, i1) &&
            r.get()->type == NodeType::Negate && is_square(r.get()->unary.child, NodeType::Sin, i2) &&
            i1 == i2)
        {
            return mgr_.unary(NodeType::Cos, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        }
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if ((is_square(l.get(), NodeType::Cosh, i1) && is_square(r.get(), NodeType::Sinh, i2) && i1 == i2) ||
            (is_square(l.get(), NodeType::Sinh, i1) && is_square(r.get(), NodeType::Cosh, i2) && i1 == i2))
        {
            return mgr_.unary(NodeType::Cosh, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        }
    }

    if (r.get()->type == NodeType::Negate) {
        return mgr_.binary(NodeType::Subtract, l, MathExpression(r.get()->unary.child));
    }

    if (r.get()->type == NodeType::Multiply) {
        auto* rl = r.get()->binary.left;
        if (rl->type == NodeType::Constant && rl->constant < 0.0) {
            MathExpression pos_const = konst(-rl->constant);
            MathExpression pos_mul   = mgr_.binary(NodeType::Multiply, pos_const, MathExpression(r.get()->binary.right));
            return mgr_.binary(NodeType::Subtract, l, pos_mul);
        }
    }

    return mgr_.binary(NodeType::Add, l, r);
}

auto MathExpressionSimplifier::simplify_subtract(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        if (is_infinite(l.get()) && is_infinite(r.get())) {
            return inf_sign(l.get()) == inf_sign(r.get()) ? indet() : MathExpression(l.get());
        }
        if (is_infinite(l.get())) return MathExpression(l.get());
        return negate_inf(r.get());
    }
    if (is_const(l.get()) && is_const(r.get())) return konst(l.get()->constant - r.get()->constant);
    if (is_const(r.get(), 0.0)) return l;
    if (is_const(l.get(), 0.0)) return mgr_.unary(NodeType::Negate, r);
    if (l.get() == r.get())     return konst(0.0);
    if (r.get()->type == NodeType::Negate) return mgr_.binary(NodeType::Add, l, MathExpression(r.get()->unary.child));
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cos, i1) && is_square(r.get(), NodeType::Sin, i2) && i1 == i2)
            return mgr_.unary(NodeType::Cos, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        if (is_square(l.get(), NodeType::Sec, i1) && is_square(r.get(), NodeType::Tan, i2) && i1 == i2)
            return konst(1.0);
        if (is_square(l.get(), NodeType::Csc, i1) && is_square(r.get(), NodeType::Cot, i2) && i1 == i2)
            return konst(1.0);
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cosh, i1) && is_square(r.get(), NodeType::Sinh, i2) && i1 == i2)
            return konst(1.0);
        if (is_square(l.get(), NodeType::Coth, i1) && is_square(r.get(), NodeType::Csch, i2) && i1 == i2)
            return konst(1.0);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_const(l.get(), 1.0) && is_square(r.get(), NodeType::Tanh, inner))
            return square(NodeType::Sech, inner);
    }

    if (r.get()->type == NodeType::Negate) { return mgr_.binary(NodeType::Add, l, MathExpression(r.get()->unary.child)); }
    return mgr_.binary(NodeType::Subtract, l, r);
}

auto MathExpressionSimplifier::simplify_multiply(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        auto check_zero_inf = [&](MathExpression& inf_side, MathExpression& other) -> MathExpression {
            if (is_const(other.get(), 0.0)) return indet();
            if (is_const(other.get())) {
                double c = other.get()->constant;
                if (c > 0.0) return MathExpression(inf_side.get());
                if (c < 0.0) return negate_inf(inf_side.get());
            }
            if (is_infinite(other.get())) {
                return inf_sign(inf_side.get()) * inf_sign(other.get()) > 0 ? pos_inf() : neg_inf();
            }
            return MathExpression{};  
        };

        MathExpression result = is_infinite(l.get()) ? check_zero_inf(l, r) : check_zero_inf(r, l);
        if (result) return result;
    }
    if (is_const(l.get()) && is_const(r.get())) return konst(l.get()->constant * r.get()->constant);
    if (is_const(l.get(), 0.0) || is_const(r.get(), 0.0)) return konst(0.0);
    if (is_const(l.get(),  1.0))  return r;
    if (is_const(r.get(),  1.0))  return l;
    if (is_const(l.get(), -1.0))  return mgr_.unary(NodeType::Negate, r);
    if (is_const(r.get(), -1.0))  return mgr_.unary(NodeType::Negate, l);
    return mgr_.binary(NodeType::Multiply, l, r);
}

auto MathExpressionSimplifier::simplify_divide(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_const(r.get(), 0.0)) { return undef(); }
    if (is_infinite(r.get())) {
        if (is_infinite(l.get())) return indet();
        return konst(0.0);
    }
    if (is_infinite(l.get())) {
        if (is_const(r.get())) {
            double c = r.get()->constant;
            if (c > 0.0) return MathExpression(l.get());
            if (c < 0.0) return negate_inf(l.get());
        }
    }
    if (is_const(l.get()) && is_const(r.get()) && r.get()->constant != 0.0) return konst(l.get()->constant / r.get()->constant);
    if (is_const(l.get(), 0.0)) return konst(0.0);
    if (is_const(r.get(), 1.0)) return l;
    if (l.get() == r.get())     return konst(1.0);
    return mgr_.binary(NodeType::Divide, l, r);
}

auto MathExpressionSimplifier::simplify_sign(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return konst( 1.0);
    if (is_neg_inf(child.get())) return konst(-1.0);
        
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v > 0.0) return konst( 1.0);
        if (v < 0.0) return konst(-1.0);
        return konst(0.0);
    }
        
    if (child.get()->type == NodeType::Negate) {
        return mgr_.unary(NodeType::Negate, mgr_.unary(NodeType::Sign, MathExpression(child.get()->unary.child)));
    }

    return mgr_.unary(NodeType::Sign, child);
}

auto MathExpressionSimplifier::simplify_unit_step(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return konst(1.0);
    if (is_neg_inf(child.get())) return konst(0.0);
    if (is_const(child.get())) { return child.get()->constant >= 0.0 ? konst(1.0) : konst(0.0); }
    return mgr_.unary(NodeType::UnitStep, child);
}

auto MathExpressionSimplifier::simplify_power(MathExpressionNode* n) -> MathExpression {
    MathExpression base = simplify_node(n->power.base);
    MathExpression exp  = simplify_node(n->power.exponent);
    if (auto p = propagate_poison2(base, exp)) return p;
    if (is_const(base.get(), 0.0)) {
        if (is_const(exp.get(), 0.0) || is_const(exp.get()) && exp.get()->constant == 0.0)
            return indet();                        
        if (is_const(exp.get()) && exp.get()->constant < 0.0)
            return undef();                        
        if (is_neg_inf(exp.get()))
            return undef();                       
        if (is_pos_inf(exp.get()))
            return konst(0.0);                    
        return konst(0.0);                        
    }
    if (is_const(exp.get(), 0.0)) {
        if (is_infinite(base.get()))  return indet(); 
        return konst(1.0);                          
    }
    if (is_const(exp.get(), 1.0))    return base;    
    if (is_const(base.get(), 1.0)) {
        if (is_infinite(exp.get()))   return indet();
        return konst(1.0);                            
    }
    if (is_pos_inf(base.get())) {
        if (is_const(exp.get()) && exp.get()->constant > 0.0) return pos_inf();
        if (is_const(exp.get()) && exp.get()->constant < 0.0) return konst(0.0);
        if (is_pos_inf(exp.get())) return pos_inf();
        if (is_neg_inf(exp.get())) return konst(0.0);
    }
    if (is_const(base.get()) && is_const(exp.get())) return konst(std::pow(base.get()->constant, exp.get()->constant));
    if (is_const(exp.get(), -1.0)) return mgr_.binary(NodeType::Divide, konst(1.0), base);
    return mgr_.power(base, exp);
}

auto MathExpressionSimplifier::simplify_exp(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get()))  return konst(std::exp(child.get()->constant));
    if (is_pos_inf(child.get())) return pos_inf();  
    if (is_neg_inf(child.get())) return konst(0.0);
    if (child.get()->type == NodeType::NaturalLog) return MathExpression(child.get()->unary.child); 
    return mgr_.unary(NodeType::NaturalExp, child);
}

auto MathExpressionSimplifier::simplify_ln(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v <  0.0) return nan_node();   
        if (v == 0.0) return neg_inf(); 
        return konst(std::log(v));
    }
    if (is_neg_inf(child.get())) return nan_node();   
    if (is_pos_inf(child.get())) return pos_inf(); 
    if (is_const(child.get(), 1.0)) return konst(0.0);
    if (child.get()->type == NodeType::NaturalExp) return MathExpression(child.get()->unary.child); 
    if (child.get()->type == NodeType::Power) {
        MathExpression b(child.get()->power.base);
        MathExpression e(child.get()->power.exponent);
        return mgr_.binary(NodeType::Multiply, e, mgr_.unary(NodeType::NaturalLog, b));
    }
    return mgr_.unary(NodeType::NaturalLog, child);
}

auto MathExpressionSimplifier::simplify_log(MathExpressionNode* n) -> MathExpression {
    MathExpression arg  = simplify_node(n->binary.left);
    MathExpression base = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, base)) return p;
    if (is_const(base.get())) {
        double b = base.get()->constant;
        if (b <= 0.0) return nan_node();
        if (b == 1.0) return undef(); 
    }
    if (is_infinite(base.get()) || is_const(base.get(), 0.0)) return undef();
    if (is_const(arg.get())) {
        double a = arg.get()->constant;
        if (a <  0.0) return nan_node();
        if (a == 0.0) return neg_inf();
    }
    if (is_neg_inf(arg.get())) return nan_node();
    if (is_pos_inf(arg.get())) return pos_inf();
    if (is_const(arg.get()) && is_const(base.get())) return konst(std::log(arg.get()->constant) / std::log(base.get()->constant));
    if (is_const(arg.get(), 1.0)) return konst(0.0); 
    if (arg.get() == base.get())  return konst(1.0);
    MathExpression ln_arg  = simplify_ln(mgr_.unary(NodeType::NaturalLog, arg).get());
    MathExpression ln_base = simplify_ln(mgr_.unary(NodeType::NaturalLog, base).get());
    return mgr_.binary(NodeType::Divide, ln_arg, ln_base);
}

auto MathExpressionSimplifier::simplify_sqrt(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))      return MathExpression(child.get());
    if (is_neg_inf(child.get()))     return nan_node();   
    if (is_pos_inf(child.get()))     return pos_inf();    
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v < 0.0) return nan_node();
        if (v == 0.0) return konst(0.0);
        if (v == 1.0) return konst(1.0);
        double r = std::sqrt(v);
        if (r == std::floor(r)) return konst(r);
    }
    if (child.get()->type == NodeType::Power && is_const(child.get()->power.exponent, 2.0)) return mgr_.unary(NodeType::AbsoluteValue, MathExpression(child.get()->power.base));
    return mgr_.unary(NodeType::Sqrt, child);
}

auto MathExpressionSimplifier::simplify_cbrt(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))  return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();   
    if (is_neg_inf(child.get())) return neg_inf();  
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v == 0.0) return konst(0.0);
        if (v == 1.0) return konst(1.0);
        if (v == -1.0) return konst(-1.0);
        double r = std::cbrt(v);
        if (r == std::floor(r)) return konst(r);
    }
    if (child.get()->type == NodeType::Power && is_const(child.get()->power.exponent, 3.0)) return MathExpression(child.get()->power.base);
    return mgr_.unary(NodeType::Cbrt, child);
}

auto MathExpressionSimplifier::simplify_sinc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_infinite(child.get())) return konst(0.0);   
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v == 0.0) return konst(1.0);
        return konst(std::sin(v) / v);
    }
    return mgr_.unary(NodeType::Sinc, child);
}

auto MathExpressionSimplifier::simplify_normalsinc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_infinite(child.get())) return konst(0.0);  
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v == 0.0) return konst(1.0);               
        double pv = constants::pi() * v;
        return konst(std::sin(pv) / pv);
    }
    return mgr_.unary(NodeType::NormalSinc, child);
}

auto MathExpressionSimplifier::simplify_erf_erfc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_pos_inf(child.get()))  return n->type == NodeType::Erf ? konst(1.0)  : konst(0.0);
    if (is_neg_inf(child.get()))  return n->type == NodeType::Erf ? konst(-1.0) : konst(2.0);
    if (is_const(child.get())) {
        double v = child.get()->constant;
        return n->type == NodeType::Erf ? konst(std::erf(v)) : konst(std::erfc(v));
    }
    if (child.get()->type == NodeType::InverseErf && n->type == NodeType::Erf) { return MathExpression(child.get()->unary.child); }
    if (child.get()->type == NodeType::InverseErfc && n->type == NodeType::Erfc) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_erfi(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_pos_inf(child.get()))  return pos_inf();
    if (is_neg_inf(child.get()))  return neg_inf();
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v == 0.0) return konst(0.0);
        return konst(fizmo::math::erfi(v));
    }
    if (child.get()->type == NodeType::InverseErfi) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(NodeType::Erfi, child);
}

auto MathExpressionSimplifier::simplify_inverse_erf_family(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_infinite(child.get())) {
        if (n->type == NodeType::InverseErfi) {
            return is_pos_inf(child.get()) ? pos_inf() : neg_inf();
        } else {
            return nan_node();
        }
    }

    if (is_const(child.get())) {
        double v = child.get()->constant;

        switch (n->type) {
            case NodeType::InverseErf: {
                if (v < -1.0 || v > 1.0) return nan_node();
                if (v == -1.0) return neg_inf();
                if (v ==  1.0) return pos_inf();
                if (v ==  0.0) return konst(0.0);
                return konst(fizmo::math::inverse_erf(v));
            }

            case NodeType::InverseErfc: {
                if (v < 0.0 || v > 2.0) return nan_node();
                if (v == 0.0) return pos_inf();  
                if (v == 2.0) return neg_inf(); 
                if (v == 1.0) return konst(0.0); 
                return konst(fizmo::math::inverse_erfc(v));
            }

            case NodeType::InverseErfi: {
                if (v == 0.0) return konst(0.0);
                return konst(fizmo::math::inverse_erfi(v));
            }

            default:
                break;
        }
    }

    if (n->type == NodeType::InverseErf && child.get()->type == NodeType::Erf) { return MathExpression(child.get()->unary.child); }
    if (n->type == NodeType::InverseErfc && child.get()->type == NodeType::Erfc) { return MathExpression(child.get()->unary.child); }
    if (n->type == NodeType::InverseErfi && child.get()->type == NodeType::Erfi) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_erf_generalized(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (l.get() == r.get()) return konst(0.0);   
    if (is_const(l.get()) && is_const(r.get())) {
        double a = l.get()->constant, b = r.get()->constant;
        return n->type == NodeType::ErfGeneralized ? konst(std::erf(a)  - std::erf(b)) : konst(std::erfc(a) - std::erfc(b));
    }
    return mgr_.binary(n->type, l, r);
}

auto MathExpressionSimplifier::simplify_gamma_factorial(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (n->type == NodeType::Gamma) {
        if (is_pos_inf(child.get())) return pos_inf();
        if (is_neg_inf(child.get())) return nan_node();
        if (is_const(child.get())) {
            double v = child.get()->constant;
            if (v <= 0.0 && v == std::floor(v)) return nan_node();
            return konst(std::tgamma(v));
        }
    } else { 
        if (is_pos_inf(child.get())) return pos_inf();
        if (is_neg_inf(child.get())) return nan_node();
        if (is_const(child.get())) {
            double v = child.get()->constant;
            if (v < 0.0 && v == std::floor(v)) return nan_node();
            return konst(std::tgamma(v + 1.0));
        }
    }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_digamma_trigamma(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return n->type == NodeType::Digamma ? pos_inf() : konst(0.0);
    if (is_neg_inf(child.get())) return nan_node();
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v <= 0.0 && v == std::floor(v)) return nan_node();
        return n->type == NodeType::Digamma ? konst(digamma(v)) : konst(trigamma(v));
    }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_beta(MathExpressionNode* n) -> MathExpression {
    MathExpression lchild = simplify_node(n->binary.left);
    MathExpression rchild = simplify_node(n->binary.right);
    if (is_poison(lchild.get())) return MathExpression(lchild.get());
    if (is_poison(rchild.get())) return MathExpression(lchild.get());
    bool lc = is_const(lchild.get());
    bool rc = is_const(rchild.get());

    if (lc && rc) {
        double lv = lchild.get()->constant;
        double rv = rchild.get()->constant; 
        double lrv = lv + rv;
        if ((lv <= 0.0 && lv == std::floor(lv)) || (rv <= 0.0 && rv == std::floor(rv)) || (lrv <= 0.0 && lrv == std::floor(lrv))) return nan_node();
        return konst(beta_function(lv, rv));
    } else {
        return mgr_.binary(NodeType::BetaFunction, lchild, rchild);
    }
}

auto MathExpressionSimplifier::simplify_lambert_w(MathExpressionNode* n) -> MathExpression {
    MathExpression arg    = simplify_node(n->binary.left);
    MathExpression branch = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, branch)) return p;

    if (is_const(branch.get())) {
        double bv = branch.get()->constant;
        if (bv != std::floor(bv)) return nan_node();            
        int k = static_cast<int>(bv);

        if (is_const(arg.get(), 0.0)) {
            if (k == 0) return konst(0.0);
            return neg_inf();             
        }

        if (is_const(arg.get())) {
            double z = arg.get()->constant;
            double neg_inv_e = -constants::reciprocal_e();
            if (std::abs(z - neg_inv_e) <= constants::middle_epsilon() && (k == 0 || k == -1)) return konst(-1.0);
            if (k == 0 && std::abs(z - constants::euler()) <= constants::middle_epsilon()) return konst(1.0);
        }

        if (is_const(arg.get())) {
            EvalContext ctx;
            MathExpression tmp = mgr_.binary(NodeType::LambertW, arg, branch);
            double v = eval_node(tmp.get(), ctx);
            if (std::isfinite(v)) return konst(v);
        }
    }

    return mgr_.binary(NodeType::LambertW, arg, branch);
}

auto MathExpressionSimplifier::simplify_chebyshev(MathExpressionNode* n) -> MathExpression {
    MathExpression x = simplify_node(n->binary.left);
    MathExpression k = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, k)) return p;
    const bool is_u = n->type == NodeType::ChebyshevU;
    const bool is_t = n->type == NodeType::ChebyshevT;

    if (is_const(k.get())) {
        double m = k.get()->constant;
        if (m == 0.0) { return konst(1.0); }
        if (m == 1.0) { return is_t ? x : mgr_.binary(NodeType::Multiply, konst(2.0), x); }
    }

    if (is_const(x.get()) && is_const(k.get())) {
        EvalContext ctx;
        MathExpression tmp = mgr_.binary(is_t ? NodeType::ChebyshevT : NodeType::ChebyshevU, x, k);
        double v = eval_node(tmp.get(), ctx);
        if (!std::isnan(v) && !std::isinf(v)) return konst(v);
        if (std::isinf(v)) return v > 0 ? pos_inf() : neg_inf();
        return undef();
    }

    return mgr_.binary(NodeType::ChebyshevT, x, k);
}

auto MathExpressionSimplifier::simplify_exp_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_neg_inf(child.get())) return konst(0.0);
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v == 0.0) return neg_inf();
        double r = fizmo::math::exp_integral(v);
        if (std::isnan(r)) return nan_node();
        if (std::isinf(r)) return r > 0 ? pos_inf() : neg_inf();
        return konst(r);
    }
    return mgr_.unary(NodeType::ExponentialIntegral, child);
}

auto MathExpressionSimplifier::simplify_log_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_const(child.get())) {
        double v = child.get()->constant;
        if (v < 0.0)  return nan_node();
        if (v == 0.0) return konst(0.0);
        if (v == 1.0) return neg_inf();
        double r = fizmo::math::log_integral(v);
        if (std::isnan(r)) return nan_node();
        if (std::isinf(r)) return r > 0 ? pos_inf() : neg_inf();
        return konst(r);
    }
    return mgr_.unary(NodeType::LogarithmicIntegral, child);
}

auto MathExpressionSimplifier::simplify_exp_integral_gen(MathExpressionNode* n) -> MathExpression {
    MathExpression x  = simplify_node(n->binary.left);
    MathExpression nn = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, nn)) return p;

    // E_1(x) == -Ei(-x)
    if (is_const(nn.get(), 1.0)) {
        return simplify_node(
            mgr_.unary(
                NodeType::Negate,
                mgr_.unary(
                    NodeType::ExponentialIntegral,
                    mgr_.unary(NodeType::Negate, x)
                )
            ).get()
        );
    }

    if (is_const(x.get()) && is_const(nn.get())) {
        double r = fizmo::math::exp_integral(x.get()->constant, nn.get()->constant);
        if (std::isnan(r)) return nan_node();
        if (std::isinf(r)) return r > 0 ? pos_inf() : neg_inf();
        return konst(r);
    }

    return mgr_.binary(NodeType::ExponentialIntegralGeneralized, x, nn);
}

auto MathExpressionSimplifier::simplify_log_integral_gen(MathExpressionNode* n) -> MathExpression {
    MathExpression x  = simplify_node(n->binary.left);
    MathExpression nn = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, nn)) return p;

    if (is_const(x.get()) && is_const(nn.get())) {
        double xv = x.get()->constant;
        if (xv < 0.0) return nan_node();
        double r = fizmo::math::log_integral(xv, nn.get()->constant);
        if (std::isnan(r)) return nan_node();
        if (std::isinf(r)) return r > 0 ? pos_inf() : neg_inf();
        return konst(r);
    }
        
    return mgr_.binary(NodeType::LogarithmicIntegralGeneralized, x, nn);
}

auto MathExpressionSimplifier::simplify_bell(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;

    if (is_const(idx.get())) {
        double nv = idx.get()->constant;
        if (nv == 0.0) return konst(1.0);
        if (nv == 1.0) return arg;
    }

    return mgr_.binary(NodeType::BellPolynomial, arg, idx);
}

auto MathExpressionSimplifier::simplify_sin_cos_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_sin  = (n->type == NodeType::SinIntegral);
    bool is_cos  = (n->type == NodeType::CosIntegral);
    bool is_sinh = (n->type == NodeType::SinhIntegral);
    bool is_cosh = (n->type == NodeType::CoshIntegral);

    if (is_infinite(child.get())) {
        if (is_sin || is_sinh) { return is_pos_inf(child.get()) ? pos_inf() : neg_inf(); }
        if (is_cosh) { return is_pos_inf(child.get()) ? pos_inf() : neg_inf(); }
        return konst(constants::quiet_nan());
    }

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_sin)  return konst(sin_integral(x));
        if (is_cos)  return konst(cos_integral(x));
        if (is_sinh) return konst(sinh_integral(x));
        if (is_cosh) return konst(std::cosh(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_fresnel(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_sin  = (n->type == NodeType::FresnelS);
    bool is_cos  = (n->type == NodeType::FresnelC);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_sin)  return konst(fresnel_s(x));
        if (is_cos)  return konst(fresnel_c(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_riemann(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_rz  = (n->type == NodeType::RiemannZeta);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_rz) return konst(riemann_zeta(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_di_tri_log(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_di  = (n->type == NodeType::Dilogarithm);
    bool is_tri = (n->type == NodeType::Trilogarithm);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_di)  return konst(dilogarithm(x));
        if (is_tri) return konst(trilogarithm(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_polylog(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;

    if (is_const(idx.get()) && is_const(arg.get())) {
        double nv = idx.get()->constant;
        double xv = arg.get()->constant;
        return konst(polylog(xv, nv));
    }

    return mgr_.binary(NodeType::Polylog, arg, idx);
}

auto MathExpressionSimplifier::simplify_spence(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_f = (n->type == NodeType::SpenceFunction);
    bool is_i = (n->type == NodeType::SpenceIntegral);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_f) return konst(spence_function(x));
        if (is_i) return konst(spence_integral(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_rogers(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_l = (n->type == NodeType::RogersL);
    bool is_r = (n->type == NodeType::RogersLR);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_l) return konst(rogers_function(x));
        if (is_r) return konst(rogers_dilogarithm(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_gudermannian(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_g = (n->type == NodeType::Gudermannian);
    bool is_i = (n->type == NodeType::InverseGudermannian);

    if (is_const(child.get())) {
        double x = child.get()->constant;
        if (is_g) return konst(gudermannian(x));
        if (is_i) return konst(inverse_gudermannian(x));
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_combination(MathExpressionNode* n) -> MathExpression {
    MathExpression nn = simplify_node(n->binary.left);
    MathExpression kk = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(nn, kk)) return p;
    if (is_const(kk.get(), 0.0)) return konst(1.0);                 
    if (is_const(kk.get(), 1.0)) return nn;                         
    if (nn.get() == kk.get())    return konst(1.0);                 
    if (is_const(nn.get(), 0.0)) return konst(0.0);                 

    if (is_const(nn.get()) && is_const(kk.get())) {
        double nv = nn.get()->constant;
        double kv = kk.get()->constant;
        double num = std::tgamma(nv + 1.0);
        double den = std::tgamma(kv + 1.0) * std::tgamma(nv - kv + 1.0);
        if (den == 0.0 || std::isnan(num) || std::isnan(den)) return nan_node();
        return konst(num / den);
    }

    return mgr_.binary(NodeType::Combination, nn, kk);
}

auto MathExpressionSimplifier::simplify_permutation(MathExpressionNode* n) -> MathExpression {
    MathExpression nn = simplify_node(n->binary.left);
    MathExpression kk = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(nn, kk)) return p;
    if (is_const(kk.get(), 0.0)) return konst(1.0);                 
    if (is_const(kk.get(), 1.0)) return nn;                         
    if (nn.get() == kk.get()) { return mgr_.unary(NodeType::Factorial, nn); }
    if (is_const(nn.get(), 0.0)) return konst(0.0);                 

    if (is_const(nn.get()) && is_const(kk.get())) {
        double nv = nn.get()->constant;
        double kv = kk.get()->constant;
        double den = std::tgamma(nv - kv + 1.0);
        if (den == 0.0 || std::isnan(den)) return nan_node();
        double num = std::tgamma(nv + 1.0);
        if (std::isnan(num)) return nan_node();
        return konst(num / den);
    }
        
    return mgr_.binary(NodeType::Permutation, nn, kk);
}

auto MathExpressionSimplifier::simplify_fib_lucas_sequence(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());

    if (is_const(child.get())) {
        double v = child.get()->constant;
        return konst(n->type == NodeType::FibonacciSequence ? fibonacci(v) : lucas(v));
    }
        
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_fib_lucas_poly(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;

    if (is_const(idx.get()) && is_const(arg.get())) {
        bool is_fib = n->type == NodeType::FibonacciPolynomial;
        double nv = idx.get()->constant;
        double xv = arg.get()->constant;
        return konst(is_fib ? fibonacci_polynomial(xv, nv) : lucas_polynomial(xv, nv));
    }

    return mgr_.binary(n->type, arg, idx);
}

auto MathExpressionSimplifier::simplify_applied(MathExpressionNode* n) -> MathExpression {
    const std::uint64_t cnt = n->applied.arg_count;
    std::vector<MathExpression> args; args.reserve(cnt);
    std::vector<std::uint64_t>  ords; ords.reserve(cnt);

    for (std::uint64_t i = 0; i < cnt; ++i) {
        MathExpression a = simplify_node(n->applied.args[i]);
        if (is_poison(a.get())) return MathExpression(a.get());
        args.push_back(a);
        ords.push_back(n->applied.orders[i]);
    }
        
    return mgr_.applied(n->applied.func_id, args, ords);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
