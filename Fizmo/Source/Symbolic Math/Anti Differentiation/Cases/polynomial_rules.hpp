#ifndef FIZMO_MATH_INTEGRATION_POLYNOMIAL_RULES_HPP
#define FIZMO_MATH_INTEGRATION_POLYNOMIAL_RULES_HPP

#include "../integrator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {
namespace integration {

 MathExpression try_polynomial(
    MathExpressionManager& mgr,
    MathExpressionSimplifier& simp,
    MathExpressionDifferentiator& /*diff*/,
    MathExpressionNode* n,
    std::uint64_t vid
);

} // namespace integration
} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INTEGRATION_POLYNOMIAL_RULES_HPP