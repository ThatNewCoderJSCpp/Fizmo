#ifndef COMPLEX_CLASS_MATH_HTRIG_IMPL_HPP
#define COMPLEX_CLASS_MATH_HTRIG_IMPL_HPP

#include "complex_class.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
BasicComplex<T> sinh(const BasicComplex<T>& c) noexcept { return BasicComplex(std::sinh(c.real()) * std::cos(c.imaginary()), std::cosh(c.real()) * std::sin(c.imaginary())); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> sinh(T r) noexcept { return BasicComplex<T>(std::sinh(r), T(0)); }

template <typename T>
BasicComplex<T> cosh(const BasicComplex<T>& c) noexcept { return BasicComplex(std::cosh(c.real()) * std::cos(c.imaginary()), std::sinh(c.real()) * std::sin(c.imaginary())); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> cosh(T r) noexcept { return BasicComplex<T>(std::cosh(r), T(0)); }

template <typename T>
BasicComplex<T> tanh(const BasicComplex<T>& c) noexcept { 
    BasicComplex<T> cv = cosh(c);
    if (cv.magnitude_squared() == T(0)) { return BasicComplex<T>::error_unit(); }
    cv.reciprocal_inplace();
    return sinh(c) * cv; 
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> tanh(T r) noexcept { return BasicComplex<T>(std::tanh(r), T(0)); }

template <typename T>
BasicComplex<T> csch(const BasicComplex<T>& c) noexcept { 
    BasicComplex<T> sv = sinh(c);
    if (sv.magnitude_squared() == T(0)) { return BasicComplex<T>::error_unit(); }
    sv.reciprocal_inplace();
    return sv; 
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> csch(T r) noexcept { return csch(BasicComplex<T>(r, T(0))); }

template <typename T>
BasicComplex<T> sech(const BasicComplex<T>& c) noexcept { 
    BasicComplex<T> cv = cosh(c);
    if (cv.magnitude_squared() == T(0)) { return BasicComplex<T>::error_unit(); }
    cv.reciprocal_inplace();
    return cv; 
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> sech(T r) noexcept { return sech(BasicComplex<T>(r, T(0))); }

template <typename T>
BasicComplex<T> coth(const BasicComplex<T>& c) noexcept { 
    BasicComplex<T> tv = tanh(c);
    if (tv.magnitude_squared() == T(0)) { return BasicComplex<T>::error_unit(); }
    tv.reciprocal_inplace();
    return tv; 
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> coth(T r) noexcept { return coth(BasicComplex<T>(r, T(0))); }

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_HTRIG_IMPL_HPP