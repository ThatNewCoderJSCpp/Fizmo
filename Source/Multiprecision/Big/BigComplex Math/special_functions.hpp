#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_FUNCTIONS_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_FUNCTIONS_HPP

#include "special_detail.hpp"

#include <type_traits>

namespace fizmo {
namespace math {
namespace cxsf {

BC sinc(const BC& z, const BCC& ctx);
BC normalized_sinc(const BC& z, const BCC& ctx);
BC gudermannian(const BC& z, const BCC& ctx);
BC inv_gudermannian(const BC& z, const BCC& ctx);
BC log(const BC& z, const BC& base, const BCC& ctx);
BC pow2(const BC& z, const BCC& ctx);
BC pow10(const BC& z, const BCC& ctx);

BC chebyshev_t(const BC& z, std::int64_t n, const BCC& ctx);
BC chebyshev_u(const BC& z, std::int64_t n, const BCC& ctx);
BC chebyshev_v(const BC& z, std::int64_t n, const BCC& ctx);
BC chebyshev_w(const BC& z, std::int64_t n, const BCC& ctx);
BC fibonacci_polynomial(const BC& z, std::int64_t n, const BCC& ctx);
BC lucas_polynomial(const BC& z, std::int64_t n, const BCC& ctx);
BC legendre_p(const BC& z, std::int64_t n, const BCC& ctx);
BC legendre_q(const BC& z, std::int64_t n, const BCC& ctx);

BC log_gamma(const BC& z, const BCC& ctx);
BC gamma(const BC& z, const BCC& ctx);
BC factorial(const BC& z, const BCC& ctx);
BC beta(const BC& a, const BC& b, const BCC& ctx);
BC log_beta(const BC& a, const BC& b, const BCC& ctx);
BC digamma(const BC& z, const BCC& ctx);
BC trigamma(const BC& z, const BCC& ctx);
BC polygamma(const BC& z, int n, const BCC& ctx);
BC harmonic(const BC& z, const BCC& ctx);
BC rising_pochhammer(const BC& x, std::int64_t n, const BCC& ctx);
BC falling_pochhammer(const BC& x, std::int64_t n, const BCC& ctx);
BC rising_pochhammer(const BC& x, const BC& a, const BCC& ctx);
BC falling_pochhammer(const BC& x, const BC& a, const BCC& ctx);
BC combination(const BC& x, std::int64_t k, const BCC& ctx);
BC combination(const BC& x, const BC& k, const BCC& ctx);
BC permutation(const BC& x, std::int64_t k, const BCC& ctx);
BC permutation(const BC& x, const BC& k, const BCC& ctx);

Approx hurwitz_core(const CX& s, const CX& a, std::size_t w);
std::vector<Approx> zeta_shifted(const CX& s, std::size_t K, std::size_t w);
std::vector<Approx> hurwitz_shifted(const CX& s, const CX& a, std::size_t K, std::size_t w, const std::vector<double>& drop = std::vector<double>());
BC hurwitz_zeta(const BC& s, const BC& a, const BCC& ctx);
BC lerch_transcendent(const BC& z, const BC& s, const BC& a, const BCC& ctx);
BC riemann_zeta(const BC& s, const BCC& ctx);
BC riemann_chi(const BC& s, const BCC& ctx);
BC riemann_xi(const BC& s, const BCC& ctx);
BC dirichlet_eta(const BC& s, const BCC& ctx);
BC dirichlet_lambda(const BC& s, const BCC& ctx);
BC dirichlet_beta(const BC& s, const BCC& ctx);

Approx polylog_core(const CX& s, const CX& z, std::size_t w);
BC polylog(const BC& z, const BC& s, const BCC& ctx);
BC dilogarithm(const BC& z, const BCC& ctx);
BC trilogarithm(const BC& z, const BCC& ctx);
BC spence_function(const BC& z, const BCC& ctx);
BC spence_integral(const BC& z, const BCC& ctx);
BC rogers_l(const BC& z, const BCC& ctx);
BC rogers_l_normalized(const BC& z, const BCC& ctx);
BC legendre_chi(const BC& z, const BC& s, const BCC& ctx);
BC clausen(const BC& theta, std::int64_t s, const BCC& ctx);
BC debye(const BC& x, std::int64_t n, const BCC& ctx);
BC debye_integral(const BC& x, std::int64_t n, const BCC& ctx);
BC debye_tail(const BC& x, std::int64_t n, const BCC& ctx);

BC erf(const BC& z, const BCC& ctx);
BC erfc(const BC& z, const BCC& ctx);
BC erfi(const BC& z, const BCC& ctx);
BC inv_erf(const BC& y, const BCC& ctx);
BC inv_erfc(const BC& c, const BCC& ctx);
BC inv_erfi(const BC& v, const BCC& ctx);
BC generalized_erf(const BC& z, const BC& b, const BCC& ctx);
BC generalized_erfc(const BC& z, const BC& b, const BCC& ctx);
BC fresnel_c(const BC& z, const BCC& ctx);
BC fresnel_s(const BC& z, const BCC& ctx);

BC sin_integral(const BC& z, const BCC& ctx);
BC cos_integral(const BC& z, const BCC& ctx);
BC sinh_integral(const BC& z, const BCC& ctx);
BC cosh_integral(const BC& z, const BCC& ctx);
BC exp_integral(const BC& z, const BCC& ctx);
BC exp_integral_generalized(const BC& z, std::int64_t n, const BCC& ctx);
BC log_integral(const BC& z, const BCC& ctx);
BC log_integral_generalized(const BC& z, std::int64_t n, const BCC& ctx);

BC lower_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx);
BC upper_incomplete_gamma(const BC& a, const BC& z, const BCC& ctx);
BC regularized_gamma_p(const BC& a, const BC& z, const BCC& ctx);
BC regularized_gamma_q(const BC& a, const BC& z, const BCC& ctx);

BC fibonacci(const BC& z, const BCC& ctx);
BC lucas(const BC& z, const BCC& ctx);

BC lambert_w(const BC& z, std::int64_t k, const BCC& ctx);

BC carlson_rc(const BC& x, const BC& y, const BCC& ctx);
BC carlson_rf(const BC& x, const BC& y, const BC& z, const BCC& ctx);
BC carlson_rd(const BC& x, const BC& y, const BC& z, const BCC& ctx);
BC carlson_rj(const BC& x, const BC& y, const BC& z, const BC& p, const BCC& ctx);
BC carlson_rg(const BC& x, const BC& y, const BC& z, const BCC& ctx);
BC elliptic_k(const BC& m, const BCC& ctx);
BC elliptic_e(const BC& m, const BCC& ctx);
BC elliptic_pi(const BC& n, const BC& m, const BCC& ctx);
BC elliptic_f(const BC& phi, const BC& m, const BCC& ctx);
BC elliptic_e(const BC& phi, const BC& m, const BCC& ctx);
BC elliptic_pi(const BC& phi, const BC& n, const BC& m, const BCC& ctx);

BC jacobi_am(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_sn(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_cn(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_dn(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_ns(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_nc(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_nd(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_sc(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_sd(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_cs(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_cd(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_ds(const BC& u, const BC& m, const BCC& ctx);
BC jacobi_dc(const BC& u, const BC& m, const BCC& ctx);

BC airy_ai(const BC& z, const BCC& ctx);
BC airy_bi(const BC& z, const BCC& ctx);
BC scorer_gi(const BC& z, const BCC& ctx);
BC scorer_hi(const BC& z, const BCC& ctx);

template <class T>
struct is_arg : std::integral_constant<bool, std::is_same<typename std::decay<T>::type, BigComplex>::value || multiprecision::bcdetail::is_real_operand<typename std::decay<T>::type>::value> {};

template <class... T> struct any_cx;
template <> struct any_cx<> : std::false_type {};
template <class T, class... R>
struct any_cx<T, R...> : std::integral_constant<bool, std::is_same<typename std::decay<T>::type, BigComplex>::value || any_cx<R...>::value> {};

template <class... T> struct all_args;
template <> struct all_args<> : std::true_type {};
template <class T, class... R>
struct all_args<T, R...> : std::integral_constant<bool, is_arg<T>::value && all_args<R...>::value> {};

template <class... T>
using if_mixed = typename std::enable_if<all_args<T...>::value && any_cx<T...>::value, int>::type;

inline BigComplex to_bc(const BigComplex& z) { return z; }

template <class R, typename std::enable_if<multiprecision::bcdetail::is_real_operand<R>::value, int>::type = 0>
inline BigComplex to_bc(const R& x) { return BigComplex(multiprecision::bcdetail::as_float(x)); }

} // namespace cxsf

#define FIZMO_CXSF_1(NAME)                                                                                  \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                   \
    inline BigComplex NAME(const C& z, const BigComplexContext& ctx) { return cxsf::NAME(z, ctx); }         \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                   \
    inline BigComplex NAME(const C& z) { return cxsf::NAME(z, BigComplexContext::current()); }

#define FIZMO_CXSF_1N(NAME)                                                                                 \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                   \
    inline BigComplex NAME(const C& z, std::int64_t n, const BigComplexContext& ctx) { return cxsf::NAME(z, n, ctx); } \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                   \
    inline BigComplex NAME(const C& z, std::int64_t n) { return cxsf::NAME(z, n, BigComplexContext::current()); }

#define FIZMO_CXSF_2(NAME)                                                                                  \
    template <typename A, typename B, cxsf::if_mixed<A, B> = 0>                                             \
    inline BigComplex NAME(const A& a, const B& b, const BigComplexContext& ctx) {                          \
        return cxsf::NAME(cxsf::to_bc(a), cxsf::to_bc(b), ctx);                                             \
    }                                                                                                       \
    template <typename A, typename B, cxsf::if_mixed<A, B> = 0>                                             \
    inline BigComplex NAME(const A& a, const B& b) { return cxsf::NAME(cxsf::to_bc(a), cxsf::to_bc(b), BigComplexContext::current()); }

FIZMO_CXSF_1(sinc)
FIZMO_CXSF_1(normalized_sinc)
FIZMO_CXSF_1(gudermannian)
FIZMO_CXSF_1(inv_gudermannian)
FIZMO_CXSF_1(pow2)
FIZMO_CXSF_1(pow10)
FIZMO_CXSF_2(log)

FIZMO_CXSF_1N(chebyshev_t)
FIZMO_CXSF_1N(chebyshev_u)
FIZMO_CXSF_1N(chebyshev_v)
FIZMO_CXSF_1N(chebyshev_w)
FIZMO_CXSF_1(fibonacci)
FIZMO_CXSF_1(lucas)
FIZMO_CXSF_1N(fibonacci_polynomial)
FIZMO_CXSF_1N(lucas_polynomial)
FIZMO_CXSF_1N(legendre_p)
FIZMO_CXSF_1N(legendre_q)

FIZMO_CXSF_1(log_gamma)
FIZMO_CXSF_1(gamma)
FIZMO_CXSF_1(factorial)
FIZMO_CXSF_1(digamma)
FIZMO_CXSF_1(trigamma)
FIZMO_CXSF_1(harmonic)
FIZMO_CXSF_2(beta)
FIZMO_CXSF_2(log_beta)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex log_abs_gamma(const C& z, const BigComplexContext& ctx) {
    const BigComplex v = cxsf::log_gamma(z, ctx);
    return v.is_undefined() ? v : BigComplex(v.real(), BigFloat::zero());
}
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex log_abs_gamma(const C& z) { return log_abs_gamma(z, BigComplexContext::current()); }

template <typename A, typename B, cxsf::if_mixed<A, B> = 0>
inline BigComplex log_abs_beta(const A& a, const B& b, const BigComplexContext& ctx) {
    const BigComplex v = cxsf::log_beta(cxsf::to_bc(a), cxsf::to_bc(b), ctx);
    return v.is_undefined() ? v : BigComplex(v.real(), BigFloat::zero());
}
template <typename A, typename B, cxsf::if_mixed<A, B> = 0>
inline BigComplex log_abs_beta(const A& a, const B& b) { return log_abs_beta(a, b, BigComplexContext::current()); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex polygamma(const C& z, int n, const BigComplexContext& ctx) { return cxsf::polygamma(z, n, ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex polygamma(const C& z, int n) { return cxsf::polygamma(z, n, BigComplexContext::current()); }

FIZMO_CXSF_1N(rising_pochhammer)
FIZMO_CXSF_1N(falling_pochhammer)
FIZMO_CXSF_1N(combination)
FIZMO_CXSF_1N(permutation)
FIZMO_CXSF_2(rising_pochhammer)
FIZMO_CXSF_2(falling_pochhammer)
FIZMO_CXSF_2(combination)
FIZMO_CXSF_2(permutation)

FIZMO_CXSF_1(riemann_zeta)
FIZMO_CXSF_1(riemann_chi)
FIZMO_CXSF_1(riemann_xi)
FIZMO_CXSF_1(dirichlet_eta)
FIZMO_CXSF_1(dirichlet_lambda)
FIZMO_CXSF_1(dirichlet_beta)
FIZMO_CXSF_2(hurwitz_zeta)

template <typename Z, typename S, typename A, cxsf::if_mixed<Z, S, A> = 0>
inline BigComplex lerch_transcendent(const Z& z, const S& s, const A& a, const BigComplexContext& ctx) {
    return cxsf::lerch_transcendent(cxsf::to_bc(z), cxsf::to_bc(s), cxsf::to_bc(a), ctx);
}
template <typename Z, typename S, typename A, cxsf::if_mixed<Z, S, A> = 0>
inline BigComplex lerch_transcendent(const Z& z, const S& s, const A& a) {
    return cxsf::lerch_transcendent(cxsf::to_bc(z), cxsf::to_bc(s), cxsf::to_bc(a), BigComplexContext::current());
}

FIZMO_CXSF_2(polylog)
FIZMO_CXSF_2(legendre_chi)
FIZMO_CXSF_1(dilogarithm)
FIZMO_CXSF_1(trilogarithm)
FIZMO_CXSF_1(spence_function)
FIZMO_CXSF_1(spence_integral)
FIZMO_CXSF_1(rogers_l)
FIZMO_CXSF_1(rogers_l_normalized)
FIZMO_CXSF_1N(clausen)
FIZMO_CXSF_1N(debye)
FIZMO_CXSF_1N(debye_integral)
FIZMO_CXSF_1N(debye_tail)

FIZMO_CXSF_1(erf)
FIZMO_CXSF_1(erfc)
FIZMO_CXSF_1(erfi)
FIZMO_CXSF_1(inv_erf)
FIZMO_CXSF_1(inv_erfc)
FIZMO_CXSF_1(inv_erfi)
FIZMO_CXSF_2(generalized_erf)
FIZMO_CXSF_2(generalized_erfc)
FIZMO_CXSF_1(fresnel_c)
FIZMO_CXSF_1(fresnel_s)

FIZMO_CXSF_1(sin_integral)
FIZMO_CXSF_1(cos_integral)
FIZMO_CXSF_1(sinh_integral)
FIZMO_CXSF_1(cosh_integral)
FIZMO_CXSF_1(exp_integral)
FIZMO_CXSF_1N(exp_integral_generalized)
FIZMO_CXSF_1(log_integral)
FIZMO_CXSF_1N(log_integral_generalized)

FIZMO_CXSF_2(lower_incomplete_gamma)
FIZMO_CXSF_2(upper_incomplete_gamma)
FIZMO_CXSF_2(regularized_gamma_p)
FIZMO_CXSF_2(regularized_gamma_q)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex lambert_w(const C& z, std::int64_t k, const BigComplexContext& ctx) { return cxsf::lambert_w(z, k, ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex lambert_w(const C& z, std::int64_t k = 0) { return cxsf::lambert_w(z, k, BigComplexContext::current()); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex lambert_w(const C& z, const BigComplexContext& ctx) { return cxsf::lambert_w(z, 0, ctx); }
#define FIZMO_CXSF_3(NAME)                                                                                  \
    template <typename A, typename B, typename C, cxsf::if_mixed<A, B, C> = 0>                              \
    inline BigComplex NAME(const A& a, const B& b, const C& c, const BigComplexContext& ctx) {               \
        return cxsf::NAME(cxsf::to_bc(a), cxsf::to_bc(b), cxsf::to_bc(c), ctx);                             \
    }                                                                                                       \
    template <typename A, typename B, typename C, cxsf::if_mixed<A, B, C> = 0>                              \
    inline BigComplex NAME(const A& a, const B& b, const C& c) {                                            \
        return cxsf::NAME(cxsf::to_bc(a), cxsf::to_bc(b), cxsf::to_bc(c), BigComplexContext::current());    \
    }

FIZMO_CXSF_2(carlson_rc)
FIZMO_CXSF_3(carlson_rf)
FIZMO_CXSF_3(carlson_rd)
FIZMO_CXSF_3(carlson_rg)

template <typename X, typename Y, typename Z, typename P, cxsf::if_mixed<X, Y, Z, P> = 0>
inline BigComplex carlson_rj(const X& x, const Y& y, const Z& z, const P& p, const BigComplexContext& ctx) {
    return cxsf::carlson_rj(cxsf::to_bc(x), cxsf::to_bc(y), cxsf::to_bc(z), cxsf::to_bc(p), ctx);
}
template <typename X, typename Y, typename Z, typename P, cxsf::if_mixed<X, Y, Z, P> = 0>
inline BigComplex carlson_rj(const X& x, const Y& y, const Z& z, const P& p) {
    return cxsf::carlson_rj(cxsf::to_bc(x), cxsf::to_bc(y), cxsf::to_bc(z), cxsf::to_bc(p), BigComplexContext::current());
}

FIZMO_CXSF_1(elliptic_k)
FIZMO_CXSF_1(elliptic_e)
FIZMO_CXSF_2(elliptic_pi)
FIZMO_CXSF_2(elliptic_f)
FIZMO_CXSF_2(elliptic_e)
FIZMO_CXSF_3(elliptic_pi)

FIZMO_CXSF_2(jacobi_am)
FIZMO_CXSF_2(jacobi_sn)
FIZMO_CXSF_2(jacobi_cn)
FIZMO_CXSF_2(jacobi_dn)
FIZMO_CXSF_2(jacobi_ns)
FIZMO_CXSF_2(jacobi_nc)
FIZMO_CXSF_2(jacobi_nd)
FIZMO_CXSF_2(jacobi_sc)
FIZMO_CXSF_2(jacobi_sd)
FIZMO_CXSF_2(jacobi_cs)
FIZMO_CXSF_2(jacobi_cd)
FIZMO_CXSF_2(jacobi_ds)
FIZMO_CXSF_2(jacobi_dc)

FIZMO_CXSF_1(airy_ai)
FIZMO_CXSF_1(airy_bi)
FIZMO_CXSF_1(scorer_gi)
FIZMO_CXSF_1(scorer_hi)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex lambert_w0(const C& z, const BigComplexContext& ctx = BigComplexContext::current()) { return cxsf::lambert_w(z, 0, ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex lambert_wm1(const C& z, const BigComplexContext& ctx = BigComplexContext::current()) { return cxsf::lambert_w(z, -1, ctx); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex clausen_integral(const C& theta, const BigComplexContext& ctx) { return cxsf::clausen(theta, 2, ctx); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex clausen_integral(const C& theta) { return cxsf::clausen(theta, 2, BigComplexContext::current()); }

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline BigComplex conj(const C& z) { return z.conjugate(); }

#define FIZMO_CXSF_PARTS(NAME)                                                                              \
    template <typename C, bcdetail_fn::if_complex<C> = 0>                                                   \
    inline BigComplex NAME(const C& z) { return BigComplex(math::NAME(z.real()), math::NAME(z.imaginary())); }

FIZMO_CXSF_PARTS(floor)
FIZMO_CXSF_PARTS(ceiling)
FIZMO_CXSF_PARTS(round)
FIZMO_CXSF_PARTS(trunc)
FIZMO_CXSF_PARTS(fractional_part)

template <typename C, bcdetail_fn::if_complex<C> = 0>
inline bool is_integer(const C& z) { return z.real().is_integer() && z.imaginary().is_integer(); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline bool is_one(const C& z) { return z.imaginary().is_zero() && BigFloat::compare(z.real(), BigFloat::one()) == BigFloat::ordering::equal; }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline bool is_real(const C& z) { return z.is_real(); }
template <typename C, bcdetail_fn::if_complex<C> = 0>
inline bool is_imaginary(const C& z) { return z.is_imaginary(); }

#undef FIZMO_CXSF_PARTS
#undef FIZMO_CXSF_3
#undef FIZMO_CXSF_2
#undef FIZMO_CXSF_1N
#undef FIZMO_CXSF_1

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_FUNCTIONS_HPP
