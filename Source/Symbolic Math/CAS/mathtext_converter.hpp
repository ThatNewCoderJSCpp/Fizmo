#ifndef FIZMO_MATH_CAS_MATHTEXT_CONVERTER_HPP
#define FIZMO_MATH_CAS_MATHTEXT_CONVERTER_HPP

#include <string>

#include "expression.hpp"
#include "../../MathText/mathtext.hpp"

namespace fizmo {
namespace math {
namespace cas {

using MathTextSyntax = ::fizmo::mathtext::SourceSyntax;

namespace symbols {

std::string to_mathtext(const MathExpressionNode* n, const VariableTable* vars, const VariableTable* funcs, MathTextSyntax syntax = MathTextSyntax::Brace);

inline std::string to_mathtext(MathExpression e, const MathExpressionManager& mgr, MathTextSyntax syntax = MathTextSyntax::Brace) {
    return to_mathtext(e.get(), &mgr.variables(), &mgr.functions(), syntax);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_CAS_MATHTEXT_CONVERTER_HPP
