#ifndef FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP
#define FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP

#include "../integrator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

namespace detail_heur {

struct ProductSplit {
    MathExpressionNode* left;
    MathExpressionNode* right;
};

void strip_const(MathExpressionNode* n, double& coeff, MathExpressionNode*& core);

} // namespace detail_heur

MathExpression try_heuristic(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& diff,
    MathExpressionNode* n,
    std::uint64_t vid
);

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_HEURISTIC_RULES_HPP