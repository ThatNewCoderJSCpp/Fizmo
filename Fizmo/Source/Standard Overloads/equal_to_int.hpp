#ifndef EQUAL_TO_AND_IS_INTEGER_HPP
#define EQUAL_TO_AND_IS_INTEGER_HPP

#include "num_theory.hpp"
#include "../Basic/constants.hpp"

namespace fizmo {

template <typename A, typename B, typename R1 = double, typename R2 = double>
constexpr typename std::enable_if<std::is_arithmetic<A>::value && std::is_arithmetic<B>::value && std::is_floating_point<R1>::value && std::is_floating_point<R2>::value, bool>::type 
are_approximately_equal(const A a, const B b, const R1 abs_tol = fizmo::constants::TYPE_EPSILON<R1>, const R2 rel_tol = R2(100) * fizmo::constants::TYPE_EPSILON<R2>) noexcept {
    using CT = typename std::common_type<A, B, R1, R2, double>::type;
    const CT va = static_cast<CT>(a);
    const CT vb = static_cast<CT>(b);
    const CT at = max_constexpr(fizmo::constants::TYPE_EPSILON<CT>, static_cast<CT>(abs_tol));
    const CT rt = max_constexpr(fizmo::constants::TYPE_EPSILON<CT>, static_cast<CT>(rel_tol));
    const CT diff = abs_constexpr(va - vb);
    if (diff <= at) return true;
    return diff <= rt * max_constexpr(abs_constexpr(va), abs_constexpr(vb));
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type
is_integer(const T x) noexcept { return true; } 

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_integer(const T x) noexcept { 
    const T rounded = fizmo::round_constexpr(x);
    return are_approximately_equal(x, rounded);
} 

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type
is_negative_integer(const T x) noexcept { return x < T(0); } 

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_negative_integer(const T x) noexcept { return x < T(0) && is_integer(x); } 

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type
is_positive_integer(const T x) noexcept { return x > T(0); } 

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_positive_integer(const T x) noexcept { return x > T(0) && is_integer(x); } 

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type
is_even(const T x) noexcept { return (x & 1) == 0; }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_even(const T x) noexcept {
    if (!is_integer(x)) return false;
    const T half = x * T(0.5);
    return are_approximately_equal(half, fizmo::round_constexpr(half));
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type
is_odd(const T x) noexcept { return (x & 1) == 1; }

template <typename T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, bool>::type
is_odd(const T x) noexcept {
    if (!is_integer(x)) return false;
    return !is_even(x);
}

template <
    typename AT, typename RT, typename PV, typename CV,
    typename = typename std::enable_if<
        std::is_floating_point<AT>::value &&
        std::is_floating_point<RT>::value &&
        std::is_arithmetic<PV>::value &&
        std::is_arithmetic<CV>::value
    >::type
>
constexpr bool values_do_converge(const PV previous_value, const CV current_value, const RT relative_tolerance, const AT absolute_tolerance) noexcept {
    using CT = typename std::common_type<PV, CV, AT, RT, double>::type;

    return are_approximately_equal(
        static_cast<CT>(previous_value), static_cast<CT>(current_value),
        static_cast<CT>(absolute_tolerance), static_cast<CT>(relative_tolerance)
    );
}

template <
    typename T, typename PV, typename CV,
    typename = typename std::enable_if<
        std::is_floating_point<T>::value &&
        std::is_arithmetic<PV>::value &&
        std::is_arithmetic<CV>::value
    >::type
>
constexpr bool values_do_converge(const PV previous_value, const CV current_value, const T tolerance) noexcept {
    return values_do_converge(previous_value, current_value, tolerance, tolerance);
}

} // namespace fizmo

#endif // EQUAL_TO_AND_IS_INTEGER_HPP