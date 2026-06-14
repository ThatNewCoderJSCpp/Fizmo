#ifndef FIZMO_BASIC_COMPLEX_CLASS_HPP
#define FIZMO_BASIC_COMPLEX_CLASS_HPP

#include "../Standard Overloads/trig.hpp"
#include "../Standard Overloads/log.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "../Standard Overloads/abs.hpp"
#include <limits>

namespace fizmo {

template <typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
class BasicComplex {
private:
    T m_r;
    T m_i;

public: 
    constexpr BasicComplex() noexcept : m_r(T(0)), m_i(T(0)) {}
    constexpr BasicComplex(T real, T imaginary = T(0)) noexcept : m_r(real), m_i(imaginary) {}
    constexpr BasicComplex(const std::complex<T>& c) noexcept : m_r(c.real()), m_i(c.imag()) {}
    constexpr BasicComplex(const BasicComplex& c) noexcept = default;
    constexpr BasicComplex(BasicComplex&& c) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator=(const BasicComplex& c) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator=(BasicComplex&& c) noexcept = default;

public:
    constexpr T real() const noexcept { return m_r; }
    constexpr T& real() noexcept { return m_r; }
    constexpr T imaginary() const noexcept { return m_i; }
    constexpr T& imaginary() noexcept { return m_i; }

public:
    constexpr T magnitude_squared() const noexcept { return (m_r * m_r) + (m_i * m_i); }
    constexpr T magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr T argument(int n = 0) const noexcept { return math::atan2_constexpr(m_i, m_r) + (T(2) * static_cast<T>(n) * constants::PI<T>); }
    constexpr BasicComplex conjugate() const noexcept { return BasicComplex(m_r, -m_i); }
    OPTIONAL_CPP14_CONSTEXPR BasicComplex& conjugate_inplace() noexcept { m_i = -m_i; return *this; }

    constexpr BasicComplex reciprocal() const noexcept { 
        BasicComplex temp(*this);
        temp.reciprocal_inplace();
        return temp;
    }
    
    OPTIONAL_CPP14_CONSTEXPR BasicComplex& reciprocal_inplace() noexcept {
        const T denom = magnitude_squared();
        m_r /= denom;
        m_i /= -denom;
        return *this;
    }

public:
    constexpr bool operator==(const BasicComplex& c) const noexcept { return m_r == c.m_r && m_i == c.m_i; }
    constexpr bool operator!=(const BasicComplex& c) const noexcept { return !(*this == c); }

public:
    constexpr bool is_error() const noexcept { return m_r == std::numeric_limits<T>::quiet_NaN() || m_i == std::numeric_limits<T>::quiet_NaN(); }
    constexpr bool is_infinite() const noexcept { return m_r == std::numeric_limits<T>::infinity() || m_i == std::numeric_limits<T>::infinity(); }
    constexpr bool is_finite() const noexcept { return !is_error() && !is_infinite(); }

public:
    OPTIONAL_CPP14_CONSTEXPR BasicComplex& negate() noexcept {
        m_r = -m_r;
        m_i = -m_i;
        return *this;
    }

    constexpr BasicComplex operator-() const noexcept {
        BasicComplex temp(*this);
        temp.negate();
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator+=(const BasicComplex& c) noexcept {
        m_r += c.m_r;
        m_i += c.m_i;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator+=(T s) noexcept {
        m_r += s;
        return *this;
    }

    constexpr BasicComplex operator+(const BasicComplex& c) const noexcept {
        BasicComplex temp(*this);
        temp += c;
        return temp;
    }

    constexpr BasicComplex operator+(T s) const noexcept {
        BasicComplex temp(*this);
        temp += s;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator-=(const BasicComplex& c) noexcept {
        m_r -= c.m_r;
        m_i -= c.m_i;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator-=(T s) noexcept {
        m_r -= s;
        return *this;
    }

    constexpr BasicComplex operator-(const BasicComplex& c) const noexcept {
        BasicComplex temp(*this);
        temp -= c;
        return temp;
    }

    constexpr BasicComplex operator-(T s) const noexcept {
        BasicComplex temp(*this);
        temp -= s;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator*=(const BasicComplex& c) noexcept {
        const T a = m_r;
        const T b = m_i;
        const T c_ = c.m_r;
        const T d = c.m_i;
        m_r = (a * c_) - (b * d);
        m_i = (a * d) + (b * c_);
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator*=(T s) noexcept {
        m_r *= s;
        m_i *= s;
        return *this;
    }

    constexpr BasicComplex operator*(const BasicComplex& c) const noexcept {
        BasicComplex temp(*this);
        temp *= c;
        return temp;
    }

    constexpr BasicComplex operator*(T s) const noexcept {
        BasicComplex temp(*this);
        temp *= s;
        return temp;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator/=(const BasicComplex& c) noexcept {
        const T a = m_r;
        const T b = m_i;
        const T c_ = c.m_r;
        const T d = c.m_i;
        const T denom = c.magnitude_squared();
        m_r = (a * c_) + (b * d);
        m_r /= denom;
        m_i = (b * c_) - (a * d);
        m_i /= denom;
        return *this;
    }

    OPTIONAL_CPP14_CONSTEXPR BasicComplex& operator/=(T s) noexcept {
        m_r /= s;
        m_i /= s;
        return *this;
    }

    constexpr BasicComplex operator/(const BasicComplex& c) const noexcept {
        BasicComplex temp(*this);
        temp /= c;
        return temp;
    }

    constexpr BasicComplex operator/(T s) const noexcept {
        BasicComplex temp(*this);
        temp /= s;
        return temp;
    }

public:
    constexpr static BasicComplex imaginary_unit() noexcept { return BasicComplex(T(0), T(1)); }
    constexpr static BasicComplex error_unit() noexcept { return BasicComplex(std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::quiet_NaN()); }

public:
    friend std::ostream& operator<<(std::ostream& os, const BasicComplex& c) {
        const T r = c.m_r;
        const T i = c.m_i;
        const bool r_zero = abs_constexpr(r) <= constants::TYPE_EPSILON<T> * T(100);
        const bool i_zero = abs_constexpr(i) <= constants::TYPE_EPSILON<T> * T(100);

        if (r_zero && i_zero) {
            os << T(0);
            return os;
        }

        if (!r_zero && i_zero) {
            os << r;
            return os;
        }

        if (r_zero && !i_zero) {
            os << i << "i";
            return os;
        }

        os << r;

        if (i < T(0)) {
            os << " - " << -i << "i";
        } else {
            os << " + " << i << "i";
        }

        return os;
    }
};

using ComplexF = BasicComplex<float>;
using ComplexD = BasicComplex<double>;
using ComplexL = BasicComplex<long double>; 

} // namespace fizmo

#endif // FIZMO_BASIC_COMPLEX_CLASS_HPP