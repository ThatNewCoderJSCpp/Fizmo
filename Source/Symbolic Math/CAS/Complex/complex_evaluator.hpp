#ifndef FIZMO_COMPLEX_EVALUATOR_HPP
#define FIZMO_COMPLEX_EVALUATOR_HPP

#include "complex_expression.hpp"

#include "../../../Complex/complex_math.hpp"

#include <limits>
#include <stdexcept>

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

ComplexD complex_nan();
ComplexD complex_inf();

ComplexD eval_complex_node(const ComplexMathExpressionNode* n, const ComplexEvalContext& ctx);

inline ComplexD evaluate(ComplexMathExpression e, const ComplexEvalContext& ctx) { return eval_complex_node(e.get(), ctx); }

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_EVALUATOR_HPP