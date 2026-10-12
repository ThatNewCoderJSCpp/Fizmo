#ifndef FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP
#define FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP

#include <cstdint>

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

} // namespace numeric
} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_CAS_NUMERIC_FUNCTIONS_HPP
