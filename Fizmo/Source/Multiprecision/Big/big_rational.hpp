#ifndef MULTIPRECISION_BIG_RATIONAL_HPP
#define MULTIPRECISION_BIG_RATIONAL_HPP

#include "big_float.hpp"

namespace fizmo {
namespace multiprecision {

class BigRational {
private:
    BigFloat    m_numer;          
    BigFloat    m_denom;          
    std::size_t m_precision_bits;

    static constexpr std::size_t default_precision() noexcept { return 256; }

    std::size_t prec() const noexcept {
        if (BigFloatContext::is_active() && BigFloatContext::precision() != 0) return BigFloatContext::precision();
        return m_precision_bits ? m_precision_bits : default_precision();
    }

    void normalize_sign() noexcept {
        if (m_denom.is_negative()) {
            m_numer.negate();
            m_denom.abs_in_place();
        }
    }

    static std::size_t pick_prec(const BigRational& a, const BigRational& b) noexcept {
        if (BigFloatContext::is_active() && BigFloatContext::precision() != 0) return BigFloatContext::precision();
        std::size_t pa = a.m_precision_bits ? a.m_precision_bits : default_precision();
        std::size_t pb = b.m_precision_bits ? b.m_precision_bits : default_precision();
        return pa > pb ? pa : pb;
    }

    static std::size_t pick_prec(const BigRational& a, const BigFloat& b) noexcept {
        if (BigFloatContext::is_active() && BigFloatContext::precision() != 0) return BigFloatContext::precision();
        std::size_t pa = a.m_precision_bits ? a.m_precision_bits : default_precision();
        std::size_t pb = BigFloat::effective_precision_bits(b);
        return pa > pb ? pa : pb;
    }

public:
    BigRational() noexcept
        : m_numer(0), m_denom(1),
          m_precision_bits(BigFloatContext::is_active() ? BigFloatContext::precision() : default_precision()) {}

    BigRational(BigFloat numer, BigFloat denom)
        : m_numer(std::move(numer)), m_denom(std::move(denom)),
          m_precision_bits(BigFloatContext::is_active() ? BigFloatContext::precision() : default_precision())
    { normalize_sign(); }

    explicit BigRational(const BigFloat& f)
        : m_numer(f), m_denom(BigFloat(1)),
          m_precision_bits(BigFloat::effective_precision_bits(f))
    { normalize_sign(); }

    explicit BigRational(const BigInt& i)  : BigRational(BigFloat(i))  {}
    explicit BigRational(const BigUint& u) : BigRational(BigFloat(u))  {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value || std::is_floating_point<T>::value>::type>
    BigRational(T value) : BigRational(BigFloat(value)) {}

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    explicit BigRational(const T& value) : BigRational(BigFloat(value)) {}

    BigRational(const std::string& s)
        : m_denom(BigFloat(1)),
          m_precision_bits(BigFloatContext::is_active() ? BigFloatContext::precision() : default_precision())
    {
        auto slash = s.find('/');
        if (slash == std::string::npos) {
            m_numer = BigFloat(s);
        } else {
            m_numer = BigFloat(s.substr(0, slash));
            m_denom = BigFloat(s.substr(slash + 1));
        }
        normalize_sign();
    }

    BigRational(const BigRational&)            = default;
    BigRational(BigRational&&) noexcept        = default;
    BigRational& operator=(const BigRational&) = default;
    BigRational& operator=(BigRational&&)      = default;
    BigRational& operator=(const BigFloat& f)  { *this = BigRational(f); return *this; }
    BigRational& operator=(const BigInt& i)    { *this = BigRational(i); return *this; }
    BigRational& operator=(const BigUint& u)   { *this = BigRational(u); return *this; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value || std::is_floating_point<T>::value>::type>
    BigRational& operator=(T v) { *this = BigRational(v); return *this; }

public:
    void        set_precision_bits(std::size_t bits) noexcept { m_precision_bits = bits; }
    void        set_precision_decimals(std::size_t n) noexcept { m_precision_bits = BigFloat::decimal_digits_to_bits(n); }
    std::size_t precision_bits() const noexcept { return prec(); }

public:
    const BigFloat& numerator()   const noexcept { return m_numer; }
    const BigFloat& denominator() const noexcept { return m_denom; }

public:
    bool is_nan() const noexcept { return m_numer.is_nan() || m_denom.is_nan(); }
    bool is_undefined() const noexcept { return !is_nan() && m_numer.is_zero() && m_denom.is_zero(); }
    bool is_infinite() const noexcept { return !is_nan() && !m_numer.is_zero() && m_denom.is_zero(); }
    bool is_positive_infinity() const noexcept { return is_infinite() && !m_numer.is_negative(); }
    bool is_negative_infinity() const noexcept { return is_infinite() &&  m_numer.is_negative(); }
    bool is_finite() const noexcept { return !is_nan() && !is_undefined() && !is_infinite(); }
    bool is_zero()   const noexcept { return is_finite() && m_numer.is_zero(); }
    bool is_negative() const noexcept { return m_numer.is_negative(); }
    static BigRational nan() { return BigRational(BigFloat::nan(), BigFloat(1)); }
    static BigRational undefined() { return BigRational(BigFloat::zero(), BigFloat::zero()); }
    static BigRational positive_infinity() { return BigRational(BigFloat(1), BigFloat::zero()); }
    static BigRational negative_infinity() { return BigRational(BigFloat(-1), BigFloat::zero()); }
    static BigRational zero() { return BigRational(BigFloat::zero(), BigFloat(1)); }

public:
    BigFloat to_float() const {
        if (is_nan())       return BigFloat::nan();
        if (is_undefined()) return BigFloat::undefined();
        if (is_infinite())  return m_numer.is_negative() ? BigFloat::negative_infinity() : BigFloat::positive_infinity();
        if (is_zero())      return BigFloat::zero();
        std::size_t p = prec();
        BigFloat n = m_numer, d = m_denom;
        n.set_precision_bits(p);
        d.set_precision_bits(p);
        return n / d;
    }

    BigFloat to_float(std::size_t bits) const {
        BigRational tmp = *this;
        tmp.m_precision_bits = bits;
        return tmp.to_float();
    }

    explicit operator BigFloat() const { return to_float(); }

public:
    void negate()       noexcept { m_numer.negate(); }
    void abs_in_place() noexcept { m_numer.abs_in_place(); }
    BigRational operator-() const { BigRational r = *this; r.negate();       return r; }
    BigRational operator+() const { return *this; }

public:
    BigRational operator+(const BigRational& o) const {
        if (is_nan() || o.is_nan()) return nan();
        std::size_t p = pick_prec(*this, o);
        if (is_undefined() || o.is_undefined()) return undefined();

        if (is_infinite() || o.is_infinite()) {
            if (is_infinite() && o.is_infinite()) {
                if (m_numer.is_negative() != o.m_numer.is_negative()) return undefined();
                return *this;
            }
            return is_infinite() ? *this : o;
        }

        if (is_zero()) { BigRational r = o; r.m_precision_bits = p; return r; }
        if (o.is_zero()) { BigRational r = *this; r.m_precision_bits = p; return r; }
        BigFloat num = m_numer * o.m_denom + o.m_numer * m_denom;
        BigFloat den = m_denom * o.m_denom;
        return BigRational(std::move(num), std::move(den));
    }

    BigRational& operator+=(const BigRational& o) { *this = *this + o; return *this; }

    BigRational operator-(const BigRational& o) const {
        BigRational neg = o; neg.negate();
        return *this + neg;
    }

    BigRational& operator-=(const BigRational& o) { *this = *this - o; return *this; }

    BigRational operator*(const BigRational& o) const {
        if (is_nan() || o.is_nan()) return nan();
        if (is_undefined() || o.is_undefined()) return undefined();

        if (is_infinite() || o.is_infinite()) {
            if (is_zero() || o.is_zero()) return undefined();
            bool neg = m_numer.is_negative() != o.m_numer.is_negative();
            return neg ? negative_infinity() : positive_infinity();
        }

        if (is_zero() || o.is_zero()) return zero();
        BigFloat num = m_numer * o.m_numer;
        BigFloat den = m_denom * o.m_denom;
        return BigRational(std::move(num), std::move(den));
    }

    BigRational& operator*=(const BigRational& o) { *this = *this * o; return *this; }

    BigRational operator/(const BigRational& o) const {
        BigRational recip(o.m_denom, o.m_numer); 
        return *this * recip;
    }

    BigRational& operator/=(const BigRational& o) { *this = *this / o; return *this; }

    BigRational reciprocal() const {
        if (is_nan())       return nan();
        if (is_undefined()) return undefined();
        if (is_zero())      return m_numer.is_negative() ? negative_infinity() : positive_infinity();
        if (is_infinite())  return zero();
        BigRational r(m_denom, m_numer);
        r.m_precision_bits = m_precision_bits;
        return r;
    }

public:
    BigRational operator+(const BigFloat& f) const { return *this + BigRational(f); }
    BigRational operator-(const BigFloat& f) const { return *this - BigRational(f); }
    BigRational operator*(const BigFloat& f) const { return *this * BigRational(f); }
    BigRational operator/(const BigFloat& f) const { return *this / BigRational(f); }
    BigRational& operator+=(const BigFloat& f) { *this = *this + f; return *this; }
    BigRational& operator-=(const BigFloat& f) { *this = *this - f; return *this; }
    BigRational& operator*=(const BigFloat& f) { *this = *this * f; return *this; }
    BigRational& operator/=(const BigFloat& f) { *this = *this / f; return *this; }

public:
    bool operator==(const BigRational& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false;
        if (is_infinite() || o.is_infinite()) {
            if (!is_infinite() || !o.is_infinite()) return false;
            return m_numer.is_negative() == o.m_numer.is_negative();
        }
        if (is_zero() && o.is_zero()) return true;
        BigFloat lhs = m_numer * o.m_denom;
        BigFloat rhs = o.m_numer * m_denom;
        return lhs == rhs;
    }

    bool operator!=(const BigRational& o) const noexcept { return !(*this == o); }

    bool operator<(const BigRational& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false;
        if (is_infinite() || o.is_infinite()) {
            if (is_infinite() && o.is_infinite()) return m_numer.is_negative() && !o.m_numer.is_negative();
            if (is_infinite()) return m_numer.is_negative();
            return !o.m_numer.is_negative();
        }
        if (is_zero() && o.is_zero()) return false;
        BigFloat lhs = m_numer * o.m_denom;
        BigFloat rhs = o.m_numer * m_denom;
        return lhs < rhs;
    }

    bool operator>(const BigRational& o)  const noexcept { return o < *this; }
    bool operator<=(const BigRational& o) const noexcept { return !(*this > o); }
    bool operator>=(const BigRational& o) const noexcept { return !(*this < o); }
    bool operator==(const BigFloat& f) const noexcept { return *this == BigRational(f); }
    bool operator!=(const BigFloat& f) const noexcept { return *this != BigRational(f); }
    bool operator< (const BigFloat& f) const noexcept { return *this <  BigRational(f); }
    bool operator> (const BigFloat& f) const noexcept { return *this >  BigRational(f); }
    bool operator<=(const BigFloat& f) const noexcept { return *this <= BigRational(f); }
    bool operator>=(const BigFloat& f) const noexcept { return *this >= BigRational(f); }

public:
    std::string to_string(std::size_t decimals) const { return to_float().to_string(decimals); }
    std::string to_string() const { return to_float().to_string(); }

    std::string to_rational_string() const {
        if (is_nan())       return "NaN";
        if (is_undefined()) return "undefined";
        if (is_infinite())  return m_numer.is_negative() ? "-\u221e" : "\u221e";
        return "(" + m_numer.to_scientific_string() + ") / (" + m_denom.to_scientific_string() + ")";
    }

    std::string to_scientific_string(std::size_t sig_figs) const { return to_float().to_scientific_string(sig_figs); }
    std::string to_scientific_string() const { return to_float().to_scientific_string(); }
    friend std::ostream& operator<<(std::ostream& os, const BigRational& r) { os << r.to_scientific_string(); return os; }
};

inline BigRational operator+(const BigFloat& f, const BigRational& r) { return BigRational(f) + r; }
inline BigRational operator-(const BigFloat& f, const BigRational& r) { return BigRational(f) - r; }
inline BigRational operator*(const BigFloat& f, const BigRational& r) { return BigRational(f) * r; }
inline BigRational operator/(const BigFloat& f, const BigRational& r) { return BigRational(f) / r; }
inline bool operator==(const BigFloat& f, const BigRational& r) noexcept { return r == f; }
inline bool operator!=(const BigFloat& f, const BigRational& r) noexcept { return r != f; }
inline bool operator< (const BigFloat& f, const BigRational& r) noexcept { return r >  f; }
inline bool operator> (const BigFloat& f, const BigRational& r) noexcept { return r <  f; }
inline bool operator<=(const BigFloat& f, const BigRational& r) noexcept { return r >= f; }
inline bool operator>=(const BigFloat& f, const BigRational& r) noexcept { return r <= f; }

} // namespace multiprecision
} // namespace fizmo

#endif // MULTIPRECISION_BIG_RATIONAL_HPP