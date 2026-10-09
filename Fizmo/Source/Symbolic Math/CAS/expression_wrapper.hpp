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
    
    explicit Expression(double value, symbols::MathExpressionManager& mgr);

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

    bool symbolic_equals(const Expression& o) const;

    bool is_symbolically_zero() const;

    friend std::ostream& operator<<(std::ostream& os, const Expression& e) {
        symbols::print_node(os, e.node(), e.vars_, nullptr);
        return os;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
    }

    bool depends_on(const std::string& var_name) const;

    bool depends_on(const Expression& sub) const { return contains_node(expr_.get(), sub.inner().get()); }

    std::vector<std::string> variable_names() const;

    double evaluate(const symbols::EvalContext& ctx) const {
        assert(*this);
        return symbols::evaluate(expr_, ctx);
    }

    double evaluate(std::initializer_list<double> vals, EvaluationPolicy policy = {}) const;

    double evaluate(const std::unordered_map<std::string, double>& named, EvaluationPolicy policy = {}) const;

    double evaluate(std::initializer_list<std::pair<std::string, double>> vals, EvaluationPolicy policy = {}) const;

    Expression rewrite(const RewriterConfig& cfg) const;

    Expression rewrite() const { return rewrite(symbols::MathExpressionRewriter::Config{}); }

    Expression full_simplify(const RewriterConfig& cfg) const;

    Expression full_simplify() const { return full_simplify(symbols::MathExpressionRewriter::Config{}); }

    Expression substitute(const std::string& var, double val) const;
    
    Expression substitute(const std::string& var, const Expression& repl) const;

    Expression substitute(std::initializer_list<std::pair<std::string, double>> pairs) const;

    Expression substitute(std::initializer_list<std::pair<std::string, Expression>> pairs) const;

    Expression substitute(const std::unordered_map<std::string, double>& named) const;

    Expression substitute(const std::unordered_map<std::string, Expression>& named) const;
    
    Expression substitute(std::initializer_list<double> positional) const;

    Expression substitute(std::initializer_list<Expression> positional) const;

    Expression substitute(const Expression& from, const Expression& to) const;
    
    Expression substitute(std::initializer_list<std::pair<Expression, Expression>> pairs) const;

    Expression partial_evaluate(const std::string& var, double val) const;
    
    Expression partial_evaluate(const std::string& var, const Expression& repl) const;

    Expression partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const;
    
    Expression partial_evaluate(std::initializer_list<std::pair<std::string, Expression>> pairs) const;

    Expression partial_evaluate(const std::unordered_map<std::string, double>& named) const;

    Expression partial_evaluate(const std::unordered_map<std::string, Expression>& named) const;

    Expression partial_evaluate(const Expression& from, const Expression& to) const;

    Expression partial_evaluate(std::initializer_list<std::pair<Expression, Expression>> pairs) const;

    Expression differentiate(const std::string& var) const;

    Expression nth_derivative_iterative(const std::string& var, unsigned int order = 1) const;

    Expression nth_derivative_recursive(const std::string& var, unsigned int order = 1) const;

    std::size_t num_nodes() const { return count_nodes(node()); }

    ExpressionLambda to_lambda() const;

    template <std::size_t N>
    auto to_function() const {
        assert(*this);
        return to_function_impl(std::make_index_sequence<N>{});
    }

    std::function<double(const std::vector<double>&)> to_function_runtime(EvaluationPolicy policy = {}) const;

private:
    static std::size_t count_nodes(const symbols::MathExpressionNode* n);

    static bool contains_var(const symbols::MathExpressionNode* n, std::uint64_t vid);

    static bool contains_node(const symbols::MathExpressionNode* n, const symbols::MathExpressionNode* target);

    static void collect_var_ids(const symbols::MathExpressionNode* n, std::vector<std::uint64_t>& out);

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

inline ExpressionLambda Expression::to_lambda() const {
    assert(*this);
    return ExpressionLambda(*this, variable_names());
}

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_EXPRESSION_WRAPPER_HPP