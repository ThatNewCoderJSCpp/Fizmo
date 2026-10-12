#include "fizmo_library.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace numeric {

namespace {

namespace fm = ::fizmo::math;
using fm::BigFloat;
using fm::BigFloatContext;

template <class Fn>
double run(Fn&& fn) { return fm::bdetail::eval<double>(std::forward<Fn>(fn)); }

template <class Fn>
double unary(double x, Fn&& fn) {
    const BigFloat a(x);
    return run([&](const BigFloatContext& c) { return fn(a, c); });
}

template <class Fn>
double binary(double x, double y, Fn&& fn) {
    const BigFloat a(x);
    const BigFloat b(y);
    return run([&](const BigFloatContext& c) { return fn(a, b, c); });
}

bool whole(double n) { return std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 9.0e15; }

double nan() { return std::numeric_limits<double>::quiet_NaN(); }

BigFloatContext guarded(const BigFloatContext& c) { return BigFloatContext(c.precision + 32, RoundingMode::nearest_even); }

} // namespace

double erf(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::erf(a, c); }); }
double erfc(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::erfc(a, c); }); }
double erfi(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::erfi(a, c); }); }

double inverse_erf(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::inv_erf(a, c); }); }
double inverse_erfc(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::inv_erfc(a, c); }); }
double inverse_erfi(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::inv_erfi(a, c); }); }

double erf_difference(double a, double b) {
    return binary(a, b, [](const BigFloat& x, const BigFloat& y, const BigFloatContext& c) { return fm::generalized_erf(x, y, c); });
}

double erfc_difference(double a, double b) {
    return binary(a, b, [](const BigFloat& x, const BigFloat& y, const BigFloatContext& c) { return fm::generalized_erfc(x, y, c); });
}

double gamma(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::gamma(a, c); }); }

double factorial(double x) {
    return unary(x, [](const BigFloat& a, const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, RoundingMode::nearest_even);
        return fm::gamma(BigFloat::add(a, BigFloat::one(), exact), c);
    });
}

double digamma(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::digamma(a, c); }); }
double trigamma(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::trigamma(a, c); }); }

double beta(double x, double y) {
    return binary(x, y, [](const BigFloat& a, const BigFloat& b, const BigFloatContext& c) { return fm::beta(a, b, c); });
}

double polygamma(double x, double n) {
    if (!whole(n) || std::fabs(n) > 1.0e6) return nan();
    const int k = static_cast<int>(n);
    return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::polygamma(a, k, c); });
}

double lambert_w(double x, double branch) {
    if (!whole(branch)) return nan();
    const std::int64_t k = static_cast<std::int64_t>(branch);
    return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::lambert_w(a, k, c); });
}

double chebyshev_t(double x, double n) {
    if (whole(n)) {
        const std::int64_t k = static_cast<std::int64_t>(n);
        return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::chebyshev_t(a, k, c); });
    }
    if (!(std::fabs(x) <= 1.0)) return nan();
    return binary(x, n, [](const BigFloat& a, const BigFloat& v, const BigFloatContext& c) {
        const BigFloatContext g = guarded(c);
        return fm::cos(BigFloat::mul(v, fm::arccos(a, g), g), g).rounded(c);
    });
}

double chebyshev_u(double x, double n) {
    if (whole(n)) {
        const std::int64_t k = static_cast<std::int64_t>(n);
        return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::chebyshev_u(a, k, c); });
    }
    if (!(std::fabs(x) < 1.0)) return nan();
    return binary(x, n, [](const BigFloat& a, const BigFloat& v, const BigFloatContext& c) {
        const BigFloatContext g     = guarded(c);
        const BigFloat        theta = fm::arccos(a, g);
        const BigFloat        top   = fm::sin(BigFloat::mul(BigFloat::add(v, BigFloat::one(), g), theta, g), g);
        return BigFloat::div(top, fm::sin(theta, g), g).rounded(c);
    });
}

double exp_integral(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::exp_integral(a, c); }); }

double exp_integral(double x, double n) {
    if (whole(n)) {
        const std::int64_t k = static_cast<std::int64_t>(n);
        return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::exp_integral_generalized(a, k, c); });
    }
    if (!(x > 0.0)) return nan();
    return binary(x, n, [](const BigFloat& a, const BigFloat& v, const BigFloatContext& c) {
        const BigFloatContext g = guarded(c);
        const BigFloat        s = BigFloat::sub(BigFloat::one(), v, g);
        return BigFloat::div(fm::upper_incomplete_gamma(s, a, g), fm::pow(a, s, g), g).rounded(c);
    });
}

double log_integral(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::log_integral(a, c); }); }

double log_integral(double x, double n) {
    if (!whole(n)) return nan();
    const std::int64_t k = static_cast<std::int64_t>(n);
    return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::log_integral_generalized(a, k, c); });
}

double sin_integral(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::sin_integral(a, c); }); }
double cos_integral(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::cos_integral(a, c); }); }
double sinh_integral(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::sinh_integral(a, c); }); }
double cosh_integral(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::cosh_integral(a, c); }); }
double fresnel_s(double x)     { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::fresnel_s(a, c); }); }
double fresnel_c(double x)     { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::fresnel_c(a, c); }); }

double riemann_zeta(double s) { return unary(s, [](const BigFloat& a, const BigFloatContext& c) { return fm::riemann_zeta(a, c); }); }
double dilogarithm(double x)  { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::dilogarithm(a, c); }); }
double trilogarithm(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::trilogarithm(a, c); }); }

double polylog(double x, double s) {
    return binary(x, s, [](const BigFloat& z, const BigFloat& v, const BigFloatContext& c) { return fm::polylog(z, v, c); });
}

double spence_function(double x)     { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::spence_function(a, c); }); }
double spence_integral(double x)     { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::spence_integral(a, c); }); }
double rogers_l(double x)            { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::rogers_l(a, c); }); }
double rogers_l_normalized(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::rogers_l_normalized(a, c); }); }

double gudermannian(double x)         { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::gudermannian(a, c); }); }
double inverse_gudermannian(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::inv_gudermannian(a, c); }); }

double fibonacci(double x) { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::fibonacci(a, c); }); }
double lucas(double x)     { return unary(x, [](const BigFloat& a, const BigFloatContext& c) { return fm::lucas(a, c); }); }

double fibonacci_polynomial(double x, double n) {
    if (!whole(n)) return nan();
    const std::int64_t k = static_cast<std::int64_t>(n);
    return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::fibonacci_polynomial(a, k, c); });
}

double lucas_polynomial(double x, double n) {
    if (!whole(n)) return nan();
    const std::int64_t k = static_cast<std::int64_t>(n);
    return unary(x, [k](const BigFloat& a, const BigFloatContext& c) { return fm::lucas_polynomial(a, k, c); });
}

} // namespace numeric
} // namespace cas
} // namespace math
} // namespace fizmo
