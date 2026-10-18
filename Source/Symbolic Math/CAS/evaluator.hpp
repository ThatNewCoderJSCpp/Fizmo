#ifndef FIZMO_MATH_EVALUATOR_CLASS_HPP
#define FIZMO_MATH_EVALUATOR_CLASS_HPP

#include "expression.hpp"
#include "../../Basic/constants.hpp"

#include "numeric_functions.hpp"
#include "../../Multiprecision/include.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

double eval_node(const MathExpressionNode* n, const EvalContext& ctx);

inline double evaluate(MathExpression e, const EvalContext& ctx) {
    return eval_node(e.get(), ctx);
}

template <class T, class = void>
struct EvalBackend;

template <>
struct EvalBackend<double> {
    using value_type = double;

    template <class F>
    static double run(F&& f) { return f(); }

    static double from_double(double v) { return v; }
    static double constant(const MathExpressionNode* n) { return n->constant; }
    static double pi()      { return constants::pi(); }
    static double half_pi() { return constants::pi_2(); }
    static double e()       { return constants::euler(); }
    static double nan()     { return std::numeric_limits<double>::quiet_NaN(); }
    static double inf(bool neg) { return neg ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity(); }

    static bool is_nan(double v)         { return std::isnan(v); }
    static bool is_integer(double v)     { return std::isfinite(v) && v == std::floor(v); }
    static bool is_odd_integer(double v) { return is_integer(v) && std::fmod(v, 2.0) != 0.0; }

    static double floor(double v) { return std::floor(v); }
    static double ceil(double v)  { return std::ceil(v); }
    static double round(double v) { return std::round(v); }
    static double trunc(double v) { return std::trunc(v); }
    static double abs(double v)   { return std::abs(v); }
    static double fmod(double a, double b) { return std::fmod(a, b); }
    static double tgamma(double v) { return std::tgamma(v); }

    static double exp(double v)  { return std::exp(v); }
    static double log(double v)  { return std::log(v); }
    static double pow(double a, double b) { return std::pow(a, b); }
    static double sqrt(double v) { return std::sqrt(v); }
    static double cbrt(double v) { return std::cbrt(v); }

    static double sin(double v)  { return std::sin(v); }
    static double cos(double v)  { return std::cos(v); }
    static double tan(double v)  { return std::tan(v); }
    static double asin(double v) { return std::asin(v); }
    static double acos(double v) { return std::acos(v); }
    static double atan(double v) { return std::atan(v); }

    static double sinh(double v)  { return std::sinh(v); }
    static double cosh(double v)  { return std::cosh(v); }
    static double tanh(double v)  { return std::tanh(v); }
    static double asinh(double v) { return std::asinh(v); }
    static double acosh(double v) { return std::acosh(v); }
    static double atanh(double v) { return std::atanh(v); }

    static double erf(double v)          { return numeric::erf(v); }
    static double erfc(double v)         { return numeric::erfc(v); }
    static double erfi(double v)         { return numeric::erfi(v); }
    static double inverse_erf(double v)  { return numeric::inverse_erf(v); }
    static double inverse_erfc(double v) { return numeric::inverse_erfc(v); }
    static double inverse_erfi(double v) { return numeric::inverse_erfi(v); }
    static double erf_difference(double a, double b)  { return numeric::erf_difference(a, b); }
    static double erfc_difference(double a, double b) { return numeric::erfc_difference(a, b); }

    static double gamma(double v)     { return numeric::gamma(v); }
    static double factorial(double v) { return numeric::factorial(v); }
    static double digamma(double v)   { return numeric::digamma(v); }
    static double trigamma(double v)  { return numeric::trigamma(v); }
    static double beta(double a, double b)       { return numeric::beta(a, b); }
    static double polygamma(double x, double n)  { return numeric::polygamma(x, n); }
    static double lambert_w(double x, double k)  { return numeric::lambert_w(x, k); }

    static double chebyshev_t(double x, double n) { return numeric::chebyshev_t(x, n); }
    static double chebyshev_u(double x, double n) { return numeric::chebyshev_u(x, n); }

    static double exp_integral(double x)            { return numeric::exp_integral(x); }
    static double exp_integral(double x, double n)  { return numeric::exp_integral(x, n); }
    static double log_integral(double x)            { return numeric::log_integral(x); }
    static double log_integral(double x, double n)  { return numeric::log_integral(x, n); }
    static double sin_integral(double x)  { return numeric::sin_integral(x); }
    static double cos_integral(double x)  { return numeric::cos_integral(x); }
    static double sinh_integral(double x) { return numeric::sinh_integral(x); }
    static double cosh_integral(double x) { return numeric::cosh_integral(x); }
    static double fresnel_s(double x)     { return numeric::fresnel_s(x); }
    static double fresnel_c(double x)     { return numeric::fresnel_c(x); }

    static double riemann_zeta(double s)      { return numeric::riemann_zeta(s); }
    static double dilogarithm(double x)       { return numeric::dilogarithm(x); }
    static double trilogarithm(double x)      { return numeric::trilogarithm(x); }
    static double polylog(double x, double s) { return numeric::polylog(x, s); }
    static double spence_function(double x)     { return numeric::spence_function(x); }
    static double spence_integral(double x)     { return numeric::spence_integral(x); }
    static double rogers_l(double x)            { return numeric::rogers_l(x); }
    static double rogers_l_normalized(double x) { return numeric::rogers_l_normalized(x); }

    static double gudermannian(double x)         { return numeric::gudermannian(x); }
    static double inverse_gudermannian(double x) { return numeric::inverse_gudermannian(x); }

    static double fibonacci(double x) { return numeric::fibonacci(x); }
    static double lucas(double x)     { return numeric::lucas(x); }
    static double fibonacci_polynomial(double x, double n) { return numeric::fibonacci_polynomial(x, n); }
    static double lucas_polynomial(double x, double n)     { return numeric::lucas_polynomial(x, n); }
};

template <class T, class D>
struct LiftedEvalBackend {
    using value_type = T;
    using BF  = ::fizmo::multiprecision::BigFloat;
    using BFC = ::fizmo::multiprecision::BigFloatContext;

    static T from_rational(const Rational& r) {
        const BF num(r.numerator());
        const BF den(r.denominator());
        return D::lift0([&](const BFC& c) { return BF::div(num, den, c); });
    }

    static T constant(const MathExpressionNode* n) {
        if (n->exact) return from_rational(*n->exact);
        return D::from_double(n->constant);
    }

    static T pi()      { return D::lift0([](const BFC& c) { return ::fizmo::multiprecision::constants::pi(c); }); }
    static T half_pi() { return D::lift0([](const BFC& c) { return ::fizmo::multiprecision::constants::half_pi(c); }); }
    static T e()       { return D::lift0([](const BFC& c) { return ::fizmo::multiprecision::constants::e(c); }); }

    static bool is_integer(const T& v)     { const BF b = D::to_big(v); return b.is_integer(); }
    static bool is_odd_integer(const T& v) { const BF b = D::to_big(v); return b.is_integer() && b.get_integer_part().is_odd(); }

    static T floor(const T& v) { return D::lift1(v, [](const BF& a, const BFC&) { return ::fizmo::math::floor(a); }); }
    static T ceil(const T& v)  { return D::lift1(v, [](const BF& a, const BFC&) { return ::fizmo::math::ceiling(a); }); }
    static T round(const T& v) { return D::lift1(v, [](const BF& a, const BFC&) { return ::fizmo::math::round(a); }); }
    static T trunc(const T& v) { return D::lift1(v, [](const BF& a, const BFC&) { return ::fizmo::math::trunc(a); }); }
    static T abs(const T& v)   { return D::lift1(v, [](const BF& a, const BFC&) { return a.abs(); }); }
    static T fmod(const T& a, const T& b) { return D::lift2(a, b, [](const BF& x, const BF& y, const BFC&) { return ::fizmo::math::fmod(x, y); }); }
    static T tgamma(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::gamma(a, c); }); }

    static T exp(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::exp(a, c); }); }
    static T log(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::ln(a, c); }); }
    static T pow(const T& a, const T& b) { return D::lift2(a, b, [](const BF& x, const BF& y, const BFC& c) { return ::fizmo::math::pow(x, y, c); }); }
    static T sqrt(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::sqrt(a, c); }); }
    static T cbrt(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::cbrt(a, c); }); }

    static T sin(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::sin(a, c); }); }
    static T cos(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::cos(a, c); }); }
    static T tan(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::tan(a, c); }); }
    static T asin(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arcsin(a, c); }); }
    static T acos(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arccos(a, c); }); }
    static T atan(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arctan(a, c); }); }

    static T sinh(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::sinh(a, c); }); }
    static T cosh(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::cosh(a, c); }); }
    static T tanh(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::tanh(a, c); }); }
    static T asinh(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arcsinh(a, c); }); }
    static T acosh(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arccosh(a, c); }); }
    static T atanh(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return ::fizmo::math::arctanh(a, c); }); }

    static T erf(const T& v)          { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::erf(a, c); }); }
    static T erfc(const T& v)         { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::erfc(a, c); }); }
    static T erfi(const T& v)         { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::erfi(a, c); }); }
    static T inverse_erf(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::inverse_erf(a, c); }); }
    static T inverse_erfc(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::inverse_erfc(a, c); }); }
    static T inverse_erfi(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::inverse_erfi(a, c); }); }
    static T erf_difference(const T& a, const T& b)  { return D::lift2(a, b, [](const BF& x, const BF& y, const BFC& c) { return numeric::erf_difference(x, y, c); }); }
    static T erfc_difference(const T& a, const T& b) { return D::lift2(a, b, [](const BF& x, const BF& y, const BFC& c) { return numeric::erfc_difference(x, y, c); }); }

    static T gamma(const T& v)     { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::gamma(a, c); }); }
    static T factorial(const T& v) { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::factorial(a, c); }); }
    static T digamma(const T& v)   { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::digamma(a, c); }); }
    static T trigamma(const T& v)  { return D::lift1(v, [](const BF& a, const BFC& c) { return numeric::trigamma(a, c); }); }
    static T beta(const T& a, const T& b)      { return D::lift2(a, b, [](const BF& x, const BF& y, const BFC& c) { return numeric::beta(x, y, c); }); }
    static T polygamma(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::polygamma(a, b, c); }); }
    static T lambert_w(const T& x, const T& k) { return D::lift2(x, k, [](const BF& a, const BF& b, const BFC& c) { return numeric::lambert_w(a, b, c); }); }

    static T chebyshev_t(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::chebyshev_t(a, b, c); }); }
    static T chebyshev_u(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::chebyshev_u(a, b, c); }); }

    static T exp_integral(const T& x)             { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::exp_integral(a, c); }); }
    static T exp_integral(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::exp_integral(a, b, c); }); }
    static T log_integral(const T& x)             { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::log_integral(a, c); }); }
    static T log_integral(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::log_integral(a, b, c); }); }
    static T sin_integral(const T& x)  { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::sin_integral(a, c); }); }
    static T cos_integral(const T& x)  { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::cos_integral(a, c); }); }
    static T sinh_integral(const T& x) { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::sinh_integral(a, c); }); }
    static T cosh_integral(const T& x) { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::cosh_integral(a, c); }); }
    static T fresnel_s(const T& x)     { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::fresnel_s(a, c); }); }
    static T fresnel_c(const T& x)     { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::fresnel_c(a, c); }); }

    static T riemann_zeta(const T& s)        { return D::lift1(s, [](const BF& a, const BFC& c) { return numeric::riemann_zeta(a, c); }); }
    static T dilogarithm(const T& x)         { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::dilogarithm(a, c); }); }
    static T trilogarithm(const T& x)        { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::trilogarithm(a, c); }); }
    static T polylog(const T& x, const T& s) { return D::lift2(x, s, [](const BF& a, const BF& b, const BFC& c) { return numeric::polylog(a, b, c); }); }
    static T spence_function(const T& x)     { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::spence_function(a, c); }); }
    static T spence_integral(const T& x)     { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::spence_integral(a, c); }); }
    static T rogers_l(const T& x)            { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::rogers_l(a, c); }); }
    static T rogers_l_normalized(const T& x) { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::rogers_l_normalized(a, c); }); }

    static T gudermannian(const T& x)         { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::gudermannian(a, c); }); }
    static T inverse_gudermannian(const T& x) { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::inverse_gudermannian(a, c); }); }

    static T fibonacci(const T& x) { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::fibonacci(a, c); }); }
    static T lucas(const T& x)     { return D::lift1(x, [](const BF& a, const BFC& c) { return numeric::lucas(a, c); }); }
    static T fibonacci_polynomial(const T& x, const T& n) { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::fibonacci_polynomial(a, b, c); }); }
    static T lucas_polynomial(const T& x, const T& n)     { return D::lift2(x, n, [](const BF& a, const BF& b, const BFC& c) { return numeric::lucas_polynomial(a, b, c); }); }
};

template <>
struct EvalBackend<::fizmo::multiprecision::BigFloat> : LiftedEvalBackend<::fizmo::multiprecision::BigFloat, EvalBackend<::fizmo::multiprecision::BigFloat>> {
    using BF  = ::fizmo::multiprecision::BigFloat;
    using BFC = ::fizmo::multiprecision::BigFloatContext;

    static constexpr std::size_t guard_bits = 32;

    template <class F>
    static BF run(F&& f) {
        const BFC outer = BFC::current();
        BF result;

        {
            ::fizmo::multiprecision::ScopedContext scope(outer.extended(guard_bits));
            result = f();
        }

        return result.rounded(outer);
    }

    static BF from_double(double v) { return BF(v); }
    static BF nan() { return BF::nan(); }
    static BF inf(bool neg) { return BF::infinity(neg); }
    static bool is_nan(const BF& v) { return v.is_unordered(); }
    static const BF& to_big(const BF& v) { return v; }

    template <class F>
    static BF lift0(F&& f) { return f(BFC::current()); }

    template <class F>
    static BF lift1(const BF& x, F&& f) { return f(x, BFC::current()); }

    template <class F>
    static BF lift2(const BF& x, const BF& y, F&& f) { return f(x, y, BFC::current()); }
};

template <class F>
struct EvalBackend<F, typename std::enable_if<::fizmo::math::bdetail::is_mp_float<F>::value>::type> : LiftedEvalBackend<F, EvalBackend<F>> {
    using BF  = ::fizmo::multiprecision::BigFloat;
    using BFC = ::fizmo::multiprecision::BigFloatContext;

    template <class G>
    static F run(G&& g) { return g(); }

    static F from_double(double v) { return F(v); }
    static F nan() { return F::nan(); }
    static F inf(bool neg) { return F::infinity(neg); }
    static bool is_nan(const F& v) { return v.is_unordered(); }
    static BF to_big(const F& v) { return BF(v); }

    template <class G>
    static F lift0(G&& g) { return ::fizmo::math::bdetail::eval<F>([&](const BFC& c) { return g(c); }); }

    template <class G>
    static F lift1(const F& x, G&& g) {
        const BF a(x);
        return ::fizmo::math::bdetail::eval<F>([&](const BFC& c) { return g(a, c); });
    }

    template <class G>
    static F lift2(const F& x, const F& y, G&& g) {
        const BF a(x);
        const BF b(y);
        return ::fizmo::math::bdetail::eval<F>([&](const BFC& c) { return g(a, b, c); });
    }
};

template <class T, class B = EvalBackend<T>>
T eval_node_as(const MathExpressionNode* n, const BasicEvalContext<T>& ctx) {
    if (is_invalid(n)) return B::nan();
    const T zero = B::from_double(0.0);
    const T one  = B::from_double(1.0);
    auto ev = [&ctx](const MathExpressionNode* c) { return eval_node_as<T, B>(c, ctx); };

    switch (n->type) {
        case NodeType::Constant:    return B::constant(n);
        case NodeType::Pi:          return B::pi();
        case NodeType::EulerNumber: return B::e();

        case NodeType::Variable: {
            const auto id = n->variable.var_id;

            if (id >= ctx.values.size()) {
                switch (ctx.policy.missing_in_table) {
                    case MissingVariablePolicy::UseDefaultValue: return B::from_double(ctx.policy.default_value);
                    case MissingVariablePolicy::ReturnNaN:       return B::nan();
                    case MissingVariablePolicy::ThrowException:  throw std::runtime_error("Variable ID out of range");
                }
            }

            if (!ctx.assigned[id]) {
                switch (ctx.policy.missing_in_input) {
                    case MissingVariablePolicy::UseDefaultValue: return B::from_double(ctx.policy.default_value);
                    case MissingVariablePolicy::ReturnNaN:       return B::nan();
                    case MissingVariablePolicy::ThrowException:  throw std::runtime_error("Variable not assigned");
                }
            }

            return ctx.values[id];
        }

        case NodeType::PositiveInfinity: return B::inf(false);
        case NodeType::NegativeInfinity: return B::inf(true);

        case NodeType::NaN:
        case NodeType::Undefined:
        case NodeType::Indeterminate:
        case NodeType::Invalid:
            return B::nan();

        case NodeType::Add:      return ev(n->binary.left) + ev(n->binary.right);
        case NodeType::Subtract: return ev(n->binary.left) - ev(n->binary.right);
        case NodeType::Multiply: return ev(n->binary.left) * ev(n->binary.right);

        case NodeType::Divide: {
            const T num = ev(n->binary.left);
            const T den = ev(n->binary.right);
            if (den == zero) return B::nan();
            return num / den;
        }

        case NodeType::Modulo: {
            const T a = ev(n->binary.left);
            const T b = ev(n->binary.right);
            if (b == zero) return B::nan();
            return B::fmod(a, b);
        }

        case NodeType::Negate:   return -ev(n->unary.child);
        case NodeType::Floor:    return B::floor(ev(n->unary.child));
        case NodeType::Ceil:     return B::ceil(ev(n->unary.child));
        case NodeType::Round:    return B::round(ev(n->unary.child));
        case NodeType::Truncate: return B::trunc(ev(n->unary.child));

        case NodeType::FractionalPart: {
            const T v = ev(n->unary.child);
            return v - B::trunc(v);
        }

        case NodeType::IntegerPart:   return B::trunc(ev(n->unary.child));
        case NodeType::AbsoluteValue: return B::abs(ev(n->unary.child));

        case NodeType::Sign: {
            const T v = ev(n->unary.child);
            if (B::is_nan(v)) return B::nan();
            if (v > zero) return one;
            if (v < zero) return -one;
            return zero;
        }

        case NodeType::UnitStep: {
            const T v = ev(n->unary.child);
            if (B::is_nan(v)) return B::nan();
            return v >= zero ? one : zero;
        }

        case NodeType::Combination: {
            const T nn = ev(n->binary.left);
            const T kk = ev(n->binary.right);
            if (B::is_nan(nn) || B::is_nan(kk)) return B::nan();
            const T num = B::tgamma(nn + one);
            const T den = B::tgamma(kk + one) * B::tgamma(nn - kk + one);
            if (den == zero) return B::nan();
            return num / den;
        }

        case NodeType::Permutation: {
            const T nn = ev(n->binary.left);
            const T kk = ev(n->binary.right);
            if (B::is_nan(nn) || B::is_nan(kk)) return B::nan();
            const T den = B::tgamma(nn - kk + one);
            if (den == zero) return B::nan();
            return B::tgamma(nn + one) / den;
        }

        case NodeType::NaturalExp: return B::exp(ev(n->unary.child));

        case NodeType::Power: {
            const T base = ev(n->power.base);
            const T exp  = ev(n->power.exponent);
            if (base == zero && exp == zero) return B::nan();
            if (base == zero && exp < zero)  return B::nan();
            return B::pow(base, exp);
        }

        case NodeType::Root: {
            const T radicand = ev(n->binary.left);
            const T degree   = ev(n->binary.right);
            if (degree == zero) return B::nan();
            if (radicand < zero) {
                if (!B::is_integer(degree)) return B::nan();
                if (!B::is_odd_integer(degree)) return B::nan();
                return -B::pow(-radicand, one / degree);
            }
            return B::pow(radicand, one / degree);
        }

        case NodeType::Sqrt: {
            const T v = ev(n->unary.child);
            if (v < zero) return B::nan();
            return B::sqrt(v);
        }

        case NodeType::Cbrt: return B::cbrt(ev(n->unary.child));

        case NodeType::NaturalLog: {
            const T v = ev(n->unary.child);
            if (v <  zero) return B::nan();
            if (v == zero) return B::inf(true);
            return B::log(v);
        }

        case NodeType::Log: {
            const T arg  = ev(n->binary.left);
            const T base = ev(n->binary.right);
            if (base <= zero || base == one) return B::nan();
            if (arg  <  zero) return B::nan();
            if (arg  == zero) return B::inf(true);
            return B::log(arg) / B::log(base);
        }

        case NodeType::Sin: return B::sin(ev(n->unary.child));

        case NodeType::Sinc: {
            const T v = ev(n->unary.child);
            if (v == zero) return one;
            return B::sin(v) / v;
        }

        case NodeType::NormalSinc: {
            const T v = ev(n->unary.child) * B::pi();
            if (v == zero) return one;
            return B::sin(v) / v;
        }

        case NodeType::Cos: return B::cos(ev(n->unary.child));

        case NodeType::Tan: {
            const T v = ev(n->unary.child);
            if (B::cos(v) == zero) return B::nan();
            return B::tan(v);
        }

        case NodeType::Csc: {
            const T s = B::sin(ev(n->unary.child));
            if (s == zero) return B::nan();
            return one / s;
        }

        case NodeType::Sec: {
            const T c = B::cos(ev(n->unary.child));
            if (c == zero) return B::nan();
            return one / c;
        }

        case NodeType::Cot: {
            const T v = ev(n->unary.child);
            const T s = B::sin(v);
            if (s == zero) return B::nan();
            return B::cos(v) / s;
        }

        case NodeType::Arcsin: {
            const T v = ev(n->unary.child);
            if (v < -one || v > one) return B::nan();
            return B::asin(v);
        }

        case NodeType::Arccos: {
            const T v = ev(n->unary.child);
            if (v < -one || v > one) return B::nan();
            return B::acos(v);
        }

        case NodeType::Arctan: return B::atan(ev(n->unary.child));

        case NodeType::Arccsc: {
            const T v = ev(n->unary.child);
            if (v == zero || (v > -one && v < one)) return B::nan();
            return B::asin(one / v);
        }

        case NodeType::Arcsec: {
            const T v = ev(n->unary.child);
            if (v == zero || (v > -one && v < one)) return B::nan();
            return B::acos(one / v);
        }

        case NodeType::Arccot: {
            const T v = ev(n->unary.child);
            if (v == zero) return B::half_pi();
            return B::atan(one / v);
        }

        case NodeType::Sinh: return B::sinh(ev(n->unary.child));
        case NodeType::Cosh: return B::cosh(ev(n->unary.child));
        case NodeType::Tanh: return B::tanh(ev(n->unary.child));

        case NodeType::Csch: {
            const T v = ev(n->unary.child);
            if (v == zero) return B::nan();
            return one / B::sinh(v);
        }

        case NodeType::Sech: return one / B::cosh(ev(n->unary.child));

        case NodeType::Coth: {
            const T v = ev(n->unary.child);
            if (v == zero) return B::nan();
            return one / B::tanh(v);
        }

        case NodeType::Arcsinh: return B::asinh(ev(n->unary.child));

        case NodeType::Arccosh: {
            const T v = ev(n->unary.child);
            if (v < one) return B::nan();
            return B::acosh(v);
        }

        case NodeType::Arctanh: {
            const T v = ev(n->unary.child);
            if (v <= -one || v >= one) return B::nan();
            return B::atanh(v);
        }

        case NodeType::Arccsch: {
            const T v = ev(n->unary.child);
            if (v == zero) return B::nan();
            return B::asinh(one / v);
        }

        case NodeType::Arcsech: {
            const T v = ev(n->unary.child);
            if (v <= zero || v > one) return B::nan();
            return B::acosh(one / v);
        }

        case NodeType::Arccoth: {
            const T v = ev(n->unary.child);
            if (v >= -one && v <= one) return B::nan();
            return B::atanh(one / v);
        }

        case NodeType::Erf:             return B::erf(ev(n->unary.child));
        case NodeType::Erfc:            return B::erfc(ev(n->unary.child));
        case NodeType::ErfGeneralized:  return B::erf_difference(ev(n->binary.left), ev(n->binary.right));
        case NodeType::ErfcGeneralized: return B::erfc_difference(ev(n->binary.left), ev(n->binary.right));
        case NodeType::Erfi:            return B::erfi(ev(n->unary.child));
        case NodeType::InverseErf:      return B::inverse_erf(ev(n->unary.child));
        case NodeType::InverseErfc:     return B::inverse_erfc(ev(n->unary.child));
        case NodeType::InverseErfi:     return B::inverse_erfi(ev(n->unary.child));

        case NodeType::Gamma:        return B::gamma(ev(n->unary.child));
        case NodeType::Factorial:    return B::factorial(ev(n->unary.child));
        case NodeType::Digamma:      return B::digamma(ev(n->unary.child));
        case NodeType::Trigamma:     return B::trigamma(ev(n->unary.child));
        case NodeType::BetaFunction: return B::beta(ev(n->binary.left), ev(n->binary.right));
        case NodeType::Polygamma:    return B::polygamma(ev(n->binary.left), ev(n->binary.right));
        case NodeType::LambertW:     return B::lambert_w(ev(n->binary.left), ev(n->binary.right));

        case NodeType::ChebyshevU: return B::chebyshev_u(ev(n->binary.left), ev(n->binary.right));
        case NodeType::ChebyshevT: return B::chebyshev_t(ev(n->binary.left), ev(n->binary.right));

        case NodeType::ExponentialIntegral:            return B::exp_integral(ev(n->unary.child));
        case NodeType::LogarithmicIntegral:            return B::log_integral(ev(n->unary.child));
        case NodeType::ExponentialIntegralGeneralized: return B::exp_integral(ev(n->binary.left), ev(n->binary.right));
        case NodeType::LogarithmicIntegralGeneralized: return B::log_integral(ev(n->binary.left), ev(n->binary.right));

        case NodeType::SinIntegral:  return B::sin_integral(ev(n->unary.child));
        case NodeType::CosIntegral:  return B::cos_integral(ev(n->unary.child));
        case NodeType::SinhIntegral: return B::sinh_integral(ev(n->unary.child));
        case NodeType::CoshIntegral: return B::cosh_integral(ev(n->unary.child));

        case NodeType::FresnelS: return B::fresnel_s(ev(n->unary.child));
        case NodeType::FresnelC: return B::fresnel_c(ev(n->unary.child));

        case NodeType::RiemannZeta: return B::riemann_zeta(ev(n->unary.child));

        case NodeType::Dilogarithm:  return B::dilogarithm(ev(n->unary.child));
        case NodeType::Trilogarithm: return B::trilogarithm(ev(n->unary.child));
        case NodeType::Polylog:      return B::polylog(ev(n->binary.left), ev(n->binary.right));

        case NodeType::SpenceFunction: return B::spence_function(ev(n->unary.child));
        case NodeType::SpenceIntegral: return B::spence_integral(ev(n->unary.child));

        case NodeType::RogersL:  return B::rogers_l_normalized(ev(n->unary.child));
        case NodeType::RogersLR: return B::rogers_l(ev(n->unary.child));

        case NodeType::Gudermannian:        return B::gudermannian(ev(n->unary.child));
        case NodeType::InverseGudermannian: return B::inverse_gudermannian(ev(n->unary.child));

        case NodeType::FibonacciSequence: return B::fibonacci(ev(n->unary.child));
        case NodeType::LucasSequence:     return B::lucas(ev(n->unary.child));

        case NodeType::FibonacciPolynomial: return B::fibonacci_polynomial(ev(n->binary.left), ev(n->binary.right));
        case NodeType::LucasPolynomial:     return B::lucas_polynomial(ev(n->binary.left), ev(n->binary.right));

        default:
            return B::nan();
    }
}

template <class T>
T evaluate_as(MathExpression e, const BasicEvalContext<T>& ctx) {
    return EvalBackend<T>::run([&]() -> T { return eval_node_as<T>(e.get(), ctx); });
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_EVALUATOR_CLASS_HPP
