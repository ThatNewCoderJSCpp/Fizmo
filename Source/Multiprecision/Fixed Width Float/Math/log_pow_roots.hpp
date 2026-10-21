#ifndef FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP
#define FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP

#include "Details/log_pow_roots_detail.hpp"
#include "Details/fast_float.hpp"

namespace fizmo {
namespace math {

#define FIZMO_FW_NATIVE_1(NAME)                                                                                    \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_integral<T>::value, double>::type                                              \
    NAME(T v) noexcept { return std::NAME(static_cast<double>(v)); }                                               \
                                                                                                                   \
    template <class T>                                                                                             \
    typename std::enable_if<std::is_floating_point<T>::value, T>::type                                             \
    NAME(T v) noexcept { return std::NAME(v); }

#define FIZMO_FW_BIG_1(NAME, BIG, FILTER)                                                                          \
    template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>                                                              \
    fizmo::multiprecision::floatmp<TB, MB, S> NAME(const fizmo::multiprecision::floatmp<TB, MB, S>& x) {                                                         \
        fizmo::multiprecision::floatmp<TB, MB, S> early;                                                           \
        if (edetail::FILTER(x, early)) return early;                                                               \
        if (fwfast::unary(fwfast::fop::NAME, x, early)) return early;                                                  \
        const BigFloat a(x);                                                                                       \
        return bdetail::eval<fizmo::multiprecision::floatmp<TB, MB, S>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return BIG(a, c); });           \
    }                                                                                                              \
                                                                                                                   \
    template <std::size_t B, fizmo::multiprecision::sign S>                                                                               \
    fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>> NAME(const fizmo::multiprecision::integer<B, S>& n) {                                           \
        const BigFloat a(n);                                                                                       \
        return bdetail::eval<fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>>([&a](const fizmo::multiprecision::BigFloatContext& c) { return BIG(a, c); }); \
    }

FIZMO_FW_NATIVE_1(exp)
FIZMO_FW_NATIVE_1(log)
FIZMO_FW_NATIVE_1(sqrt)
FIZMO_FW_NATIVE_1(cbrt)

FIZMO_FW_BIG_1(exp, exp, no_domain_filter)
FIZMO_FW_BIG_1(pow2, pow2, no_domain_filter)
FIZMO_FW_BIG_1(pow10, pow10, no_domain_filter)
FIZMO_FW_BIG_1(log, ln, log_domain_filter)
FIZMO_FW_BIG_1(log2, log2, log_domain_filter)
FIZMO_FW_BIG_1(log10, log10, log_domain_filter)
FIZMO_FW_BIG_1(sqrt, sqrt, sqrt_domain_filter)
FIZMO_FW_BIG_1(cbrt, cbrt, no_domain_filter)

#undef FIZMO_FW_NATIVE_1
#undef FIZMO_FW_BIG_1

namespace pdetail {

template <class A, class B, int K = kind<A, B>::value> struct apply;

template <class A, class B>
struct apply<A, B, 0> {
    static typename result<A, B>::type run(const A& a, const B& b) {
        using C = typename result<A, B>::type;
        return std::pow(static_cast<C>(a), static_cast<C>(b));
    }
};

template <class A, class B>
struct apply<A, B, 1> {
    static typename result<A, B>::type run(const A& a, const B& b) {
        using R = typename result<A, B>::type;
        return int_pow(R(a), b);
    }
};

template <class A, class B>
struct apply<A, B, 2> {
    static typename result<A, B>::type run(const A& a, const B& b) {
        using F = typename result<A, B>::type;
        if constexpr (std::is_same<A, F>::value && std::is_same<B, F>::value) {
            F quick;
            if (fwfast::binary(fwfast::fop2::pow, a, b, quick)) return quick;
        }
        return pow_run<F>(widen<F>(a), widen<F>(b), bdetail::to_bf(a), bdetail::to_bf(b));
    }
};

} // namespace pdetail

template <class A, class B>
typename std::enable_if<pdetail::is_operand<A>::value && pdetail::is_operand<B>::value, typename pdetail::result<A, B>::type>::type
pow(const A& a, const B& b) { return pdetail::apply<A, B>::run(a, b); }

template <class A, class B>
typename std::enable_if<ldetail::is_operand<A>::value && ldetail::is_operand<B>::value, typename ldetail::common_result<A, B>::type>::type
log(const A& arg, const B& base) {
    using F = typename ldetail::common_result<A, B>::type;
    if constexpr (!ldetail::is_mp_float<F>::value) {
        return static_cast<F>(std::log(static_cast<F>(arg)) / std::log(static_cast<F>(base)));
    } else {
        return ldetail::log_run<F>(ldetail::widen<F>(arg), ldetail::widen<F>(base), bdetail::to_bf(arg), bdetail::to_bf(base));
    }
}

template <class X, class N>
typename std::enable_if<rdetail::is_operand<X>::value && rdetail::is_operand<N>::value, typename rdetail::result<X, N>::type>::type
nth_root(const X& x, const N& n) {
    using F = typename rdetail::result<X, N>::type;

    if constexpr (!ldetail::is_mp_float<F>::value) {
        const double dx = static_cast<double>(x), dn = static_cast<double>(n);
        if (dn == 2.0) return static_cast<F>(std::sqrt(dx));
        if (dn == 3.0) return static_cast<F>(std::cbrt(dx));

        if (dx < 0.0) {
            const bool odd_int = (dn == std::floor(dn)) && (std::fmod(dn, 2.0) != 0.0);
            return odd_int ? static_cast<F>(-std::pow(-dx, 1.0 / dn)) : std::numeric_limits<F>::quiet_NaN();
        }

        return static_cast<F>(std::pow(dx, 1.0 / dn));
    } else {
        return rdetail::root_run<F>(ldetail::widen<F>(x), n, bdetail::to_bf(x), bdetail::to_bf(n));
    }
}

} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_LOG_POW_ROOTS_HPP
