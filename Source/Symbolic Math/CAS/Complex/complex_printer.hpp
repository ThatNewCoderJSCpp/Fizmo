#ifndef FIZMO_COMPLEX_PRINTER_HPP
#define FIZMO_COMPLEX_PRINTER_HPP

#include "complex_evaluator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

int complex_node_prec(ComplexNodeType t) noexcept;

const char* complex_unary_fn_name(ComplexNodeType t) noexcept;

void print_complex_double(std::ostream& os, double v);

void print_complex_constant(std::ostream& os, double r, double i);

void print_complex_node(
    std::ostream& os,
    const ComplexMathExpressionNode* n,
    const ComplexVariableTable* vars,
    int parent_prec = 0,
    bool right_child = false
);

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_PRINTER_HPP