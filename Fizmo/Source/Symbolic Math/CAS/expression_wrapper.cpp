#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

Expression::Expression(double value, symbols::MathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) {
        using NT = symbols::NodeType;

        if (std::isnan(value)) {
            expr_ = mgr.nan_expr();
        } else if (std::isinf(value)) {
            expr_ = value > 0 ? mgr.pos_inf() : mgr.neg_inf();
        } else {
            expr_ = mgr.constant(value);
        }
    }

auto Expression::symbolic_equals(const Expression& o) const -> bool {
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

auto Expression::is_symbolically_zero() const -> bool {
        if (is_invalid()) return false;
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionRewriter   rw(manager(), simp);
        auto s = rw.rewrite(simp.simplify(expr_));
        return s.get()->type == symbols::NodeType::Constant && s.get()->constant == 0.0;
    }

auto Expression::depends_on(const std::string& var_name) const -> bool {
        std::uint64_t id = vars_->get(var_name);
        if (id == symbols::VariableTable::invalid_id) return false;
        return contains_var(expr_.get(), id);
    }

auto Expression::variable_names() const -> std::vector<std::string> {
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

auto Expression::evaluate(std::initializer_list<double> vals, EvaluationPolicy policy) const -> double {
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

auto Expression::evaluate(const std::unordered_map<std::string, double>& named, EvaluationPolicy policy) const -> double {
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

auto Expression::evaluate(std::initializer_list<std::pair<std::string, double>> vals, EvaluationPolicy policy) const -> double {
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

auto Expression::rewrite(const RewriterConfig& cfg) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionRewriter   rw(manager(), simp, cfg);
        return Expression(rw.rewrite(expr_), manager());
    }

auto Expression::full_simplify(const RewriterConfig& cfg) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpression s = simp.simplify(expr_);
        symbols::MathExpressionRewriter   rw(manager(), simp, cfg);
        return Expression(rw.rewrite(s), manager());
    }

auto Expression::substitute(const std::string& var, double val) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, val), manager());
    }

auto Expression::substitute(const std::string& var, const Expression& repl) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, repl.inner()), manager());
    }

auto Expression::substitute(std::initializer_list<std::pair<std::string, double>> pairs) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, pairs), manager());
    }

auto Expression::substitute(std::initializer_list<std::pair<std::string, Expression>> pairs) const -> Expression {
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

auto Expression::substitute(const std::unordered_map<std::string, double>& named) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, named), manager());
    }

auto Expression::substitute(const std::unordered_map<std::string, Expression>& named) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (auto& name : named) inner.emplace(name.first, name.second.inner());
        return Expression(sub.substitute(expr_, inner), manager());
    }

auto Expression::substitute(std::initializer_list<double> positional) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, positional), manager());
    }

auto Expression::substitute(std::initializer_list<Expression> positional) const -> Expression {
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

auto Expression::substitute(const Expression& from, const Expression& to) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, from.inner(), to.inner()), manager());
    }

auto Expression::substitute(std::initializer_list<std::pair<Expression, Expression>> pairs) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;
        for (auto& pair : pairs) smap.by_node[pair.first.inner().get()] = pair.second.inner().get();
        return Expression(sub.substitute(expr_, smap), manager());
    }

auto Expression::partial_evaluate(const std::string& var, double val) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, val), manager());
    }

auto Expression::partial_evaluate(const std::string& var, const Expression& repl) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, var, repl.inner()), manager());
    }

auto Expression::partial_evaluate(std::initializer_list<std::pair<std::string, double>> pairs) const -> Expression {
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

auto Expression::partial_evaluate(std::initializer_list<std::pair<std::string, Expression>> pairs) const -> Expression {
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

auto Expression::partial_evaluate(const std::unordered_map<std::string, double>& named) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, named), manager());
    }

auto Expression::partial_evaluate(const std::unordered_map<std::string, Expression>& named) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        std::unordered_map<std::string, symbols::MathExpression> inner;
        inner.reserve(named.size());
        for (const auto& name : named) inner.emplace(name.first, name.second.inner());
        return Expression(sub.substitute(expr_, inner), manager());
    }

auto Expression::partial_evaluate(const Expression& from, const Expression& to) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        return Expression(sub.substitute(expr_, from.inner(), to.inner()), manager());
    }

auto Expression::partial_evaluate(std::initializer_list<std::pair<Expression, Expression>> pairs) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier  simp(manager());
        symbols::MathExpressionSubstitutor sub(manager(), manager().variables(), simp);
        symbols::SubstitutionMap smap;
        for (auto& pair : pairs) smap.by_node[pair.first.inner().get()] = pair.second.inner().get();
        return Expression(sub.substitute(expr_, smap), manager());
    }

auto Expression::differentiate(const std::string& var) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.derivative(inner(), var), manager());
    }

auto Expression::nth_derivative_iterative(const std::string& var, unsigned int order) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.nth_derivative_iterative(inner(), var, order), manager());
    }

auto Expression::nth_derivative_recursive(const std::string& var, unsigned int order) const -> Expression {
        assert(*this);
        symbols::MathExpressionSimplifier simp(manager());
        symbols::MathExpressionDifferentiator diff(manager(), manager().variables(), simp);
        return Expression(diff.nth_derivative_recursive(inner(), var, order), manager());
    }

auto Expression::to_function_runtime(EvaluationPolicy policy) const -> std::function<double(const std::vector<double>&)> {
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

auto Expression::count_nodes(const symbols::MathExpressionNode* n) -> std::size_t {
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

auto Expression::contains_var(const symbols::MathExpressionNode* n, std::uint64_t vid) -> bool {
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

auto Expression::contains_node(const symbols::MathExpressionNode* n, const symbols::MathExpressionNode* target) -> bool {
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

auto Expression::collect_var_ids(const symbols::MathExpressionNode* n, std::vector<std::uint64_t>& out) -> void {
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

} // namespace cas
} // namespace math
} // namespace fizmo
