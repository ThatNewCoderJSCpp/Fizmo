#ifndef FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP
#define FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP

#include "../integrator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

namespace detail_heur {

struct ProductSplit {
    MathExpressionNode* left;
    MathExpressionNode* right;
};

inline void strip_const(MathExpressionNode* n, double& coeff, MathExpressionNode*& core) {
    coeff = 1.0;
    core  = n;
    if (n->type == NodeType::Negate) {
        coeff = -1.0;
        core  = n->unary.child;
    }
    if (core->type == NodeType::Multiply && core->binary.left->type == NodeType::Constant) {
        coeff *= core->binary.left->constant;
        core   = core->binary.right;
    } else if (core->type == NodeType::Multiply && core->binary.right->type == NodeType::Constant) {
        coeff *= core->binary.right->constant;
        core   = core->binary.left;
    }
}

} // namespace detail_heur

inline MathExpression try_heuristic(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& diff,
    MathExpressionNode* n,
    std::uint64_t vid
) {
    auto K  = [&](double v)                             { return mgr.constant(v); };
    auto pw = [&](MathExpression b, MathExpression e)   { return mgr.power(b, e); };
    auto ml = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Multiply, a, b); };
    auto dv = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Divide,   a, b); };
    auto ad = [&](MathExpression a, MathExpression b)   { return mgr.binary(NodeType::Add,      a, b); };
    auto ng = [&](MathExpression a)                     { return mgr.unary(NodeType::Negate, a); };
    auto ln = [&](MathExpression a)                     { return mgr.unary(NodeType::NaturalLog, a); };
    auto ab = [&](MathExpression a)                     { return mgr.unary(NodeType::AbsoluteValue, a); };
    auto un = [&](NodeType t, MathExpression a)         { return mgr.unary(t, a); };
    auto sq = [&](MathExpression a)                     { return mgr.unary(NodeType::Sqrt, a); };
    auto W  = [](MathExpressionNode* p)                 { return MathExpression(p); };

    const std::string& var_name = mgr.variables().name(vid);

    auto deriv_of = [&](MathExpressionNode* expr) -> MathExpression {
        return simp.simplify(diff.derivative(W(expr), var_name));
    };

    auto exprs_match = [&](MathExpression a, MathExpression b) -> bool {
        return a.get() == b.get();
    };

    if (n->type == NodeType::Divide && !is_free(n->binary.right, vid)) {
        MathExpressionNode* numer = n->binary.left;
        MathExpressionNode* denom_node = n->binary.right;
        MathExpression fp = deriv_of(denom_node);
        MathExpression simplified_numer = simp.simplify(W(numer));
        if (exprs_match(simplified_numer, fp)) return ln(ab(W(denom_node)));
        double c_num, c_fp;
        MathExpressionNode *core_num, *core_fp;
        detail_heur::strip_const(simplified_numer.get(), c_num, core_num);
        detail_heur::strip_const(fp.get(), c_fp, core_fp);
        if (core_num == core_fp && c_fp != 0.0) {
            double ratio = c_num / c_fp;
            return ml(K(ratio), ln(ab(W(denom_node))));
        }
    }

    if (n->type == NodeType::Multiply) {
        auto try_substitution = [&](MathExpressionNode* candidate_fp, MathExpressionNode* g_of_f) -> MathExpression {
            if (is_free(candidate_fp, vid) || is_free(g_of_f, vid)) return {};
            MathExpressionNode* f_node = nullptr;
            if (NodeKeyHash::is_unary(g_of_f->type)) { 
                f_node = g_of_f->unary.child; 
            } else if (g_of_f->type == NodeType::Power && is_free(g_of_f->power.exponent, vid)) {
                f_node = g_of_f->power.base;
            } else if (g_of_f->type == NodeType::Divide && g_of_f->binary.left->type == NodeType::Constant && g_of_f->binary.left->constant == 1.0) {
                f_node = g_of_f->binary.right;
            } else {
                return {};
            }
            if (!f_node || is_free(f_node, vid)) return {};
            MathExpression fp = deriv_of(f_node);
            MathExpression simp_candidate = simp.simplify(W(candidate_fp));
            double c_cand, c_fp;
            MathExpressionNode *core_cand, *core_fp;
            detail_heur::strip_const(simp_candidate.get(), c_cand, core_cand);
            detail_heur::strip_const(fp.get(), c_fp, core_fp);
            bool exact_match  = exprs_match(simp_candidate, fp);
            bool scalar_match = (core_cand == core_fp) && (c_fp != 0.0);
            if (!exact_match && !scalar_match) return {};
            double scale = exact_match ? 1.0 : (c_cand / c_fp);
            MathExpression f = W(f_node);
            if (g_of_f->type == NodeType::Power && is_free(g_of_f->power.exponent, vid)) {
                if (g_of_f->power.exponent->type == NodeType::Constant) {
                    double exp = g_of_f->power.exponent->constant;
                    if (exp == -1.0) return ml(K(scale), ln(ab(f)));          
                    double np1 = exp + 1.0;
                    return ml(K(scale / np1), pw(f, K(np1)));                 
                }
                MathExpression e = W(g_of_f->power.exponent);
                MathExpression np1 = ad(e, K(1.0));
                return dv(ml(K(scale), pw(f, np1)), np1);
            }
            if (g_of_f->type == NodeType::NaturalExp) return ml(K(scale), un(NodeType::NaturalExp, f));
            if (g_of_f->type == NodeType::Sin) return ml(K(scale), ng(un(NodeType::Cos, f)));
            if (g_of_f->type == NodeType::Cos) return ml(K(scale), un(NodeType::Sin, f));
            if (g_of_f->type == NodeType::Tan) return ml(K(scale), ng(ln(ab(un(NodeType::Cos, f)))));
            if (g_of_f->type == NodeType::Cot) return ml(K(scale), ln(ab(un(NodeType::Sin, f))));
            if (g_of_f->type == NodeType::Sec) return ml(K(scale), ln(ab(ad(un(NodeType::Sec, f), un(NodeType::Tan, f)))));
            if (g_of_f->type == NodeType::Csc) return ml(K(scale), ng(ln(ab(ad(un(NodeType::Csc, f), un(NodeType::Cot, f))))));
            if (g_of_f->type == NodeType::Sinh) return ml(K(scale), un(NodeType::Cosh, f));
            if (g_of_f->type == NodeType::Cosh) return ml(K(scale), un(NodeType::Sinh, f));
            if (g_of_f->type == NodeType::NaturalLog) return ml(K(scale), mgr.binary(NodeType::Subtract, ml(f, ln(f)), f));
            if (g_of_f->type == NodeType::Sqrt) return ml(K(scale * 2.0 / 3.0), pw(f, K(1.5)));
            if (g_of_f->type == NodeType::Divide && g_of_f->binary.left->type == NodeType::Constant && g_of_f->binary.left->constant == 1.0) return ml(K(scale), ln(ab(f)));
            return {};
        };

        auto result = try_substitution(n->binary.left, n->binary.right);
        if (result) return result;
        result = try_substitution(n->binary.right, n->binary.left);
        if (result) return result;

        auto try_triple = [&](MathExpressionNode* pair_node, MathExpressionNode* third) -> MathExpression {
            if (pair_node->type != NodeType::Multiply) return {};

            auto check_sec_tan_fp = [&](MathExpressionNode* a, MathExpressionNode* b, MathExpressionNode* fp_cand) -> MathExpression {
                if (a->type == NodeType::Sec && b->type == NodeType::Tan && same(a->unary.child, b->unary.child)) {
                    MathExpressionNode* f_node = a->unary.child;
                    if (is_free(f_node, vid)) return {};
                    MathExpression fp = deriv_of(f_node);
                    MathExpression sc = simp.simplify(W(fp_cand));
                    double c1, c2; MathExpressionNode *k1, *k2;
                    detail_heur::strip_const(sc.get(), c1, k1);
                    detail_heur::strip_const(fp.get(), c2, k2);
                    if (k1 == k2 && c2 != 0.0) return ml(K(c1 / c2), un(NodeType::Sec, W(f_node)));
                }
                return {};
            };

            auto r = check_sec_tan_fp(pair_node->binary.left, pair_node->binary.right, third);
            if (r) return r;
            r = check_sec_tan_fp(pair_node->binary.right, pair_node->binary.left, third);
            if (r) return r;

            auto check_csc_cot_fp = [&](MathExpressionNode* a, MathExpressionNode* b, MathExpressionNode* fp_cand) -> MathExpression {
                if (a->type == NodeType::Csc && b->type == NodeType::Cot && same(a->unary.child, b->unary.child)) {
                    MathExpressionNode* f_node = a->unary.child;
                    if (is_free(f_node, vid)) return {};
                    MathExpression fp = deriv_of(f_node);
                    MathExpression sc = simp.simplify(W(fp_cand));
                    double c1, c2; MathExpressionNode *k1, *k2;
                    detail_heur::strip_const(sc.get(), c1, k1);
                    detail_heur::strip_const(fp.get(), c2, k2);
                    if (k1 == k2 && c2 != 0.0)
                        return ml(K(c1 / c2), ng(un(NodeType::Csc, W(f_node))));
                }
                return {};
            };

            r = check_csc_cot_fp(pair_node->binary.left, pair_node->binary.right, third);
            if (r) return r;
            r = check_csc_cot_fp(pair_node->binary.right, pair_node->binary.left, third);
            return r;
        };

        auto res = try_triple(n->binary.left, n->binary.right);
        if (res) return res;
        res = try_triple(n->binary.right, n->binary.left);
        if (res) return res;
    }

    return {};
}

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP