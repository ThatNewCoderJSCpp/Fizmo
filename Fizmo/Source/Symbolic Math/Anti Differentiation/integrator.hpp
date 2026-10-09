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

 bool is_free(const MathExpressionNode* n, std::uint64_t vid);

inline bool is_var(const MathExpressionNode* n, std::uint64_t vid) { return n && n->type == NodeType::Variable && n->variable.var_id == vid; }

 bool match_linear(MathExpressionNode* n, std::uint64_t vid, double& a_out, double& b_out);

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

    MathExpression integrate(MathExpression expr, const std::string& var_name);

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

    MathExpression integrate_node(MathExpressionNode* n, std::uint64_t vid);
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATOR_CLASS_HPP