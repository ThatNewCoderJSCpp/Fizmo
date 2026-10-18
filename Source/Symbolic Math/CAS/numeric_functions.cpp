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

namespace {

bool whole_big(const BigFloat& n, std::int64_t& k) {
    if (!n.is_finite() || !n.is_integer()) return false;
    const BigInt v = n.get_integer_part();
    if (v.bit_length() > 53) return false;
    const std::int64_t mag = static_cast<std::int64_t>(v.get_lowest_bits());
    k = v.is_negative() ? -mag : mag;
    return true;
}

BigFloat big_nan() { return BigFloat::nan(); }

} // namespace

BigFloat erf(const BigFloat& x, const BigFloatContext& c)          { return fm::erf(x, c); }
BigFloat erfc(const BigFloat& x, const BigFloatContext& c)         { return fm::erfc(x, c); }
BigFloat erfi(const BigFloat& x, const BigFloatContext& c)         { return fm::erfi(x, c); }
BigFloat inverse_erf(const BigFloat& x, const BigFloatContext& c)  { return fm::inv_erf(x, c); }
BigFloat inverse_erfc(const BigFloat& x, const BigFloatContext& c) { return fm::inv_erfc(x, c); }
BigFloat inverse_erfi(const BigFloat& x, const BigFloatContext& c) { return fm::inv_erfi(x, c); }

BigFloat erf_difference(const BigFloat& a, const BigFloat& b, const BigFloatContext& c)  { return fm::generalized_erf(a, b, c); }
BigFloat erfc_difference(const BigFloat& a, const BigFloat& b, const BigFloatContext& c) { return fm::generalized_erfc(a, b, c); }

BigFloat gamma(const BigFloat& x, const BigFloatContext& c) { return fm::gamma(x, c); }

BigFloat factorial(const BigFloat& x, const BigFloatContext& c) {
    const BigFloatContext exact(BigFloatContext::max_prec, RoundingMode::nearest_even);
    return fm::gamma(BigFloat::add(x, BigFloat::one(), exact), c);
}

BigFloat digamma(const BigFloat& x, const BigFloatContext& c)  { return fm::digamma(x, c); }
BigFloat trigamma(const BigFloat& x, const BigFloatContext& c) { return fm::trigamma(x, c); }

BigFloat beta(const BigFloat& x, const BigFloat& y, const BigFloatContext& c) { return fm::beta(x, y, c); }

BigFloat polygamma(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (!whole_big(n, k) || k > 1000000 || k < -1000000) return big_nan();
    return fm::polygamma(x, static_cast<int>(k), c);
}

BigFloat lambert_w(const BigFloat& x, const BigFloat& branch, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (!whole_big(branch, k)) return big_nan();
    return fm::lambert_w(x, k, c);
}

BigFloat chebyshev_t(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (whole_big(n, k)) return fm::chebyshev_t(x, k, c);
    if (!x.is_finite() || x.abs() > BigFloat::one()) return big_nan();
    const BigFloatContext g = guarded(c);
    return fm::cos(BigFloat::mul(n, fm::arccos(x, g), g), g).rounded(c);
}

BigFloat chebyshev_u(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (whole_big(n, k)) return fm::chebyshev_u(x, k, c);
    if (!x.is_finite() || !(x.abs() < BigFloat::one())) return big_nan();
    const BigFloatContext g     = guarded(c);
    const BigFloat        theta = fm::arccos(x, g);
    const BigFloat        top   = fm::sin(BigFloat::mul(BigFloat::add(n, BigFloat::one(), g), theta, g), g);
    return BigFloat::div(top, fm::sin(theta, g), g).rounded(c);
}

BigFloat exp_integral(const BigFloat& x, const BigFloatContext& c) { return fm::exp_integral(x, c); }

BigFloat exp_integral(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (whole_big(n, k)) return fm::exp_integral_generalized(x, k, c);
    if (!(x > BigFloat::zero())) return big_nan();
    const BigFloatContext g = guarded(c);
    const BigFloat        s = BigFloat::sub(BigFloat::one(), n, g);
    return BigFloat::div(fm::upper_incomplete_gamma(s, x, g), fm::pow(x, s, g), g).rounded(c);
}

BigFloat log_integral(const BigFloat& x, const BigFloatContext& c) { return fm::log_integral(x, c); }

BigFloat log_integral(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (!whole_big(n, k)) return big_nan();
    return fm::log_integral_generalized(x, k, c);
}

BigFloat sin_integral(const BigFloat& x, const BigFloatContext& c)  { return fm::sin_integral(x, c); }
BigFloat cos_integral(const BigFloat& x, const BigFloatContext& c)  { return fm::cos_integral(x, c); }
BigFloat sinh_integral(const BigFloat& x, const BigFloatContext& c) { return fm::sinh_integral(x, c); }
BigFloat cosh_integral(const BigFloat& x, const BigFloatContext& c) { return fm::cosh_integral(x, c); }
BigFloat fresnel_s(const BigFloat& x, const BigFloatContext& c)     { return fm::fresnel_s(x, c); }
BigFloat fresnel_c(const BigFloat& x, const BigFloatContext& c)     { return fm::fresnel_c(x, c); }

BigFloat riemann_zeta(const BigFloat& s, const BigFloatContext& c) { return fm::riemann_zeta(s, c); }
BigFloat dilogarithm(const BigFloat& x, const BigFloatContext& c)  { return fm::dilogarithm(x, c); }
BigFloat trilogarithm(const BigFloat& x, const BigFloatContext& c) { return fm::trilogarithm(x, c); }

BigFloat polylog(const BigFloat& x, const BigFloat& s, const BigFloatContext& c) { return fm::polylog(x, s, c); }

BigFloat spence_function(const BigFloat& x, const BigFloatContext& c)     { return fm::spence_function(x, c); }
BigFloat spence_integral(const BigFloat& x, const BigFloatContext& c)     { return fm::spence_integral(x, c); }
BigFloat rogers_l(const BigFloat& x, const BigFloatContext& c)            { return fm::rogers_l(x, c); }
BigFloat rogers_l_normalized(const BigFloat& x, const BigFloatContext& c) { return fm::rogers_l_normalized(x, c); }

BigFloat gudermannian(const BigFloat& x, const BigFloatContext& c)         { return fm::gudermannian(x, c); }
BigFloat inverse_gudermannian(const BigFloat& x, const BigFloatContext& c) { return fm::inv_gudermannian(x, c); }

BigFloat fibonacci(const BigFloat& x, const BigFloatContext& c) { return fm::fibonacci(x, c); }
BigFloat lucas(const BigFloat& x, const BigFloatContext& c)     { return fm::lucas(x, c); }

BigFloat fibonacci_polynomial(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (!whole_big(n, k)) return big_nan();
    return fm::fibonacci_polynomial(x, k, c);
}

BigFloat lucas_polynomial(const BigFloat& x, const BigFloat& n, const BigFloatContext& c) {
    std::int64_t k = 0;
    if (!whole_big(n, k)) return big_nan();
    return fm::lucas_polynomial(x, k, c);
}

} // namespace numeric
} // namespace cas
} // namespace math
} // namespace fizmo
