#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <limits>
#include <string>
#include <ostream>
#include <cmath>
#include <utility>

#include "big_int.hpp"
#include "../Fixed Width Float/traits.hpp"

namespace fizmo {
namespace multiprecision {

enum class RoundingMode {
    nearest_even = 0, // ties to even significand 
    nearest_away,     // ties away from zero
    toward_zero,      // truncate
    toward_pos_inf,   // ceiling
    toward_neg_inf    // floor
};

struct BigFloatContext {
    static constexpr long double log2_10      = 3.321928094887362347870319429489L;
    static constexpr long double log10_2      = 0.301029995663981195213738894724L;
    static constexpr std::size_t min_prec     = 2;
    static constexpr std::size_t default_prec = 53; // double's significant precision
    static constexpr std::size_t max_prec     = BigUInt::max_bits;
    static constexpr std::size_t max_dps      = std::size_t(static_cast<long double>(max_prec) * log10_2);

    std::size_t  precision;
    RoundingMode rounding_mode;

    constexpr BigFloatContext() noexcept : precision(default_prec), rounding_mode(RoundingMode::nearest_even) {}
    explicit constexpr BigFloatContext(std::size_t p, RoundingMode r = RoundingMode::nearest_even) noexcept : precision(clamp_precision(p)), rounding_mode(r) {}

    static constexpr std::size_t clamp_precision(std::size_t p) noexcept { return p < min_prec ? min_prec : (p > max_prec ? max_prec : p); }
    static constexpr std::size_t precision_from_dps(std::size_t dps) noexcept { return clamp_precision(static_cast<std::size_t>(static_cast<long double>(dps) * log2_10) + 2); }

    static constexpr std::size_t dps_from_precision(std::size_t prec) noexcept {
        const std::size_t d = static_cast<std::size_t>(static_cast<long double>(prec) * log10_2);
        return d == 0 ? 1 : d;
    }

    std::size_t dps() const noexcept { return dps_from_precision(precision); }

    static constexpr BigFloatContext with_dps(std::size_t dps, RoundingMode r = RoundingMode::nearest_even) noexcept {
        return BigFloatContext(precision_from_dps(dps), r);
    }

    BigFloatContext extended(std::size_t guard_bits) const noexcept {
        return BigFloatContext(precision + guard_bits, rounding_mode);
    }

    static BigFloatContext& current() noexcept {
        static thread_local BigFloatContext ctx;
        return ctx;
    }

    static void set_current(const BigFloatContext& c) noexcept { current() = c; }
};

class ScopedContext {
private:
    BigFloatContext m_saved;

public:
    explicit ScopedContext(const BigFloatContext& c) noexcept : m_saved(BigFloatContext::current()) { BigFloatContext::current() = c; }

    explicit ScopedContext(std::size_t prec) noexcept;

    ScopedContext(std::size_t prec, RoundingMode rnd) noexcept;

    static ScopedContext extend(std::size_t guard_bits) noexcept {
        return ScopedContext(BigFloatContext::current().extended(guard_bits));
    }

    ~ScopedContext() noexcept { BigFloatContext::current() = m_saved; }

    ScopedContext(const ScopedContext&)            = delete;
    ScopedContext& operator=(const ScopedContext&) = delete;

    const BigFloatContext& saved() const noexcept { return m_saved; }
};

class BigFloat {
public:
    enum class fpclass : std::uint8_t { finite = 0, infinite, nan, undefined };
    enum class ordering : std::int8_t { less = -1, equal = 0, greater = 1, unordered = 2 };

    static constexpr std::int64_t exp_max         = std::int64_t(1) << 48;
    static constexpr std::int64_t exp_min         = -exp_max;
    static constexpr std::int64_t exp_inf         = std::numeric_limits<std::int64_t>::max(); // returned by get_exp_*, never stored
    static constexpr std::int64_t exp_none        = std::numeric_limits<std::int64_t>::min(); // returned by get_exp_*, never stored
    static constexpr std::size_t  pow10_cmp_cap   = std::size_t(1) << 20;
    static constexpr std::size_t  no_digit_limit  = ~std::size_t(0);
    static constexpr std::size_t  decimal_exp_cap = std::size_t(1) << 22;

private:
    BigUInt      m_mag;  // significand magnitude, always odd when finite and non-zero
    std::int64_t m_exp;  // value == (-1)^m_neg * m_mag * 2^m_exp
    bool         m_neg;  // sign bit; meaningful for zero, ignored for nan/undefined
    fpclass      m_cls;

    struct raw_tag {};

    BigFloat(raw_tag, BigUInt mag, bool neg, std::int64_t exp, fpclass cls) : m_mag(std::move(mag)), m_exp(exp), m_neg(neg), m_cls(cls) {}

    static std::int64_t add_sat(std::int64_t e, std::int64_t d) noexcept;

    BigFloat& set_finite(BigUInt mag, bool neg, std::int64_t exp);

    BigFloat& set_signed(const BigInt& v, std::int64_t exp);

    static void round_significand(BigUInt& mag, std::int64_t& exp, bool neg, std::size_t prec, bool sticky, RoundingMode mode);

    static BigUInt pow5(std::uint64_t n);

    void assign_decimal(BigUInt digits, bool neg, long long e10);

    void construct_from_long_double(long double value);

    static bool word_is(const char* s, const char* w) noexcept;

    bool parse_string(const char* s);

    int compare_abs_pow10(std::int64_t k, bool& ok) const;

    template <std::size_t Bits, sign S>
    typename std::enable_if<S == sign::is_unsigned, BigFloat&>::type
    assign_integer(const integer<Bits, S>& v, std::int64_t exp) {
        if (v.is_undefined()) return *this = undefined();
        return set_finite(BigUInt(v), false, exp);
    }

    template <std::size_t Bits, sign S>
    typename std::enable_if<S == sign::is_signed, BigFloat&>::type
    assign_integer(const integer<Bits, S>& v, std::int64_t exp) {
        if (v.is_undefined()) return *this = undefined();
        return set_finite(BigUInt(v.abs()), v.is_negative(), exp);
    }

    static BigFloat reciprocal_finite(const BigUInt& mag, bool neg, std::int64_t exp, const BigFloatContext& ctx);

    static BigUInt align_to(const BigUInt& m, std::int64_t e, std::int64_t E, bool& sticky);

    static BigFloat add_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
    );

    static BigFloat mul_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
    );

    static BigFloat add_signed_impl(const BigFloat& a, const BigFloat& b, bool flip_b, const BigFloatContext& ctx);

    static BigFloat divide_finite(
        const BigUInt& xm, bool xneg, std::int64_t xe,
        const BigUInt& ym, bool yneg, std::int64_t ye,
        const BigFloatContext& ctx
    );

    static int compare_aligned(const BigUInt& a, std::size_t La, const BigUInt& b, std::size_t Lb) noexcept;

    static std::string digits_of(BigUInt v);

    static std::string exponent_string(std::int64_t k);

    bool decimal_digits(std::size_t digits, bool truncate, std::string& out, std::int64_t& k, bool& inexact) const;

    std::string render(const std::string& d, std::int64_t k, bool scientific, bool ellipsis) const;

    std::string approximate_string() const;

public:
    BigFloat() : m_mag(BigUInt::zero()), m_exp(0), m_neg(false), m_cls(fpclass::finite) {}

    BigFloat(const BigFloat&)            = default;
    BigFloat(BigFloat&&)                 = default;
    BigFloat& operator=(const BigFloat&) = default;
    BigFloat& operator=(BigFloat&&)      = default;

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigFloat(T v) : BigFloat() { set_signed(BigInt(v), 0); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void>
    BigFloat(T v) : BigFloat() { construct_from_long_double(static_cast<long double>(v)); }

    BigFloat(const BigInt& mant, std::int64_t exp = 0) : BigFloat() { set_signed(mant, exp); }

    BigFloat(const BigUInt& mag, bool neg = false, std::int64_t exp = 0) : BigFloat() { set_finite(mag, neg, exp); }
    BigFloat(BigUInt&& mag, bool neg = false, std::int64_t exp = 0)      : BigFloat() { set_finite(std::move(mag), neg, exp); }

    template <std::size_t Bits, sign S>
    BigFloat(const integer<Bits, S>& v, std::int64_t exp = 0) : BigFloat() { assign_integer(v, exp); }

    template <std::size_t TB, std::size_t MB, sign S>
    BigFloat(const floatmp<TB, MB, S>& v) : BigFloat() {
        if (v.is_nan())       { *this = nan();                     return; }
        if (v.is_undefined()) { *this = undefined();               return; }
        if (v.is_infinite())  { *this = infinity(v.is_negative()); return; }
        if (v.is_zero())      { *this = zero(v.is_negative());     return; }
        const auto fx = v.frexp_signed();
        const auto  m = std::get<0>(fx);
        const auto  e = std::get<1>(fx);
        const bool  n = std::get<2>(fx);
        const std::int64_t ev = e.is_negative() ? -static_cast<std::int64_t>(e.get_lowest_bits()) : static_cast<std::int64_t>(e.get_lowest_bits());
        set_signed(BigInt(m, n), ev - static_cast<std::int64_t>(MB));
    }

    explicit BigFloat(const std::string& str) : BigFloat() { if (!parse_string(str.c_str())) *this = undefined(); }
    explicit BigFloat(const char* str)        : BigFloat() { if (!parse_string(str))         *this = undefined(); }

public:
    static BigFloat zero(bool neg = false)     { return BigFloat(raw_tag{}, BigUInt::zero(), neg,   0, fpclass::finite); }
    static BigFloat positive_zero()            { return zero(false); }
    static BigFloat negative_zero()            { return zero(true);  }
    static BigFloat one(bool neg = false)      { return BigFloat(raw_tag{}, BigUInt::one(),  neg,   0, fpclass::finite); }
    static BigFloat infinity(bool neg = false) { return BigFloat(raw_tag{}, BigUInt::zero(), neg,   0, fpclass::infinite); }
    static BigFloat positive_infinity()        { return infinity(false); }
    static BigFloat negative_infinity()        { return infinity(true);  }
    static BigFloat nan()                      { return BigFloat(raw_tag{}, BigUInt::zero(), false, 0, fpclass::nan); }
    static BigFloat undefined()                { return BigFloat(raw_tag{}, BigUInt::zero(), false, 0, fpclass::undefined); }

    static BigFloat ldexp(const BigUInt& mag, bool neg, std::int64_t exp) { return BigFloat(mag, neg, exp); }
    static BigFloat ldexp(BigUInt&& mag, bool neg, std::int64_t exp)      { return BigFloat(std::move(mag), neg, exp); }
    static BigFloat ldexp(const BigInt& mant, std::int64_t exp)           { return BigFloat(mant, exp); }
    static BigFloat ldexp(const BigFloat& x, std::int64_t n)              { return x.scaled2(n); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static BigFloat ldexp(T v, std::int64_t n) { return BigFloat(v).scaled2(n); }

    static BigFloat from_magnitude(const BigUInt& mag, bool neg = false, std::int64_t exp = 0) { return BigFloat(mag, neg, exp); }
    static BigFloat from_magnitude(BigUInt&& mag, bool neg = false, std::int64_t exp = 0)      { return BigFloat(std::move(mag), neg, exp); }

    static BigFloat from_string(const std::string& s) { return BigFloat(s); }

public:
    const BigUInt& significand()      const noexcept { return m_mag; }
    std::size_t    significand_bits() const noexcept { return m_mag.bit_length(); }
    std::int64_t   exponent()         const noexcept { return m_exp; }
    bool           signbit()          const noexcept { return m_neg; }
    fpclass        classify()         const noexcept { return m_cls; }

    BigInt mantissa() const;

    BigInt get_significand_int() const { return mantissa(); }

    bool is_nan()           const noexcept { return m_cls == fpclass::nan; }
    bool is_undefined()     const noexcept { return m_cls == fpclass::undefined; }
    bool is_infinite()      const noexcept { return m_cls == fpclass::infinite; }
    bool is_finite()        const noexcept { return m_cls == fpclass::finite; }
    bool is_special()       const noexcept { return m_cls != fpclass::finite; }
    bool is_zero()          const noexcept { return m_cls == fpclass::finite && m_mag.is_zero(); }
    bool is_negative_zero() const noexcept { return is_zero() &&  m_neg; }
    bool is_positive_zero() const noexcept { return is_zero() && !m_neg; }
    bool is_negative()      const noexcept { return m_neg && (m_cls == fpclass::finite || m_cls == fpclass::infinite); }
    bool is_positive()      const noexcept { return !m_neg && (m_cls == fpclass::finite || m_cls == fpclass::infinite); }
    bool is_integer()       const noexcept { return m_cls == fpclass::finite && (m_mag.is_zero() || m_exp >= 0); }
    bool is_comparable()    const noexcept { return m_cls == fpclass::finite || m_cls == fpclass::infinite; }

    int sign() const noexcept;

public:
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    BigFloat& operator=(T v) { return *this = BigFloat(v); }

    BigFloat& operator=(const BigInt& v)  { return set_signed(v, 0); }
    BigFloat& operator=(const BigUInt& v) { return set_finite(v, false, 0); }
    BigFloat& operator=(BigUInt&& v)      { return set_finite(std::move(v), false, 0); }

    template <std::size_t Bits, multiprecision::sign S>
    BigFloat& operator=(const integer<Bits, S>& v) { return assign_integer(v, 0); }

    template <std::size_t TB, std::size_t MB, multiprecision::sign S>
    BigFloat& operator=(const floatmp<TB, MB, S>& v) { return *this = BigFloat(v); }

    BigFloat& operator=(const std::string& s) { if (!parse_string(s.c_str())) *this = undefined(); return *this; }
    BigFloat& operator=(const char* s)        { if (!parse_string(s))         *this = undefined(); return *this; }

public:
    BigFloat abs() const;

    BigFloat with_sign(bool neg) const;

    BigFloat& negate_mutable()          noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = !m_neg; return *this; }
    BigFloat& abs_mutable()             noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = false;  return *this; }
    BigFloat& set_sign_mutable(bool n)  noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = n;      return *this; }

    static BigFloat copysign(const BigFloat& x, const BigFloat& y) { return x.with_sign(y.signbit()); }
    
public:
    static ordering compare(const BigFloat& a, const BigFloat& b) noexcept;

    ordering compare(const BigFloat& o) const noexcept { return compare(*this, o); }

    static bool unordered(const BigFloat& a, const BigFloat& b) noexcept {
        return compare(a, b) == ordering::unordered;
    }

public:
    static int total_order(const BigFloat& a, const BigFloat& b) noexcept;

    struct total_less {
        bool operator()(const BigFloat& a, const BigFloat& b) const noexcept {
            return total_order(a, b) < 0;
        }
    };

public:
    static ordering signbit_order(const BigFloat& a, const BigFloat& b) noexcept;

    ordering signbit_order(const BigFloat& o) const noexcept { return signbit_order(*this, o); }

    bool same_signbit(const BigFloat& o) const noexcept {
        return signbit_order(*this, o) == ordering::equal;
    }

    int signbit_compare(const BigFloat& other) const noexcept;

public:
    std::pair<BigInt, std::int64_t> frexp() const {
        return std::pair<BigInt, std::int64_t>(mantissa(), m_exp);
    }

    std::tuple<BigUInt, std::int64_t, bool> frexp_signed() const {
        return std::tuple<BigUInt, std::int64_t, bool>(m_mag, m_exp, m_neg);
    }

    std::pair<BigFloat, std::int64_t> frexp_normalized() const;

    std::tuple<BigFloat, std::int64_t, bool> frexp_signed_normalized() const;

    BigFloat get_significand() const;

    std::int64_t get_exp_base2() const noexcept;

    std::int64_t get_exp_low_bit() const noexcept { return (is_finite() && !is_zero()) ? m_exp : exp_none; }

    std::int64_t approximate_exp_base10() const noexcept;

    std::int64_t get_exp_base10() const;

    // value * 2^n
    BigFloat scaled2(std::int64_t n) const;

    BigFloat& scale2_mutable(std::int64_t n);

    BigInt get_integer_part() const;

    BigFloat get_fractional_part() const;

    BigFloat rounded(const BigFloatContext& ctx) const;

    BigFloat rounded() const { return rounded(BigFloatContext::current()); }

    BigFloat scaled_pow2(std::int64_t k) const;

    BigFloat& scale_pow2_mutable(std::int64_t k) {
        if (m_cls == fpclass::finite && !m_mag.is_zero()) m_exp = add_sat(m_exp, k);
        return *this;
    }

public:
    BigFloat reciprocal(const BigFloatContext& ctx) const;

    BigFloat reciprocal() const { return reciprocal(BigFloatContext::current()); }

    BigFloat& reciprocal_inplace(const BigFloatContext& ctx) { return *this = reciprocal(ctx); }
    BigFloat& reciprocal_inplace() { return reciprocal_inplace(BigFloatContext::current()); }

public:
    static BigFloat add(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        return add_signed_impl(a, b, false, ctx);
    }

    static BigFloat sub(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        return add_signed_impl(a, b, true, ctx);
    }

    static BigFloat mul(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

    static BigFloat div(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx);

    static BigFloat add(const BigFloat& a, const BigFloat& b) { return add(a, b, BigFloatContext::current()); }
    static BigFloat sub(const BigFloat& a, const BigFloat& b) { return sub(a, b, BigFloatContext::current()); }
    static BigFloat mul(const BigFloat& a, const BigFloat& b) { return mul(a, b, BigFloatContext::current()); }
    static BigFloat div(const BigFloat& a, const BigFloat& b) { return div(a, b, BigFloatContext::current()); }

    BigFloat& add_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = add(*this, o, c); }
    BigFloat& sub_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = sub(*this, o, c); }
    BigFloat& mul_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = mul(*this, o, c); }
    BigFloat& div_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = div(*this, o, c); }

    BigFloat& add_mutable(const BigFloat& o) { return add_mutable(o, BigFloatContext::current()); }
    BigFloat& sub_mutable(const BigFloat& o) { return sub_mutable(o, BigFloatContext::current()); }
    BigFloat& mul_mutable(const BigFloat& o) { return mul_mutable(o, BigFloatContext::current()); }
    BigFloat& div_mutable(const BigFloat& o) { return div_mutable(o, BigFloatContext::current()); }

public:
    std::string to_string_scientific(std::size_t max_digits = no_digit_limit, std::size_t digits = 0) const;

    std::string to_string(std::size_t max_digits = no_digit_limit, std::int64_t scientific_notation_exp = -5, std::size_t digits = 0) const;

    inline friend std::ostream& operator<<(std::ostream& os, const BigFloat& x) { return os << x.to_string(); }
};

inline bool operator==(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::equal;
}

inline bool operator!=(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) != BigFloat::ordering::equal;   // unordered => true
}

inline bool operator<(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::less;
}

inline bool operator>(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::greater;
}

 bool operator<=(const BigFloat& a, const BigFloat& b) noexcept;

 bool operator>=(const BigFloat& a, const BigFloat& b) noexcept;

inline BigFloat operator+(const BigFloat& a) { return a; }
inline BigFloat operator+(BigFloat&& a)      { return std::move(a); }

inline BigFloat operator-(const BigFloat& a) { BigFloat r(a); r.negate_mutable(); return r; }
inline BigFloat operator-(BigFloat&& a)      { a.negate_mutable(); return std::move(a); }

inline BigFloat operator+(const BigFloat& a, const BigFloat& b) { return BigFloat::add(a, b); }
inline BigFloat operator+(BigFloat&& a,      const BigFloat& b) { a.add_mutable(b); return std::move(a); }
inline BigFloat operator+(const BigFloat& a, BigFloat&& b)      { b.add_mutable(a); return std::move(b); }
inline BigFloat operator+(BigFloat&& a,      BigFloat&& b)      { a.add_mutable(b); return std::move(a); }

inline BigFloat operator-(const BigFloat& a, const BigFloat& b) { return BigFloat::sub(a, b); }
inline BigFloat operator-(BigFloat&& a,      const BigFloat& b) { a.sub_mutable(b); return std::move(a); }
inline BigFloat operator-(BigFloat&& a,      BigFloat&& b)      { a.sub_mutable(b); return std::move(a); }

inline BigFloat operator-(const BigFloat& a, BigFloat&& b) {
    b = BigFloat::sub(a, b);   
    return std::move(b);
}

inline BigFloat operator*(const BigFloat& a, const BigFloat& b) { return BigFloat::mul(a, b); }
inline BigFloat operator*(BigFloat&& a,      const BigFloat& b) { a.mul_mutable(b); return std::move(a); }
inline BigFloat operator*(const BigFloat& a, BigFloat&& b)      { b.mul_mutable(a); return std::move(b); }
inline BigFloat operator*(BigFloat&& a,      BigFloat&& b)      { a.mul_mutable(b); return std::move(a); }

inline BigFloat operator/(const BigFloat& a, const BigFloat& b) { return BigFloat::div(a, b); }
inline BigFloat operator/(BigFloat&& a,      const BigFloat& b) { a.div_mutable(b); return std::move(a); }
inline BigFloat operator/(BigFloat&& a,      BigFloat&& b)      { a.div_mutable(b); return std::move(a); }

inline BigFloat operator/(const BigFloat& a, BigFloat&& b) {
    b = BigFloat::div(a, b);
    return std::move(b);
}

inline BigFloat& operator+=(BigFloat& a, const BigFloat& b) { return a.add_mutable(b); }
inline BigFloat& operator-=(BigFloat& a, const BigFloat& b) { return a.sub_mutable(b); }
inline BigFloat& operator*=(BigFloat& a, const BigFloat& b) { return a.mul_mutable(b); }
inline BigFloat& operator/=(BigFloat& a, const BigFloat& b) { return a.div_mutable(b); }

inline BigFloat& operator++(BigFloat& a) { return a.add_mutable(BigFloat::one()); }
inline BigFloat& operator--(BigFloat& a) { return a.sub_mutable(BigFloat::one()); }
inline BigFloat  operator++(BigFloat& a, int) { BigFloat t(a); ++a; return t; }
inline BigFloat  operator--(BigFloat& a, int) { BigFloat t(a); --a; return t; }

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP