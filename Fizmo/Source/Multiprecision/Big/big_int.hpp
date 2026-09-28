#ifndef FIZMO_MULTIPRECISION_BIG_INTEGER_CLASS_HPP
#define FIZMO_MULTIPRECISION_BIG_INTEGER_CLASS_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <limits>
#include <string>
#include <ostream>
#include <cmath>
#include <utility>

#include "big_uint.hpp"
#include "BigUint Detail/big_uint_ops.hpp"

namespace fizmo {
namespace multiprecision {

class BigInt {
private:
    BigUInt m_mag;
    bool    m_neg = false;

private:
    void normalize() noexcept {
        if (m_mag.is_zero() || m_mag.is_undefined()) m_neg = false;
    }

    BigInt& assign_from(BigUInt mag, bool neg) {
        m_mag = std::move(mag);
        m_neg = neg;
        normalize();
        return *this;
    }

    static BigInt make(BigUInt mag, bool neg) {
        BigInt r;
        r.assign_from(std::move(mag), neg);
        return r;
    }

    BigInt& add_signed(const BigUInt& omag, bool oneg) {
        if (m_neg == oneg) {
            m_mag.add_mutable(omag);
            normalize();
            return *this;
        }

        const int c = m_mag.compare(omag);
        if (c == 0) { m_mag = BigUInt::zero(); m_neg = false; return *this; }

        if (c > 0) {
            m_mag.sub_mutable(omag);
            normalize();
            return *this;
        }

        BigUInt t(omag);
        t.sub_mutable(m_mag);
        return assign_from(std::move(t), oneg);
    }

public:
    static constexpr std::size_t max_limbs  = BigUInt::max_limbs;
    static constexpr std::size_t max_bits   = BigUInt::max_bits;
    static constexpr std::size_t max_digits = BigUInt::max_digits;

public:
    BigInt() : m_mag(), m_neg(false) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
    BigInt(T v) : m_mag(v), m_neg(false) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type, typename = void>
    BigInt(T v) : m_mag(), m_neg(false) {
        const budetail::small_arg s = budetail::as_small(v);
        m_mag = BigUInt(s.mag);
        m_neg = s.neg && !m_mag.is_zero();
    }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void, typename = void>
    explicit BigInt(T value) : m_mag(), m_neg(false) {
        if (!(value == value)) { *this = nan(); return; }
        const bool neg = value < T(0);
        long double v = std::floor(std::fabs(static_cast<long double>(value)));
        if (!(v < std::numeric_limits<long double>::infinity())) { *this = undefined(); return; }
        if (v < 1.0L) return;                       
        const long double two64 = 18446744073709551616.0L;
        budetail::limb_vec limbs;

        while (v >= 1.0L) {
            limbs.push_back(static_cast<std::uint64_t>(std::fmod(v, two64)));
            v = std::floor(v / two64);
        }

        assign_from(BigUInt(std::move(limbs)), neg);
    }

    BigInt(const BigUInt& mag, bool neg = false) : m_mag(), m_neg(false) { assign_from(mag, neg); }
    BigInt(BigUInt&& mag, bool neg = false)      : m_mag(), m_neg(false) { assign_from(std::move(mag), neg); }

    template <std::size_t Bits, sign S, typename = typename std::enable_if<S == sign::is_unsigned>::type>
    BigInt(const integer<Bits, S>& src) : m_mag(src), m_neg(false) {}

    template <std::size_t Bits, sign S, typename = typename std::enable_if<S == sign::is_signed>::type, typename = void>
    BigInt(const integer<Bits, S>& src) : m_mag(), m_neg(false) {
        if (src.is_undefined()) { *this = undefined(); return; }
        m_mag = BigUInt(src.abs());
        m_neg = src.is_negative() && !m_mag.is_zero();
    }

    explicit BigInt(const std::string& str, long long base = 10) : m_mag(), m_neg(false) {
        if (!parse_string(str, base)) *this = undefined();
    }

public:
    static BigInt zero() { return BigInt(); }
    static BigInt one()  { return make(BigUInt::one(), false); }
    static BigInt undefined() { BigInt r; r.m_mag = BigUInt::undefined(); r.m_neg = false; return r; }
    static BigInt nan()       { BigInt r; r.m_mag = BigUInt::undefined(); r.m_neg = true;  return r; }
    static BigInt max() { return make(BigUInt::max(), false); }
    static BigInt min() { return make(BigUInt::max(), true);  }
    static BigInt from_magnitude(const BigUInt& mag, bool neg) { return make(mag, neg); }
    static BigInt from_magnitude(BigUInt&& mag, bool neg)      { return make(std::move(mag), neg); }

public:
    const BigUInt& magnitude()    const noexcept { return m_mag; }
    bool           is_nan()       const noexcept { return m_mag.is_undefined() && m_neg;  }
    bool           is_undefined() const noexcept { return m_mag.is_undefined() && !m_neg; }
    bool           is_finite()    const noexcept { return !m_mag.is_undefined(); }
    bool           is_negative()  const noexcept { return is_finite() && m_neg; }
    bool           is_positive()  const noexcept { return is_finite() && !m_neg && !m_mag.is_zero(); }
    bool           is_zero()      const noexcept { return is_finite() && m_mag.is_zero(); }
    bool           is_one()       const noexcept { return is_finite() && !m_neg && m_mag.is_one(); }
    bool           is_even()      const noexcept { return is_finite() && m_mag.is_even(); }
    bool           is_odd()       const noexcept { return is_finite() && m_mag.is_odd(); }
    std::size_t    bit_length()   const noexcept { return is_finite() ? m_mag.bit_length() : 0; }

    std::size_t limb_count() const noexcept { return m_mag.limb_count(); }
    std::uint64_t get_lowest_bits() const noexcept { return m_mag.get_lowest_bits(); }
    std::uint64_t get_highest_bits() const noexcept { return m_mag.get_highest_bits(); }

    bool get_bit(std::size_t i) const noexcept { return is_finite() && m_mag.get_bit(i); }

    void set_bit(std::size_t i, bool b = true) {
        if (!is_finite()) return;
        m_mag.set_bit(i, b);
        normalize();
    }

public:
    BigInt& negate_mutable() noexcept {
        if (is_finite() && !m_mag.is_zero()) m_neg = !m_neg;
        return *this;
    }

    BigInt& abs_mutable() noexcept {
        if (is_finite()) m_neg = false;
        return *this;
    }

    BigInt& add_mutable(const BigInt& o) {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        return add_signed(o.m_mag, o.m_neg);
    }

    BigInt& sub_mutable(const BigInt& o) {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        if (this == &o) { m_mag = BigUInt::zero(); m_neg = false; return *this; }
        return add_signed(o.m_mag, !o.m_neg);
    }

    BigInt& mul_mutable(const BigInt& o) {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        return assign_from(m_mag * o.m_mag, m_neg != o.m_neg);
    }

    BigInt& div_mutable(const BigInt& o) {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return *this = undefined();
        return assign_from(m_mag / o.m_mag, m_neg != o.m_neg);
    }

    BigInt& mod_mutable(const BigInt& o) {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return *this = undefined();
        return assign_from(m_mag % o.m_mag, m_neg);
    }

    BigInt& shift_left_mutable(std::size_t n) {
        if (!is_finite()) return *this;
        m_mag.shift_left_mutable(n);
        normalize();
        return *this;
    }

    BigInt& shift_right_mutable(std::size_t n) {
        if (!is_finite()) return *this;
        m_mag.shift_right_mutable(n);
        normalize();
        return *this;
    }

public:
    bool is_exact_division(const BigInt& o) const {
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return false;
        return (m_mag % o.m_mag).is_zero();
    }

    BigInt floor_div(const BigInt& o) const {
        if (is_nan() || o.is_nan()) return nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return undefined();
        BigInt q = make(m_mag / o.m_mag, m_neg != o.m_neg);
        if ((m_neg != o.m_neg) && !is_exact_division(o)) q.add_signed(BigUInt::one(), true);
        return q;
    }

public:
    double to_double() const noexcept {
        if (!is_finite()) return std::numeric_limits<double>::quiet_NaN();
        double result = 0.0;
        for (std::size_t i = m_mag.limb_count(); i > 0; --i) { result = result * 18446744073709551616.0 + static_cast<double>(m_mag.limb(i - 1)); }
        return m_neg ? -result : result;
    }

    long double to_long_double() const noexcept { return static_cast<long double>(to_double()); }

    std::string to_string(long long base = 10) const {
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (base < 2 || base > 36) return "";
        if (m_mag.is_zero()) return "0";
        BigUInt t = m_mag;
        const std::uint64_t b = static_cast<std::uint64_t>(base);
        std::string s;

        while (!t.is_zero()) {
            BigUInt r = t;
            r.mod_small_mutable(b);
            const std::uint64_t d = r.limb(0);
            s.push_back(d < 10 ? char('0' + d) : char('a' + (d - 10)));
            t.div_small_mutable(b);
        }

        if (m_neg) s.push_back('-');
        for (std::size_t i = 0, j = s.size(); i < j; ++i, --j) { char c = s[i]; s[i] = s[j - 1]; s[j - 1] = c; }
        return s;
    }

    static BigInt from_hex(const std::string& s)     { return BigInt(s, 16); }
    static BigInt from_decimal(const std::string& s) { return BigInt(s, 10); }
    static BigInt from_binary(const std::string& s)  { return BigInt(s, 2);  }

    static bool is_valid_string(const std::string& s, long long base) {
        BigInt t;
        return t.parse_string(s, base, true);
    }

    bool parse_string(const std::string& str, long long base, bool validate_only = false) {
        if (str.empty() || base < 2 || base > 36) return false;
        std::size_t start = 0;
        bool negative = false;
        if (str[0] == '-') { negative = true; start = 1; }
        else if (str[0] == '+') start = 1;
        if (start >= str.size()) return false;
        if (base == 16 && str.size() > start + 2 && str[start] == '0' && (str[start + 1] == 'x' || str[start + 1] == 'X')) start += 2;
        BigUInt acc = BigUInt::zero();
        const std::uint64_t b = static_cast<std::uint64_t>(base);
        bool found = false;

        for (std::size_t i = start; i < str.size(); ++i) {
            const char c = str[i];
            long long d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else if (c == '_' || c == ',' || c == '\'') continue;
            else return false;
            if (d < 0 || d >= base) return false;
            found = true;
            if (!validate_only) { acc.mul_small_mutable(b); acc.add_small_mutable(static_cast<std::uint64_t>(d)); }
        }

        if (found && !validate_only) assign_from(std::move(acc), negative);
        return found;
    }
};

inline bool operator==(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return a.is_negative() == b.is_negative() && a.magnitude() == b.magnitude();
}

inline bool operator!=(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return !(a.is_negative() == b.is_negative() && a.magnitude() == b.magnitude());
}

inline bool operator<(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    if (a.is_negative() != b.is_negative()) return a.is_negative();
    return a.is_negative() ? (b.magnitude() < a.magnitude()) : (a.magnitude() < b.magnitude());
}

inline bool operator>(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return b < a;
}

inline bool operator<=(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return !(b < a);
}

inline bool operator>=(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return !(a < b);
}

inline BigInt operator+(const BigInt& a) { return a; }
inline BigInt operator+(BigInt&& a)      { return std::move(a); }

inline BigInt operator-(const BigInt& a) { BigInt r(a); r.negate_mutable(); return r; }
inline BigInt operator-(BigInt&& a)      { a.negate_mutable(); return std::move(a); }

inline BigInt operator+(const BigInt& a, const BigInt& b) { BigInt r(a); r.add_mutable(b); return r; }
inline BigInt operator+(BigInt&& a,      const BigInt& b) { a.add_mutable(b); return std::move(a); }
inline BigInt operator+(const BigInt& a, BigInt&& b)      { b.add_mutable(a); return std::move(b); }
inline BigInt operator+(BigInt&& a,      BigInt&& b)      { a.add_mutable(b); return std::move(a); }

inline BigInt operator-(const BigInt& a, const BigInt& b) { BigInt r(a); r.sub_mutable(b); return r; }
inline BigInt operator-(BigInt&& a,      const BigInt& b) { a.sub_mutable(b); return std::move(a); }
inline BigInt operator-(BigInt&& a,      BigInt&& b)      { a.sub_mutable(b); return std::move(a); }

inline BigInt operator-(const BigInt& a, BigInt&& b) {
    if (&a == &b) { b.sub_mutable(a); return std::move(b); }
    b.negate_mutable();
    b.add_mutable(a);
    return std::move(b);
}

inline BigInt operator*(const BigInt& a, const BigInt& b) { BigInt r(a); r.mul_mutable(b); return r; }
inline BigInt operator*(BigInt&& a,      const BigInt& b) { a.mul_mutable(b); return std::move(a); }
inline BigInt operator*(const BigInt& a, BigInt&& b)      { b.mul_mutable(a); return std::move(b); }
inline BigInt operator*(BigInt&& a,      BigInt&& b)      { a.mul_mutable(b); return std::move(a); }

inline BigInt operator/(const BigInt& a, const BigInt& b) { BigInt r(a); r.div_mutable(b); return r; }
inline BigInt operator/(BigInt&& a,      const BigInt& b) { a.div_mutable(b); return std::move(a); }

inline BigInt operator%(const BigInt& a, const BigInt& b) { BigInt r(a); r.mod_mutable(b); return r; }
inline BigInt operator%(BigInt&& a,      const BigInt& b) { a.mod_mutable(b); return std::move(a); }

inline BigInt& operator+=(BigInt& a, const BigInt& b) { return a.add_mutable(b); }
inline BigInt& operator-=(BigInt& a, const BigInt& b) { return a.sub_mutable(b); }
inline BigInt& operator*=(BigInt& a, const BigInt& b) { return a.mul_mutable(b); }
inline BigInt& operator/=(BigInt& a, const BigInt& b) { return a.div_mutable(b); }
inline BigInt& operator%=(BigInt& a, const BigInt& b) { return a.mod_mutable(b); }

inline BigInt& operator++(BigInt& a)    { return a.add_mutable(BigInt(std::uint64_t(1))); }
inline BigInt& operator--(BigInt& a)    { return a.sub_mutable(BigInt(std::uint64_t(1))); }
inline BigInt  operator++(BigInt& a, int) { BigInt t(a); ++a; return t; }
inline BigInt  operator--(BigInt& a, int) { BigInt t(a); --a; return t; }

inline BigInt  operator<<(const BigInt& a, std::size_t n) { BigInt r(a); r.shift_left_mutable(n); return r; }
inline BigInt  operator<<(BigInt&& a,      std::size_t n) { a.shift_left_mutable(n); return std::move(a); }
inline BigInt  operator>>(const BigInt& a, std::size_t n) { BigInt r(a); r.shift_right_mutable(n); return r; }
inline BigInt  operator>>(BigInt&& a,      std::size_t n) { a.shift_right_mutable(n); return std::move(a); }
inline BigInt& operator<<=(BigInt& a, std::size_t n) { return a.shift_left_mutable(n); }
inline BigInt& operator>>=(BigInt& a, std::size_t n) { return a.shift_right_mutable(n); }

#define FIZMO_BIGINT_BINOP(OP)                                                        \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, BigInt>::type                 \
    operator OP(const BigInt& a, T v) { BigInt r(a); r OP##= BigInt(v); return r; }   \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, BigInt>::type                 \
    operator OP(BigInt&& a, T v) { a OP##= BigInt(v); return std::move(a); }          \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, BigInt>::type                 \
    operator OP(T v, const BigInt& a) { BigInt r(v); r OP##= a; return r; }

FIZMO_BIGINT_BINOP(+)
FIZMO_BIGINT_BINOP(-)
FIZMO_BIGINT_BINOP(*)
FIZMO_BIGINT_BINOP(/)
FIZMO_BIGINT_BINOP(%)

#undef FIZMO_BIGINT_BINOP

#define FIZMO_BIGINT_CMP(OP)                                                          \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, bool>::type                   \
    operator OP(const BigInt& a, T v) noexcept { return a OP BigInt(v); }             \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, bool>::type                   \
    operator OP(T v, const BigInt& a) noexcept { return BigInt(v) OP a; }

FIZMO_BIGINT_CMP(==)
FIZMO_BIGINT_CMP(!=)
FIZMO_BIGINT_CMP(<)
FIZMO_BIGINT_CMP(>)
FIZMO_BIGINT_CMP(<=)
FIZMO_BIGINT_CMP(>=)

#undef FIZMO_BIGINT_CMP

inline std::ostream& operator<<(std::ostream& os, const BigInt& x) { return os << x.to_string(); }

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_INTEGER_CLASS_HPP