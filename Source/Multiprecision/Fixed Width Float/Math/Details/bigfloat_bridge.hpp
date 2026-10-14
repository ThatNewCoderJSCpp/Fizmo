#ifndef FIZMO_MULTIPRECISION_BIGFLOAT_BRIDGE_DETAIL_HPP
#define FIZMO_MULTIPRECISION_BIGFLOAT_BRIDGE_DETAIL_HPP

#include "../../../Big/big_complex.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace math {
namespace bdetail {

template <class T> struct is_mp_float : std::false_type {};
template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
struct is_mp_float<fizmo::multiprecision::floatmp<TB, MB, S>> : std::true_type {};

template <class T> struct is_mp_int : std::false_type {};
template <std::size_t B, fizmo::multiprecision::sign S>
struct is_mp_int<fizmo::multiprecision::integer<B, S>> : std::true_type {};

template <class T>
struct is_operand : std::integral_constant<bool,
    std::is_arithmetic<T>::value || is_mp_float<T>::value || is_mp_int<T>::value> {};

template <class T, class = void>
struct as_float { using type = T; };

template <class T>
struct as_float<T, typename std::enable_if<std::is_integral<T>::value>::type> { using type = double; };

template <std::size_t B, fizmo::multiprecision::sign S>
struct as_float<fizmo::multiprecision::integer<B, S>, void> { using type = fizmo_float_from_int_t<fizmo::multiprecision::integer<B, S>>; };

template <class FA, class FB, bool AMp, bool BMp> struct pick;
template <class FA, class FB> struct pick<FA, FB, true,  true > { using type = fizmo_common_type_t<FA, FB>; };
template <class FA, class FB> struct pick<FA, FB, true,  false> { using type = FA; };
template <class FA, class FB> struct pick<FA, FB, false, true > { using type = FB; };
template <class FA, class FB> struct pick<FA, FB, false, false> { using type = typename std::common_type<FA, FB>::type; };

template <class A, class B>
struct common_result {
    using FA   = typename as_float<A>::type;
    using FB   = typename as_float<B>::type;
    using type = typename pick<FA, FB, is_mp_float<FA>::value, is_mp_float<FB>::value>::type;
};

template <class T>
struct is_fw : std::integral_constant<bool, is_mp_float<T>::value || is_mp_int<T>::value> {};

template <class... T> struct any_fw;
template <> struct any_fw<> : std::false_type {};
template <class T, class... R>
struct any_fw<T, R...> : std::integral_constant<bool, is_fw<T>::value || any_fw<R...>::value> {};

template <class... T> struct all_operand;
template <> struct all_operand<> : std::true_type {};
template <class T, class... R>
struct all_operand<T, R...> : std::integral_constant<bool, is_operand<T>::value && all_operand<R...>::value> {};

template <class... T> struct common;
template <class T> struct common<T> { using type = typename as_float<T>::type; };
template <class A, class B, class... R>
struct common<A, B, R...> { using type = typename common<typename common_result<A, B>::type, R...>::type; };

template <class... T>
using common_t = typename common<T...>::type;

template <class... T>
using enable_fw_t = typename std::enable_if<all_operand<T...>::value && any_fw<T...>::value, int>::type;

template <class T>
fizmo::multiprecision::BigFloat to_bf(const T& v) { return fizmo::multiprecision::BigFloat(v); }

inline fizmo::multiprecision::BigFloatContext ctx_of(std::size_t p) {
    return fizmo::multiprecision::BigFloatContext(fizmo::multiprecision::BigFloatContext::clamp_precision(p), fizmo::multiprecision::RoundingMode::nearest_even);
}

template <class F>
std::int64_t min_exponent_ll() noexcept {
    const std::int64_t eb   = static_cast<std::int64_t>(F::exponent_bits);
    const std::int64_t bias = (eb >= 62) ? (std::numeric_limits<std::int64_t>::max() / 4) : ((std::int64_t(1) << (eb - 1)) - 1);
    return 1 - bias;
}

inline std::int64_t top_exponent(const fizmo::multiprecision::BigFloat& r) noexcept {
    return r.exponent() + static_cast<std::int64_t>(r.significand().bit_length()) - 1;
}

template <class F>
F finish(const fizmo::multiprecision::BigFloat& r) {
    if (!is_fizmo_signed_v<F> && r.signbit() && !r.is_unordered() && !r.is_zero()) return F::nan();
    return r.template to_floatmp<F>();
}

template <class F, class Eval, typename std::enable_if<std::is_floating_point<F>::value, int>::type = 0>
F eval(Eval&& ev) {
    const std::int64_t digits = std::numeric_limits<F>::digits;
    const std::int64_t emin   = std::numeric_limits<F>::min_exponent - 1;
    std::size_t        p      = static_cast<std::size_t>(digits);
    fizmo::multiprecision::BigFloat           r      = ev(ctx_of(p));

    for (int i = 0; i < 4 && r.is_finite() && !r.is_zero(); ++i) {
        const std::int64_t top = top_exponent(r);
        if (top >= emin) break;
        const std::int64_t need = digits - (emin - top);
        if (need < 2 || static_cast<std::size_t>(need) >= p) break;
        p = static_cast<std::size_t>(need);
        r = ev(ctx_of(p));
    }

    return r.template to_std_float<F>();
}

template <class F, class Eval, typename std::enable_if<!std::is_floating_point<F>::value, int>::type = 0>
F eval(Eval&& ev) {
    const std::size_t full = F::mantissa_bits + 1;
    std::size_t       p    = full;
    fizmo::multiprecision::BigFloat          r    = ev(ctx_of(p));

    for (int i = 0; i < 4 && r.is_finite() && !r.is_zero(); ++i) {
        const std::int64_t me  = min_exponent_ll<F>();
        const std::int64_t top = top_exponent(r);
        if (top >= me) break;
        const std::int64_t need = static_cast<std::int64_t>(full) - (me - top);
        if (need < 1 || static_cast<std::size_t>(need) >= p) break;
        p = static_cast<std::size_t>(need);
        r = ev(ctx_of(p));
    }

    return finish<F>(r);
}

template <class F>
F exact(const fizmo::multiprecision::BigFloat& v) { return finish<F>(v); }

} // namespace bdetail
} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIGFLOAT_BRIDGE_DETAIL_HPP
