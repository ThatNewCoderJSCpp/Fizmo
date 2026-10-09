#ifndef FIZMO_MATH_INTEGRATION_TRIGONOMETRIC_RULES_HPP
#define FIZMO_MATH_INTEGRATION_TRIGONOMETRIC_RULES_HPP

#include "../integrator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

namespace detail_trig {

bool is_trig_squared(MathExpressionNode* n, NodeType fn, std::uint64_t vid, double& a_out, double& b_out, MathExpressionNode*& inner_out);

inline bool linear_or_var(MathExpressionNode* inner, std::uint64_t vid, double& a, double& b) {
    if (is_var(inner, vid)) { a = 1.0; b = 0.0; return true; }
    return is_linear_in_var(inner, vid, a, b);
}

} // namespace detail_trig

MathExpression try_trigonometric(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& /*simp*/,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
);

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_TRIGONOMETRIC_RULES_HPP