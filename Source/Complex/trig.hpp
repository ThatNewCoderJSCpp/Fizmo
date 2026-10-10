#ifndef COMPLEX_CLASS_MATH_TRIG_IMPL_HPP
#define COMPLEX_CLASS_MATH_TRIG_IMPL_HPP

#include "util.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
BasicComplex<T> sin(const BasicComplex<T>& c) noexcept {
    return BasicComplex(
        std::sin(c.real()) * std::cosh(c.imaginary()),
        std::cos(c.real()) * std::sinh(c.imaginary())
    );
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> sin(T r) noexcept { return BasicComplex<T>(std::sin(r), T(0)); }

template <typename T>
BasicComplex<T> cos(const BasicComplex<T>& c) noexcept {
    return BasicComplex(
        std::cos(c.real()) * std::cosh(c.imaginary()),
        -std::sin(c.real()) * std::sinh(c.imaginary())
    );
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> cos(T r) noexcept { return BasicComplex<T>(std::cos(r), T(0)); }

template <typename T>
BasicComplex<T> tan(const BasicComplex<T>& c) noexcept {
    BasicComplex<T> cv = cos(c);
    if (cv.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    cv.reciprocal_inplace();
    return sin(c) * cv;
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> tan(T r) noexcept { return BasicComplex<T>(std::tan(r), T(0)); }

template <typename T>
BasicComplex<T> csc(const BasicComplex<T>& c) noexcept {
    BasicComplex<T> sv = sin(c);
    if (sv.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    sv.reciprocal_inplace();
    return sv;
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> csc(T r) noexcept { return csc(BasicComplex<T>(r, T(0))); }

template <typename T>
BasicComplex<T> sec(const BasicComplex<T>& c) noexcept {
    BasicComplex<T> cv = cos(c);
    if (cv.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    cv.reciprocal_inplace();
    return cv;
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> sec(T r) noexcept { return sec(BasicComplex<T>(r, T(0))); }

template <typename T>
BasicComplex<T> cot(const BasicComplex<T>& c) noexcept {
    BasicComplex<T> tv = tan(c);
    if (tv.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    tv.reciprocal_inplace();
    return tv;
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> cot(T r) noexcept { return cot(BasicComplex<T>(r, T(0))); }

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_TRIG_IMPL_HPP