#ifndef FIZMO_MULTIPRECISION_ABS_MIN_MAX_HPP
#define FIZMO_MULTIPRECISION_ABS_MIN_MAX_HPP

#include <initializer_list>
#include "basic_constants.hpp"
#include "Details/min_max_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

template <std::size_t TB, std::size_t MB>
OPTIONAL_CPP14_CONSTEXPR floatmp<TB, MB, sign::is_signed> abs(const floatmp<TB, MB, sign::is_signed>& x) noexcept {
    using F = floatmp<TB, MB, sign::is_signed>;
    if (x.is_nan())       return F::nan();
    if (x.is_undefined()) return F::undefined();
    return x.is_negative() ? -x : x;
}

template <std::size_t TB, std::size_t MB>
constexpr floatmp<TB, MB, sign::is_unsigned> abs(const floatmp<TB, MB, sign::is_unsigned>& x) noexcept {
    return x;
}

template <std::size_t Bits>
OPTIONAL_CPP14_CONSTEXPR integer<Bits, sign::is_signed> abs(const integer<Bits, sign::is_signed>& x) noexcept {
    if (x.is_undefined()) return integer<Bits, sign::is_signed>::undefined();
    return x.is_negative() ? -x : x;
}

template <std::size_t Bits>
constexpr integer<Bits, sign::is_unsigned> abs(const integer<Bits, sign::is_unsigned>& x) noexcept {
    return x; 
}

template <class T>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, T>::type
abs(T x) noexcept { return x; }

template <class T>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, T>::type
abs(T x) noexcept { return (x < T(0)) ? static_cast<T>(-x) : x; }

template <class T>
constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
abs(T x) noexcept { return (x == T(0)) ? T(0) : ((x < T(0)) ? -x : x); }

template <class F>
constexpr typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
min(const F& x) noexcept { return x; }

template <class F>
constexpr typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
max(const F& x) noexcept { return x; }

template <class A, class B>
OPTIONAL_CPP14_CONSTEXPR typename mmdetail::common2<A, B>::type
min(const A& a, const B& b) noexcept {
    using C = typename mmdetail::common2<A, B>::type;
    return mmdetail::min2(C(a), C(b));
}

template <class A, class B>
OPTIONAL_CPP14_CONSTEXPR typename mmdetail::common2<A, B>::type
max(const A& a, const B& b) noexcept {
    using C = typename mmdetail::common2<A, B>::type;
    return mmdetail::max2(C(a), C(b));
}

template <class A, class B, class C0, class... R>
OPTIONAL_CPP14_CONSTEXPR typename mmdetail::common_n<A, B, C0, R...>::type
min(const A& a, const B& b, const C0& c, const R&... rest) noexcept {
    using C = typename mmdetail::common2<A, B>::type;
    return math::min(mmdetail::min2(C(a), C(b)), c, rest...);
}

template <class A, class B, class C0, class... R>
OPTIONAL_CPP14_CONSTEXPR typename mmdetail::common_n<A, B, C0, R...>::type
max(const A& a, const B& b, const C0& c, const R&... rest) noexcept {
    using C = typename mmdetail::common2<A, B>::type;
    return math::max(mmdetail::max2(C(a), C(b)), c, rest...);
}

template <class F>
OPTIONAL_CPP14_CONSTEXPR typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
min(std::initializer_list<F> xs) noexcept {
    if (xs.size() == 0) return mmdetail::empty_fold<F>::get();
    typename std::initializer_list<F>::const_iterator it = xs.begin();
    F r = *it;
    for (++it; it != xs.end(); ++it) r = mmdetail::min2(r, *it);
    return r;
}

template <class F>
OPTIONAL_CPP14_CONSTEXPR typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
max(std::initializer_list<F> xs) noexcept {
    if (xs.size() == 0) return mmdetail::empty_fold<F>::get();
    typename std::initializer_list<F>::const_iterator it = xs.begin();
    F r = *it;
    for (++it; it != xs.end(); ++it) r = mmdetail::max2(r, *it);
    return r;
}

template <class F>
OPTIONAL_CPP14_CONSTEXPR
typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
clamp(const F& x, const F& lo, const F& hi) noexcept {
    return mmdetail::min2(mmdetail::max2(x, lo), hi);
}

template <class A, class B, class C>
OPTIONAL_CPP14_CONSTEXPR typename mmdetail::common_n<A, B, C>::type
clamp(const A& x, const B& lo, const C& hi) noexcept {
    using R = typename mmdetail::common_n<A, B, C>::type;
    R xr = R(x);
    R lor = R(lo);
    R hir = R(hi);
    return mmdetail::min2(mmdetail::max2(xr, lor), hir);
}

template <class F>
OPTIONAL_CPP14_CONSTEXPR
typename std::enable_if<mmdetail::is_operand<F>::value, F>::type
clamp(std::initializer_list<F> xs, const F& lo, const F& hi) noexcept {
    if (xs.size() == 0) return mmdetail::empty_fold<F>::get();
    typename std::initializer_list<F>::const_iterator it = xs.begin();
    F r = mmdetail::min2(mmdetail::max2(*it, lo), hi);
    for (++it; it != xs.end(); ++it) r = mmdetail::min2(mmdetail::max2(*it, lo), hi);
    return r;
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_ABS_MIN_MAX_HPP