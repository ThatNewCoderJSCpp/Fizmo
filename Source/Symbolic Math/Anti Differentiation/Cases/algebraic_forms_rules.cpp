#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

bool extract_var_squared(MathExpressionNode* n, std::uint64_t vid, double& coeff) {
    if (n->type == NodeType::Power && is_var(n->power.base, vid)
        && n->power.exponent->type == NodeType::Constant
        && std::abs(n->power.exponent->constant - 2.0) <= constants::middle_epsilon()
    ) {
        coeff = 1.0;
        return true;
    }
    if (n->type == NodeType::Multiply) {
        double inner;
        if (n->binary.left->type == NodeType::Constant && extract_var_squared(n->binary.right, vid, inner)) {
            coeff = n->binary.left->constant * inner;
            return true;
        }
        if (n->binary.right->type == NodeType::Constant && extract_var_squared(n->binary.left, vid, inner)) {
            coeff = n->binary.right->constant * inner;
            return true;
        }
    }
    if (n->type == NodeType::Negate) {
        double inner;
        if (extract_var_squared(n->unary.child, vid, inner)) {
            coeff = -inner;
            return true;
        }
    }
    return false;
}

bool match_ax2_plus_b(MathExpressionNode* n, std::uint64_t vid, QuadraticDecomp& q) {
    double c;
    if (extract_var_squared(n, vid, c)) {
        q.x2_coeff = c; q.free_node = nullptr; q.free_val = 0.0;
        return true;
    }
    if (n->type == NodeType::Add) {
        if (extract_var_squared(n->binary.left, vid, c) && is_free(n->binary.right, vid)) {
            q.x2_coeff = c;
            q.free_node = n->binary.right;
            q.free_val  = (n->binary.right->type == NodeType::Constant) ? n->binary.right->constant : std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (extract_var_squared(n->binary.right, vid, c) && is_free(n->binary.left, vid)) {
            q.x2_coeff = c;
            q.free_node = n->binary.left;
            q.free_val  = (n->binary.left->type == NodeType::Constant) ? n->binary.left->constant : std::numeric_limits<double>::quiet_NaN();
            return true;
        }
    }

    if (n->type == NodeType::Subtract) {
        if (extract_var_squared(n->binary.left, vid, c) && is_free(n->binary.right, vid)) {
            q.x2_coeff = c;
            q.free_node = n->binary.right;
            q.free_val  = (n->binary.right->type == NodeType::Constant) ? -n->binary.right->constant : std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        if (is_free(n->binary.left, vid) && extract_var_squared(n->binary.right, vid, c)) {
            q.x2_coeff = -c;
            q.free_node = n->binary.left;
            q.free_val  = (n->binary.left->type == NodeType::Constant) ? n->binary.left->constant : std::numeric_limits<double>::quiet_NaN();
            return true;
        }
    }
    return false;
}

MathExpression symbolic_sqrt_free(MathExpressionManager& mgr, MathExpressionNode* n) {
    if (!n) return mgr.constant(0.0);
    if (n->type == NodeType::Constant && n->constant > 0.0) return mgr.constant(std::sqrt(n->constant));
    if (n->type == NodeType::Power
        && n->power.exponent->type == NodeType::Constant
        && std::abs(n->power.exponent->constant - 2.0) < 1e-13
    ) {
        return mgr.unary(NodeType::AbsoluteValue, MathExpression(n->power.base));
    }
    if (n->type == NodeType::Multiply
        && n->binary.left->type == NodeType::Constant && n->binary.left->constant > 0.0
        && n->binary.right->type == NodeType::Power
        && n->binary.right->power.exponent->type == NodeType::Constant
        && std::abs(n->binary.right->power.exponent->constant - 2.0) < 1e-13
    ) {
        MathExpression sc = mgr.constant(std::sqrt(n->binary.left->constant));
        MathExpression sa = mgr.unary(NodeType::AbsoluteValue, MathExpression(n->binary.right->power.base));
        return mgr.binary(NodeType::Multiply, sc, sa);
    }
    return mgr.unary(NodeType::Sqrt, MathExpression(n));
}

int classify_quadratic(const QuadraticDecomp& q) {
    double A = q.x2_coeff;
    bool B_positive = false, B_negative = false;
    if (!std::isnan(q.free_val)) {
        B_positive = q.free_val > 0.0;
        B_negative = q.free_val < 0.0;
    } else if (q.free_node) {
        auto* fn = q.free_node;
        if (fn->type == NodeType::Power && fn->power.exponent->type == NodeType::Constant
            && std::abs(fn->power.exponent->constant - 2.0) < 1e-13) {
            B_positive = true;
        } else if (fn->type == NodeType::Multiply
                   && fn->binary.left->type == NodeType::Constant && fn->binary.left->constant > 0.0
                   && fn->binary.right->type == NodeType::Power
                   && fn->binary.right->power.exponent->type == NodeType::Constant
                   && std::abs(fn->binary.right->power.exponent->constant - 2.0) < 1e-13) {
            B_positive = true;
        }
    }
    if (A > 0.0 && B_positive)  return +1; // a^2 + x^2
    if (A > 0.0 && B_negative)  return  0; // x^2 - a^2
    if (A < 0.0 && B_positive)  return -1; // a^2 - x^2
    return -2; // unknown / unhandled
}

double quadratic_ratio(const QuadraticDecomp& q) {
    if (std::isnan(q.free_val)) return std::numeric_limits<double>::quiet_NaN();
    return std::abs(q.free_val / q.x2_coeff);
}

bool match_x_times_sqrt_quad(MathExpressionNode* n, std::uint64_t vid, MathExpressionNode*& sqrt_inner) {
    if (n->type != NodeType::Multiply) return false;
    auto check = [&](MathExpressionNode* maybe_x, MathExpressionNode* maybe_sqrt) -> bool {
        if (!is_var(maybe_x, vid)) return false;
        if (maybe_sqrt->type == NodeType::Sqrt) {
            sqrt_inner = maybe_sqrt->unary.child;
            return true;
        }
        if (maybe_sqrt->type == NodeType::Power
            && maybe_sqrt->power.exponent->type == NodeType::Constant
            && std::abs(maybe_sqrt->power.exponent->constant - 0.5) < 1e-13) {
            sqrt_inner = maybe_sqrt->power.base;
            return true;
        }
        return false;
    };
    return check(n->binary.left, n->binary.right) || check(n->binary.right, n->binary.left);
}

MathExpression try_algebraic_forms(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K   = [&](double v)                             { return mgr.constant(v); };
    auto pw  = [&](MathExpression b, MathExpression e)   { return mgr.power(b, e); };
    auto ml  = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv  = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ad  = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Add,      a, b); };
    auto sb  = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Subtract, a, b); };
    auto ln  = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ab  = [&](MathExpression a)                     { return mgr.unary(NodeType::AbsoluteValue, a); };
    auto sq  = [&](MathExpression a)                     { return mgr.unary(NodeType::Sqrt, a); };
    auto ng  = [&](MathExpression a)                     { return mgr.unary(NodeType::Negate, a); };
    auto atn = [&](MathExpression a)                     { return mgr.unary(NodeType::Arctan, a); };
    auto asn = [&](MathExpression a)                     { return mgr.unary(NodeType::Arcsin, a); };
    auto ash = [&](MathExpression a)                     { return mgr.unary(NodeType::Arcsinh, a); };
    auto ach = [&](MathExpression a)                     { return mgr.unary(NodeType::Arccosh, a); };
    auto asc = [&](MathExpression a)                     { return mgr.unary(NodeType::Arcsec, a); };
    auto W   = [](MathExpressionNode* p)                 { return MathExpression(p); };
    auto vx  = [&]()                                     { return mgr.variable_by_id(vid); };

    auto build_a = [&](const QuadraticDecomp& q) -> MathExpression {
        double absA = std::abs(q.x2_coeff);
        if (!std::isnan(q.free_val)) {
            double r = std::abs(q.free_val) / absA;
            return K(std::sqrt(r));
        }
        MathExpression s = symbolic_sqrt_free(mgr, q.free_node);
        if (std::abs(absA - 1.0) < 1e-13) return s;
        return dv(s, K(std::sqrt(absA)));
    };

    auto sqrt_absA = [&](const QuadraticDecomp& q) -> MathExpression { return K(std::sqrt(std::abs(q.x2_coeff))); };

    if (n->type == NodeType::Divide && is_free(n->binary.left, vid)) {
        MathExpressionNode* sqrt_inner = nullptr;
        if (match_x_times_sqrt_quad(n->binary.right, vid, sqrt_inner)) {
            QuadraticDecomp q;
            if (match_ax2_plus_b(sqrt_inner, vid, q)) {
                int cls = classify_quadratic(q);
                if (cls == 0) {
                    MathExpression a_val  = build_a(q);
                    MathExpression coeff  = W(n->binary.left);
                    MathExpression result = ml(coeff, dv(asc(dv(ab(vx()), a_val)), a_val));
                    if (std::abs(q.x2_coeff - 1.0) > 1e-13) result = dv(result, sqrt_absA(q));
                    return result;
                }
            }
        }
    }

    auto try_inv_sqrt_quad = [&](MathExpressionNode* numer, MathExpressionNode* quad_inner) -> MathExpression {
        QuadraticDecomp q;
        if (!match_ax2_plus_b(quad_inner, vid, q)) return {};
        int cls = classify_quadratic(q);
        MathExpression a_val = build_a(q);
        MathExpression coeff = W(numer);
        MathExpression sA    = sqrt_absA(q);

        if (cls == -1) {
            MathExpression result = ml(coeff, asn(dv(vx(), a_val)));
            if (std::abs(std::abs(q.x2_coeff) - 1.0) > 1e-13) result = dv(result, sA);
            return result;
        }
        if (cls == +1) {
            MathExpression result = ml(coeff, ash(dv(vx(), a_val)));
            if (std::abs(q.x2_coeff - 1.0) > 1e-13) result = dv(result, sA);
            return result;
        }
        if (cls == 0) {
            MathExpression result = ml(coeff, ln(ab(ad(vx(), sq(W(quad_inner))))));
            if (std::abs(q.x2_coeff - 1.0) > 1e-13) result = dv(result, sA);
            return result;
        }
        return {};
    };

    if (n->type == NodeType::Divide && is_free(n->binary.left, vid)) {
        if (n->binary.right->type == NodeType::Sqrt) {
            MathExpression r = try_inv_sqrt_quad(n->binary.left, n->binary.right->unary.child);
            if (r) return r;
        }
        if (n->binary.right->type == NodeType::Power && n->binary.right->power.exponent->type == NodeType::Constant && std::abs(n->binary.right->power.exponent->constant - 0.5) < 1e-13) {
            MathExpression r = try_inv_sqrt_quad(n->binary.left, n->binary.right->power.base);
            if (r) return r;
        }
    }

    if (n->type == NodeType::Power && n->power.exponent->type == NodeType::Constant && std::abs(n->power.exponent->constant + 0.5) < 1e-13) {
        auto* one = mgr.constant(1.0).get();
        MathExpression r = try_inv_sqrt_quad(one, n->power.base);
        if (r) return r;
    }

    auto try_sqrt_quad = [&](MathExpressionNode* quad_inner) -> MathExpression {
        QuadraticDecomp q;
        if (!match_ax2_plus_b(quad_inner, vid, q)) return {};
        int cls = classify_quadratic(q);
        MathExpression a_val    = build_a(q);
        MathExpression a_sq     = ml(a_val, a_val);  // a^2
        MathExpression half_a2  = dv(a_sq, K(2.0));
        MathExpression sqrt_q   = sq(W(quad_inner));
        MathExpression x_half   = dv(vx(), K(2.0));
        MathExpression first    = ml(x_half, sqrt_q);     
        MathExpression sA       = sqrt_absA(q);

        if (cls == -1) {
            MathExpression result = ad(first, ml(half_a2, asn(dv(vx(), a_val))));
            if (std::abs(std::abs(q.x2_coeff) - 1.0) > 1e-13) result = ml(sA, result);
            return result;
        }
        if (cls == +1) {
            MathExpression result = ad(first, ml(half_a2, ash(dv(vx(), a_val))));
            if (std::abs(q.x2_coeff - 1.0) > 1e-13) result = ml(sA, result);
            return result;
        }
        if (cls == 0) {
            MathExpression result = sb(first, ml(half_a2, ln(ab(ad(vx(), sqrt_q)))));
            if (std::abs(q.x2_coeff - 1.0) > 1e-13) result = ml(sA, result);
            return result;
        }
        return {};
    };

    if (n->type == NodeType::Sqrt) {
        MathExpression r = try_sqrt_quad(n->unary.child);
        if (r) return r;
    }
    if (n->type == NodeType::Power && n->power.exponent->type == NodeType::Constant && std::abs(n->power.exponent->constant - 0.5) < 1e-13) {
        MathExpression r = try_sqrt_quad(n->power.base);
        if (r) return r;
    }
    if (n->type == NodeType::Divide && is_free(n->binary.left, vid)) {
        QuadraticDecomp q;
        if (match_ax2_plus_b(n->binary.right, vid, q)) {
            int cls = classify_quadratic(q);
            MathExpression a_val = build_a(q);
            MathExpression coeff = W(n->binary.left);
            double absA = std::abs(q.x2_coeff);

            if (cls == +1) {
                MathExpression result = ml(coeff, dv(atn(dv(vx(), a_val)), ml(K(absA), a_val)));
                return result;
            }
            if (cls == 0) {
                MathExpression two_Aa = ml(K(2.0 * absA), a_val);
                MathExpression result = ml(coeff, dv(sb(ln(ab(sb(vx(), a_val))), ln(ab(ad(vx(), a_val)))), two_Aa));
                return result;
            }
            if (cls == -1) {
                MathExpression two_Aa = ml(K(2.0 * absA), a_val);
                MathExpression result = ml(coeff, dv(sb(ln(ab(ad(a_val, vx()))), ln(ab(sb(a_val, vx())))), two_Aa));
                return result;
            }
        }
    }

    if (n->type == NodeType::Power && n->power.exponent->type == NodeType::Constant && std::abs(n->power.exponent->constant + 1.0) < 1e-13) {
        QuadraticDecomp q;
        if (match_ax2_plus_b(n->power.base, vid, q)) {
            int cls = classify_quadratic(q);
            MathExpression a_val = build_a(q);
            double absA = std::abs(q.x2_coeff);
            if (cls == +1) { return dv(atn(dv(vx(), a_val)), ml(K(absA), a_val)); }
            if (cls == 0) {
                MathExpression two_Aa = ml(K(2.0 * absA), a_val);
                return dv(sb(ln(ab(sb(vx(), a_val))), ln(ab(ad(vx(), a_val)))), two_Aa);
            }
            if (cls == -1) {
                MathExpression two_Aa = ml(K(2.0 * absA), a_val);
                return dv(sb(ln(ab(ad(a_val, vx()))), ln(ab(sb(a_val, vx())))), two_Aa);
            }
        }
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
