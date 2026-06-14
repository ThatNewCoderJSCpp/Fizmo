#ifndef FIZMO_MATH_INTEGRATION_LOGARITHMIC_RULES_HPP
#define FIZMO_MATH_INTEGRATION_LOGARITHMIC_RULES_HPP

#include "../integrator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

inline MathExpression try_logarithmic(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& /*simp*/,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto sb = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Subtract, a, b); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };
    auto vx = [&]()                                     { return mgr.variable_by_id(vid); };

    if (n->type == NodeType::NaturalLog) {
        double a, b;
        if (is_var(n->unary.child, vid)) return sb(ml(vx(), W(n)), vx());
        if (is_linear_in_var(n->unary.child, vid, a, b)) {
            MathExpression inner = W(n->unary.child);
            MathExpression body  = ml(inner, sb(ln(inner), K(1.0)));
            return dv(body, K(a));
        }
        return {};
    }
    if (n->type == NodeType::Log && is_free(n->binary.right, vid)) {
        double a, b;
        MathExpression base_expr = W(n->binary.right);
        MathExpression ln_base   = ln(base_expr);
        if (is_var(n->binary.left, vid)) { return dv(sb(ml(vx(), ln(vx())), vx()), ln_base); }
        if (is_linear_in_var(n->binary.left, vid, a, b)) {
            MathExpression inner = W(n->binary.left);
            MathExpression body  = ml(inner, sb(ln(inner), K(1.0)));
            return dv(body, ml(K(a), ln_base));
        }
        return {};
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_LOGARITHMIC_RULES_HPP