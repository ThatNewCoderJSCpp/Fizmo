#ifndef COMPLEX_CLASS_MATH_LOG_POW_ROOT_IMPL_HPP
#define COMPLEX_CLASS_MATH_LOG_POW_ROOT_IMPL_HPP

#include "trig.hpp"

namespace fizmo {
namespace math {
namespace cx {

template <typename T>
constexpr BasicComplex<T> ln(const BasicComplex<T>& c, int n = 0) noexcept { return BasicComplex(T(0.5) * log_constexpr(c.magnitude_squared()), c.argument(n)); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr BasicComplex<T> ln(T r, int n = 0) noexcept { return ln(BasicComplex<T>(r, T(0)), n); }

template <typename T>
constexpr BasicComplex<T> log10(const BasicComplex<T>& c, int n = 0) noexcept { return ln(c, n) / log_constexpr(10); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr BasicComplex<T> log10(T r, int n = 0) noexcept { return log10(BasicComplex<T>(r, T(0)), n); }

template <typename T>
constexpr BasicComplex<T> log2(const BasicComplex<T>& c, int n = 0) noexcept { return ln(c, n) / log_constexpr(2); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
constexpr BasicComplex<T> log2(T r, int n = 0) noexcept { return log2(BasicComplex<T>(r, T(0)), n); }

template <typename A, typename B>
constexpr BasicComplex<typename std::common_type<A, B>::type> log(const BasicComplex<A>& arg, const BasicComplex<B>& base, int n = 0) noexcept { 
    using CT = typename std::common_type<A, B>::type;
    return ln(BasicComplex<CT>(arg), n) / ln(BasicComplex<CT>(base), n);
}

template <typename T>
BasicComplex<T> exp(const BasicComplex<T>& c) noexcept { 
    const T mult = std::exp(c.real());
    return BasicComplex(mult * std::cos(c.imaginary()), mult * std::sin(c.imaginary())); 
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> exp(T r) noexcept { return BasicComplex<T>(std::exp(r), T(0)); }

template <typename T>
BasicComplex<T> pow2(const BasicComplex<T>& c) noexcept { return exp(c * std::log(T(2))); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> pow2(T r, int n) noexcept { return BasicComplex<T>(std::pow(T(2), r), T(0)); }

template <typename T>
BasicComplex<T> pow10(const BasicComplex<T>& c) noexcept { return exp(c * std::log(T(2))); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> pow10(T r) noexcept { return BasicComplex<T>(std::pow(T(10), r), T(0)); }

template <typename T>
BasicComplex<T> neg_one_pow(const BasicComplex<T>& z, int n = 0) noexcept {
    const T pi = constants::PI<T>;
    BasicComplex<T> i(0, 1);
    BasicComplex<T> factor = i * ((T(2) * T(n) + T(1)) * pi);
    return exp(z * factor);
}

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> neg_one_pow(T r, int n = 0) noexcept { return neg_one_pow(BasicComplex<T>(r, T(0)), n); }

template <typename B, typename P>
BasicComplex<typename std::common_type<B, P>::type> 
pow(const BasicComplex<B>& base, const BasicComplex<P>& power, int n = 0) noexcept {
    using CT = typename std::common_type<B, P>::type;
    const BasicComplex<CT> b(base);
    const BasicComplex<CT> p(power);
    const BasicComplex<CT> logb = ln(b, n);
    return exp(p * logb);
}

template <typename B, typename P, typename = typename std::enable_if<std::is_floating_point<P>::value>::type>
constexpr BasicComplex<typename std::common_type<B,P>::type>
pow(const BasicComplex<B>& base, P power, int n = 0) noexcept {
    using CT = typename std::common_type<B,P>::type;
    return exp(BasicComplex<CT>(power, CT(0)) * ln(BasicComplex<CT>(base), n));
}

template <typename B, typename P, typename = typename std::enable_if<std::is_floating_point<B>::value>::type>
constexpr BasicComplex<typename std::common_type<B,P>::type>
pow(B base, const BasicComplex<P>& power, int n = 0) noexcept {
    using CT = typename std::common_type<B,P>::type;
    return exp(BasicComplex<CT>(power) * ln(BasicComplex<CT>(base, CT(0)), n));
}

template <typename B, typename P, typename = typename std::enable_if<std::is_floating_point<B>::value && std::is_floating_point<P>::value>::type>
constexpr BasicComplex<typename std::common_type<B,P>::type>
pow(B base, P power, int n = 0) noexcept {
    using CT = typename std::common_type<B,P>::type;
    return exp(BasicComplex<CT>(power, CT(0)) * ln(BasicComplex<CT>(base, CT(0)), n));
}

template <typename T>
BasicComplex<T> sqrt(const BasicComplex<T>& z, int n = 0) noexcept { return exp(ln(z, n) * T(0.5)); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> sqrt(T r, int n = 0) noexcept { return sqrt(BasicComplex<T>(r, T(0)), n); }

template <typename T>
BasicComplex<T> cbrt(const BasicComplex<T>& z, int n = 0) noexcept { return exp(ln(z, n) * (T(1) / T(3))); }

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
BasicComplex<T> cbrt(T r, int n = 0) noexcept { return cbrt(BasicComplex<T>(r, T(0)), n); }

template <typename Z, typename W>
BasicComplex<typename std::common_type<Z, W>::type> 
nth_root(const BasicComplex<Z>& z, const BasicComplex<W>& w, int n = 0) noexcept {
    using CT = typename std::common_type<Z, W>::type;
    const BasicComplex<CT> Zc(z);
    BasicComplex<CT> Wc(w);
    Wc.reciprocal_inplace();
    return exp(Wc * ln(Zc, n));
}

template <typename Z, typename W, typename = typename std::enable_if<std::is_floating_point<W>::value>::type>
constexpr BasicComplex<typename std::common_type<Z,W>::type>
nth_root(const BasicComplex<Z>& z, W w, int n = 0) noexcept {
    using CT = typename std::common_type<Z,W>::type;
    BasicComplex<CT> Wc(w, CT(0));
    Wc.reciprocal_inplace();
    return exp(Wc * ln(BasicComplex<CT>(z), n));
}

template <typename Z, typename W, typename = typename std::enable_if<std::is_floating_point<Z>::value>::type>
constexpr BasicComplex<typename std::common_type<Z,W>::type>
nth_root(Z z, const BasicComplex<W>& w, int n = 0) noexcept {
    using CT = typename std::common_type<Z,W>::type;
    BasicComplex<CT> Wc(w);
    Wc.reciprocal_inplace();
    return exp(Wc * ln(BasicComplex<CT>(z, CT(0)), n));
}

template <typename Z, typename W, typename = typename std::enable_if<std::is_floating_point<Z>::value && std::is_floating_point<W>::value>::type>
constexpr BasicComplex<typename std::common_type<Z,W>::type>
nth_root(Z z, W w, int n = 0) noexcept {
    using CT = typename std::common_type<Z,W>::type;
    BasicComplex<CT> Wc(w, CT(0));
    Wc.reciprocal_inplace();
    return exp(Wc * ln(BasicComplex<CT>(z, CT(0)), n));
}

} // namespace cx
} // namespace math
} // namespace fizmo

#endif // COMPLEX_CLASS_MATH_LOG_POW_ROOT_IMPL_HPP