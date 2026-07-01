#ifndef FIZMO_SOURCE_SYMBOLIC_MATH_CAS_DOMAIN_ANALYSIS_HPP
#define FIZMO_SOURCE_SYMBOLIC_MATH_CAS_DOMAIN_ANALYSIS_HPP

#include "symbolic_function.hpp"
#include "../Logic/relation.hpp"
#include "../Geometry/implicit_curve.hpp"
#include <unordered_set>
#include <vector>
#include <cmath>

namespace fizmo {
namespace math {
namespace cas {

struct Rational {
    long long num;
    long long den;
    bool valid;
};

static Rational extract_rational(const Expression& e) {
    Rational out{0,1,false};
    auto* n = e.node();
    if (!n) return out;
    using NT = symbols::NodeType;

    if (n->type == NT::Constant) {
        double v = n->constant;
        long long k;

        if (logic::detail::integer_value_in_range(v, constants::middle_epsilon(), k)) {
            out.num = k;
            out.den = 1;
            out.valid = true;
            return out;
        }

        return out;
    }

    if (n->type == NT::Divide) {
        auto* L = n->binary.left;
        auto* R = n->binary.right;

        if (L->type == NT::Constant && R->type == NT::Constant) {
            long long p, q;
            if (!logic::detail::integer_value_in_range(L->constant, constants::middle_epsilon(), p)) return out;
            if (!logic::detail::integer_value_in_range(R->constant, constants::middle_epsilon(), q)) return out;
            if (q == 0) return out;
            out.num = p;
            out.den = q;
            out.valid = true;
            return out;
        }
    }

    if (n->type == NT::Negate) {
        Expression child(symbols::MathExpression(n->unary.child), e.manager());
        Rational r = extract_rational(child);
        if (!r.valid) return out;
        r.num = -r.num;
        return r;
    }

    return out;
}

class DomainAnalysis {
public:
    static logic::Predicate analyze(const Expression& e) {
        if (e.is_invalid()) return logic::Predicate(true);
        std::vector<logic::Predicate> constraints;
        walk(e.node(), e.manager(), constraints);
        if (constraints.empty()) return logic::Predicate(true);
        return logic::AND(std::move(constraints)).simplify();
    }

    static logic::Predicate analyze(symbols::MathExpression e, symbols::MathExpressionManager& mgr) {
        return analyze(Expression(e, mgr));
    }

private:
    static Expression wrap(symbols::MathExpressionNode* n, symbols::MathExpressionManager& mgr) {
        return Expression(symbols::MathExpression(n), mgr);
    }

    static void emit_local(
        symbols::MathExpressionNode* n,
        symbols::MathExpressionManager& mgr,
        std::vector<logic::Predicate>& out
    ) {
        using NT = symbols::NodeType;

        switch (n->type) {
            case NT::Divide:
            case NT::Modulo: {                              
                Expression den = wrap(n->binary.right, mgr);
                out.push_back(logic::IsNonZero(den));
                break;
            }

            case NT::Power: {
                Expression base = wrap(n->power.base, mgr);
                Expression exp  = wrap(n->power.exponent, mgr);
                Rational r = extract_rational(exp);

                if (r.valid) {
                    long long p = r.num;
                    long long q = r.den;
                    std::vector<logic::Predicate> preds;
                    preds.reserve(3);

                    if (q <= 0) {
                        out.push_back(logic::Predicate(false));
                        break;
                    }

                    if (p < 0) preds.push_back(logic::IsNonZero(base));

                    if ((q % 2) == 0) {
                        preds.push_back(logic::IsNonNegative(base));
                    } else {
                        preds.push_back(logic::Predicate(true));
                    }

                    out.push_back(logic::AND(std::move(preds)));
                    break;
                }

                out.push_back(
                    logic::OR({
                        logic::IsPositive(base),
                        logic::AND(logic::IsZero(base), logic::IsPositive(exp)),
                        logic::AND(logic::IsNegative(base), logic::IsInteger(exp))
                    })
                );

                break;
            }

            case NT::NaturalLog: {                          
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsPositive(a));
                break;
            }

            case NT::Log: {                                 
                Expression base = wrap(n->binary.right, mgr);
                Expression arg  = wrap(n->binary.left , mgr);
                
                out.push_back(
                    logic::AND({
                        logic::IsPositive(base),
                        logic::IsPositive(arg),
                        logic::NOT_EQUALS(base, 1.0)
                    })
                );

                break;
            }

            case NT::Sqrt: {                                
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonNegative(a));
                break;
            }

            case NT::Root: {
                Expression radicand = wrap(n->binary.left,  mgr);
                Expression degree   = wrap(n->binary.right, mgr);
                Rational r = extract_rational(degree);
                std::vector<logic::Predicate> preds;
                preds.reserve(3);
                preds.push_back(logic::IsNonZero(degree));

                if (r.valid) {
                    long long k = r.num;
                    long long q = r.den;

                    if (q == 1) {
                        if ((k % 2) == 0) {
                            preds.push_back(logic::IsNonNegative(radicand));
                        } else {
                            preds.push_back(logic::Predicate(true));
                        }
                    } else {
                        preds.push_back(logic::IsNonNegative(radicand));
                    }
                } else {
                    preds.push_back(logic::IsNonNegative(radicand));
                }

                out.push_back(logic::AND(std::move(preds)));
                break;
            }

            case NT::Cot:
            case NT::Csc: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(SIN(a)));
                break;
            }
            
            case NT::Tan:
            case NT::Sec: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(COS(a)));
                break;
            }

            case NT::Coth:
            case NT::Csch: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(SINH(a)));
                break;
            }
            
            case NT::Tanh:
            case NT::Sech: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(COSH(a)));
                break;
            }

            case NT::Arcsin:
            case NT::Arccos: {                              
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(a >= -1.0, a <= 1.0));
                break;
            }

            case NT::Arcsec:
            case NT::Arccsc: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::OR(a <= -1.0, a >= 1.0));
                break;
            }

            case NT::Arccosh: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(a >= 1.0);
                break;
            }

            case NT::Arctanh: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(a > -1.0, a < 1.0));
                break;
            }

            case NT::Arccoth: {                            
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::OR(a < -1.0, a > 1.0));
                break;
            }

            case NT::Arcsech: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(logic::IsPositive(a), a <= 1.0));
                break;
            }

            case NT::Arccsch: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(a));
                break;
            }

            case NT::ChebyshevT: {
                Expression arg  = wrap(n->binary.left, mgr);
                out.push_back(logic::AND(arg > -1.0, arg < 1.0));
                break;
            }

            case NT::ChebyshevU: {
                Expression arg  = wrap(n->binary.left, mgr);
                out.push_back(logic::AND(arg >= -1.0, arg <= 1.0));
                break;
            }

            case NT::InverseErf: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(a > -1.0, a < 1.0));
                break;
            }

            case NT::InverseErfc: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(logic::IsPositive(a), a < 2.0));
                break;
            }

            case NT::ExponentialIntegral: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonZero(a));
                break;
            }

            case NT::ExponentialIntegralGeneralized: {
                Expression arg  = wrap(n->binary.left , mgr);
                Expression base = wrap(n->binary.right, mgr);
                
                out.push_back(
                    logic::OR({
                        logic::AND(logic::EQUALS(base, 1.0), logic::IsNegative(arg)),
                        logic::AND(base <= 1.0, logic::IsZero(arg)),
                        logic::AND(logic::NOT_EQUALS(base, 1.0), logic::IsPositive(arg))
                    })
                );

                break;
            }

            case NT::LogarithmicIntegral: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(logic::Predicate(a >= 0.0), logic::Predicate(logic::NOT_EQUALS(a, 1.0))));
                break;
            }
            
            case NT::LogarithmicIntegralGeneralized: {
                Expression arg  = wrap(n->binary.left , mgr);
                Expression base = wrap(n->binary.right, mgr);

                out.push_back(
                    logic::OR({
                        logic::AND(
                            logic::Predicate(logic::EQUALS(base, 1.0)),
                            logic::Predicate(arg > 1.0)
                        ),
                        logic::AND(
                            logic::Predicate(base <= 1.0),
                            logic::Predicate(logic::EQUALS(arg, 1.0))
                        ),
                        logic::AND({
                            logic::Predicate(logic::NOT_EQUALS(base, 1.0)),
                            logic::Predicate(arg > 0.0),
                            logic::Predicate(arg < 1.0),
                            logic::Predicate(logic::IsInteger(1.0 - base))   
                        })
                    })
                );

                break;
            }

            case NT::Gamma:
            case NT::Digamma:
            case NT::Trigamma: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNotNonPositiveInteger(a));
                break;
            }

            case NT::Factorial: {                             
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNotNegativeInteger(a));
                break;
            }

            case NT::Polygamma: {
                Expression arg  = wrap(n->binary.left , mgr);
                Expression base = wrap(n->binary.right, mgr);

                out.push_back(
                    logic::AND(
                        logic::IsInteger(base),
                        logic::OR({
                            logic::AND(logic::EQUALS(base, -1.0),  logic::IsPositive(arg)),                  
                            logic::AND(base < -1.0,         logic::IsNonNegative(arg)),               
                            logic::AND(logic::IsNonNegative(base), logic::IsNotNonPositiveInteger(arg)) 
                        })
                    )
                );

                break;
            }

            case NT::InverseGudermannian: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(a > -constants::pi_2(), a < constants::pi_2()));
                break;
            }

            case NT::LambertW: {
                Expression arg  = wrap(n->binary.left , mgr);
                Expression base = wrap(n->binary.right, mgr);

                out.push_back(
                    logic::AND(
                        logic::IsInteger(base),
                        logic::OR({
                            logic::AND(logic::IsZero(base), arg >= -constants::reciprocal_e()),
                            logic::AND(logic::EQUALS(base, -1.0), logic::AND(arg >= -constants::reciprocal_e(), logic::IsNegative(arg)))
                        })
                    )
                );

                break;
            }

            case NT::RiemannZeta: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::NOT_EQUALS(a, 1.0));
                break;
            }

            case NT::CosIntegral:
            case NT::CoshIntegral: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsPositive(a));
                break;
            }

            case NT::Dilogarithm:
            case NT::Trilogarithm: {
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(a <= 1.0);
                break;
            }

            case NT::Polylog: {
                Expression arg  = wrap(n->binary.left , mgr);
                Expression base = wrap(n->binary.right, mgr);
                constexpr double convergence_min = -230.76458831914587924007515393100900168785407806547543955549888911;

                out.push_back(
                    logic::OR({
                        logic::AND(arg > -1.0, arg < 1.0),
                        logic::AND(logic::EQUALS(arg, -1.0), logic::NOT_EQUALS(base, 1.0)),
                        logic::AND(logic::EQUALS(arg, 1.0), base > 1.0),
                        logic::AND({arg > 1.0, logic::IsInteger(base), logic::IsNonPositive(base)}),
                        logic::AND(arg < -1.0, logic::OR(logic::IsNonNegativeOrInteger(base), arg > convergence_min))
                    }
                ));

                break;
            }

            case NT::SpenceFunction: {                       
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(a >= -1.0);
                break;
            }

            case NT::SpenceIntegral: {                       
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::IsNonNegative(a));
                break;
            }

            case NT::RogersL:
            case NT::RogersLR: {                       
                Expression a = wrap(n->unary.child, mgr);
                out.push_back(logic::AND(logic::IsPositive(a), a < 1.0));   
                break;
            }

            default:
                break; 
        }
    }

    static void walk(
        symbols::MathExpressionNode* n,
        symbols::MathExpressionManager& mgr,
        std::vector<logic::Predicate>& out
    ) {
        if (!n) return;
        emit_local(n, mgr, out);
        using NT = symbols::NodeType;

        switch (n->type) {
            case NT::Constant:
            case NT::Variable:
            case NT::PositiveInfinity:
            case NT::NegativeInfinity:
            case NT::NaN:
            case NT::Undefined:
            case NT::Indeterminate:
            case NT::Invalid:
                return;

            case NT::Power:                                 
                walk(n->power.base, mgr, out);
                walk(n->power.exponent, mgr, out);
                return;

            case NT::AppliedFunction:
                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) walk(n->applied.args[i], mgr, out);
                return;

            default:
                if (symbols::NodeKeyHash::is_unary(n->type)) {
                    walk(n->unary.child, mgr, out);
                } else if (symbols::NodeKeyHash::is_binary(n->type)) {
                    walk(n->binary.left, mgr, out);
                    walk(n->binary.right, mgr, out);
                }
                return;
        }
    }
};

logic::Predicate analyze_domain(const Expression& e) {
    return DomainAnalysis::analyze(e);
}

logic::Predicate analyze(symbols::MathExpression e, symbols::MathExpressionManager& mgr) {
    return DomainAnalysis::analyze(e, mgr);
}

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_SOURCE_SYMBOLIC_MATH_CAS_DOMAIN_ANALYSIS_HPP