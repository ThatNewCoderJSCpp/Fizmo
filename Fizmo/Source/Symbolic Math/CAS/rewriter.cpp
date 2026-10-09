#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

auto MathExpressionRewriter::rewrite(MathExpression e) -> MathExpression {
        MathExpressionNode* cur = e.get();
        for (unsigned pass = 0; pass < cfg_.max_passes; ++pass) {
            MathExpressionNode* next = rewrite_node(cur);
            next = simp_.simplify(W(next)).get();
            if (next == cur) break;
            cur = next;
        }
        return W(cur);
    }

auto MathExpressionRewriter::rebuild_children(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        switch (n->type) {
        case NT::Constant: case NT::Variable:
        case NT::PositiveInfinity: case NT::NegativeInfinity:
        case NT::NaN: case NT::Undefined: case NT::Indeterminate:
            return n;
        case NT::Power:
            return mgr_.power(W(rewrite_node(n->power.base)), W(rewrite_node(n->power.exponent))).get();
        case NT::AppliedFunction: {
            const std::uint64_t cnt = n->applied.arg_count;
            std::vector<MathExpression> args; args.reserve(cnt);
            std::vector<std::uint64_t>  ords; ords.reserve(cnt);
            
            for (std::uint64_t i = 0; i < cnt; ++i) {
                args.push_back(W(rewrite_node(n->applied.args[i])));
                ords.push_back(n->applied.orders[i]);
            }

            return mgr_.applied(n->applied.func_id, args, ords).get();
        }
        default:
            if (NodeKeyHash::is_unary(n->type))
                return mgr_.unary(n->type, W(rewrite_node(n->unary.child))).get();
            if (NodeKeyHash::is_binary(n->type))
                return mgr_.binary(n->type, W(rewrite_node(n->binary.left)), W(rewrite_node(n->binary.right))).get();
            return n;
        }
    }

auto MathExpressionRewriter::rewrite_node(MathExpressionNode* n) -> MathExpressionNode* {
        if (!n) return n;
        n = rebuild_children(n);
        n = rule_const_fold(n);
        n = rule_power_simplify(n);
        n = rule_mul_normalize(n);
        n = rule_add_normalize(n);
        n = rule_error_rewrites(n);
        if (cfg_.do_trig)         n = rule_trig(n);
        if (cfg_.do_log_contract) n = rule_log(n);
        if (cfg_.do_factor)       n = rule_factor(n);
        if (cfg_.do_expand)       n = rule_expand(n, 0);
        n = rule_add_normalize(n); 
        return n;
    }

auto MathExpressionRewriter::collect_mul(MathExpressionNode* n, int side, double& coeff, std::vector<MulEntry>& out) -> void {
        using NT = NodeType;
        switch (n->type) {
        case NT::Multiply:
            collect_mul(n->binary.left,  side, coeff, out);
            collect_mul(n->binary.right, side, coeff, out);
            return;
        case NT::Divide:
            collect_mul(n->binary.left,   side, coeff, out);
            collect_mul(n->binary.right, -side, coeff, out);
            return;
        case NT::Negate:
            coeff *= -1.0;
            collect_mul(n->unary.child, side, coeff, out);
            return;
        case NT::Constant:
            if (side > 0) coeff *= n->constant;
            else          coeff /= n->constant;   
            return;
        case NT::Power:
            out.push_back({ n->power.base, n->power.exponent, side });
            return;
        case NT::Sqrt:
            out.push_back({ n->unary.child, K(0.5).get(), side });
            return;
        case NT::Cbrt:
            out.push_back({ n->unary.child, K(1.0 / 3.0).get(), side });
            return;
        default:
            out.push_back({ n, K(1.0).get(), side });
            return;
        }
    }

auto MathExpressionRewriter::build_const_exp_product(const std::vector<std::pair<MathExpressionNode*, double>>& factors) -> MathExpressionNode* {
        MathExpressionNode* result = nullptr;
        for (auto& p : factors) {
            MathExpressionNode* base = p.first;
            double              e    = p.second;
            MathExpressionNode* term;
            if      (near_eq(e, 1.0))       term = base;
            else if (near_eq(e, 0.5))       term = un(NodeType::Sqrt, W(base)).get();
            else if (near_eq(e, 1.0 / 3.0)) term = un(NodeType::Cbrt, W(base)).get();
            else                             term = pw(W(base), K(e)).get();
            result = result ? mul(W(result), W(term)).get() : term;
        }
        return result; 
    }

auto MathExpressionRewriter::build_sym_product(const std::vector<MulEntry>& entries) -> MathExpressionNode* {
        struct SymGroup {
            MathExpressionNode* base;
            MathExpressionNode* exp;   
        };
        std::vector<SymGroup> groups;
        for (auto& e : entries) {
            SymGroup* g = nullptr;
            for (auto& grp : groups) { if (same(grp.base, e.base)) { g = &grp; break; } }
            if (!g) {
                groups.push_back({ e.base, nullptr });
                g = &groups.back();
            }
            MathExpressionNode* signed_exp = (e.side > 0) ? e.exp : neg(W(e.exp)).get();
            g->exp = g->exp ? add(W(g->exp), W(signed_exp)).get() : signed_exp;
        }
        MathExpressionNode* result = nullptr;
        for (auto& grp : groups) {
            MathExpressionNode* exp = simp_.simplify(W(grp.exp)).get();
            MathExpressionNode* term;
            if      (isK(exp) && near_eq(kv(exp),       1.0)) term = grp.base;
            else if (isK(exp) && near_eq(kv(exp),       0.5)) term = un(NodeType::Sqrt, W(grp.base)).get();
            else if (isK(exp) && near_eq(kv(exp), 1.0 / 3.0)) term = un(NodeType::Cbrt, W(grp.base)).get();
            else if (isK(exp) && near_eq(kv(exp),       0.0)) continue; 
            else                                               term = pw(W(grp.base), W(exp)).get();
            result = result ? mul(W(result), W(term)).get() : term;
        }
        return result;
    }

auto MathExpressionRewriter::absorb_coeff(double c, MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        if (near_eq(c, 1.0)) return n;
        if (n->type == NT::Add) return mgr_.binary(NT::Add, W(absorb_coeff(c, n->binary.left)), W(absorb_coeff(c, n->binary.right))).get();
        if (n->type == NT::Subtract) return mgr_.binary(NT::Subtract, W(absorb_coeff(c, n->binary.left)), W(absorb_coeff(c, n->binary.right))).get();
        if (n->type == NT::Constant) return K(c * n->constant).get();
        return near_eq(c, -1.0) ? neg(W(n)).get() : mul(K(c), W(n)).get();
    }

auto MathExpressionRewriter::is_polynomial_add(MathExpressionNode* n) -> bool {
        using NT = NodeType;
        if (n->type == NT::Constant || n->type == NT::Variable) return true;
        if (n->type == NT::Add || n->type == NT::Subtract) return is_polynomial_add(n->binary.left) && is_polynomial_add(n->binary.right);
        if (n->type == NT::Multiply) return (n->binary.left->type == NT::Constant || n->binary.right->type == NT::Constant);
        if (n->type == NT::Power) return n->power.base->type == NT::Variable && n->power.exponent->type == NT::Constant;
        if (n->type == NT::Negate) return is_polynomial_add(n->unary.child);
        return false;
    }

auto MathExpressionRewriter::rule_mul_normalize(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        if (n->type != NT::Multiply && n->type != NT::Divide && n->type != NT::Negate) return n;
        double coeff = 1.0;
        std::vector<MulEntry> raw;
        collect_mul(n, +1, coeff, raw);
        if (std::isnan(coeff)) return K(coeff).get();
        struct Group {
            MathExpressionNode*   base;
            double                num_exp; 
            std::vector<MulEntry> sym;      
        };
        std::vector<Group> groups;
        for (auto& e : raw) {
            Group* g = nullptr;
            for (auto& grp : groups) { if (same(grp.base, e.base)) { g = &grp; break; }}
            if (!g) {
                groups.push_back({ e.base, 0.0, {} });
                g = &groups.back();
            }
            if (isK(e.exp)) {
                g->num_exp += (e.side > 0 ? 1.0 : -1.0) * kv(e.exp);
            } else {
                g->sym.push_back(e);
            }
        }
        std::vector<std::pair<MathExpressionNode*, double>> num_const, den_const;
        std::vector<MulEntry> sym_num, sym_den;
        for (auto& grp : groups) {
            if (!near_zero(grp.num_exp)) {
                if (grp.num_exp > 0.0) num_const.push_back({ grp.base,  grp.num_exp });
                else                   den_const.push_back({ grp.base, -grp.num_exp });
            }
            for (auto& se : grp.sym) {
                if (se.side > 0) sym_num.push_back(se);
                else             sym_den.push_back(se);
            }
        }
        MathExpressionNode* numerator   = combine_products(build_const_exp_product(num_const), build_sym_product(sym_num));
        MathExpressionNode* denominator = combine_products(build_const_exp_product(den_const), build_sym_product(sym_den));
        MathExpressionNode* result;
        if (!numerator) {
            result = K(coeff).get();
        } else if (near_eq(coeff, 1.0)) {
            result = numerator;
        } else if (near_eq(coeff, -1.0)) {
            result = neg(W(numerator)).get();
        } else {
            if ((numerator->type == NT::Add || numerator->type == NT::Subtract) && is_polynomial_add(numerator)) {
                result = absorb_coeff(coeff, numerator);
            } else {
                double recip = 1.0 / std::abs(coeff);
                int ri = static_cast<int>(std::round(recip));
                if (near_eq(recip, (double)ri) && ri >= 2) {
                    MathExpressionNode* pos = dv(W(numerator), K((double)ri)).get();
                    result = (coeff < 0.0) ? neg(W(pos)).get() : pos;
                } else {
                    result = mul(K(coeff), W(numerator)).get();
                }
            }
        }
        if (denominator) result = dv(W(result), W(denominator)).get();
        return result;
    }

auto MathExpressionRewriter::rule_error_rewrites(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        if (n->type == NT::Subtract) {
            auto* L = n->binary.left;
            auto* R = n->binary.right;
            if (L->type == NT::Constant && L->constant == 1.0 && R->type == NT::Erf) { return mgr_.unary(NT::Erfc, W(R->unary.child)).get(); }
        }
        if (n->type == NT::Subtract) {
            auto* L = n->binary.left;
            auto* R = n->binary.right;
            if (L->type == NT::Constant && L->constant == 1.0 && R->type == NT::Erfc) { return mgr_.unary(NT::Erf, W(R->unary.child)).get(); }
        }
        if (n->type == NT::InverseErf) {
            auto* c = n->unary.child;
            if (c->type == NT::Subtract && c->binary.left->type == NT::Constant && c->binary.left->constant == 1.0) { return mgr_.unary(NT::InverseErfc, W(c->binary.right)).get(); }
        }
        if (n->type == NT::InverseErfc) {
            auto* c = n->unary.child;
            if (c->type == NT::Subtract && c->binary.left->type == NT::Constant && c->binary.left->constant == 1.0) { return mgr_.unary(NT::InverseErf, W(c->binary.right)).get(); }
        }
        return n;
    }

auto MathExpressionRewriter::rebuild_add_entries(double const_sum, const std::vector<AddEntry>& entries) -> MathExpressionNode* {
        MathExpressionNode* result = nullptr;
        auto append = [&](MathExpressionNode* term, double c) {
            if (!result) {
                result = (c < 0.0) ? neg(W(term)).get() : term;
            } else if (c >= 0.0) {
                result = add(W(result), W(term)).get();
            } else {
                result = sub(W(result), W(term)).get();
            }
        };
        for (auto& e : entries) { 
            if (near_zero(e.coeff)) continue;
            if (e.coeff <= 0.0)    continue;
            double ac   = e.coeff;
            MathExpressionNode* term = near_eq(ac, 1.0) ? e.kernel : mul(K(ac), W(e.kernel)).get();
            append(term, 1.0);
        }
        for (auto& e : entries) { 
            if (near_zero(e.coeff)) continue;
            if (e.coeff >= 0.0)    continue;
            double ac   = std::abs(e.coeff);
            MathExpressionNode* term = near_eq(ac, 1.0) ? e.kernel : mul(K(ac), W(e.kernel)).get();
            append(term, -1.0);
        }
        if (!near_zero(const_sum)) {
            if (!result) return K(const_sum).get();
            if (const_sum > 0.0) result = add(W(result), K(const_sum)).get();
            else                 result = sub(W(result), K(-const_sum)).get();
        }
        return result ? result : K(0.0).get();
    }

auto MathExpressionRewriter::strip_scalar(MathExpressionNode* n, double base_sign, double& coeff_out, MathExpressionNode*& kernel_out) -> void {
        double c = base_sign;
        MathExpressionNode* k = n;
        if (k->type == NodeType::Negate) {
            c *= -1.0;
            k = k->unary.child;
        }
        if (k->type == NodeType::Multiply && k->binary.left->type == NodeType::Constant) {
            c *= k->binary.left->constant;
            k  = k->binary.right;
        } else if (k->type == NodeType::Multiply && k->binary.right->type == NodeType::Constant) {
            c *= k->binary.right->constant;
            k  = k->binary.left;
        }
        coeff_out  = c;
        kernel_out = k;
    }

auto MathExpressionRewriter::extract_coeff(MathExpressionNode* n, MathExpressionNode*& kernel_out) -> double {
        if (n->type == NodeType::Negate) {
            double c = extract_coeff(n->unary.child, kernel_out);
            return -c;
        }
        if (n->type == NodeType::Multiply) {
            if (n->binary.left->type == NodeType::Constant) {
                double c = extract_coeff(n->binary.right, kernel_out);
                return n->binary.left->constant * c;
            }
            if (n->binary.right->type == NodeType::Constant) {
                double c = extract_coeff(n->binary.left, kernel_out);
                return n->binary.right->constant * c;
            }
        }
        kernel_out = n;
        return 1.0;
    }

auto MathExpressionRewriter::collect_add(MathExpressionNode* n, double sign, double& const_sum, std::vector<AddEntry>& out) -> void {
        using NT = NodeType;
        switch (n->type) {
        case NT::Add:
            collect_add(n->binary.left,  sign, const_sum, out);
            collect_add(n->binary.right, sign, const_sum, out);
            return;
        case NT::Subtract:
            collect_add(n->binary.left,   sign, const_sum, out);
            collect_add(n->binary.right, -sign, const_sum, out);
            return;
        case NT::Negate:
            collect_add(n->unary.child, -sign, const_sum, out);
            return;
        case NT::Constant:
            const_sum += sign * n->constant;
            return;
        case NT::Divide: {
            MathExpressionNode* num_k = nullptr;
            MathExpressionNode* den_k = nullptr;
            double num_c = extract_coeff(n->binary.left,  num_k);
            double den_c = extract_coeff(n->binary.right, den_k);
            double net   = sign * (num_c / den_c);
            symbols::MathExpressionNode* reduced = mgr_.binary(NT::Divide, W(num_k), W(den_k)).get();
            for (auto& e : out) { if (same(e.kernel, reduced)) { e.coeff += net; return; }}
            out.push_back({ reduced, net });
            return;
        }
        default:
            break;
        }

        double c;
        MathExpressionNode* kernel;
        strip_scalar(n, sign, c, kernel);
        if (kernel->type == NT::Constant) {
            const_sum += c * kernel->constant;
            return;
        }
        for (auto& e : out) {
            if (same(e.kernel, kernel)) {
                e.coeff += c;
                return;
            }
        }
        out.push_back({ kernel, c });
    }

auto MathExpressionRewriter::rule_add_normalize(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        if (n->type != NT::Add && n->type != NT::Subtract && n->type != NT::Negate) return n;
        double const_sum = 0.0;
        std::vector<AddEntry> entries;
        collect_add(n, 1.0, const_sum, entries);
        std::vector<AddEntry> live;
        live.reserve(entries.size());
        for (auto& e : entries) { if (!near_zero(e.coeff)) live.push_back(e); }
        std::stable_partition(live.begin(), live.end(), [](const AddEntry& e) { return e.coeff > 0.0; });
        MathExpressionNode* result = nullptr;
        for (auto& e : live) {
            double ac = std::abs(e.coeff);
            MathExpressionNode* term;
            if (near_eq(ac, 1.0)) term = e.kernel;
            else                  term = mul(K(ac), W(e.kernel)).get();

            if (!result) {
                if (near_eq(ac, 1.0))
                    result = (e.coeff < 0.0) ? neg(W(e.kernel)).get() : e.kernel;
                else if (e.coeff < 0.0)
                    result = mul(K(e.coeff), W(e.kernel)).get(); 
                else
                    result = term;
            } else if (e.coeff > 0.0) {
                result = mgr_.binary(NT::Add,      W(result), W(term)).get();
            } else {
                result = mgr_.binary(NT::Subtract, W(result), W(term)).get();
            }
        }
        if (result == nullptr) return K(const_sum).get();
        if (!near_zero(const_sum)) {
            if (const_sum > 0.0)
                result = mgr_.binary(NT::Add,      W(result), K(const_sum)).get();
            else
                result = mgr_.binary(NT::Subtract, W(result), K(-const_sum)).get();
        }
        return result;
    }

auto MathExpressionRewriter::rule_power_simplify(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        // (x^a)^b  ->  x^(a*b)
        if (n->type == NT::Power && n->power.base->type == NT::Power) {
            auto* inner  = n->power.base;
            MathExpression new_exp = simp_.simplify(mul(W(inner->power.exponent), W(n->power.exponent)));
            return pw(W(inner->power.base), new_exp).get();
        }
        // sqrt(x^2)  ->  |x|
        if (n->type == NT::Sqrt && n->unary.child->type == NT::Power && isK(n->unary.child->power.exponent, 2.0)) return un(NT::AbsoluteValue, W(n->unary.child->power.base)).get();
        // (sqrt(x))^2  ->  |x|
        if (n->type == NT::Power && n->power.base->type == NT::Sqrt && isK(n->power.exponent, 2.0)) return un(NT::AbsoluteValue, W(n->power.base->unary.child)).get();
        // (cbrt(x))^3  ->  |x|
        if (n->type == NT::Power && n->power.base->type == NT::Cbrt && isK(n->power.exponent, 3.0)) return un(NT::AbsoluteValue, W(n->power.base->unary.child)).get();
        // x^0.5  ->  sqrt(x)
        if (n->type == NT::Power && isK(n->power.exponent, 0.5)) return un(NT::Sqrt, W(n->power.base)).get();
        // x^(1/3)  ->  cbrt(x)
        if (n->type == NT::Power && isK(n->power.exponent) && near_eq(kv(n->power.exponent), 1.0 / 3.0)) return un(NT::Cbrt, W(n->power.base)).get();
        // (c*x)^k  ->  c^k * x^k  for constant c and numeric k
        if (n->type == NT::Power && isK(n->power.exponent)) {
            double e    = kv(n->power.exponent);
            auto*  base = n->power.base;
            if (base->type == NT::Multiply && isK(base->binary.left))
                return mul(K(std::pow(kv(base->binary.left), e)), pw(W(base->binary.right), W(n->power.exponent))).get();
            if (base->type == NT::Multiply && isK(base->binary.right))
                return mul(K(std::pow(kv(base->binary.right), e)), pw(W(base->binary.left),  W(n->power.exponent))).get();
            // (-x)^n  for integer n
            if (base->type == NT::Negate && e == std::floor(e)) {
                int ei = static_cast<int>(e);
                MathExpression inner = pw(W(base->unary.child), W(n->power.exponent));
                return (ei % 2 == 0) ? inner.get() : neg(inner).get();
            }
        }
        // ln(x^n)  ->  n*ln(x)   for non-negative base
        if (n->type == NT::NaturalLog && n->unary.child->type == NT::Power) {
            auto* inner = n->unary.child;
            if (!isK(inner->power.base) || kv(inner->power.base) > 0.0) return mul(W(inner->power.exponent), un(NT::NaturalLog, W(inner->power.base))).get();
        }
        return n;
    }

auto MathExpressionRewriter::rule_expand(MathExpressionNode* n, unsigned depth) -> MathExpressionNode* {
        if (depth >= cfg_.expand_depth) return n;
        using NT = NodeType;

        if (n->type == NT::Multiply) {
            auto* l = n->binary.left;
            auto* r = n->binary.right;
            if (r->type == NT::Add) return add(W(rule_expand(mul(W(l), W(r->binary.left) ).get(), depth+1)), W(rule_expand(mul(W(l), W(r->binary.right)).get(), depth+1))).get();
            if (r->type == NT::Subtract) return sub(W(rule_expand(mul(W(l), W(r->binary.left) ).get(), depth+1)), W(rule_expand(mul(W(l), W(r->binary.right)).get(), depth+1))).get();
            if (l->type == NT::Add) return add(W(rule_expand(mul(W(l->binary.left),  W(r)).get(), depth+1)), W(rule_expand(mul(W(l->binary.right), W(r)).get(), depth+1))).get();
            if (l->type == NT::Subtract) return sub(W(rule_expand(mul(W(l->binary.left),  W(r)).get(), depth+1)), W(rule_expand(mul(W(l->binary.right), W(r)).get(), depth+1))).get();
        }
        if (n->type == NT::Power && isK(n->power.exponent)) {
            double expv = kv(n->power.exponent);
            auto*  base = n->power.base;
            if ((base->type == NT::Add || base->type == NT::Subtract)
                && expv == std::floor(expv) && expv >= 2.0 && expv <= 4.0) {
                int e = static_cast<int>(expv);
                MathExpressionNode* r = base;
                for (int i = 1; i < e; ++i) r = rule_expand(mul(W(r), W(base)).get(), depth+1);
                return r;
            }
        }
        return n;
    }

auto MathExpressionRewriter::rule_factor(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        if (n->type != NT::Add && n->type != NT::Subtract) return n;
        double const_sum = 0.0;
        std::vector<AddEntry> entries;
        collect_add(n, 1.0, const_sum, entries);
        if (!near_zero(const_sum) || entries.size() < 2) return n;
        MathExpressionNode* candidate = entries[0].kernel;
        for (auto& e : entries) { if (!same(e.kernel, candidate)) return n; }
        double coeff_sum = 0.0;
        for (auto& e : entries) coeff_sum += e.coeff;
        if (near_zero(coeff_sum)) return K(0.0).get();
        return mul(K(coeff_sum), W(candidate)).get();
    }

auto MathExpressionRewriter::is_trig_sq(MathExpressionNode* n, NodeType t, MathExpressionNode** out) -> bool {
        if (n->type != NodeType::Power)                          return false;
        if (n->power.base->type != t)                            return false;
        if (n->power.exponent->type != NodeType::Constant)       return false;
        if (std::abs(n->power.exponent->constant - 2.0) > 1e-13) return false;
        if (out) *out = n->power.base->unary.child;
        return true;
    }

auto MathExpressionRewriter::find_sq(const std::vector<AddEntry>& ent, NodeType t, MathExpressionNode** out) const -> int {
        for (int i = 0; i < (int)ent.size(); ++i) {
            MathExpressionNode* th = nullptr;
            if (is_trig_sq(ent[i].kernel, t, &th)) { if (out) *out = th; return i; }
        }
        return -1;
    }

auto MathExpressionRewriter::find_sq(const std::vector<AddEntry>& ent, NodeType t, MathExpressionNode* theta) const -> int {
        for (int i = 0; i < (int)ent.size(); ++i) {
            MathExpressionNode* th = nullptr;
            if (is_trig_sq(ent[i].kernel, t, &th) && same(th, theta)) return i;
        }
        return -1;
    }

auto MathExpressionRewriter::node_rule_active(const TrigNodeRule& r) const -> bool {
        if (r.exclude_pref != TrigFormPreference::Auto && cfg_.trig_form == r.exclude_pref) return false;
        return r.required_pref == TrigFormPreference::Auto || cfg_.trig_form == r.required_pref;
    }

auto MathExpressionRewriter::match_pi2_minus(MathExpressionNode* nd) -> MathExpressionNode* {
        if (nd->type != NodeType::Subtract) return nullptr;
        auto* lhs = nd->binary.left;
        if (lhs->type != NodeType::Constant) return nullptr;
        if (std::abs(lhs->constant - constants::pi_2()) > 1e-12) return nullptr;
        return nd->binary.right;
    }

auto MathExpressionRewriter::init_trig_rules() -> void {
        using NT  = NodeType;
        using TFP = TrigFormPreference;

        auto ratio = [&](const char* nm, NT a, NT b, NT res) {
            node_rules_.push_back({ nm,
                [a, b](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (nd->type != NodeType::Divide) return nullptr;
                    auto* l = nd->binary.left; auto* r = nd->binary.right;
                    if (l->type != a || r->type != b) return nullptr;
                    if (!same(l->unary.child, r->unary.child)) return nullptr;
                    return l->unary.child;
                },
                [this, res](MathExpressionNode* th) { return un(res, W(th)).get(); },
                TFP::Auto, TFP::Expanded
            });
        };

        auto recip = [&](const char* nm, NT func, NT res) {
            node_rules_.push_back({ nm,
                [func](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (nd->type != NodeType::Divide) return nullptr;
                    if (nd->binary.left->type != NodeType::Constant || std::abs(nd->binary.left->constant - 1.0) > 1e-12) return nullptr;
                    if (nd->binary.right->type != func) return nullptr;
                    return nd->binary.right->unary.child;
                },
                [this, res](MathExpressionNode* th) { return un(res, W(th)).get(); },
                TFP::Auto, TFP::Expanded
            });
        };

        auto expand_ratio = [&](const char* nm, NT named, NT num, NT den) {
            node_rules_.push_back({ nm,
                [named](MathExpressionNode* nd) -> MathExpressionNode* {
                    return (NodeKeyHash::is_unary(named) && nd->type == named) ? nd->unary.child : nullptr;
                },
                [this, num, den](MathExpressionNode* th) {
                    return dv(un(num, W(th)), un(den, W(th))).get();
                },
                TFP::Expanded, TFP::Auto
            });
        };

        auto expand_recip = [&](const char* nm, NT named, NT denom) {
            node_rules_.push_back({ nm,
                [named](MathExpressionNode* nd) -> MathExpressionNode* {
                    return (NodeKeyHash::is_unary(named) && nd->type == named) ? nd->unary.child : nullptr;
                },
                [this, denom](MathExpressionNode* th) {
                    return dv(K(1.0), un(denom, W(th))).get();
                },
                TFP::Expanded, TFP::Auto
            });
        };

        auto sq_rule = [&](const char* nm, NT func, std::function<MathExpressionNode*(MathExpressionNode*)> bld, TFP pref) {
            node_rules_.push_back({ nm,
                [func](MathExpressionNode* nd) -> MathExpressionNode* {
                    MathExpressionNode* th = nullptr;
                    return is_trig_sq(nd, func, &th) ? th : nullptr;
                },
                std::move(bld), pref, TFP::Auto
            });
        };

        auto arc_recip = [&](const char* nm, NT func, NT res) {
            node_rules_.push_back({ nm,
                [func](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (nd->type != func) return nullptr;
                    auto* arg = nd->unary.child;
                    if (arg->type != NodeType::Divide) return nullptr;
                    if (arg->binary.left->type != NodeType::Constant) return nullptr;
                    if (std::abs(arg->binary.left->constant - 1.0) > 1e-12) return nullptr;
                    return arg->binary.right;  // x
                },
                [this, res](MathExpressionNode* x) { return un(res, W(x)).get(); },
                TFP::Auto, TFP::Auto
            });
        };

        // Trig ratios
        ratio("sin/cos->tan", NT::Sin, NT::Cos, NT::Tan);   
        ratio("sin/tan->cos", NT::Sin, NT::Tan, NT::Cos);   
        ratio("cos/sin->cot", NT::Cos, NT::Sin, NT::Cot);  
        ratio("cos/cot->sin", NT::Cos, NT::Cot, NT::Sin);  
        ratio("tan/sin->sec", NT::Tan, NT::Sin, NT::Sec);   
        ratio("tan/sec->sin", NT::Tan, NT::Sec, NT::Sin);   
        ratio("cot/cos->csc", NT::Cot, NT::Cos, NT::Csc);   
        ratio("cot/csc->cos", NT::Cot, NT::Csc, NT::Cos);   
        ratio("sec/tan->csc", NT::Sec, NT::Tan, NT::Csc);   
        ratio("sec/csc->tan", NT::Sec, NT::Csc, NT::Tan);   
        ratio("csc/cot->sec", NT::Csc, NT::Cot, NT::Sec);   
        ratio("csc/sec->cot", NT::Csc, NT::Sec, NT::Cot);   

        // Hyperbolic trig ratios
        ratio("sinh/cosh->tanh", NT::Sinh, NT::Cosh, NT::Tanh);  
        ratio("sinh/tanh->cosh", NT::Sinh, NT::Tanh, NT::Cosh);  
        ratio("cosh/sinh->coth", NT::Cosh, NT::Sinh, NT::Coth);  
        ratio("cosh/coth->sinh", NT::Cosh, NT::Coth, NT::Sinh);  
        ratio("tanh/sinh->sech", NT::Tanh, NT::Sinh, NT::Sech);  
        ratio("tanh/sech->sinh", NT::Tanh, NT::Sech, NT::Sinh);  
        ratio("coth/cosh->csch", NT::Coth, NT::Cosh, NT::Csch);  
        ratio("coth/csch->cosh", NT::Coth, NT::Csch, NT::Cosh);  
        ratio("sech/tanh->csch", NT::Sech, NT::Tanh, NT::Csch);  
        ratio("sech/csch->tanh", NT::Sech, NT::Csch, NT::Tanh);  
        ratio("csch/coth->sech", NT::Csch, NT::Coth, NT::Sech);  
        ratio("csch/sech->coth", NT::Csch, NT::Sech, NT::Coth);  

        // Reciprocals
        recip("1/sin->csc",   NT::Sin,  NT::Csc);
        recip("1/cos->sec",   NT::Cos,  NT::Sec);
        recip("1/tan->cot",   NT::Tan,  NT::Cot);
        recip("1/cot->tan",   NT::Cot,  NT::Tan);
        recip("1/csc->sin",   NT::Csc,  NT::Sin);
        recip("1/sec->cos",   NT::Sec,  NT::Cos);
        recip("1/sinh->csch", NT::Sinh, NT::Csch);
        recip("1/cosh->sech", NT::Cosh, NT::Sech);
        recip("1/tanh->coth", NT::Tanh, NT::Coth);
        recip("1/coth->tanh", NT::Coth, NT::Tanh);
        recip("1/csch->sinh", NT::Csch, NT::Sinh);
        recip("1/sech->cosh", NT::Sech, NT::Cosh);

        // Expanded ratio
        expand_ratio("tan->sin/cos",    NT::Tan,  NT::Sin,  NT::Cos);
        expand_ratio("cot->cos/sin",    NT::Cot,  NT::Cos,  NT::Sin);
        expand_ratio("tanh->sinh/cosh", NT::Tanh, NT::Sinh, NT::Cosh);
        expand_ratio("coth->cosh/sinh", NT::Coth, NT::Cosh, NT::Sinh);
        expand_recip("csc->1/sin",      NT::Csc,  NT::Sin);
        expand_recip("sec->1/cos",      NT::Sec,  NT::Cos);
        expand_recip("csch->1/sinh",    NT::Csch, NT::Sinh);
        expand_recip("sech->1/cosh",    NT::Sech, NT::Cosh);

        // arcf(1/x) → arccofunc(x)
        // Trig
        arc_recip("asin(1/x)->acsc(x)", NT::Arcsin, NT::Arccsc);  
        arc_recip("acsc(1/x)->asin(x)", NT::Arccsc, NT::Arcsin);  
        arc_recip("acos(1/x)->asec(x)", NT::Arccos, NT::Arcsec);  
        arc_recip("asec(1/x)->acos(x)", NT::Arcsec, NT::Arccos);  
        arc_recip("atan(1/x)->acot(x)", NT::Arctan, NT::Arccot);  
        arc_recip("acot(1/x)->atan(x)", NT::Arccot, NT::Arctan); 

        // Hyperbolic 
        arc_recip("asinh(1/x)->acsch(x)", NT::Arcsinh, NT::Arccsch); 
        arc_recip("acsch(1/x)->asinh(x)", NT::Arccsch, NT::Arcsinh);  
        arc_recip("acosh(1/x)->asech(x)", NT::Arccosh, NT::Arcsech);  
        arc_recip("asech(1/x)->acosh(x)", NT::Arcsech, NT::Arccosh);  
        arc_recip("atanh(1/x)->acoth(x)", NT::Arctanh, NT::Arccoth);
        arc_recip("acoth(1/x)->atanh(x)", NT::Arccoth, NT::Arctanh);  

        // Products
        // sin(x)*cos(x) -> sin(2x)/2,  sinh(x)*cosh(x) -> sinh(2x)/2
        node_rules_.push_back({ "sin*cos->sin(2x)/2",
            [](MathExpressionNode* nd) -> MathExpressionNode* {
                if (nd->type != NodeType::Multiply) return nullptr;
                auto* l = nd->binary.left; auto* r = nd->binary.right;
                bool sc = l->type == NodeType::Sin && r->type == NodeType::Cos;
                bool cs = l->type == NodeType::Cos && r->type == NodeType::Sin;
                if ((!sc && !cs) || !same(l->unary.child, r->unary.child)) return nullptr;
                return l->unary.child;
            },
            [this](MathExpressionNode* th) {
                return dv(un(NT::Sin, mul(K(2.0), W(th))), K(2.0)).get();
            }
        });

        node_rules_.push_back({ "sinh*cosh->sinh(2x)/2",
            [](MathExpressionNode* nd) -> MathExpressionNode* {
                if (nd->type != NodeType::Multiply) return nullptr;
                auto* l = nd->binary.left; auto* r = nd->binary.right;
                bool sc = l->type == NodeType::Sinh && r->type == NodeType::Cosh;
                bool cs = l->type == NodeType::Cosh && r->type == NodeType::Sinh;
                if ((!sc && !cs) || !same(l->unary.child, r->unary.child)) return nullptr;
                return l->unary.child;
            },
            [this](MathExpressionNode* th) {
                return dv(un(NT::Sinh, mul(K(2.0), W(th))), K(2.0)).get();
            }
        });

        // Power-Reduce 
        sq_rule("sin^2->(1-cos2x)/2", NT::Sin,
            [this](MathExpressionNode* th) {
                return dv(sub(K(1.0), un(NT::Cos, mul(K(2.0), W(th)))), K(2.0)).get();
            }, TFP::PowerReduced);

        sq_rule("cos^2->(1+cos2x)/2", NT::Cos,
            [this](MathExpressionNode* th) {
                return dv(add(K(1.0), un(NT::Cos, mul(K(2.0), W(th)))), K(2.0)).get();
            }, TFP::PowerReduced);

        sq_rule("sinh^2->(cosh2x-1)/2", NT::Sinh,
            [this](MathExpressionNode* th) {
                return dv(sub(un(NT::Cosh, mul(K(2.0), W(th))), K(1.0)), K(2.0)).get();
            }, TFP::PowerReduced);

        sq_rule("cosh^2->(cosh2x+1)/2", NT::Cosh,
            [this](MathExpressionNode* th) {
                return dv(add(un(NT::Cosh, mul(K(2.0), W(th))), K(1.0)), K(2.0)).get();
            }, TFP::PowerReduced);

        sq_rule("tan^2->sec^2-1",  NT::Tan,
            [this](MathExpressionNode* th) { return sub(pw(un(NT::Sec,  W(th)), K(2.0)), K(1.0)).get(); },
            TFP::PowerReduced);

        sq_rule("cot^2->csc^2-1",  NT::Cot,
            [this](MathExpressionNode* th) { return sub(pw(un(NT::Csc,  W(th)), K(2.0)), K(1.0)).get(); },
            TFP::PowerReduced);

        sq_rule("tanh^2->1-sech^2", NT::Tanh,
            [this](MathExpressionNode* th) { return sub(K(1.0), pw(un(NT::Sech, W(th)), K(2.0))).get(); },
            TFP::PowerReduced);

        sq_rule("coth^2->csch^2+1", NT::Coth,
            [this](MathExpressionNode* th) { return add(pw(un(NT::Csch, W(th)), K(2.0)), K(1.0)).get(); },
            TFP::PowerReduced);

        // SINONLY / COSONLY — single-node squared substitution
        // SinOnly:  cos^2->1-sin^2,   cosh^2->sinh^2+1
        // CosOnly:  sin^2->1-cos^2,   sinh^2->cosh^2-1
        sq_rule("cos^2->1-sin^2",   NT::Cos,
            [this](MathExpressionNode* th) { return sub(K(1.0), pw(un(NT::Sin,  W(th)), K(2.0))).get(); },
            TFP::SinOnly);

        sq_rule("cosh^2->sinh^2+1", NT::Cosh,
            [this](MathExpressionNode* th) { return add(pw(un(NT::Sinh, W(th)), K(2.0)), K(1.0)).get(); },
            TFP::SinOnly);

        sq_rule("sin^2->1-cos^2",   NT::Sin,
            [this](MathExpressionNode* th) { return sub(K(1.0), pw(un(NT::Cos,  W(th)), K(2.0))).get(); },
            TFP::CosOnly);

        sq_rule("sinh^2->cosh^2-1", NT::Sinh,
            [this](MathExpressionNode* th) { return sub(pw(un(NT::Cosh, W(th)), K(2.0)), K(1.0)).get(); },
            TFP::CosOnly);

        // Inverses
        struct InvPair { NT outer, inner; };
        static constexpr InvPair inv_pairs[] = {
            { NT::Arcsin,  NT::Sin  }, { NT::Arccos,  NT::Cos  },
            { NT::Arctan,  NT::Tan  }, { NT::Arcsinh, NT::Sinh },
            { NT::Arccosh, NT::Cosh }, { NT::Arctanh, NT::Tanh },
            { NT::Arccsc,  NT::Csc  }, { NT::Arcsec,  NT::Sec  },
            { NT::Arccot,  NT::Cot  }, { NT::Arccsch, NT::Csch },
            { NT::Arcsech, NT::Sech }, { NT::Arccoth, NT::Coth },
        };
        for (auto& p : inv_pairs) {
            node_rules_.push_back({ "arcf(f(x))->x",
                [p](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (nd->type != p.outer) return nullptr;
                    if (!NodeKeyHash::is_unary(p.inner) || nd->unary.child->type != p.inner) return nullptr;
                    return nd->unary.child->unary.child;
                },
                [](MathExpressionNode* th) { return th; }
            });
            node_rules_.push_back({ "f(arcf(x))->x",
                [p](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (!NodeKeyHash::is_unary(p.inner) || nd->type != p.inner) return nullptr;
                    if (nd->unary.child->type != p.outer) return nullptr;
                    return nd->unary.child->unary.child;
                },
                [](MathExpressionNode* th) { return th; }
            });
        }

        struct ShiftPair { NT func; NT cofunc; };
        static constexpr ShiftPair shift_pairs[] = {
            { NT::Sin, NT::Cos }, { NT::Cos, NT::Sin },
            { NT::Tan, NT::Cot }, { NT::Cot, NT::Tan },
            { NT::Sec, NT::Csc }, { NT::Csc, NT::Sec },
        };

        for (auto& p : shift_pairs) {
            node_rules_.push_back({
                "f(pi/2-x)->cofunc(x)",
                [p](MathExpressionNode* nd) -> MathExpressionNode* {
                    if (nd->type != p.func) return nullptr;
                    return match_pi2_minus(nd->unary.child);
                },
                [this, p](MathExpressionNode* x) -> MathExpressionNode* {
                    return un(p.cofunc, W(x)).get();
                }
            });
        }

        struct ArcPair { NT a; NT b; };
        static constexpr ArcPair arc_pairs[] = {
            { NT::Arcsin,  NT::Arccos  },
            { NT::Arctan,  NT::Arccot  },
            { NT::Arcsec,  NT::Arccsc  },
        };

        for (auto& ap : arc_pairs) {
            sum_rules_.push_back({
                "arcf(x)+arcg(x)->pi/2",
                [ap, this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                    for (int i = 0; i < (int)ent.size(); ++i) {
                        auto* ki = ent[i].kernel;
                        if (ki->type != ap.a && ki->type != ap.b) continue;
                        NT need = (ki->type == ap.a) ? ap.b : ap.a;
                        MathExpressionNode* xi = ki->unary.child;
                        for (int j = i + 1; j < (int)ent.size(); ++j) {
                            auto* kj = ent[j].kernel;
                            if (kj->type != need) continue;
                            if (!same(xi, kj->unary.child)) continue;
                            if (!near_eq(ent[i].coeff, ent[j].coeff)) continue;
                            double c = ent[i].coeff;
                            ent.erase(ent.begin() + j);
                            ent.erase(ent.begin() + i);
                            cs += c * constants::pi_2();
                            return rebuild_add_entries(cs, ent);
                        }
                    }
                    return nullptr;
                }
            });
        }

        // Sum-level pythagorean
        // ca*cos^2(x) + cb*sin^2(x)
        // ca == cb              -> cs += ca          
        // ca == -cb             -> ca*cos(2x)       
        // SinOnly               -> ca + (cb-ca)*sin^2
        // CosOnly / Auto / rest -> cb + (ca-cb)*cos^2
        sum_rules_.push_back({ "ca*cos^2+cb*sin^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                MathExpressionNode* theta = nullptr;
                int ci = find_sq(ent, NT::Cos, &theta);  if (ci < 0) return nullptr;
                int si = find_sq(ent, NT::Sin, theta);   if (si < 0) return nullptr;
                double ca = ent[ci].coeff, cb = ent[si].coeff;
                erase_pair(ent, ci, si);
                if (near_eq(ca, cb)) {
                    cs += ca;
                } else if (near_eq(ca, -cb)) {
                    ent.push_back({ un(NT::Cos, mul(K(2.0), W(theta))).get(), ca });
                } else if (cfg_.trig_form == TFP::SinOnly) {
                    cs += ca;
                    ent.push_back({ pw(un(NT::Sin, W(theta)), K(2.0)).get(), cb - ca });
                } else {
                    cs += cb;
                    ent.push_back({ pw(un(NT::Cos, W(theta)), K(2.0)).get(), ca - cb });
                }
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct*tan^2(x) + ct = ct*sec^2(x)
        sum_rules_.push_back({ "ct*tan^2+ct->ct*sec^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int ti = find_sq(ent, NT::Tan, &theta);  if (ti < 0) return nullptr;
                if (!near_eq(ent[ti].coeff, cs)) return nullptr;
                double ct = cs;
                ent.erase(ent.begin() + ti);  cs = 0.0;
                ent.push_back({ pw(un(NT::Sec, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct*sec^2(x) − ct = ct*tan^2(x)  [PowerReduced only]
        sum_rules_.push_back({ "ct*sec^2-ct->ct*tan^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int si = find_sq(ent, NT::Sec, &theta);  if (si < 0) return nullptr;
                if (!near_eq(ent[si].coeff, -cs)) return nullptr;
                double ct = ent[si].coeff;
                ent.erase(ent.begin() + si);  cs = 0.0;
                ent.push_back({ pw(un(NT::Tan, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            },
            TFP::PowerReduced
        });

        // ct*cot^2(x) + ct = ct*csc^2(x)
        sum_rules_.push_back({ "ct*cot^2+ct->ct*csc^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int ti = find_sq(ent, NT::Cot, &theta);  if (ti < 0) return nullptr;
                if (!near_eq(ent[ti].coeff, cs)) return nullptr;
                double ct = cs;
                ent.erase(ent.begin() + ti);  cs = 0.0;
                ent.push_back({ pw(un(NT::Csc, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            }
        });

        // ca*cosh^2(x) + cb*sinh^2(x)
        //   ca == -cb  ->  cs += ca          (cosh^2-sinh^2=1)
        //   ca ==  cb  ->  ca*cosh(2x)       (cosh^2+sinh^2 = cosh(2x))
        //   else       ->  cs += ca,  (ca+cb)*sinh^2
        sum_rules_.push_back({ "ca*cosh^2+cb*sinh^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                MathExpressionNode* theta = nullptr;
                int ci = find_sq(ent, NT::Cosh, &theta);  if (ci < 0) return nullptr;
                int si = find_sq(ent, NT::Sinh, theta);   if (si < 0) return nullptr;
                double ca = ent[ci].coeff, cb = ent[si].coeff;
                erase_pair(ent, ci, si);
                if (near_eq(ca, -cb)) {
                    cs += ca;
                } else if (near_eq(ca, cb)) {
                    ent.push_back({ un(NT::Cosh, mul(K(2.0), W(theta))).get(), ca });
                } else {
                    cs += ca;
                    if (!near_zero(ca + cb)) ent.push_back({ pw(un(NT::Sinh, W(theta)), K(2.0)).get(), ca + cb });
                }
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct*sinh^2(x) + ct = ct*cosh^2(x)    i.e. sinh^2+1 = cosh^2
        sum_rules_.push_back({ "ct*sinh^2+ct->ct*cosh^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int si = find_sq(ent, NT::Sinh, &theta);  if (si < 0) return nullptr;
                if (!near_eq(ent[si].coeff, cs)) return nullptr;
                double ct = cs;
                ent.erase(ent.begin() + si);  cs = 0.0;
                ent.push_back({ pw(un(NT::Cosh, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct − ct*tanh^2(x) = ct*sech^2(x)    i.e. 1-tanh^2=sech^2
        sum_rules_.push_back({ "ct-ct*tanh^2->ct*sech^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int ti = find_sq(ent, NT::Tanh, &theta);  if (ti < 0) return nullptr;
                if (!near_eq(ent[ti].coeff, -cs)) return nullptr;
                double ct = cs;
                ent.erase(ent.begin() + ti);  cs = 0.0;
                ent.push_back({ pw(un(NT::Sech, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct*tanh^2(x) + ct*sech^2(x) = ct    tanh^2+sech^2=1 direction
        sum_rules_.push_back({ "ct*tanh^2+ct*sech^2->ct",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                MathExpressionNode* theta = nullptr;
                int ti = find_sq(ent, NT::Tanh, &theta);  if (ti < 0) return nullptr;
                int si = find_sq(ent, NT::Sech, theta);   if (si < 0) return nullptr;
                if (!near_eq(ent[ti].coeff, ent[si].coeff)) return nullptr;
                double ct = ent[ti].coeff;
                erase_pair(ent, ti, si);
                cs += ct;
                return rebuild_add_entries(cs, ent);
            }
        });

        // ca*coth^2(x) − ca*csch^2(x) = ca    (coth^2-csch^2=1)
        sum_rules_.push_back({ "ca*coth^2-ca*csch^2->ca",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                MathExpressionNode* theta = nullptr;
                int ci = find_sq(ent, NT::Coth, &theta);  if (ci < 0) return nullptr;
                int si = find_sq(ent, NT::Csch, theta);   if (si < 0) return nullptr;
                if (!near_eq(ent[ci].coeff, -ent[si].coeff)) return nullptr;
                double ct = ent[ci].coeff;
                erase_pair(ent, ci, si);
                cs += ct;
                return rebuild_add_entries(cs, ent);
            }
        });

        // ct*csch^2(x) + ct = ct*coth^2(x)  (csch^2+1=coth^2) 
        sum_rules_.push_back({ "ct*csch^2+ct->ct*coth^2",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                if (near_zero(cs)) return nullptr;
                MathExpressionNode* theta = nullptr;
                int si = find_sq(ent, NT::Csch, &theta);  if (si < 0) return nullptr;
                if (!near_eq(ent[si].coeff, cs)) return nullptr;
                double ct = cs;
                ent.erase(ent.begin() + si);  cs = 0.0;
                ent.push_back({ pw(un(NT::Coth, W(theta)), K(2.0)).get(), ct });
                return rebuild_add_entries(cs, ent);
            }
        });

        // sin(A)*cos(B) + sin(B)*cos(A) = sin(A+B) 
        sum_rules_.push_back({ "sin(A)cos(B)+sin(B)cos(A)->sin(A+B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *su = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Sin, NT::Cos, &su, &cv)) continue;
                    for (int j = i+1; j < (int)ent.size(); ++j) {
                        if (!near_eq(ent[i].coeff, ent[j].coeff)) continue;
                        MathExpressionNode *sv = nullptr, *cu = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sin, NT::Cos, &sv, &cu)) continue;
                        if (!same(su, cu) || !same(sv, cv)) continue;
                        double c = ent[i].coeff;
                        ent.erase(ent.begin()+j); ent.erase(ent.begin()+i);
                        auto* sum_arg = add(W(su), W(sv)).get();
                        ent.push_back({ un(NT::Sin, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // sin(A)*cos(B) - sin(B)*cos(A) = sin(A-B) 
        sum_rules_.push_back({ "sin(A)cos(B)+sin(B)cos(A)->sin(A+B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *su = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Sin, NT::Cos, &su, &cv)) continue;
                    for (int j = i+1; j < (int)ent.size(); ++j) {
                        if (!near_eq(ent[i].coeff, -ent[j].coeff)) continue;
                        MathExpressionNode *sv = nullptr, *cu = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sin, NT::Cos, &sv, &cu)) continue;
                        if (!same(su, cu) || !same(sv, cv)) continue;
                        double c = ent[i].coeff;
                        ent.erase(ent.begin()+j); ent.erase(ent.begin()+i);
                        auto* sum_arg = sub(W(su), W(sv)).get();
                        ent.push_back({ un(NT::Sin, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // cos(A)*cos(B) − sin(A)*sin(B) = cos(A+B) 
        sum_rules_.push_back({ "cos(A)cos(B)-sin(A)sin(B)->cos(A+B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *cu = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Cos, NT::Cos, &cu, &cv)) continue;
                    for (int j = 0; j < (int)ent.size(); ++j) {
                        if (j == i) continue;
                        if (!near_eq(ent[i].coeff, -ent[j].coeff)) continue;
                        MathExpressionNode *su = nullptr, *sv = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sin, NT::Sin, &su, &sv)) continue;
                        if (!(same(su, cu) && same(sv, cv)) && !(same(su, cv) && same(sv, cu))) continue;
                        double c = ent[i].coeff;
                        int hi = std::max(i,j), lo = std::min(i,j);
                        ent.erase(ent.begin()+hi); ent.erase(ent.begin()+lo);
                        auto* sum_arg = add(W(cu), W(cv)).get();
                        ent.push_back({ un(NT::Cos, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // cos(A)*cos(B) + sin(A)*sin(B) = cos(A−B) 
        sum_rules_.push_back({ "cos(A)cos(B)+sin(A)sin(B)->cos(A-B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *cu = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Cos, NT::Cos, &cu, &cv)) continue;
                    for (int j = 0; j < (int)ent.size(); ++j) {
                        if (j == i) continue;
                        if (!near_eq(ent[i].coeff, ent[j].coeff)) continue;
                        MathExpressionNode *su = nullptr, *sv = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sin, NT::Sin, &su, &sv)) continue;
                        if (!(same(su, cu) && same(sv, cv)) && !(same(su, cv) && same(sv, cu))) continue;
                        double c = ent[i].coeff;
                        int hi = std::max(i,j), lo = std::min(i,j);
                        ent.erase(ent.begin()+hi); ent.erase(ent.begin()+lo);
                        auto* diff_arg = sub(W(cu), W(cv)).get();
                        ent.push_back({ un(NT::Cos, W(diff_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // sinh(A)*cosh(B) + cosh(A)*sinh(B) = sinh(A+B)
        sum_rules_.push_back({ "sinh(A)cosh(B)+cosh(A)sinh(B)->sinh(A+B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *su = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Sinh, NT::Cosh, &su, &cv)) continue;
                    for (int j = i+1; j < (int)ent.size(); ++j) {
                        if (!near_eq(ent[i].coeff, ent[j].coeff)) continue;
                        MathExpressionNode *sv = nullptr, *cu = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sinh, NT::Cosh, &sv, &cu)) continue;
                        if (!same(su, cu) || !same(sv, cv)) continue;
                        double c = ent[i].coeff;
                        ent.erase(ent.begin()+j); ent.erase(ent.begin()+i);
                        auto* sum_arg = add(W(su), W(sv)).get();
                        ent.push_back({ un(NT::Sinh, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // sinh(A)*cosh(B) - cosh(A)*sinh(B) = sinh(A-B)
        sum_rules_.push_back({ "sinh(A)cosh(B)-cosh(A)sinh(B)->sinh(A-B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *su = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Sinh, NT::Cosh, &su, &cv)) continue;
                    for (int j = i+1; j < (int)ent.size(); ++j) {
                        if (!near_eq(ent[i].coeff, -ent[j].coeff)) continue;  // opposite signs
                        MathExpressionNode *sv = nullptr, *cu = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sinh, NT::Cosh, &sv, &cu)) continue;
                        if (!same(su, cu) || !same(sv, cv)) continue;
                        double c = ent[i].coeff;
                        ent.erase(ent.begin()+j); ent.erase(ent.begin()+i);
                        auto* sum_arg = sub(W(su), W(sv)).get();
                        ent.push_back({ un(NT::Sinh, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // cosh(A)*cosh(B) + sinh(A)*sinh(B) = cosh(A+B)  [NOTE: sign is +, unlike cos(A+B)]
        sum_rules_.push_back({ "cosh(A)cosh(B)+sinh(A)sinh(B)->cosh(A+B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *cu = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Cosh, NT::Cosh, &cu, &cv)) continue;
                    for (int j = 0; j < (int)ent.size(); ++j) {
                        if (j == i) continue;
                        if (!near_eq(ent[i].coeff, ent[j].coeff)) continue;  // same sign (+ unlike trig)
                        MathExpressionNode *su = nullptr, *sv = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sinh, NT::Sinh, &su, &sv)) continue;
                        if (!(same(su, cu) && same(sv, cv)) && !(same(su, cv) && same(sv, cu))) continue;
                        double c = ent[i].coeff;
                        int hi = std::max(i,j), lo = std::min(i,j);
                        ent.erase(ent.begin()+hi); ent.erase(ent.begin()+lo);
                        auto* sum_arg = add(W(cu), W(cv)).get();
                        ent.push_back({ un(NT::Cosh, W(sum_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });

        // cosh(A)*cosh(B) - sinh(A)*sinh(B) = cosh(A-B)
        sum_rules_.push_back({ "cosh(A)cosh(B)-sinh(A)sinh(B)->cosh(A-B)",
            [this](double& cs, std::vector<AddEntry>& ent) -> MathExpressionNode* {
                for (int i = 0; i < (int)ent.size(); ++i) {
                    MathExpressionNode *cu = nullptr, *cv = nullptr;
                    if (!is_sincos_product(ent[i].kernel, NT::Cosh, NT::Cosh, &cu, &cv)) continue;
                    for (int j = 0; j < (int)ent.size(); ++j) {
                        if (j == i) continue;
                        if (!near_eq(ent[i].coeff, -ent[j].coeff)) continue;  // opposite signs
                        MathExpressionNode *su = nullptr, *sv = nullptr;
                        if (!is_sincos_product(ent[j].kernel, NT::Sinh, NT::Sinh, &su, &sv)) continue;
                        if (!(same(su, cu) && same(sv, cv)) && !(same(su, cv) && same(sv, cu))) continue;
                        double c = ent[i].coeff;
                        int hi = std::max(i,j), lo = std::min(i,j);
                        ent.erase(ent.begin()+hi); ent.erase(ent.begin()+lo);
                        auto* diff_arg = sub(W(cu), W(cv)).get();
                        ent.push_back({ un(NT::Cosh, W(diff_arg)).get(), c });
                        return rebuild_add_entries(cs, ent);
                    }
                }
                return nullptr;
            }
        });
    }

auto MathExpressionRewriter::is_sincos_product(
        MathExpressionNode* nd,
        NodeType ta, NodeType tb,
        MathExpressionNode** u_out,
        MathExpressionNode** v_out
) -> bool {
        if (nd->type != NodeType::Multiply) return false;
        auto* l = nd->binary.left;
        auto* r = nd->binary.right;
        if (l->type == ta && r->type == tb) { *u_out = l->unary.child; *v_out = r->unary.child; return true; }
        if (r->type == ta && l->type == tb) { *u_out = r->unary.child; *v_out = l->unary.child; return true; }
        return false;
    }

auto MathExpressionRewriter::rule_trig(MathExpressionNode* n) -> MathExpressionNode* {
        for (auto& r : node_rules_) {
            if (!node_rule_active(r)) continue;
            MathExpressionNode* theta = r.match(n);
            if (!theta) continue;
            MathExpressionNode* result = r.build(theta);
            if (result && result != n) return result;
        }
        if (!cfg_.do_trig_pythagorean) return n;
        if (n->type != NodeType::Add && n->type != NodeType::Subtract) return n;
        double cs = 0.0;
        std::vector<AddEntry> ent;
        collect_add(n, 1.0, cs, ent);
        for (auto& r : sum_rules_) {
            if (!sum_rule_active(r)) continue;
            MathExpressionNode* result = r.apply(cs, ent);
            if (result) return result;
        }
        return n;
    }

auto MathExpressionRewriter::rule_log(MathExpressionNode* n) -> MathExpressionNode* {
        using NT = NodeType;
        // ln(a) + ln(b)  ->  ln(a*b)
        if (n->type == NT::Add && n->binary.left->type  == NT::NaturalLog && n->binary.right->type == NT::NaturalLog) return un(NT::NaturalLog, mul(W(n->binary.left->unary.child), W(n->binary.right->unary.child))).get();
        // ln(a) - ln(b)  ->  ln(a/b)
        if (n->type == NT::Subtract && n->binary.left->type  == NT::NaturalLog && n->binary.right->type == NT::NaturalLog) return un(NT::NaturalLog, dv(W(n->binary.left->unary.child), W(n->binary.right->unary.child))).get();
        // k * ln(x)  ->  ln(x^k)
        if (n->type == NT::Multiply) {
            if (isK(n->binary.left) && n->binary.right->type == NT::NaturalLog) return un(NT::NaturalLog, pw(W(n->binary.right->unary.child), W(n->binary.left))).get();
            if (isK(n->binary.right) && n->binary.left->type == NT::NaturalLog) return un(NT::NaturalLog, pw(W(n->binary.left->unary.child), W(n->binary.right))).get();
        }
        // ln(e^x)  ->  x
        if (n->type == NT::NaturalLog && n->unary.child->type == NT::NaturalExp) return n->unary.child->unary.child;
        // e^(ln(x))  ->  x
        if (n->type == NT::NaturalExp && n->unary.child->type == NT::NaturalLog) return n->unary.child->unary.child;
        // e^a / e^b  ->  e^(a-b)
        if (n->type == NT::Divide && n->binary.left->type  == NT::NaturalExp && n->binary.right->type == NT::NaturalExp) return un(NT::NaturalExp, sub(W(n->binary.left->unary.child), W(n->binary.right->unary.child))).get();
        return n;
    }

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
