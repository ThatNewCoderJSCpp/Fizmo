#ifndef FIZMO_MATH_EXPRESSION_WRAPPER_HPP
#define FIZMO_MATH_EXPRESSION_WRAPPER_HPP

#include "expression.hpp"
#include "simplifier.hpp"
#include "differentiator.hpp"
#include "evaluator.hpp"
#include "printer.hpp"
#include "rewriter.hpp"
#include "substituter.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <cassert>
#include <type_traits>

namespace fizmo {
namespace math {
namespace cas {

using RewriterConfig = symbols::MathExpressionRewriter::Config;

class ExpressionLambda;

class Expression {
public:
    Expression() = default;
    explicit Expression(symbols::MathExpression expr, symbols::MathExpressionManager& mgr) : expr_(expr), mgr_(&mgr), vars_(&mgr.variables()) {}
    explicit Expression(symbols::MathExpressionNode* node, symbols::MathExpressionManager& mgr) : expr_(mgr.canonicalize(node)), mgr_(&mgr), vars_(&mgr.variables()) {}
    
    explicit Expression(double value, symbols::MathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) {
        using NT = symbols::NodeType;

        if (std::isnan(value)) {
            expr_ = mgr.nan_expr();
        } else if (std::isinf(value)) {
            expr_ = value > 0 ? mgr.pos_inf() : mgr.neg_inf();
        } else {
            expr_ = mgr.constant(value);
        }
    }

    explicit Expression(const std::string& var_name, symbols::MathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) { expr_ = mgr.variable(var_name); }
    
    symbols::MathExpression      inner() const noexcept { return expr_; }
    symbols::MathExpressionNode* node()  const noexcept { return expr_.get(); }

    bool is_invalid() const noexcept { return expr_.get() == nullptr || mgr_ == nullptr; }

    static Expression invalid() noexcept {
        Expression e;
        e.mgr_  = nullptr;
        e.vars_ = nullptr;
        return e;
    }

    symbols::MathExpressionManager& manager() const {
        assert(mgr_ && "Expression has no associated MathExpressionManager"); 
        return *mgr_; 
    }

    explicit operator bool() const noexcept { return !is_invalid(); }
    bool operator==(const Expression& o) const noexcept { return expr_.get() == o.expr_.get(); }
    bool operator!=(const Expression& o) const noexcept { return !(*this == o); }

    bool symbolic_equals(const Expression& o) const {
        if (expr_.get() == o.expr_.get()) return true;         
        if (is_invalid() || o.is_invalid()) return false;
        symbols::MathExpressionSimplifier simp(manager());

        symbols::MathExpression diff = simp.simplify(
            manager().binary(symbols::NodeType::Subtract, expr_, o.expr_)
        );

        symbols::MathExpressionRewriter rw(manager(), simp);
        diff = rw.rewrite(diff);
        return diff.get()->type == symbols::NodeType::Constant && diff.get()->constant == 0.0;
    }

    bool is_symbolically_zero() const {
        if (is_invalid()) return false;
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionRewriter   rw(manager(), simp);
        auto s = rw.rewrite(simp.simplify(expr_));
        return s.get()->type == symbols::NodeType::Constant && s.get()->constant == 0.0;
    }

    friend std::ostream& operator<<(std::ostream& os, const Expression& e) {
        symbols::print_node(os, e.node(), e.vars_, nullptr);
        return os;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
    }

    bool depends_on(const std::string& var_name) const {
        std::uint64_t id = vars_->get(var_name);
        if (id == symbols::VariableTable::invalid_id) return false;
        return contains_var(expr_.get(), id);
    }

    bool depends_on(const Expression& sub) const { return contains_node(expr_.get(), sub.inner().get()); }

    std::vector<std::string> variable_names() const {
        std::vector<std::uint64_t> ids;
        collect_var_ids(expr_.get(), ids);
        std::vector<std::string> names;
        names.reserve(ids.size());

        for (auto id : ids) {
            const auto& n = vars_->name(id);
            if (!n.empty()) names.push_back(n);
        }

        return names;
    }

    double evaluate(const symbols::EvalContext& ctx) const {
        assert(*this);
        return symbols::evaluate(expr_, ctx);
    }

    double evaluate(std::initializer_list<double> vals, EvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();  
        symbols::EvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());
        std::size_t i = 0;

        for (double v : vals) {
            if (i >= vars.size()) break;
            ctx.values[i] = v;
            ctx.assigned[i] = true;
            ++i;
        }

        return symbols::evaluate(expr_, ctx);
    }

    double evaluate(const std::unordered_map<std::string, double>& named, EvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();
        symbols::EvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());

        for (auto& [name, value] : named) {
            std::uint64_t id = vars.get(name);

            if (id != symbols::VariableTable::invalid_id && id < ctx.values.size()) {
                ctx.values[id] = value;
                ctx.assigned[id] = true;
            }
        }

        return symbols::evaluate(expr_, ctx);
    }

    double evaluate(std::initializer_list<std::pair<std::string, double>> vals, EvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();
        symbols::EvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());

        for (auto& val : vals) {
            std::uint64_t id = vars.get(val.first);

            if (id != symbols::VariableTable::invalid_id && id < ctx.values.size()) {
                ctx.values[id] = val.second;
                ctx.assigned[id] = true;
            }
        }

        return symbols::evaluate(expr_, ctx);
    }

    Expression rewrite(const RewriterConfig& cfg) const {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionRewriter   rw(manager(), simp, cfg);
        return Expression(rw.rewrite(expr_), manager());
    }

    Expression rewrite() const { return rewrite(symbols::MathExpressionRewriter::Config{}); }

    Expression full_simplify(const RewriterConfig& cfg) const {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpression s = simp.simplify(expr_);
        symbols::MathExpressionRewriter   rw(manager(), simp, cfg);
        return Expression(rw.rewrite(s), manager());
    }

    Expression full_simplify() const { return full_simplify(symbols::MathExpressionRewriter::Config{}); }

    Expression substitute(const std::string& var, double val) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, val), manager());
    }
    
    Expression substitute(const std::string& var, const Expression& repl) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, repl.inner()), manager());
    }

    Expression substitute(std::initializer_list<std::pair<std::string, double>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, pairs), manager());
    }

    Expression substitute(std::initializer_list<std::pair<std::string, Expression>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;

        for (auto& pair : pairs) {
            auto id = manager().variables().get(pair.first);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = pair.second.inner().get();
        }

        return Expression(sub.substitute(expr_, smap), manager());
    }

    Expression substitute(const std::unordered_map<std::string, double>& named) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, named), manager());
    }

    Expression substitute(const std::unordered_map<std::string, Expression>& named) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& name : named) inner.emplace(name.first, name.second.inner());
        return Expression(sub.substitute(expr_, inner), manager());
    }
    
    Expression substitute(std::initializer_list<double> positional) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, positional), manager());
    }

    Expression substitute(std::initializer_list<Expression> positional) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;
        std::uint64_t id = 0;

        for (auto& p : positional) {
            if (id >= manager().variables().size()) break;
            smap.by_var[id] = p.inner().get();
            ++id;
        }

        return Expression(sub.substitute(expr_, smap), manager());
    }

    Expression substitute(const Expression& from, const Expression& to) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, from.inner(), to.inner()), manager());
    }
    
    Expression substitute(std::initializer_list<std::pair<Expression, Expression>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;
        for (auto& pair : pairs) smap.by_node[pair.first.inner().get()] = pair.second.inner().get();
        return Expression(sub.substitute(expr_, smap), manager());
    }

    Expression partial_evaluate(const std::string& var, double val) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, val), manager());
    }
    
    Expression partial_evaluate(const std::string& var, const Expression& repl) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, repl.inner()), manager());
    }

    Expression partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;

        for (auto& pair : pairs) {
            auto id = manager().variables().get(pair.first);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = manager().constant(pair.second).get();
        }

        return Expression(sub.substitute(expr_, smap), manager());
    }
    
    Expression partial_evaluate(std::initializer_list<std::pair<std::string, Expression>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;

        for (auto& pair : pairs) {
            auto id = manager().variables().get(pair.first);
            if (id != symbols::VariableTable::invalid_id) smap.by_var[id] = pair.second.inner().get();
        }

        return Expression(sub.substitute(expr_, smap), manager());
    }

    Expression partial_evaluate(const std::unordered_map<std::string, double>& named) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, named), manager());
    }

    Expression partial_evaluate(const std::unordered_map<std::string, Expression>& named) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (const auto& name : named) inner.emplace(name.first, name.second.inner());
        return Expression(sub.substitute(expr_, inner), manager());
    }

    Expression partial_evaluate(const Expression& from, const Expression& to) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, from.inner(), to.inner()), manager());
    }

    Expression partial_evaluate(std::initializer_list<std::pair<Expression, Expression>> pairs) const {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;
        for (auto& pair : pairs) smap.by_node[pair.first.inner().get()] = pair.second.inner().get();
        return Expression(sub.substitute(expr_, smap), manager());
    }

    Expression differentiate(const std::string& var) const {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.derivative(inner(), var), manager());
    }

    Expression nth_derivative_iterative(const std::string& var, unsigned int order = 1) const {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.nth_derivative_iterative(inner(), var, order), manager());
    }

    Expression nth_derivative_recursive(const std::string& var, unsigned int order = 1) const {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.nth_derivative_recursive(inner(), var, order), manager());
    }

    std::size_t num_nodes() const { return count_nodes(node()); }

    ExpressionLambda to_lambda() const;

    template <std::size_t N>
    auto to_function() const {
        assert(*this);
        return to_function_impl(std::make_index_sequence<N>{});
    }

    std::function<double(const std::vector<double>&)> to_function_runtime(EvaluationPolicy policy = {}) const {
        assert(*this);
        Expression captured = *this;

        return [captured, policy](const std::vector<double>& args) -> double {
            const auto& vars = captured.manager().variables();
            symbols::EvalContext ctx;
            ctx.policy = policy;
            ctx.resize(vars.size());

            for (std::size_t i = 0; i < args.size() && i < ctx.values.size(); ++i) {
                ctx.values[i]   = args[i];
                ctx.assigned[i] = true;
            }

            return symbols::evaluate(captured.inner(), ctx);
        };
    }

private:
    static std::size_t count_nodes(const symbols::MathExpressionNode* n) {
        if (!n) return 0;

        switch (n->type) {
            case symbols::NodeType::Constant:
            case symbols::NodeType::Variable:
            case symbols::NodeType::PositiveInfinity:
            case symbols::NodeType::NegativeInfinity:
            case symbols::NodeType::NaN:
            case symbols::NodeType::Undefined:
            case symbols::NodeType::Indeterminate:
                return 1;
            case symbols::NodeType::Power:
                return 1 + count_nodes(n->power.base) + count_nodes(n->power.exponent);
            default:
                if (symbols::NodeKeyHash::is_unary(n->type)) { return 1 + count_nodes(n->unary.child); }
                if (symbols::NodeKeyHash::is_binary(n->type)) { return 1 + count_nodes(n->binary.left) + count_nodes(n->binary.right); }
                return 1;
        }
    }

    static bool contains_var(const symbols::MathExpressionNode* n, std::uint64_t vid) {
        if (!n) return false;

        switch (n->type) {
            case symbols::NodeType::Variable: 
                return n->variable.var_id == vid;
            case symbols::NodeType::AppliedFunction:
                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) if (contains_var(n->applied.args[i], vid)) return true;
                return false;
            case symbols::NodeType::Constant:
            case symbols::NodeType::PositiveInfinity:
            case symbols::NodeType::NegativeInfinity:
            case symbols::NodeType::NaN:
            case symbols::NodeType::Undefined:
            case symbols::NodeType::Indeterminate:
                return false;
            case symbols::NodeType::Power:
                return contains_var(n->power.base, vid) || contains_var(n->power.exponent, vid);
            default:
                if (symbols::NodeKeyHash::is_unary(n->type)) return contains_var(n->unary.child, vid);
                if (symbols::NodeKeyHash::is_binary(n->type)) return contains_var(n->binary.left, vid) || contains_var(n->binary.right, vid);
                return false;
        }
    }

    static bool contains_node(const symbols::MathExpressionNode* n, const symbols::MathExpressionNode* target) {
        if (!n) return false;
        if (n == target) return true;

        switch (n->type) {
            case symbols::NodeType::Constant:
            case symbols::NodeType::Variable:
            case symbols::NodeType::PositiveInfinity:
            case symbols::NodeType::NegativeInfinity:
            case symbols::NodeType::NaN:
            case symbols::NodeType::Undefined:
            case symbols::NodeType::Indeterminate:
                return false;
            case symbols::NodeType::AppliedFunction:
                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) if (contains_node(n->applied.args[i], target)) return true;
                return false;
            case symbols::NodeType::Power:
                return contains_node(n->power.base, target) || contains_node(n->power.exponent, target);
            default:
                if (symbols::NodeKeyHash::is_unary(n->type)) return contains_node(n->unary.child, target);
                if (symbols::NodeKeyHash::is_binary(n->type)) return contains_node(n->binary.left, target) || contains_node(n->binary.right, target);
                return false;
        }
    }

    static void collect_var_ids(const symbols::MathExpressionNode* n, std::vector<std::uint64_t>& out) {
        if (!n) return;

        switch (n->type) {
            case symbols::NodeType::Variable:
                if (std::find(out.begin(), out.end(), n->variable.var_id) == out.end()) out.push_back(n->variable.var_id);
                return;
            case symbols::NodeType::AppliedFunction:
                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) collect_var_ids(n->applied.args[i], out);
                return;
            case symbols::NodeType::Constant:
            case symbols::NodeType::PositiveInfinity:
            case symbols::NodeType::NegativeInfinity:
            case symbols::NodeType::NaN:
            case symbols::NodeType::Undefined:
            case symbols::NodeType::Indeterminate:
                return;
            case symbols::NodeType::Power:
                collect_var_ids(n->power.base,     out);
                collect_var_ids(n->power.exponent, out);
                return;
            default:
                if (symbols::NodeKeyHash::is_binary(n->type)) {
                    collect_var_ids(n->binary.left,  out);
                    collect_var_ids(n->binary.right, out);
                } else if (symbols::NodeKeyHash::is_unary(n->type)) {
                    collect_var_ids(n->unary.child, out);
                }
                return;
        }
    }

    template <std::size_t... Is>
    auto to_function_impl(std::index_sequence<Is...>) const {
        Expression captured = *this;
        return std::function<double(decltype((void)Is, double{})...)>(
            [captured](decltype((void)Is, double{})... args) -> double {
                return captured.evaluate({args...});
            }
        );
    }

private:
    symbols::MathExpression         expr_{};
    symbols::MathExpressionManager* mgr_{ nullptr };
    const symbols::VariableTable*   vars_{ nullptr };
};

class ExpressionLambda {
public:
    ExpressionLambda() = default;
    ExpressionLambda(Expression expr, std::vector<std::string> param_names) : expr_(std::move(expr)), param_names_(std::move(param_names)) {}
    ExpressionLambda(Expression expr) : expr_(std::move(expr)), param_names_(expr_.variable_names()) {}
 
    template <typename... Args, typename = typename std::enable_if<std::is_convertible<Args..., double>::value>::type>
    double operator()(Args... args) const { return expr_.evaluate({static_cast<double>(args)...}); }

    double operator()(std::initializer_list<std::pair<std::string,double>> named) const { return expr_.evaluate(named); }
    double operator()(const std::unordered_map<std::string, double>& named) const { return expr_.evaluate(named); }

    const std::vector<std::string>& parameters() const noexcept { return param_names_; }
    std::size_t arity() const noexcept { return param_names_.size(); }
    const Expression& expression() const noexcept { return expr_; }
    Expression& expression() noexcept { return expr_; }

    bool is_invalid() const { return expr_.is_invalid(); }
    
    template <std::size_t N>
    auto to_function() { return expr_.to_function<N>(); }

    std::function<double(const std::vector<double>&)>
    to_function_runtime(EvaluationPolicy policy = {}) const { return expr_.to_function_runtime(policy); }

private:
    Expression                expr_;
    std::vector<std::string>  param_names_;
};

ExpressionLambda Expression::to_lambda() const {
    assert(*this);
    return ExpressionLambda(*this, variable_names());
}

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_EXPRESSION_WRAPPER_HPP