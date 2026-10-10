#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

MathExpression try_polynomial(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto pw = [&](MathExpression b, MathExpression e)   { return mgr.power(b, e); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ad = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Add,      a, b); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ab = [&](MathExpression a)                     { return mgr.unary(NodeType::AbsoluteValue, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };
    auto vx = [&]()                                     { return mgr.variable_by_id(vid); };

    if (n->type == NodeType::Power && is_var(n->power.base, vid) && is_free(n->power.exponent, vid)) {
        if (n->power.exponent->type == NodeType::Constant) {
            double exp = n->power.exponent->constant;
            if (exp == -1.0) return ln(ab(vx()));                             
            return dv(pw(vx(), K(exp + 1.0)), K(exp + 1.0));                 
        }
        MathExpression new_exp = ad(W(n->power.exponent), K(1.0));
        return dv(pw(vx(), new_exp), new_exp);
    }
    if (n->type == NodeType::Power && is_free(n->power.exponent, vid)) {
        double a, b;
        if (is_linear_in_var(n->power.base, vid, a, b)) {
            MathExpression inner = W(n->power.base);
            if (n->power.exponent->type == NodeType::Constant) {
                double exp = n->power.exponent->constant;
                if (exp == -1.0) return dv(ln(ab(inner)), K(a));              
                double np1 = exp + 1.0;
                return dv(pw(inner, K(np1)), K(a * np1));                    
            }
            MathExpression e = W(n->power.exponent);
            MathExpression np1 = ad(e, K(1.0));
            return dv(pw(inner, np1), ml(K(a), np1));
        }
    }
    if (n->type == NodeType::Sqrt) {
        double a, b;
        if (is_var(n->unary.child, vid)) { return ml(K(2.0 / 3.0), pw(vx(), K(1.5))); }
        if (is_linear_in_var(n->unary.child, vid, a, b)) {
            MathExpression inner = W(n->unary.child);
            return dv(ml(K(2.0 / 3.0), pw(inner, K(1.5))), K(a));
        }
    }
    if (n->type == NodeType::Cbrt) {
        double a, b;
        if (is_var(n->unary.child, vid)) { return ml(K(3.0 / 4.0), pw(vx(), K(4.0 / 3.0))); }
        if (is_linear_in_var(n->unary.child, vid, a, b)) {
            MathExpression inner = W(n->unary.child);
            return dv(ml(K(3.0 / 4.0), pw(inner, K(4.0 / 3.0))), K(a));
        }
    }
    if (n->type == NodeType::Divide && is_free(n->binary.left, vid)) {
        double a, b;
        if (is_linear_in_var(n->binary.right, vid, a, b)) {
            MathExpression inner = W(n->binary.right);
            return dv(ml(W(n->binary.left), ln(ab(inner))), K(a));
        }
    }
    if (n->type == NodeType::Divide && 
        is_free(n->binary.left, vid) && 
        n->binary.right->type == NodeType::Power && 
        is_free(n->binary.right->power.exponent, vid)
    ) {
        double a, b;
        if (is_linear_in_var(n->binary.right->power.base, vid, a, b) && n->binary.right->power.exponent->type == NodeType::Constant) {
            double exp = n->binary.right->power.exponent->constant;
            MathExpression inner = W(n->binary.right->power.base);
            MathExpression coeff = W(n->binary.left);
            if (exp == 1.0) { return dv(ml(coeff, ln(ab(inner))), K(a)); }
            double new_exp = -exp + 1.0;
            if (new_exp == 0.0) return dv(ml(coeff, ln(ab(inner))), K(a));
            return dv(ml(coeff, pw(inner, K(new_exp))), K(a * new_exp));
        }
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
