#ifndef FIZMO_MULTIPRECISION_ABS_MIN_MAX_DETAIL_HPP
#define FIZMO_MULTIPRECISION_ABS_MIN_MAX_DETAIL_HPP

#include <initializer_list>
#include "../basic_constants.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace mmdetail {

template <class T>
struct is_fizmo_numeric : std::integral_constant<bool, is_fizmo_float_v<T> || is_fizmo_int_v<T>> {};

template <class T>
struct is_operand : std::integral_constant<bool, is_fizmo_numeric<T>::value || std::is_arithmetic<T>::value> {};

template <class A, class B>
struct same_family : std::integral_constant<bool, (is_fizmo_float_v<A> && is_fizmo_float_v<B>) || (is_fizmo_int_v<A> && is_fizmo_int_v<B>)> {};

template <class A, class B, class = void> struct common2 {};   

template <class A, class B>
struct common2<A, B, typename std::enable_if<same_family<A, B>::value>::type> {
    using type = fizmo_common_type_t<A, B>;
};

template <class A, class B>
struct common2<A, B, typename std::enable_if<is_fizmo_numeric<A>::value && std::is_arithmetic<B>::value>::type> {
    using type = A;
};

template <class A, class B>
struct common2<A, B, typename std::enable_if<std::is_arithmetic<A>::value && is_fizmo_numeric<B>::value>::type> {
    using type = B;
};

template <class A, class B>
struct common2<A, B, typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value>::type> {
    using type = typename std::common_type<A, B>::type;
};

template <class...> struct common_n;
template <class A>  struct common_n<A> { using type = A; };

template <class A, class B>
struct common_n<A, B> { using type = typename common2<A, B>::type; };

template <class A, class B, class C, class... R>
struct common_n<A, B, C, R...> { using type = typename common_n<typename common2<A, B>::type, C, R...>::type; };

template <class T>
struct has_undefined : std::integral_constant<bool, is_fizmo_float_v<T> || is_fizmo_signed_v<T>> {};

template <class T, int Kind = has_undefined<T>::value ? 2 : (std::is_floating_point<T>::value ? 1 : 0)>
struct empty_fold { static OPTIONAL_CPP14_CONSTEXPR T get() noexcept { return T(); } };

template <class T> struct empty_fold<T, 1> { static constexpr T get() noexcept { return std::numeric_limits<T>::quiet_NaN(); } };
template <class T> struct empty_fold<T, 2> { static OPTIONAL_CPP14_CONSTEXPR T get() noexcept { return T::undefined(); } };

template <std::size_t TB, std::size_t MB, sign S>
OPTIONAL_CPP14_CONSTEXPR floatmp<TB, MB, S> min2(const floatmp<TB, MB, S>& a, const floatmp<TB, MB, S>& b) noexcept {
    using F = floatmp<TB, MB, S>;
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    return (b < a) ? b : a;
}

template <std::size_t TB, std::size_t MB, sign S>
OPTIONAL_CPP14_CONSTEXPR floatmp<TB, MB, S> max2(const floatmp<TB, MB, S>& a, const floatmp<TB, MB, S>& b) noexcept {
    using F = floatmp<TB, MB, S>;
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    return (a < b) ? b : a;
}

template <std::size_t B>
OPTIONAL_CPP14_CONSTEXPR integer<B, sign::is_signed> min2(const integer<B, sign::is_signed>& a, const integer<B, sign::is_signed>& b) noexcept {
    using I = integer<B, sign::is_signed>;
    if (a.is_undefined() || b.is_undefined()) return I::undefined();
    return (b < a) ? b : a;
}

template <std::size_t B>
OPTIONAL_CPP14_CONSTEXPR integer<B, sign::is_signed> max2(const integer<B, sign::is_signed>& a, const integer<B, sign::is_signed>& b) noexcept {
    using I = integer<B, sign::is_signed>;
    if (a.is_undefined() || b.is_undefined()) return I::undefined();
    return (a < b) ? b : a;
}

template <std::size_t B>
constexpr integer<B, sign::is_unsigned> min2(const integer<B, sign::is_unsigned>& a, const integer<B, sign::is_unsigned>& b) noexcept {
    return (b < a) ? b : a;
}

template <std::size_t B>
constexpr integer<B, sign::is_unsigned> max2(const integer<B, sign::is_unsigned>& a, const integer<B, sign::is_unsigned>& b) noexcept {
    return (a < b) ? b : a;
}

template <class T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
min2(T a, T b) noexcept { return (b < a) ? b : a; }

template <class T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
max2(T a, T b) noexcept { return (a < b) ? b : a; }

template <class T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
min2(T a, T b) noexcept {
    return (a != a || b != b) ? std::numeric_limits<T>::quiet_NaN() : ((b < a) ? b : a);
}

template <class T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
max2(T a, T b) noexcept {
    return (a != a || b != b) ? std::numeric_limits<T>::quiet_NaN() : ((a < b) ? b : a);
}

} // namespace mmdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_ABS_MIN_MAX_DETAIL_HPP