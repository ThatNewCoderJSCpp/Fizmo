#ifndef COMPLEX_CLASS_MATH_UTIL_IMPL_HPP
#define COMPLEX_CLASS_MATH_UTIL_IMPL_HPP

#include "complex_class.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
constexpr T argument(const BasicComplex<T>& c, int n = 0) noexcept { return c.argument(n); }

template <typename T>
constexpr T magnitude(const BasicComplex<T>& c) noexcept { return c.magnitude(); }

template <typename T>
constexpr T real(const BasicComplex<T>& c) noexcept { return c.real(); }

template <typename T>
constexpr T imaginary(const BasicComplex<T>& c) noexcept { return c.imaginary(); }

template <typename T>
constexpr T conjugate(const BasicComplex<T>& c) noexcept { return c.conjugate(); }

template <typename T>
constexpr T reciprocal(const BasicComplex<T>& c) noexcept { return c.reciprocal(); }

template <typename T>
BasicComplex<T> unity_root(std::size_t n, std::size_t k) noexcept {
    if (n == 0) { return BasicComplex<T>::error_unit(); }
    const T two_pi = T(2) * constants::PI<T>;
    T angle = two_pi * T(k % n) / T(n);
    return BasicComplex<T>(std::cos(angle), std::sin(angle));
}

template <typename T>
std::vector<BasicComplex<T>> unity_roots(std::size_t n) {
    std::vector<BasicComplex<T>> roots;
    roots.reserve(n);
    for (std::size_t k = 0; k < n; ++k) { roots.emplace_back(unity_root<T>(n, k)); }
    return roots;
}

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_UTIL_IMPL_HPP