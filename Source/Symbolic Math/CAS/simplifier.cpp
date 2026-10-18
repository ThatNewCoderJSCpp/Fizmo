#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

namespace {

constexpr std::int64_t factorial_limit   = 1000;
constexpr std::int64_t sequence_limit    = 100000;
constexpr std::int64_t polynomial_limit  = 10000;
constexpr std::int64_t binomial_limit    = 10000;

BigInt factorial_of(std::int64_t n) {
    BigInt r = BigInt::one();
    for (std::int64_t i = 2; i <= n; ++i) r *= BigInt(i);
    return r;
}

BigInt falling_product(std::int64_t n, std::int64_t k) {
    BigInt r = BigInt::one();
    for (std::int64_t i = 0; i < k; ++i) r *= BigInt(n - i);
    return r;
}

void fib_lucas_pair(std::int64_t n, BigInt& f, BigInt& l) {
    BigInt a = BigInt::zero();
    BigInt b = BigInt::one();

    for (int bit = 62; bit >= 0; --bit) {
        BigInt c = a * ((b << 1) - a);
        BigInt d = a * a + b * b;

        if ((n >> bit) & 1) {
            a = std::move(d);
            b = c + a;
        } else {
            a = std::move(c);
            b = std::move(d);
        }
    }

    f = a;
    l = (b << 1) - a;
}

bool rational_int(const Rational& q, std::int64_t& out) { return q.to_int64(out); }

std::int64_t mod_positive(std::int64_t a, std::int64_t m) {
    std::int64_t r = a % m;
    return r < 0 ? r + m : r;
}

} // namespace

bool MathExpressionSimplifier::is_const(const MathExpressionNode* n, double val) const {
    if (n->type != NodeType::Constant || n->constant != val) return false;
    if (!n->exact) return true;
    const Rational& q = *n->exact;
    if (val == std::floor(val) && std::fabs(val) < 9.0e15) return q.is_integer() && q.numerator() == BigInt(static_cast<std::int64_t>(val));
    return q == Rational::from_double_exact(val);
}

int MathExpressionSimplifier::known_sign(const MathExpressionNode* n) const {
    if (is_const(n)) return qv(n).sign();
    if (is_named_const(n)) return 1;
    if (is_pos_inf(n)) return 1;
    if (is_neg_inf(n)) return -1;
    return 0;
}

bool MathExpressionSimplifier::pi_multiple(const MathExpressionNode* n, Rational& r) const {
    switch (n->type) {
        case NodeType::Pi:
            r = Rational(1);
            return true;

        case NodeType::Negate:
            if (!pi_multiple(n->unary.child, r)) return false;
            r = -r;
            return true;

        case NodeType::Multiply:
            if (is_const(n->binary.left) && pi_multiple(n->binary.right, r)) { r = qv(n->binary.left) * r; return true; }
            if (is_const(n->binary.right) && pi_multiple(n->binary.left, r)) { r = qv(n->binary.right) * r; return true; }
            return false;

        case NodeType::Divide:
            if (is_const(n->binary.right) && !qv(n->binary.right).is_zero() && pi_multiple(n->binary.left, r)) { r = r / qv(n->binary.right); return true; }
            return false;

        default:
            return false;
    }
}

bool MathExpressionSimplifier::is_inv_e(const MathExpressionNode* n) const {
    if (n->type == NodeType::Divide && is_const(n->binary.left, 1.0) && is_e(n->binary.right)) return true;
    if (n->type == NodeType::Power && is_e(n->power.base) && is_const(n->power.exponent, -1.0)) return true;
    if (n->type == NodeType::NaturalExp && is_const(n->unary.child, -1.0)) return true;
    return false;
}

bool MathExpressionSimplifier::is_neg_inv_e(const MathExpressionNode* n) const {
    if (n->type == NodeType::Negate) return is_inv_e(n->unary.child);
    if (n->type == NodeType::Divide && is_const(n->binary.left, -1.0) && is_e(n->binary.right)) return true;
    if (n->type == NodeType::Multiply) {
        if (is_const(n->binary.left, -1.0) && is_inv_e(n->binary.right)) return true;
        if (is_const(n->binary.right, -1.0) && is_inv_e(n->binary.left)) return true;
    }
    return false;
}

auto MathExpressionSimplifier::pi_times(const Rational& r) -> MathExpression {
    if (r.is_zero()) return konst(Rational());
    const bool neg = r.is_negative();
    const Rational a = r.abs();
    MathExpression body;

    if (a.is_one()) {
        body = pi_node();
    } else if (a.is_integer()) {
        body = mgr_.binary(NodeType::Multiply, konst(a), pi_node());
    } else if (a.numerator().is_one()) {
        body = mgr_.binary(NodeType::Divide, pi_node(), konst(Rational(a.denominator())));
    } else {
        body = mgr_.binary(NodeType::Divide, mgr_.binary(NodeType::Multiply, konst(Rational(a.numerator())), pi_node()), konst(Rational(a.denominator())));
    }

    return neg ? mgr_.unary(NodeType::Negate, body) : body;
}

auto MathExpressionSimplifier::scaled_sqrt(const Rational& a, std::int64_t s) -> MathExpression {
    if (s == 1 || a.is_zero()) return konst(a);
    const bool neg = a.is_negative();
    const Rational m = a.abs();
    MathExpression root = mgr_.unary(NodeType::Sqrt, konst(Rational(s)));
    MathExpression body;

    if (m.is_one()) {
        body = root;
    } else if (m.is_integer()) {
        body = mgr_.binary(NodeType::Multiply, konst(m), root);
    } else if (m.numerator().is_one()) {
        body = mgr_.binary(NodeType::Divide, root, konst(Rational(m.denominator())));
    } else {
        body = mgr_.binary(NodeType::Divide, mgr_.binary(NodeType::Multiply, konst(Rational(m.numerator())), root), konst(Rational(m.denominator())));
    }

    return neg ? mgr_.unary(NodeType::Negate, body) : body;
}

auto MathExpressionSimplifier::make_exp(MathExpression child) -> MathExpression {
    if (is_poison(child.get()))  return child;
    if (is_const(child.get(), 0.0)) return konst(1.0);
    if (is_const(child.get(), 1.0)) return e_node();
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_neg_inf(child.get())) return konst(0.0);
    if (child.get()->type == NodeType::NaturalLog) return MathExpression(child.get()->unary.child);
    return mgr_.unary(NodeType::NaturalExp, child);
}

auto MathExpressionSimplifier::trig_at_pi_multiple(NodeType t, const Rational& r) -> MathExpression {
    struct SqrtValue { Rational a; std::int64_t s; bool defined; };

    auto sin_value = [](const Rational& x, SqrtValue& out) -> bool {
        Rational six = x * Rational(6);
        std::int64_t m = 0;

        if (rational_int(six, m)) {
            static const std::int64_t num[12] = { 0, 1, 1, 1, 1, 1, 0, -1, -1, -1, -1, -1 };
            static const std::int64_t den[12] = { 1, 2, 2, 1, 2, 2, 1,  2,  2,  1,  2,  2 };
            static const std::int64_t rad[12] = { 1, 1, 3, 1, 3, 1, 1,  1,  3,  1,  3,  1 };
            const std::int64_t i = mod_positive(m, 12);
            out = { Rational(BigInt(num[i]), BigInt(den[i])), rad[i], true };
            return true;
        }

        Rational four = x * Rational(4);

        if (rational_int(four, m)) {
            const std::int64_t i = mod_positive(m, 8);
            const std::int64_t sgn = (i == 1 || i == 3) ? 1 : -1;
            out = { Rational(BigInt(sgn), BigInt(2)), 2, true };
            return true;
        }

        return false;
    };

    auto recip = [](const SqrtValue& v) -> SqrtValue {
        if (v.a.is_zero()) return { Rational(), 1, false };
        return { (v.a * Rational(v.s)).reciprocal(), v.s, true };
    };

    auto quotient = [](const SqrtValue& n, const SqrtValue& d, SqrtValue& out) -> bool {
        if (d.a.is_zero()) { out = { Rational(), 1, false }; return true; }
        if (n.a.is_zero()) { out = { Rational(), 1, true }; return true; }

        if (n.s == d.s) {
            out = { n.a / d.a, 1, true };
            return true;
        }

        if (d.s == 1) {
            out = { n.a / d.a, n.s, true };
            return true;
        }

        if (n.s == 1) {
            out = { n.a / (d.a * Rational(d.s)), d.s, true };
            return true;
        }

        return false;
    };

    SqrtValue s{}, c{};
    if (!sin_value(r, s)) return MathExpression{};
    if (!sin_value(r + Rational(BigInt(1), BigInt(2)), c)) return MathExpression{};
    SqrtValue v{};

    switch (t) {
        case NodeType::Sin: v = s; break;
        case NodeType::Cos: v = c; break;
        case NodeType::Csc: v = recip(s); break;
        case NodeType::Sec: v = recip(c); break;
        case NodeType::Tan: if (!quotient(s, c, v)) return MathExpression{}; break;
        case NodeType::Cot: if (!quotient(c, s, v)) return MathExpression{}; break;
        default: return MathExpression{};
    }

    if (!v.defined) return undef();
    return scaled_sqrt(v.a, v.s);
}

auto MathExpressionSimplifier::nan_check(MathExpression candidate, bool undefined_on_nan) -> MathExpression {
    EvalContext ctx;
    const double v = eval_node(candidate.get(), ctx);
    if (std::isnan(v)) return undefined_on_nan ? undef() : nan_node();
    return candidate;
}

auto MathExpressionSimplifier::gamma_exact(const Rational& x) -> MathExpression {
    std::int64_t n = 0;

    if (rational_int(x, n)) {
        if (n <= 0) return nan_node();
        if (n - 1 > factorial_limit) return MathExpression{};
        return konst(Rational(factorial_of(n - 1)));
    }

    if (!x.is_half_integer()) return MathExpression{};
    const Rational shifted = x - Rational(BigInt(1), BigInt(2));
    std::int64_t k = 0;
    if (!rational_int(shifted, k)) return MathExpression{};
    if (k > factorial_limit / 2 || k < -factorial_limit / 2) return MathExpression{};
    MathExpression sqrt_pi = mgr_.unary(NodeType::Sqrt, pi_node());
    Rational coeff;

    if (k >= 0) {
        coeff = Rational(factorial_of(2 * k), Rational::int_pow(BigInt(4), static_cast<std::uint64_t>(k)) * factorial_of(k));
    } else {
        const std::int64_t m = -k;
        BigInt num = Rational::int_pow(BigInt(4), static_cast<std::uint64_t>(m)) * factorial_of(m);
        if (m & 1) num.negate_mutable();
        coeff = Rational(num, factorial_of(2 * m));
    }

    if (coeff.is_one()) return sqrt_pi;
    return mgr_.binary(NodeType::Multiply, konst(coeff), sqrt_pi);
}

auto MathExpressionSimplifier::fold_power_exact(const Rational& base, const Rational& exp) -> MathExpression {
    std::int64_t k = 0;

    if (rational_int(exp, k)) {
        if (base.is_zero() && k < 0) return undef();
        Rational out;
        if (base.pow(k, out)) return konst(out);
        return MathExpression{};
    }

    if (base.is_negative()) return nan_node();
    std::int64_t p = 0, q = 0;
    if (!Rational(exp.numerator()).to_int64(p) || !Rational(exp.denominator()).to_int64(q)) return MathExpression{};
    Rational root;
    if (!base.exact_root(static_cast<std::uint64_t>(q), root)) return MathExpression{};
    Rational out;
    if (root.pow(p, out)) return konst(out);
    return MathExpression{};
}

auto MathExpressionSimplifier::fold_unary_exact(NodeType t, MathExpression child) -> MathExpression {
    const MathExpressionNode* c = child.get();
    Rational pr;

    if (pi_multiple(c, pr)) {
        MathExpression tv = trig_at_pi_multiple(t, pr);
        if (tv) return tv;
    }

    if (!is_const(c)) return MathExpression{};
    const Rational q = qv(c);
    const Rational one(1);
    const Rational half(BigInt(1), BigInt(2));

    switch (t) {
        case NodeType::AbsoluteValue:  return konst(q.abs());
        case NodeType::Floor:          return konst(Rational(q.floor()));
        case NodeType::Ceil:           return konst(Rational(q.ceil()));
        case NodeType::Round:          return konst(Rational(q.round()));
        case NodeType::Truncate:       return konst(Rational(q.trunc()));
        case NodeType::IntegerPart:    return konst(Rational(q.trunc()));
        case NodeType::FractionalPart: return konst(q.fractional_part());

        case NodeType::Sin:
        case NodeType::Cos:
        case NodeType::Tan:
        case NodeType::Csc:
        case NodeType::Sec:
        case NodeType::Cot:
            if (q.is_zero()) return trig_at_pi_multiple(t, Rational());
            return MathExpression{};

        case NodeType::Arcsin:
            if (q > one || q < -one) return undef();
            if (q.is_zero()) return konst(Rational());
            if (q == one)    return pi_times(half);
            if (q == -one)   return pi_times(-half);
            if (q == half)   return pi_times(Rational(BigInt(1), BigInt(6)));
            if (q == -half)  return pi_times(Rational(BigInt(-1), BigInt(6)));
            return MathExpression{};

        case NodeType::Arccos:
            if (q > one || q < -one) return undef();
            if (q == one)    return konst(Rational());
            if (q.is_zero()) return pi_times(half);
            if (q == -one)   return pi_times(one);
            if (q == half)   return pi_times(Rational(BigInt(1), BigInt(3)));
            if (q == -half)  return pi_times(Rational(BigInt(2), BigInt(3)));
            return MathExpression{};

        case NodeType::Arctan:
            if (q.is_zero()) return konst(Rational());
            if (q == one)    return pi_times(Rational(BigInt(1), BigInt(4)));
            if (q == -one)   return pi_times(Rational(BigInt(-1), BigInt(4)));
            return MathExpression{};

        case NodeType::Arccot:
            if (q.is_zero()) return pi_times(half);
            if (q == one)    return pi_times(Rational(BigInt(1), BigInt(4)));
            if (q == -one)   return pi_times(Rational(BigInt(-1), BigInt(4)));
            return MathExpression{};

        case NodeType::Arccsc:
            if (q < one && q > -one) return undef();
            if (q == one)  return pi_times(half);
            if (q == -one) return pi_times(-half);
            return MathExpression{};

        case NodeType::Arcsec:
            if (q < one && q > -one) return undef();
            if (q == one)  return konst(Rational());
            if (q == -one) return pi_times(one);
            return MathExpression{};

        case NodeType::Sinh:
        case NodeType::Tanh:
        case NodeType::Arcsinh:
            if (q.is_zero()) return konst(Rational());
            return MathExpression{};

        case NodeType::Cosh:
        case NodeType::Sech:
            if (q.is_zero()) return konst(one);
            return MathExpression{};

        case NodeType::Csch:
        case NodeType::Coth:
        case NodeType::Arccsch:
            if (q.is_zero()) return undef();
            return MathExpression{};

        case NodeType::Arccosh:
            if (q < one)  return undef();
            if (q == one) return konst(Rational());
            return MathExpression{};

        case NodeType::Arctanh:
            if (!(q < one && q > -one)) return undef();
            if (q.is_zero()) return konst(Rational());
            return MathExpression{};

        case NodeType::Arcsech:
            if (!q.is_positive() || q > one) return undef();
            if (q == one) return konst(Rational());
            return MathExpression{};

        case NodeType::Arccoth:
            if (!(q > one || q < -one)) return undef();
            return MathExpression{};

        default:
            return MathExpression{};
    }
}

auto MathExpressionSimplifier::fold_binary_exact(NodeType t, MathExpression l, MathExpression r) -> MathExpression {
    if (!is_const(l.get()) || !is_const(r.get())) return MathExpression{};
    const Rational a = qv(l.get());
    const Rational b = qv(r.get());

    switch (t) {
        case NodeType::Modulo: {
            if (b.is_zero()) return undef();
            return konst(a - b * Rational((a / b).trunc()));
        }

        case NodeType::Root: {
            if (b.is_zero()) return undef();
            std::int64_t k = 0;
            if (!rational_int(b, k)) return MathExpression{};
            if (a.is_negative() && (k % 2 == 0)) return undef();
            const std::uint64_t ak = k < 0 ? static_cast<std::uint64_t>(-k) : static_cast<std::uint64_t>(k);
            Rational root;
            if (!a.exact_root(ak, root)) return MathExpression{};
            if (k < 0) {
                if (root.is_zero()) return undef();
                root = root.reciprocal();
            }
            return konst(root);
        }

        default:
            return MathExpression{};
    }
}

auto MathExpressionSimplifier::dominant_poison(MathExpressionNode* a, MathExpressionNode* b) const -> MathExpressionNode* {
    auto rank = [](NodeType t) -> int {
        if (t == NodeType::Undefined)     return 3;
        if (t == NodeType::NaN)           return 2;
        if (t == NodeType::Indeterminate) return 1;
        return 0;
    };
    int ra = rank(a->type), rb = rank(b->type);
    if (ra == 0 && rb == 0) return nullptr;
    return (ra >= rb) ? a : b;
}

bool MathExpressionSimplifier::is_square(MathExpressionNode* n, NodeType fn, MathExpressionNode*& inner) const {
    if (n->type != NodeType::Power) return false;
    if (!is_const(n->power.exponent, 2.0)) return false;
    if (n->power.base->type != fn) return false;
    inner = n->power.base->unary.child;
    return true;
}

auto MathExpressionSimplifier::simplify_node(MathExpressionNode* n) -> MathExpression {
    switch (n->type) {
        case NodeType::Constant:
        case NodeType::Variable:
        case NodeType::Indeterminate:
        case NodeType::PositiveInfinity:
        case NodeType::NegativeInfinity:
        case NodeType::NaN:
        case NodeType::Undefined:
        case NodeType::Invalid:
        case NodeType::Pi:
        case NodeType::EulerNumber:
            return MathExpression(n);

        case NodeType::AppliedFunction: return simplify_applied(n);

        case NodeType::Negate:     return simplify_negate(n);
        case NodeType::Add:        return simplify_add(n);
        case NodeType::Subtract:   return simplify_subtract(n);
        case NodeType::Multiply:   return simplify_multiply(n);
        case NodeType::Divide:     return simplify_divide(n);
        case NodeType::Sign:       return simplify_sign(n);
        case NodeType::UnitStep:   return simplify_unit_step(n);
        case NodeType::Power:      return simplify_power(n);
        case NodeType::NaturalExp: return simplify_exp(n);
        case NodeType::NaturalLog: return simplify_ln(n);
        case NodeType::Log:        return simplify_log(n);
        case NodeType::Sqrt:       return simplify_sqrt(n);
        case NodeType::Cbrt:       return simplify_cbrt(n);
        case NodeType::Sinc:       return simplify_sinc(n);
        case NodeType::NormalSinc: return simplify_normalsinc(n);
        case NodeType::Erf:
        case NodeType::Erfc:
            return simplify_erf_erfc(n);
        case NodeType::Erfi:       return simplify_erfi(n);
        case NodeType::InverseErf:
        case NodeType::InverseErfc:
        case NodeType::InverseErfi:
            return simplify_inverse_erf_family(n);
        case NodeType::ErfGeneralized:
        case NodeType::ErfcGeneralized:
            return simplify_erf_generalized(n);
        case NodeType::Gamma:
        case NodeType::Factorial:
            return simplify_gamma_factorial(n);
        case NodeType::Digamma:
        case NodeType::Trigamma:
            return simplify_digamma_trigamma(n);
        case NodeType::BetaFunction:
            return simplify_beta(n);
        case NodeType::LambertW:
            return simplify_lambert_w(n);
        case NodeType::ChebyshevU:
        case NodeType::ChebyshevT:
            return simplify_chebyshev(n);
        case NodeType::ExponentialIntegral:
            return simplify_exp_integral(n);
        case NodeType::LogarithmicIntegral:
            return simplify_log_integral(n);
        case NodeType::ExponentialIntegralGeneralized:
            return simplify_exp_integral_gen(n);
        case NodeType::LogarithmicIntegralGeneralized:
            return simplify_log_integral_gen(n);
        case NodeType::BellPolynomial:
            return simplify_bell(n);
        case NodeType::SinIntegral:
        case NodeType::CosIntegral:
        case NodeType::SinhIntegral:
        case NodeType::CoshIntegral:
            return simplify_sin_cos_integral(n);
        case NodeType::FresnelS:
        case NodeType::FresnelC:
            return simplify_fresnel(n);
        case NodeType::Dilogarithm:
        case NodeType::Trilogarithm:
            return simplify_di_tri_log(n);
        case NodeType::Polylog:
            return simplify_polylog(n);
        case NodeType::RiemannZeta:
            return simplify_riemann(n);
        case NodeType::SpenceFunction:
        case NodeType::SpenceIntegral:
            return simplify_spence(n);
        case NodeType::RogersL:
        case NodeType::RogersLR:
            return simplify_rogers(n);
        case NodeType::Gudermannian:
        case NodeType::InverseGudermannian:
            return simplify_gudermannian(n);
        case NodeType::Combination:
            return simplify_combination(n);
        case NodeType::Permutation:
            return simplify_permutation(n);
        case NodeType::FibonacciSequence:
        case NodeType::LucasSequence:
            return simplify_fib_lucas_sequence(n);
        case NodeType::FibonacciPolynomial:
        case NodeType::LucasPolynomial:
            return simplify_fib_lucas_poly(n);

        default:
            return simplify_generic(n);
    }
}

auto MathExpressionSimplifier::rebuild_unary(NodeType t, MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (MathExpression folded = fold_unary_exact(t, child)) return folded;
    MathExpression result = mgr_.unary(t, child);
    if (is_const(child.get()) || is_named_const(child.get())) return nan_check(result, true);
    return result;
}

auto MathExpressionSimplifier::rebuild_binary(NodeType t, MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (MathExpression folded = fold_binary_exact(t, l, r)) return folded;
    MathExpression result = mgr_.binary(t, l, r);
    const bool lc = is_const(l.get()) || is_named_const(l.get());
    const bool rc = is_const(r.get()) || is_named_const(r.get());
    if (lc && rc) return nan_check(result, true);
    return result;
}

auto MathExpressionSimplifier::simplify_generic(MathExpressionNode* n) -> MathExpression {
    if (NodeKeyHash::is_unary(n->type))  return rebuild_unary(n->type, n);
    if (NodeKeyHash::is_binary(n->type)) return rebuild_binary(n->type, n);
    return MathExpression(n);
}

auto MathExpressionSimplifier::simplify_negate(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))           return MathExpression(child.get());
    if (is_const(child.get()))            return konst(-qv(child.get()));
    if (is_infinite(child.get()))         return negate_inf(child.get());
    if (child.get()->type == NodeType::Negate) return MathExpression(child.get()->unary.child);
    return mgr_.unary(NodeType::Negate, child);
}

auto MathExpressionSimplifier::simplify_add(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        if (is_infinite(l.get()) && is_infinite(r.get())) {
            return inf_sign(l.get()) == inf_sign(r.get()) ? MathExpression(l.get()) : indet();
        }
        return is_infinite(l.get()) ? MathExpression(l.get()) : MathExpression(r.get());
    }

    if (is_const(l.get()) && is_const(r.get())) return konst(qv(l.get()) + qv(r.get()));
    if (is_const(l.get(), 0.0)) return r;
    if (is_const(r.get(), 0.0)) return l;

    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if ((is_square(l.get(), NodeType::Sin, i1) && is_square(r.get(), NodeType::Cos, i2) && i1 == i2) ||
            (is_square(l.get(), NodeType::Cos, i1) && is_square(r.get(), NodeType::Sin, i2) && i1 == i2))
            return konst(1.0);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_square(l.get(), NodeType::Tan, inner) && is_const(r.get(), 1.0)) return square(NodeType::Sec, inner);
        if (is_square(r.get(), NodeType::Tan, inner) && is_const(l.get(), 1.0)) return square(NodeType::Sec, inner);
        if (is_square(l.get(), NodeType::Cot, inner) && is_const(r.get(), 1.0)) return square(NodeType::Csc, inner);
        if (is_square(r.get(), NodeType::Cot, inner) && is_const(l.get(), 1.0)) return square(NodeType::Csc, inner);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_square(l.get(), NodeType::Sinh, inner) && is_const(r.get(), 1.0)) return square(NodeType::Cosh, inner);
        if (is_square(r.get(), NodeType::Sinh, inner) && is_const(l.get(), 1.0)) return square(NodeType::Cosh, inner);
        if (is_square(l.get(), NodeType::Coth, inner) && is_const(r.get(), 1.0)) return square(NodeType::Csch, inner);
        if (is_square(r.get(), NodeType::Coth, inner) && is_const(l.get(), 1.0)) return square(NodeType::Csch, inner);
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cos, i1) &&
            r.get()->type == NodeType::Negate && is_square(r.get()->unary.child, NodeType::Sin, i2) &&
            i1 == i2)
        {
            return mgr_.unary(NodeType::Cos, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        }
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if ((is_square(l.get(), NodeType::Cosh, i1) && is_square(r.get(), NodeType::Sinh, i2) && i1 == i2) ||
            (is_square(l.get(), NodeType::Sinh, i1) && is_square(r.get(), NodeType::Cosh, i2) && i1 == i2))
        {
            return mgr_.unary(NodeType::Cosh, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        }
    }

    if (r.get()->type == NodeType::Negate) {
        return mgr_.binary(NodeType::Subtract, l, MathExpression(r.get()->unary.child));
    }

    if (r.get()->type == NodeType::Multiply) {
        auto* rl = r.get()->binary.left;
        if (rl->type == NodeType::Constant && qv(rl).is_negative()) {
            MathExpression pos_const = konst(-qv(rl));
            MathExpression pos_mul   = mgr_.binary(NodeType::Multiply, pos_const, MathExpression(r.get()->binary.right));
            return mgr_.binary(NodeType::Subtract, l, pos_mul);
        }
    }

    return mgr_.binary(NodeType::Add, l, r);
}

auto MathExpressionSimplifier::simplify_subtract(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        if (is_infinite(l.get()) && is_infinite(r.get())) {
            return inf_sign(l.get()) == inf_sign(r.get()) ? indet() : MathExpression(l.get());
        }
        if (is_infinite(l.get())) return MathExpression(l.get());
        return negate_inf(r.get());
    }
    if (is_const(l.get()) && is_const(r.get())) return konst(qv(l.get()) - qv(r.get()));
    if (is_const(r.get(), 0.0)) return l;
    if (is_const(l.get(), 0.0)) return mgr_.unary(NodeType::Negate, r);
    if (l.get() == r.get())     return konst(0.0);
    if (r.get()->type == NodeType::Negate) return mgr_.binary(NodeType::Add, l, MathExpression(r.get()->unary.child));
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cos, i1) && is_square(r.get(), NodeType::Sin, i2) && i1 == i2)
            return mgr_.unary(NodeType::Cos, mgr_.binary(NodeType::Multiply, konst(2.0), MathExpression(i1)));
        if (is_square(l.get(), NodeType::Sec, i1) && is_square(r.get(), NodeType::Tan, i2) && i1 == i2)
            return konst(1.0);
        if (is_square(l.get(), NodeType::Csc, i1) && is_square(r.get(), NodeType::Cot, i2) && i1 == i2)
            return konst(1.0);
    }
    {
        MathExpressionNode *i1 = nullptr, *i2 = nullptr;
        if (is_square(l.get(), NodeType::Cosh, i1) && is_square(r.get(), NodeType::Sinh, i2) && i1 == i2)
            return konst(1.0);
        if (is_square(l.get(), NodeType::Coth, i1) && is_square(r.get(), NodeType::Csch, i2) && i1 == i2)
            return konst(1.0);
    }
    {
        MathExpressionNode *inner = nullptr;
        if (is_const(l.get(), 1.0) && is_square(r.get(), NodeType::Tanh, inner))
            return square(NodeType::Sech, inner);
    }

    if (r.get()->type == NodeType::Negate) { return mgr_.binary(NodeType::Add, l, MathExpression(r.get()->unary.child)); }
    return mgr_.binary(NodeType::Subtract, l, r);
}

auto MathExpressionSimplifier::simplify_multiply(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_infinite(l.get()) || is_infinite(r.get())) {
        auto check_zero_inf = [&](MathExpression& inf_side, MathExpression& other) -> MathExpression {
            if (is_const(other.get(), 0.0)) return indet();
            if (is_const(other.get()) || is_named_const(other.get())) {
                const int s = known_sign(other.get());
                if (s > 0) return MathExpression(inf_side.get());
                if (s < 0) return negate_inf(inf_side.get());
            }
            if (is_infinite(other.get())) {
                return inf_sign(inf_side.get()) * inf_sign(other.get()) > 0 ? pos_inf() : neg_inf();
            }
            return MathExpression{};
        };

        MathExpression result = is_infinite(l.get()) ? check_zero_inf(l, r) : check_zero_inf(r, l);
        if (result) return result;
    }
    if (is_const(l.get()) && is_const(r.get())) return konst(qv(l.get()) * qv(r.get()));
    if (is_const(l.get(), 0.0) || is_const(r.get(), 0.0)) return konst(0.0);
    if (is_const(l.get(),  1.0))  return r;
    if (is_const(r.get(),  1.0))  return l;
    if (is_const(l.get(), -1.0))  return mgr_.unary(NodeType::Negate, r);
    if (is_const(r.get(), -1.0))  return mgr_.unary(NodeType::Negate, l);
    return mgr_.binary(NodeType::Multiply, l, r);
}

auto MathExpressionSimplifier::simplify_divide(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (is_const(r.get(), 0.0)) { return undef(); }
    if (is_infinite(r.get())) {
        if (is_infinite(l.get())) return indet();
        return konst(0.0);
    }
    if (is_infinite(l.get())) {
        if (is_const(r.get()) || is_named_const(r.get())) {
            const int s = known_sign(r.get());
            if (s > 0) return MathExpression(l.get());
            if (s < 0) return negate_inf(l.get());
        }
    }
    if (is_const(l.get()) && is_const(r.get())) return konst(qv(l.get()) / qv(r.get()));
    if (is_const(l.get(), 0.0)) return konst(0.0);
    if (is_const(r.get(), 1.0)) return l;
    if (l.get() == r.get())     return konst(1.0);
    return mgr_.binary(NodeType::Divide, l, r);
}

auto MathExpressionSimplifier::simplify_sign(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return konst( 1.0);
    if (is_neg_inf(child.get())) return konst(-1.0);

    if (is_const(child.get()) || is_named_const(child.get())) {
        return konst(Rational(known_sign(child.get())));
    }

    if (child.get()->type == NodeType::Negate) {
        return mgr_.unary(NodeType::Negate, mgr_.unary(NodeType::Sign, MathExpression(child.get()->unary.child)));
    }

    return mgr_.unary(NodeType::Sign, child);
}

auto MathExpressionSimplifier::simplify_unit_step(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return konst(1.0);
    if (is_neg_inf(child.get())) return konst(0.0);
    if (is_const(child.get()) || is_named_const(child.get())) { return known_sign(child.get()) >= 0 ? konst(1.0) : konst(0.0); }
    return mgr_.unary(NodeType::UnitStep, child);
}

auto MathExpressionSimplifier::simplify_power(MathExpressionNode* n) -> MathExpression {
    MathExpression base = simplify_node(n->power.base);
    MathExpression exp  = simplify_node(n->power.exponent);
    if (auto p = propagate_poison2(base, exp)) return p;
    if (is_const(base.get(), 0.0)) {
        if (is_const(exp.get(), 0.0))
            return indet();
        if (is_const(exp.get()) && qv(exp.get()).is_negative())
            return undef();
        if (is_neg_inf(exp.get()))
            return undef();
        if (is_pos_inf(exp.get()))
            return konst(0.0);
        return konst(0.0);
    }
    if (is_const(exp.get(), 0.0)) {
        if (is_infinite(base.get()))  return indet();
        return konst(1.0);
    }
    if (is_const(exp.get(), 1.0))    return base;
    if (is_const(base.get(), 1.0)) {
        if (is_infinite(exp.get()))   return indet();
        return konst(1.0);
    }
    if (is_pos_inf(base.get())) {
        const int s = (is_const(exp.get()) || is_named_const(exp.get())) ? known_sign(exp.get()) : 0;
        if (s > 0) return pos_inf();
        if (s < 0) return konst(0.0);
        if (is_pos_inf(exp.get())) return pos_inf();
        if (is_neg_inf(exp.get())) return konst(0.0);
    }
    if (is_const(base.get()) && is_const(exp.get())) {
        if (MathExpression folded = fold_power_exact(qv(base.get()), qv(exp.get()))) return folded;
    }
    if (is_e(base.get())) return make_exp(exp);
    if (is_const(exp.get(), -1.0)) return mgr_.binary(NodeType::Divide, konst(1.0), base);
    return mgr_.power(base, exp);
}

auto MathExpressionSimplifier::simplify_exp(MathExpressionNode* n) -> MathExpression {
    return make_exp(simplify_node(n->unary.child));
}

auto MathExpressionSimplifier::simplify_ln(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        if (v.is_negative()) return nan_node();
        if (v.is_zero()) return neg_inf();
        if (v.is_one())  return konst(0.0);
    }
    if (is_e(child.get())) return konst(1.0);
    if (is_neg_inf(child.get())) return nan_node();
    if (is_pos_inf(child.get())) return pos_inf();
    if (child.get()->type == NodeType::NaturalExp) return MathExpression(child.get()->unary.child);
    if (child.get()->type == NodeType::Power) {
        MathExpression b(child.get()->power.base);
        MathExpression e(child.get()->power.exponent);
        return mgr_.binary(NodeType::Multiply, e, mgr_.unary(NodeType::NaturalLog, b));
    }
    return mgr_.unary(NodeType::NaturalLog, child);
}

auto MathExpressionSimplifier::simplify_log(MathExpressionNode* n) -> MathExpression {
    MathExpression arg  = simplify_node(n->binary.left);
    MathExpression base = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, base)) return p;
    if (is_const(base.get())) {
        const Rational b = qv(base.get());
        if (!b.is_positive()) return nan_node();
        if (b.is_one()) return undef();
    }
    if (is_infinite(base.get())) return undef();
    if (is_const(arg.get())) {
        const Rational a = qv(arg.get());
        if (a.is_negative()) return nan_node();
        if (a.is_zero()) return neg_inf();
    }
    if (is_neg_inf(arg.get())) return nan_node();
    if (is_pos_inf(arg.get())) return pos_inf();
    if (is_const(arg.get(), 1.0)) return konst(0.0);
    if (arg.get() == base.get())  return konst(1.0);

    if (is_const(arg.get()) && is_const(base.get())) {
        const Rational a = qv(arg.get());
        const Rational b = qv(base.get());
        const double guess = std::log(std::fabs(a.to_double())) / std::log(std::fabs(b.to_double()));

        if (std::isfinite(guess) && std::fabs(guess) < 4096.0) {
            const std::int64_t k0 = static_cast<std::int64_t>(std::llround(guess));

            for (std::int64_t k = k0 - 1; k <= k0 + 1; ++k) {
                Rational pw;
                if (b.pow(k, pw) && pw == a) return konst(Rational(k));
            }
        }
    }

    if (is_e(base.get())) return simplify_ln(mgr_.unary(NodeType::NaturalLog, arg).get());
    MathExpression ln_arg  = simplify_ln(mgr_.unary(NodeType::NaturalLog, arg).get());
    MathExpression ln_base = simplify_ln(mgr_.unary(NodeType::NaturalLog, base).get());
    return mgr_.binary(NodeType::Divide, ln_arg, ln_base);
}

auto MathExpressionSimplifier::simplify_sqrt(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))      return MathExpression(child.get());
    if (is_neg_inf(child.get()))     return nan_node();
    if (is_pos_inf(child.get()))     return pos_inf();
    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        if (v.is_negative()) return nan_node();
        Rational root;
        if (v.exact_root(2, root)) return konst(root);
    }
    if (child.get()->type == NodeType::Power && is_const(child.get()->power.exponent, 2.0)) return mgr_.unary(NodeType::AbsoluteValue, MathExpression(child.get()->power.base));
    return mgr_.unary(NodeType::Sqrt, child);
}

auto MathExpressionSimplifier::simplify_cbrt(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))  return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_neg_inf(child.get())) return neg_inf();
    if (is_const(child.get())) {
        Rational root;
        if (qv(child.get()).exact_root(3, root)) return konst(root);
    }
    if (child.get()->type == NodeType::Power && is_const(child.get()->power.exponent, 3.0)) return MathExpression(child.get()->power.base);
    return mgr_.unary(NodeType::Cbrt, child);
}

auto MathExpressionSimplifier::simplify_sinc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_infinite(child.get())) return konst(0.0);
    if (is_const(child.get(), 0.0)) return konst(1.0);
    Rational r;
    if (pi_multiple(child.get(), r) && r.is_integer()) return r.is_zero() ? konst(1.0) : konst(0.0);
    return mgr_.unary(NodeType::Sinc, child);
}

auto MathExpressionSimplifier::simplify_normalsinc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_infinite(child.get())) return konst(0.0);
    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        if (v.is_zero()) return konst(1.0);
        if (v.is_integer()) return konst(0.0);
    }
    return mgr_.unary(NodeType::NormalSinc, child);
}

auto MathExpressionSimplifier::simplify_erf_erfc(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_pos_inf(child.get()))  return n->type == NodeType::Erf ? konst(1.0)  : konst(0.0);
    if (is_neg_inf(child.get()))  return n->type == NodeType::Erf ? konst(-1.0) : konst(2.0);
    if (is_const(child.get(), 0.0)) return n->type == NodeType::Erf ? konst(0.0) : konst(1.0);
    if (child.get()->type == NodeType::InverseErf && n->type == NodeType::Erf) { return MathExpression(child.get()->unary.child); }
    if (child.get()->type == NodeType::InverseErfc && n->type == NodeType::Erfc) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_erfi(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get()))   return MathExpression(child.get());
    if (is_pos_inf(child.get()))  return pos_inf();
    if (is_neg_inf(child.get()))  return neg_inf();
    if (is_const(child.get(), 0.0)) return konst(0.0);
    if (child.get()->type == NodeType::InverseErfi) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(NodeType::Erfi, child);
}

auto MathExpressionSimplifier::simplify_inverse_erf_family(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_infinite(child.get())) {
        if (n->type == NodeType::InverseErfi) {
            return is_pos_inf(child.get()) ? pos_inf() : neg_inf();
        } else {
            return nan_node();
        }
    }

    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        const Rational one(1);
        const Rational two(2);

        switch (n->type) {
            case NodeType::InverseErf: {
                if (v < -one || v > one) return nan_node();
                if (v == -one) return neg_inf();
                if (v ==  one) return pos_inf();
                if (v.is_zero()) return konst(0.0);
                break;
            }

            case NodeType::InverseErfc: {
                if (v.is_negative() || v > two) return nan_node();
                if (v.is_zero()) return pos_inf();
                if (v == two) return neg_inf();
                if (v == one) return konst(0.0);
                break;
            }

            case NodeType::InverseErfi: {
                if (v.is_zero()) return konst(0.0);
                break;
            }

            default:
                break;
        }
    }

    if (n->type == NodeType::InverseErf && child.get()->type == NodeType::Erf) { return MathExpression(child.get()->unary.child); }
    if (n->type == NodeType::InverseErfc && child.get()->type == NodeType::Erfc) { return MathExpression(child.get()->unary.child); }
    if (n->type == NodeType::InverseErfi && child.get()->type == NodeType::Erfi) { return MathExpression(child.get()->unary.child); }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_erf_generalized(MathExpressionNode* n) -> MathExpression {
    MathExpression l = simplify_node(n->binary.left);
    MathExpression r = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(l, r)) return p;
    if (l.get() == r.get()) return konst(0.0);
    return mgr_.binary(n->type, l, r);
}

auto MathExpressionSimplifier::simplify_gamma_factorial(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (n->type == NodeType::Gamma) {
        if (is_pos_inf(child.get())) return pos_inf();
        if (is_neg_inf(child.get())) return nan_node();
        if (is_const(child.get())) {
            if (MathExpression g = gamma_exact(qv(child.get()))) return g;
        }
    } else {
        if (is_pos_inf(child.get())) return pos_inf();
        if (is_neg_inf(child.get())) return nan_node();
        if (is_const(child.get())) {
            const Rational v = qv(child.get());
            if (v.is_negative() && v.is_integer()) return nan_node();
            if (MathExpression g = gamma_exact(v + Rational(1))) return g;
        }
    }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_digamma_trigamma(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return n->type == NodeType::Digamma ? pos_inf() : konst(0.0);
    if (is_neg_inf(child.get())) return nan_node();
    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        if (!v.is_positive() && v.is_integer()) return nan_node();
        if (n->type == NodeType::Trigamma && v.is_one()) {
            return mgr_.binary(NodeType::Divide, mgr_.power(pi_node(), konst(2.0)), konst(6.0));
        }
    }
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_beta(MathExpressionNode* n) -> MathExpression {
    MathExpression lchild = simplify_node(n->binary.left);
    MathExpression rchild = simplify_node(n->binary.right);
    if (is_poison(lchild.get())) return MathExpression(lchild.get());
    if (is_poison(rchild.get())) return MathExpression(rchild.get());
    bool lc = is_const(lchild.get());
    bool rc = is_const(rchild.get());

    if (lc && rc) {
        const Rational a = qv(lchild.get());
        const Rational b = qv(rchild.get());
        const Rational s = a + b;
        if ((!a.is_positive() && a.is_integer()) || (!b.is_positive() && b.is_integer()) || (!s.is_positive() && s.is_integer())) return nan_node();
        std::int64_t ai = 0, bi = 0;

        if (rational_int(a, ai) && rational_int(b, bi) && ai + bi - 1 <= factorial_limit) {
            return konst(Rational(factorial_of(ai - 1) * factorial_of(bi - 1), factorial_of(ai + bi - 1)));
        }
    }

    return mgr_.binary(NodeType::BetaFunction, lchild, rchild);
}

auto MathExpressionSimplifier::simplify_lambert_w(MathExpressionNode* n) -> MathExpression {
    MathExpression arg    = simplify_node(n->binary.left);
    MathExpression branch = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, branch)) return p;

    if (is_const(branch.get())) {
        const Rational bv = qv(branch.get());
        if (!bv.is_integer()) return nan_node();
        std::int64_t k = 0;
        if (!rational_int(bv, k)) return mgr_.binary(NodeType::LambertW, arg, branch);

        if (is_const(arg.get(), 0.0)) {
            if (k == 0) return konst(0.0);
            return neg_inf();
        }

        if (is_neg_inv_e(arg.get()) && (k == 0 || k == -1)) return konst(-1.0);
        if (k == 0 && is_e(arg.get())) return konst(1.0);
    }

    return mgr_.binary(NodeType::LambertW, arg, branch);
}

auto MathExpressionSimplifier::simplify_chebyshev(MathExpressionNode* n) -> MathExpression {
    MathExpression x = simplify_node(n->binary.left);
    MathExpression k = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, k)) return p;
    const bool is_t = n->type == NodeType::ChebyshevT;

    if (is_const(k.get())) {
        if (is_const(k.get(), 0.0)) { return konst(1.0); }
        if (is_const(k.get(), 1.0)) { return is_t ? x : mgr_.binary(NodeType::Multiply, konst(2.0), x); }
    }

    if (is_const(x.get()) && is_const(k.get())) {
        std::int64_t m = 0;

        if (rational_int(qv(k.get()), m) && m >= 0 && m <= polynomial_limit) {
            const Rational xv = qv(x.get());
            const Rational two_x = Rational(2) * xv;
            Rational prev(1);
            Rational cur = is_t ? xv : two_x;

            for (std::int64_t i = 1; i < m; ++i) {
                Rational next = two_x * cur - prev;
                prev = std::move(cur);
                cur  = std::move(next);
            }

            return konst(cur);
        }

        return nan_check(mgr_.binary(n->type, x, k), true);
    }

    return mgr_.binary(n->type, x, k);
}

auto MathExpressionSimplifier::simplify_exp_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_neg_inf(child.get())) return konst(0.0);
    if (is_const(child.get(), 0.0)) return neg_inf();
    return mgr_.unary(NodeType::ExponentialIntegral, child);
}

auto MathExpressionSimplifier::simplify_log_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_pos_inf(child.get())) return pos_inf();
    if (is_const(child.get())) {
        const Rational v = qv(child.get());
        if (v.is_negative()) return nan_node();
        if (v.is_zero()) return konst(0.0);
        if (v.is_one()) return neg_inf();
    }
    return mgr_.unary(NodeType::LogarithmicIntegral, child);
}

auto MathExpressionSimplifier::simplify_exp_integral_gen(MathExpressionNode* n) -> MathExpression {
    MathExpression x  = simplify_node(n->binary.left);
    MathExpression nn = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, nn)) return p;

    // E_1(x) == -Ei(-x)
    if (is_const(nn.get(), 1.0)) {
        return simplify_node(
            mgr_.unary(
                NodeType::Negate,
                mgr_.unary(
                    NodeType::ExponentialIntegral,
                    mgr_.unary(NodeType::Negate, x)
                )
            ).get()
        );
    }

    if (is_const(x.get()) && is_const(nn.get())) return nan_check(mgr_.binary(NodeType::ExponentialIntegralGeneralized, x, nn), false);
    return mgr_.binary(NodeType::ExponentialIntegralGeneralized, x, nn);
}

auto MathExpressionSimplifier::simplify_log_integral_gen(MathExpressionNode* n) -> MathExpression {
    MathExpression x  = simplify_node(n->binary.left);
    MathExpression nn = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(x, nn)) return p;

    if (is_const(x.get()) && is_const(nn.get())) {
        if (qv(x.get()).is_negative()) return nan_node();
        return nan_check(mgr_.binary(NodeType::LogarithmicIntegralGeneralized, x, nn), false);
    }

    return mgr_.binary(NodeType::LogarithmicIntegralGeneralized, x, nn);
}

auto MathExpressionSimplifier::simplify_bell(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;

    if (is_const(idx.get())) {
        if (is_const(idx.get(), 0.0)) return konst(1.0);
        if (is_const(idx.get(), 1.0)) return arg;
    }

    return mgr_.binary(NodeType::BellPolynomial, arg, idx);
}

auto MathExpressionSimplifier::simplify_sin_cos_integral(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_sin  = (n->type == NodeType::SinIntegral);
    bool is_sinh = (n->type == NodeType::SinhIntegral);
    bool is_cosh = (n->type == NodeType::CoshIntegral);

    if (is_infinite(child.get())) {
        if (is_sin || is_sinh) { return is_pos_inf(child.get()) ? pos_inf() : neg_inf(); }
        if (is_cosh) { return is_pos_inf(child.get()) ? pos_inf() : neg_inf(); }
        return nan_node();
    }

    if (is_const(child.get(), 0.0)) {
        if (is_sin || is_sinh) return konst(0.0);
        return neg_inf();
    }

    if (is_const(child.get())) return nan_check(mgr_.unary(n->type, child), false);
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_fresnel(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get(), 0.0)) return konst(0.0);
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_riemann(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());

    if (n->type == NodeType::RiemannZeta && is_const(child.get())) {
        std::int64_t s = 0;

        if (rational_int(qv(child.get()), s)) {
            if (s == 0) return konst(Rational(BigInt(-1), BigInt(2)));
            if (s < 0 && (s % 2 == 0)) return konst(0.0);

            static const std::int64_t neg_num[7] = { -1, 1, -1, 1, -1, 691, -1 };
            static const std::int64_t neg_den[7] = { 12, 120, 252, 240, 132, 32760, 12 };

            if (s < 0 && s >= -13) {
                const std::int64_t i = (-s - 1) / 2;
                return konst(Rational(BigInt(neg_num[i]), BigInt(neg_den[i])));
            }

            static const std::int64_t pos_num[6] = { 1, 1, 1, 1, 1, 691 };
            static const std::int64_t pos_den[6] = { 6, 90, 945, 9450, 93555, 638512875 };

            if (s > 0 && s <= 12 && (s % 2 == 0)) {
                const std::int64_t i = s / 2 - 1;
                MathExpression pw = mgr_.power(pi_node(), konst(Rational(s)));
                if (pos_num[i] != 1) pw = mgr_.binary(NodeType::Multiply, konst(Rational(pos_num[i])), pw);
                return mgr_.binary(NodeType::Divide, pw, konst(Rational(pos_den[i])));
            }
        }

        return nan_check(mgr_.unary(n->type, child), false);
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_di_tri_log(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    bool is_di  = (n->type == NodeType::Dilogarithm);

    if (is_const(child.get())) {
        if (is_const(child.get(), 0.0)) return konst(0.0);

        if (is_di && is_const(child.get(), 1.0)) {
            return mgr_.binary(NodeType::Divide, mgr_.power(pi_node(), konst(2.0)), konst(6.0));
        }

        if (is_di && is_const(child.get(), -1.0)) {
            return mgr_.unary(NodeType::Negate, mgr_.binary(NodeType::Divide, mgr_.power(pi_node(), konst(2.0)), konst(12.0)));
        }

        return nan_check(mgr_.unary(n->type, child), false);
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_polylog(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;
    if (is_const(arg.get(), 0.0)) return konst(0.0);
    if (is_const(idx.get(), 2.0)) return simplify_di_tri_log(mgr_.unary(NodeType::Dilogarithm, arg).get());
    if (is_const(idx.get(), 3.0)) return simplify_di_tri_log(mgr_.unary(NodeType::Trilogarithm, arg).get());
    if (is_const(idx.get()) && is_const(arg.get())) return nan_check(mgr_.binary(NodeType::Polylog, arg, idx), false);
    return mgr_.binary(NodeType::Polylog, arg, idx);
}

auto MathExpressionSimplifier::simplify_spence(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get())) return nan_check(mgr_.unary(n->type, child), false);
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_rogers(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get())) return nan_check(mgr_.unary(n->type, child), false);
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_gudermannian(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());
    if (is_const(child.get(), 0.0)) return konst(0.0);
    if (is_const(child.get())) return nan_check(mgr_.unary(n->type, child), false);
    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_combination(MathExpressionNode* n) -> MathExpression {
    MathExpression nn = simplify_node(n->binary.left);
    MathExpression kk = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(nn, kk)) return p;
    if (is_const(kk.get(), 0.0)) return konst(1.0);
    if (is_const(kk.get(), 1.0)) return nn;
    if (nn.get() == kk.get())    return konst(1.0);
    if (is_const(nn.get(), 0.0)) return konst(0.0);

    if (is_const(nn.get()) && is_const(kk.get())) {
        std::int64_t nv = 0, kv = 0;
        const bool ni = rational_int(qv(nn.get()), nv);
        const bool ki = rational_int(qv(kk.get()), kv);

        if (ni && ki) {
            if (nv < 0 || kv < 0) return nan_node();
            if (kv > nv) return konst(0.0);
            if (nv <= binomial_limit) {
                const std::int64_t k = kv < nv - kv ? kv : nv - kv;
                return konst(Rational(falling_product(nv, k), factorial_of(k)));
            }
        }

        return nan_check(mgr_.binary(NodeType::Combination, nn, kk), false);
    }

    return mgr_.binary(NodeType::Combination, nn, kk);
}

auto MathExpressionSimplifier::simplify_permutation(MathExpressionNode* n) -> MathExpression {
    MathExpression nn = simplify_node(n->binary.left);
    MathExpression kk = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(nn, kk)) return p;
    if (is_const(kk.get(), 0.0)) return konst(1.0);
    if (is_const(kk.get(), 1.0)) return nn;
    if (nn.get() == kk.get()) { return mgr_.unary(NodeType::Factorial, nn); }
    if (is_const(nn.get(), 0.0)) return konst(0.0);

    if (is_const(nn.get()) && is_const(kk.get())) {
        std::int64_t nv = 0, kv = 0;
        const bool ni = rational_int(qv(nn.get()), nv);
        const bool ki = rational_int(qv(kk.get()), kv);

        if (ni && ki) {
            if (nv < 0 || kv < 0) return nan_node();
            if (kv > nv) return konst(0.0);
            if (nv <= binomial_limit) return konst(Rational(falling_product(nv, kv)));
        }

        return nan_check(mgr_.binary(NodeType::Permutation, nn, kk), false);
    }

    return mgr_.binary(NodeType::Permutation, nn, kk);
}

auto MathExpressionSimplifier::simplify_fib_lucas_sequence(MathExpressionNode* n) -> MathExpression {
    MathExpression child = simplify_node(n->unary.child);
    if (is_poison(child.get())) return MathExpression(child.get());

    if (is_const(child.get())) {
        std::int64_t k = 0;

        if (rational_int(qv(child.get()), k) && k <= sequence_limit && k >= -sequence_limit) {
            const std::int64_t ak = k < 0 ? -k : k;
            BigInt f, l;
            fib_lucas_pair(ak, f, l);

            if (n->type == NodeType::FibonacciSequence) {
                if (k < 0 && (ak % 2 == 0)) f.negate_mutable();
                return konst(Rational(f));
            }

            if (k < 0 && (ak % 2 == 1)) l.negate_mutable();
            return konst(Rational(l));
        }

        return nan_check(mgr_.unary(n->type, child), false);
    }

    return mgr_.unary(n->type, child);
}

auto MathExpressionSimplifier::simplify_fib_lucas_poly(MathExpressionNode* n) -> MathExpression {
    MathExpression arg = simplify_node(n->binary.left);
    MathExpression idx = simplify_node(n->binary.right);
    if (auto p = propagate_poison2(arg, idx)) return p;

    if (is_const(idx.get()) && is_const(arg.get())) {
        const bool is_fib = n->type == NodeType::FibonacciPolynomial;
        std::int64_t m = 0;

        if (rational_int(qv(idx.get()), m) && m >= 0 && m <= polynomial_limit) {
            const Rational x = qv(arg.get());
            Rational prev = is_fib ? Rational(0) : Rational(2);
            Rational cur  = is_fib ? Rational(1) : x;
            if (m == 0) return konst(prev);

            for (std::int64_t i = 1; i < m; ++i) {
                Rational next = x * cur + prev;
                prev = std::move(cur);
                cur  = std::move(next);
            }

            return konst(cur);
        }

        return nan_check(mgr_.binary(n->type, arg, idx), false);
    }

    return mgr_.binary(n->type, arg, idx);
}

auto MathExpressionSimplifier::simplify_applied(MathExpressionNode* n) -> MathExpression {
    const std::uint64_t cnt = n->applied.arg_count;
    std::vector<MathExpression> args; args.reserve(cnt);
    std::vector<std::uint64_t>  ords; ords.reserve(cnt);

    for (std::uint64_t i = 0; i < cnt; ++i) {
        MathExpression a = simplify_node(n->applied.args[i]);
        if (is_poison(a.get())) return MathExpression(a.get());
        args.push_back(a);
        ords.push_back(n->applied.orders[i]);
    }

    return mgr_.applied(n->applied.func_id, args, ords);
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
