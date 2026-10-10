#ifndef FIZMO_MATH_DIFFERENTIATOR_CLASS_HPP
#define FIZMO_MATH_DIFFERENTIATOR_CLASS_HPP

#include "expression.hpp"
#include "simplifier.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

struct DerivKey {
    MathExpressionNode* node;
    std::uint64_t       var_id;
    unsigned            order;
    bool operator==(const DerivKey& o) const noexcept { return node == o.node && var_id == o.var_id && order == o.order; }
};

struct DerivKeyHash {
    std::size_t operator()(const DerivKey& k) const noexcept;
};

using DerivCache = std::unordered_map<DerivKey, MathExpressionNode*, DerivKeyHash>;

class MathExpressionDifferentiator {
public:
    MathExpressionDifferentiator(MathExpressionManager& mgr, VariableTable& vars, MathExpressionSimplifier& simp) : mgr_(mgr), vars_(vars), simp_(simp) {}

    MathExpression derivative(MathExpression expr, const std::string& var_name);

    MathExpression nth_derivative_recursive(MathExpression expr, const std::string& var_name, unsigned int order);

    MathExpression nth_derivative_iterative(MathExpression expr, const std::string& var_name, unsigned int order);

    void clear_cache() noexcept { cache_.clear(); }

    bool contains_var(const MathExpressionNode* n, std::uint64_t vid) const;

private:
    MathExpressionManager&    mgr_;
    VariableTable&             vars_;
    MathExpressionSimplifier&  simp_;
    DerivCache cache_;

    MathExpression konst(double v)       { return mgr_.constant(v); }
    MathExpression neg(MathExpression a) { return mgr_.unary(NodeType::Negate, a); }
    MathExpression add(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Add,      a, b); }
    MathExpression sub(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Subtract, a, b); }
    MathExpression mul(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Multiply, a, b); }
    MathExpression div(MathExpression a, MathExpression b) { return mgr_.binary(NodeType::Divide,   a, b); }
    MathExpression pow(MathExpression b, MathExpression e) { return mgr_.power(b, e); }
    MathExpression unary(NodeType t, MathExpression a)     { return mgr_.unary(t, a); }
    MathExpression binary(NodeType t, MathExpression a, MathExpression b) { return mgr_.binary(t, a, b); }
    MathExpression wrap(MathExpressionNode* n)             { return MathExpression(n); }
    MathExpression invalid() { return mgr_.invalid_expr(); }
    static bool is_invalid_node(const MathExpressionNode* n) noexcept { return is_invalid(n); }

    MathExpression chain(MathExpression outer_prime, MathExpressionNode* inner, std::uint64_t vid) {
        MathExpression ip = diff(inner, vid);
        return mul(outer_prime, ip);
    }

    MathExpression diff(MathExpressionNode* n, std::uint64_t vid);

    MathExpression diff_n_recurs(MathExpressionNode* n, std::uint64_t vid, unsigned int order);

    MathExpression nth_derivative_iter(MathExpression expr, const std::string& var_name, unsigned int order);

    MathExpression diff_power(MathExpressionNode* n, std::uint64_t vid);

    MathExpression diff_quotient(MathExpressionNode* num_n, MathExpressionNode* den_n, std::uint64_t vid);
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_DIFFERENTIATOR_CLASS_HPP