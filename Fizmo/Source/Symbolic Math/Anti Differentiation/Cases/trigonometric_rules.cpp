#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {
namespace detail_trig {

bool is_trig_squared(MathExpressionNode* n, NodeType fn, std::uint64_t vid, double& a_out, double& b_out, MathExpressionNode*& inner_out) {
    if (n->type != NodeType::Power) return false;
    if (n->power.exponent->type != NodeType::Constant || n->power.exponent->constant != 2.0) return false;
    if (n->power.base->type != fn) return false;
    inner_out = n->power.base->unary.child;
    return is_linear_in_var(inner_out, vid, a_out, b_out) || is_var(inner_out, vid);
}

} // namespace detail_trig
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

MathExpression try_trigonometric(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& /*simp*/,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ad = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Add,      a, b); };
    auto sb = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Subtract, a, b); };
    auto ng = [&](MathExpression a)                     { return mgr.unary(NodeType::Negate, a); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ab = [&](MathExpression a)                     { return mgr.unary(NodeType::AbsoluteValue, a); };
    auto un = [&](NodeType t, MathExpression a)         { return mgr.unary(t, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };

    using namespace detail_trig;

    auto div_a = [&](MathExpression result, double a) -> MathExpression {
        return (a == 1.0) ? result : dv(result, K(a));
    };

    auto try_unary_trig = [&](NodeType fn, MathExpressionNode* inner) -> MathExpression {
        double a, b;
        if (!linear_or_var(inner, vid, a, b)) return {};
        MathExpression u = W(inner);

        switch (fn) {
        case NodeType::Sin:  return div_a(ng(un(NodeType::Cos, u)), a);               
        case NodeType::Cos:  return div_a(un(NodeType::Sin, u), a);                    
        case NodeType::Tan:  return div_a(ng(ln(ab(un(NodeType::Cos, u)))), a);        
        case NodeType::Cot:  return div_a(ln(ab(un(NodeType::Sin, u))), a);           
        case NodeType::Sec:
            return div_a(ln(ab(ad(un(NodeType::Sec, u), un(NodeType::Tan, u)))), a);   
        case NodeType::Csc:
            return div_a(ng(ln(ab(ad(un(NodeType::Csc, u), un(NodeType::Cot, u))))), a); 
        default: return {};
        }
    };

    if (NodeKeyHash::is_unary(n->type)) {
        auto result = try_unary_trig(n->type, n->unary.child);
        if (result) return result;
    }

    {
        double a, b;
        MathExpressionNode* inner = nullptr;
        if (detail_trig::is_trig_squared(n, NodeType::Sec, vid, a, b, inner)) {
            if (linear_or_var(inner, vid, a, b)) return div_a(un(NodeType::Tan, W(inner)), a);                
        }
        if (detail_trig::is_trig_squared(n, NodeType::Csc, vid, a, b, inner)) {
            if (linear_or_var(inner, vid, a, b)) return div_a(ng(un(NodeType::Cot, W(inner))), a);            
        }
    }

    if (n->type == NodeType::Multiply) {
        auto check_sec_tan = [&](MathExpressionNode* l, MathExpressionNode* r) -> MathExpression {
            if (l->type == NodeType::Sec && r->type == NodeType::Tan && same(l->unary.child, r->unary.child)) {
                double a, b;
                if (linear_or_var(l->unary.child, vid, a, b)) return div_a(un(NodeType::Sec, W(l->unary.child)), a);
            }
            if (l->type == NodeType::Csc && r->type == NodeType::Cot && same(l->unary.child, r->unary.child)) {
                double a, b;
                if (linear_or_var(l->unary.child, vid, a, b)) return div_a(ng(un(NodeType::Csc, W(l->unary.child))), a);
            }
            return {};
        };

        auto result = check_sec_tan(n->binary.left, n->binary.right);
        if (result) return result;
        result = check_sec_tan(n->binary.right, n->binary.left); 
        if (result) return result;
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
