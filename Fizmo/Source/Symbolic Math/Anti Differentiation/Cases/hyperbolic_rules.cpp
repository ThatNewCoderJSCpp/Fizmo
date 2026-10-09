#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {
namespace detail_hyp {

bool is_hyp_squared(MathExpressionNode* n, NodeType fn, std::uint64_t vid, double& a_out, double& b_out, MathExpressionNode*& inner_out) {
    if (n->type != NodeType::Power) return false;
    if (n->power.exponent->type != NodeType::Constant || n->power.exponent->constant != 2.0) return false;
    if (n->power.base->type != fn) return false;
    inner_out = n->power.base->unary.child;
    return linear_or_var(inner_out, vid, a_out, b_out);
}

} // namespace detail_hyp
} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

MathExpression try_hyperbolic(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& /*simp*/,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ng = [&](MathExpression a)                     { return mgr.unary(NodeType::Negate, a); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ab = [&](MathExpression a)                     { return mgr.unary(NodeType::AbsoluteValue, a); };
    auto un = [&](NodeType t, MathExpression a)         { return mgr.unary(t, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };

    using namespace detail_hyp;

    auto div_a = [&](MathExpression result, double a) -> MathExpression {
        return (a == 1.0) ? result : dv(result, K(a));
    };
    
    auto try_unary_hyp = [&](NodeType fn, MathExpressionNode* inner) -> MathExpression {
        double a, b;
        if (!linear_or_var(inner, vid, a, b)) return {};
        MathExpression u = W(inner);

        switch (fn) {
        case NodeType::Sinh:  return div_a(un(NodeType::Cosh, u), a);                            
        case NodeType::Cosh:  return div_a(un(NodeType::Sinh, u), a);                            
        case NodeType::Tanh:  return div_a(ln(ab(un(NodeType::Cosh, u))), a);                    
        case NodeType::Coth:  return div_a(ln(ab(un(NodeType::Sinh, u))), a);                    
        case NodeType::Sech:  return div_a(un(NodeType::Arctan, un(NodeType::Sinh, u)), a);      
        case NodeType::Csch:
            return div_a(ln(ab(un(NodeType::Tanh, dv(u, K(2.0))))), a);                         
        default: return {};
        }
    };

    if (NodeKeyHash::is_unary(n->type)) {
        auto result = try_unary_hyp(n->type, n->unary.child);
        if (result) return result;
    }

    {
        double a, b;
        MathExpressionNode* inner = nullptr;
        if (detail_hyp::is_hyp_squared(n, NodeType::Sech, vid, a, b, inner)) return div_a(un(NodeType::Tanh, W(inner)), a);                                      
        if (detail_hyp::is_hyp_squared(n, NodeType::Csch, vid, a, b, inner)) return div_a(ng(un(NodeType::Coth, W(inner))), a);                                  
    }

    if (n->type == NodeType::Multiply) {
        auto check = [&](MathExpressionNode* l, MathExpressionNode* r) -> MathExpression {
            if (l->type == NodeType::Sech && r->type == NodeType::Tanh && same(l->unary.child, r->unary.child)) {
                double a, b;
                if (linear_or_var(l->unary.child, vid, a, b)) return div_a(ng(un(NodeType::Sech, W(l->unary.child))), a);
            }
            if (l->type == NodeType::Csch && r->type == NodeType::Coth && same(l->unary.child, r->unary.child)) {
                double a, b;
                if (linear_or_var(l->unary.child, vid, a, b)) return div_a(ng(un(NodeType::Csch, W(l->unary.child))), a);
            }
            return {};
        };

        auto result = check(n->binary.left, n->binary.right);
        if (result) return result;
        result = check(n->binary.right, n->binary.left);
        if (result) return result;
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
