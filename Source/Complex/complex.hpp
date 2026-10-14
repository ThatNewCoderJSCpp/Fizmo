#ifndef FIZMO_COMPLEX_HPP
#define FIZMO_COMPLEX_HPP

#include "../Scalar/scalar.hpp"

#include <complex>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>

namespace fizmo {

template <class T = double>
class Complex;

template <class T>
struct is_complex : std::false_type {};

template <class T>
struct is_complex<Complex<T>> : std::true_type {};

template <class T>
OPTIONAL_CPP17_INLINE constexpr bool is_complex_v = is_complex<T>::value;

namespace complex_detail {

using multiprecision::BigComplex;
using multiprecision::BigComplexContext;
using scalar::BigFloat;
using scalar::BigFloatContext;

template <class T>
BigComplex to_big(const T& re, const T& im) { return BigComplex(BigFloat(re), BigFloat(im)); }

template <class T>
bool round_part(const BigFloat& v, std::size_t w, T& out) {
    return scalar::detail::round_checked(scalar::Target<T>(), v, w, out);
}

template <class T, class F>
void evaluate(F&& f, T& re, T& im, bool composite) {
    const std::size_t p = scalar::digits<T>();
    std::size_t w = p + 32;
    BigComplex v;
    T pr = T(0), pi = T(0);
    bool have_prev = false;

    for (int round = 0; round < 6; ++round) {
        v = f(BigComplexContext(w));

        if (v.is_unordered()) {
            re = scalar::nan<T>();
            im = scalar::nan<T>();
            return;
        }

        T r, i;
        const bool ok = round_part(v.real(), w, r) && round_part(v.imaginary(), w, i);

        if (ok && (!composite || (have_prev && scalar::identical(r, pr) && scalar::identical(i, pi)))) {
            re = r;
            im = i;
            return;
        }

        if (ok) {
            pr = r;
            pi = i;
            have_prev = true;
        }

        w *= 2;
    }

    re = scalar::from_big<T>(v.real());
    im = scalar::from_big<T>(v.imaginary());
}

} // namespace complex_detail

template <class T>
class Complex {
    static_assert(scalar::is_real<T>::value, "Complex<T> needs a standard floating-point type or a signed fixed-width float");

private:
    using BigFloat = scalar::BigFloat;

    T m_real;
    T m_imaginary;

    template <class U>
    static T convert(const U& v) { return scalar::from_big<T>(scalar::BigFloat(v)); }

public:
    using value_type = T;

    Complex() : m_real(T(0)), m_imaginary(T(0)) {}
    Complex(const T& re) : m_real(re), m_imaginary(T(0)) {}
    Complex(const T& re, const T& im) : m_real(re), m_imaginary(im) {}

    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    Complex(const U& re) : m_real(convert(re)), m_imaginary(T(0)) {}

    template <class U, class V, typename std::enable_if<std::is_arithmetic<U>::value && std::is_arithmetic<V>::value && !(std::is_same<U, T>::value && std::is_same<V, T>::value), int>::type = 0>
    Complex(const U& re, const V& im) : m_real(convert(re)), m_imaginary(convert(im)) {}

    template <class U, typename std::enable_if<!std::is_same<U, T>::value, int>::type = 0>
    explicit Complex(const Complex<U>& o) : m_real(convert(o.real())), m_imaginary(convert(o.imaginary())) {}

    template <class U, typename std::enable_if<std::is_floating_point<U>::value, int>::type = 0>
    Complex(const std::complex<U>& c) : m_real(convert(c.real())), m_imaginary(convert(c.imag())) {}

    explicit Complex(const multiprecision::BigComplex& z) {
        if (z.is_unordered()) {
            m_real = scalar::nan<T>();
            m_imaginary = scalar::nan<T>();
            return;
        }

        m_real = scalar::from_big<T>(z.real());
        m_imaginary = scalar::from_big<T>(z.imaginary());
    }

    static Complex zero() { return Complex(); }
    static Complex one() { return Complex(T(1), T(0)); }
    static Complex i() { return Complex(T(0), T(1)); }
    static Complex imaginary_unit() { return i(); }
    static Complex nan() { return Complex(scalar::nan<T>(), scalar::nan<T>()); }
    static Complex infinity() { return Complex(scalar::infinity<T>(), T(0)); }

    static Complex polar(const T& r, const T& theta) {
        if (!scalar::is_finite(r) || !scalar::is_finite(theta)) return Complex(r * scalar::cos(theta), r * scalar::sin(theta));
        const scalar::BigFloat br(r), bt(theta);
        T re, im;

        complex_detail::evaluate<T>([&](const multiprecision::BigComplexContext& c) {
            const scalar::BigFloatContext w = c.scalar();
            return multiprecision::BigComplex(BigFloat::mul(br, math::cos(bt, w), c.real()), BigFloat::mul(br, math::sin(bt, w), c.imaginary()));
        }, re, im, true);

        return Complex(re, im);
    }

    const T& real() const noexcept { return m_real; }
    T&       real() noexcept { return m_real; }
    const T& imaginary() const noexcept { return m_imaginary; }
    T&       imaginary() noexcept { return m_imaginary; }
    const T& imag() const noexcept { return m_imaginary; }
    T&       imag() noexcept { return m_imaginary; }

    multiprecision::BigComplex to_big() const { return complex_detail::to_big(m_real, m_imaginary); }

    template <class U = T, typename std::enable_if<std::is_floating_point<U>::value, int>::type = 0>
    std::complex<U> to_std() const { return std::complex<U>(m_real, m_imaginary); }

    template <class U = T, typename std::enable_if<std::is_floating_point<U>::value, int>::type = 0>
    explicit operator std::complex<U>() const { return to_std<U>(); }

    bool is_nan() const { return scalar::is_nan(m_real) || scalar::is_nan(m_imaginary); }
    bool is_infinite() const { return !is_nan() && (scalar::is_inf(m_real) || scalar::is_inf(m_imaginary)); }
    bool is_finite() const { return scalar::is_finite(m_real) && scalar::is_finite(m_imaginary); }
    bool is_zero() const { return m_real == T(0) && m_imaginary == T(0); }
    bool is_real() const { return m_imaginary == T(0); }
    bool is_imaginary() const { return m_real == T(0) && m_imaginary != T(0); }

    T magnitude_squared() const { return scalar::sum_of_products(m_real, m_real, m_imaginary, m_imaginary); }

    T magnitude() const {
        if (m_imaginary == T(0)) return scalar::abs(m_real);
        if (m_real == T(0)) return scalar::abs(m_imaginary);
        return scalar::hypot(m_real, m_imaginary);
    }

    T abs() const { return magnitude(); }

    T argument(std::int64_t branch = 0) const {
        if (is_nan()) return scalar::nan<T>();

        if (!is_finite()) {
            const T base = scalar::atan2(m_imaginary, m_real);
            if (branch == 0) return base;
            return scalar::evaluate<T>([&](const scalar::BigFloatContext& c) {
                const scalar::BigFloatContext w = scalar::context(c.precision + 8);
                return BigFloat::add(BigFloat(base), BigFloat::mul(BigFloat(2 * branch), multiprecision::constants::pi(w), w), c);
            });
        }

        const multiprecision::BigComplex z = to_big();
        return scalar::evaluate<T>([&](const scalar::BigFloatContext& c) { return z.argument(branch, c); });
    }

    Complex conjugate() const { return Complex(m_real, -m_imaginary); }
    Complex& conjugate_mutable() { m_imaginary = -m_imaginary; return *this; }
    Complex negated() const { return Complex(-m_real, -m_imaginary); }
    Complex& negate_mutable() { m_real = -m_real; m_imaginary = -m_imaginary; return *this; }
    Complex mul_i() const { return Complex(-m_imaginary, m_real); }
    Complex div_i() const { return Complex(m_imaginary, -m_real); }

    Complex reciprocal() const { return Complex::one() / *this; }
    Complex& reciprocal_mutable() { return *this = reciprocal(); }
    Complex square() const { return *this * *this; }
    Complex& square_mutable() { return *this = square(); }

    Complex operator+() const { return *this; }
    Complex operator-() const { return negated(); }

    Complex& operator+=(const Complex& o) { m_real = m_real + o.m_real; m_imaginary = m_imaginary + o.m_imaginary; return *this; }
    Complex& operator-=(const Complex& o) { m_real = m_real - o.m_real; m_imaginary = m_imaginary - o.m_imaginary; return *this; }
    Complex& operator*=(const Complex& o) { return *this = *this * o; }
    Complex& operator/=(const Complex& o) { return *this = *this / o; }
    Complex& operator+=(const T& s) { m_real = m_real + s; return *this; }
    Complex& operator-=(const T& s) { m_real = m_real - s; return *this; }
    Complex& operator*=(const T& s) { m_real = m_real * s; m_imaginary = m_imaginary * s; return *this; }
    Complex& operator/=(const T& s) { m_real = m_real / s; m_imaginary = m_imaginary / s; return *this; }

    friend Complex operator+(const Complex& a, const Complex& b) { Complex r(a); r += b; return r; }
    friend Complex operator-(const Complex& a, const Complex& b) { Complex r(a); r -= b; return r; }
    friend Complex operator+(const Complex& a, const T& s) { Complex r(a); r += s; return r; }
    friend Complex operator+(const T& s, const Complex& a) { Complex r(a); r += s; return r; }
    friend Complex operator-(const Complex& a, const T& s) { Complex r(a); r -= s; return r; }
    friend Complex operator-(const T& s, const Complex& a) { return Complex(s - a.m_real, -a.m_imaginary); }
    friend Complex operator*(const Complex& a, const T& s) { Complex r(a); r *= s; return r; }
    friend Complex operator*(const T& s, const Complex& a) { Complex r(a); r *= s; return r; }
    friend Complex operator/(const Complex& a, const T& s) { Complex r(a); r /= s; return r; }

    friend Complex operator*(const Complex& z, const Complex& w) {
        if (z.m_imaginary == T(0)) return Complex(z.m_real * w.m_real, z.m_real * w.m_imaginary);
        if (w.m_imaginary == T(0)) return Complex(z.m_real * w.m_real, z.m_imaginary * w.m_real);

        return Complex(
            scalar::diff_of_products(z.m_real, w.m_real, z.m_imaginary, w.m_imaginary),
            scalar::sum_of_products(z.m_real, w.m_imaginary, z.m_imaginary, w.m_real)
        );
    }

    friend Complex operator/(const Complex& z, const Complex& w) {
        const T& a = z.m_real;
        const T& b = z.m_imaginary;
        const T& c = w.m_real;
        const T& d = w.m_imaginary;
        if (d == T(0)) return Complex(a / c, b / c);
        if (c == T(0)) return Complex(b / d, -(a / d));
        if (!z.is_finite() || !w.is_finite()) return Complex::nan();

        const BigFloat A(a), B(b), C(c), D(d);
        const BigFloat den = scalar::exact_add(scalar::exact_mul(C, C), scalar::exact_mul(D, D));
        const BigFloat nr = scalar::exact_add(scalar::exact_mul(A, C), scalar::exact_mul(B, D));
        const BigFloat ni = scalar::exact_sub(scalar::exact_mul(B, C), scalar::exact_mul(A, D));
        const T re = nr.is_zero() ? T(0) : scalar::evaluate<T>([&](const scalar::BigFloatContext& ctx) { return BigFloat::div(nr, den, ctx); });
        const T im = ni.is_zero() ? T(0) : scalar::evaluate<T>([&](const scalar::BigFloatContext& ctx) { return BigFloat::div(ni, den, ctx); });
        return Complex(re, im);
    }

    friend Complex operator/(const T& s, const Complex& w) { return Complex(s) / w; }

    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator+(const Complex& a, const U& s) { return a + convert(s); }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator+(const U& s, const Complex& a) { return convert(s) + a; }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator-(const Complex& a, const U& s) { return a - convert(s); }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator-(const U& s, const Complex& a) { return convert(s) - a; }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator*(const Complex& a, const U& s) { return a * convert(s); }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator*(const U& s, const Complex& a) { return convert(s) * a; }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator/(const Complex& a, const U& s) { return a / convert(s); }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend Complex operator/(const U& s, const Complex& a) { return convert(s) / a; }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend bool operator==(const Complex& a, const U& s) { return a == convert(s); }
    template <class U, typename std::enable_if<std::is_arithmetic<U>::value && !std::is_same<U, T>::value, int>::type = 0>
    friend bool operator!=(const Complex& a, const U& s) { return a != convert(s); }

    friend bool operator==(const Complex& a, const Complex& b) { return a.m_real == b.m_real && a.m_imaginary == b.m_imaginary; }
    friend bool operator!=(const Complex& a, const Complex& b) { return !(a == b); }
    friend bool operator==(const Complex& a, const T& s) { return a.m_real == s && a.m_imaginary == T(0); }
    friend bool operator!=(const Complex& a, const T& s) { return !(a == s); }

    bool approx_equal(const Complex& o, const T& eps) const {
        return scalar::abs(m_real - o.m_real) <= eps && scalar::abs(m_imaginary - o.m_imaginary) <= eps;
    }

    std::string to_string() const {
        if (is_nan()) return "nan";
        const bool neg_im = scalar::signbit(m_imaginary);
        const T mag_im = neg_im ? -m_imaginary : m_imaginary;
        if (m_imaginary == T(0)) return scalar::to_string(m_real);
        if (m_real == T(0)) return (neg_im ? "-" : "") + scalar::to_string(mag_im) + "i";
        return scalar::to_string(m_real) + (neg_im ? " - " : " + ") + scalar::to_string(mag_im) + "i";
    }

    friend std::ostream& operator<<(std::ostream& os, const Complex& z) { return os << z.to_string(); }
};

using ComplexF = Complex<float>;
using ComplexD = Complex<double>;
using ComplexL = Complex<long double>;
using Complex32 = Complex<multiprecision::float32>;
using Complex64 = Complex<multiprecision::float64>;
using Complex128 = Complex<multiprecision::float128>;

} // namespace fizmo

#endif // FIZMO_COMPLEX_HPP
