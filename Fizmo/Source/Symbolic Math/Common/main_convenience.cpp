#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace math {
namespace cas {

Expression NEGATE(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Negate, a.inner()), ctx.manager());
}

Expression ABS(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::AbsoluteValue, a.inner()), ctx.manager());
}

Expression FLOOR(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Floor, a.inner()), ctx.manager());
}

Expression ROUND(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Round, a.inner()), ctx.manager());
}

Expression TRUNC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Truncate, a.inner()), ctx.manager());
}

Expression FRAC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FractionalPart, a.inner()), ctx.manager());
}

Expression INT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::IntegerPart, a.inner()), ctx.manager());
}

Expression UNIT_STEP(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::UnitStep, a.inner()), ctx.manager());
}

Expression EXP(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NaturalExp, a.inner()), ctx.manager());
}

Expression LN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NaturalLog, a.inner()), ctx.manager());
}

Expression NSINC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NormalSinc, a.inner()), ctx.manager());
}

Expression ASIN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsin, a.inner()), ctx.manager());
}

Expression ACOS(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccos, a.inner()), ctx.manager());
}

Expression ATAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctan, a.inner()), ctx.manager());
}

Expression ATAN2(SymbolicContext& ctx, const Expression& y, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctan, ctx.manager().binary(symbols::NodeType::Divide, y.inner(), x.inner())), ctx.manager());
}

Expression ACSC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccsc, a.inner()), ctx.manager());
}

Expression ASEC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsec, a.inner()), ctx.manager());
}

Expression ACOT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccot, a.inner()), ctx.manager());
}

Expression ASINH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsinh, a.inner()), ctx.manager());
}

Expression ACOSH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccosh, a.inner()), ctx.manager());
}

Expression ATANH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctanh, a.inner()), ctx.manager());
}

Expression ACSCH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccsch, a.inner()), ctx.manager());
}

Expression ASECH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsech, a.inner()), ctx.manager());
}

Expression ACOTH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccoth, a.inner()), ctx.manager());
}

Expression INVERSE_ERF(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::InverseErf, a.inner()), ctx.manager());
}

Expression INVERSE_ERFC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::InverseErfc, a.inner()), ctx.manager());
}

Expression INVERSE_ERFI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::InverseErfi, a.inner()), ctx.manager());
}

Expression ADD(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Add, a.inner(), b.inner()), ctx.manager());
}

Expression SUB(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Subtract, a.inner(), b.inner()), ctx.manager());
}

Expression MUL(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Multiply, a.inner(), b.inner()), ctx.manager());
}

Expression DIV(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Divide, a.inner(), b.inner()), ctx.manager());
}

Expression MOD(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Modulo, a.inner(), b.inner()), ctx.manager());
}

Expression LOG(SymbolicContext& ctx, const Expression& arg, const Expression& base) {
    return Expression(ctx.manager().binary(symbols::NodeType::Log, arg.inner(), base.inner()), ctx.manager());
}

Expression ROOT(SymbolicContext& ctx, const Expression& rad, const Expression& deg) {
    return Expression(ctx.manager().binary(symbols::NodeType::Root, rad.inner(), deg.inner()), ctx.manager());
}

Expression GENERALIZED_ERF(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::ErfGeneralized, a.inner(), b.inner()), ctx.manager());
}

Expression GENERALIZED_ERFC(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::ErfcGeneralized, a.inner(), b.inner()), ctx.manager());
}

Expression GAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Gamma, a.inner()), ctx.manager());
}

Expression FACTORIAL(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Factorial, a.inner()), ctx.manager());
}

Expression DIGAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Digamma, a.inner()), ctx.manager());
}

Expression TRIGAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Trigamma, a.inner()), ctx.manager());
}

Expression POLYGAMMA(SymbolicContext& ctx, const Expression& a, int n) {
    return Expression(ctx.manager().binary(symbols::NodeType::Polygamma, a.inner(), ctx.manager().constant(static_cast<double>(n))), ctx.manager());
}

Expression BETA(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::BetaFunction, x.inner(), y.inner()), ctx.manager());
}

Expression LAMBERT_W(SymbolicContext& ctx, const Expression& z, int k) {
    return Expression(
        ctx.manager().binary(symbols::NodeType::LambertW, z.inner(), ctx.manager().constant(static_cast<double>(k))),
        ctx.manager()
    );
}

Expression CHEBYSHEV_U(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::ChebyshevU, x.inner(), y.inner()), ctx.manager());
}

Expression CHEBYSHEV_T(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::ChebyshevT, x.inner(), y.inner()), ctx.manager());
}

Expression EI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::ExponentialIntegral, a.inner()), ctx.manager());
}

Expression LI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::LogarithmicIntegral, a.inner()), ctx.manager());
}

Expression GENERALIZED_EI(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::ExponentialIntegralGeneralized, x.inner(), n.inner()), ctx.manager());
}

Expression GENERALIZED_LI(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::LogarithmicIntegralGeneralized, x.inner(), n.inner()), ctx.manager());
}

Expression BELL(SymbolicContext& ctx, const Expression& x, unsigned int n) {
    if (n == 0) { return ctx.constant(1.0); }
    if (n == 1) { return x; }
    
    return Expression(
        ctx.manager().binary(
            symbols::NodeType::BellPolynomial,
            x.inner(),
            ctx.manager().constant(static_cast<double>(n))
        ),
        ctx.manager()
    );
}

Expression SI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SinIntegral, a.inner()), ctx.manager());
}

Expression CI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::CosIntegral, a.inner()), ctx.manager());
}

Expression SHI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SinhIntegral, a.inner()), ctx.manager());
}

Expression CHI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::CoshIntegral, a.inner()), ctx.manager());
}

Expression FRESNEL_S(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FresnelS, a.inner()), ctx.manager());
}

Expression FRESNEL_C(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FresnelC, a.inner()), ctx.manager());
}

Expression RIEMANN_ZETA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RiemannZeta, a.inner()), ctx.manager());
}

Expression DILOGARITHM(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Dilogarithm, a.inner()), ctx.manager());
}

Expression TRILOGARITHM(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Trilogarithm, a.inner()), ctx.manager());
}

Expression POLYLOGARITHM(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Dilogarithm, a.inner(), b.inner()), ctx.manager());
}

Expression SPENCE_FUNCTION(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SpenceFunction, a.inner()), ctx.manager());
}

Expression SPENCE_INTEGRAL(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SpenceIntegral, a.inner()), ctx.manager());
}

Expression ROGERS_L(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RogersL, a.inner()), ctx.manager());
}

Expression ROGERS_L_R(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RogersLR, a.inner()), ctx.manager());
}

Expression GUDERMANNIAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Gudermannian, a.inner()), ctx.manager());
}

Expression INVERSE_GUDERMANNIAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::InverseGudermannian, a.inner()), ctx.manager());
}

Expression COMBINATION(SymbolicContext& ctx, const Expression& n, const Expression& k) {
    return Expression(ctx.manager().binary(symbols::NodeType::Combination, n.inner(), k.inner()), ctx.manager());
}

Expression PERMUTATION(SymbolicContext& ctx, const Expression& n, const Expression& k) {
    return Expression(ctx.manager().binary(symbols::NodeType::Permutation, n.inner(), k.inner()), ctx.manager());
}

Expression FIBONACCI_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, int n) {
    return Expression(
        ctx.manager().binary(symbols::NodeType::FibonacciPolynomial, x.inner(), ctx.manager().constant(static_cast<double>(n))),
        ctx.manager()
    );
}

Expression LUCAS_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, int n) {
    return Expression(
        ctx.manager().binary(symbols::NodeType::LucasPolynomial, x.inner(), ctx.manager().constant(static_cast<double>(n))),
        ctx.manager()
    );
}

Expression FIBONACCI(SymbolicContext& ctx, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::FibonacciSequence, x.inner()), ctx.manager());
}

Expression LUCAS(SymbolicContext& ctx, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::LucasSequence, x.inner()), ctx.manager());
}

} // namespace cas
} // namespace math
} // namespace fizmo
