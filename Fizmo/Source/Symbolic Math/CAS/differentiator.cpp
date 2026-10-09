#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

auto DerivKeyHash::operator()(const DerivKey& k) const noexcept -> std::size_t {
        std::size_t h = std::hash<MathExpressionNode*>{}(k.node);
        auto mix = [&](std::size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
        mix(std::hash<std::uint64_t>{}(k.var_id));
        mix(std::hash<unsigned>{}(k.order));
        return h;
    }

auto MathExpressionDifferentiator::derivative(MathExpression expr, const std::string& var_name) -> MathExpression {
        if (is_invalid(expr)) return invalid();
        std::uint64_t vid = vars_.get(var_name);
        if (vid == VariableTable::invalid_id) return konst(0.0);
        return simp_.simplify(diff(expr.get(), vid));
    }

auto MathExpressionDifferentiator::nth_derivative_recursive(MathExpression expr, const std::string& var_name, unsigned int order) -> MathExpression {
        if (is_invalid(expr)) return invalid();
        if (order == 0) return expr;
        std::uint64_t vid = vars_.get(var_name);
        if (vid == VariableTable::invalid_id) return konst(0.0);
        MathExpressionNode* n = expr.get();
        return simp_.simplify(diff_n_recurs(n, vid, order));
    }

auto MathExpressionDifferentiator::nth_derivative_iterative(MathExpression expr, const std::string& var_name, unsigned int order) -> MathExpression {
        if (is_invalid(expr)) return invalid();
        if (order == 0) return expr;
        std::uint64_t vid = vars_.get(var_name);
        if (vid == VariableTable::invalid_id) return konst(0.0);
        return simp_.simplify(nth_derivative_iter(expr, var_name, order));
    }

auto MathExpressionDifferentiator::contains_var(const MathExpressionNode* n, std::uint64_t vid) const -> bool {
        if (is_invalid_node(n)) return false;
        switch (n->type) {
            case NodeType::Variable:
                return n->variable.var_id == vid;
            case NodeType::Constant:
            case NodeType::PositiveInfinity:
            case NodeType::NegativeInfinity:
            case NodeType::NaN:
            case NodeType::Undefined:
            case NodeType::Indeterminate:
            case NodeType::Invalid:
                return false;
            case NodeType::Power:
                return contains_var(n->power.base, vid) || contains_var(n->power.exponent, vid);
            case NodeType::AppliedFunction:
                for (std::uint64_t i = 0; i < n->applied.arg_count; ++i) if (contains_var(n->applied.args[i], vid)) return true;
                return false;
            default:
                if (NodeKeyHash::is_unary(n->type)) return contains_var(n->unary.child, vid);
                if (NodeKeyHash::is_binary(n->type)) return contains_var(n->binary.left, vid) || contains_var(n->binary.right, vid);
                return false;
        }
    }

auto MathExpressionDifferentiator::diff(MathExpressionNode* n, std::uint64_t vid) -> MathExpression {
        if (is_invalid_node(n)) { return invalid(); }
        switch (n->type) {
            case NodeType::Constant:
            case NodeType::PositiveInfinity:
            case NodeType::NegativeInfinity:
            case NodeType::NaN:
            case NodeType::Undefined:
            case NodeType::Indeterminate:
            case NodeType::Floor:
            case NodeType::Ceil:
            case NodeType::Round:
            case NodeType::Truncate:
            case NodeType::IntegerPart:
            case NodeType::Sign:        
            case NodeType::UnitStep: 
                return konst(0.0);

            case NodeType::AppliedFunction: {
                const std::uint64_t cnt = n->applied.arg_count;
                if (cnt == 0) return konst(0.0);
                std::vector<MathExpression> args;        args.reserve(cnt);
                std::vector<std::uint64_t>  base_orders; base_orders.reserve(cnt);

                for (std::uint64_t j = 0; j < cnt; ++j) {
                    args.push_back(wrap(n->applied.args[j]));
                    base_orders.push_back(n->applied.orders[j]);
                }

                MathExpression sum = konst(0.0);
                bool started = false;

                for (std::uint64_t i = 0; i < cnt; ++i) {
                    std::vector<std::uint64_t> bumped = base_orders;
                    bumped[i] += 1;
                    MathExpression partial = mgr_.applied(n->applied.func_id, args, bumped);
                    MathExpression term    = mul(partial, diff(n->applied.args[i], vid));
                    sum = started ? add(sum, term) : term;
                    started = true;
                }

                return sum;
            }
            
            case NodeType::Invalid: return invalid();

            case NodeType::Variable: return (n->variable.var_id == vid) ? konst(1.0) : konst(0.0);
            case NodeType::Negate: return neg(diff(n->unary.child, vid));
            case NodeType::Add: return add(diff(n->binary.left, vid), diff(n->binary.right, vid));
            case NodeType::Subtract: return sub(diff(n->binary.left, vid), diff(n->binary.right, vid));
            case NodeType::FractionalPart: return diff(n->unary.child, vid);

            case NodeType::Modulo: {
                MathExpression f(n->binary.left), g(n->binary.right);
                MathExpression fp = diff(n->binary.left,  vid);
                MathExpression gp = diff(n->binary.right, vid);
                MathExpression floor_fg = unary(NodeType::Floor, div(f, g));
                return sub(fp, mul(gp, floor_fg));
            }

            case NodeType::AbsoluteValue: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                return mul(fp, div(f, wrap(n)));  
            }

            case NodeType::Multiply: {
                MathExpression f(n->binary.left), g(n->binary.right);
                return add(mul(diff(n->binary.left, vid),  g), mul(f, diff(n->binary.right, vid)));
            }

            case NodeType::Divide: {
                MathExpression f(n->binary.left), g(n->binary.right);
                MathExpression num = sub(mul(diff(n->binary.left,  vid), g), mul(f, diff(n->binary.right, vid)));
                MathExpression den = pow(g, konst(2.0));
                return div(num, den);
            }

            case NodeType::Power: return diff_power(n, vid);
            case NodeType::NaturalExp: return chain(wrap(n), n->unary.child, vid);

            case NodeType::NaturalLog: {
                MathExpression f(n->unary.child);
                return div(diff(n->unary.child, vid), f);
            }

            case NodeType::Log: {
                MathExpression f(n->binary.left), b(n->binary.right);
                MathExpression ln_f = unary(NodeType::NaturalLog, f);
                MathExpression ln_b = unary(NodeType::NaturalLog, b);
                return diff_quotient(ln_f.get(), ln_b.get(), vid);
            }

            case NodeType::Sqrt: {
                MathExpression f(n->unary.child);
                return div(diff(n->unary.child, vid), mul(konst(2.0), wrap(n)));
            }

            case NodeType::Cbrt: {
                MathExpression f(n->unary.child);
                MathExpression denom = mul(konst(3.0), pow(f, konst(2.0 / 3.0)));
                return div(diff(n->unary.child, vid), denom);
            }

            case NodeType::Sin: return chain(unary(NodeType::Cos, wrap(n->unary.child)), n->unary.child, vid);

            case NodeType::Sinc: {
                MathExpression f(n->unary.child);
                MathExpression fp       = diff(n->unary.child, vid);
                MathExpression sin_f    = unary(NodeType::Sin, f);
                MathExpression cos_f    = unary(NodeType::Cos, f);
                MathExpression num      = sub(mul(f, cos_f), sin_f);
                MathExpression den      = pow(f, konst(2.0));
                return mul(div(num, den), fp);
            }

            case NodeType::NormalSinc: {
                MathExpression f(n->unary.child);
                MathExpression fp     = diff(n->unary.child, vid);
                MathExpression pi_k   = konst(constants::pi());
                MathExpression pi_f   = mul(pi_k, f);
                MathExpression sin_pf = unary(NodeType::Sin, pi_f);
                MathExpression cos_pf = unary(NodeType::Cos, pi_f);
                MathExpression num    = sub(mul(pi_f, cos_pf), sin_pf);
                MathExpression den    = mul(pi_k, pow(f, konst(2.0)));
                return mul(div(num, den), fp);
            }

            case NodeType::Cos: return chain(neg(unary(NodeType::Sin, wrap(n->unary.child))), n->unary.child, vid);

            case NodeType::Tan: {
                MathExpression sec_f = unary(NodeType::Sec, wrap(n->unary.child));
                return chain(pow(sec_f, konst(2.0)), n->unary.child, vid);
            }

            case NodeType::Csc: {
                MathExpression f(n->unary.child);
                MathExpression op = neg(mul(unary(NodeType::Csc, f), unary(NodeType::Cot, f)));
                return chain(op, n->unary.child, vid);
            }

            case NodeType::Sec: {
                MathExpression f(n->unary.child);
                MathExpression op = mul(unary(NodeType::Sec, f), unary(NodeType::Tan, f));
                return chain(op, n->unary.child, vid);
            }

            case NodeType::Cot: {
                MathExpression csc_f = unary(NodeType::Csc, wrap(n->unary.child));
                return chain(neg(pow(csc_f, konst(2.0))), n->unary.child, vid);
            }

            case NodeType::Arcsin: {
                MathExpression f(n->unary.child);
                MathExpression denom = unary(NodeType::Sqrt, sub(konst(1.0), pow(f, konst(2.0))));
                return div(diff(n->unary.child, vid), denom);
            }

            case NodeType::Arccos: {
                MathExpression f(n->unary.child);
                MathExpression denom = unary(NodeType::Sqrt, sub(konst(1.0), pow(f, konst(2.0))));
                return neg(div(diff(n->unary.child, vid), denom));
            }

            case NodeType::Arctan: {
                MathExpression f(n->unary.child);
                return div(diff(n->unary.child, vid), add(konst(1.0), pow(f, konst(2.0))));
            }

            case NodeType::Arccsc: {
                MathExpression f(n->unary.child);
                MathExpression denom = mul(unary(NodeType::AbsoluteValue, f), unary(NodeType::Sqrt, sub(pow(f, konst(2.0)), konst(1.0))));
                return neg(div(diff(n->unary.child, vid), denom));
            }

            case NodeType::Arcsec: {
                MathExpression f(n->unary.child);
                MathExpression denom = mul(unary(NodeType::AbsoluteValue, f), unary(NodeType::Sqrt, sub(pow(f, konst(2.0)), konst(1.0))));
                return div(diff(n->unary.child, vid), denom);
            }

            case NodeType::Arccot: {
                MathExpression f(n->unary.child);
                return neg(div(diff(n->unary.child, vid), add(konst(1.0), pow(f, konst(2.0)))));
            }

            case NodeType::Sinh: return chain(unary(NodeType::Cosh, wrap(n->unary.child)), n->unary.child, vid);
            case NodeType::Cosh: return chain(unary(NodeType::Sinh, wrap(n->unary.child)), n->unary.child, vid);

            case NodeType::Tanh: {
                MathExpression sech_f = unary(NodeType::Sech, wrap(n->unary.child));
                return chain(pow(sech_f, konst(2.0)), n->unary.child, vid);
            }

            case NodeType::Csch: {
                MathExpression f(n->unary.child);
                MathExpression op = neg(mul(unary(NodeType::Csch, f), unary(NodeType::Coth, f)));
                return chain(op, n->unary.child, vid);
            }

            case NodeType::Sech: {
                MathExpression f(n->unary.child);
                MathExpression op = neg(mul(unary(NodeType::Sech, f), unary(NodeType::Tanh, f)));
                return chain(op, n->unary.child, vid);
            }

            case NodeType::Coth: {
                MathExpression csch_f = unary(NodeType::Csch, wrap(n->unary.child));
                return chain(neg(pow(csch_f, konst(2.0))), n->unary.child, vid);
            }

            case NodeType::Arcsinh: {
                MathExpression f(n->unary.child);
                MathExpression denom = unary(NodeType::Sqrt, add(pow(f, konst(2.0)), konst(1.0)));
                return div(diff(n->unary.child, vid), denom);
            }

            case NodeType::Arccosh: {
                MathExpression f(n->unary.child);
                MathExpression denom = unary(NodeType::Sqrt, sub(pow(f, konst(2.0)), konst(1.0)));
                return div(diff(n->unary.child, vid), denom);
            }

            case NodeType::Arctanh: {
                MathExpression f(n->unary.child);
                return div(diff(n->unary.child, vid), sub(konst(1.0), pow(f, konst(2.0))));
            }

            case NodeType::Arccsch: {
                MathExpression f(n->unary.child);
                MathExpression denom = mul(unary(NodeType::AbsoluteValue, f), unary(NodeType::Sqrt, add(pow(f, konst(2.0)), konst(1.0))));
                return neg(div(diff(n->unary.child, vid), denom));
            }

            case NodeType::Arcsech: {
                MathExpression f(n->unary.child);
                MathExpression denom = mul(f, unary(NodeType::Sqrt, sub(konst(1.0), pow(f, konst(2.0)))));
                return neg(div(diff(n->unary.child, vid), denom));
            }

            case NodeType::Arccoth: {
                MathExpression f(n->unary.child);
                return div(diff(n->unary.child, vid), sub(konst(1.0), pow(f, konst(2.0))));
            }

            case NodeType::Erf: {
                MathExpression f(n->unary.child);
                MathExpression kernel = mul(konst(2.0 * constants::reciprocal_sqrt_pi()), unary(NodeType::NaturalExp, neg(pow(f, konst(2.0)))));
                return mul(kernel, diff(n->unary.child, vid));
            }

            case NodeType::Erfc: {
                MathExpression f(n->unary.child);
                MathExpression kernel = mul(konst(-2.0 * constants::reciprocal_sqrt_pi()), unary(NodeType::NaturalExp, neg(pow(f, konst(2.0)))));
                return mul(kernel, diff(n->unary.child, vid));
            }

            case NodeType::Erfi: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                double c = 2.0 * constants::reciprocal_sqrt_pi();
                MathExpression kernel = mul(konst(c), unary(NodeType::NaturalExp, pow(f, konst(2.0))));
                return mul(kernel, fp);
            }

            case NodeType::InverseErf: {
                MathExpression x(n->unary.child);
                MathExpression xp = diff(n->unary.child, vid);
                double c = 0.5 * constants::sqrt_pi(); 
                MathExpression inv = wrap(n);
                MathExpression kernel = mul(konst(c), unary(NodeType::NaturalExp, pow(inv, konst(2.0))));
                return mul(kernel, xp);
            }

            case NodeType::InverseErfc: {
                MathExpression x(n->unary.child);
                MathExpression xp = diff(n->unary.child, vid);
                double c = 0.5 * constants::sqrt_pi(); 
                MathExpression inv = wrap(n);
                MathExpression kernel = mul(konst(-c), unary(NodeType::NaturalExp, pow(inv, konst(2.0))));
                return mul(kernel, xp);
            }

            case NodeType::InverseErfi: {
                MathExpression x(n->unary.child);
                MathExpression xp = diff(n->unary.child, vid);
                double c = 0.5 * constants::sqrt_pi(); 
                MathExpression inv = wrap(n);
                MathExpression kernel = mul(konst(c), unary(NodeType::NaturalExp, neg(pow(inv, konst(2.0)))));
                return mul(kernel, xp);
            }

            case NodeType::ErfGeneralized: {
                double c = 2.0 * constants::reciprocal_sqrt_pi();
                MathExpression f(n->binary.left), g(n->binary.right);
                MathExpression df = mul(mul(konst(c), unary(NodeType::NaturalExp, neg(pow(f, konst(2.0))))), diff(n->binary.left,  vid));
                MathExpression dg = mul(mul(konst(c), unary(NodeType::NaturalExp, neg(pow(g, konst(2.0))))), diff(n->binary.right, vid));
                return sub(df, dg);
            }

            case NodeType::ErfcGeneralized: {
                double c = -2.0 * constants::reciprocal_sqrt_pi();
                MathExpression f(n->binary.left), g(n->binary.right);
                MathExpression df = mul(mul(konst(c), unary(NodeType::NaturalExp, neg(pow(f, konst(2.0))))), diff(n->binary.left,  vid));
                MathExpression dg = mul(mul(konst(c), unary(NodeType::NaturalExp, neg(pow(g, konst(2.0))))), diff(n->binary.right, vid));
                return sub(df, dg);
            }

            case NodeType::Gamma: {
                MathExpression f(n->unary.child);
                MathExpression outer = mul(unary(NodeType::Gamma, f), unary(NodeType::Digamma, f));
                return chain(outer, n->unary.child, vid);
            }

            case NodeType::Factorial: {
                MathExpression f(n->unary.child);
                MathExpression fp1 = add(f, konst(1.0));
                MathExpression outer = mul(unary(NodeType::Factorial, f), unary(NodeType::Digamma, fp1));
                return chain(outer, n->unary.child, vid);
            }

            case NodeType::Digamma: return chain(unary(NodeType::Trigamma, wrap(n->unary.child)), n->unary.child, vid);
            
            case NodeType::Trigamma: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                return mul(fp, binary(NodeType::Polygamma, f, konst(2)));
            }

            case NodeType::Polygamma: {
                MathExpression f(n->binary.left);
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression n_(n->binary.right);
                bool n_free = !contains_var(n->binary.right, vid) && n->type == NodeType::Constant;
                if (!n_free) { return invalid(); }
                double np1 = n_.get()->constant + 1.0;
                return mul(fp, binary(NodeType::Polygamma, f, konst(np1)));
            }

            case NodeType::BetaFunction: {
                MathExpression a(n->binary.left), b(n->binary.right);
                MathExpression ap     = diff(n->binary.left,  vid);
                MathExpression bp     = diff(n->binary.right, vid);
                MathExpression ab     = add(a, b);
                MathExpression psi_ab = unary(NodeType::Digamma, ab);
                MathExpression beta   = wrap(n);
                MathExpression term1  = mul(sub(unary(NodeType::Digamma, a), psi_ab), ap);
                MathExpression term2  = mul(sub(unary(NodeType::Digamma, b), psi_ab), bp);
                return mul(beta, add(term1, term2));
            }

            case NodeType::LambertW: {
                MathExpression f(n->binary.left);
                MathExpression k(n->binary.right);       
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression Wkf = wrap(n);   
                MathExpression one_plus_W = add(konst(1.0), Wkf);
                MathExpression denom      = mul(f, one_plus_W);
                MathExpression outer      = div(Wkf, denom);
                return mul(outer, fp);
            }

            case NodeType::ChebyshevU: {
                MathExpression f(n->binary.left);
                MathExpression g(n->binary.right);
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression gp = diff(n->binary.right, vid);
                MathExpression gp1 = add(g, konst(1.0));
                MathExpression sqrtf = unary(NodeType::Sqrt, sub(konst(1.0), binary(NodeType::Power, f, konst(2.0))));
                MathExpression sqrtf3 = binary(NodeType::Power, sqrtf, konst(3.0));
                MathExpression arccosf = unary(NodeType::Arccos, f);
                MathExpression gp1acf = mul(gp1, arccosf);
                MathExpression m1 = div(unary(NodeType::Cos, gp1acf), sqrtf);

                MathExpression m2 = sub(
                    mul(gp, arccosf),
                    mul(gp1, div(fp, sqrtf))
                );

                MathExpression adder = div(
                    mul(
                        unary(NodeType::Sin, gp1acf),
                        mul(f, fp)
                    ),
                    sqrtf3
                );

                return add(mul(m1, m2), adder);
            }

            case NodeType::ChebyshevT: {
                MathExpression f(n->binary.left);
                MathExpression g(n->binary.right);
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression gp = diff(n->binary.right, vid);
                MathExpression sqrtf = unary(NodeType::Sqrt, sub(konst(1.0), binary(NodeType::Power, f, konst(2.0))));
                MathExpression arccosf = unary(NodeType::Arccos, f);
                MathExpression m1 = neg(unary(NodeType::Sin, mul(f, arccosf)));

                MathExpression m2 = sub(
                    mul(gp, arccosf),
                    div(mul(g, fp), sqrtf)
                );

                return mul(m1, m2);
            }

            case NodeType::ExponentialIntegral: {
                MathExpression f(n->unary.child);
                MathExpression kernel = div(unary(NodeType::NaturalExp, f), f);
                return chain(kernel, n->unary.child, vid);
            }

            case NodeType::LogarithmicIntegral: {
                MathExpression f(n->unary.child);
                MathExpression denom = unary(NodeType::NaturalLog, f);
                return div(diff(n->unary.child, vid), denom);
            }
                
            case NodeType::SinIntegral: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                return mul(fp, unary(NodeType::Sinc, f));
            }

            case NodeType::CosIntegral: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                
                return mul(
                    fp, 
                    div(unary(NodeType::Cos, f), f)
                );
            }

            case NodeType::SinhIntegral: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                
                return mul(
                    fp, 
                    div(unary(NodeType::Sinh, f), f)
                );
            }

            case NodeType::CoshIntegral: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                
                return mul(
                    fp, 
                    div(unary(NodeType::Cosh, f), f)
                );
            }

            case NodeType::FresnelS: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);

                return mul(
                    fp,
                    unary(
                        NodeType::Sin,
                        mul(
                            konst(constants::pi_2()), 
                            binary(NodeType::Power, f, konst(2.0))
                        )
                    )
                );
            }

            case NodeType::FresnelC: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);

                return mul(
                    fp,
                    unary(
                        NodeType::Cos,
                        mul(
                            konst(constants::pi_2()), 
                            binary(NodeType::Power, f, konst(2.0))
                        )
                    )
                );
            }

            case NodeType::Dilogarithm: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression kernel = neg(div(unary(NodeType::NaturalLog, sub(konst(1.0), f)), f));
                return mul(kernel, fp);
            }

            case NodeType::Trilogarithm: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression kernel = div(unary(NodeType::Dilogarithm, f), f);
                return mul(kernel, fp);
            }

            case NodeType::SpenceFunction: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression kernel = div(unary(NodeType::NaturalLog, add(konst(1.0), f)), f);
                return mul(kernel, fp);
            }

            case NodeType::SpenceIntegral: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression kernel = div(unary(NodeType::NaturalLog, f), sub(konst(1.0), f));
                return mul(kernel, fp);
            }

            case NodeType::RogersLR: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression one_minus_f = sub(konst(1.0), f);
                MathExpression ln_1mf = unary(NodeType::NaturalLog, one_minus_f);
                MathExpression ln_f   = unary(NodeType::NaturalLog, f);
                MathExpression t1 = div(ln_1mf, mul(konst(2.0), f));
                MathExpression t2 = div(ln_f,   mul(konst(2.0), one_minus_f));
                MathExpression kernel = neg(add(t1, t2));
                return mul(kernel, fp);
            }

            case NodeType::RogersL: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                MathExpression one_minus_f = sub(konst(1.0), f);
                MathExpression ln_1mf = unary(NodeType::NaturalLog, one_minus_f);
                MathExpression ln_f   = unary(NodeType::NaturalLog, f);
                MathExpression t1 = div(ln_1mf, mul(konst(2.0), f));
                MathExpression t2 = div(ln_f,   mul(konst(2.0), one_minus_f));
                MathExpression lr_prime = neg(add(t1, t2));
                MathExpression kernel = mul(konst(6.0 * constants::reciprocal_pi_squared()), lr_prime);
                return mul(kernel, fp);
            }

            case NodeType::Gudermannian: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                return mul(fp, unary(NodeType::Sech, f));
            }

            case NodeType::InverseGudermannian: {
                MathExpression f(n->unary.child);
                MathExpression fp = diff(n->unary.child, vid);
                return mul(fp, unary(NodeType::Sec, f));
            }

            case NodeType::Permutation: {
                MathExpression f(n->binary.left);
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression g(n->binary.right);
                MathExpression gp = diff(n->binary.right, vid);
                MathExpression mult = binary(NodeType::Permutation, f, g);
                MathExpression t1 = mul(fp, unary(NodeType::Digamma, add(f, konst(1.0))));

                MathExpression t2 = mul(
                    unary(NodeType::Digamma, add(sub(f, g), konst(1.0))),
                    sub(fp, gp)
                );

                return mul(mult, sub(t1, t2));
            }

            case NodeType::Combination: {
                MathExpression f(n->binary.left);
                MathExpression fp = diff(n->binary.left, vid);
                MathExpression g(n->binary.right);
                MathExpression gp = diff(n->binary.right, vid);
                MathExpression mult = binary(NodeType::Combination, f, g);
                MathExpression t1 = mul(fp, unary(NodeType::Digamma, add(f, konst(1.0))));
                MathExpression t2 = mul(gp, unary(NodeType::Digamma, add(g, konst(1.0))));

                MathExpression t3 = mul(
                    unary(NodeType::Digamma, add(sub(f, g), konst(1.0))),
                    sub(fp, gp)
                );

                return mul(mult, sub(sub(t1, t2), t3));
            }

            case NodeType::FibonacciSequence: {
                MathExpression f(n->unary.child);
                const double phi_v    = constants::phi();
                const double psi_v    = -constants::psi();
                const double ln_phi   = std::log(phi_v);
                const double ln_psi   = std::log(psi_v);
                const double inv_sq5  = 1.0 / std::sqrt(5.0);
                MathExpression pi_u   = mul(konst(constants::pi()), f);
                MathExpression cos_pu = unary(NodeType::Cos, pi_u);
                MathExpression sin_pu = unary(NodeType::Sin, pi_u);
                MathExpression phi_u  = pow(konst(phi_v), f);
                MathExpression psi_u  = pow(konst(psi_v), f);
                MathExpression t1     = mul(phi_u, konst(ln_phi));
                MathExpression inner  = sub(mul(konst(constants::pi()), sin_pu), mul(konst(ln_psi), cos_pu));
                MathExpression outer  = mul(konst(inv_sq5), add(t1, mul(psi_u, inner)));
                return chain(outer, n->unary.child, vid);
            }

            case NodeType::LucasSequence: {
                MathExpression f(n->unary.child);
                const double phi_v    = constants::phi();
                const double psi_v    = -constants::psi();
                const double ln_phi   = std::log(phi_v);
                const double ln_psi   = std::log(psi_v);
                MathExpression pi_u   = mul(konst(constants::pi()), f);
                MathExpression cos_pu = unary(NodeType::Cos, pi_u);
                MathExpression sin_pu = unary(NodeType::Sin, pi_u);
                MathExpression phi_u  = pow(konst(phi_v), f);
                MathExpression psi_u  = pow(konst(psi_v), f);
                MathExpression t1     = mul(phi_u, konst(ln_phi));
                MathExpression inner  = sub(mul(konst(ln_psi), cos_pu), mul(konst(constants::pi()), sin_pu));
                MathExpression outer  = add(t1, mul(psi_u, inner));
                return chain(outer, n->unary.child, vid);
            }

            case NodeType::ExponentialIntegralGeneralized:
            case NodeType::LogarithmicIntegralGeneralized:
            case NodeType::BellPolynomial:
            case NodeType::RiemannZeta:
            case NodeType::Polylog:
            case NodeType::FibonacciPolynomial:
            case NodeType::LucasPolynomial:
                return invalid();

            default: return invalid();
        }
    }

auto MathExpressionDifferentiator::diff_n_recurs(MathExpressionNode* n, std::uint64_t vid, unsigned int order) -> MathExpression {
        if (is_invalid_node(n)) return invalid();
        DerivKey key{n, vid, order};
        if (auto it = cache_.find(key); it != cache_.end()) return wrap(it->second);

        MathExpression result =
            (order == 0) ? wrap(n)
            : (order == 1) ? diff(n, vid)
            : diff_n_recurs(diff(n, vid).get(), vid, order - 1);

        if (is_invalid(result)) return invalid();
        MathExpression simplified = simp_.simplify(result);
        cache_[key] = simplified.get();
        return simplified;
    }

auto MathExpressionDifferentiator::nth_derivative_iter(MathExpression expr, const std::string& var_name, unsigned int order) -> MathExpression {
        if (order == 0) return expr;
        if (is_invalid_node(expr.get())) return invalid();
        std::uint64_t vid = vars_.get(var_name);
        if (vid == VariableTable::invalid_id) return konst(0.0);
        MathExpressionNode* root = expr.get();
        MathExpressionNode* cur  = root;

        for (unsigned k = 1; k <= order; ++k) {
            DerivKey key{root, vid, k};
            if (auto it = cache_.find(key); it != cache_.end()) {
                cur = it->second;
                continue;
            }
            MathExpression next = diff(cur, vid);
            if (is_invalid(next)) return invalid();
            MathExpression simplified = simp_.simplify(next);
            cur = simplified.get();
            cache_[key] = cur;
        }

        return wrap(cur);
    }

auto MathExpressionDifferentiator::diff_power(MathExpressionNode* n, std::uint64_t vid) -> MathExpression {
        MathExpression f(n->power.base);
        MathExpression g(n->power.exponent);
        MathExpression fp = diff(n->power.base, vid);
        MathExpression gp = diff(n->power.exponent, vid);
        const bool f_free = !contains_var(n->power.base, vid);     
        const bool g_free = !contains_var(n->power.exponent, vid);  

        if (g_free && n->power.exponent->type == NodeType::Constant) {
            double c = n->power.exponent->constant;
            return mul(mul(konst(c), pow(f, konst(c - 1.0))), fp);
        }

        if (f_free && n->power.base->type == NodeType::Constant) {
            double c = n->power.base->constant;
            MathExpression ln_c = konst(std::log(c));
            return mul(mul(wrap(n), ln_c), gp);
        }

        if (g_free) {
            MathExpression new_exp = sub(g, konst(1.0));
            return mul(mul(g, pow(f, new_exp)), fp);
        }

        if (f_free) {
            MathExpression ln_f = unary(NodeType::NaturalLog, f);
            return mul(mul(wrap(n), ln_f), gp);
        }

        MathExpression ln_f   = unary(NodeType::NaturalLog, f);
        MathExpression term1  = mul(gp, ln_f);
        MathExpression term2  = mul(g, div(fp, f));
        return mul(wrap(n), add(term1, term2));
    }

auto MathExpressionDifferentiator::diff_quotient(MathExpressionNode* num_n, MathExpressionNode* den_n, std::uint64_t vid) -> MathExpression {
        MathExpression f(num_n), g(den_n);
        MathExpression fp = diff(num_n, vid);
        MathExpression gp = diff(den_n, vid);
        MathExpression num = sub(mul(fp, g), mul(f, gp));
        MathExpression den = pow(g, konst(2.0));
        return div(num, den);
    }

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
