#ifndef COMPLEX_CLASS_MATH_INV_TRIG_IMPL_HPP
#define COMPLEX_CLASS_MATH_INV_TRIG_IMPL_HPP

#include "log_pow_root.hpp"
#include "trig.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
BasicComplex<T> asin(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> i(0, 1);
    const BasicComplex<T> one(T(1), T(0));
    BasicComplex<T> root = sqrt(one - z * z, n);
    BasicComplex<T> inside = i * z + root;
    return -i * ln(inside, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> asin(T r, int n = 0) noexcept { return asin(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acos(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> half_pi(constants::PI_2<T>, T(0));
    return half_pi - asin(z, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acos(T r, int n = 0) noexcept { return acos(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> atan(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> i(0, 1);
    const BasicComplex<T> half(T(0.5), T(0));
    BasicComplex<T> num = i + z;
    BasicComplex<T> den = i - z;
    if (den.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    den.reciprocal_inplace();
    BasicComplex<T> ratio = num;
    ratio *= den;
    return i * half * ln(ratio, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> atan(T r, int n = 0) noexcept { return atan(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acsc(const BasicComplex<T>& z, int n = 0) noexcept {
    if (z.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    BasicComplex<T> r = z;
    r.reciprocal_inplace();
    return asin(r, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acsc(T r, int n = 0) noexcept { return acsc(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> asec(const BasicComplex<T>& z, int n = 0) noexcept {
    if (z.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    BasicComplex<T> r = z;
    r.reciprocal_inplace();
    return acos(r, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> asec(T r, int n = 0) noexcept { return asec(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acot(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> half_pi(constants::PI_2<T>, T(0));
    return half_pi - atan(z, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acot(T r, int n = 0) noexcept { return acot(BasicComplex<T>(r, T(0)), n); }

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_INV_TRIG_IMPL_HPP