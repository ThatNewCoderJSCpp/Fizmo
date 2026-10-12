#ifndef FIZMO_LORENTZ_FACTOR_HPP
#define FIZMO_LORENTZ_FACTOR_HPP

#include "../../../Converters/quantity.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

#if defined(__cpp_lib_is_constant_evaluated)
    #define FIZMO_REL_CONSTANT_EVALUATED() std::is_constant_evaluated()
#elif defined(__has_builtin)
    #if __has_builtin(__builtin_is_constant_evaluated)
        #define FIZMO_REL_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
    #endif
#elif defined(_MSC_VER) && _MSC_VER >= 1925
    #define FIZMO_REL_CONSTANT_EVALUATED() __builtin_is_constant_evaluated()
#endif

#ifndef FIZMO_REL_CONSTANT_EVALUATED
    #define FIZMO_REL_CONSTANT_EVALUATED() true
#endif

namespace fizmo {
namespace relativity {
namespace special {
namespace rdetail {

namespace bd = math::bdetail;

using multiprecision::BigFloat;
using multiprecision::BigFloatContext;

template <class T>
struct is_big : std::is_same<typename std::decay<T>::type, BigFloat> {};

template <class T>
struct is_value : std::integral_constant<bool, std::is_arithmetic<T>::value || bd::is_fw<T>::value || is_big<T>::value> {};

template <class... T> struct all_values;
template <> struct all_values<> : std::true_type {};
template <class T, class... R>
struct all_values<T, R...> : std::integral_constant<bool, is_value<T>::value && all_values<R...>::value> {};

template <class... T> struct any_big;
template <> struct any_big<> : std::false_type {};
template <class T, class... R>
struct any_big<T, R...> : std::integral_constant<bool, is_big<T>::value || any_big<R...>::value> {};

template <bool Big, bool Fixed, class... T> struct result_impl;
template <bool Fixed, class... T> struct result_impl<true, Fixed, T...> { using type = BigFloat; };
template <class... T> struct result_impl<false, true, T...>  { using type = bd::common_t<T...>; };
template <class... T> struct result_impl<false, false, T...> { using type = typename std::common_type<T..., double>::type; };

template <class... T>
using result_t = typename result_impl<any_big<T...>::value, bd::any_fw<T...>::value, typename std::decay<T>::type...>::type;

template <class N>
struct tag { using type = N; };

template <class U>
constexpr typename std::enable_if<is_fizmo_unit_v<U>, bool>::type has_dim(units::Dimension d) noexcept { return U::dimension() == d; }

template <class U>
constexpr typename std::enable_if<!is_fizmo_unit_v<U>, bool>::type has_dim(units::Dimension) noexcept { return false; }

template <bool B>
using enable_t = typename std::enable_if<B, int>::type;

template <class T>
constexpr T newton_sqrt(T x) noexcept {
    if (x != x) return x;
    if (x < T(0)) return std::numeric_limits<T>::quiet_NaN();
    if (x == T(0) || x == std::numeric_limits<T>::infinity()) return x;
    T g = (x > T(1)) ? x : T(1);

    for (;;) {
        const T n = (g + x / g) / T(2);
        if (!(n < g)) return g;
        g = n;
    }
}

template <class N>
constexpr N sqrt_(const N& x) {
    if constexpr (std::is_floating_point<N>::value) {
        if (FIZMO_REL_CONSTANT_EVALUATED()) return newton_sqrt(x);
        return std::sqrt(x);
    } else {
        return math::sqrt(x, BigFloatContext::current());
    }
}

template <class N>
constexpr N nan_() {
    if constexpr (std::is_floating_point<N>::value) return std::numeric_limits<N>::quiet_NaN();
    else                                            return BigFloat::nan();
}

template <class N>
constexpr N abs_(const N& x) {
    if constexpr (std::is_floating_point<N>::value) return (x < N(0)) ? -x : x;
    else                                            return x.abs();
}

template <class N>
constexpr N c_() { return N(299792458); }

template <class N, class X>
constexpr N to_(const X& x) {
    if constexpr (std::is_floating_point<N>::value) return static_cast<N>(x);
    else                                            return BigFloat(x);
}

template <class N, class FU, class TU>
constexpr N conv(const N& x, FU from, TU to) {
    if constexpr (std::is_floating_point<N>::value) return static_cast<N>(units::convert(static_cast<long double>(x), from, to));
    else                                            return units::convert(x, from, to, BigFloatContext::current());
}

template <class N, class X, class FU, class TU>
constexpr N in_(const X& x, FU from, TU to) { return conv<N>(to_<N>(x), from, to); }

template <class N>
constexpr N s_(const N& beta) {
    const N one(1);
    const N arg = (one - beta) * (one + beta);
    if (!(arg > N(0))) return nan_<N>();
    return sqrt_(arg);
}

template <class N>
constexpr bool neg_(const N& x) { return x < N(0); }

template <class N>
constexpr bool is_nan_(const N& x) { return !(x == x); }

template <class N>
constexpr N gamma_(const N& beta) {
    const N one(1);
    const N arg = (one - beta) * (one + beta);
    if (!(arg > N(0))) return nan_<N>();
    return one / sqrt_(arg);
}

template <class N>
constexpr N gamma_minus_one_(const N& beta) {
    const N one(1);
    const N arg = (one - beta) * (one + beta);
    if (!(arg > N(0))) return nan_<N>();
    const N s = sqrt_(arg);
    return beta * beta / (s * (one + s));
}

template <class F>
BigFloat run_big(F& f) {
    const BigFloatContext               ctx = BigFloatContext::current();
    const multiprecision::ScopedContext guard(ctx.extended(32));
    return f(tag<BigFloat>{}).rounded(ctx);
}

template <class R, class F>
R run_fixed(F& f) {
    return bd::eval<R>([&](const BigFloatContext& c) {
        const multiprecision::ScopedContext guard(c.extended(32));
        return f(tag<BigFloat>{}).rounded(c);
    });
}

template <class R, class F>
constexpr R run(F&& f) {
    if constexpr (std::is_floating_point<R>::value) return f(tag<R>{});
    else if constexpr (is_big<R>::value)            return run_big(f);
    else                                            return run_fixed<R>(f);
}

} // namespace rdetail

template <typename T, rdetail::enable_t<rdetail::is_value<T>::value> = 0>
constexpr rdetail::result_t<T> lorentz_factor(const T& velocity) {
    return rdetail::run<rdetail::result_t<T>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::gamma_(rdetail::to_<N>(velocity));
    });
}

template <typename T, class VU, rdetail::enable_t<rdetail::is_value<T>::value && rdetail::has_dim<VU>(units::DIMENSION_VELOCITY)> = 0>
constexpr rdetail::result_t<T> lorentz_factor(const T& velocity, const VU velocity_unit) {
    return rdetail::run<rdetail::result_t<T>>([&](auto t) {
        using N = typename decltype(t)::type;
        return rdetail::gamma_(rdetail::in_<N>(velocity, velocity_unit, units::velocity::light_speed));
    });
}

} // namespace special
} // namespace relativity
} // namespace fizmo

#endif // FIZMO_LORENTZ_FACTOR_HPP
