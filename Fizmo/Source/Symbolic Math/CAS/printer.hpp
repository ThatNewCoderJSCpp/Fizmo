#ifndef FIZMO_MATH_PRINTER_HPP
#define FIZMO_MATH_PRINTER_HPP

#include "expression.hpp"
#include <ostream>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <string>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

 int node_prec(NodeType t) noexcept;

 std::string fmt_double(double v);

 void print_double(std::ostream& os, double v);

 std::string to_superscript(int value, bool include_parentheses = false);

 std::string to_subscript(int value, bool include_parentheses = false);

 const char* unary_fn_name(NodeType t) noexcept;

 void print_node(std::ostream& os, const MathExpressionNode* n, const VariableTable* vars, const VariableTable* funcs, int parent_prec = 0, bool right_child = false);

 void print_node(
    std::ostream& os,
    const MathExpressionNode* n,
    const VariableTable* vars,
    const VariableTable* funcs,
    int parent_prec,
    bool right_child
);

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_PRINTER_HPP