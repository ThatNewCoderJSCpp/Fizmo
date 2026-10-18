#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

bool is_free(const MathExpressionNode* n, std::uint64_t vid) {
    if (!n) return true;
    switch (n->type) {
    case NodeType::Variable:          return n->variable.var_id != vid;
    case NodeType::Constant:
    case NodeType::Pi:
    case NodeType::EulerNumber:
    case NodeType::PositiveInfinity:
    case NodeType::NegativeInfinity:
    case NodeType::NaN:
    case NodeType::Undefined:
    case NodeType::Indeterminate:     return true;
    case NodeType::Power:
        return is_free(n->power.base, vid) && is_free(n->power.exponent, vid);
    default:
        if (NodeKeyHash::is_unary(n->type))  return is_free(n->unary.child, vid);
        if (NodeKeyHash::is_binary(n->type)) return is_free(n->binary.left, vid) && is_free(n->binary.right, vid);
        return false;
    }
}

bool match_linear(MathExpressionNode* n, std::uint64_t vid, double& a_out, double& b_out) {
    if (!n) return false;
    if (is_var(n, vid)) { a_out = 1.0; b_out = 0.0; return true; }
    if (n->type == NodeType::Negate) {
        if (match_linear(n->unary.child, vid, a_out, b_out)) { a_out = -a_out; b_out = -b_out; return true; }
        return false;
    }
    if (n->type == NodeType::Multiply) {
        if (n->binary.left->type == NodeType::Constant && is_var(n->binary.right, vid)) { a_out = n->binary.left->constant; b_out = 0.0; return true; }
        if (n->binary.right->type == NodeType::Constant && is_var(n->binary.left, vid)) { a_out = n->binary.right->constant; b_out = 0.0; return true; }
        return false;
    }
    if (n->type == NodeType::Add || n->type == NodeType::Subtract) {
        double la, lb;
        bool l_lin = match_linear(n->binary.left,  vid, la, lb);
        bool l_free = is_free(n->binary.left, vid);
        bool r_lin = match_linear(n->binary.right, vid, a_out, b_out);
        bool r_free = is_free(n->binary.right, vid);
        if (l_lin && r_free) {
            a_out = la;
            double rval = (n->binary.right->type == NodeType::Constant) ? n->binary.right->constant : 0.0;
            if (n->binary.right->type != NodeType::Constant) return false; // only handle numeric const
            b_out = (n->type == NodeType::Add) ? lb + rval : lb - rval;
            return true;
        }
        if (r_lin && l_free) {
            if (n->binary.left->type != NodeType::Constant) return false;
            double lval = n->binary.left->constant;
            if (n->type == NodeType::Add) {
                b_out += lval;
            } else {
                a_out = -a_out; b_out = lval - b_out;
            }
            return true;
        }
        return false;
    }
    return false;
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

auto MathExpressionIntegrator::integrate(MathExpression expr, const std::string& var_name) -> MathExpression {
    std::uint64_t vid = vars_.get(var_name);
    if (vid == VariableTable::invalid_id) { return mgr_.binary(NodeType::Multiply, expr, mgr_.variable(var_name)); }
    MathExpression result = integrate_node(expr.get(), vid);
    if (result) result = simp_.simplify(result);
    return result;
}

auto MathExpressionIntegrator::integrate_node(MathExpressionNode* n, std::uint64_t vid) -> MathExpression {
    using namespace integration;
    if (is_free(n, vid)) return mul(W(n), var_expr(vid));
    if (is_var(n, vid)) return divn(pw(var_expr(vid), K(2.0)), K(2.0));
    if (n->type == NodeType::Add || n->type == NodeType::Subtract) {
        MathExpression lhs = integrate_node(n->binary.left,  vid);
        MathExpression rhs = integrate_node(n->binary.right, vid);
        if (!lhs || !rhs) return {};
        return (n->type == NodeType::Add) ? add(lhs, rhs) : sub(lhs, rhs);
    }
    if (n->type == NodeType::Negate) {
        MathExpression inner = integrate_node(n->unary.child, vid);
        return inner ? neg(inner) : MathExpression{};
    }
    if (n->type == NodeType::Multiply) {
        if (is_free(n->binary.left, vid)) {
            MathExpression inner = integrate_node(n->binary.right, vid);
            return inner ? mul(W(n->binary.left), inner) : MathExpression{};
        }
        if (is_free(n->binary.right, vid)) {
            MathExpression inner = integrate_node(n->binary.left, vid);
            return inner ? mul(inner, W(n->binary.right)) : MathExpression{};
        }
    }
    if (n->type == NodeType::Divide && is_free(n->binary.right, vid)) {
        MathExpression inner = integrate_node(n->binary.left, vid);
        return inner ? divn(inner, W(n->binary.right)) : MathExpression{};
    }
    MathExpression result;
    result = try_polynomial(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_exponential(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_logarithmic(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_trigonometric(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_hyperbolic(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_algebraic_forms(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    result = try_heuristic(mgr_, simp_, diff_, n, vid);
    if (result) return result;
    return integrate_linear_substitution(n, vid);
}

bool MathExpressionIntegrator::linear_parts(MathExpressionNode* n, std::uint64_t vid, MathExpression& a, MathExpression& b) {
    using namespace integration;
    if (is_var(n, vid)) { a = K(1.0); b = K(0.0); return true; }

    switch (n->type) {
        case NodeType::Negate:
            if (!linear_parts(n->unary.child, vid, a, b)) return false;
            a = neg(a);
            b = neg(b);
            return true;

        case NodeType::Multiply: {
            MathExpressionNode* c = nullptr;
            MathExpressionNode* u = nullptr;
            if (is_free(n->binary.left, vid))       { c = n->binary.left;  u = n->binary.right; }
            else if (is_free(n->binary.right, vid)) { c = n->binary.right; u = n->binary.left;  }
            else return false;
            if (!linear_parts(u, vid, a, b)) return false;
            a = mul(W(c), a);
            b = mul(W(c), b);
            return true;
        }

        case NodeType::Divide: {
            if (!is_free(n->binary.right, vid)) return false;
            if (!linear_parts(n->binary.left, vid, a, b)) return false;
            a = divn(a, W(n->binary.right));
            b = divn(b, W(n->binary.right));
            return true;
        }

        case NodeType::Add:
        case NodeType::Subtract: {
            const bool add_op = n->type == NodeType::Add;
            MathExpression la, lb, ra, rb;
            const bool lfree = is_free(n->binary.left, vid);
            const bool rfree = is_free(n->binary.right, vid);
            if (lfree) { la = K(0.0); lb = W(n->binary.left); }
            else if (!linear_parts(n->binary.left, vid, la, lb)) return false;
            if (rfree) { ra = K(0.0); rb = W(n->binary.right); }
            else if (!linear_parts(n->binary.right, vid, ra, rb)) return false;
            a = add_op ? add(la, ra) : sub(la, ra);
            b = add_op ? add(lb, rb) : sub(lb, rb);
            return true;
        }

        default:
            return false;
    }
}

auto MathExpressionIntegrator::integrate_linear_substitution(MathExpressionNode* n, std::uint64_t vid) -> MathExpression {
    using namespace integration;
    MathExpressionNode* inner = nullptr;
    MathExpression plain;
    const MathExpression x = var_expr(vid);

    if (NodeKeyHash::is_unary(n->type)) {
        inner = n->unary.child;
        plain = un(n->type, x);
    } else if (n->type == NodeType::Power && is_free(n->power.exponent, vid)) {
        inner = n->power.base;
        plain = pw(x, W(n->power.exponent));
    } else if (n->type == NodeType::Power && is_free(n->power.base, vid)) {
        inner = n->power.exponent;
        plain = pw(W(n->power.base), x);
    } else {
        return {};
    }

    if (is_var(inner, vid)) return {};
    MathExpression a, b;
    if (!linear_parts(inner, vid, a, b)) return {};
    a = simp_.simplify(a);
    if (a.get()->type == NodeType::Constant && a.get()->exact && a.get()->exact->is_zero()) return {};
    MathExpression antiderivative = integrate_node(plain.get(), vid);
    if (!antiderivative) return {};
    MathExpressionSubstitutor subst(mgr_, vars_, simp_);
    MathExpression back = subst.substitute(antiderivative, vars_.name(vid), MathExpression(inner));
    return divn(back, a);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
