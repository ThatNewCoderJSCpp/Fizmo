#ifndef FIZMO_MATH_CONVENIENCE_HPP
#define FIZMO_MATH_CONVENIENCE_HPP

#include "../CAS/symbolic_context.hpp"
#include "../CAS/expression_wrapper.hpp"

namespace fizmo {
namespace math {
namespace cas {

inline Expression make_expr(SymbolicContext& ctx, const Expression& e) { return e; }
inline Expression make_expr(SymbolicContext& ctx, double v) { return ctx.constant(v); }

// NEGATE
inline Expression NEGATE(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Negate, a.inner()), ctx.manager());
}
inline Expression NEGATE(SymbolicContext& ctx, double a) {
    return NEGATE(ctx, make_expr(ctx, a));
}

// ABS
inline Expression ABS(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::AbsoluteValue, a.inner()), ctx.manager());
}
inline Expression ABS(SymbolicContext& ctx, double a) {
    return ABS(ctx, make_expr(ctx, a));
}

// FLOOR
inline Expression FLOOR(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Floor, a.inner()), ctx.manager());
}
inline Expression FLOOR(SymbolicContext& ctx, double a) {
    return FLOOR(ctx, make_expr(ctx, a));
}

// CEIL
inline Expression CEIL(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Ceil, a.inner()), ctx.manager());
}
inline Expression CEIL(SymbolicContext& ctx, double a) {
    return CEIL(ctx, make_expr(ctx, a));
}

// ROUND
inline Expression ROUND(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Round, a.inner()), ctx.manager());
}
inline Expression ROUND(SymbolicContext& ctx, double a) {
    return ROUND(ctx, make_expr(ctx, a));
}

// TRUNC
inline Expression TRUNC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Truncate, a.inner()), ctx.manager());
}
inline Expression TRUNC(SymbolicContext& ctx, double a) {
    return TRUNC(ctx, make_expr(ctx, a));
}

// FRAC
inline Expression FRAC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FractionalPart, a.inner()), ctx.manager());
}
inline Expression FRAC(SymbolicContext& ctx, double a) {
    return FRAC(ctx, make_expr(ctx, a));
}

// INT
inline Expression INT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::IntegerPart, a.inner()), ctx.manager());
}
inline Expression INT(SymbolicContext& ctx, double a) {
    return INT(ctx, make_expr(ctx, a));
}

// SIGN
inline Expression SIGN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sign, a.inner()), ctx.manager());
}
inline Expression SIGN(SymbolicContext& ctx, double a) { return SIGN(ctx, make_expr(ctx, a)); }

// UNIT_STEP (Heaviside)
inline Expression UNIT_STEP(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::UnitStep, a.inner()), ctx.manager());
}
inline Expression UNIT_STEP(SymbolicContext& ctx, double a) { return UNIT_STEP(ctx, make_expr(ctx, a)); }

// EXP
inline Expression EXP(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NaturalExp, a.inner()), ctx.manager());
}
inline Expression EXP(SymbolicContext& ctx, double a) {
    return EXP(ctx, make_expr(ctx, a));
}

// LN
inline Expression LN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NaturalLog, a.inner()), ctx.manager());
}
inline Expression LN(SymbolicContext& ctx, double a) {
    return LN(ctx, make_expr(ctx, a));
}

// SQRT
inline Expression SQRT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sqrt, a.inner()), ctx.manager());
}
inline Expression SQRT(SymbolicContext& ctx, double a) {
    return SQRT(ctx, make_expr(ctx, a));
}

// CBRT
inline Expression CBRT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Cbrt, a.inner()), ctx.manager());
}
inline Expression CBRT(SymbolicContext& ctx, double a) {
    return CBRT(ctx, make_expr(ctx, a));
}

// SIN
inline Expression SIN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sin, a.inner()), ctx.manager());
}
inline Expression SIN(SymbolicContext& ctx, double a) {
    return SIN(ctx, make_expr(ctx, a));
}

// SINC
inline Expression SINC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sinc, a.inner()), ctx.manager());
}
inline Expression SINC(SymbolicContext& ctx, double a) {
    return SINC(ctx, make_expr(ctx, a));
}

// NSINC (NormalSinc)
inline Expression NSINC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::NormalSinc, a.inner()), ctx.manager());
}
inline Expression NSINC(SymbolicContext& ctx, double a) {
    return NSINC(ctx, make_expr(ctx, a));
}

// COS
inline Expression COS(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Cos, a.inner()), ctx.manager());
}
inline Expression COS(SymbolicContext& ctx, double a) {
    return COS(ctx, make_expr(ctx, a));
}

// TAN
inline Expression TAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Tan, a.inner()), ctx.manager());
}
inline Expression TAN(SymbolicContext& ctx, double a) {
    return TAN(ctx, make_expr(ctx, a));
}

// CSC
inline Expression CSC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Csc, a.inner()), ctx.manager());
}
inline Expression CSC(SymbolicContext& ctx, double a) {
    return CSC(ctx, make_expr(ctx, a));
}

// SEC
inline Expression SEC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sec, a.inner()), ctx.manager());
}
inline Expression SEC(SymbolicContext& ctx, double a) {
    return SEC(ctx, make_expr(ctx, a));
}

// COT
inline Expression COT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Cot, a.inner()), ctx.manager());
}
inline Expression COT(SymbolicContext& ctx, double a) {
    return COT(ctx, make_expr(ctx, a));
}

// ASIN
inline Expression ASIN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsin, a.inner()), ctx.manager());
}
inline Expression ASIN(SymbolicContext& ctx, double a) {
    return ASIN(ctx, make_expr(ctx, a));
}

// ACOS
inline Expression ACOS(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccos, a.inner()), ctx.manager());
}
inline Expression ACOS(SymbolicContext& ctx, double a) {
    return ACOS(ctx, make_expr(ctx, a));
}

// ATAN
inline Expression ATAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctan, a.inner()), ctx.manager());
}
inline Expression ATAN(SymbolicContext& ctx, double a) {
    return ATAN(ctx, make_expr(ctx, a));
}

// ATAN2
inline Expression ATAN2(SymbolicContext& ctx, const Expression& y, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctan, ctx.manager().binary(symbols::NodeType::Divide, y.inner(), x.inner())), ctx.manager());
}
inline Expression ATAN2(SymbolicContext& ctx, const Expression& y, double x) {
    return ATAN2(ctx, y, make_expr(ctx, x));
}
inline Expression ATAN2(SymbolicContext& ctx, double y, const Expression& x) {
    return ATAN2(ctx, make_expr(ctx, y), x);
}
inline Expression ATAN2(SymbolicContext& ctx, double y, double x) {
    return ATAN2(ctx, make_expr(ctx, y), make_expr(ctx, x));
}

// ACSC
inline Expression ACSC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccsc, a.inner()), ctx.manager());
}
inline Expression ACSC(SymbolicContext& ctx, double a) {
    return ACSC(ctx, make_expr(ctx, a));
}

// ASEC
inline Expression ASEC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsec, a.inner()), ctx.manager());
}
inline Expression ASEC(SymbolicContext& ctx, double a) {
    return ASEC(ctx, make_expr(ctx, a));
}

// ACOT
inline Expression ACOT(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccot, a.inner()), ctx.manager());
}
inline Expression ACOT(SymbolicContext& ctx, double a) {
    return ACOT(ctx, make_expr(ctx, a));
}

// SINH
inline Expression SINH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sinh, a.inner()), ctx.manager());
}
inline Expression SINH(SymbolicContext& ctx, double a) {
    return SINH(ctx, make_expr(ctx, a));
}

// COSH
inline Expression COSH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Cosh, a.inner()), ctx.manager());
}
inline Expression COSH(SymbolicContext& ctx, double a) {
    return COSH(ctx, make_expr(ctx, a));
}

// TANH
inline Expression TANH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Tanh, a.inner()), ctx.manager());
}
inline Expression TANH(SymbolicContext& ctx, double a) {
    return TANH(ctx, make_expr(ctx, a));
}

// CSCH
inline Expression CSCH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Csch, a.inner()), ctx.manager());
}
inline Expression CSCH(SymbolicContext& ctx, double a) {
    return CSCH(ctx, make_expr(ctx, a));
}

// SECH
inline Expression SECH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Sech, a.inner()), ctx.manager());
}
inline Expression SECH(SymbolicContext& ctx, double a) {
    return SECH(ctx, make_expr(ctx, a));
}

// COTH
inline Expression COTH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Coth, a.inner()), ctx.manager());
}
inline Expression COTH(SymbolicContext& ctx, double a) {
    return COTH(ctx, make_expr(ctx, a));
}

// ASINH
inline Expression ASINH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsinh, a.inner()), ctx.manager());
}
inline Expression ASINH(SymbolicContext& ctx, double a) {
    return ASINH(ctx, make_expr(ctx, a));
}

// ACOSH
inline Expression ACOSH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccosh, a.inner()), ctx.manager());
}
inline Expression ACOSH(SymbolicContext& ctx, double a) {
    return ACOSH(ctx, make_expr(ctx, a));
}

// ATANH
inline Expression ATANH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arctanh, a.inner()), ctx.manager());
}
inline Expression ATANH(SymbolicContext& ctx, double a) {
    return ATANH(ctx, make_expr(ctx, a));
}

// ACSCH
inline Expression ACSCH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccsch, a.inner()), ctx.manager());
}
inline Expression ACSCH(SymbolicContext& ctx, double a) {
    return ACSCH(ctx, make_expr(ctx, a));
}

// ASECH
inline Expression ASECH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arcsech, a.inner()), ctx.manager());
}
inline Expression ASECH(SymbolicContext& ctx, double a) {
    return ASECH(ctx, make_expr(ctx, a));
}

// ACOTH
inline Expression ACOTH(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Arccoth, a.inner()), ctx.manager());
}
inline Expression ACOTH(SymbolicContext& ctx, double a) {
    return ACOTH(ctx, make_expr(ctx, a));
}

// ERF
inline Expression ERF(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Erf, a.inner()), ctx.manager());
}
inline Expression ERF(SymbolicContext& ctx, double a) {
    return ERF(ctx, make_expr(ctx, a));
}

// ERFC
inline Expression ERFC(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Erfc, a.inner()), ctx.manager());
}
inline Expression ERFC(SymbolicContext& ctx, double a) {
    return ERFC(ctx, make_expr(ctx, a));
}

// ADD
inline Expression ADD(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Add, a.inner(), b.inner()), ctx.manager());
}
inline Expression ADD(SymbolicContext& ctx, const Expression& a, double b) {
    return ADD(ctx, a, make_expr(ctx, b));
}
inline Expression ADD(SymbolicContext& ctx, double a, const Expression& b) {
    return ADD(ctx, make_expr(ctx, a), b);
}
inline Expression ADD(SymbolicContext& ctx, double a, double b) {
    return ADD(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// SUB
inline Expression SUB(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Subtract, a.inner(), b.inner()), ctx.manager());
}
inline Expression SUB(SymbolicContext& ctx, const Expression& a, double b) {
    return SUB(ctx, a, make_expr(ctx, b));
}
inline Expression SUB(SymbolicContext& ctx, double a, const Expression& b) {
    return SUB(ctx, make_expr(ctx, a), b);
}
inline Expression SUB(SymbolicContext& ctx, double a, double b) {
    return SUB(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// MUL
inline Expression MUL(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Multiply, a.inner(), b.inner()), ctx.manager());
}
inline Expression MUL(SymbolicContext& ctx, const Expression& a, double b) {
    return MUL(ctx, a, make_expr(ctx, b));
}
inline Expression MUL(SymbolicContext& ctx, double a, const Expression& b) {
    return MUL(ctx, make_expr(ctx, a), b);
}
inline Expression MUL(SymbolicContext& ctx, double a, double b) {
    return MUL(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// DIV
inline Expression DIV(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Divide, a.inner(), b.inner()), ctx.manager());
}
inline Expression DIV(SymbolicContext& ctx, const Expression& a, double b) {
    return DIV(ctx, a, make_expr(ctx, b));
}
inline Expression DIV(SymbolicContext& ctx, double a, const Expression& b) {
    return DIV(ctx, make_expr(ctx, a), b);
}
inline Expression DIV(SymbolicContext& ctx, double a, double b) {
    return DIV(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// MOD
inline Expression MOD(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Modulo, a.inner(), b.inner()), ctx.manager());
}
inline Expression MOD(SymbolicContext& ctx, const Expression& a, double b) {
    return MOD(ctx, a, make_expr(ctx, b));
}
inline Expression MOD(SymbolicContext& ctx, double a, const Expression& b) {
    return MOD(ctx, make_expr(ctx, a), b);
}
inline Expression MOD(SymbolicContext& ctx, double a, double b) {
    return MOD(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// POW
inline Expression POW(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().power(a.inner(), b.inner()), ctx.manager());
}
inline Expression POW(SymbolicContext& ctx, const Expression& a, double b) {
    return POW(ctx, a, make_expr(ctx, b));
}
inline Expression POW(SymbolicContext& ctx, double a, const Expression& b) {
    return POW(ctx, make_expr(ctx, a), b);
}
inline Expression POW(SymbolicContext& ctx, double a, double b) {
    return POW(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// LOG(arg, base)
inline Expression LOG(SymbolicContext& ctx, const Expression& arg, const Expression& base) {
    return Expression(ctx.manager().binary(symbols::NodeType::Log, arg.inner(), base.inner()), ctx.manager());
}
inline Expression LOG(SymbolicContext& ctx, const Expression& arg, double base) {
    return LOG(ctx, arg, make_expr(ctx, base));
}
inline Expression LOG(SymbolicContext& ctx, double arg, const Expression& base) {
    return LOG(ctx, make_expr(ctx, arg), base);
}
inline Expression LOG(SymbolicContext& ctx, double arg, double base) {
    return LOG(ctx, make_expr(ctx, arg), make_expr(ctx, base));
}

// ROOT(radicand, degree)  (matches symbols::NodeType::Root: left=radicand, right=degree)
inline Expression ROOT(SymbolicContext& ctx, const Expression& rad, const Expression& deg) {
    return Expression(ctx.manager().binary(symbols::NodeType::Root, rad.inner(), deg.inner()), ctx.manager());
}
inline Expression ROOT(SymbolicContext& ctx, const Expression& rad, double deg) {
    return ROOT(ctx, rad, make_expr(ctx, deg));
}
inline Expression ROOT(SymbolicContext& ctx, double rad, const Expression& deg) {
    return ROOT(ctx, make_expr(ctx, rad), deg);
}
inline Expression ROOT(SymbolicContext& ctx, double rad, double deg) {
    return ROOT(ctx, make_expr(ctx, rad), make_expr(ctx, deg));
}

// GENERALIZED_ERFERALIZED(a, b)
inline Expression GENERALIZED_ERF(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::ErfGeneralized, a.inner(), b.inner()), ctx.manager());
}
inline Expression GENERALIZED_ERF(SymbolicContext& ctx, const Expression& a, double b) {
    return GENERALIZED_ERF(ctx, a, make_expr(ctx, b));
}
inline Expression GENERALIZED_ERF(SymbolicContext& ctx, double a, const Expression& b) {
    return GENERALIZED_ERF(ctx, make_expr(ctx, a), b);
}
inline Expression GENERALIZED_ERF(SymbolicContext& ctx, double a, double b) {
    return GENERALIZED_ERF(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// GENERALIZED_ERFCERALIZED(a, b)
inline Expression GENERALIZED_ERFC(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::ErfcGeneralized, a.inner(), b.inner()), ctx.manager());
}
inline Expression GENERALIZED_ERFC(SymbolicContext& ctx, const Expression& a, double b) {
    return GENERALIZED_ERFC(ctx, a, make_expr(ctx, b));
}
inline Expression GENERALIZED_ERFC(SymbolicContext& ctx, double a, const Expression& b) {
    return GENERALIZED_ERFC(ctx, make_expr(ctx, a), b);
}
inline Expression GENERALIZED_ERFC(SymbolicContext& ctx, double a, double b) {
    return GENERALIZED_ERFC(ctx, make_expr(ctx, a), make_expr(ctx, b));
}

// Gamma
inline Expression GAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Gamma, a.inner()), ctx.manager());
}
inline Expression GAMMA(SymbolicContext& ctx, double a) { return GAMMA(ctx, make_expr(ctx, a)); }

// Factorial
inline Expression FACTORIAL(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Factorial, a.inner()), ctx.manager());
}
inline Expression FACTORIAL(SymbolicContext& ctx, double a) { return FACTORIAL(ctx, make_expr(ctx, a)); }

// Digamma
inline Expression DIGAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Digamma, a.inner()), ctx.manager());
}
inline Expression DIGAMMA(SymbolicContext& ctx, double a) { return DIGAMMA(ctx, make_expr(ctx, a)); }

// Trigamma
inline Expression TRIGAMMA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Trigamma, a.inner()), ctx.manager());
}
inline Expression TRIGAMMA(SymbolicContext& ctx, double a) { return TRIGAMMA(ctx, make_expr(ctx, a)); }

// Polygamma
inline Expression POLYGAMMA(SymbolicContext& ctx, const Expression& a, int n) {
    return Expression(ctx.manager().binary(symbols::NodeType::Polygamma, a.inner(), ctx.manager().constant(static_cast<double>(n))), ctx.manager());
}
inline Expression POLYGAMMA(SymbolicContext& ctx, double a, int n) { return POLYGAMMA(ctx, make_expr(ctx, a), n); }

// Beta function
inline Expression BETA(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::BetaFunction, x.inner(), y.inner()), ctx.manager());
}
inline Expression BETA(SymbolicContext& ctx, const Expression& x, double y) {
    return BETA(ctx, x, make_expr(ctx, y));
}
inline Expression BETA(SymbolicContext& ctx, double x, const Expression& y) {
    return BETA(ctx, make_expr(ctx, x), y);
}
inline Expression BETA(SymbolicContext& ctx, double x, double y) {
    return BETA(ctx, make_expr(ctx, x), make_expr(ctx, y));
}

// LAMBERT_W
inline Expression LAMBERT_W(SymbolicContext& ctx, const Expression& z, int k = 0) {
    return Expression(
        ctx.manager().binary(symbols::NodeType::LambertW, z.inner(), ctx.manager().constant(static_cast<double>(k))),
        ctx.manager()
    );
}
inline Expression LAMBERT_W(SymbolicContext& ctx, double z, int k = 0) { 
    return LAMBERT_W(ctx, make_expr(ctx, z), k); 
}

// Chebyshev functions
inline Expression CHEBYSHEV_U(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::ChebyshevU, x.inner(), y.inner()), ctx.manager());
}
inline Expression CHEBYSHEV_U(SymbolicContext& ctx, const Expression& x, double y) {
    return CHEBYSHEV_U(ctx, x, make_expr(ctx, y));
}
inline Expression CHEBYSHEV_U(SymbolicContext& ctx, double x, const Expression& y) {
    return CHEBYSHEV_U(ctx, make_expr(ctx, x), y);
}
inline Expression CHEBYSHEV_U(SymbolicContext& ctx, double x, double y) {
    return CHEBYSHEV_U(ctx, make_expr(ctx, x), make_expr(ctx, y));
}

inline Expression CHEBYSHEV_T(SymbolicContext& ctx, const Expression& x, const Expression& y) {
    return Expression(ctx.manager().binary(symbols::NodeType::ChebyshevT, x.inner(), y.inner()), ctx.manager());
}
inline Expression CHEBYSHEV_T(SymbolicContext& ctx, const Expression& x, double y) {
    return CHEBYSHEV_T(ctx, x, make_expr(ctx, y));
}
inline Expression CHEBYSHEV_T(SymbolicContext& ctx, double x, const Expression& y) {
    return CHEBYSHEV_T(ctx, make_expr(ctx, x), y);
}
inline Expression CHEBYSHEV_T(SymbolicContext& ctx, double x, double y) {
    return CHEBYSHEV_T(ctx, make_expr(ctx, x), make_expr(ctx, y));
}

// EI (Exponential Integral)
inline Expression EI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::ExponentialIntegral, a.inner()), ctx.manager());
}
inline Expression EI(SymbolicContext& ctx, double a) { return EI(ctx, make_expr(ctx, a)); }

// LI (Logarithmic Integral)
inline Expression LI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::LogarithmicIntegral, a.inner()), ctx.manager());
}
inline Expression LI(SymbolicContext& ctx, double a) { return LI(ctx, make_expr(ctx, a)); }

// GENERALIZED_EI(x, n)  -> E_n(x)
inline Expression GENERALIZED_EI(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::ExponentialIntegralGeneralized, x.inner(), n.inner()), ctx.manager());
}
inline Expression GENERALIZED_EI(SymbolicContext& ctx, const Expression& x, double n) { return GENERALIZED_EI(ctx, x, make_expr(ctx, n)); }
inline Expression GENERALIZED_EI(SymbolicContext& ctx, double x, const Expression& n) { return GENERALIZED_EI(ctx, make_expr(ctx, x), n); }
inline Expression GENERALIZED_EI(SymbolicContext& ctx, double x, double n)            { return GENERALIZED_EI(ctx, make_expr(ctx, x), make_expr(ctx, n)); }

// GENERALIZED_LI(x, n)
inline Expression GENERALIZED_LI(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::LogarithmicIntegralGeneralized, x.inner(), n.inner()), ctx.manager());
}
inline Expression GENERALIZED_LI(SymbolicContext& ctx, const Expression& x, double n) { return GENERALIZED_LI(ctx, x, make_expr(ctx, n)); }
inline Expression GENERALIZED_LI(SymbolicContext& ctx, double x, const Expression& n) { return GENERALIZED_LI(ctx, make_expr(ctx, x), n); }
inline Expression GENERALIZED_LI(SymbolicContext& ctx, double x, double n)            { return GENERALIZED_LI(ctx, make_expr(ctx, x), make_expr(ctx, n)); }

// Bell polynomial
inline Expression BELL(SymbolicContext& ctx, const Expression& x, unsigned int n) {
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

inline Expression BELL(SymbolicContext& ctx, double x, unsigned int n) { return BELL(ctx, make_expr(ctx, x), n); }

// SI (Sin Integral)
inline Expression SI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SinIntegral, a.inner()), ctx.manager());
}
inline Expression SI(SymbolicContext& ctx, double a) { return SI(ctx, make_expr(ctx, a)); }

// CI (Cos Integral)
inline Expression CI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::CosIntegral, a.inner()), ctx.manager());
}
inline Expression CI(SymbolicContext& ctx, double a) { return CI(ctx, make_expr(ctx, a)); }

// SHI (Sinh Integral)
inline Expression SHI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SinhIntegral, a.inner()), ctx.manager());
}
inline Expression SHI(SymbolicContext& ctx, double a) { return SHI(ctx, make_expr(ctx, a)); }

// CHI (Cosh Integral)
inline Expression CHI(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::CoshIntegral, a.inner()), ctx.manager());
}
inline Expression CHI(SymbolicContext& ctx, double a) { return CHI(ctx, make_expr(ctx, a)); }

// Fresnel S
inline Expression FRESNEL_S(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FresnelS, a.inner()), ctx.manager());
}
inline Expression FRESNEL_S(SymbolicContext& ctx, double a) { return FRESNEL_S(ctx, make_expr(ctx, a)); }

// Fresnel S
inline Expression FRESNEL_C(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::FresnelC, a.inner()), ctx.manager());
}
inline Expression FRESNEL_C(SymbolicContext& ctx, double a) { return FRESNEL_C(ctx, make_expr(ctx, a)); }

// Riemann 
inline Expression RIEMANN_ZETA(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RiemannZeta, a.inner()), ctx.manager());
}
inline Expression RIEMANN_ZETA(SymbolicContext& ctx, double a) { return RIEMANN_ZETA(ctx, make_expr(ctx, a)); }

// Logs
inline Expression DILOGARITHM(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Dilogarithm, a.inner()), ctx.manager());
}
inline Expression DILOGARITHM(SymbolicContext& ctx, double a) { return DILOGARITHM(ctx, make_expr(ctx, a)); }

inline Expression TRILOGARITHM(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Trilogarithm, a.inner()), ctx.manager());
}
inline Expression TRILOGARITHM(SymbolicContext& ctx, double a) { return TRILOGARITHM(ctx, make_expr(ctx, a)); }

inline Expression POLYLOGARITHM(SymbolicContext& ctx, const Expression& a, const Expression& b) {
    return Expression(ctx.manager().binary(symbols::NodeType::Dilogarithm, a.inner(), b.inner()), ctx.manager());
}
inline Expression POLYLOGARITHM(SymbolicContext& ctx, double a, const Expression& b) { return POLYLOGARITHM(ctx, make_expr(ctx, a), b); }
inline Expression POLYLOGARITHM(SymbolicContext& ctx, const Expression& a, double b) { return POLYLOGARITHM(ctx, a, make_expr(ctx, b)); }
inline Expression POLYLOGARITHM(SymbolicContext& ctx, double a, double b)            { return POLYLOGARITHM(ctx, make_expr(ctx, a), make_expr(ctx, b)); }

// Spence
inline Expression SPENCE_FUNCTION(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SpenceFunction, a.inner()), ctx.manager());
}
inline Expression SPENCE_FUNCTION(SymbolicContext& ctx, double a) { return SPENCE_FUNCTION(ctx, make_expr(ctx, a)); }

inline Expression SPENCE_INTEGRAL(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::SpenceIntegral, a.inner()), ctx.manager());
}
inline Expression SPENCE_INTEGRAL(SymbolicContext& ctx, double a) { return SPENCE_INTEGRAL(ctx, make_expr(ctx, a)); }

// Rogers
inline Expression ROGERS_L(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RogersL, a.inner()), ctx.manager());
}
inline Expression ROGERS_L(SymbolicContext& ctx, double a) { return ROGERS_L(ctx, make_expr(ctx, a)); }

inline Expression ROGERS_L_R(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::RogersLR, a.inner()), ctx.manager());
}
inline Expression ROGERS_L_R(SymbolicContext& ctx, double a) { return ROGERS_L_R(ctx, make_expr(ctx, a)); }

// Rogers
inline Expression GUDERMANNIAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::Gudermannian, a.inner()), ctx.manager());
}
inline Expression GUDERMANNIAN(SymbolicContext& ctx, double a) { return GUDERMANNIAN(ctx, make_expr(ctx, a)); }

inline Expression INVERSE_GUDERMANNIAN(SymbolicContext& ctx, const Expression& a) {
    return Expression(ctx.manager().unary(symbols::NodeType::InverseGudermannian, a.inner()), ctx.manager());
}
inline Expression INVERSE_GUDERMANNIAN(SymbolicContext& ctx, double a) { return INVERSE_GUDERMANNIAN(ctx, make_expr(ctx, a)); }

// COMBINATION  C(n, k)
inline Expression COMBINATION(SymbolicContext& ctx, const Expression& n, const Expression& k) {
    return Expression(ctx.manager().binary(symbols::NodeType::Combination, n.inner(), k.inner()), ctx.manager());
}
inline Expression COMBINATION(SymbolicContext& ctx, const Expression& n, double k) { return COMBINATION(ctx, n, make_expr(ctx, k)); }
inline Expression COMBINATION(SymbolicContext& ctx, double n, const Expression& k) { return COMBINATION(ctx, make_expr(ctx, n), k); }
inline Expression COMBINATION(SymbolicContext& ctx, double n, double k)            { return COMBINATION(ctx, make_expr(ctx, n), make_expr(ctx, k)); }

// PERMUTATION  P(n, k)
inline Expression PERMUTATION(SymbolicContext& ctx, const Expression& n, const Expression& k) {
    return Expression(ctx.manager().binary(symbols::NodeType::Permutation, n.inner(), k.inner()), ctx.manager());
}
inline Expression PERMUTATION(SymbolicContext& ctx, const Expression& n, double k) { return PERMUTATION(ctx, n, make_expr(ctx, k)); }
inline Expression PERMUTATION(SymbolicContext& ctx, double n, const Expression& k) { return PERMUTATION(ctx, make_expr(ctx, n), k); }
inline Expression PERMUTATION(SymbolicContext& ctx, double n, double k)            { return PERMUTATION(ctx, make_expr(ctx, n), make_expr(ctx, k)); }

// Fibonacci Polynomial(x, n)
inline Expression FIBONACCI_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::FibonacciPolynomial, x.inner(), n.inner()), ctx.manager());
}
inline Expression FIBONACCI_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, double n) { return FIBONACCI_POLYNOMIAL(ctx, x, make_expr(ctx, n)); }
inline Expression FIBONACCI_POLYNOMIAL(SymbolicContext& ctx, double x, const Expression& n) { return FIBONACCI_POLYNOMIAL(ctx, make_expr(ctx, x), n); }
inline Expression FIBONACCI_POLYNOMIAL(SymbolicContext& ctx, double x, double n)            { return FIBONACCI_POLYNOMIAL(ctx, make_expr(ctx, x), make_expr(ctx, n)); }

// Lucas Polynomial(x, n)
inline Expression LUCAS_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, const Expression& n) {
    return Expression(ctx.manager().binary(symbols::NodeType::LucasPolynomial, x.inner(), n.inner()), ctx.manager());
}
inline Expression LUCAS_POLYNOMIAL(SymbolicContext& ctx, const Expression& x, double n) { return LUCAS_POLYNOMIAL(ctx, x, make_expr(ctx, n)); }
inline Expression LUCAS_POLYNOMIAL(SymbolicContext& ctx, double x, const Expression& n) { return LUCAS_POLYNOMIAL(ctx, make_expr(ctx, x), n); }
inline Expression LUCAS_POLYNOMIAL(SymbolicContext& ctx, double x, double n)            { return LUCAS_POLYNOMIAL(ctx, make_expr(ctx, x), make_expr(ctx, n)); }

// Fibonacci 
inline Expression FIBONACCI(SymbolicContext& ctx, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::FibonacciSequence, x.inner()), ctx.manager());
}
inline Expression FIBONACCI(SymbolicContext& ctx, double x) { return FIBONACCI(ctx, make_expr(ctx, x)); }

// Lucas 
inline Expression LUCAS(SymbolicContext& ctx, const Expression& x) {
    return Expression(ctx.manager().unary(symbols::NodeType::LucasSequence, x.inner()), ctx.manager());
}
inline Expression LUCAS(SymbolicContext& ctx, double x) { return LUCAS(ctx, make_expr(ctx, x)); }

// ------------------------------------------------------------
// Global context shorthands
// ------------------------------------------------------------

inline SymbolicContext& G() { return global_context(); }

inline Expression NEGATE(const Expression& a)    { return NEGATE(G(), a); }
inline Expression NEGATE(double a)               { return NEGATE(G(), a); }
inline Expression ABS(const Expression& a)       { return ABS(G(), a); }
inline Expression ABS(double a)                  { return ABS(G(), a); }
inline Expression FLOOR(const Expression& a)     { return FLOOR(G(), a); }
inline Expression FLOOR(double a)                { return FLOOR(G(), a); }
inline Expression CEIL(const Expression& a)      { return CEIL(G(), a); }
inline Expression CEIL(double a)                 { return CEIL(G(), a); }
inline Expression ROUND(const Expression& a)     { return ROUND(G(), a); }
inline Expression ROUND(double a)                { return ROUND(G(), a); }
inline Expression TRUNC(const Expression& a)     { return TRUNC(G(), a); }
inline Expression TRUNC(double a)                { return TRUNC(G(), a); }
inline Expression FRAC(const Expression& a)      { return FRAC(G(), a); }
inline Expression FRAC(double a)                 { return FRAC(G(), a); }
inline Expression INT(const Expression& a)       { return INT(G(), a); }
inline Expression INT(double a)                  { return INT(G(), a); }
inline Expression SIGN(const Expression& a)      { return SIGN(G(), a); }
inline Expression SIGN(double a)                 { return SIGN(G(), a); }
inline Expression UNIT_STEP(const Expression& a) { return UNIT_STEP(G(), a); }
inline Expression UNIT_STEP(double a)            { return UNIT_STEP(G(), a); }

inline Expression EXP(const Expression& a) { return EXP(G(), a); }
inline Expression EXP(double a)            { return EXP(G(), a); }
inline Expression LN(const Expression& a)  { return LN(G(), a); }
inline Expression LN(double a)             { return LN(G(), a); }

inline Expression SQRT(const Expression& a) { return SQRT(G(), a); }
inline Expression SQRT(double a)            { return SQRT(G(), a); }
inline Expression CBRT(const Expression& a) { return CBRT(G(), a); }
inline Expression CBRT(double a)            { return CBRT(G(), a); }

inline Expression SIN(const Expression& a)   { return SIN(G(), a); }
inline Expression SIN(double a)              { return SIN(G(), a); }
inline Expression SINC(const Expression& a)  { return SINC(G(), a); }
inline Expression SINC(double a)             { return SINC(G(), a); }
inline Expression NSINC(const Expression& a) { return NSINC(G(), a); }
inline Expression NSINC(double a)            { return NSINC(G(), a); }
inline Expression COS(const Expression& a)   { return COS(G(), a); }
inline Expression COS(double a)              { return COS(G(), a); }
inline Expression TAN(const Expression& a)   { return TAN(G(), a); }
inline Expression TAN(double a)              { return TAN(G(), a); }
inline Expression CSC(const Expression& a)   { return CSC(G(), a); }
inline Expression CSC(double a)              { return CSC(G(), a); }
inline Expression SEC(const Expression& a)   { return SEC(G(), a); }
inline Expression SEC(double a)              { return SEC(G(), a); }
inline Expression COT(const Expression& a)   { return COT(G(), a); }
inline Expression COT(double a)              { return COT(G(), a); }

inline Expression ASIN(const Expression& a) { return ASIN(G(), a); }
inline Expression ASIN(double a)            { return ASIN(G(), a); }
inline Expression ACOS(const Expression& a) { return ACOS(G(), a); }
inline Expression ACOS(double a)            { return ACOS(G(), a); }
inline Expression ATAN(const Expression& a) { return ATAN(G(), a); }
inline Expression ATAN(double a)            { return ATAN(G(), a); }
inline Expression ATAN2(const Expression& y, const Expression& x) { return ATAN2(G(), y, x); }
inline Expression ATAN2(const Expression& y, double x)            { return ATAN2(G(), y, x); }
inline Expression ATAN2(double y, const Expression& x)            { return ATAN2(G(), y, x); }
inline Expression ATAN2(double y, double x)                       { return ATAN2(G(), y, x); }
inline Expression ACSC(const Expression& a) { return ACSC(G(), a); }
inline Expression ACSC(double a)            { return ACSC(G(), a); }
inline Expression ASEC(const Expression& a) { return ASEC(G(), a); }
inline Expression ASEC(double a)            { return ASEC(G(), a); }
inline Expression ACOT(const Expression& a) { return ACOT(G(), a); }
inline Expression ACOT(double a)            { return ACOT(G(), a); }

inline Expression SINH(const Expression& a) { return SINH(G(), a); }
inline Expression SINH(double a)            { return SINH(G(), a); }
inline Expression COSH(const Expression& a) { return COSH(G(), a); }
inline Expression COSH(double a)            { return COSH(G(), a); }
inline Expression TANH(const Expression& a) { return TANH(G(), a); }
inline Expression TANH(double a)            { return TANH(G(), a); }
inline Expression CSCH(const Expression& a) { return CSCH(G(), a); }
inline Expression CSCH(double a)            { return CSCH(G(), a); }
inline Expression SECH(const Expression& a) { return SECH(G(), a); }
inline Expression SECH(double a)            { return SECH(G(), a); }
inline Expression COTH(const Expression& a) { return COTH(G(), a); }
inline Expression COTH(double a)            { return COTH(G(), a); }

inline Expression ASINH(const Expression& a) { return ASINH(G(), a); }
inline Expression ASINH(double a)            { return ASINH(G(), a); }
inline Expression ACOSH(const Expression& a) { return ACOSH(G(), a); }
inline Expression ACOSH(double a)            { return ACOSH(G(), a); }
inline Expression ATANH(const Expression& a) { return ATANH(G(), a); }
inline Expression ATANH(double a)            { return ATANH(G(), a); }
inline Expression ACSCH(const Expression& a) { return ACSCH(G(), a); }
inline Expression ACSCH(double a)            { return ACSCH(G(), a); }
inline Expression ASECH(const Expression& a) { return ASECH(G(), a); }
inline Expression ASECH(double a)            { return ASECH(G(), a); }
inline Expression ACOTH(const Expression& a) { return ACOTH(G(), a); }
inline Expression ACOTH(double a)            { return ACOTH(G(), a); }

inline Expression ERF(const Expression& a)  { return ERF(G(), a); }
inline Expression ERF(double a)             { return ERF(G(), a); }
inline Expression ERFC(const Expression& a) { return ERFC(G(), a); }
inline Expression ERFC(double a)            { return ERFC(G(), a); }

inline Expression ADD(const Expression& a, const Expression& b) { return ADD(G(), a, b); }
inline Expression ADD(const Expression& a, double b)            { return ADD(G(), a, b); }
inline Expression ADD(double a, const Expression& b)            { return ADD(G(), a, b); }
inline Expression ADD(double a, double b)                       { return ADD(G(), a, b); }

inline Expression SUB(const Expression& a, const Expression& b) { return SUB(G(), a, b); }
inline Expression SUB(const Expression& a, double b)            { return SUB(G(), a, b); }
inline Expression SUB(double a, const Expression& b)            { return SUB(G(), a, b); }
inline Expression SUB(double a, double b)                       { return SUB(G(), a, b); }

inline Expression MUL(const Expression& a, const Expression& b) { return MUL(G(), a, b); }
inline Expression MUL(const Expression& a, double b)            { return MUL(G(), a, b); }
inline Expression MUL(double a, const Expression& b)            { return MUL(G(), a, b); }
inline Expression MUL(double a, double b)                       { return MUL(G(), a, b); }

inline Expression DIV(const Expression& a, const Expression& b) { return DIV(G(), a, b); }
inline Expression DIV(const Expression& a, double b)            { return DIV(G(), a, b); }
inline Expression DIV(double a, const Expression& b)            { return DIV(G(), a, b); }
inline Expression DIV(double a, double b)                       { return DIV(G(), a, b); }

inline Expression MOD(const Expression& a, const Expression& b) { return MOD(G(), a, b); }
inline Expression MOD(const Expression& a, double b)            { return MOD(G(), a, b); }
inline Expression MOD(double a, const Expression& b)            { return MOD(G(), a, b); }
inline Expression MOD(double a, double b)                       { return MOD(G(), a, b); }

inline Expression POW(const Expression& a, const Expression& b) { return POW(G(), a, b); }
inline Expression POW(const Expression& a, double b)            { return POW(G(), a, b); }
inline Expression POW(double a, const Expression& b)            { return POW(G(), a, b); }
inline Expression POW(double a, double b)                       { return POW(G(), a, b); }

inline Expression LOG(const Expression& arg, const Expression& base) { return LOG(G(), arg, base); }
inline Expression LOG(const Expression& arg, double base)            { return LOG(G(), arg, base); }
inline Expression LOG(double arg, const Expression& base)            { return LOG(G(), arg, base); }
inline Expression LOG(double arg, double base)                       { return LOG(G(), arg, base); }

inline Expression ROOT(const Expression& rad, const Expression& deg) { return ROOT(G(), rad, deg); }
inline Expression ROOT(const Expression& rad, double deg)            { return ROOT(G(), rad, deg); }
inline Expression ROOT(double rad, const Expression& deg)            { return ROOT(G(), rad, deg); }
inline Expression ROOT(double rad, double deg)                       { return ROOT(G(), rad, deg); }

inline Expression GENERALIZED_ERF(const Expression& a, const Expression& b)  { return GENERALIZED_ERF(G(), a, b); }
inline Expression GENERALIZED_ERF(const Expression& a, double b)             { return GENERALIZED_ERF(G(), a, b); }
inline Expression GENERALIZED_ERF(double a, const Expression& b)             { return GENERALIZED_ERF(G(), a, b); }
inline Expression GENERALIZED_ERF(double a, double b)                        { return GENERALIZED_ERF(G(), a, b); }
inline Expression GENERALIZED_ERFC(const Expression& a, const Expression& b) { return GENERALIZED_ERFC(G(), a, b); }
inline Expression GENERALIZED_ERFC(const Expression& a, double b)            { return GENERALIZED_ERFC(G(), a, b); }
inline Expression GENERALIZED_ERFC(double a, const Expression& b)            { return GENERALIZED_ERFC(G(), a, b); }
inline Expression GENERALIZED_ERFC(double a, double b)                       { return GENERALIZED_ERFC(G(), a, b); }

inline Expression GAMMA(const Expression& a)     { return GAMMA(G(), a); }
inline Expression GAMMA(double a)                { return GAMMA(G(), a); }
inline Expression FACTORIAL(const Expression& a) { return FACTORIAL(G(), a); }
inline Expression FACTORIAL(double a)            { return FACTORIAL(G(), a); }
inline Expression DIGAMMA(const Expression& a)   { return DIGAMMA(G(), a); }
inline Expression DIGAMMA(double a)              { return DIGAMMA(G(), a); }
inline Expression TRIGAMMA(const Expression& a)  { return TRIGAMMA(G(), a); }
inline Expression TRIGAMMA(double a)             { return TRIGAMMA(G(), a); }
inline Expression POLYGAMMA(const Expression& a, int n) { return POLYGAMMA(G(), a, n); }
inline Expression POLYGAMMA(double a, int n)            { return POLYGAMMA(G(), a, n); }

inline Expression BETA(const Expression& x, const Expression& y) { return BETA(G(), x, y); }
inline Expression BETA(const Expression& x, double y)            { return BETA(G(), x, y); }
inline Expression BETA(double x, const Expression& y)            { return BETA(G(), x, y); }
inline Expression BETA(double x, double y)                       { return BETA(G(), x, y); }

inline Expression LAMBERT_W(const Expression& z, int k = 0) { return LAMBERT_W(G(), z, k); }
inline Expression LAMBERT_W(double z, int k = 0)            { return LAMBERT_W(G(), z, k); }

inline Expression CHEBYSHEV_U(const Expression& x, const Expression& y) { return CHEBYSHEV_U(G(), x, y); }
inline Expression CHEBYSHEV_U(const Expression& x, double y)            { return CHEBYSHEV_U(G(), x, y); }
inline Expression CHEBYSHEV_U(double x, const Expression& y)            { return CHEBYSHEV_U(G(), x, y); }
inline Expression CHEBYSHEV_U(double x, double y)                       { return CHEBYSHEV_U(G(), x, y); }
inline Expression CHEBYSHEV_T(const Expression& x, const Expression& y) { return CHEBYSHEV_T(G(), x, y); }
inline Expression CHEBYSHEV_T(const Expression& x, double y)            { return CHEBYSHEV_T(G(), x, y); }
inline Expression CHEBYSHEV_T(double x, const Expression& y)            { return CHEBYSHEV_T(G(), x, y); }
inline Expression CHEBYSHEV_T(double x, double y)                       { return CHEBYSHEV_T(G(), x, y); }

inline Expression EI(const Expression& a) { return EI(G(), a); }
inline Expression EI(double a)            { return EI(G(), a); }
inline Expression LI(const Expression& a) { return LI(G(), a); }
inline Expression LI(double a)            { return LI(G(), a); }
inline Expression GENERALIZED_EI(const Expression& x, const Expression& n) { return GENERALIZED_EI(G(), x, n); }
inline Expression GENERALIZED_EI(const Expression& x, double n)            { return GENERALIZED_EI(G(), x, n); }
inline Expression GENERALIZED_EI(double x, const Expression& n)            { return GENERALIZED_EI(G(), x, n); }
inline Expression GENERALIZED_EI(double x, double n)                       { return GENERALIZED_EI(G(), x, n); }
inline Expression GENERALIZED_LI(const Expression& x, const Expression& n) { return GENERALIZED_LI(G(), x, n); }
inline Expression GENERALIZED_LI(const Expression& x, double n)            { return GENERALIZED_LI(G(), x, n); }
inline Expression GENERALIZED_LI(double x, const Expression& n)            { return GENERALIZED_LI(G(), x, n); }
inline Expression GENERALIZED_LI(double x, double n)                       { return GENERALIZED_LI(G(), x, n); }

inline Expression BELL(const Expression& x, unsigned int n) { return BELL(G(), x, n); }
inline Expression BELL(double x, unsigned int n) { return BELL(G(), x, n); }

inline Expression SI(const Expression& a)  { return SI(G(), a); }
inline Expression SI(double a)             { return SI(G(), a); }
inline Expression CI(const Expression& a)  { return CI(G(), a); }
inline Expression CI(double a)             { return CI(G(), a); }
inline Expression SHI(const Expression& a) { return SHI(G(), a); }
inline Expression SHI(double a)            { return SHI(G(), a); }
inline Expression CHI(const Expression& a) { return CHI(G(), a); }
inline Expression CHI(double a)            { return CHI(G(), a); }

inline Expression FRESNEL_S(const Expression& a)  { return FRESNEL_S(G(), a); }
inline Expression FRESNEL_S(double a)             { return FRESNEL_S(G(), a); }
inline Expression FRESNEL_C(const Expression& a)  { return FRESNEL_C(G(), a); }
inline Expression FRESNEL_C(double a)             { return FRESNEL_C(G(), a); }

inline Expression SPENCE_FUNCTION(const Expression& a) { return SPENCE_FUNCTION(G(), a); }
inline Expression SPENCE_FUNCTION(double a)            { return SPENCE_FUNCTION(G(), a); }
inline Expression SPENCE_INTEGRAL(const Expression& a) { return SPENCE_INTEGRAL(G(), a); }
inline Expression SPENCE_INTEGRAL(double a)            { return SPENCE_INTEGRAL(G(), a); }

inline Expression ROGERS_L(const Expression& a)   { return ROGERS_L(G(), a); }
inline Expression ROGERS_L(double a)              { return ROGERS_L(G(), a); }
inline Expression ROGERS_L_R(const Expression& a) { return ROGERS_L_R(G(), a); }
inline Expression ROGERS_L_R(double a)            { return ROGERS_L_R(G(), a); }

inline Expression GUDERMANNIAN(const Expression& a)         { return GUDERMANNIAN(G(), a); }
inline Expression GUDERMANNIAN(double a)                    { return GUDERMANNIAN(G(), a); }
inline Expression INVERSE_GUDERMANNIAN(const Expression& a) { return INVERSE_GUDERMANNIAN(G(), a); }
inline Expression INVERSE_GUDERMANNIAN(double a)            { return INVERSE_GUDERMANNIAN(G(), a); }

inline Expression COMBINATION(const Expression& n, const Expression& k) { return COMBINATION(G(), n, k); }
inline Expression COMBINATION(const Expression& n, double k)            { return COMBINATION(G(), n, k); }
inline Expression COMBINATION(double n, const Expression& k)            { return COMBINATION(G(), n, k); }
inline Expression COMBINATION(double n, double k)                       { return COMBINATION(G(), n, k); }

inline Expression PERMUTATION(const Expression& n, const Expression& k) { return PERMUTATION(G(), n, k); }
inline Expression PERMUTATION(const Expression& n, double k)            { return PERMUTATION(G(), n, k); }
inline Expression PERMUTATION(double n, const Expression& k)            { return PERMUTATION(G(), n, k); }
inline Expression PERMUTATION(double n, double k)                       { return PERMUTATION(G(), n, k); }

inline Expression FIBONACCI_POLYNOMIAL(const Expression& x, const Expression& n)            { return FIBONACCI_POLYNOMIAL(G(), x, n); }
inline Expression FIBONACCI_POLYNOMIAL(const Expression& x, double n) { return FIBONACCI_POLYNOMIAL(G(), x, n); }
inline Expression FIBONACCI_POLYNOMIAL(double x, const Expression& n) { return FIBONACCI_POLYNOMIAL(G(), x, n); }
inline Expression FIBONACCI_POLYNOMIAL(double x, double n)            { return FIBONACCI_POLYNOMIAL(G(), x, n); }
inline Expression LUCAS_POLYNOMIAL(const Expression& x, const Expression& n)                { return LUCAS_POLYNOMIAL(G(), x, n); }
inline Expression LUCAS_POLYNOMIAL(const Expression& x, double n)     { return LUCAS_POLYNOMIAL(G(), x, n); }
inline Expression LUCAS_POLYNOMIAL(double x, const Expression& n)     { return LUCAS_POLYNOMIAL(G(), x, n); }
inline Expression LUCAS_POLYNOMIAL(double x, double n)                { return LUCAS_POLYNOMIAL(G(), x, n); }

inline Expression FIBONACCI(const Expression& x) { return FIBONACCI(G(), x); }
inline Expression FIBONACCI(double x)            { return FIBONACCI(G(), x); }
inline Expression LUCAS(const Expression& x)     { return LUCAS(G(), x);     }
inline Expression LUCAS(double x)                { return LUCAS(G(), x);     }

inline Expression operator+(const Expression& a, const Expression& b) { return ADD(a, b); }
inline Expression operator+(const Expression& a, double b)            { return ADD(a, b); }
inline Expression operator+(double a, const Expression& b)            { return ADD(a, b); }
inline Expression operator-(const Expression& a, const Expression& b) { return SUB(a, b); }
inline Expression operator-(const Expression& a, double b)            { return SUB(a, b); }
inline Expression operator-(double a, const Expression& b)            { return SUB(a, b); }
inline Expression operator*(const Expression& a, const Expression& b) { return MUL(a, b); }
inline Expression operator*(const Expression& a, double b)            { return MUL(a, b); }
inline Expression operator*(double a, const Expression& b)            { return MUL(a, b); }
inline Expression operator/(const Expression& a, const Expression& b) { return DIV(a, b); }
inline Expression operator/(const Expression& a, double b)            { return DIV(a, b); }
inline Expression operator/(double a, const Expression& b)            { return DIV(a, b); }
inline Expression operator%(const Expression& a, const Expression& b) { return MOD(a, b); }
inline Expression operator%(const Expression& a, double b)            { return MOD(a, b); }
inline Expression operator%(double a, const Expression& b)            { return MOD(a, b); }
inline Expression operator-(const Expression& a)                      { return NEGATE(a); }

// --------------------------------
// Convenience for other stuff
// --------------------------------
inline Expression Const(SymbolicContext& ctx, double v) { return ctx.constant(v); }
inline Expression Const(double v) { return Const(G(), v); }
inline Expression VARIABLE(SymbolicContext& ctx, const std::string& name) { return ctx.variable(name); }
inline Expression VARIABLE(const std::string& name) { return VARIABLE(G(), name); }
inline Expression DIFFERENTIATE(SymbolicContext& ctx, const Expression& e, const std::string& var) { return ctx.derivative(e, var); }
inline Expression DIFFERENTIATE(const Expression& e, const std::string& var) { return DIFFERENTIATE(G(), e, var); }
inline Expression DIFFERENTIATE_ITERATIVE(SymbolicContext& ctx, const Expression& e, const std::string& var, unsigned int order) { return ctx.nth_derivative_iterative(e, var, order); }
inline Expression DIFFERENTIATE_ITERATIVE(const Expression& e, const std::string& var, unsigned order) { return DIFFERENTIATE_ITERATIVE(G(), e, var, order); }
inline Expression DIFFERENTIATE_RECURSIVE(SymbolicContext& ctx, const Expression& e, const std::string& var, unsigned int order) { return ctx.nth_derivative_recursive(e, var, order); }
inline Expression DIFFERENTIATE_RECURSIVE(const Expression& e, const std::string& var, unsigned order) { return DIFFERENTIATE_RECURSIVE(G(), e, var, order); }
inline Expression SIMPLIFY(SymbolicContext& ctx, const Expression& e) { return Expression(ctx.simplifier().simplify(e.inner()), ctx.manager()); }
inline Expression SIMPLIFY(const Expression& e) { return SIMPLIFY(G(), e); }
inline std::string TO_STRING(SymbolicContext& ctx, const Expression& e) { return e.to_string(); }
inline std::string TO_STRING(const Expression& e) { return TO_STRING(G(), e); }
inline void WRITE_OUT(SymbolicContext& ctx, std::ostream& os, const Expression& e) { os << e; }
inline void WRITE_OUT(std::ostream& os, const Expression& e) { WRITE_OUT(G(), os, e); }
inline double EVALUATE(const Expression& e, std::initializer_list<double> vals, EvaluationPolicy policy = {}) { return e.evaluate(vals, policy); }
inline double EVALUATE(const Expression& e, const std::unordered_map<std::string,double>& named, EvaluationPolicy policy = {}) { return e.evaluate(named, policy); }
inline double EVALUATE(const Expression& e, std::initializer_list<std::pair<std::string,double>> vals, EvaluationPolicy policy = {}) { return e.evaluate(vals, policy); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, const std::unordered_map<std::string, Expression>& named) { return ctx.substitute(e, named); }
inline Expression SUBSTITUTE(const Expression& e, const std::unordered_map<std::string, Expression>& named) { return SUBSTITUTE(G(), e, named); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, const std::unordered_map<std::string, double>& named) { return ctx.substitute(e, named); }
inline Expression SUBSTITUTE(const Expression& e, const std::unordered_map<std::string, double>& named) { return SUBSTITUTE(G(), e, named); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) { return ctx.substitute(e, pairs); }
inline Expression SUBSTITUTE(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) { return SUBSTITUTE(G(), e, pairs); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) { return ctx.substitute(e, pairs); }
inline Expression SUBSTITUTE(const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) { return SUBSTITUTE(G(), e, pairs); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, std::initializer_list<Expression> positional) { return ctx.substitute(e, positional); }
inline Expression SUBSTITUTE(const Expression& e, std::initializer_list<Expression> positional) { return SUBSTITUTE(G(), e, positional); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, std::initializer_list<double> positional) { return ctx.substitute(e, positional); }
inline Expression SUBSTITUTE(const Expression& e, std::initializer_list<double> positional) { return SUBSTITUTE(G(), e, positional); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, const std::string& var, const Expression& repl) { return ctx.substitute(e, var, repl); }
inline Expression SUBSTITUTE(const Expression& e, const std::string& var, const Expression& repl) { return SUBSTITUTE(G(), e, var, repl); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, const std::string& var, double val) { return ctx.substitute(e, var, val); }
inline Expression SUBSTITUTE(const Expression& e, const std::string& var, double val) { return SUBSTITUTE(G(), e, var, val); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, const Expression& from, const Expression& to) { return ctx.substitute(e, from, to); }
inline Expression SUBSTITUTE(const Expression& e, const Expression& from, const Expression& to) { return SUBSTITUTE(G(), e, from, to); }
inline Expression SUBSTITUTE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) { return ctx.substitute(e, pairs); }
inline Expression SUBSTITUTE(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) { return SUBSTITUTE(G(), e, pairs); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, const std::string& var, const Expression& repl) { return ctx.partial_evaluate(e, var, repl); }
inline Expression PARTIAL_EVALUATE(const Expression& e, const std::string& var, const Expression& repl) { return PARTIAL_EVALUATE(G(), e, var, repl); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, const std::string& var, double val) { return ctx.partial_evaluate(e, var, val); }
inline Expression PARTIAL_EVALUATE(const Expression& e, const std::string& var, double val) { return PARTIAL_EVALUATE(G(), e, var, val); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, const std::unordered_map<std::string, Expression>& named) { return ctx.partial_evaluate(e, named); }
inline Expression PARTIAL_EVALUATE(const Expression& e, const std::unordered_map<std::string, Expression>& named) { return PARTIAL_EVALUATE(G(), e, named); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, const std::unordered_map<std::string, double>& named) { return ctx.partial_evaluate(e, named); }
inline Expression PARTIAL_EVALUATE(const Expression& e, const std::unordered_map<std::string, double>& named) { return PARTIAL_EVALUATE(G(), e, named); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) { return ctx.partial_evaluate(e, pairs); }
inline Expression PARTIAL_EVALUATE(const Expression& e, std::initializer_list<std::pair<std::string, Expression>> pairs) { return PARTIAL_EVALUATE(G(), e, pairs); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) { return ctx.partial_evaluate(e, pairs); }
inline Expression PARTIAL_EVALUATE(const Expression& e, std::initializer_list<std::pair<std::string, double>> pairs) { return PARTIAL_EVALUATE(G(), e, pairs); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, const Expression& from, const Expression& to) { return ctx.partial_evaluate(e, from, to); }
inline Expression PARTIAL_EVALUATE(const Expression& e, const Expression& from, const Expression& to) { return PARTIAL_EVALUATE(G(), e, from, to); }
inline Expression PARTIAL_EVALUATE(SymbolicContext& ctx, const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) { return ctx.partial_evaluate(e, pairs); }
inline Expression PARTIAL_EVALUATE(const Expression& e, std::initializer_list<std::pair<Expression, Expression>> pairs) { return PARTIAL_EVALUATE(G(), e, pairs); }
inline bool DEPENDS_ON(const Expression& e, const std::string& var) { return e.depends_on(var); }
inline bool DEPENDS_ON(const Expression& e, const Expression& sub ) { return e.depends_on(sub); }
inline bool SYMBOLIC_EQUALS(const Expression& a, const Expression& b) { return a.symbolic_equals(b); }
inline bool IS_SYMBOLICALLY_ZERO(const Expression& e) { return e.is_symbolically_zero(); }
inline Expression REWRITE(SymbolicContext& ctx, const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) { return ctx.rewrite(e, cfg); }
inline Expression REWRITE(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) { return REWRITE(G(), e, cfg); }
inline Expression FULL_SIMPLIFY(SymbolicContext& ctx, const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) { return ctx.full_simplify(e, cfg); }
inline Expression FULL_SIMPLIFY(const Expression& e, const symbols::MathExpressionRewriter::Config& cfg = {}) { return FULL_SIMPLIFY(G(), e, cfg); }
inline Expression INTEGRATE(SymbolicContext& ctx, const Expression& e, const std::string& var) { return ctx.integrate(e, var); }
inline Expression INTEGRATE(const Expression& e, const std::string& var) { return INTEGRATE(G(), e, var); }

constexpr bool can_be_differentiated(symbols::NodeType nt) noexcept {
    return !(
        nt == symbols::NodeType::ExponentialIntegralGeneralized ||
        nt == symbols::NodeType::LogarithmicIntegralGeneralized ||
        nt == symbols::NodeType::BellPolynomial ||
        nt == symbols::NodeType::RiemannZeta ||
        nt == symbols::NodeType::Polylog ||
        nt == symbols::NodeType::FibonacciPolynomial ||
        nt == symbols::NodeType::LucasPolynomial
    );
}

constexpr bool can_be_evaluated(symbols::NodeType nt) noexcept {
    return !(
        nt == symbols::NodeType::BellPolynomial
    );
} 

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_CONVENIENCE_HPP