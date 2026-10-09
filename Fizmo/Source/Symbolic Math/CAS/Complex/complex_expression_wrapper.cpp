#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

ComplexExpression::ComplexExpression(const ComplexD& value, complex_symbols::ComplexMathExpressionManager& mgr) : mgr_(&mgr), vars_(&mgr.variables()) {
        if (value.is_error()) {
            expr_ = mgr.nan_expr();
        } else if (value.is_infinite()) {
            expr_ = mgr.infinity();
        } else {
            expr_ = mgr.constant(value);
        }
    }

auto ComplexExpression::evaluate(std::initializer_list<ComplexD> vals, complex_symbols::ComplexEvaluationPolicy policy) const -> ComplexD {
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

auto ComplexExpression::evaluate(const std::unordered_map<std::string, ComplexD>& named, complex_symbols::ComplexEvaluationPolicy policy) const -> ComplexD {
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

auto ComplexExpression::evaluate(std::initializer_list<std::pair<std::string, ComplexD>> vals, complex_symbols::ComplexEvaluationPolicy policy) const -> ComplexD {
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

auto ComplexExpression::to_function_runtime(complex_symbols::ComplexEvaluationPolicy policy) const -> std::function<ComplexD(const std::vector<ComplexD>&)> {
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

auto ComplexExpression::depends_on(const std::string& var_name) const -> bool {
        std::uint64_t id = vars_->get(var_name);
        if (id == complex_symbols::ComplexVariableTable::invalid_id) return false;
        return contains_var(expr_.get(), id);
    }

auto ComplexExpression::variable_names() const -> std::vector<std::string> {
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

auto ComplexExpression::operator-() const -> ComplexExpression {
        assert(*this);

        return ComplexExpression(
            manager().unary(complex_symbols::ComplexNodeType::Negate, expr_),
            manager()
        );
    }

auto ComplexExpression::count_nodes(const complex_symbols::ComplexMathExpressionNode* n) -> std::size_t {
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

auto ComplexExpression::contains_var(const complex_symbols::ComplexMathExpressionNode* n, std::uint64_t vid) -> bool {
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

auto ComplexExpression::contains_node(const complex_symbols::ComplexMathExpressionNode* tree, const complex_symbols::ComplexMathExpressionNode* target) -> bool {
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

auto ComplexExpression::collect_var_ids(const complex_symbols::ComplexMathExpressionNode* n, std::vector<std::uint64_t>& ids) -> void {
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

} // namespace cas
} // namespace math
} // namespace fizmo
