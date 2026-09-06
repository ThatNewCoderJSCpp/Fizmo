#ifndef FIZMO_SYMBOLIC_CONTEXT_HPP
#define FIZMO_SYMBOLIC_CONTEXT_HPP

#include "expression.hpp"
#include "simplifier.hpp"
#include "differentiator.hpp"
#include "evaluator.hpp"
#include "printer.hpp"
#include "expression_wrapper.hpp"
#include "substituter.hpp"
#include "rewriter.hpp"

#include "../Anti Differentiation/integrator.hpp"
#include "../Anti Differentiation/Cases/polynomial_rules.hpp"
#include "../Anti Differentiation/Cases/exponential_rules.hpp"
#include "../Anti Differentiation/Cases/logarithmic_rules.hpp"
#include "../Anti Differentiation/Cases/trigonometric_rules.hpp"
#include "../Anti Differentiation/Cases/hyperbolic_rules.hpp"
#include "../Anti Differentiation/Cases/algebraic_forms_rules.hpp"
#include "../Anti Differentiation/Cases/heuristic_rules.hpp"

namespace fizmo {
namespace math {
namespace cas {

class SymbolicContext {
public:
    explicit SymbolicContext(std::size_t arena_block_size = 64 * 1024)
        : arena_(arena_block_size),
          mgr_(arena_, vars_),
          simp_(mgr_),
          diff_(mgr_, vars_, simp_),
          subst_(mgr_, vars_, simp_),
          rewriter_(mgr_, simp_),
          integ_(mgr_, vars_, simp_, diff_)
    {}

    symbols::MathExpressionManager& manager() noexcept { return mgr_; }
    const symbols::MathExpressionManager& manager() const noexcept { return mgr_; }

    symbols::VariableTable& variables() noexcept { return vars_; }
    const symbols::VariableTable& variables() const noexcept { return vars_; }

    symbols::MathExpressionSimplifier& simplifier() noexcept { return simp_; }
    const symbols::MathExpressionSimplifier& simplifier() const noexcept { return simp_; }

    symbols::MathExpressionDifferentiator& differentiator() noexcept { return diff_; }
    const symbols::MathExpressionDifferentiator& differentiator() const noexcept { return diff_; }

    symbols::MathExpressionSubstitutor& substitutor() noexcept { return subst_; }
    const symbols::MathExpressionSubstitutor& substitutor() const noexcept { return subst_; }

    symbols::MathExpressionRewriter& rewriter() noexcept { return rewriter_; }
    const symbols::MathExpressionRewriter& rewriter() const noexcept { return rewriter_; }

    symbols::MathExpressionIntegrator& integrator() noexcept { return integ_; }
    const symbols::MathExpressionIntegrator& integrator() const noexcept { return integ_; }

    EvaluationPolicy& default_policy() { return default_policy_; } 
    const EvaluationPolicy& default_policy() const { return default_policy_; }

    Expression variable(const std::string& name) { return Expression(mgr_.variable(name), mgr_); }
    Expression constant(double v) { return Expression(mgr_.constant(v), mgr_); }
    Expression derivative(const Expression& e, const std::string& var) { return Expression(diff_.derivative(e.inner(), var), mgr_); }
    Expression nth_derivative_iterative(const Expression& e, const std::string& var, unsigned int order) { return Expression(diff_.nth_derivative_iterative(e.inner(), var, order), mgr_); }
    Expression nth_derivative_recursive(const Expression& e, const std::string& var, unsigned int order) { return Expression(diff_.nth_derivative_recursive(e.inner(), var, order), mgr_); }
    void reset_arena() { arena_.reset(); }

    Expression substitute(const Expression& e, const std::unordered_map<std::string, Expression>& named) {
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& [k, v] : named) inner.emplace(k, v.inner());
        return Expression(subst_.substitute(e.inner(), inner), mgr_);
    }

    Expression substitute(const Expression& e, const std::unordered_map<std::string, double>& named) { return Expression(subst_.substitute(e.inner(), named), mgr_); }

    Expression substitute(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) {
        std::initializer_list<std::pair<std::string, symbols::MathExpression>> converted; 
        std::vector<std::pair<std::string, symbols::MathExpression>> v;
        v.reserve(pairs.size());
        for (auto& [k, expr] : pairs) v.emplace_back(k, expr.inner());
        symbols::SubstitutionMap smap;
        for (auto& [name, me] : v) {
            auto id = variables().get(name);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = me.get();
        }
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

    Expression substitute(const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) { return Expression(subst_.substitute(e.inner(), pairs), mgr_); }

    Expression substitute(const Expression& e, std::initializer_list<Expression> positional) {
        std::vector<symbols::MathExpression> v;
        v.reserve(positional.size());
        for (auto& p : positional) v.push_back(p.inner());
        symbols::SubstitutionMap smap;
        std::uint64_t id = 0;
        for (auto& me : v) {
            if (id >= variables().size()) break;
            smap.by_var[id] = me.get();
            ++id;
        }
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

    Expression substitute(const Expression& e, std::initializer_list<double> positional) { return Expression(subst_.substitute(e.inner(), positional), mgr_); }
    Expression substitute(const Expression& e, const std::string& var_name, const Expression& repl) { return Expression(subst_.substitute(e.inner(), var_name, repl.inner()), mgr_); }
    Expression substitute(const Expression& e, const std::string& var_name, double val) { return Expression(subst_.substitute(e.inner(), var_name, val), mgr_); }
    Expression substitute(const Expression& e, const Expression& from, const Expression& to) { return Expression(subst_.substitute(e.inner(), from.inner(), to.inner()), mgr_); }
    
    Expression substitute(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) {
        std::vector<std::pair<symbols::MathExpression, symbols::MathExpression>> v;
        v.reserve(pairs.size());
        for (auto& [f, t] : pairs) v.emplace_back(f.inner(), t.inner());
        symbols::SubstitutionMap smap;
        for (auto& [f, t] : v) smap.by_node[f.get()] = t.get();
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

    Expression partial_evaluate(const Expression& e, const std::string& var, const Expression& repl) {
        return Expression(subst_.substitute(e.inner(), var, repl.inner()), mgr_);
    }

    Expression partial_evaluate(const Expression& e, const std::string& var, double val) {
        return Expression(subst_.substitute(e.inner(), var, val), mgr_);
    }

    Expression partial_evaluate(const Expression& e, const std::unordered_map<std::string, Expression>& named) {
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& [k, v] : named) inner.emplace(k, v.inner());
        return Expression(subst_.substitute(e.inner(), inner), mgr_);
    }

    Expression partial_evaluate(const Expression& e, const std::unordered_map<std::string, double>& named) {
        return Expression(subst_.substitute(e.inner(), named), mgr_);
    }

    Expression partial_evaluate(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) {
        symbols::SubstitutionMap smap;
        for (auto& [name, expr] : pairs) {
            auto id = vars_.get(name);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = expr.inner().get();
        }
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

    Expression partial_evaluate(const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) {
        return Expression(subst_.substitute(e.inner(), pairs), mgr_);
    }

    Expression partial_evaluate(const Expression& e, const Expression& from, const Expression& to) {
        return Expression(subst_.substitute(e.inner(), from.inner(), to.inner()), mgr_);
    }

    Expression partial_evaluate(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) {
        symbols::SubstitutionMap smap;
        for (auto& [f, t] : pairs) smap.by_node[f.inner().get()] = t.inner().get();
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

    Expression rewrite(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) {
        symbols::MathExpressionRewriter rw(mgr_, simp_, cfg);
        return Expression(rw.rewrite(e.inner()), mgr_);
    }

    Expression full_simplify(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) {
        symbols::MathExpression s = simp_.simplify(e.inner());
        symbols::MathExpressionRewriter rw(mgr_, simp_, cfg);
        return Expression(rw.rewrite(s), mgr_);
    }

    Expression integrate(const Expression& e, const std::string& var) {
        symbols::MathExpression result = integ_.integrate(e.inner(), var);
        if (!result) return Expression{}; // integration failed
        return Expression(result, mgr_);
    }

private:
    symbols::VariableTable vars_;
    detail::ExpressionArena arena_;
    symbols::MathExpressionManager mgr_;
    symbols::MathExpressionSimplifier simp_;
    symbols::MathExpressionDifferentiator diff_;
    symbols::MathExpressionSubstitutor subst_;
    symbols::MathExpressionRewriter rewriter_; 
    symbols::MathExpressionIntegrator integ_;
    EvaluationPolicy default_policy_;
};

inline SymbolicContext& global_context() {
    static SymbolicContext ctx;
    return ctx;
}

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_SYMBOLIC_CONTEXT_HPP
