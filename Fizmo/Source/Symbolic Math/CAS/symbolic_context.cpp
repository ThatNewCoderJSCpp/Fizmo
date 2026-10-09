#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

SymbolicContext::SymbolicContext(std::size_t arena_block_size) : arena_(arena_block_size),
          mgr_(arena_, vars_),
          simp_(mgr_),
          diff_(mgr_, vars_, simp_),
          subst_(mgr_, vars_, simp_),
          rewriter_(mgr_, simp_),
          integ_(mgr_, vars_, simp_, diff_)
    {}

auto SymbolicContext::substitute(const Expression& e, const std::unordered_map<std::string, Expression>& named) -> Expression {
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& [k, v] : named) inner.emplace(k, v.inner());
        return Expression(subst_.substitute(e.inner(), inner), mgr_);
    }

auto SymbolicContext::substitute(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) -> Expression {
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

auto SymbolicContext::substitute(const Expression& e, std::initializer_list<Expression> positional) -> Expression {
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

auto SymbolicContext::substitute(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) -> Expression {
        std::vector<std::pair<symbols::MathExpression, symbols::MathExpression>> v;
        v.reserve(pairs.size());
        for (auto& [f, t] : pairs) v.emplace_back(f.inner(), t.inner());
        symbols::SubstitutionMap smap;
        for (auto& [f, t] : v) smap.by_node[f.get()] = t.get();
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

auto SymbolicContext::partial_evaluate(const Expression& e, const std::unordered_map<std::string, Expression>& named) -> Expression {
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& [k, v] : named) inner.emplace(k, v.inner());
        return Expression(subst_.substitute(e.inner(), inner), mgr_);
    }

auto SymbolicContext::partial_evaluate(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) -> Expression {
        symbols::SubstitutionMap smap;
        for (auto& [name, expr] : pairs) {
            auto id = vars_.get(name);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = expr.inner().get();
        }
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

auto SymbolicContext::partial_evaluate(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) -> Expression {
        symbols::SubstitutionMap smap;
        for (auto& [f, t] : pairs) smap.by_node[f.inner().get()] = t.inner().get();
        return Expression(subst_.substitute(e.inner(), smap), mgr_);
    }

auto SymbolicContext::rewrite(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg) -> Expression {
        symbols::MathExpressionRewriter rw(mgr_, simp_, cfg);
        return Expression(rw.rewrite(e.inner()), mgr_);
    }

auto SymbolicContext::full_simplify(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg) -> Expression {
        symbols::MathExpression s = simp_.simplify(e.inner());
        symbols::MathExpressionRewriter rw(mgr_, simp_, cfg);
        return Expression(rw.rewrite(s), mgr_);
    }

auto SymbolicContext::integrate(const Expression& e, const std::string& var) -> Expression {
        symbols::MathExpression result = integ_.integrate(e.inner(), var);
        if (!result) return Expression{}; // integration failed
        return Expression(result, mgr_);
    }

} // namespace cas
} // namespace math
} // namespace fizmo
