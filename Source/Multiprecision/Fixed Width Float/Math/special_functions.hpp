#ifndef FIZMO_MULTIPRECISION_FIXED_SPECIAL_FUNCTIONS_HPP
#define FIZMO_MULTIPRECISION_FIXED_SPECIAL_FUNCTIONS_HPP

#include "Details/bigfloat_bridge.hpp"
#include "Details/fast_float.hpp"

namespace fizmo {
namespace math {

#define FIZMO_FW_UNARY(NAME)                                                                                       \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                                              \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {                                                         \
        const fizmo::multiprecision::BigFloat a(x);                                                                                       \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); });          \
    }                                                                                                              \
                                                                                                                   \
    template <std::size_t B, fizmo::multiprecision::sign S>                                                                               \
    fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>> NAME(const fizmo::multiprecision::integer<B, S>& n) {                                           \
        const fizmo::multiprecision::BigFloat a(n);                                                                                       \
        return bdetail::eval<fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); }); \
    }

#define FIZMO_FW_UNARY_FAST(NAME, OP)                                                                              \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                       \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {           \
        fizmo::multiprecision::floatmp<TB, MB, S> quick;                                                           \
        if (fwfast::unary(fwfast::fop::OP, x, quick)) return quick;                                                    \
        const fizmo::multiprecision::BigFloat a(x);                                                                \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); }); \
    }                                                                                                              \
                                                                                                                   \
    template <std::size_t B, fizmo::multiprecision::sign S>                                                        \
    fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>> NAME(const fizmo::multiprecision::integer<B, S>& n) { \
        const fizmo::multiprecision::BigFloat a(n);                                                                \
        return bdetail::eval<fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, c); }); \
    }

#define FIZMO_FW_BINARY_FAST(NAME, OP)                                                                             \
    template <class A, class B, bdetail::enable_fw_t<A, B> = 0>                                                    \
    bdetail::common_t<A, B> NAME(const A& x, const B& y) {                                                         \
        if constexpr (std::is_same<A, bdetail::common_t<A, B>>::value && std::is_same<B, bdetail::common_t<A, B>>::value) { \
            bdetail::common_t<A, B> quick;                                                                         \
            if (fwfast::binary(fwfast::fop2::OP, x, y, quick)) return quick;                                           \
        }                                                                                                          \
        const fizmo::multiprecision::BigFloat a = bdetail::to_bf(x);                                               \
        const fizmo::multiprecision::BigFloat b = bdetail::to_bf(y);                                               \
        return bdetail::eval<bdetail::common_t<A, B>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, b, c); }); \
    }

#define FIZMO_FW_BINARY(NAME)                                                                                      \
    template <class A, class B, bdetail::enable_fw_t<A, B> = 0>                                                    \
    bdetail::common_t<A, B> NAME(const A& x, const B& y) {                                                         \
        const fizmo::multiprecision::BigFloat a = bdetail::to_bf(x);                                                                      \
        const fizmo::multiprecision::BigFloat b = bdetail::to_bf(y);                                                                      \
        return bdetail::eval<bdetail::common_t<A, B>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, b, c); });   \
    }

#define FIZMO_FW_TERNARY(NAME)                                                                                     \
    template <class A, class B, class C, bdetail::enable_fw_t<A, B, C> = 0>                                        \
    bdetail::common_t<A, B, C> NAME(const A& x, const B& y, const C& z) {                                          \
        const fizmo::multiprecision::BigFloat a = bdetail::to_bf(x);                                                                      \
        const fizmo::multiprecision::BigFloat b = bdetail::to_bf(y);                                                                      \
        const fizmo::multiprecision::BigFloat d = bdetail::to_bf(z);                                                                      \
        return bdetail::eval<bdetail::common_t<A, B, C>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, b, d, c); }); \
    }

#define FIZMO_FW_QUATERNARY(NAME)                                                                                  \
    template <class A, class B, class C, class D, bdetail::enable_fw_t<A, B, C, D> = 0>                            \
    bdetail::common_t<A, B, C, D> NAME(const A& x, const B& y, const C& z, const D& w) {                           \
        const fizmo::multiprecision::BigFloat a = bdetail::to_bf(x);                                                                      \
        const fizmo::multiprecision::BigFloat b = bdetail::to_bf(y);                                                                      \
        const fizmo::multiprecision::BigFloat d = bdetail::to_bf(z);                                                                      \
        const fizmo::multiprecision::BigFloat e = bdetail::to_bf(w);                                                                      \
        return bdetail::eval<bdetail::common_t<A, B, C, D>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, b, d, e, c); }); \
    }

#define FIZMO_FW_INDEXED(NAME, IT)                                                                                 \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                                              \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x, IT n) {                                                   \
        const fizmo::multiprecision::BigFloat a(x);                                                                                       \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, n, c); });       \
    }

#define FIZMO_FW_COUNTING(NAME)                                                                                    \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S, class I, typename std::enable_if<std::is_integral<I>::value, int>::type = 0> \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x, I n) {                                                    \
        const fizmo::multiprecision::BigFloat     a(x);                                                                                   \
        const std::int64_t k = static_cast<std::int64_t>(n);                                                       \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, k, c); });       \
    }                                                                                                              \
                                                                                                                   \
    template <class A, class B, typename std::enable_if<!std::is_integral<B>::value, int>::type = 0, bdetail::enable_fw_t<A, B> = 0> \
    bdetail::common_t<A, B> NAME(const A& x, const B& y) {                                                         \
        const fizmo::multiprecision::BigFloat a = bdetail::to_bf(x);                                                                      \
        const fizmo::multiprecision::BigFloat b = bdetail::to_bf(y);                                                                      \
        return bdetail::eval<bdetail::common_t<A, B>>([&](const fizmo::multiprecision::BigFloatContext& c) { return NAME(a, b, c); });   \
    }

FIZMO_FW_UNARY_FAST(sinc, sinc)
FIZMO_FW_UNARY_FAST(normalized_sinc, normalized_sinc)

FIZMO_FW_UNARY_FAST(arcsin, arcsin)
FIZMO_FW_UNARY_FAST(arccos, arccos)
FIZMO_FW_UNARY_FAST(arctan, arctan)
FIZMO_FW_UNARY_FAST(arccot, arccot)
FIZMO_FW_UNARY_FAST(arcsec, arcsec)
FIZMO_FW_UNARY_FAST(arccsc, arccsc)
FIZMO_FW_BINARY_FAST(atan2, atan2)

FIZMO_FW_UNARY_FAST(ln, log)

FIZMO_FW_UNARY_FAST(arcsinh, arcsinh)
FIZMO_FW_UNARY_FAST(arccosh, arccosh)
FIZMO_FW_UNARY_FAST(arctanh, arctanh)
FIZMO_FW_UNARY_FAST(arccoth, arccoth)
FIZMO_FW_UNARY_FAST(arcsech, arcsech)
FIZMO_FW_UNARY_FAST(arccsch, arccsch)

FIZMO_FW_UNARY_FAST(gudermannian, gudermannian)
FIZMO_FW_UNARY_FAST(inv_gudermannian, inv_gudermannian)

FIZMO_FW_UNARY_FAST(erf, erf)
FIZMO_FW_UNARY_FAST(erfc, erfc)
FIZMO_FW_UNARY(erfi)
FIZMO_FW_UNARY(inv_erf)
FIZMO_FW_UNARY(inv_erfc)
FIZMO_FW_UNARY(inv_erfi)
FIZMO_FW_BINARY(generalized_erf)
FIZMO_FW_BINARY(generalized_erfc)

FIZMO_FW_UNARY_FAST(gamma, gamma)
FIZMO_FW_UNARY(log_gamma)
FIZMO_FW_UNARY_FAST(log_abs_gamma, log_abs_gamma)
FIZMO_FW_UNARY(factorial)
FIZMO_FW_BINARY(beta)
FIZMO_FW_BINARY(log_beta)
FIZMO_FW_BINARY(log_abs_beta)

FIZMO_FW_UNARY(digamma)
FIZMO_FW_UNARY(trigamma)
FIZMO_FW_INDEXED(polygamma, int)
FIZMO_FW_UNARY(harmonic)

FIZMO_FW_INDEXED(chebyshev_t, std::int64_t)
FIZMO_FW_INDEXED(chebyshev_u, std::int64_t)
FIZMO_FW_INDEXED(chebyshev_v, std::int64_t)
FIZMO_FW_INDEXED(chebyshev_w, std::int64_t)
FIZMO_FW_UNARY(fibonacci)
FIZMO_FW_UNARY(lucas)
FIZMO_FW_INDEXED(fibonacci_polynomial, std::int64_t)
FIZMO_FW_INDEXED(lucas_polynomial, std::int64_t)
FIZMO_FW_INDEXED(legendre_p, std::int64_t)
FIZMO_FW_INDEXED(legendre_q, std::int64_t)
FIZMO_FW_BINARY(legendre_chi)

FIZMO_FW_UNARY(riemann_zeta)
FIZMO_FW_UNARY(riemann_xi)
FIZMO_FW_UNARY(riemann_chi)
FIZMO_FW_BINARY(hurwitz_zeta)
FIZMO_FW_TERNARY(lerch_transcendent)
FIZMO_FW_UNARY(dirichlet_eta)
FIZMO_FW_UNARY(dirichlet_lambda)
FIZMO_FW_UNARY(dirichlet_beta)

FIZMO_FW_BINARY(polylog)
FIZMO_FW_UNARY(dilogarithm)
FIZMO_FW_UNARY(trilogarithm)
FIZMO_FW_UNARY(rogers_l)
FIZMO_FW_UNARY(rogers_l_normalized)
FIZMO_FW_UNARY(spence_function)
FIZMO_FW_UNARY(spence_integral)

FIZMO_FW_UNARY(airy_ai)
FIZMO_FW_UNARY(airy_bi)
FIZMO_FW_UNARY(scorer_gi)
FIZMO_FW_UNARY(scorer_hi)

FIZMO_FW_BINARY(carlson_rc)
FIZMO_FW_TERNARY(carlson_rf)
FIZMO_FW_TERNARY(carlson_rd)
FIZMO_FW_TERNARY(carlson_rg)
FIZMO_FW_QUATERNARY(carlson_rj)

FIZMO_FW_UNARY(elliptic_k)
FIZMO_FW_UNARY(elliptic_e)
FIZMO_FW_BINARY(elliptic_e)
FIZMO_FW_BINARY(elliptic_f)
FIZMO_FW_BINARY(elliptic_pi)
FIZMO_FW_TERNARY(elliptic_pi)

FIZMO_FW_BINARY(jacobi_am)
FIZMO_FW_BINARY(jacobi_sn)
FIZMO_FW_BINARY(jacobi_cn)
FIZMO_FW_BINARY(jacobi_dn)
FIZMO_FW_BINARY(jacobi_ns)
FIZMO_FW_BINARY(jacobi_nc)
FIZMO_FW_BINARY(jacobi_nd)
FIZMO_FW_BINARY(jacobi_sc)
FIZMO_FW_BINARY(jacobi_sd)
FIZMO_FW_BINARY(jacobi_cs)
FIZMO_FW_BINARY(jacobi_cd)
FIZMO_FW_BINARY(jacobi_ds)
FIZMO_FW_BINARY(jacobi_dc)

FIZMO_FW_UNARY(lambert_w)
FIZMO_FW_INDEXED(lambert_w, std::int64_t)
FIZMO_FW_UNARY(lambert_w0)
FIZMO_FW_UNARY(lambert_wm1)

FIZMO_FW_UNARY(fresnel_c)
FIZMO_FW_UNARY(fresnel_s)
FIZMO_FW_UNARY(sin_integral)
FIZMO_FW_UNARY(cos_integral)
FIZMO_FW_UNARY(sinh_integral)
FIZMO_FW_UNARY(cosh_integral)
FIZMO_FW_UNARY(exp_integral)
FIZMO_FW_INDEXED(exp_integral_generalized, std::int64_t)
FIZMO_FW_UNARY(log_integral)
FIZMO_FW_INDEXED(log_integral_generalized, std::int64_t)
FIZMO_FW_INDEXED(clausen, std::int64_t)
FIZMO_FW_UNARY(clausen_integral)
FIZMO_FW_INDEXED(debye, std::int64_t)
FIZMO_FW_INDEXED(debye_integral, std::int64_t)
FIZMO_FW_INDEXED(debye_tail, std::int64_t)

FIZMO_FW_COUNTING(rising_pochhammer)
FIZMO_FW_COUNTING(falling_pochhammer)
FIZMO_FW_COUNTING(permutation)
FIZMO_FW_COUNTING(combination)

FIZMO_FW_TERNARY(generalized_gaussian_moment)
FIZMO_FW_BINARY(generalized_gaussian_integral)

FIZMO_FW_UNARY_FAST(log1p, log1p)
FIZMO_FW_UNARY_FAST(expm1, expm1)

FIZMO_FW_BINARY(regularized_gamma_p)
FIZMO_FW_BINARY(regularized_gamma_q)
FIZMO_FW_BINARY(lower_incomplete_gamma)
FIZMO_FW_BINARY(upper_incomplete_gamma)
FIZMO_FW_BINARY(inverse_regularized_gamma_p)
FIZMO_FW_BINARY(inverse_regularized_gamma_q)

FIZMO_FW_TERNARY(regularized_beta)
FIZMO_FW_TERNARY(regularized_beta_complement)
FIZMO_FW_TERNARY(incomplete_beta)
FIZMO_FW_TERNARY(inverse_regularized_beta)
FIZMO_FW_TERNARY(inverse_regularized_beta_complement)

#undef FIZMO_FW_UNARY
#undef FIZMO_FW_UNARY_FAST
#undef FIZMO_FW_BINARY_FAST
#undef FIZMO_FW_BINARY
#undef FIZMO_FW_TERNARY
#undef FIZMO_FW_QUATERNARY
#undef FIZMO_FW_INDEXED
#undef FIZMO_FW_COUNTING

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_SPECIAL_FUNCTIONS_HPP