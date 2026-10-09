#ifndef FIZMO_MATH_EVALUATOR_CLASS_HPP
#define FIZMO_MATH_EVALUATOR_CLASS_HPP

#include "expression.hpp"
#include "../../Basic/constants.hpp"

#include "../Math Impl/gamma.hpp"
#include "../Math Impl/error.hpp"
#include "../Math Impl/lambert_w.hpp"
#include "../Math Impl/chebyshev.hpp"
#include "../Math Impl/exp_log_integral.hpp"
#include "../Math Impl/trig_integrals.hpp"
#include "../Math Impl/fresnel.hpp"
#include "../Math Impl/spence.hpp"
#include "../Math Impl/rogers.hpp"
#include "../Math Impl/gudermannian.hpp"
#include "../Math Impl/fibonacci_lucas.hpp"

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