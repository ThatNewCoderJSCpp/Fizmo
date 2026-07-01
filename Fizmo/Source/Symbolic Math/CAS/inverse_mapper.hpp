#ifndef FIZMO_MATH_INVERSE_MAPPER_HPP
#define FIZMO_MATH_INVERSE_MAPPER_HPP

#include "../Logic/relation.hpp"
#include "symbolic_function.hpp"
#include <string>
#include <vector>

namespace fizmo {
namespace math {
namespace cas {

enum class InverseKind : std::uint8_t {
    Bijective = 0,
    MultivaluedFinite,
    MultivaluedPeriodic,
    Regional,
    Unsupported
};

struct InverseBranch {
    logic::Predicate constraint;                   
    logic::Predicate output_domain = logic::Predicate(true); 
    std::string index_var;   

    bool has_index_var() const noexcept { return !index_var.empty(); }
};

struct InverseResult {
    InverseKind kind = InverseKind::Unsupported;
    std::vector<InverseBranch> branches;

    bool defined() const { return kind != InverseKind::Unsupported && !branches.empty(); }

    logic::Predicate combined() const {
        if (branches.empty()) return logic::Predicate(false);
        std::vector<logic::Predicate> ors;
        ors.reserve(branches.size());
        for (auto& b : branches) ors.push_back(AND(b.constraint, b.output_domain));
        if (ors.size() == 1) return ors[0];
        return OR(std::move(ors));
    }
};

class InverseMapper {
public:
    static InverseResult invert(
        symbols::NodeType fn,
        const Expression& x,
        const Expression& y,
        SymbolicContext& ctx,
        const std::string& index_var = "k"
    ) {
        using NT = symbols::NodeType;
        auto& mgr = y.manager();
        auto K = [&](double v) { return Expression(v, mgr); };

        switch (fn) {
            case NT::Sqrt:        return bijective(x, POW(y, 2.0), logic::IsNonNegative(y));
            case NT::Cbrt:        return bijective(x, POW(y, 3.0), logic::Predicate(true));
            case NT::NaturalExp:  return bijective(x, LN(y), logic::IsPositive(y));
            case NT::NaturalLog:  return bijective(x, EXP(y), logic::Predicate(true));
            case NT::Negate:      return bijective(x, -y, logic::Predicate(true));
            case NT::Floor:       return regional(x >= y, x < y + 1.0, logic::IsInteger(y));
            case NT::Ceil:        return regional(x > y - 1.0, x <= y, logic::IsInteger(y));
            case NT::Round:       return regional(x >= y - 0.5, x < y + 0.5, logic::IsInteger(y));

            case NT::Truncate:
            case NT::IntegerPart: { 
                InverseResult r; r.kind = InverseKind::Regional;
                r.branches.push_back({ logic::AND(x >= y, x < y + 1.0), logic::AND(y > 0.0, logic::IsInteger(y)), "" });
                r.branches.push_back({ logic::AND(x > y - 1.0, x <= y), logic::AND(y < 0.0, logic::IsInteger(y)), "" });
                r.branches.push_back({ logic::AND(x > -1.0, x < 1.0),   logic::Predicate(logic::EQUALS(y, 0.0)), "" });
                return r;
            }

            case NT::FractionalPart: {
                return periodic(x, { y }, AND(y >= 0.0, y < 1.0), index_var, K(1.0));
            }

            case NT::Sign: {
                InverseResult r; r.kind = InverseKind::Regional;
                r.branches.push_back({ logic::Predicate(x > 0.0), logic::Predicate(logic::EQUALS(y, 1.0)), "" });
                r.branches.push_back({ logic::Predicate(logic::IsZero(x)), logic::Predicate(logic::EQUALS(y, 0.0)), "" });
                r.branches.push_back({ logic::Predicate(x < 0.0), logic::Predicate(logic::EQUALS(y, -1.0)), "" });
                return r;
            }

            case NT::UnitStep: {
                InverseResult r; r.kind = InverseKind::Regional;
                r.branches.push_back({ logic::Predicate(x >= 0.0), logic::Predicate(logic::EQUALS(y, 1.0)), "" });
                r.branches.push_back({ logic::Predicate(x <  0.0), logic::Predicate(logic::EQUALS(y, 0.0)), "" });
                return r;
            }

            case NT::Sin: {
                Expression base = ASIN(y);
                return periodic(x, { base, K(constants::pi()) - base }, AND(y >= -1.0, y <= 1.0), index_var, K(2.0 * constants::pi()));
            }

            case NT::Cos: {
                Expression base = ACOS(y);
                return periodic(x, { base, -base }, AND(y >= -1.0, y <= 1.0), index_var, K(2.0 * constants::pi()));
            }

            case NT::Tan: return periodic(x, { ATAN(y) }, logic::Predicate(true), index_var, K(constants::pi()));

            case NT::Csc: {
                Expression base = ACSC(y);
                logic::Predicate dom = logic::OR(logic::Predicate(y >= 1.0), logic::Predicate(y <= -1.0));
                return periodic(x, { base, K(constants::pi()) - base }, dom, index_var, K(2.0 * constants::pi()));
            }

            case NT::Sec: {
                Expression base = ASEC(y);
                logic::Predicate dom = logic::OR(logic::Predicate(y >= 1.0), logic::Predicate(y <= -1.0));
                return periodic(x, { base, -base }, dom, index_var, K(2.0 * constants::pi()));
            }

            case NT::Cot: return periodic(x, { ACOT(y) }, logic::Predicate(true), index_var, K(constants::pi()));

            case NT::Sinh: return bijective(x, ASINH(y), logic::Predicate(true));
            case NT::Tanh: return bijective(x, ATANH(y), AND(y > -1.0, y < 1.0));
            case NT::Cosh: return finite(x, { ACOSH(y), -ACOSH(y) }, y >= 1.0);
            case NT::Csch: return bijective(x, ACSCH(y), logic::Predicate(logic::IsNonZero(y)));
            case NT::Sech: return finite(x, { ASECH(y), -ASECH(y) }, AND(y > 0.0, y <= 1.0));

            case NT::Coth: {
                logic::Predicate dom = logic::OR(logic::Predicate(y > 1.0), logic::Predicate(y < -1.0));
                return bijective(x, ACOTH(y), dom);
            }

            case NT::Arcsin: return bijective(x, SIN(y), AND(y >= -constants::pi() / 2.0, y <= constants::pi() / 2.0));
            case NT::Arccos: return bijective(x, COS(y), AND(y >= 0.0, y <= constants::pi()));
            case NT::Arctan: return bijective(x, TAN(y), AND(y > -constants::pi() / 2.0, y < constants::pi() / 2.0));
            case NT::Arccsc: return bijective(x, CSC(y), AND(y >= -constants::pi() / 2.0, y <= constants::pi() / 2.0)); 
            case NT::Arcsec: return bijective(x, SEC(y), AND(y >= 0.0, y <= constants::pi()));                         
            case NT::Arccot: return bijective(x, COT(y), AND(y > 0.0, y < constants::pi()));

            case NT::Arcsinh:  return bijective(x, SINH(y), logic::Predicate(true));
            case NT::Arccosh:  return bijective(x, COSH(y), logic::IsNonNegative(y));  
            case NT::Arctanh:  return bijective(x, TANH(y), logic::Predicate(true));
            case NT::Arccsch:  return bijective(x, CSCH(y), logic::Predicate(logic::IsNonZero(y)));
            case NT::Arcsech:  return bijective(x, SECH(y), logic::IsNonNegative(y));   
            case NT::Arccoth:  return bijective(x, COTH(y), logic::Predicate(logic::IsNonZero(y)));

            case NT::Gudermannian:        return bijective(x, INVERSE_GUDERMANNIAN(y), AND(y > -constants::pi() / 2.0, y < constants::pi() / 2.0));
            case NT::InverseGudermannian: return bijective(x, GUDERMANNIAN(y), logic::Predicate(true));

            case NT::Erf:  return bijective(x, INVERSE_ERF(y),  AND(y > -1.0, y < 1.0));
            case NT::Erfc: return bijective(x, INVERSE_ERFC(y), AND(y > 0.0, y < 2.0));
            case NT::Erfi: return bijective(x, INVERSE_ERFI(y), logic::Predicate(true));

            case NT::InverseErf:  return bijective(x, ERF(y),  logic::Predicate(true));
            case NT::InverseErfc: return bijective(x, ERFC(y), logic::Predicate(true));
            case NT::InverseErfi: return bijective(x, ERFI(y), logic::Predicate(true));

            default:
                return InverseResult{};
        }
    }

    static InverseResult invert_binary(
        symbols::NodeType fn,
        int wrt_arg,                  
        const Expression& other_arg, 
        const Expression& x,         
        const Expression& y
    ) {
        using NT = symbols::NodeType;

        switch (fn) {
            case NT::LambertW: {
                if (wrt_arg != 0) return InverseResult{};
                bool has_k = false;
                double k = as_constant(other_arg, has_k);
                if (!has_k) return InverseResult{};   

                logic::Predicate dom =
                      (k ==  0.0) ? logic::Predicate(y >= -1.0)
                    : (k == -1.0) ? logic::Predicate(y <= -1.0)
                    :               logic::Predicate(false);

                return bijective(x, y * EXP(y), dom);
            }

            default:
                return InverseResult{};
        }
    }

private:
    static InverseResult bijective(const Expression& x, const Expression& solution, logic::Predicate output_domain) {
        InverseResult r; r.kind = InverseKind::Bijective;
        r.branches.push_back({ logic::Predicate(logic::EQUALS(x, solution)), std::move(output_domain), "" });
        return r;
    }

    static InverseResult finite(const Expression& x, std::vector<Expression> solutions, logic::Predicate output_domain) {
        InverseResult r; r.kind = InverseKind::MultivaluedFinite;
        for (auto& s : solutions) r.branches.push_back({ logic::Predicate(logic::EQUALS(x, s)), output_domain, "" });
        return r;
    }

    static InverseResult periodic(
        const Expression& x, std::vector<Expression> base_solutions, logic::Predicate output_domain,
        const std::string& index_var, const Expression& period
    ) {
        InverseResult r; r.kind = InverseKind::MultivaluedPeriodic;
        Expression k(index_var, base_solutions.front().manager());
        for (auto& s : base_solutions) {
            r.branches.push_back({ logic::Predicate(logic::EQUALS(x, s + period * k)), output_domain, index_var });
        }
        return r;
    }

    static InverseResult regional(logic::Relation lo, logic::Relation hi, logic::Predicate output_domain) {
        InverseResult r; r.kind = InverseKind::Regional;
        r.branches.push_back({ logic::AND(logic::Predicate(lo), logic::Predicate(hi)), std::move(output_domain), "" });
        return r;
    }

    static double as_constant(const Expression& e, bool& found) {
        const auto* n = e.inner().get();

        if (n && n->type == symbols::NodeType::Constant) {
            found = true;
            return n->constant;
        }

        found = false;
        return 0.0;
    }
};

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_INVERSE_MAPPER_HPP