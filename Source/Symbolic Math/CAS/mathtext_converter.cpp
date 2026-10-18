#include "fizmo_library.hpp"

#include <cstdio>
#include <vector>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

namespace {

class BraceWriter {
public:
    BraceWriter(const VariableTable* vars, const VariableTable* funcs) : m_vars(vars), m_funcs(funcs) {}

    std::string write(const MathExpressionNode* n) {
        if (!n) return quoted("invalid");

        switch (n->type) {
            case NodeType::Constant:         return constant(n);
            case NodeType::Variable:         return quoted(variable_name(n->variable.var_id));
            case NodeType::Pi:               return "\\pi\\";
            case NodeType::EulerNumber:      return quoted("e");
            case NodeType::PositiveInfinity: return "\\infty\\";
            case NodeType::NegativeInfinity: return call("negate", { "\\infty\\" });
            case NodeType::NaN:              return quoted("NaN");
            case NodeType::Undefined:        return quoted("undefined");
            case NodeType::Indeterminate:    return quoted("indeterminate");
            case NodeType::Invalid:          return quoted("invalid");

            case NodeType::AppliedFunction:  return applied(n);

            case NodeType::Add: {
                std::vector<std::string> terms;
                flatten(n, NodeType::Add, terms);
                return call("add", terms);
            }

            case NodeType::Multiply: {
                std::vector<std::string> factors;
                flatten(n, NodeType::Multiply, factors);
                return call("multiply", factors);
            }

            case NodeType::Subtract:       return bin("subtract", n);
            case NodeType::Divide:         return bin("fraction", n);
            case NodeType::Negate:         return un("negate", n);
            case NodeType::Modulo:         return bin("mod", n);
            case NodeType::Power:          return call("power", { write(n->power.base), write(n->power.exponent) });

            case NodeType::Sqrt:           return un("sqrt", n);
            case NodeType::Cbrt:           return call("root", { write(n->unary.child), "3" });
            case NodeType::Root:           return bin("root", n);
            case NodeType::NaturalExp:     return call("power", { quoted("e"), write(n->unary.child) });
            case NodeType::NaturalLog:     return un("ln", n);
            case NodeType::Log:            return applied_head(call("subscript", { quoted("log"), write(n->binary.right) }), { write(n->binary.left) });

            case NodeType::AbsoluteValue:  return un("abs", n);
            case NodeType::Floor:          return un("floor", n);
            case NodeType::Ceil:           return un("ceil", n);
            case NodeType::Round:          return un("round", n);
            case NodeType::Truncate:       return un("trunc", n);
            case NodeType::FractionalPart: return un("frac", n);
            case NodeType::IntegerPart:    return un("int", n);
            case NodeType::Sign:           return un("sgn", n);
            case NodeType::UnitStep:       return un("H", n);
            case NodeType::Factorial:      return un("factorial", n);
            case NodeType::Combination:    return bin("C", n);
            case NodeType::Permutation:    return bin("P", n);

            case NodeType::Sin:            return un("sin", n);
            case NodeType::Cos:            return un("cos", n);
            case NodeType::Tan:            return un("tan", n);
            case NodeType::Csc:            return un("csc", n);
            case NodeType::Sec:            return un("sec", n);
            case NodeType::Cot:            return un("cot", n);
            case NodeType::Arcsin:         return un("arcsin", n);
            case NodeType::Arccos:         return un("arccos", n);
            case NodeType::Arctan:         return un("arctan", n);
            case NodeType::Arccsc:         return un("arccsc", n);
            case NodeType::Arcsec:         return un("arcsec", n);
            case NodeType::Arccot:         return un("arccot", n);
            case NodeType::Sinh:           return un("sinh", n);
            case NodeType::Cosh:           return un("cosh", n);
            case NodeType::Tanh:           return un("tanh", n);
            case NodeType::Csch:           return un("csch", n);
            case NodeType::Sech:           return un("sech", n);
            case NodeType::Coth:           return un("coth", n);
            case NodeType::Arcsinh:        return un("arsinh", n);
            case NodeType::Arccosh:        return un("arcosh", n);
            case NodeType::Arctanh:        return un("artanh", n);
            case NodeType::Arccsch:        return un("arcsch", n);
            case NodeType::Arcsech:        return un("arsech", n);
            case NodeType::Arccoth:        return un("arcoth", n);
            case NodeType::Sinc:           return un("sinc", n);
            case NodeType::NormalSinc:     return applied_head(call("subscript", { quoted("sinc"), "\\pi\\" }), { write(n->unary.child) });

            case NodeType::Erf:             return un("erf", n);
            case NodeType::Erfc:            return un("erfc", n);
            case NodeType::Erfi:            return un("erfi", n);
            case NodeType::ErfGeneralized:  return bin("erf", n);
            case NodeType::ErfcGeneralized: return bin("erfc", n);
            case NodeType::InverseErf:      return inverse("erf", n);
            case NodeType::InverseErfc:     return inverse("erfc", n);
            case NodeType::InverseErfi:     return inverse("erfi", n);

            case NodeType::Gamma:        return call("\\Gamma\\", { write(n->unary.child) });
            case NodeType::Digamma:      return call("\\psi\\", { write(n->unary.child) });
            case NodeType::Trigamma:     return applied_head(call("subscript", { "\\psi\\", "1" }), { write(n->unary.child) });
            case NodeType::Polygamma:    return applied_head(call("power", { "\\psi\\", call("parens", { write(n->binary.right) }) }), { write(n->binary.left) });
            case NodeType::BetaFunction: return bin("B", n);

            case NodeType::LambertW: {
                const MathExpressionNode* k = n->binary.right;
                if (k->type == NodeType::Constant && k->exact && k->exact->is_zero()) return call("W", { write(n->binary.left) });
                return applied_head(call("subscript", { quoted("W"), write(k) }), { write(n->binary.left) });
            }

            case NodeType::ChebyshevT:          return indexed("T", n->binary.right, n->binary.left);
            case NodeType::ChebyshevU:          return indexed("U", n->binary.right, n->binary.left);
            case NodeType::BellPolynomial:      return indexed("B", n->binary.right, n->binary.left);
            case NodeType::FibonacciPolynomial: return indexed("F", n->binary.right, n->binary.left);
            case NodeType::LucasPolynomial:     return indexed("L", n->binary.right, n->binary.left);
            case NodeType::FibonacciSequence:   return call("subscript", { quoted("F"), write(n->unary.child) });
            case NodeType::LucasSequence:       return call("subscript", { quoted("L"), write(n->unary.child) });

            case NodeType::ExponentialIntegral:            return un("Ei", n);
            case NodeType::LogarithmicIntegral:            return un("li", n);
            case NodeType::ExponentialIntegralGeneralized: return indexed("E", n->binary.right, n->binary.left);
            case NodeType::LogarithmicIntegralGeneralized: return indexed("li", n->binary.right, n->binary.left);
            case NodeType::SinIntegral:                    return un("Si", n);
            case NodeType::CosIntegral:                    return un("Ci", n);
            case NodeType::SinhIntegral:                   return un("Shi", n);
            case NodeType::CoshIntegral:                   return un("Chi", n);
            case NodeType::FresnelS:                       return un("S", n);
            case NodeType::FresnelC:                       return un("C", n);

            case NodeType::RiemannZeta:  return call("\\zeta\\", { write(n->unary.child) });
            case NodeType::Dilogarithm:  return applied_head(call("subscript", { quoted("Li"), "2" }), { write(n->unary.child) });
            case NodeType::Trilogarithm: return applied_head(call("subscript", { quoted("Li"), "3" }), { write(n->unary.child) });
            case NodeType::Polylog:      return indexed("Li", n->binary.right, n->binary.left);

            case NodeType::SpenceFunction:      return un("Sp", n);
            case NodeType::SpenceIntegral:      return un("Spi", n);
            case NodeType::RogersL:             return un("L", n);
            case NodeType::RogersLR:            return applied_head(call("subscript", { quoted("L"), quoted("R") }), { write(n->unary.child) });
            case NodeType::Gudermannian:        return un("gd", n);
            case NodeType::InverseGudermannian: return inverse("gd", n);

            default:
                if (NodeKeyHash::is_unary(n->type))  return un("f", n);
                if (NodeKeyHash::is_binary(n->type)) return bin("f", n);
                return quoted("?");
        }
    }

private:
    static std::string quoted(const std::string& text) {
        std::string out = "\"";

        for (char c : text) {
            if (c == '"' || c == '\\') out.push_back('\\');
            out.push_back(c);
        }

        out.push_back('"');
        return out;
    }

    static bool plain_name(const std::string& name) {
        if (name.empty()) return false;
        if (!((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z'))) return false;

        for (char c : name) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
            if (!ok) return false;
        }

        return !::fizmo::mathtext::is_call_spelling(name) || ::fizmo::mathtext::canonical_call(name) == name;
    }

    static std::string call(const std::string& head, const std::vector<std::string>& args) {
        std::string out = head;
        out.push_back('{');

        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) out += ", ";
            out += args[i];
        }

        out.push_back('}');
        return out;
    }

    static std::string applied_head(const std::string& head, const std::vector<std::string>& args) {
        return call("implicit_multiply", { head, call("parens", args) });
    }

    static std::string function(const std::string& name, const std::vector<std::string>& args) {
        if (plain_name(name) || (name.size() > 2 && name.front() == '\\' && name.back() == '\\')) return call(name, args);
        return applied_head(quoted(name), args);
    }

    std::string un(const std::string& name, const MathExpressionNode* n) { return function(name, { write(n->unary.child) }); }

    std::string bin(const std::string& name, const MathExpressionNode* n) { return function(name, { write(n->binary.left), write(n->binary.right) }); }

    std::string inverse(const std::string& name, const MathExpressionNode* n) {
        return applied_head(call("power", { quoted(name), "-1" }), { write(n->unary.child) });
    }

    std::string indexed(const std::string& name, const MathExpressionNode* index, const MathExpressionNode* arg) {
        return applied_head(call("subscript", { quoted(name), write(index) }), { write(arg) });
    }

    std::string constant(const MathExpressionNode* n) {
        if (!n->exact) {
            const double v = n->constant;
            if (std::isnan(v)) return quoted("NaN");
            if (std::isinf(v)) return v > 0.0 ? "\\infty\\" : call("negate", { "\\infty\\" });
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.17g", v);
            return buf;
        }

        const Rational& q = *n->exact;
        if (q.is_integer()) return q.numerator().to_string();
        const std::string body = call("fraction", { BigInt(q.numerator().magnitude(), false).to_string(), q.denominator().to_string() });
        return q.is_negative() ? call("negate", { body }) : body;
    }

    std::string variable_name(std::uint64_t id) const {
        if (m_vars && id < m_vars->size()) return m_vars->name(id);
        return "x" + std::to_string(id);
    }

    void flatten(const MathExpressionNode* n, NodeType t, std::vector<std::string>& out) {
        if (n->type == t) {
            flatten(n->binary.left, t, out);
            flatten(n->binary.right, t, out);
            return;
        }

        out.push_back(write(n));
    }

    std::string applied(const MathExpressionNode* n) {
        std::string name = (m_funcs && n->applied.func_id < m_funcs->size()) ? m_funcs->name(n->applied.func_id) : ("f" + std::to_string(n->applied.func_id));
        std::vector<std::string> args;
        std::string wrt;

        for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) {
            const MathExpressionNode* a = n->applied.args[i];
            args.push_back(write(a));
            const std::string label = a->type == NodeType::Variable ? variable_name(a->variable.var_id) : std::to_string(i + 1);
            for (std::uint64_t k = 0; k < n->applied.orders[i]; ++k) wrt += label;
        }

        if (wrt.empty()) return function(name, args);
        return applied_head(call("subscript", { quoted(name), quoted(wrt) }), args);
    }

    const VariableTable* m_vars;
    const VariableTable* m_funcs;
};

} // namespace

std::string to_mathtext(const MathExpressionNode* n, const VariableTable* vars, const VariableTable* funcs, MathTextSyntax syntax) {
    const std::string brace = BraceWriter(vars, funcs).write(n);
    if (syntax == MathTextSyntax::Math) return ::fizmo::mathtext::brace_to_math(brace);
    return brace;
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
