#ifndef FIZMO_COMPLEX_MATH_HPP
#define FIZMO_COMPLEX_MATH_HPP

#include "complex.hpp"

#include <vector>

namespace fizmo {
namespace math {

namespace cxfn {

using multiprecision::BigComplex;
using multiprecision::BigComplexContext;
using multiprecision::BigInt;
using scalar::BigFloat;
using scalar::BigFloatContext;

template <class T, class F>
Complex<T> eval(F&& f, bool composite = false) {
    T re, im;
    complex_detail::evaluate<T>(std::forward<F>(f), re, im, composite);
    return Complex<T>(re, im);
}

inline BigFloat pi_times(std::int64_t k, const BigFloatContext& c) {
    return BigFloat::mul(BigFloat(k), multiprecision::constants::pi(scalar::context(c.precision + 8)), c);
}

inline BigComplex shift_real(const BigComplex& p, bool negate, std::int64_t k, const BigComplexContext& c) {
    const BigFloat re = negate ? -p.real() : p.real();
    const BigFloat im = negate ? -p.imaginary() : p.imaginary();
    if (k == 0) return BigComplex(re, im);
    return BigComplex(BigFloat::add(re, pi_times(k, c.real()), c.real()), im);
}

inline BigComplex shift_imaginary(const BigComplex& p, bool negate, std::int64_t k, const BigComplexContext& c) {
    const BigFloat re = negate ? -p.real() : p.real();
    const BigFloat im = negate ? -p.imaginary() : p.imaginary();
    if (k == 0) return BigComplex(re, im);
    return BigComplex(re, BigFloat::add(im, pi_times(k, c.imaginary()), c.imaginary()));
}

inline bool odd(std::int64_t k) noexcept { return (k % 2) != 0; }

inline std::int64_t cos_shift(std::int64_t k) noexcept { return odd(k) ? k + 1 : k; }

inline BigComplexContext widened(const BigComplexContext& c, std::size_t extra) {
    return BigComplexContext(c.real_precision + extra, c.imaginary_precision + extra);
}

} // namespace cxfn

template <class T> T real(const Complex<T>& z) { return z.real(); }
template <class T> T imaginary(const Complex<T>& z) { return z.imaginary(); }
template <class T> T imag(const Complex<T>& z) { return z.imaginary(); }
template <class T> Complex<T> conj(const Complex<T>& z) { return z.conjugate(); }
template <class T> Complex<T> conjugate(const Complex<T>& z) { return z.conjugate(); }
template <class T> T abs(const Complex<T>& z) { return z.magnitude(); }
template <class T> T magnitude(const Complex<T>& z) { return z.magnitude(); }
template <class T> T norm(const Complex<T>& z) { return z.magnitude_squared(); }
template <class T> T arg(const Complex<T>& z, std::int64_t branch = 0) { return z.argument(branch); }
template <class T> T argument(const Complex<T>& z, std::int64_t branch = 0) { return z.argument(branch); }
template <class T> Complex<T> reciprocal(const Complex<T>& z) { return z.reciprocal(); }
template <class T, scalar::if_real<T> = 0> Complex<T> polar(const T& r, const T& theta) { return Complex<T>::polar(r, theta); }

#define FIZMO_CXT_UNARY(NAME)                                                                                      \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& z) {                                                                         \
        const cxfn::BigComplex b = z.to_big();                                                                     \
        return cxfn::eval<T>([&b](const cxfn::BigComplexContext& c) { return math::NAME(b, c); });               \
    }

#define FIZMO_CXT_INDEXED(NAME)                                                                                    \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& z, std::int64_t n) {                                                         \
        const cxfn::BigComplex b = z.to_big();                                                                     \
        return cxfn::eval<T>([&b, n](const cxfn::BigComplexContext& c) { return math::NAME(b, n, c); });          \
    }

#define FIZMO_CXT_BINARY(NAME)                                                                                     \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& x, const Complex<T>& y) {                                                    \
        const cxfn::BigComplex a = x.to_big();                                                                     \
        const cxfn::BigComplex b = y.to_big();                                                                     \
        return cxfn::eval<T>([&a, &b](const cxfn::BigComplexContext& c) { return math::NAME(a, b, c); });        \
    }                                                                                                              \
    template <class T, scalar::if_real<T> = 0>                                                                     \
    Complex<T> NAME(const Complex<T>& x, const T& y) { return NAME(x, Complex<T>(y)); }                            \
    template <class T, scalar::if_real<T> = 0>                                                                     \
    Complex<T> NAME(const T& x, const Complex<T>& y) { return NAME(Complex<T>(x), y); }

#define FIZMO_CXT_TERNARY(NAME)                                                                                    \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& x, const Complex<T>& y, const Complex<T>& z) {                               \
        const cxfn::BigComplex a = x.to_big();                                                                     \
        const cxfn::BigComplex b = y.to_big();                                                                     \
        const cxfn::BigComplex d = z.to_big();                                                                     \
        return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::NAME(a, b, d, c); });          \
    }

FIZMO_CXT_UNARY(exp)
FIZMO_CXT_UNARY(sqrt)
FIZMO_CXT_UNARY(cbrt)
FIZMO_CXT_UNARY(pow2)
FIZMO_CXT_UNARY(pow10)
FIZMO_CXT_UNARY(sin)
FIZMO_CXT_UNARY(cos)
FIZMO_CXT_UNARY(tan)
FIZMO_CXT_UNARY(csc)
FIZMO_CXT_UNARY(sec)
FIZMO_CXT_UNARY(cot)
FIZMO_CXT_UNARY(sinh)
FIZMO_CXT_UNARY(cosh)
FIZMO_CXT_UNARY(tanh)
FIZMO_CXT_UNARY(csch)
FIZMO_CXT_UNARY(sech)
FIZMO_CXT_UNARY(coth)
FIZMO_CXT_UNARY(sinc)
FIZMO_CXT_UNARY(normalized_sinc)
FIZMO_CXT_UNARY(gudermannian)
FIZMO_CXT_UNARY(inv_gudermannian)
FIZMO_CXT_UNARY(fibonacci)
FIZMO_CXT_UNARY(lucas)
FIZMO_CXT_UNARY(gamma)
FIZMO_CXT_UNARY(log_gamma)
FIZMO_CXT_UNARY(log_abs_gamma)
FIZMO_CXT_UNARY(factorial)
FIZMO_CXT_UNARY(digamma)
FIZMO_CXT_UNARY(trigamma)
FIZMO_CXT_UNARY(harmonic)
FIZMO_CXT_UNARY(riemann_zeta)
FIZMO_CXT_UNARY(riemann_chi)
FIZMO_CXT_UNARY(riemann_xi)
FIZMO_CXT_UNARY(dirichlet_eta)
FIZMO_CXT_UNARY(dirichlet_lambda)
FIZMO_CXT_UNARY(dirichlet_beta)
FIZMO_CXT_UNARY(dilogarithm)
FIZMO_CXT_UNARY(trilogarithm)
FIZMO_CXT_UNARY(spence_function)
FIZMO_CXT_UNARY(spence_integral)
FIZMO_CXT_UNARY(rogers_l)
FIZMO_CXT_UNARY(rogers_l_normalized)
FIZMO_CXT_UNARY(erf)
FIZMO_CXT_UNARY(erfc)
FIZMO_CXT_UNARY(erfi)
FIZMO_CXT_UNARY(inv_erf)
FIZMO_CXT_UNARY(inv_erfc)
FIZMO_CXT_UNARY(inv_erfi)
FIZMO_CXT_UNARY(fresnel_c)
FIZMO_CXT_UNARY(fresnel_s)
FIZMO_CXT_UNARY(sin_integral)
FIZMO_CXT_UNARY(cos_integral)
FIZMO_CXT_UNARY(sinh_integral)
FIZMO_CXT_UNARY(cosh_integral)
FIZMO_CXT_UNARY(exp_integral)
FIZMO_CXT_UNARY(log_integral)
FIZMO_CXT_UNARY(elliptic_k)
FIZMO_CXT_UNARY(elliptic_e)
FIZMO_CXT_UNARY(airy_ai)
FIZMO_CXT_UNARY(airy_bi)
FIZMO_CXT_UNARY(scorer_gi)
FIZMO_CXT_UNARY(scorer_hi)
FIZMO_CXT_UNARY(lambert_w0)
FIZMO_CXT_UNARY(lambert_wm1)
FIZMO_CXT_UNARY(clausen_integral)

FIZMO_CXT_INDEXED(chebyshev_t)
FIZMO_CXT_INDEXED(chebyshev_u)
FIZMO_CXT_INDEXED(chebyshev_v)
FIZMO_CXT_INDEXED(chebyshev_w)
FIZMO_CXT_INDEXED(fibonacci_polynomial)
FIZMO_CXT_INDEXED(lucas_polynomial)
FIZMO_CXT_INDEXED(legendre_p)
FIZMO_CXT_INDEXED(legendre_q)
FIZMO_CXT_INDEXED(rising_pochhammer)
FIZMO_CXT_INDEXED(falling_pochhammer)
FIZMO_CXT_INDEXED(combination)
FIZMO_CXT_INDEXED(permutation)
FIZMO_CXT_INDEXED(clausen)
FIZMO_CXT_INDEXED(debye)
FIZMO_CXT_INDEXED(debye_integral)
FIZMO_CXT_INDEXED(debye_tail)
FIZMO_CXT_INDEXED(exp_integral_generalized)
FIZMO_CXT_INDEXED(log_integral_generalized)
FIZMO_CXT_INDEXED(lambert_w)

FIZMO_CXT_BINARY(beta)
FIZMO_CXT_BINARY(log_beta)
FIZMO_CXT_BINARY(log_abs_beta)
FIZMO_CXT_BINARY(rising_pochhammer)
FIZMO_CXT_BINARY(falling_pochhammer)
FIZMO_CXT_BINARY(combination)
FIZMO_CXT_BINARY(permutation)
FIZMO_CXT_BINARY(hurwitz_zeta)
FIZMO_CXT_BINARY(polylog)
FIZMO_CXT_BINARY(legendre_chi)
FIZMO_CXT_BINARY(generalized_erf)
FIZMO_CXT_BINARY(generalized_erfc)
FIZMO_CXT_BINARY(lower_incomplete_gamma)
FIZMO_CXT_BINARY(upper_incomplete_gamma)
FIZMO_CXT_BINARY(regularized_gamma_p)
FIZMO_CXT_BINARY(regularized_gamma_q)
FIZMO_CXT_BINARY(carlson_rc)
FIZMO_CXT_BINARY(elliptic_f)
FIZMO_CXT_BINARY(elliptic_e)
FIZMO_CXT_BINARY(elliptic_pi)
FIZMO_CXT_BINARY(jacobi_am)
FIZMO_CXT_BINARY(jacobi_sn)
FIZMO_CXT_BINARY(jacobi_cn)
FIZMO_CXT_BINARY(jacobi_dn)
FIZMO_CXT_BINARY(jacobi_ns)
FIZMO_CXT_BINARY(jacobi_nc)
FIZMO_CXT_BINARY(jacobi_nd)
FIZMO_CXT_BINARY(jacobi_sc)
FIZMO_CXT_BINARY(jacobi_sd)
FIZMO_CXT_BINARY(jacobi_cs)
FIZMO_CXT_BINARY(jacobi_cd)
FIZMO_CXT_BINARY(jacobi_ds)
FIZMO_CXT_BINARY(jacobi_dc)

FIZMO_CXT_TERNARY(lerch_transcendent)
FIZMO_CXT_TERNARY(carlson_rf)
FIZMO_CXT_TERNARY(carlson_rd)
FIZMO_CXT_TERNARY(carlson_rg)
FIZMO_CXT_TERNARY(elliptic_pi)

template <class T>
Complex<T> carlson_rj(const Complex<T>& x, const Complex<T>& y, const Complex<T>& z, const Complex<T>& p) {
    const cxfn::BigComplex a = x.to_big(), b = y.to_big(), d = z.to_big(), e = p.to_big();
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::carlson_rj(a, b, d, e, c); });
}

template <class T>
Complex<T> polygamma(const Complex<T>& z, int n) {
    const cxfn::BigComplex b = z.to_big();
    return cxfn::eval<T>([&b, n](const cxfn::BigComplexContext& c) { return math::polygamma(b, n, c); });
}

#define FIZMO_CXT_LOG(NAME)                                                                                        \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& z, std::int64_t branch = 0) {                                                \
        const cxfn::BigComplex b = z.to_big();                                                                     \
        return cxfn::eval<T>([&b, branch](const cxfn::BigComplexContext& c) { return math::NAME(b, branch, c); }); \
    }                                                                                                              \
    template <class T, scalar::if_real<T> = 0>                                                                     \
    Complex<T> NAME(const T& x, std::int64_t branch) { return NAME(Complex<T>(x), branch); }

FIZMO_CXT_LOG(ln)
FIZMO_CXT_LOG(log2)
FIZMO_CXT_LOG(log10)

template <class T>
Complex<T> log(const Complex<T>& z, const Complex<T>& base, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();
    const cxfn::BigComplex b = base.to_big();
    if (branch == 0) return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::log(a, b, c); });

    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) {
        const cxfn::BigComplexContext w = cxfn::widened(c, 16);
        return cxfn::BigComplex::div(math::ln(a, branch, w), math::ln(b, branch, w), c);
    }, true);
}

template <class T, scalar::if_real<T> = 0>
Complex<T> log(const Complex<T>& z, const T& base, std::int64_t branch = 0) { return log(z, Complex<T>(base), branch); }

template <class T, scalar::if_real<T> = 0>
Complex<T> log(const T& x, const Complex<T>& base, std::int64_t branch = 0) { return log(Complex<T>(x), base, branch); }

template <class T>
Complex<T> pow(const Complex<T>& z, const Complex<T>& w, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();
    const cxfn::BigComplex b = w.to_big();
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::pow(a, b, branch, c); });
}

template <class T, scalar::if_real<T> = 0>
Complex<T> pow(const Complex<T>& z, const T& y, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();
    const cxfn::BigFloat b(y);
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::pow(a, b, branch, c); });
}

template <class T, scalar::if_real<T> = 0>
Complex<T> pow(const T& x, const Complex<T>& w, std::int64_t branch = 0) {
    const cxfn::BigFloat a(x);
    const cxfn::BigComplex b = w.to_big();
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::pow(a, b, branch, c); });
}

template <class T, class I, typename std::enable_if<std::is_integral<I>::value, int>::type = 0>
Complex<T> pow(const Complex<T>& z, I n) {
    const cxfn::BigComplex a = z.to_big();
    const cxfn::BigInt k(n);
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::pow(a, k, c); });
}

template <class T, class I, typename std::enable_if<std::is_integral<I>::value, int>::type = 0>
Complex<T> nth_root(const Complex<T>& z, I n, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();
    const cxfn::BigInt k(n);
    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::nth_root(a, k, branch, c); });
}

template <class T>
Complex<T> nth_root(const Complex<T>& z, const Complex<T>& w, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();

    if (w.is_real() && scalar::is_finite(w.real())) {
        const cxfn::BigFloat r(w.real());

        if (!r.is_zero() && r.is_integer()) {
            const cxfn::BigInt k(r.significand().shifted_left(static_cast<std::size_t>(r.exponent())), r.signbit());
            return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) { return math::nth_root(a, k, branch, c); });
        }
    }

    const cxfn::BigComplex b = w.to_big();

    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) {
        const cxfn::BigComplexContext x = cxfn::widened(c, 16);
        return math::pow(a, b.reciprocal(x), branch, c);
    }, true);
}

template <class T, scalar::if_real<T> = 0>
Complex<T> nth_root(const Complex<T>& z, const T& w, std::int64_t branch = 0) { return nth_root(z, Complex<T>(w), branch); }

template <class T>
Complex<T> neg_one_pow(const Complex<T>& z, std::int64_t branch = 0) {
    const cxfn::BigComplex a = z.to_big();

    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) {
        const cxfn::BigComplexContext x = cxfn::widened(c, 16);
        const BigFloat theta = cxfn::pi_times(2 * branch + 1, x.scalar());
        return math::exp(cxfn::BigComplex::mul(a, cxfn::BigComplex(BigFloat::zero(), theta), x), c);
    }, true);
}

#define FIZMO_CXT_INVERSE(NAME, SHIFT, NEGATE_ODD, SHIFT_OF)                                                       \
    template <class T>                                                                                             \
    Complex<T> NAME(const Complex<T>& z, std::int64_t branch = 0) {                                                \
        const cxfn::BigComplex b = z.to_big();                                                                     \
        if (branch == 0) return cxfn::eval<T>([&b](const cxfn::BigComplexContext& c) { return math::NAME(b, c); }); \
        const bool negate = NEGATE_ODD && cxfn::odd(branch);                                                       \
        const std::int64_t k = SHIFT_OF(branch);                                                                   \
        return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) {                                               \
            const cxfn::BigComplexContext x = cxfn::widened(c, 16);                                                \
            return cxfn::SHIFT(math::NAME(b, x), negate, k, x).rounded(c);                                         \
        }, true);                                                                                                  \
    }

#define FIZMO_CXT_SAME(k) (k)

FIZMO_CXT_INVERSE(arcsin,  shift_real,      true,  FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccsc,  shift_real,      true,  FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccos,  shift_real,      true,  cxfn::cos_shift)
FIZMO_CXT_INVERSE(arcsec,  shift_real,      true,  cxfn::cos_shift)
FIZMO_CXT_INVERSE(arctan,  shift_real,      false, FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccot,  shift_real,      false, FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arcsinh, shift_imaginary, true,  FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccsch, shift_imaginary, true,  FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccosh, shift_imaginary, true,  cxfn::cos_shift)
FIZMO_CXT_INVERSE(arcsech, shift_imaginary, true,  cxfn::cos_shift)
FIZMO_CXT_INVERSE(arctanh, shift_imaginary, false, FIZMO_CXT_SAME)
FIZMO_CXT_INVERSE(arccoth, shift_imaginary, false, FIZMO_CXT_SAME)

#undef FIZMO_CXT_SAME
#undef FIZMO_CXT_INVERSE
#undef FIZMO_CXT_LOG
#undef FIZMO_CXT_TERNARY
#undef FIZMO_CXT_BINARY
#undef FIZMO_CXT_INDEXED
#undef FIZMO_CXT_UNARY

template <class T, scalar::if_real<T> = 0>
Complex<T> unity_root(std::size_t n, std::size_t k) {
    if (n == 0) return Complex<T>::nan();
    k %= n;
    if (k == 0) return Complex<T>::one();
    if (2 * k == n) return Complex<T>(T(-1), T(0));
    if (4 * k == n) return Complex<T>(T(0), T(1));
    if (4 * k == 3 * n) return Complex<T>(T(0), T(-1));

    return cxfn::eval<T>([&](const cxfn::BigComplexContext& c) {
        const BigFloatContext x = scalar::context(c.precision() + 16);
        const BigFloat theta = BigFloat::div(cxfn::pi_times(static_cast<std::int64_t>(2 * k), x), BigFloat(static_cast<std::uint64_t>(n)), x);
        return cxfn::BigComplex(math::cos(theta, c.real()), math::sin(theta, c.imaginary()));
    }, true);
}

template <class T, scalar::if_real<T> = 0>
std::vector<Complex<T>> unity_roots(std::size_t n) {
    std::vector<Complex<T>> roots;
    roots.reserve(n);
    for (std::size_t k = 0; k < n; ++k) roots.push_back(unity_root<T>(n, k));
    return roots;
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_COMPLEX_MATH_HPP
