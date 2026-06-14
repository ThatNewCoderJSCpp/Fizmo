#ifndef FIZMO_MATH_INTEGRATOR_CLASS_HPP
#define FIZMO_MATH_INTEGRATOR_CLASS_HPP

#include "../CAS/expression.hpp"
#include "../CAS/simplifier.hpp"
#include "../CAS/differentiator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

inline bool is_free(const MathExpressionNode* n, std::uint64_t vid) {
    if (!n) return true;
    switch (n->type) {
    case NodeType::Variable:          return n->variable.var_id != vid;
    case NodeType::Constant:
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

inline bool is_var(const MathExpressionNode* n, std::uint64_t vid) { return n && n->type == NodeType::Variable && n->variable.var_id == vid; }

inline bool match_linear(MathExpressionNode* n, std::uint64_t vid, double& a_out, double& b_out) {
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

inline bool is_linear_in_var(MathExpressionNode* n, std::uint64_t vid, double& a, double& b) {
    if (is_free(n, vid)) return false;
    return match_linear(n, vid, a, b);
}

inline bool same(const MathExpressionNode* a, const MathExpressionNode* b) { return a == b; }

MathExpression try_polynomial     (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_exponential    (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_logarithmic    (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_trigonometric  (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_hyperbolic     (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_algebraic_forms(MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);
MathExpression try_heuristic      (MathExpressionManager&, MathExpressionSimplifier&, MathExpressionDifferentiator&, MathExpressionNode*, std::uint64_t);

} // namespace integration

class MathExpressionIntegrator {
public:
    MathExpressionIntegrator(MathExpressionManager& mgr, VariableTable& vars, MathExpressionSimplifier& simp, MathExpressionDifferentiator& diff) : mgr_(mgr), vars_(vars), simp_(simp), diff_(diff) {}

    MathExpression integrate(MathExpression expr, const std::string& var_name) {
        std::uint64_t vid = vars_.get(var_name);
        if (vid == VariableTable::invalid_id) { return mgr_.binary(NodeType::Multiply, expr, mgr_.variable(var_name)); }
        MathExpression result = integrate_node(expr.get(), vid);
        if (result) result = simp_.simplify(result);
        return result;
    }

private:
    MathExpressionManager&       mgr_;
    VariableTable&                vars_;
    MathExpressionSimplifier&    simp_;
    MathExpressionDifferentiator& diff_;
    MathExpression K(double v)                              { return mgr_.constant(v); }
    MathExpression var_expr(std::uint64_t vid)              { return mgr_.variable_by_id(vid); }
    MathExpression neg(MathExpression a)                    { return mgr_.unary(NodeType::Negate, a); }
    MathExpression add(MathExpression a, MathExpression b)  { return mgr_.binary(NodeType::Add, a, b); }
    MathExpression sub(MathExpression a, MathExpression b)  { return mgr_.binary(NodeType::Subtract, a, b); }
    MathExpression mul(MathExpression a, MathExpression b)  { return mgr_.binary(NodeType::Multiply, a, b); }
    MathExpression divn(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Divide, a, b); }
    MathExpression pw(MathExpression b, MathExpression e)   { return mgr_.power(b, e); }
    MathExpression un(NodeType t, MathExpression a)         { return mgr_.unary(t, a); }
    MathExpression W(MathExpressionNode* n)                 { return MathExpression(n); }

    MathExpression integrate_node(MathExpressionNode* n, std::uint64_t vid) {
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
        return {};
    }
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_CLASS_HPP