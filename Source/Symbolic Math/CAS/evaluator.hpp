#ifndef FIZMO_MATH_EVALUATOR_CLASS_HPP
#define FIZMO_MATH_EVALUATOR_CLASS_HPP

#include "expression.hpp"
#include "../../Basic/constants.hpp"

#include "numeric_functions.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

double eval_node(const MathExpressionNode* n, const EvalContext& ctx);

inline double evaluate(MathExpression e, const EvalContext& ctx) {
    return eval_node(e.get(), ctx);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_EVALUATOR_CLASS_HPP