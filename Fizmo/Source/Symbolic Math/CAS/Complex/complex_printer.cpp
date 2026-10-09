#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace complex_symbols {

int complex_node_prec(ComplexNodeType t) noexcept {
    switch (t) {
        case ComplexNodeType::Add:
        case ComplexNodeType::Subtract:  return 1;
        case ComplexNodeType::Multiply:
        case ComplexNodeType::Divide:    return 2;
        case ComplexNodeType::Negate:    return 3;
        case ComplexNodeType::Power:     return 4;
        default:                         return 5;
    }
}

const char* complex_unary_fn_name(ComplexNodeType t) noexcept {
    switch (t) {
        case ComplexNodeType::Sqrt:          return "sqrt";
        case ComplexNodeType::Cbrt:          return "cbrt";
        case ComplexNodeType::NaturalExp:    return "exp";
        case ComplexNodeType::NaturalLog:    return "ln";
        case ComplexNodeType::Sin:           return "sin";
        case ComplexNodeType::Cos:           return "cos";
        case ComplexNodeType::Tan:           return "tan";
        case ComplexNodeType::Csc:           return "csc";
        case ComplexNodeType::Sec:           return "sec";
        case ComplexNodeType::Cot:           return "cot";
        case ComplexNodeType::Arcsin:        return "sin\u207b\u00b9";
        case ComplexNodeType::Arccos:        return "cos\u207b\u00b9";
        case ComplexNodeType::Arctan:        return "tan\u207b\u00b9";
        case ComplexNodeType::Arccsc:        return "csc\u207b\u00b9";
        case ComplexNodeType::Arcsec:        return "sec\u207b\u00b9";
        case ComplexNodeType::Arccot:        return "cot\u207b\u00b9";
        case ComplexNodeType::Sinh:          return "sinh";
        case ComplexNodeType::Cosh:          return "cosh";
        case ComplexNodeType::Tanh:          return "tanh";
        case ComplexNodeType::Csch:          return "csch";
        case ComplexNodeType::Sech:          return "sech";
        case ComplexNodeType::Coth:          return "coth";
        case ComplexNodeType::Arcsinh:       return "sinh\u207b\u00b9";
        case ComplexNodeType::Arccosh:       return "cosh\u207b\u00b9";
        case ComplexNodeType::Arctanh:       return "tanh\u207b\u00b9";
        case ComplexNodeType::Arccsch:       return "csch\u207b\u00b9";
        case ComplexNodeType::Arcsech:       return "sech\u207b\u00b9";
        case ComplexNodeType::Arccoth:       return "coth\u207b\u00b9";
        case ComplexNodeType::Conjugate:     return "conj";
        case ComplexNodeType::RealPart:      return "Re";
        case ComplexNodeType::ImaginaryPart: return "Im";
        case ComplexNodeType::Magnitude:     return "mag";
        case ComplexNodeType::Argument:      return "arg";
        case ComplexNodeType::Reciprocal:    return "recip";
        case ComplexNodeType::Sign:          return "sgn";
        default:                             return "fn";
    }
}

void print_complex_double(std::ostream& os, double v) {
    if (std::isinf(v)) { os << (v > 0.0 ? "\u221e" : "-\u221e"); return; }
    if (std::isnan(v)) { os << "NaN"; return; }

    if (v == std::floor(v) && std::abs(v) < 1e15) {
        os << static_cast<long long>(v);
    } else {
        os << v;
    }
}

void print_complex_constant(std::ostream& os, double r, double i) {
    const double eps = constants::middle_epsilon();
    bool rz = std::abs(r) <= eps;
    bool iz = std::abs(i) <= eps;
    if (rz && iz) { os << '0'; return; }
    if (!rz && iz) { print_complex_double(os, r); return; }

    if (rz && !iz) {
        if (std::abs(i - 1.0) <= eps) { os << 'i'; return; }
        if (std::abs(i + 1.0) <= eps) { os << "-i"; return; }
        print_complex_double(os, i);
        os << 'i';
        return;
    }

    os << '(';
    print_complex_double(os, r);

    if (i < 0.0) {
        os << " - ";
        if (std::abs(-i - 1.0) <= eps) os << 'i';
        else { print_complex_double(os, -i); os << 'i'; }
    } else {
        os << " + ";
        if (std::abs(i - 1.0) <= eps) os << 'i';
        else { print_complex_double(os, i); os << 'i'; }
    }

    os << ')';
}

void print_complex_node(
    std::ostream& os,
    const ComplexMathExpressionNode* n,
    const ComplexVariableTable* vars,
    int parent_prec,
    bool right_child 
) {
    if (is_invalid(n)) { os << "<Invalid>"; return; }
    if (!n) { os << "<null>"; return; }
    const int prec = complex_node_prec(n->type);
    const bool need_parens = (prec < parent_prec) || (right_child && prec == parent_prec && prec <= 2);
    if (need_parens) os << '(';

    switch (n->type) {
        case ComplexNodeType::Constant:
            print_complex_constant(os, n->constant.real, n->constant.imag);
            break;

        case ComplexNodeType::Variable:
            if (vars && n->variable.var_id < vars->size())
                os << vars->name(n->variable.var_id);
            else
                os << 'z' << n->variable.var_id;
            break;

        case ComplexNodeType::Infinity:      os << "\u221e\u0302"; break;  
        case ComplexNodeType::NaN:           os << "NaN";          break;
        case ComplexNodeType::Undefined:     os << "Undefined";    break;
        case ComplexNodeType::Indeterminate: os << "Indeterminate";break;

        case ComplexNodeType::Negate:
            os << '-';
            print_complex_node(os, n->unary.child, vars, prec);
            break;

        case ComplexNodeType::Add:
            print_complex_node(os, n->binary.left,  vars, prec);
            os << " + ";
            print_complex_node(os, n->binary.right, vars, prec, true);
            break;

        case ComplexNodeType::Subtract:
            print_complex_node(os, n->binary.left,  vars, prec);
            os << " - ";
            print_complex_node(os, n->binary.right, vars, prec, true);
            break;

        case ComplexNodeType::Multiply:
            print_complex_node(os, n->binary.left,  vars, prec);
            os << " * ";
            print_complex_node(os, n->binary.right, vars, prec, true);
            break;

        case ComplexNodeType::Divide:
            print_complex_node(os, n->binary.left,  vars, prec);
            os << " / ";
            print_complex_node(os, n->binary.right, vars, prec, true);
            break;

        case ComplexNodeType::Power:
            print_complex_node(os, n->power.base, vars, prec);
            os << "^";
            print_complex_node(os, n->power.exponent, vars, prec, true);
            break;

        case ComplexNodeType::Log:
            os << "log(";
            print_complex_node(os, n->binary.left,  vars, 0);
            os << ", ";
            print_complex_node(os, n->binary.right, vars, 0);
            os << ')';
            break;

        case ComplexNodeType::Root:
            os << "root(";
            print_complex_node(os, n->binary.left,  vars, 0);
            os << ", ";
            print_complex_node(os, n->binary.right, vars, 0);
            os << ')';
            break;

        case ComplexNodeType::Magnitude:
            os << '|';
            print_complex_node(os, n->unary.child, vars, 0);
            os << '|';
            break;

        case ComplexNodeType::Conjugate:
            print_complex_node(os, n->unary.child, vars, 5);
            os << '*';
            break;

        default:
            os << complex_unary_fn_name(n->type) << '(';
            print_complex_node(os, n->unary.child, vars, 0);
            os << ')';
            break;
    }

    if (need_parens) os << ')';
}

} // namespace complex_symbols
} // namespace cas
} // namespace math
} // namespace fizmo
