#ifndef COMPLEX_CLASS_MATH_INV_HTRIG_IMPL_HPP
#define COMPLEX_CLASS_MATH_INV_HTRIG_IMPL_HPP

#include "log_pow_root.hpp"
#include "htrig.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
BasicComplex<T> asinh(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> one(T(1), T(0));
    BasicComplex<T> root = sqrt(z * z + one, n);
    return ln(z + root, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> asinh(T r, int n = 0) noexcept { return asinh(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acosh(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> one(T(1), T(0));
    BasicComplex<T> r1 = sqrt(z + one, n);
    BasicComplex<T> r2 = sqrt(z - one, n);
    return ln(z + r1 * r2, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acosh(T r, int n = 0) noexcept { return acosh(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> atanh(const BasicComplex<T>& z, int n = 0) noexcept {
    const BasicComplex<T> one(T(1), T(0));
    BasicComplex<T> num = one + z;
    BasicComplex<T> den = one - z;
    if (den.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    return ln(num / den, n) * T(0.5);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> atanh(T r, int n = 0) noexcept { return atanh(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acsch(const BasicComplex<T>& z, int n = 0) noexcept {
    if (z.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    BasicComplex<T> r = z;
    r.reciprocal_inplace();
    return asinh(r, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acsch(T r, int n = 0) noexcept { return acsch(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> asech(const BasicComplex<T>& z, int n = 0) noexcept {
    if (z.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    BasicComplex<T> r = z;
    r.reciprocal_inplace();
    return acosh(r, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> asech(T r, int n = 0) noexcept { return asech(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> acoth(const BasicComplex<T>& z, int n = 0) noexcept {
    if (z.magnitude_squared() == T(0)) return BasicComplex<T>::error_unit();
    BasicComplex<T> r = z;
    r.reciprocal_inplace();
    return atanh(r, n);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> acoth(T r, int n = 0) noexcept { return acoth(BasicComplex<T>(r, T(0)), n); }

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_INV_HTRIG_IMPL_HPP