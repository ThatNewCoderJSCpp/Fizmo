#ifndef MULTIPRECISION_BIG_FLOAT_HPP
#define MULTIPRECISION_BIG_FLOAT_HPP

#include "big_int.hpp"
#include "big_uint.hpp"
#include "../../Util Hpp/predicate.hpp"

namespace fizmo {
namespace multiprecision {

struct BigFloatContext {
    std::size_t precision_bits      = 0;
    std::size_t repr_precision_bits = 0;
    bool        active              = false;

    static BigFloatContext& current() {
        thread_local BigFloatContext ctx;
        return ctx;
    }

    static void set_precision(std::size_t bits) {
        auto& ctx = current();
        ctx.precision_bits = bits;
        ctx.active = (bits != 0 || ctx.repr_precision_bits != 0);
    }

    static void set_representative_precision(std::size_t bits) {
        auto& ctx = current();
        ctx.repr_precision_bits = bits;
        ctx.active = (ctx.precision_bits != 0 || bits != 0);
    }

    static void set_decimal_precision(std::size_t num_decimals);
    static void set_decimal_representative_precision(std::size_t num_decimals);

    static void clear() {
        auto& ctx = current();
        ctx.precision_bits      = 0;
        ctx.repr_precision_bits = 0;
        ctx.active              = false;
    }

    static bool        is_active()            { return current().active; }
    static std::size_t precision()            { return current().precision_bits; }
    static std::size_t represented_precision(){ return current().repr_precision_bits; }
};

class BigFloat {
private:
    BigUint      m_significand;
    std::int64_t m_exponent;
    bool         m_is_nan;
    bool         m_is_undefined;
    bool         m_is_inf;
    bool         m_is_negative;

    std::size_t  m_precision_bits;
    std::size_t  m_repr_precision_bits;

    static BigUint pow_biguint(const BigUint& base, std::size_t exp) {
        if (exp == 0) return BigUint(1);
        BigUint result(1), b = base;
        while (exp > 0) {
            if (exp & 1) result *= b;
            b *= b;
            exp >>= 1;
        }
        return result;
    }

    static const BigUint& cached_pow10(std::size_t decimals) {
        thread_local std::size_t s_dec = 0;
        thread_local BigUint     s_val;
        if (decimals != s_dec) { s_val = pow_biguint(BigUint(10), decimals); s_dec = decimals; }
        return s_val;
    }

    static constexpr std::size_t default_precision_bits() noexcept { return 256; }

    static std::size_t context_or_default_precision() noexcept {
        return BigFloatContext::is_active() ? BigFloatContext::precision() : default_precision_bits();
    }

    static std::size_t context_or_default_repr_precision() noexcept {
        return BigFloatContext::is_active() ? BigFloatContext::represented_precision() : 0;
    }

    static std::size_t combine_precisions(std::size_t a, std::size_t b) noexcept { return a > b ? a : b; }

    void divide_significands(const BigUint& num_sig, std::int64_t num_exp, const BigUint& den_sig, std::int64_t den_exp, std::size_t prec) {
        std::size_t nb    = num_sig.bit_length();
        std::size_t db    = den_sig.bit_length();
        std::size_t extra = prec + 32 + (db > nb ? db - nb : 0);
        BigUint scaled    = num_sig << static_cast<std::uint64_t>(extra);
        m_significand     = scaled / den_sig;
        m_exponent        = num_exp - den_exp - static_cast<std::int64_t>(extra);
        m_precision_bits  = prec;
        normalize();
    }

public:
    static std::size_t effective_precision_bits(const BigFloat& a) noexcept {
        if (BigFloatContext::is_active()) return BigFloatContext::precision();
        return a.m_precision_bits ? a.m_precision_bits : default_precision_bits();
    }

    static std::size_t effective_precision_bits(const BigFloat& a, const BigFloat& b) noexcept {
        if (BigFloatContext::is_active()) return BigFloatContext::precision();
        std::size_t pa = a.m_precision_bits ? a.m_precision_bits : default_precision_bits();
        std::size_t pb = b.m_precision_bits ? b.m_precision_bits : default_precision_bits();
        return combine_precisions(pa, pb);
    }

    static std::size_t effective_repr_precision_bits(const BigFloat& a) noexcept {
        if (BigFloatContext::is_active() && BigFloatContext::represented_precision() != 0) return BigFloatContext::represented_precision();
        if (a.m_repr_precision_bits != 0) return a.m_repr_precision_bits;
        return effective_precision_bits(a);
    }

    static std::size_t bits_to_decimal_digits(std::size_t bits) noexcept {
        return static_cast<std::size_t>(static_cast<double>(bits) * 0.301029995663981195214) + 2;
    }

    static std::size_t decimal_digits_to_bits(std::size_t dd) noexcept {
        return static_cast<std::size_t>(static_cast<double>(dd) * 3.32192809488736234787) + 2;
    }

public:
    static constexpr unsigned int radix = 2;

    BigFloat() noexcept
        : m_significand(), m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false), m_is_negative(false),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision()) {}

    BigFloat(const BigFloat&)                = default;
    BigFloat(BigFloat&&) noexcept            = default;
    BigFloat& operator=(const BigFloat&)     = default;
    BigFloat& operator=(BigFloat&&) noexcept = default;

    explicit BigFloat(const BigUint& u) noexcept
        : m_significand(u), m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false), m_is_negative(false),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision())
    { if (!u.is_zero()) normalize(); }

    explicit BigFloat(const BigInt& i) noexcept
        : m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false),
          m_is_negative(i.is_negative()),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision())
    {
        if (i.is_error()) { m_is_undefined = true; m_is_negative = false; return; }
        m_significand = i.magnitude();
        if (m_significand.is_zero()) { m_is_negative = false; return; }
        normalize();
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigFloat(T value) noexcept
        : m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false),
          m_is_negative(value < 0),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision())
    {
        using U = typename std::make_unsigned<T>::type;
        U mag = m_is_negative ? U(-value) : U(value);
        m_significand = BigUint(mag);
        if (m_significand.is_zero()) { m_is_negative = false; return; }
        normalize();
    }

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    BigFloat(const T& value) { BigInt i(value); *this = BigFloat(i); }

    template <typename F, typename = typename std::enable_if<fizmo::is_fizmo_fixed_float_v<F>>::type, typename = void>
    BigFloat(const F& f)
        : m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false), m_is_negative(false),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision())
    {
        if (f.is_nan())       { m_is_nan  = true; return; }
        if (f.is_undefined()) { m_is_undefined = true; return; }
        if (f.is_infinite())  { m_is_inf  = true; m_is_negative = f.is_negative(); return; }
        if (f.is_zero())      { m_is_negative = f.is_negative(); return; }
        m_is_negative = f.is_negative();
        BigUint mant = f.get_mantissa();
        if (!f.is_subnormal()) mant.set_bit(F::mantissa_bits);
        m_significand = mant;
        m_exponent    = static_cast<std::int64_t>(f.get_unbiased_exponent().as_signed());
        normalize();
    }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void>
    BigFloat(T value) noexcept : m_exponent(0), m_is_nan(false), m_is_undefined(false), m_is_inf(false), m_is_negative(false), m_precision_bits(context_or_default_precision()), m_repr_precision_bits(context_or_default_repr_precision()) {
        if (std::isnan(value))     { m_is_nan = true; return; }
        if (!std::isfinite(value)) { m_is_inf = true; m_is_negative = std::signbit(value); return; }
        if (value == T(0))         return;          
        m_is_negative = std::signbit(value);
        const double dval = m_is_negative ? -static_cast<double>(value) :  static_cast<double>(value);
        std::uint64_t bits;
        static_assert(sizeof(double) == sizeof(std::uint64_t), "unexpected double size");
        std::memcpy(&bits, &dval, sizeof(bits));
        constexpr int      mantissa_width = 52;
        constexpr int      exponent_bias  = 1023;
        constexpr std::uint64_t mant_mask = (1ULL << mantissa_width) - 1;
        const std::uint64_t raw_mant = bits & mant_mask;
        const int biased_exp = static_cast<int>((bits >> mantissa_width) & 0x7FF);

        if (biased_exp == 0) {
            m_significand = BigUint(raw_mant);
            m_exponent    = 1 - exponent_bias - mantissa_width;  
        } else {
            m_significand = BigUint(raw_mant | (1ULL << mantissa_width));
            m_exponent    = biased_exp - exponent_bias - mantissa_width;
        }

        normalize();
    }

    BigFloat(const std::string& s)
        : m_exponent(0),
          m_is_nan(false), m_is_undefined(false), m_is_inf(false), m_is_negative(false),
          m_precision_bits(context_or_default_precision()),
          m_repr_precision_bits(context_or_default_repr_precision())
    {
        if (s.empty()) return;
        std::size_t i = 0;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '-')      { m_is_negative = true; ++i; }
        else if (i < s.size() && s[i] == '+') { ++i; }

        auto ci_eq = [&](const char* lit) -> bool {
            std::size_t j = i, k = 0;
            while (lit[k] && j < s.size()) if (std::tolower(static_cast<unsigned char>(s[j++])) != lit[k++]) return false;
            return lit[k] == '\0' && (j == s.size() || s[j] == '\0');
        };

        auto is_inf_sym = [&]() -> bool {
            return i + 2 < s.size()
                && static_cast<unsigned char>(s[i])     == 0xE2
                && static_cast<unsigned char>(s[i + 1]) == 0x88
                && static_cast<unsigned char>(s[i + 2]) == 0x9E;
        };

        if (ci_eq("nan"))       { m_is_nan = true; m_is_negative = false; return; }
        if (ci_eq("undefined")) { m_is_undefined = true; m_is_negative = false; return; }
        if (ci_eq("infinity") || ci_eq("inf") || is_inf_sym()) { m_is_inf = true; return; }
        std::string int_digits, frac_digits;
        bool seen_dot = false;
        while (i < s.size()) {
            char c = s[i];
            if (c == '.' && !seen_dot) { seen_dot = true; ++i; continue; }
            if (!std::isdigit(static_cast<unsigned char>(c))) break;
            if (seen_dot) frac_digits += c; else int_digits += c;
            ++i;
        }

        long long exp10 = 0;
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            bool neg_exp = false;
            if (i < s.size() && s[i] == '-')      { neg_exp = true; ++i; }
            else if (i < s.size() && s[i] == '+') { ++i; }
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
                exp10 = exp10 * 10 + (s[i++] - '0');
            if (neg_exp) exp10 = -exp10;
        }

        std::string all_digits = int_digits + frac_digits;
        std::size_t nz = all_digits.find_first_not_of('0');
        if (nz == std::string::npos || all_digits.empty()) { m_is_negative = false; return; }
        all_digits = all_digits.substr(nz);
        BigUint val;
        for (char c : all_digits) val = val * BigUint(10) + BigUint(static_cast<unsigned>(c - '0'));
        long long eff_exp10 = exp10 - static_cast<long long>(frac_digits.size());

        if (eff_exp10 >= 0) {
            auto e = static_cast<std::size_t>(eff_exp10);
            m_significand = val * pow_biguint(BigUint(5), e);
            m_exponent    = static_cast<std::int64_t>(e);
            normalize();
        } else {
            auto e     = static_cast<std::size_t>(-eff_exp10);
            BigUint d5 = pow_biguint(BigUint(5), e);
            std::size_t prec = context_or_default_precision();
            divide_significands(val, 0, d5, 0, prec);
            m_exponent -= static_cast<std::int64_t>(e); 
            normalize();
        }
    }

    BigFloat& operator=(const BigUint& u) { *this = BigFloat(u); return *this; }
    BigFloat& operator=(const BigInt& i)  { *this = BigFloat(i); return *this; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value || std::is_floating_point<T>::value || fizmo::is_fizmo_static_int_v<T> || fizmo::is_fizmo_fixed_float_v<T>>::type>
    BigFloat& operator=(T value) { *this = BigFloat(value); return *this; }

public:
    void        set_precision_bits(std::size_t bits) noexcept { m_precision_bits = bits; }
    void        set_precision_decimals(std::size_t n) noexcept { m_precision_bits = decimal_digits_to_bits(n); }
    std::size_t precision_bits() const noexcept { return m_precision_bits ? m_precision_bits : default_precision_bits(); }
    std::size_t decimal_precision() const noexcept { return bits_to_decimal_digits(precision_bits()); }
    void        set_representative_precision_bits(std::size_t bits) noexcept { m_repr_precision_bits = bits; }
    void        set_representative_precision_decimals(std::size_t n) noexcept { m_repr_precision_bits = decimal_digits_to_bits(n); }
    std::size_t representative_precision_bits() const noexcept { return m_repr_precision_bits; }
    std::size_t representative_decimal_precision() const noexcept { return m_repr_precision_bits ? bits_to_decimal_digits(m_repr_precision_bits) : 0; }

    std::size_t display_precision_bits() const noexcept {
        if (BigFloatContext::is_active()) {
            if (BigFloatContext::represented_precision() != 0) return BigFloatContext::represented_precision();
            if (BigFloatContext::precision()             != 0) return BigFloatContext::precision();
        }
        if (m_repr_precision_bits != 0) return m_repr_precision_bits;
        if (m_precision_bits      != 0) return m_precision_bits;
        return default_precision_bits();
    }

    std::size_t display_decimal_precision() const noexcept { return bits_to_decimal_digits(display_precision_bits()); }

public:
    static BigFloat with_precision(std::size_t bits, const BigInt& i)  { BigFloat x(i); x.m_precision_bits = bits; return x; }
    static BigFloat with_precision(std::size_t bits, const BigUint& u) { BigFloat x(u); x.m_precision_bits = bits; return x; }

    static BigFloat from_parts(bool neg, const BigUint& sig, std::int64_t exp, std::size_t prec) {
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = sig;
        r.m_exponent    = exp;
        r.m_precision_bits = prec;
        if (!sig.is_zero()) r.normalize();
        else { r.m_is_negative = false; r.m_exponent = 0; }
        return r;
    }

    static BigFloat from_parts_raw(bool neg, BigUint&& sig, std::int64_t exp, std::size_t prec) {
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = std::move(sig);
        r.m_exponent    = exp;
        r.m_precision_bits = prec;
        return r;
    }

    static BigFloat zero(bool neg = false) noexcept { BigFloat x; x.m_is_negative = neg; return x; }
    static BigFloat positive_infinity()    noexcept { BigFloat x; x.m_is_inf = true; return x; }
    static BigFloat negative_infinity()    noexcept { BigFloat x; x.m_is_inf = true; x.m_is_negative = true; return x; }
    static BigFloat nan()                  noexcept { BigFloat x; x.m_is_nan = true; return x; }
    static BigFloat undefined()            noexcept { BigFloat x; x.m_is_undefined = true; return x; }

public:
    bool is_zero()              const noexcept { return !m_is_nan && !m_is_inf && !m_is_undefined && m_significand.is_zero(); }
    bool is_negative()          const noexcept { return m_is_negative; }
    bool is_positive()          const noexcept { return !m_is_negative; }
    bool is_infinite()          const noexcept { return m_is_inf; }
    bool is_positive_infinity() const noexcept { return m_is_inf && !m_is_negative; }
    bool is_negative_infinity() const noexcept { return m_is_inf &&  m_is_negative; }
    bool is_nan()               const noexcept { return m_is_nan; }
    bool is_undefined()         const noexcept { return m_is_undefined; }
    bool is_finite()            const noexcept { return !m_is_nan && !m_is_inf && !m_is_undefined; }
    bool is_subnormal()         const noexcept { return is_finite() && !is_zero() && m_significand.get_exponent_base2() == 0; }
    bool is_normal()            const noexcept { return is_finite() && !is_zero() && !is_subnormal(); }
    bool is_integer()           const noexcept { return get_fractional_part_uint().is_zero(); }
    const BigUint&  raw_significand() const noexcept { return m_significand; }
    std::int64_t    raw_exponent()    const noexcept { return m_exponent; }

public:
    std::int64_t exponent_base2() const noexcept {
        if (!is_finite() || is_zero()) return 0;
        return m_exponent + static_cast<std::int64_t>(m_significand.get_exponent_base2());
    }

    std::int64_t exponent_base10() const {
        if (!is_finite() || is_zero()) return 0;
        return m_exponent + static_cast<std::int64_t>(m_significand.get_exponent_base10());
    }

public:
    void normalize() noexcept {
        if (!is_finite()) return;
        if (m_significand.is_zero()) { m_exponent = 0; m_is_negative = false; return; }
        std::size_t tz = m_significand.count_trailing_zeros();
        if (tz > 0) {
            m_significand >>= static_cast<std::uint64_t>(tz);
            m_exponent    += static_cast<std::int64_t>(tz);
        }
    }

    BigFloat as_normal() const noexcept { BigFloat f = *this; f.normalize(); return f; }

public:
    BigUint get_integer_part_uint() const {
        if (!is_finite() || is_zero()) return BigUint::zero();
        if (m_exponent >= 0) return m_exponent == 0 ? m_significand : (m_significand << static_cast<std::uint64_t>(m_exponent));
        return m_significand >> static_cast<std::uint64_t>(-m_exponent);
    }

    BigUint get_fractional_part_uint() const {
        if (!is_finite() || is_zero() || m_exponent >= 0) return BigUint::zero();
        std::uint64_t ae = static_cast<std::uint64_t>(-m_exponent);
        BigUint ip = m_significand >> ae;
        return m_significand - (ip << ae);
    }

    BigFloat get_integer_part() const {
        if (!is_finite() || is_zero() || m_exponent >= 0) return *this;
        BigUint ip = get_integer_part_uint();
        if (ip.is_zero()) { BigFloat r; r.m_is_negative = m_is_negative; r.m_precision_bits = precision_bits(); return r; }
        return from_parts(m_is_negative, ip, 0, precision_bits());
    }

    BigFloat get_fractional_part() const {
        if (!is_finite() || is_zero() || m_exponent >= 0) return BigFloat::zero();
        BigUint fp = get_fractional_part_uint();
        if (fp.is_zero()) return BigFloat::zero();
        BigFloat r;
        r.m_is_negative = m_is_negative;
        r.m_significand = fp;
        r.m_exponent    = m_exponent;
        r.m_precision_bits = precision_bits();
        r.normalize();
        return r;
    }

public:
    bool operator==(const BigFloat& o) const noexcept {
        if (m_is_nan || m_is_undefined || o.m_is_nan || o.m_is_undefined) return false;
        if (m_is_inf || o.m_is_inf) return m_is_inf && o.m_is_inf && m_is_negative == o.m_is_negative;
        return m_is_negative == o.m_is_negative && m_significand == o.m_significand && m_exponent == o.m_exponent;
    }

    bool operator!=(const BigFloat& o) const noexcept { return !(*this == o); }

    bool operator<(const BigFloat& o) const noexcept {
        if (m_is_nan || m_is_undefined || o.m_is_nan || o.m_is_undefined) return false;
        if (m_is_inf || o.m_is_inf) {
            if (m_is_inf && o.m_is_inf) return m_is_negative && !o.m_is_negative;
            if (m_is_inf) return m_is_negative;
            return !o.m_is_negative;
        }
        if (is_zero() && o.is_zero()) return false;
        if (is_zero()) return !o.m_is_negative;
        if (o.is_zero()) return m_is_negative;
        if (m_is_negative != o.m_is_negative) return m_is_negative;
        bool neg = m_is_negative;
        std::int64_t msb_a = m_exponent + static_cast<std::int64_t>(m_significand.get_exponent_base2());
        std::int64_t msb_b = o.m_exponent + static_cast<std::int64_t>(o.m_significand.get_exponent_base2());
        if (msb_a != msb_b) return neg ? (msb_a > msb_b) : (msb_a < msb_b);
        BigUint sa = m_significand, sb = o.m_significand;
        if      (m_exponent > o.m_exponent) sa <<= static_cast<std::uint64_t>(m_exponent - o.m_exponent);
        else if (o.m_exponent > m_exponent) sb <<= static_cast<std::uint64_t>(o.m_exponent - m_exponent);
        if (sa != sb) return neg ? (sa > sb) : (sa < sb);
        return false;
    }

    bool operator>(const BigFloat& o)  const noexcept { return o < *this; }
    bool operator<=(const BigFloat& o) const noexcept { return !(*this > o); }
    bool operator>=(const BigFloat& o) const noexcept { return !(*this < o); }

public:
    BigFloat& negate()        noexcept { if (!m_is_nan && !m_is_undefined) m_is_negative = !m_is_negative; return *this; }
    BigFloat& abs_in_place()  noexcept { m_is_negative = false; return *this; }
    BigFloat operator-() const noexcept { BigFloat r = *this; r.negate(); return r; }
    BigFloat operator+() const noexcept { return *this; }
    BigFloat abs() const noexcept { BigFloat temp(*this); temp.abs_in_place(); return temp; }

public:
    BigFloat& operator+=(const BigFloat& o) {
        if (m_is_nan || o.m_is_nan)             { *this = nan();       return *this; }
        if (m_is_undefined || o.m_is_undefined) { *this = undefined(); return *this; }

        if (m_is_inf || o.m_is_inf) {
            if (m_is_inf && o.m_is_inf && m_is_negative != o.m_is_negative) *this = undefined();
            else if (o.m_is_inf) *this = o;
            m_precision_bits = effective_precision_bits(*this, o);
            return *this;
        }

        std::size_t prec = effective_precision_bits(*this, o);
        if (is_zero())   { *this = o; m_precision_bits = prec; return *this; }
        if (o.is_zero()) { m_precision_bits = prec; return *this; }

        if (m_is_negative != o.m_is_negative) {
            BigFloat tmp = o; tmp.m_is_negative = !tmp.m_is_negative;
            *this -= tmp; m_precision_bits = prec; return *this;
        }

        if (m_exponent > o.m_exponent) {
            m_significand <<= static_cast<std::uint64_t>(m_exponent - o.m_exponent);
            m_exponent = o.m_exponent;
        } else if (o.m_exponent > m_exponent) {
            m_significand += o.m_significand << static_cast<std::uint64_t>(o.m_exponent - m_exponent);
            m_precision_bits = prec; normalize(); return *this;
        }

        m_significand += o.m_significand;
        m_precision_bits = prec; normalize(); return *this;
    }

    BigFloat operator+(const BigFloat& o) const { BigFloat r = *this; r += o; return r; }

    BigFloat& operator-=(const BigFloat& o) {
        if (m_is_nan || o.m_is_nan)             { *this = nan();       return *this; }
        if (m_is_undefined || o.m_is_undefined) { *this = undefined(); return *this; }

        if (m_is_inf || o.m_is_inf) {
            if (m_is_inf && o.m_is_inf) {
                if (m_is_negative == o.m_is_negative) *this = undefined();
            } else if (o.m_is_inf) {
                *this = o.m_is_negative ? positive_infinity() : negative_infinity();
            }
            m_precision_bits = effective_precision_bits(*this, o);
            return *this;
        }

        std::size_t prec = effective_precision_bits(*this, o);
        if (o.is_zero()) { m_precision_bits = prec; return *this; }
        if (is_zero())   { *this = o; negate(); m_precision_bits = prec; return *this; }

        if (m_is_negative != o.m_is_negative) {
            BigFloat tmp = o; tmp.m_is_negative = !tmp.m_is_negative;
            *this += tmp; m_precision_bits = prec; return *this;
        }

        BigUint sa = m_significand, sb = o.m_significand;
        std::int64_t ea = m_exponent;
        if (m_exponent > o.m_exponent) {
            sa <<= static_cast<std::uint64_t>(m_exponent - o.m_exponent); ea = o.m_exponent;
        } else if (o.m_exponent > m_exponent) {
            sb = o.m_significand << static_cast<std::uint64_t>(o.m_exponent - m_exponent);
        }

        if (sa >= sb) { m_significand = sa - sb; }
        else          { m_significand = sb - sa; m_is_negative = !m_is_negative; }
        m_exponent = ea; m_precision_bits = prec; normalize(); return *this;
    }

    BigFloat operator-(const BigFloat& o) const { BigFloat r = *this; r -= o; return r; }

public:
    BigFloat operator*(const BigFloat& o) const { return textbook_multiply(o); }
    BigFloat& operator*=(const BigFloat& o) { *this = *this * o; return *this; }

    BigFloat textbook_multiply(const BigFloat& o) const {
        if (m_is_nan || o.m_is_nan)             return nan();
        if (m_is_undefined || o.m_is_undefined) return undefined();
        const bool neg  = m_is_negative != o.m_is_negative;
        std::size_t prec = effective_precision_bits(*this, o);
        if (m_is_inf || o.m_is_inf) {
            if (is_zero() || o.is_zero()) return undefined();
            BigFloat r = neg ? negative_infinity() : positive_infinity(); r.m_precision_bits = prec; return r;
        }
        if (is_zero() || o.is_zero()) { BigFloat r; r.m_is_negative = neg; r.m_precision_bits = prec; return r; }
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = m_significand * o.m_significand;
        r.m_exponent    = m_exponent + o.m_exponent;
        r.m_precision_bits = prec;
        r.normalize();
        return r;
    }

    BigFloat karatsuba_multiply(const BigFloat& o) const {
        if (m_is_nan || o.m_is_nan)             return nan();
        if (m_is_undefined || o.m_is_undefined) return undefined();
        const bool neg  = m_is_negative != o.m_is_negative;
        std::size_t prec = effective_precision_bits(*this, o);
        if (m_is_inf || o.m_is_inf) {
            if (is_zero() || o.is_zero()) return undefined();
            BigFloat r = neg ? negative_infinity() : positive_infinity(); r.m_precision_bits = prec; return r;
        }
        if (is_zero() || o.is_zero()) { BigFloat r; r.m_is_negative = neg; r.m_precision_bits = prec; return r; }
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = m_significand.mul_karatsuba(o.m_significand);
        r.m_exponent    = m_exponent + o.m_exponent;
        r.m_precision_bits = prec;
        r.normalize();
        return r;
    }

    BigFloat toom3_multiply(const BigFloat& o) const {
        if (m_is_nan || o.m_is_nan)             return nan();
        if (m_is_undefined || o.m_is_undefined) return undefined();
        const bool neg  = m_is_negative != o.m_is_negative;
        std::size_t prec = effective_precision_bits(*this, o);
        if (m_is_inf || o.m_is_inf) {
            if (is_zero() || o.is_zero()) return undefined();
            BigFloat r = neg ? negative_infinity() : positive_infinity(); r.m_precision_bits = prec; return r;
        }
        if (is_zero() || o.is_zero()) { BigFloat r; r.m_is_negative = neg; r.m_precision_bits = prec; return r; }
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = m_significand.mul_toom3(o.m_significand);
        r.m_exponent    = m_exponent + o.m_exponent;
        r.m_precision_bits = prec;
        r.normalize();
        return r;
    }

    BigFloat fft_multiply(const BigFloat& o) const {
        if (m_is_nan || o.m_is_nan)             return nan();
        if (m_is_undefined || o.m_is_undefined) return undefined();
        const bool neg  = m_is_negative != o.m_is_negative;
        std::size_t prec = effective_precision_bits(*this, o);
        if (m_is_inf || o.m_is_inf) {
            if (is_zero() || o.is_zero()) return undefined();
            BigFloat r = neg ? negative_infinity() : positive_infinity(); r.m_precision_bits = prec; return r;
        }
        if (is_zero() || o.is_zero()) { BigFloat r; r.m_is_negative = neg; r.m_precision_bits = prec; return r; }
        BigFloat r;
        r.m_is_negative = neg;
        r.m_significand = m_significand.mul_fft(o.m_significand);
        r.m_exponent    = m_exponent + o.m_exponent;
        r.m_precision_bits = prec;
        r.normalize();
        return r;
    }

    static BigFloat textbook_multiply (const BigFloat& a, const BigFloat& b) { return a.textbook_multiply(b);  }
    static BigFloat karatsuba_multiply(const BigFloat& a, const BigFloat& b) { return a.karatsuba_multiply(b); }
    static BigFloat toom3_multiply    (const BigFloat& a, const BigFloat& b) { return a.toom3_multiply(b);     }
    static BigFloat fft_multiply      (const BigFloat& a, const BigFloat& b) { return a.fft_multiply(b);       }

public:
    BigFloat& operator/=(const BigFloat& o) {
        if (m_is_nan || o.m_is_nan)             { *this = nan();       return *this; }
        if (m_is_undefined || o.m_is_undefined) { *this = undefined(); return *this; }
        const bool neg  = m_is_negative != o.m_is_negative;
        std::size_t prec = effective_precision_bits(*this, o);

        if (o.is_zero()) {
            *this = is_zero() ? undefined() : (neg ? negative_infinity() : positive_infinity());
            m_precision_bits = prec; return *this;
        }
        if (is_zero()) { m_is_negative = neg; m_precision_bits = prec; return *this; }

        if (m_is_inf || o.m_is_inf) {
            if (m_is_inf && o.m_is_inf) { *this = undefined(); return *this; }
            if (m_is_inf) { *this = neg ? negative_infinity() : positive_infinity(); }
            else          { m_significand = BigUint(); m_exponent = 0; m_is_inf = false; }
            m_is_negative = neg; m_precision_bits = prec; return *this;
        }
        
        m_is_negative = neg;
        divide_significands(m_significand, m_exponent, o.m_significand, o.m_exponent, prec);
        return *this;
    }

    BigFloat operator/(const BigFloat& o) const { BigFloat r = *this; r /= o; return r; }

    BigFloat& fast_divide_inplace(BigFloat o) {
        o.reciprocal_inplace();
        *this *= o;
        return *this;
    }

    BigFloat fast_divide(BigFloat o) const {
        o.reciprocal_inplace();
        return *this * o;
    }

    BigFloat& reciprocal_inplace() {
        if (m_is_nan || m_is_undefined) return *this;
        if (is_zero()) { *this = m_is_negative ? negative_infinity() : positive_infinity(); return *this; }
        if (m_is_inf) { *this = BigFloat::zero(m_is_negative); return *this; }
        std::size_t prec = precision_bits();
        std::size_t k = prec + 32;
        BigUint num = BigUint(1) << k;
        m_significand = num / m_significand;
        m_exponent = -m_exponent - static_cast<std::int64_t>(k);
        normalize();
        return *this;
    }

    BigFloat reciprocal() const {
        BigFloat temp = *this;
        temp.reciprocal_inplace();
        return temp;
    }

public:
    friend BigFloat operator+(BigFloat&& a, const BigFloat& b) { a += b; return a; }
    friend BigFloat operator+(const BigFloat& a, BigFloat&& b) { b += a; return b; }
    friend BigFloat operator+(BigFloat&& a, BigFloat&& b)      { a += b; return a; }

    friend BigFloat operator-(BigFloat&& a, const BigFloat& b) { a -= b; return a; }
    friend BigFloat operator-(const BigFloat& a, BigFloat&& b) { b -= a; b.negate(); return b; }
    friend BigFloat operator-(BigFloat&& a, BigFloat&& b)      { a -= b; return a; }

    friend BigFloat operator/(BigFloat&& a, const BigFloat& b) { a /= b; return a; }
    friend BigFloat operator/(BigFloat&& a, BigFloat&& b)      { a /= b; return a; }

public:
    std::string to_string(std::size_t decimals, bool include_sep = true, char sep = ',', std::size_t group = 3) const {
        if (m_is_nan)       return "NaN";
        if (m_is_undefined) return "undefined";
        if (m_is_inf)       return m_is_negative ? "-\u221e" : "\u221e";
        
        if (is_zero()) {
            std::string s = "0";
            if (decimals > 0) { s += '.'; s.append(decimals, '0'); }
            return s;
        }

        bool exp_neg  = (m_exponent < 0);
        std::uint64_t abs_exp = exp_neg ? static_cast<std::uint64_t>(-m_exponent) : static_cast<std::uint64_t>(m_exponent);
        BigUint integer_part, frac_num;
        std::uint64_t frac_shift = 0;

        if (!exp_neg) {
            integer_part = abs_exp == 0 ? m_significand : (m_significand << abs_exp);
        } else {
            integer_part = m_significand >> abs_exp;
            frac_num     = m_significand - (integer_part << abs_exp);
            frac_shift   = abs_exp;
        }

        std::string result;
        if (m_is_negative) result += '-';
        result += integer_part.to_string(include_sep, sep, group);

        if (decimals > 0) {
            result += '.';

            if (frac_num.is_zero()) {
                result.append(decimals, '0');
            } else {
                const BigUint& pow10 = cached_pow10(decimals);
                BigUint frac_val     = (frac_num * pow10) >> frac_shift;
                std::string fs       = frac_val.to_string(false, '\0');
                if (fs.size() < decimals) fs = std::string(decimals - fs.size(), '0') + fs;
                else if (fs.size() > decimals) fs = fs.substr(0, decimals);
                result += fs;
            }
        }

        if (decimals > 0) {
            std::size_t dot = result.find('.');
            if (dot != std::string::npos) {
                std::size_t end = result.size();
                while (end > dot + 1 && result[end - 1] == '0') --end;
                if (end == dot + 1) {
                    result.erase(dot + 2); 
                } else {
                    result.erase(end);
                }
            }
        }

        return result;
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        if (m_is_nan)       return "NaN";
        if (m_is_undefined) return "undefined";
        if (m_is_inf)       return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero())      return "0";
        if (sig_figs == 0)  sig_figs = 1;
        bool exp_neg  = (m_exponent < 0);
        std::uint64_t abs_exp = exp_neg ? static_cast<std::uint64_t>(-m_exponent) : static_cast<std::uint64_t>(m_exponent);
        double approx_log10 = static_cast<double>(m_significand.to_string(false,'\0').size() - 1) + (exp_neg ? -1.0 : 1.0) * static_cast<double>(abs_exp) * 0.301029995663981195214;
        std::size_t N = (approx_log10 < static_cast<double>(sig_figs)) ? static_cast<std::size_t>(static_cast<double>(sig_figs) - approx_log10) + 10 : 10;
        BigUint pow5   = pow_biguint(BigUint(5), N);
        BigUint scaled = m_significand * pow5;
        BigUint res_int;

        if (exp_neg && abs_exp > N) {
            res_int = scaled >> (abs_exp - N);
        } else {
            std::uint64_t ls = exp_neg ? (N - abs_exp) : (N + abs_exp);
            res_int = scaled << ls;
        }

        if (res_int.is_zero()) return "0";
        std::string digits = res_int.to_string(false, '\0');
        long long dec_exp  = static_cast<long long>(digits.size()) - (1 + static_cast<long long>(N));

        if (digits.size() > sig_figs) {
            bool ru = digits[sig_figs] >= '5';
            digits  = digits.substr(0, sig_figs);

            if (ru) {
                int carry = 1;
                
                for (std::size_t k = digits.size(); k > 0 && carry; --k) {
                    int d = (digits[k-1] - '0') + carry;
                    digits[k-1] = static_cast<char>('0' + d % 10);
                    carry = d / 10;
                }

                if (carry) { digits.insert(digits.begin(), '1'); digits.pop_back(); ++dec_exp; }
            }
        } else if (digits.size() < sig_figs) {
            digits.append(sig_figs - digits.size(), '0');
        }

        std::string result;
        if (m_is_negative) result += '-';
        result += digits[0];
        if (sig_figs > 1) { result += '.'; result += digits.substr(1); }
        result += 'e';
        result += std::to_string(dec_exp);
        std::size_t dot = result.find('.');

        if (dot != std::string::npos) {
            std::size_t epos = result.find('e', dot);
            
            if (epos != std::string::npos) {
                std::size_t end = epos;
                while (end > dot + 1 && result[end - 1] == '0') --end;
                
                if (end == dot + 1) {
                    result.erase(dot + 2, epos - (dot + 2));
                } else {
                    result.erase(end, epos - end);
                }
            }
        }
        
        return result;
    }

    std::string to_string()           const { return to_string(bits_to_decimal_digits(display_precision_bits()), true, ',', 3); }
    std::string to_scientific_string() const { return to_scientific_string(bits_to_decimal_digits(display_precision_bits())); }
    friend std::ostream& operator<<(std::ostream& os, const BigFloat& x) { os << x.to_scientific_string(); return os; }
};

inline void BigFloatContext::set_decimal_precision(std::size_t decimals) {
    auto& ctx = current();
    ctx.precision_bits = BigFloat::decimal_digits_to_bits(decimals);
    ctx.active = true;
}

inline void BigFloatContext::set_decimal_representative_precision(std::size_t decimals) {
    set_representative_precision(BigFloat::decimal_digits_to_bits(decimals));
}

struct PrecisionScope {
    explicit PrecisionScope(std::size_t decimal_digits) { BigFloatContext::set_decimal_precision(decimal_digits); }
    ~PrecisionScope() { BigFloatContext::clear(); }
};

} // namespace multiprecision

template <> struct is_fizmo_float<multiprecision::BigFloat> : std::true_type{};

template <>
struct fizmo_float_rank<
    multiprecision::BigFloat,
    void
> : std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max() - 1> {};

} // namespace fizmo

#endif // MULTIPRECISION_BIG_FLOAT_HPP