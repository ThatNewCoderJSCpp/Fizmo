#ifndef FIZMO_MATH_CAS_RATIONAL_HPP
#define FIZMO_MATH_CAS_RATIONAL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include "../../Multiprecision/Big/big_int.hpp"

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

class Rational {
public:
    static constexpr std::size_t max_power_bits = std::size_t(1) << 16;

    Rational() : m_num(), m_den(BigInt::one()) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    Rational(T v) : m_num(v), m_den(BigInt::one()) {}

    explicit Rational(const BigInt& n) : m_num(n), m_den(BigInt::one()) {}

    Rational(const BigInt& n, const BigInt& d);

    static Rational from_double(double v);

    static Rational from_double_exact(double v);

    static bool parse(const std::string& text, Rational& out);

    const BigInt& numerator()   const noexcept { return m_num; }
    const BigInt& denominator() const noexcept { return m_den; }

    bool is_zero()      const noexcept { return m_num.is_zero(); }
    bool is_one()       const noexcept { return m_num.is_one() && m_den.is_one(); }
    bool is_minus_one() const;
    bool is_integer()   const noexcept { return m_den.is_one(); }
    bool is_negative()  const noexcept { return m_num.is_negative(); }
    bool is_positive()  const noexcept { return m_num.is_positive(); }
    int  sign()         const noexcept { return m_num.is_negative() ? -1 : (m_num.is_zero() ? 0 : 1); }

    bool is_half_integer() const;

    bool to_int64(std::int64_t& out) const;

    double to_double() const;

    Rational abs() const;

    Rational reciprocal() const;

    BigInt floor() const;

    BigInt ceil() const;

    BigInt trunc() const;

    BigInt round() const;

    Rational fractional_part() const;

    bool pow(std::int64_t k, Rational& out) const;

    bool exact_root(std::uint64_t k, Rational& out) const;

    std::size_t hash() const noexcept;

    std::string to_string() const;

    friend Rational operator-(const Rational& a);
    friend Rational operator+(const Rational& a, const Rational& b);
    friend Rational operator-(const Rational& a, const Rational& b);
    friend Rational operator*(const Rational& a, const Rational& b);
    friend Rational operator/(const Rational& a, const Rational& b);

    Rational& operator+=(const Rational& o) { return *this = *this + o; }
    Rational& operator-=(const Rational& o) { return *this = *this - o; }
    Rational& operator*=(const Rational& o) { return *this = *this * o; }
    Rational& operator/=(const Rational& o) { return *this = *this / o; }

    friend bool operator==(const Rational& a, const Rational& b) { return a.m_num == b.m_num && a.m_den == b.m_den; }
    friend bool operator!=(const Rational& a, const Rational& b) { return !(a == b); }
    friend bool operator<(const Rational& a, const Rational& b);
    friend bool operator>(const Rational& a, const Rational& b)  { return b < a; }
    friend bool operator<=(const Rational& a, const Rational& b) { return !(b < a); }
    friend bool operator>=(const Rational& a, const Rational& b) { return !(a < b); }

    static BigInt int_pow(const BigInt& base, std::uint64_t k);

    static bool int_root(const BigInt& n, std::uint64_t k, BigInt& out);

    static BigInt gcd(const BigInt& a, const BigInt& b);

private:
    struct raw_tag {};

    Rational(raw_tag, BigInt n, BigInt d) : m_num(std::move(n)), m_den(std::move(d)) {}

    void normalize();

    BigInt m_num;
    BigInt m_den;
};

struct RationalHash {
    std::size_t operator()(const Rational& r) const noexcept { return r.hash(); }
};

} // namespace symbols

using symbols::Rational;

} // namespace cas
} // namespace math
} // namespace fizmo

#endif // FIZMO_MATH_CAS_RATIONAL_HPP
