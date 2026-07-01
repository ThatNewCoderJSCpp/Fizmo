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

inline int node_prec(NodeType t) noexcept {
    switch (t) {
        case NodeType::Add:
        case NodeType::Subtract:  return 1;
        case NodeType::Multiply:
        case NodeType::Divide:
        case NodeType::Modulo:    return 2;
        case NodeType::Negate:    return 3;
        case NodeType::Power:     return 4;
        default:                  return 5;
    }
}

std::string fmt_double(double v) {
    std::ostringstream oss;
    oss << v;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

inline void print_double(std::ostream& os, double v) {
    if (std::isinf(v)) { os << (v > 0.0 ? "\u221e" : "-\u221e"); return; }
    if (std::isnan(v)) { os << "NaN"; return; }
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        os << static_cast<long long>(v);
    } else {
        os << fmt_double(v);
    }
}

inline std::string to_superscript(int value, bool include_parentheses = false) {
    static const std::unordered_map<char, const char*> sup = {
        {'0',"⁰"}, {'1',"¹"}, {'2',"²"}, {'3',"³"}, {'4',"⁴"},
        {'5',"⁵"}, {'6',"⁶"}, {'7',"⁷"}, {'8',"⁸"}, {'9',"⁹"},
        {'+', "⁺"}, {'-', "⁻"}
    };

    std::string s = std::to_string(value);
    std::string out;
    if (include_parentheses) out += "⁽";

    for (char c : s) {
        auto it = sup.find(c);
        out += (it != sup.end() ? it->second : std::string(1, c));
    }

    if (include_parentheses) out += "⁾";
    return out;
}

inline std::string to_subscript(int value, bool include_parentheses = false) {
    static const std::unordered_map<char, const char*> sub = {
        {'0',"₀"}, {'1',"₁"}, {'2',"₂"}, {'3',"₃"}, {'4',"₄"},
        {'5',"₅"}, {'6',"₆"}, {'7',"₇"}, {'8',"₈"}, {'9',"₉"},
        {'+', "₊"}, {'-', "₋"}
    };

    std::string s = std::to_string(value);
    std::string out;
    if (include_parentheses) out += "₍";

    for (char c : s) {
        auto it = sub.find(c);
        out += (it != sub.end() ? it->second : std::string(1, c));
    }

    if (include_parentheses) out += "₎";
    return out;
}

inline const char* unary_fn_name(NodeType t) noexcept {
    switch (t) {
        case NodeType::Truncate:            return "trunc";
        case NodeType::FractionalPart:      return "frac";
        case NodeType::IntegerPart:         return "int";
        case NodeType::Sign:                return "sgn";
        case NodeType::UnitStep:            return "unit_step";
        case NodeType::NaturalExp:          return "exp";
        case NodeType::NaturalLog:          return "ln";
        case NodeType::Sqrt:                return "sqrt";
        case NodeType::Cbrt:                return "cbrt";
        case NodeType::Sin:                 return "sin";
        case NodeType::Sinc:                return "sinc";
        case NodeType::NormalSinc:          return "Nsinc";
        case NodeType::Cos:                 return "cos";
        case NodeType::Tan:                 return "tan";
        case NodeType::Csc:                 return "csc";
        case NodeType::Sec:                 return "sec";
        case NodeType::Cot:                 return "cot";
        case NodeType::Arcsin:              return "sin\u207b\u00b9";
        case NodeType::Arccos:              return "cos\u207b\u00b9";
        case NodeType::Arctan:              return "tan\u207b\u00b9";
        case NodeType::Arccsc:              return "csc\u207b\u00b9";
        case NodeType::Arcsec:              return "sec\u207b\u00b9";
        case NodeType::Arccot:              return "cot\u207b\u00b9";
        case NodeType::Sinh:                return "sinh";
        case NodeType::Cosh:                return "cosh";
        case NodeType::Tanh:                return "tanh";
        case NodeType::Csch:                return "csch";
        case NodeType::Sech:                return "sech";
        case NodeType::Coth:                return "coth";
        case NodeType::Arcsinh:             return "sinh\u207b\u00b9";
        case NodeType::Arccosh:             return "cosh\u207b\u00b9";
        case NodeType::Arctanh:             return "tanh\u207b\u00b9";
        case NodeType::Arccsch:             return "csch\u207b\u00b9";
        case NodeType::Arcsech:             return "sech\u207b\u00b9";
        case NodeType::Arccoth:             return "coth\u207b\u00b9";
        case NodeType::Erf:                 return "erf";
        case NodeType::Erfc:                return "erfc";
        case NodeType::Erfi:                return "erfi";
        case NodeType::InverseErf:          return "erf\u207b\u00b9";
        case NodeType::InverseErfc:         return "erfc\u207b\u00b9";
        case NodeType::InverseErfi:         return "erfi\u207b\u00b9";
        case NodeType::Gamma:               return "\u0393";      
        case NodeType::Digamma:             return "\u03c8";      
        case NodeType::Trigamma:            return "\u03c8\u2081"; 
        case NodeType::ExponentialIntegral: return "ei";
        case NodeType::LogarithmicIntegral: return "li";
        case NodeType::SinIntegral:         return "si";
        case NodeType::CosIntegral:         return "ci";
        case NodeType::SinhIntegral:        return "shi";
        case NodeType::CoshIntegral:        return "chi";
        case NodeType::FresnelS:            return "S";
        case NodeType::FresnelC:            return "C";
        case NodeType::Dilogarithm:         return "Li\u2082";
        case NodeType::Trilogarithm:        return "Li\u2083";
        case NodeType::RiemannZeta:         return "\u03b6";
        case NodeType::RogersL:             return "L";
        case NodeType::RogersLR:            return "L\u1d63"; 
        case NodeType::Gudermannian:        return "gd";
        case NodeType::InverseGudermannian: return "gd\u207b\u00b9";
        case NodeType::FibonacciSequence:   return "Fibonacci";
        case NodeType::LucasSequence:       return "Lucas";

        default: return "fn";
    }
}

inline void print_node(std::ostream& os, const MathExpressionNode* n, const VariableTable* vars, const VariableTable* funcs, int parent_prec = 0, bool right_child = false);

inline void print_node(
    std::ostream& os,
    const MathExpressionNode* n,
    const VariableTable* vars,
    const VariableTable* funcs,
    int parent_prec,
    bool right_child
) {
    if (is_invalid(n)) { os << "<Invalid>"; return; }
    if (!n) { os << "<null>"; return; }
    const int prec = node_prec(n->type);
    const bool need_parens = (prec < parent_prec) || (right_child && prec == parent_prec && prec <= 2);  
    if (need_parens) os << '(';

    switch (n->type) {
        case NodeType::Constant:
            print_double(os, n->constant);
            break;

        case NodeType::Variable:
            if (funcs && n->applied.func_id < funcs->size()) {
                os << funcs->name(n->applied.func_id);
            } else {
                os << 'f' << n->applied.func_id;
            }
            break;

        case NodeType::PositiveInfinity:  os << "\u221e";        break;
        case NodeType::NegativeInfinity:  os << "-\u221e";       break;
        case NodeType::NaN:               os << "NaN";           break;
        case NodeType::Undefined:         os << "Undefined";     break;
        case NodeType::Indeterminate:     os << "Indeterminate"; break;

        case NodeType::Negate:
            os << '-';
            print_node(os, n->unary.child, vars, funcs, prec);
            break;

        case NodeType::AbsoluteValue:
            os << '|';
            print_node(os, n->unary.child, vars, funcs);
            os << '|';
            break;

        case NodeType::Floor:
            os << "\u230a";                               
            print_node(os, n->unary.child, vars, funcs);
            os << "\u230b";                               
            break;

        case NodeType::Ceil:
            os << "\u2308";                               
            print_node(os, n->unary.child, vars, funcs);
            os << "\u2309";                               
            break;

        case NodeType::Round:
            os << '[';
            print_node(os, n->unary.child, vars, funcs);
            os << ']';
            break;

        case NodeType::Add:
            print_node(os, n->binary.left, vars, funcs, prec);
            os << " + ";
            print_node(os, n->binary.right, vars, funcs, prec);
            break;

        case NodeType::Subtract:
            print_node(os, n->binary.left,  vars, funcs, prec);
            os << " - ";
            print_node(os, n->binary.right, vars, funcs, prec, true);   
            break;

        case NodeType::Multiply:
            print_node(os, n->binary.left,  vars, funcs, prec);
            os << " * ";
            print_node(os, n->binary.right, vars, funcs, prec);
            break;

        case NodeType::Divide:
            print_node(os, n->binary.left,  vars, funcs, prec);
            os << " / ";
            print_node(os, n->binary.right, vars, funcs, prec, true);  
            break;

        case NodeType::Modulo:
            print_node(os, n->binary.left,  vars, funcs, prec);
            os << " % ";
            print_node(os, n->binary.right, vars, funcs, prec, true);
            break;

        case NodeType::Power:
            print_node(os, n->power.base, vars, funcs, prec);
            os << '^';
            print_node(os, n->power.exponent, vars, funcs, prec - 1);
            break;

        case NodeType::Log:
            os << "log(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::Root:
            os << "root(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::Factorial:
            os << "(";
            print_node(os, n->unary.child, vars, funcs, prec);
            os << ")!";
            break;

        case NodeType::Polygamma: {
            os << "\u03c8";

            if (n->binary.right->type == NodeType::Constant) {
                int branch = static_cast<int>(n->binary.right->constant);
                os << to_subscript(branch);
            } else {
                os << "_{";
                print_node(os, n->binary.right, vars, funcs);
                os << '}';
            }

            os << '(';
            print_node(os, n->binary.left, vars, funcs);     
            os << ')';
            break;
        }

        case NodeType::ErfGeneralized:
            os << "erf(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::ErfcGeneralized:
            os << "erf(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::BetaFunction:
            os << "\u03b2(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::LambertW:
            os << 'W';

            if (n->binary.right->type == NodeType::Constant) {
                int branch = static_cast<int>(n->binary.right->constant);

                if (branch != 0) {                
                    os << '_';
                    if (branch < 0) os << "(" << branch << ")";
                    else            os << branch;
                }
            } else {
                os << "_{";
                print_node(os, n->binary.right, vars, funcs);
                os << '}';
            }

            os << '(';
            print_node(os, n->binary.left, vars, funcs);     
            os << ')';
            break;

        case NodeType::ChebyshevU: 
            os << "U(";
            print_node(os, n->binary.left, vars, funcs);
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);
            os << ")";
            break;

        case NodeType::ChebyshevT: 
            os << "T(";
            print_node(os, n->binary.left, vars, funcs);
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);
            os << ")";
            break;

        case NodeType::ExponentialIntegralGeneralized:
            os << "ei_";

            if (n->binary.right->type == NodeType::Constant) {
                double v = n->binary.right->constant;
                if (v == std::floor(v) && std::abs(v) < 1e15) os << static_cast<long long>(v);
                else { os << '{'; print_node(os, n->binary.right, vars, funcs); os << '}'; }
            } else {
                os << '{'; print_node(os, n->binary.right, vars, funcs); os << '}';
            }

            os << '(';
            print_node(os, n->binary.left, vars, funcs);
            os << ')';
            break;

        case NodeType::LogarithmicIntegralGeneralized:
            os << "li_";

            if (n->binary.right->type == NodeType::Constant) {
                double v = n->binary.right->constant;
                if (v == std::floor(v) && std::abs(v) < 1e15) os << static_cast<long long>(v);
                else { os << '{'; print_node(os, n->binary.right, vars, funcs); os << '}'; }
            } else {
                os << '{'; print_node(os, n->binary.right, vars, funcs); os << '}';
            }

            os << '(';
            print_node(os, n->binary.left, vars, funcs);
            os << ')';
            break;

        case NodeType::BellPolynomial:
            os << 'B';

            if (n->binary.right->type == NodeType::Constant) {
                unsigned int idx = static_cast<unsigned int>(n->binary.right->constant);
                os << to_subscript(idx);
            }

            os << '(';
            print_node(os, n->binary.left, vars, funcs);
            os << ')';
            break;

        case NodeType::Polylog: {
            os << "Li_{";
            print_node(os, n->binary.right, vars, funcs);
            os << "}(";
            print_node(os, n->binary.left, vars, funcs);
            os << ")";
            break;
        }

        case NodeType::Combination:
            os << "C(";
            print_node(os, n->binary.left, vars, funcs);
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);
            os << ')';
            break;

        case NodeType::Permutation:
            os << "P(";
            print_node(os, n->binary.left, vars, funcs);
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);
            os << ')';
            break;

        case NodeType::FibonacciPolynomial:
            os << "FibPoly(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::LucasPolynomial:
            os << "LucasPoly(";
            print_node(os, n->binary.left, vars, funcs);   
            os << ", ";
            print_node(os, n->binary.right, vars, funcs);   
            os << ')';
            break;

        case NodeType::AppliedFunction: {
            if (vars && n->applied.func_id < vars->size()) {
                os << vars->name(n->applied.func_id);
            } else {
                os << 'f' << n->applied.func_id;
            }

            std::uint64_t total = 0;
            for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) total += n->applied.orders[i];

            if (total > 0) {                       
                os << "_{";
                bool first = true;

                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i)
                    for (std::uint64_t k = 0; k < n->applied.orders[i]; ++k) {
                        if (!first) os << ',';
                        print_node(os, n->applied.args[i], vars, 0);
                        first = false;
                    }

                os << '}';
            }

            os << '(';

            for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) {
                if (i) os << ", ";
                print_node(os, n->applied.args[i], vars, 0);
            }
            
            os << ')';
            break;
        }

        default:
            os << unary_fn_name(n->type) << '(';
            print_node(os, n->unary.child, vars, funcs);
            os << ')';
            break;
    }

    if (need_parens) os << ')';
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_PRINTER_HPP