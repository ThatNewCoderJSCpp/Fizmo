#ifndef MULTIPRECISION_GIGANTIC_UINT_HPP
#define MULTIPRECISION_GIGANTIC_UINT_HPP

#include "big_uint.hpp"
#include "big_int.hpp"
#include <string>
#include <sstream>
#include <utility>

namespace fizmo {
namespace multiprecision {

class GiganticInt;

class GiganticUInt {
private:
    BigUint m_mantissa;
    BigUint m_exponent;

    void normalize() noexcept {
        if (m_mantissa.is_zero()) { m_exponent = BigUint::zero(); return; }
        std::size_t tz = m_mantissa.count_trailing_zeros();

        if (tz > 0) {
            m_mantissa >>= tz;
            m_exponent += BigUint(tz);
        }
    }

    static bool exp_diff_small(const BigUint& hi, const BigUint& lo, std::size_t& shift, std::size_t cap = 1ULL << 24) {
        if (hi < lo) return false;
        BigUint diff = hi - lo;
        if (diff.limb_count() > 1) return false;
        std::uint64_t d = diff.limb(0);
        if (d > cap) return false;
        shift = static_cast<std::size_t>(d);
        return true;
    }

public:
    GiganticUInt() noexcept : m_mantissa(), m_exponent() {}
    explicit GiganticUInt(const BigUint& v) : m_mantissa(v), m_exponent() { normalize(); }
    explicit GiganticUInt(BigUint&& v) : m_mantissa(std::move(v)), m_exponent() { normalize(); }
    GiganticUInt(BigUint mantissa, BigUint exponent) : m_mantissa(std::move(mantissa)), m_exponent(std::move(exponent)) { normalize(); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    GiganticUInt(T v) : m_mantissa(BigUint(v)), m_exponent() { normalize(); }

    GiganticUInt(const GiganticUInt&) = default;
    GiganticUInt(GiganticUInt&&) noexcept = default;
    GiganticUInt& operator=(const GiganticUInt&) = default;
    GiganticUInt& operator=(GiganticUInt&&) noexcept = default;

    GiganticUInt(const GiganticInt& i);
    GiganticUInt& operator=(const GiganticInt& i);
    explicit operator GiganticInt();

    static GiganticUInt zero() noexcept { return GiganticUInt(); }
    static GiganticUInt one()  noexcept { return GiganticUInt(1); }
    static GiganticUInt power_of_two(const BigUint& exponent) { return GiganticUInt(BigUint::one(), exponent); }
    static GiganticUInt power_of_two(BigUint&& exponent) { return GiganticUInt(BigUint::one(), std::move(exponent)); }

    bool is_zero() const noexcept { return m_mantissa.is_zero(); }
    bool is_one()  const noexcept { return m_mantissa.is_one() && m_exponent.is_zero(); }
    bool has_exponent() const noexcept { return !m_exponent.is_zero(); }
    const BigUint& mantissa() const noexcept { return m_mantissa; }
    const BigUint& exponent() const noexcept { return m_exponent; }

    bool is_evaluable(std::size_t max_bits) const noexcept {
        if (!has_exponent()) return true;
        if (m_exponent.limb_count() > 1) return false;
        return m_exponent.limb(0) <= max_bits;
    }

    BigUint evaluate(std::size_t max_bits = 1ULL << 24) const {
        if (!has_exponent()) return m_mantissa;
        if (!is_evaluable(max_bits)) return BigUint::zero();
        std::size_t shift = static_cast<std::size_t>(m_exponent.limb(0));
        return m_mantissa << shift;
    }

    BigUint approx_log2() const {
        if (is_zero()) return BigUint::zero();
        BigUint bl(m_mantissa.bit_length() > 0 ? m_mantissa.bit_length() - 1 : 0);
        return bl + m_exponent;
    }

    friend bool operator==(const GiganticUInt& a, const GiganticUInt& b) noexcept { return a.m_mantissa == b.m_mantissa && a.m_exponent == b.m_exponent; }
    friend bool operator!=(const GiganticUInt& a, const GiganticUInt& b) noexcept { return !(a == b); }

    friend bool operator<(const GiganticUInt& a, const GiganticUInt& b) noexcept {
        if (a.is_zero()) return !b.is_zero();
        if (b.is_zero()) return false;
        BigUint a_bl(a.m_mantissa.bit_length());
        BigUint b_bl(b.m_mantissa.bit_length());
        BigUint a_log = a_bl + a.m_exponent;
        BigUint b_log = b_bl + b.m_exponent;
        if (a_log != b_log) return a_log < b_log;
        
        if (a.m_exponent != b.m_exponent) {
            std::size_t shift;

            if (a.m_exponent < b.m_exponent && exp_diff_small(b.m_exponent, a.m_exponent, shift)) {
                BigUint b_aligned = b.m_mantissa << shift;
                return a.m_mantissa < b_aligned;
            }

            if (b.m_exponent < a.m_exponent && exp_diff_small(a.m_exponent, b.m_exponent, shift)) {
                BigUint a_aligned = a.m_mantissa << shift;
                return a_aligned < b.m_mantissa;
            }

            return a.m_exponent < b.m_exponent;
        }

        return a.m_mantissa < b.m_mantissa;
    }

    friend bool operator> (const GiganticUInt& a, const GiganticUInt& b) noexcept { return b < a;   }
    friend bool operator<=(const GiganticUInt& a, const GiganticUInt& b) noexcept { return !(b < a); }
    friend bool operator>=(const GiganticUInt& a, const GiganticUInt& b) noexcept { return !(a < b); }

    friend GiganticUInt operator*(const GiganticUInt& a, const GiganticUInt& b) {
        if (a.is_zero() || b.is_zero()) return GiganticUInt();
        if (a.is_one()) return b;
        if (b.is_one()) return a;
        return GiganticUInt(a.m_mantissa * b.m_mantissa, a.m_exponent + b.m_exponent);
    }

    friend GiganticUInt operator/(const GiganticUInt& a, const GiganticUInt& b) {
        if (b.is_zero() || a.is_zero()) return GiganticUInt();
        if (b.is_one()) return a;
        BigUint ne = (a.m_exponent >= b.m_exponent) ? (a.m_exponent - b.m_exponent) : BigUint::zero();
        auto dr = BigUint::divmod(a.m_mantissa, b.m_mantissa);
        BigUint nm = dr.quotient.is_zero() ? BigUint::one() : std::move(dr.quotient);
        return GiganticUInt(std::move(nm), std::move(ne));
    }


    friend GiganticUInt operator+(const GiganticUInt& a, const GiganticUInt& b) {
        if (a.is_zero()) return b;
        if (b.is_zero()) return a;
        const GiganticUInt& lo = (a.m_exponent <= b.m_exponent) ? a : b;
        const GiganticUInt& hi = (a.m_exponent <= b.m_exponent) ? b : a;
        std::size_t shift;

        if (exp_diff_small(hi.m_exponent, lo.m_exponent, shift)) {
            BigUint hi_shifted = hi.m_mantissa << shift;
            return GiganticUInt(lo.m_mantissa + hi_shifted, lo.m_exponent);
        }
        
        return hi;
    }

    friend GiganticUInt operator-(const GiganticUInt& a, const GiganticUInt& b) {
        if (b.is_zero()) return a;
        if (a <= b) return GiganticUInt();
        if (a.m_exponent == b.m_exponent) { return GiganticUInt(a.m_mantissa - b.m_mantissa, a.m_exponent); }
        std::size_t shift;

        if (a.m_exponent > b.m_exponent && exp_diff_small(a.m_exponent, b.m_exponent, shift)) {
            BigUint a_shifted = a.m_mantissa << shift;
            if (a_shifted <= b.m_mantissa) return GiganticUInt();
            return GiganticUInt(a_shifted - b.m_mantissa, b.m_exponent);
        }

        if (b.m_exponent > a.m_exponent && exp_diff_small(b.m_exponent, a.m_exponent, shift)) {
            BigUint b_shifted = b.m_mantissa << shift;
            if (a.m_mantissa <= b_shifted) return GiganticUInt();
            return GiganticUInt(a.m_mantissa - b_shifted, a.m_exponent);
        }
        
        return a;
    }

    friend GiganticUInt operator%(const GiganticUInt& a, const GiganticUInt& b) {
        if (b.is_zero() || a.is_zero()) return GiganticUInt();
        if (!a.has_exponent() && !b.has_exponent()) return GiganticUInt(a.m_mantissa % b.m_mantissa);
        if (a.m_exponent == b.m_exponent) return GiganticUInt(a.m_mantissa % b.m_mantissa, BigUint::zero());
        return GiganticUInt(); // can't compute
    }

    GiganticUInt& operator+=(const GiganticUInt& o) { *this = *this + o; return *this; }
    GiganticUInt& operator-=(const GiganticUInt& o) { *this = *this - o; return *this; }
    GiganticUInt& operator*=(const GiganticUInt& o) { *this = *this * o; return *this; }
    GiganticUInt& operator/=(const GiganticUInt& o) { *this = *this / o; return *this; }
    GiganticUInt& operator%=(const GiganticUInt& o) { *this = *this % o; return *this; }

    GiganticUInt operator*(std::uint64_t d) const {
        if (d == 0 || is_zero()) return GiganticUInt();
        return GiganticUInt(m_mantissa * d, m_exponent);
    }

    GiganticUInt operator/(std::uint64_t d) const {
        if (d == 0 || is_zero()) return GiganticUInt();
        return GiganticUInt(m_mantissa / d, m_exponent);
    }

    GiganticUInt& operator*=(std::uint64_t d) { *this = *this * d; return *this; }
    GiganticUInt& operator/=(std::uint64_t d) { *this = *this / d; return *this; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    GiganticUInt operator<<(T s) const {
        if (s <= 0 || is_zero()) return *this;
        return GiganticUInt(m_mantissa, m_exponent + BigUint(static_cast<std::uint64_t>(s)));
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    GiganticUInt operator>>(T s) const {
        if (s <= 0 || is_zero()) return *this;
        BigUint delta(static_cast<std::uint64_t>(s));
        if (m_exponent >= delta) return GiganticUInt(m_mantissa, m_exponent - delta);
        BigUint remaining = delta - m_exponent;
        if (remaining.limb_count() > 1) return GiganticUInt(); 
        return GiganticUInt(m_mantissa >> static_cast<std::size_t>(remaining.limb(0)));
    }

    GiganticUInt symbolic_shift_left(const BigUint& s) const {
        if (s.is_zero() || is_zero()) return *this;
        return GiganticUInt(m_mantissa, m_exponent + s);
    }

    GiganticUInt symbolic_shift_right(const BigUint& s) const {
        if (s.is_zero() || is_zero()) return *this;
        if (m_exponent >= s) return GiganticUInt(m_mantissa, m_exponent - s);
        return GiganticUInt();
    }

    template <typename T> GiganticUInt& operator<<=(T s) { *this = *this << s; return *this; }
    template <typename T> GiganticUInt& operator>>=(T s) { *this = *this >> s; return *this; }

    std::string to_string() const {
        if (is_zero()) return "0";
        if (!has_exponent()) return m_mantissa.to_string();
        return m_mantissa.to_string() + "\n+ 2 \u00d7 " + m_exponent.to_string(); 
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        if (is_zero()) return "0";
        if (!has_exponent()) return m_mantissa.to_scientific_string(sig_figs);
        return m_mantissa.to_scientific_string(sig_figs) + "\n+ 2 \u00d7 " + m_exponent.to_scientific_string(sig_figs); 
    }

    friend std::ostream& operator<<(std::ostream& os, const GiganticUInt& x) {
        os << x.to_scientific_string(64);
        return os;
    }
};

class GiganticInt {
private:
    GiganticUInt m_mag;
    bool m_is_negative;
    void normalize() noexcept { if (m_mag.is_zero()) m_is_negative = false; }

public:
    GiganticInt() noexcept : m_mag(), m_is_negative(false) {}
    GiganticInt(const GiganticUInt& u) : m_mag(u), m_is_negative(false) {}
    GiganticInt(GiganticUInt&& u) noexcept : m_mag(std::move(u)), m_is_negative(false) {}
    GiganticInt(const GiganticInt&) = default;
    GiganticInt(GiganticInt&&) noexcept = default;
    GiganticInt& operator=(const GiganticInt&) = default;
    GiganticInt& operator=(GiganticInt&&) noexcept = default;
    GiganticInt& operator=(const GiganticUInt& u) { m_mag = u; m_is_negative = false; return *this; }
    explicit operator GiganticUInt() { return m_mag; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    GiganticInt(T value) {
        if (value < 0) {
            m_is_negative = true;
            using U = typename std::make_unsigned<T>::type;
            m_mag = GiganticUInt(static_cast<U>(-value));
        } else {
            m_is_negative = false;
            m_mag = GiganticUInt(static_cast<typename std::make_unsigned<T>::type>(value));
        }

        normalize();
    }

    GiganticInt(const BigInt& bi) : m_mag(bi.magnitude()), m_is_negative(bi.is_negative()) { normalize(); }
    GiganticInt(const BigUint& bu) : m_mag(bu), m_is_negative(false) {}

    static GiganticInt zero()  noexcept { return GiganticInt(); }
    static GiganticInt one()   noexcept { return GiganticInt(1); }
    static GiganticInt error() noexcept { GiganticInt e; e.m_is_negative = true; return e; }

    bool is_zero()     const noexcept { return !m_is_negative && m_mag.is_zero(); }
    bool is_one()      const noexcept { return !m_is_negative && m_mag.is_one(); }
    bool is_negative() const noexcept { return m_is_negative; }
    bool is_positive() const noexcept { return !m_is_negative && !m_mag.is_zero(); }
    bool is_error()    const noexcept { return m_is_negative && m_mag.is_zero(); }
    const GiganticUInt& magnitude() const noexcept { return m_mag; }

    GiganticInt operator-() const { GiganticInt r(*this); r.negate(); return r; }
    GiganticInt operator+() const { return *this; }

    GiganticInt& negate() noexcept {
        if (!is_error() && !is_zero()) m_is_negative = !m_is_negative;
        return *this;
    }

    GiganticInt abs() const {
        GiganticInt r(*this);
        r.m_is_negative = false;
        return r;
    }

    GiganticInt& abs_inplace() {
        if (!(m_mag.is_zero() && m_is_negative == true)) { m_is_negative = false; }
        return *this;  
    } 

    friend bool operator==(const GiganticInt& a, const GiganticInt& b) noexcept {
        return a.m_is_negative == b.m_is_negative && a.m_mag == b.m_mag;
    }

    friend bool operator!=(const GiganticInt& a, const GiganticInt& b) noexcept { return !(a == b); }

    friend bool operator<(const GiganticInt& a, const GiganticInt& b) noexcept {
        if (a.is_error() || b.is_error()) return false;
        if (a.m_is_negative != b.m_is_negative) return a.m_is_negative;
        if (!a.m_is_negative) return a.m_mag < b.m_mag;
        return b.m_mag < a.m_mag;
    }

    friend bool operator> (const GiganticInt& a, const GiganticInt& b) noexcept { return b < a;   }
    friend bool operator<=(const GiganticInt& a, const GiganticInt& b) noexcept { return !(b < a); }
    friend bool operator>=(const GiganticInt& a, const GiganticInt& b) noexcept { return !(a < b); }

    friend GiganticInt operator+(const GiganticInt& a, const GiganticInt& b) {
        if (a.is_error() || b.is_error()) return GiganticInt::error();
        GiganticInt r;

        if (a.m_is_negative == b.m_is_negative) {
            r.m_mag = a.m_mag + b.m_mag;
            r.m_is_negative = a.m_is_negative;
        } else if (a.m_mag >= b.m_mag) {
            r.m_mag = a.m_mag - b.m_mag;
            r.m_is_negative = a.m_is_negative;
        } else {
            r.m_mag = b.m_mag - a.m_mag;
            r.m_is_negative = b.m_is_negative;
        }

        r.normalize();
        return r;
    }

    friend GiganticInt operator-(const GiganticInt& a, const GiganticInt& b) {
        GiganticInt nb = b; nb.negate();
        return a + nb;
    }

    friend GiganticInt operator*(const GiganticInt& a, const GiganticInt& b) {
        if (a.is_error() || b.is_error()) return GiganticInt::error();
        GiganticInt r;
        r.m_mag = a.m_mag * b.m_mag;
        r.m_is_negative = (a.m_is_negative != b.m_is_negative);
        r.normalize();
        return r;
    }

    friend GiganticInt operator/(const GiganticInt& a, const GiganticInt& b) {
        if (a.is_error() || b.is_error() || b.is_zero()) return GiganticInt::error();
        GiganticInt r;
        r.m_mag = a.m_mag / b.m_mag;
        r.m_is_negative = (a.m_is_negative != b.m_is_negative) && !r.m_mag.is_zero();
        r.normalize();
        return r;
    }

    friend GiganticInt operator%(const GiganticInt& a, const GiganticInt& b) {
        if (a.is_error() || b.is_error() || b.is_zero()) return GiganticInt::error();
        GiganticInt r;
        r.m_mag = a.m_mag % b.m_mag;
        r.m_is_negative = a.m_is_negative && !r.m_mag.is_zero();
        r.normalize();
        return r;
    }

    GiganticInt& operator+=(const GiganticInt& o) { *this = *this + o; return *this; }
    GiganticInt& operator-=(const GiganticInt& o) { *this = *this - o; return *this; }
    GiganticInt& operator*=(const GiganticInt& o) { *this = *this * o; return *this; }
    GiganticInt& operator/=(const GiganticInt& o) { *this = *this / o; return *this; }
    GiganticInt& operator%=(const GiganticInt& o) { *this = *this % o; return *this; }

    GiganticInt operator*(std::uint64_t d) const {
        if (is_error()) return error();
        GiganticInt r; 
        r.m_mag = m_mag * d; 
        r.m_is_negative = m_is_negative; 
        r.normalize(); 
        return r;
    }

    GiganticInt operator/(std::uint64_t d) const {
        if (is_error() || d == 0) return error();
        GiganticInt r; 
        r.m_mag = m_mag / d; 
        r.m_is_negative = m_is_negative && !r.m_mag.is_zero(); 
        return r;
    }

    GiganticInt& operator*=(std::uint64_t d) { *this = *this * d; return *this; }
    GiganticInt& operator/=(std::uint64_t d) { *this = *this / d; return *this; }

    std::string to_string() const {
        if (is_error()) return "GiganticInt-error";
        if (is_zero()) return "0";
        return (m_is_negative ? "-" : "") + m_mag.to_string();
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        if (is_error()) return "GiganticInt-error";
        if (is_zero()) return "0";
        return (m_is_negative ? "-" : "") + m_mag.to_scientific_string(sig_figs);
    }

    friend std::ostream& operator<<(std::ostream& os, const GiganticInt& x) {
        os << x.to_scientific_string(64);
        return os;
    }
};

inline GiganticUInt::GiganticUInt(const GiganticInt& i) : GiganticUInt(i.magnitude()) {}
inline GiganticUInt& GiganticUInt::operator=(const GiganticInt& i) { *this = i.magnitude(); return *this; }
inline GiganticUInt::operator GiganticInt() { return GiganticInt(*this); }

} // namespace multiprecision

template <> struct is_fizmo_int<multiprecision::GiganticUInt> : std::true_type{};
template <> struct is_fizmo_int<multiprecision::GiganticInt>  : std::true_type{};
template <> struct fizmo_make_unsigned<multiprecision::GiganticUInt> { using type = multiprecision::BigUint; };
template <> struct fizmo_make_unsigned<multiprecision::GiganticInt>  { using type = multiprecision::BigUint; };
template <> struct fizmo_make_signed<multiprecision::GiganticUInt>   { using type = multiprecision::BigInt;  };
template <> struct fizmo_make_signed<multiprecision::GiganticInt>    { using type = multiprecision::BigInt;  };
template <> struct is_fizmo_signed<multiprecision::GiganticInt>    : std::true_type{};
template <> struct is_fizmo_unsigned<multiprecision::GiganticUInt> : std::true_type{};
template <> struct integer_rank<multiprecision::GiganticUInt> : std::integral_constant<int, std::numeric_limits<int>::max() - 1> {};
template <> struct integer_rank<multiprecision::GiganticInt>  : std::integral_constant<int, std::numeric_limits<int>::max()> {};

} // namespace fizmo

#endif // MULTIPRECISION_GIGANTIC_UINT_HPP