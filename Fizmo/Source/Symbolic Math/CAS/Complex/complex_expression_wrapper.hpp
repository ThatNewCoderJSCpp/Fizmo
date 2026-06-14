#ifndef FIZMO_COMPLEX_EXPRESSION_WRAPPER_HPP
#define FIZMO_COMPLEX_EXPRESSION_WRAPPER_HPP

#include "complex_printer.hpp"
#include <cassert>

namespace fizmo {
namespace math {
namespace cas {

class ComplexExpression {
public:
    ComplexExpression() = default;

    explicit ComplexExpression(complex_symbols::ComplexMathExpression expr, complex_symbols::ComplexMathExpressionManager& mgr) : expr_(expr), mgr_(&mgr), vars_(&mgr.variables()) {}
    explicit ComplexExpression(complex_symbols::ComplexMathExpressionNode* node, complex_symbols::ComplexMathExpressionManager& mgr) : expr_(mgr.canonicalize(node)), mgr_(&mgr), vars_(&mgr.variables()) {}

    explicit ComplexExpression(const ComplexD& value, complex_symbols::ComplexMathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) {
        if (value.is_error()) {
            expr_ = mgr.nan_expr();
        } else if (value.is_infinite()) {
            expr_ = mgr.infinity();
        } else {
            expr_ = mgr.constant(value);
        }
    }

    explicit ComplexExpression(double real, complex_symbols::ComplexMathExpressionManager& mgr) : ComplexExpression(ComplexD(real, 0.0), mgr) {}

    explicit ComplexExpression(const std::string& var_name, complex_symbols::ComplexMathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) {
        expr_ = mgr.variable(var_name);
    }

    complex_symbols::ComplexMathExpression      inner() const noexcept { return expr_; }
    complex_symbols::ComplexMathExpressionNode* node()  const noexcept { return expr_.get(); }

    bool is_invalid() const noexcept { return expr_.get() == nullptr || mgr_ == nullptr; }

    static ComplexExpression invalid() noexcept {
        ComplexExpression e;
        e.mgr_  = nullptr;
        e.vars_ = nullptr;
        return e;
    }

    complex_symbols::ComplexMathExpressionManager& manager() const {
        assert(mgr_ && "ComplexExpression has no associated manager");
        return *mgr_;
    }

    explicit operator bool() const noexcept { return !is_invalid(); }

    bool operator==(const ComplexExpression& o) const noexcept { return expr_.get() == o.expr_.get(); }
    bool operator!=(const ComplexExpression& o) const noexcept { return !(*this == o); }

    ComplexD evaluate(const complex_symbols::ComplexEvalContext& ctx) const {
        assert(*this);
        return complex_symbols::evaluate(expr_, ctx);
    }

    ComplexD evaluate(std::initializer_list<ComplexD> vals, complex_symbols::ComplexEvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();
        complex_symbols::ComplexEvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());
        std::size_t i = 0;

        for (const ComplexD& v : vals) {
            if (i >= vars.size()) break;
            ctx.values[i] = v;
            ctx.assigned[i] = true;
            ++i;
        }

        return complex_symbols::evaluate(expr_, ctx);
    }

    ComplexD evaluate(const std::unordered_map<std::string, ComplexD>& named, complex_symbols::ComplexEvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();
        complex_symbols::ComplexEvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());

        for (auto& [name, value] : named) {
            std::uint64_t id = vars.get(name);
            
            if (id != complex_symbols::ComplexVariableTable::invalid_id && id < ctx.values.size()) {
                ctx.values[id] = value;
                ctx.assigned[id] = true;
            }
        }

        return complex_symbols::evaluate(expr_, ctx);
    }

    ComplexD evaluate(std::initializer_list<std::pair<std::string, ComplexD>> vals, complex_symbols::ComplexEvaluationPolicy policy = {}) const {
        assert(*this);
        const auto& vars = manager().variables();
        complex_symbols::ComplexEvalContext ctx;
        ctx.policy = policy;
        ctx.resize(vars.size());

        for (auto& [name, value] : vals) {
            std::uint64_t id = vars.get(name);

            if (id != complex_symbols::ComplexVariableTable::invalid_id && id < ctx.values.size()) {
                ctx.values[id] = value;
                ctx.assigned[id] = true;
            }
        }

        return complex_symbols::evaluate(expr_, ctx);
    }

    std::function<ComplexD(const std::vector<ComplexD>&)>
    to_function_runtime(complex_symbols::ComplexEvaluationPolicy policy = {}) const {
        assert(*this);
        ComplexExpression captured = *this;

        return [captured, policy](const std::vector<ComplexD>& args) -> ComplexD {
            const auto& vars = captured.manager().variables();
            complex_symbols::ComplexEvalContext ctx;
            ctx.policy = policy;
            ctx.resize(vars.size());

            for (std::size_t i = 0; i < args.size() && i < ctx.values.size(); ++i) {
                ctx.values[i]   = args[i];
                ctx.assigned[i] = true;
            }

            return complex_symbols::evaluate(captured.inner(), ctx);
        };
    }

    template <std::size_t N>
    auto to_function(complex_symbols::ComplexEvaluationPolicy policy = {}) const {
        assert(*this);
        return to_function_impl(policy, std::make_index_sequence<N>{});
    }

    std::size_t num_nodes() const { return count_nodes(node()); }

    bool depends_on(const std::string& var_name) const {
        std::uint64_t id = vars_->get(var_name);
        if (id == complex_symbols::ComplexVariableTable::invalid_id) return false;
        return contains_var(expr_.get(), id);
    }

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

    friend std::ostream& operator<<(std::ostream& os, const ComplexExpression& e) {
        complex_symbols::print_complex_node(os, e.node(), e.vars_);
        return os;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
    }

    ComplexExpression operator-() const {
        assert(*this);

        return ComplexExpression(
            manager().unary(complex_symbols::ComplexNodeType::Negate, expr_),
            manager()
        );
    }

    friend ComplexExpression operator+(const ComplexExpression& a, const ComplexExpression& b) {
        assert(a && b);

        return ComplexExpression(
            a.manager().binary(complex_symbols::ComplexNodeType::Add, a.expr_, b.expr_),
            a.manager()
        );
    }

    friend ComplexExpression operator+(const ComplexExpression& a, const ComplexD& b) {
        assert(a);
        return a + ComplexExpression(b, a.manager());
    }

    friend ComplexExpression operator+(const ComplexD& a, const ComplexExpression& b) {
        assert(b);
        return ComplexExpression(a, b.manager()) + b;
    }

    friend ComplexExpression operator+(const ComplexExpression& a, double b) {
        return a + ComplexD(b, 0.0);
    }

    friend ComplexExpression operator+(double a, const ComplexExpression& b) {
        return ComplexD(a, 0.0) + b;
    }

    friend ComplexExpression operator-(const ComplexExpression& a, const ComplexExpression& b) {
        assert(a && b);

        return ComplexExpression(
            a.manager().binary(complex_symbols::ComplexNodeType::Subtract, a.expr_, b.expr_),
            a.manager()
        );
    }

    friend ComplexExpression operator-(const ComplexExpression& a, const ComplexD& b) {
        assert(a);
        return a - ComplexExpression(b, a.manager());
    }

    friend ComplexExpression operator-(const ComplexD& a, const ComplexExpression& b) {
        assert(b);
        return ComplexExpression(a, b.manager()) - b;
    }

    friend ComplexExpression operator-(const ComplexExpression& a, double b) {
        return a - ComplexD(b, 0.0);
    }

    friend ComplexExpression operator-(double a, const ComplexExpression& b) {
        return ComplexD(a, 0.0) - b;
    }

    friend ComplexExpression operator*(const ComplexExpression& a, const ComplexExpression& b) {
        assert(a && b);

        return ComplexExpression(
            a.manager().binary(complex_symbols::ComplexNodeType::Multiply, a.expr_, b.expr_),
            a.manager()
        );
    }

    friend ComplexExpression operator*(const ComplexExpression& a, const ComplexD& b) {
        assert(a);
        return a * ComplexExpression(b, a.manager());
    }

    friend ComplexExpression operator*(const ComplexD& a, const ComplexExpression& b) {
        assert(b);
        return ComplexExpression(a, b.manager()) * b;
    }

    friend ComplexExpression operator*(const ComplexExpression& a, double b) {
        return a * ComplexD(b, 0.0);
    }

    friend ComplexExpression operator*(double a, const ComplexExpression& b) {
        return ComplexD(a, 0.0) * b;
    }

    friend ComplexExpression operator/(const ComplexExpression& a, const ComplexExpression& b) {
        assert(a && b);

        return ComplexExpression(
            a.manager().binary(complex_symbols::ComplexNodeType::Divide, a.expr_, b.expr_),
            a.manager()
        );
    }

    friend ComplexExpression operator/(const ComplexExpression& a, const ComplexD& b) {
        assert(a);
        return a / ComplexExpression(b, a.manager());
    }

    friend ComplexExpression operator/(const ComplexD& a, const ComplexExpression& b) {
        assert(b);
        return ComplexExpression(a, b.manager()) / b;
    }

    friend ComplexExpression operator/(const ComplexExpression& a, double b) {
        return a / ComplexD(b, 0.0);
    }

    friend ComplexExpression operator/(double a, const ComplexExpression& b) {
        return ComplexD(a, 0.0) / b;
    }

private:
    complex_symbols::ComplexMathExpression          expr_;
    complex_symbols::ComplexMathExpressionManager*  mgr_  = nullptr;
    complex_symbols::ComplexVariableTable*          vars_ = nullptr;

    static std::size_t count_nodes(const complex_symbols::ComplexMathExpressionNode* n) {
        if (!n) return 0;

        switch (n->type) {
            case complex_symbols::ComplexNodeType::Constant:
            case complex_symbols::ComplexNodeType::Variable:
            case complex_symbols::ComplexNodeType::Infinity:
            case complex_symbols::ComplexNodeType::NaN:
            case complex_symbols::ComplexNodeType::Undefined:
            case complex_symbols::ComplexNodeType::Indeterminate:
                return 1;
            case complex_symbols::ComplexNodeType::Power:
                return 1 + count_nodes(n->power.base) + count_nodes(n->power.exponent);
            default:
                if (complex_symbols::ComplexNodeKeyHash::is_unary(n->type))  return 1 + count_nodes(n->unary.child);
                if (complex_symbols::ComplexNodeKeyHash::is_binary(n->type)) return 1 + count_nodes(n->binary.left) + count_nodes(n->binary.right);
                return 1;
        }
    }

    static bool contains_var(const complex_symbols::ComplexMathExpressionNode* n, std::uint64_t vid) {
        if (!n) return false;

        switch (n->type) {
            case complex_symbols::ComplexNodeType::Variable:
                return n->variable.var_id == vid;
            case complex_symbols::ComplexNodeType::Constant:
            case complex_symbols::ComplexNodeType::Infinity:
            case complex_symbols::ComplexNodeType::NaN:
            case complex_symbols::ComplexNodeType::Undefined:
            case complex_symbols::ComplexNodeType::Indeterminate:
            case complex_symbols::ComplexNodeType::Invalid:
                return false;
            case complex_symbols::ComplexNodeType::Power:
                return contains_var(n->power.base, vid) || contains_var(n->power.exponent, vid);
            default:
                if (complex_symbols::ComplexNodeKeyHash::is_unary(n->type))
                    return contains_var(n->unary.child, vid);
                if (complex_symbols::ComplexNodeKeyHash::is_binary(n->type))
                    return contains_var(n->binary.left, vid) || contains_var(n->binary.right, vid);
                return false;
        }
    }

    static bool contains_node(const complex_symbols::ComplexMathExpressionNode* tree, const complex_symbols::ComplexMathExpressionNode* target) {
        if (!tree) return false;
        if (tree == target) return true;

        switch (tree->type) {
            case complex_symbols::ComplexNodeType::Constant:
            case complex_symbols::ComplexNodeType::Variable:
            case complex_symbols::ComplexNodeType::Infinity:
            case complex_symbols::ComplexNodeType::NaN:
            case complex_symbols::ComplexNodeType::Undefined:
            case complex_symbols::ComplexNodeType::Indeterminate:
            case complex_symbols::ComplexNodeType::Invalid:
                return false;
            case complex_symbols::ComplexNodeType::Power:
                return contains_node(tree->power.base, target) || contains_node(tree->power.exponent, target);
            default:
                if (complex_symbols::ComplexNodeKeyHash::is_unary(tree->type))
                    return contains_node(tree->unary.child, target);
                if (complex_symbols::ComplexNodeKeyHash::is_binary(tree->type))
                    return contains_node(tree->binary.left, target) || contains_node(tree->binary.right, target);
                return false;
        }
    }

    static void collect_var_ids(const complex_symbols::ComplexMathExpressionNode* n, std::vector<std::uint64_t>& ids) {
        if (!n) return;

        if (n->type == complex_symbols::ComplexNodeType::Variable) {
            auto id = n->variable.var_id;
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
            return;
        }

        switch (n->type) {
            case complex_symbols::ComplexNodeType::Constant:
            case complex_symbols::ComplexNodeType::Infinity:
            case complex_symbols::ComplexNodeType::NaN:
            case complex_symbols::ComplexNodeType::Undefined:
            case complex_symbols::ComplexNodeType::Indeterminate:
            case complex_symbols::ComplexNodeType::Invalid:
                return;
            case complex_symbols::ComplexNodeType::Power:
                collect_var_ids(n->power.base, ids);
                collect_var_ids(n->power.exponent, ids);
                return;
            default:
                if (complex_symbols::ComplexNodeKeyHash::is_unary(n->type)) {
                    collect_var_ids(n->unary.child, ids);
                } else if (complex_symbols::ComplexNodeKeyHash::is_binary(n->type)) {
                    collect_var_ids(n->binary.left, ids);
                    collect_var_ids(n->binary.right, ids);
                }
                return;
        }
    }

    template <std::size_t... Is>
    auto to_function_impl(complex_symbols::ComplexEvaluationPolicy policy, std::index_sequence<Is...>) const {
        ComplexExpression captured = *this;

        return [captured, policy](decltype((void)Is, ComplexD{})... args) -> ComplexD {
            const auto& vars = captured.manager().variables();
            complex_symbols::ComplexEvalContext ctx;
            ctx.policy = policy;
            ctx.resize(vars.size());
            ComplexD arr[] = { args... };

            for (std::size_t i = 0; i < sizeof...(Is) && i < ctx.values.size(); ++i) {
                ctx.values[i]   = arr[i];
                ctx.assigned[i] = true;
            }
            
            return complex_symbols::evaluate(captured.inner(), ctx);
        };
    }
};

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_EXPRESSION_WRAPPER_HPP