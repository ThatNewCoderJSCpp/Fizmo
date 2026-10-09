#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

MathExpression try_exponential(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& /*simp*/,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto pw = [&](MathExpression b, MathExpression e)   { return mgr.power(b, e); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ex = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalExp, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };
    auto vx = [&]()                                     { return mgr.variable_by_id(vid); };

    if (n->type == NodeType::NaturalExp) {
        double a, b;
        if (is_var(n->unary.child, vid)) return W(n); 
        if (is_linear_in_var(n->unary.child, vid, a, b)) return dv(W(n), K(a));   
        return {};
    }
    if (n->type == NodeType::Power && is_free(n->power.base, vid) && !is_free(n->power.exponent, vid)) {
        double a, b_coeff;
        MathExpression base = W(n->power.base);
        if (is_var(n->power.exponent, vid)) return dv(W(n), ln(base));
        if (is_linear_in_var(n->power.exponent, vid, a, b_coeff)) return dv(W(n), ml(K(a), ln(base)));
        return {};
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
