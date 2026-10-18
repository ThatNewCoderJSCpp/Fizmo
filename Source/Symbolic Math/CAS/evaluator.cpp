#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

double eval_node(const MathExpressionNode* n, const EvalContext& ctx) {
    return eval_node_as<double>(n, ctx);
}

template BigFloat eval_node_as<BigFloat, EvalBackend<BigFloat>>(const MathExpressionNode* n, const BasicEvalContext<BigFloat>& ctx);

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
