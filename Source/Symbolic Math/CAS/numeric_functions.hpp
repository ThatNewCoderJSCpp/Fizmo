#ifndef FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP
#define FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP

#include <cstdint>

#include "../../Multiprecision/Big/big_float.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace numeric {

double erf(double x);
double erfc(double x);
double erfi(double x);
double inverse_erf(double x);
double inverse_erfc(double x);
double inverse_erfi(double x);
double erf_difference(double a, double b);
double erfc_difference(double a, double b);

double gamma(double x);
double factorial(double x);
double digamma(double x);
double trigamma(double x);
double beta(double x, double y);
double polygamma(double x, double n);
double lambert_w(double x, double branch);

double chebyshev_t(double x, double n);
double chebyshev_u(double x, double n);

double exp_integral(double x);
double exp_integral(double x, double n);
double log_integral(double x);
double log_integral(double x, double n);
double sin_integral(double x);
double cos_integral(double x);
double sinh_integral(double x);
double cosh_integral(double x);
double fresnel_s(double x);
double fresnel_c(double x);

double riemann_zeta(double s);
double dilogarithm(double x);
double trilogarithm(double x);
double polylog(double x, double s);
double spence_function(double x);
double spence_integral(double x);
double rogers_l(double x);
double rogers_l_normalized(double x);

double gudermannian(double x);
double inverse_gudermannian(double x);

double fibonacci(double x);
double lucas(double x);
double fibonacci_polynomial(double x, double n);
double lucas_polynomial(double x, double n);

BigFloat erf(const BigFloat& x, const BigFloatContext& c);
BigFloat erfc(const BigFloat& x, const BigFloatContext& c);
BigFloat erfi(const BigFloat& x, const BigFloatContext& c);
BigFloat inverse_erf(const BigFloat& x, const BigFloatContext& c);
BigFloat inverse_erfc(const BigFloat& x, const BigFloatContext& c);
BigFloat inverse_erfi(const BigFloat& x, const BigFloatContext& c);
BigFloat erf_difference(const BigFloat& a, const BigFloat& b, const BigFloatContext& c);
BigFloat erfc_difference(const BigFloat& a, const BigFloat& b, const BigFloatContext& c);

BigFloat gamma(const BigFloat& x, const BigFloatContext& c);
BigFloat factorial(const BigFloat& x, const BigFloatContext& c);
BigFloat digamma(const BigFloat& x, const BigFloatContext& c);
BigFloat trigamma(const BigFloat& x, const BigFloatContext& c);
BigFloat beta(const BigFloat& x, const BigFloat& y, const BigFloatContext& c);
BigFloat polygamma(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);
BigFloat lambert_w(const BigFloat& x, const BigFloat& branch, const BigFloatContext& c);

BigFloat chebyshev_t(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);
BigFloat chebyshev_u(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);

BigFloat exp_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat exp_integral(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);
BigFloat log_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat log_integral(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);
BigFloat sin_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat cos_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat sinh_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat cosh_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat fresnel_s(const BigFloat& x, const BigFloatContext& c);
BigFloat fresnel_c(const BigFloat& x, const BigFloatContext& c);

BigFloat riemann_zeta(const BigFloat& s, const BigFloatContext& c);
BigFloat dilogarithm(const BigFloat& x, const BigFloatContext& c);
BigFloat trilogarithm(const BigFloat& x, const BigFloatContext& c);
BigFloat polylog(const BigFloat& x, const BigFloat& s, const BigFloatContext& c);
BigFloat spence_function(const BigFloat& x, const BigFloatContext& c);
BigFloat spence_integral(const BigFloat& x, const BigFloatContext& c);
BigFloat rogers_l(const BigFloat& x, const BigFloatContext& c);
BigFloat rogers_l_normalized(const BigFloat& x, const BigFloatContext& c);

BigFloat gudermannian(const BigFloat& x, const BigFloatContext& c);
BigFloat inverse_gudermannian(const BigFloat& x, const BigFloatContext& c);

BigFloat fibonacci(const BigFloat& x, const BigFloatContext& c);
BigFloat lucas(const BigFloat& x, const BigFloatContext& c);
BigFloat fibonacci_polynomial(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);
BigFloat lucas_polynomial(const BigFloat& x, const BigFloat& n, const BigFloatContext& c);

} // namespace numeric
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP
