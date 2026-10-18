#ifndef FIZMO_MATH_SIMPLIFIER_CLASS_HPP
#define FIZMO_MATH_SIMPLIFIER_CLASS_HPP

#include "expression.hpp"
#include "evaluator.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

class MathExpressionSimplifier {
private:
    MathExpressionManager& mgr_;

public:
    explicit MathExpressionSimplifier(MathExpressionManager& mgr) : mgr_(mgr) {}

    MathExpression simplify(MathExpression e) {
        if (!e) return e;
        return simplify_node(e.get());
    }

private:
    MathExpression konst(double v)   { return mgr_.constant(v);          }
    MathExpression konst(const Rational& r) { return mgr_.constant(r);   }
    MathExpression pi_node()         { return mgr_.pi();                 }
    MathExpression e_node()          { return mgr_.euler_number();       }
    MathExpression pos_inf()         { return mgr_.pos_inf();            }
    MathExpression neg_inf()         { return mgr_.neg_inf();            }
    MathExpression nan_node()        { return mgr_.nan_expr();           }
    MathExpression undef()           { return mgr_.undefined_expr();     }
    MathExpression indet()           { return mgr_.indeterminate_expr(); }

public:
    bool is_const(const MathExpressionNode* n)             const { return n->type == NodeType::Constant;           }
    bool is_const(const MathExpressionNode* n, double val) const;
    bool is_pi(const MathExpressionNode* n)                const { return n->type == NodeType::Pi;                 }
    bool is_e(const MathExpressionNode* n)                 const { return n->type == NodeType::EulerNumber;        }
    bool is_named_const(const MathExpressionNode* n)       const { return is_pi(n) || is_e(n);                     }
    Rational qv(const MathExpressionNode* n)               const { return n->exact ? *n->exact : Rational::from_double(n->constant); }
    int known_sign(const MathExpressionNode* n)            const;
    bool pi_multiple(const MathExpressionNode* n, Rational& r) const;
    bool is_inv_e(const MathExpressionNode* n)             const;
    bool is_neg_inv_e(const MathExpressionNode* n)         const;
    bool is_pos_inf(const MathExpressionNode* n)           const { return n->type == NodeType::PositiveInfinity;   }
    bool is_neg_inf(const MathExpressionNode* n)           const { return n->type == NodeType::NegativeInfinity;   }
    bool is_infinite(const MathExpressionNode* n)          const { return is_pos_inf(n) || is_neg_inf(n);          }
    bool is_nan(const MathExpressionNode* n)               const { return n->type == NodeType::NaN;                }
    bool is_undef(const MathExpressionNode* n)             const { return n->type == NodeType::Undefined;          }
    bool is_indet(const MathExpressionNode* n)             const { return n->type == NodeType::Indeterminate;      }
    bool is_poison(const MathExpressionNode* n)            const { return is_nan(n) || is_undef(n) || is_indet(n); }

public:
    MathExpressionNode* dominant_poison(MathExpressionNode* a, MathExpressionNode* b) const;

    MathExpression propagate_poison2(MathExpression& l, MathExpression& r) {
        auto* p = dominant_poison(l.get(), r.get());
        return p ? MathExpression(p) : MathExpression{};
    }

    int inf_sign(const MathExpressionNode* n) const {
        if (is_pos_inf(n)) return +1;
        if (is_neg_inf(n)) return -1;
        return 0;
    }

    MathExpression negate_inf(const MathExpressionNode* n) { return is_pos_inf(n) ? neg_inf() : pos_inf(); }

    bool is_square(MathExpressionNode* n, NodeType fn, MathExpressionNode*& inner) const;

    MathExpression square(NodeType fn, MathExpressionNode* inner) {
        MathExpression arg(inner);
        return mgr_.power(mgr_.unary(fn, arg), konst(2.0));
    }

    MathExpression simplify_node(MathExpressionNode* n);

    MathExpression pi_times(const Rational& r);

    MathExpression scaled_sqrt(const Rational& a, std::int64_t s);

    MathExpression make_exp(MathExpression child);

    MathExpression trig_at_pi_multiple(NodeType t, const Rational& r);

    MathExpression fold_unary_exact(NodeType t, MathExpression child);

    MathExpression fold_binary_exact(NodeType t, MathExpression l, MathExpression r);

    MathExpression fold_power_exact(const Rational& base, const Rational& exp);

    MathExpression gamma_exact(const Rational& x);

    MathExpression nan_check(MathExpression candidate, bool undefined_on_nan);

    MathExpression rebuild_unary(NodeType t, MathExpressionNode* n);

    MathExpression rebuild_binary(NodeType t, MathExpressionNode* n);

    MathExpression simplify_generic(MathExpressionNode* n);

    MathExpression simplify_negate(MathExpressionNode* n);

    MathExpression simplify_add(MathExpressionNode* n);

    MathExpression simplify_subtract(MathExpressionNode* n);

    MathExpression simplify_multiply(MathExpressionNode* n);

    MathExpression simplify_divide(MathExpressionNode* n);

    MathExpression simplify_sign(MathExpressionNode* n);

    MathExpression simplify_unit_step(MathExpressionNode* n);

    MathExpression simplify_power(MathExpressionNode* n);

    MathExpression simplify_exp(MathExpressionNode* n);

    MathExpression simplify_ln(MathExpressionNode* n);

    MathExpression simplify_log(MathExpressionNode* n);

    MathExpression simplify_sqrt(MathExpressionNode* n);

    MathExpression simplify_cbrt(MathExpressionNode* n);

    MathExpression simplify_sinc(MathExpressionNode* n);

    MathExpression simplify_normalsinc(MathExpressionNode* n);

    MathExpression simplify_erf_erfc(MathExpressionNode* n);

    MathExpression simplify_erfi(MathExpressionNode* n);

    MathExpression simplify_inverse_erf_family(MathExpressionNode* n);

    MathExpression simplify_erf_generalized(MathExpressionNode* n);

    MathExpression simplify_gamma_factorial(MathExpressionNode* n);

    MathExpression simplify_digamma_trigamma(MathExpressionNode* n);

    MathExpression simplify_beta(MathExpressionNode* n);

    MathExpression simplify_lambert_w(MathExpressionNode* n);

    MathExpression simplify_chebyshev(MathExpressionNode* n);

    MathExpression simplify_exp_integral(MathExpressionNode* n);

    MathExpression simplify_log_integral(MathExpressionNode* n);

    MathExpression simplify_exp_integral_gen(MathExpressionNode* n);

    MathExpression simplify_log_integral_gen(MathExpressionNode* n);

    MathExpression simplify_bell(MathExpressionNode* n);

    MathExpression simplify_sin_cos_integral(MathExpressionNode* n);

    MathExpression simplify_fresnel(MathExpressionNode* n);

    MathExpression simplify_riemann(MathExpressionNode* n);

    MathExpression simplify_di_tri_log(MathExpressionNode* n);

    MathExpression simplify_polylog(MathExpressionNode* n);

    MathExpression simplify_spence(MathExpressionNode* n);

    MathExpression simplify_rogers(MathExpressionNode* n);

    MathExpression simplify_gudermannian(MathExpressionNode* n);

    MathExpression simplify_combination(MathExpressionNode* n);

    MathExpression simplify_permutation(MathExpressionNode* n);

    MathExpression simplify_fib_lucas_sequence(MathExpressionNode* n);

    MathExpression simplify_fib_lucas_poly(MathExpressionNode* n);

    MathExpression simplify_applied(MathExpressionNode* n);
};

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_SIMPLIFIER_CLASS_HPP