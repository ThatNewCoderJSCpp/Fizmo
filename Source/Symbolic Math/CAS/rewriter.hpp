#ifndef FIZMO_MATH_REWRITER_HPP
#define FIZMO_MATH_REWRITER_HPP

#include "expression.hpp"
#include "simplifier.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace fizmo {
namespace math {
namespace cas {

// Auto: collapse via named functions, contract Pythagorean. Uses CosOnly
// PowerReduced:  expand squared trig to half-angle form (sin^2->(1-cos2x)/2)
// SinOnly      Prefers sin/sinh and rewrites standalone cos^2->1-sin^2, cosh^2->sinh^2+1
// CosOnly      Prefers cos/cosh and rewrites standalone sin^2->1-cos^2, sinh^2->cosh^2+1
// Expanded     expand all named trig back to sin/cos 
enum class TrigFormPreference {
    Auto,
    PowerReduced,
    SinOnly,
    CosOnly,
    Expanded, 
};

namespace symbols {

inline bool same(MathExpressionNode* a, MathExpressionNode* b) { return a == b; }

class MathExpressionRewriter {
public:
    struct Config {
        unsigned int max_passes      = 64;
        bool     do_expand           = true;
        bool     do_factor           = true;
        bool     do_trig             = true;
        bool     do_log_contract     = true;
        unsigned int expand_depth    = 4;
        bool     do_trig_pythagorean = true;  
        TrigFormPreference trig_form = TrigFormPreference::Auto;
    };

    explicit MathExpressionRewriter(MathExpressionManager& mgr, MathExpressionSimplifier& simp) : mgr_(mgr), simp_(simp), cfg_() { init_trig_rules(); }
    explicit MathExpressionRewriter(MathExpressionManager& mgr, MathExpressionSimplifier& simp, Config cfg) : mgr_(mgr), simp_(simp), cfg_(cfg) { init_trig_rules(); }

    MathExpression rewrite(MathExpression e);

private:
    MathExpressionManager&    mgr_;
    MathExpressionSimplifier& simp_;
    Config                    cfg_;
private:
    MathExpression W(MathExpressionNode* n)               { return MathExpression(n); }
    MathExpression K(double v)                             { return mgr_.constant(v); }
    MathExpression add(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Add,      a, b); }
    MathExpression sub(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Subtract, a, b); }
    MathExpression mul(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Multiply, a, b); }
    MathExpression dv (MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Divide,   a, b); }
    MathExpression pw (MathExpression b, MathExpression e) { return mgr_.power(b, e); }
    MathExpression neg(MathExpression a)                   { return mgr_.unary(NodeType::Negate, a); }
    MathExpression un (NodeType t, MathExpression a)       { return mgr_.unary(t, a); }
private:
    bool isK(MathExpressionNode* n)           const { return n->type == NodeType::Constant; }
    bool isK(MathExpressionNode* n, double v) const { return isK(n) && n->constant == v; }
    double kv(MathExpressionNode* n)          const { return n->constant; }
    bool near_zero(double v)         const { return std::abs(v) < 1e-13; }
    bool near_eq(double a, double b) const { return std::abs(a - b) < 1e-13; }

    struct TrigNodeRule {
        const char* name;
        std::function<MathExpressionNode*(MathExpressionNode*)> match;  // node -> theta, or nullptr
        std::function<MathExpressionNode*(MathExpressionNode*)> build;  // theta -> result
        TrigFormPreference required_pref = TrigFormPreference::Auto;
        TrigFormPreference exclude_pref  = TrigFormPreference::Auto;    // Auto = no exclusion
    };

    MathExpressionNode* rebuild_children(MathExpressionNode* n);

    MathExpressionNode* rewrite_node(MathExpressionNode* n);

    MathExpressionNode* rule_const_fold(MathExpressionNode* n) { return simp_.simplify(W(n)).get(); }

    struct MulEntry {
        MathExpressionNode* base;
        MathExpressionNode* exp;  
        int                 side; // +1 = numerator, -1 = denominator
    };

    void collect_mul(MathExpressionNode* n, int side, double& coeff, std::vector<MulEntry>& out);

    MathExpressionNode* build_const_exp_product(const std::vector<std::pair<MathExpressionNode*, double>>& factors);

    MathExpressionNode* build_sym_product(const std::vector<MulEntry>& entries);

    MathExpressionNode* combine_products(MathExpressionNode* a, MathExpressionNode* b) {
        if (!a && !b) return nullptr;
        if (!a)       return b;
        if (!b)       return a;
        return mul(W(a), W(b)).get();
    }

    MathExpressionNode* absorb_coeff(double c, MathExpressionNode* n);

    static bool is_polynomial_add(MathExpressionNode* n);

    MathExpressionNode* rule_mul_normalize(MathExpressionNode* n);

    MathExpressionNode* rule_error_rewrites(MathExpressionNode* n);

    struct AddEntry {
        MathExpressionNode* kernel;
        double              coeff;
    };

    struct TrigSumRule {
        const char* name;
        std::function<MathExpressionNode*(double&, std::vector<AddEntry>&)> apply;
        TrigFormPreference required_pref = TrigFormPreference::Auto;
    };

    std::vector<TrigNodeRule> node_rules_;
    std::vector<TrigSumRule>  sum_rules_;

    MathExpressionNode* rebuild_add_entries(double const_sum, const std::vector<AddEntry>& entries);

    static void strip_scalar(MathExpressionNode* n, double base_sign, double& coeff_out, MathExpressionNode*& kernel_out);

    static double extract_coeff(MathExpressionNode* n, MathExpressionNode*& kernel_out);

    void collect_add(MathExpressionNode* n, double sign, double& const_sum, std::vector<AddEntry>& out);

    MathExpressionNode* rule_add_normalize(MathExpressionNode* n);

    MathExpressionNode* rule_power_simplify(MathExpressionNode* n);

    MathExpressionNode* rule_expand(MathExpressionNode* n, unsigned depth);

    MathExpressionNode* rule_factor(MathExpressionNode* n);

    // sets *out = x upon finding trig squared
    static bool is_trig_sq(MathExpressionNode* n, NodeType t, MathExpressionNode** out);

    // Find first AddEntry whose kernel == trig_type^2(any x).  Sets *out = x.
    int find_sq(const std::vector<AddEntry>& ent, NodeType t, MathExpressionNode** out) const;

    // Find first AddEntry whose kernel == trig_type^2(specific x).
    int find_sq(const std::vector<AddEntry>& ent, NodeType t, MathExpressionNode* theta) const;

    static void erase_pair(std::vector<AddEntry>& v, int a, int b) {
        v.erase(v.begin() + std::max(a, b));
        v.erase(v.begin() + std::min(a, b));
    }

    bool node_rule_active(const TrigNodeRule& r) const;

    bool sum_rule_active(const TrigSumRule& r) const { return r.required_pref == TrigFormPreference::Auto || cfg_.trig_form == r.required_pref; }

    static MathExpressionNode* match_pi2_minus(MathExpressionNode* nd);

    void init_trig_rules(); // end init_trig_rules()

    static bool is_sincos_product(
        MathExpressionNode* nd,
        NodeType ta, NodeType tb,
        MathExpressionNode** u_out,
        MathExpressionNode** v_out
    );

    MathExpressionNode* rule_trig(MathExpressionNode* n);

    MathExpressionNode* rule_log(MathExpressionNode* n);
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_REWRITER_HPP