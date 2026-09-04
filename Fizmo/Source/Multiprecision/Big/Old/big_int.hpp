#ifndef MULTIPRECISION_BIG_INT_HPP
#define MULTIPRECISION_BIG_INT_HPP

#include "big_uint.hpp"
#include <string>
#include <ostream>
#include <limits>

namespace fizmo {
namespace multiprecision {

class BigInt {
private:
    BigUint m_mag;        
    bool m_is_negative;   
    void normalize() noexcept { if (m_mag.is_zero()) m_is_negative = false; }

public:
    BigInt() noexcept : m_mag(0), m_is_negative(false) {}
    BigInt(const BigUint& u) : m_mag(u), m_is_negative(false) {}
    BigInt(const BigInt&) = default;
    BigInt(BigInt&&) noexcept = default;
    BigInt& operator=(const BigInt&) = default;
    BigInt& operator=(BigInt&&) noexcept = default;
    BigInt& operator=(const BigUint& u) { m_mag = u; m_is_negative = false; return *this; }
    explicit operator BigUint() { return m_mag; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigInt(T value) {
        if (value < 0) {
            m_is_negative = true;
            using U = typename std::make_unsigned<T>::type;
            m_mag = BigUint(static_cast<U>(-value));
        } else {
            m_is_negative = false;
            m_mag = BigUint(static_cast<typename std::make_unsigned<T>::type>(value));
        }

        normalize();
    }

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    BigInt(const T& value) {
        using SignedT   = fizmo::fizmo_make_signed_t<T>;
        using UnsignedT = fizmo::fizmo_make_unsigned_t<T>;
        SignedT s = static_cast<SignedT>(value);

        if (s.is_negative()) {
            m_is_negative = true;
            UnsignedT mag = static_cast<UnsignedT>(-s);
            m_mag = BigUint(mag);
        } else {
            m_is_negative = false;
            UnsignedT mag = static_cast<UnsignedT>(s);
            m_mag = BigUint(mag);
        }

        normalize();
    }

    BigInt(const std::string& s) : m_is_negative(false), m_mag(BigUint::zero()) {
        if (s.empty()) return;
        std::size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '-') { m_is_negative = true; ++i; }
        else if (i < s.size() && s[i] == '+') { ++i; }
        std::string digits;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) digits += s[i++];
        long long exp10 = 0;

        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            bool neg_exp = false;
            if (i < s.size() && s[i] == '-') { neg_exp = true; ++i; }
            else if (i < s.size() && s[i] == '+') { ++i; }
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) exp10 = exp10 * 10 + (s[i++] - '0');
            if (neg_exp) exp10 = -exp10;
        }

        if (digits.empty()) { normalize(); return; }
        for (char c : digits) m_mag = m_mag * BigUint(10) + BigUint(c - '0');

        if (exp10 > 0) {
            BigUint pow10(1);
            for (long long k = 0; k < exp10; ++k) pow10 *= BigUint(10);
            m_mag *= pow10;
        } else if (exp10 < 0) {
            BigUint pow10(1);
            for (long long k = 0; k < -exp10; ++k) pow10 *= BigUint(10);
            m_mag = BigUint::divmod(m_mag, pow10).quotient;
        }

        normalize();
    }

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    BigInt& operator=(const T& value) {
        *this = BigInt(value);
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigInt& operator=(T value) {
        *this = BigInt(value);
        return *this;
    }

    bool is_zero() const noexcept { return !m_is_negative && m_mag.is_zero(); }
    bool is_one() const noexcept { return m_mag.is_one(); }
    bool is_even() const noexcept { return m_mag.is_even(); }
    bool is_negative() const noexcept { return m_is_negative; }
    bool is_positive() const noexcept { return !m_is_negative && !m_mag.is_zero(); }
    const BigUint& magnitude() const noexcept { return m_mag; }
    bool is_error() const noexcept { return m_is_negative && m_mag.is_zero(); }

    std::size_t get_exponent_base2() const noexcept {
        if (is_zero() || is_error()) return 0;
        return m_mag.get_exponent_base2();
    }

    std::size_t get_exponent_base10() const {
        if (is_zero() || is_error()) return 0;
        return m_mag.get_exponent_base10();
    }

    std::size_t count_trailing_zeros() const { return m_mag.count_trailing_zeros(); }

    static BigInt error() noexcept {
        BigInt e;
        e.m_mag = BigUint::zero();
        e.m_is_negative = true;
        return e;
    }

    BigInt operator-() const noexcept {
        BigInt r(*this);
        r.negate();
        return r;
    }

    BigInt abs() const noexcept {
        BigInt r(*this);
        r.abs_in_place();
        return r;
    }

    BigInt& negate() noexcept {
        if (is_error()) return *this;
        if (!is_zero()) m_is_negative = !m_is_negative;
        return *this;
    }

    BigInt& abs_in_place() noexcept {
        if (is_error()) return *this;
        m_is_negative = false;
        return *this;
    }

    BigInt operator+() const noexcept { return *this; }

    friend bool operator==(const BigInt& a, const BigInt& b) noexcept { return a.m_is_negative == b.m_is_negative && a.m_mag == b.m_mag; }
    friend bool operator!=(const BigInt& a, const BigInt& b) noexcept { return !(a == b); }

    friend bool operator<(const BigInt& a, const BigInt& b) noexcept {
        if (a.is_error() || b.is_error()) { return false; }
        if (a.m_is_negative != b.m_is_negative) return a.m_is_negative;
        if (!a.m_is_negative) return a.m_mag < b.m_mag;
        return b.m_mag < a.m_mag;
    }

    friend bool operator>(const BigInt& a, const BigInt& b) noexcept { return b < a; }
    friend bool operator<=(const BigInt& a, const BigInt& b) noexcept { return !(b < a); }
    friend bool operator>=(const BigInt& a, const BigInt& b) noexcept { return !(a < b); }

    friend BigInt operator+(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error()) { return BigInt::error(); }
        BigInt r;

        if (a.m_is_negative == b.m_is_negative) {
            r.m_mag = a.m_mag + b.m_mag;
            r.m_is_negative = a.m_is_negative;
            return r;
        }

        if (a.m_mag >= b.m_mag) {
            r.m_mag = a.m_mag - b.m_mag;
            r.m_is_negative = a.m_is_negative;
        } else {
            r.m_mag = b.m_mag - a.m_mag;
            r.m_is_negative = b.m_is_negative;
        }

        r.normalize();
        return r;
    }

    BigInt& operator+=(const BigInt& other) {
        if (is_error() || other.is_error()) { *this = error(); return *this; }
        if (other.m_mag.is_zero()) return *this;

        if (m_is_negative == other.m_is_negative) {
            m_mag += other.m_mag;
        } else {
            if (m_mag >= other.m_mag) {
                m_mag -= other.m_mag;
            } else {
                m_mag = other.m_mag - m_mag; 
                m_is_negative = other.m_is_negative;
            }

            normalize();
        }

        return *this;
    }

    BigInt operator+(std::uint64_t d) const {
        if (is_error()) return error();

        if (m_is_negative) {
            if (m_mag < BigUint(d)) return BigInt(static_cast<std::int64_t>(d - m_mag.limb(0)));  
            BigInt r; 
            r.m_mag = m_mag - d; 
            r.m_is_negative = !r.m_mag.is_zero(); 
            r.normalize(); 
            return r;
        }

        BigInt r; 
        r.m_mag = m_mag + d; 
        return r;
    }

    BigInt& operator+=(std::uint64_t d) { *this = *this + d; return *this; }

    friend BigInt operator-(const BigInt& a, const BigInt& b) { 
        BigInt r = a;
        r -= b;
        return r;
    }

    BigInt& operator-=(const BigInt& other) {
        if (is_error() || other.is_error()) { *this = error(); return *this; }
        if (other.m_mag.is_zero()) return *this;
        bool other_neg = !other.m_is_negative;

        if (m_is_negative == other_neg) {
            m_mag += other.m_mag;
        } else {
            if (m_mag >= other.m_mag) {
                m_mag -= other.m_mag;
            } else {
                m_mag = other.m_mag - m_mag;
                m_is_negative = other_neg;
            }

            normalize();
        }

        return *this;
    }

    BigInt operator-(std::uint64_t d) const {
        if (is_error()) return error();

        if (m_is_negative) {
            BigInt r; 
            r.m_mag = m_mag + d; 
            r.m_is_negative = true; 
            return r;
        }

        if (m_mag < BigUint(d)) {
            BigInt r; 
            r.m_mag = BigUint(d) - m_mag; 
            r.m_is_negative = !r.m_mag.is_zero(); 
            return r;
        }

        BigInt r; r.m_mag = m_mag - d; r.normalize(); return r;
    }

    BigInt& operator-=(std::uint64_t d) { *this = *this - d; return *this; }

public:
    friend BigInt operator+(BigInt&& a, const BigInt& b) { a += b; return a; }
    friend BigInt operator+(const BigInt& a, BigInt&& b) { b += a; return b; }
    friend BigInt operator+(BigInt&& a, BigInt&& b)      { a += b; return a; }

    friend BigInt operator-(BigInt&& a, const BigInt& b) { a -= b; return a; }
    friend BigInt operator-(const BigInt& a, BigInt&& b) { b -= a; b.negate(); return b; }
    friend BigInt operator-(BigInt&& a, BigInt&& b)      { a -= b; return a; }

    friend BigInt operator&(BigInt&& a, const BigInt& b) { a &= b; return a; }
    friend BigInt operator&(const BigInt& a, BigInt&& b) { b &= a; return b; }
    friend BigInt operator&(BigInt&& a, BigInt&& b)      { a &= b; return a; }

    friend BigInt operator|(BigInt&& a, const BigInt& b) { a |= b; return a; }
    friend BigInt operator|(const BigInt& a, BigInt&& b) { b |= a; return b; }
    friend BigInt operator|(BigInt&& a, BigInt&& b)      { a |= b; return a; }

    friend BigInt operator^(BigInt&& a, const BigInt& b) { a ^= b; return a; }
    friend BigInt operator^(const BigInt& a, BigInt&& b) { b ^= a; return b; }
    friend BigInt operator^(BigInt&& a, BigInt&& b)      { a ^= b; return a; }

public:
    friend BigInt operator*(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error()) { return BigInt::error(); }
        BigInt r;
        r.m_mag = a.m_mag * b.m_mag;
        r.m_is_negative = (a.m_is_negative != b.m_is_negative);
        r.normalize();
        return r;
    }

    BigInt& operator*=(const BigInt& other) {
        if (is_error() || other.is_error()) { *this = error(); return *this; }
        m_is_negative = (m_is_negative != other.m_is_negative);
        m_mag *= other.m_mag;
        normalize();
        return *this;
    }

    BigInt operator*(std::uint64_t d) const {
        if (is_error()) return error();
        BigInt r; 
        r.m_mag = m_mag * d; 
        r.m_is_negative = m_is_negative && !r.m_mag.is_zero(); 
        return r;
    }

    BigInt& operator*=(std::uint64_t d) {
        if (is_error()) return *this;
        m_mag *= d; 
        if (m_mag.is_zero()) m_is_negative = false; 
        return *this;
    }

public:
    BigInt mul_textbook(const BigInt& other) const {
        if (is_error() || other.is_error()) return error();
        BigInt r;
        r.m_mag = m_mag.mul_textbook(other.m_mag);
        r.m_is_negative = (m_is_negative != other.m_is_negative);
        r.normalize();
        return r;
    }

    BigInt mul_karatsuba(const BigInt& other) const {
        if (is_error() || other.is_error()) return error();
        BigInt r;
        r.m_mag = m_mag.mul_karatsuba(other.m_mag);
        r.m_is_negative = (m_is_negative != other.m_is_negative);
        r.normalize();
        return r;
    }

    BigInt mul_toom3(const BigInt& other) const {
        if (is_error() || other.is_error()) return error();
        BigInt r;
        r.m_mag = m_mag.mul_toom3(other.m_mag);
        r.m_is_negative = (m_is_negative != other.m_is_negative);
        r.normalize();
        return r;
    }

    BigInt mul_fft(const BigInt& other) const {
        if (is_error() || other.is_error()) return error();
        BigInt r;
        r.m_mag = m_mag.mul_fft(other.m_mag);
        r.m_is_negative = (m_is_negative != other.m_is_negative);
        r.normalize();
        return r;
    }

public:
    friend BigInt operator/(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error() || b.is_zero()) { return BigInt::error(); }
        auto qr = BigUint::divmod(a.m_mag, b.m_mag);
        BigInt q;
        q.m_mag = qr.quotient;
        q.m_is_negative = (a.m_is_negative != b.m_is_negative) && !q.m_mag.is_zero();
        return q;
    }

    friend BigInt operator%(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error() || b.is_zero()) { return BigInt::error(); }
        auto qr = BigUint::divmod(a.m_mag, b.m_mag);
        BigInt r;
        r.m_mag = qr.remainder;
        r.m_is_negative = a.m_is_negative && !r.m_mag.is_zero();
        return r;
    }

    BigInt& operator/=(const BigInt& other) {
        if (is_error() || other.is_error() || other.is_zero()) {
            *this = error();
            return *this;
        }

        auto qr = BigUint::divmod(m_mag, other.m_mag);
        m_is_negative = (m_is_negative != other.m_is_negative) && !qr.quotient.is_zero();
        m_mag = std::move(qr.quotient);
        return *this;
    }

    BigInt& operator%=(const BigInt& other) {
        if (is_error() || other.is_error() || other.is_zero()) {
        *this = error();
        return *this;
    }

        auto qr = BigUint::divmod(m_mag, other.m_mag);
        m_is_negative = m_is_negative && !qr.remainder.is_zero();
        m_mag = std::move(qr.remainder);
        return *this;
    }

    BigInt operator/(std::uint64_t d) const {
        if (is_error() || d == 0) return error();
        BigInt r; 
        r.m_mag = m_mag / d;
        r.m_is_negative = m_is_negative && !r.m_mag.is_zero(); 
        return r;
    }

    BigInt& operator/=(std::uint64_t d) {
        if (is_error() || d == 0) { *this = error(); return *this; }
        m_mag /= d; 
        if (m_mag.is_zero()) m_is_negative = false; 
        return *this;
    }
    
    BigInt operator%(std::uint64_t d) const {
        if (is_error() || d == 0) return error();
        BigUint rem = m_mag;
        rem %= d;
        BigInt r(rem);
        r.m_is_negative = m_is_negative && rem != 0;
        return r;
    }
    
    BigInt& operator%=(std::uint64_t d) { *this = *this % d; return *this; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigInt operator<<(T shift) const {
        if (is_error()) return error();
        BigInt r;
        r.m_mag = m_mag << shift;
        r.m_is_negative = m_is_negative;
        return r;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigInt operator>>(T shift) const {
        if (is_error()) return error();
        BigInt r;
        r.m_mag = m_mag >> shift;
        r.m_is_negative = m_is_negative && !r.m_mag.is_zero();
        return r;
    }

    template <typename T>
    BigInt& operator<<=(T shift) {
        if (is_error()) return *this;
        m_mag <<= shift;
        return *this;
    }

    template <typename T>
    BigInt& operator>>=(T shift) { 
        if (is_error()) return *this;
        m_mag >>= shift;
        if (m_is_negative && m_mag.is_zero()) m_is_negative = false;
        return *this;
    }

    friend BigInt operator&(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error()) { return BigInt::error(); }
        if (a.m_is_negative || b.m_is_negative) return BigInt::error();
        return BigInt(a.m_mag & b.m_mag);
    }

    friend BigInt operator|(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error()) { return BigInt::error(); }
        if (a.m_is_negative || b.m_is_negative) return BigInt::error();
        return BigInt(a.m_mag | b.m_mag);
    }

    friend BigInt operator^(const BigInt& a, const BigInt& b) {
        if (a.is_error() || b.is_error()) { return BigInt::error(); }
        if (a.m_is_negative || b.m_is_negative) return BigInt::error();
        return BigInt(a.m_mag ^ b.m_mag);
    }

    BigInt& operator&=(const BigInt& other) {
        if (is_error() || other.is_error() || m_is_negative || other.m_is_negative) {
            *this = error();
            return *this;
        }
        
        m_mag &= other.m_mag;
        return *this;
    }

    BigInt& operator|=(const BigInt& other) {
        if (is_error() || other.is_error() || m_is_negative || other.m_is_negative) {
            *this = error();
            return *this;
        }

        m_mag |= other.m_mag;
        return *this;
    }

    BigInt& operator^=(const BigInt& other) {
        if (is_error() || other.is_error() || m_is_negative || other.m_is_negative) {
            *this = error();
            return *this;
        }

        m_mag ^= other.m_mag;
        normalize();
        return *this;
    }

    std::string to_string() const {
        if (is_error()) return "BigInt-error";
        if (is_zero()) return "0";
        std::string s = m_mag.to_string();
        if (m_is_negative) s.insert(s.begin(), '-');
        return s;
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        if (is_error()) return "BigInt-error";
        if (is_zero()) return "0";
        std::string s = m_mag.to_scientific_string(sig_figs);
        if (m_is_negative) s.insert(s.begin(), '-');
        return s;
    }

    friend std::ostream& operator<<(std::ostream& os, const BigInt& x) {
        os << x.to_scientific_string(20);
        return os;
    }
};

BigUint::BigUint(const BigInt& i) { *this = i.magnitude(); }
BigUint& BigUint::operator=(const BigInt& i) { *this = i.magnitude(); return *this; }
BigUint::operator BigInt() { return BigInt(*this); }

} // namespace multiprecision

template<> struct is_fizmo_int<multiprecision::BigUint> : std::true_type{};
template<> struct is_fizmo_int<multiprecision::BigInt> : std::true_type{};
template <> struct fizmo_make_unsigned<multiprecision::BigUint> { using type = multiprecision::BigUint; };
template <> struct fizmo_make_unsigned<multiprecision::BigInt>  { using type = multiprecision::BigUint; };
template <> struct fizmo_make_signed<multiprecision::BigUint>   { using type = multiprecision::BigInt;  };
template <> struct fizmo_make_signed<multiprecision::BigInt>    { using type = multiprecision::BigInt;  };
template<> struct is_fizmo_signed<multiprecision::BigInt> : std::true_type{};
template<> struct is_fizmo_unsigned<multiprecision::BigUint> : std::true_type{};
template <> struct integer_rank<multiprecision::BigUint> : std::integral_constant<int, std::numeric_limits<int>::max() - 3> {};
template <> struct integer_rank<multiprecision::BigInt> : std::integral_constant<int, std::numeric_limits<int>::max() - 2> {};

} // namespace fizmo

#endif