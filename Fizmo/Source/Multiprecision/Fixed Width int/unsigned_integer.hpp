#ifndef FIZMO_MULTIPRECISION_UNSIGNED_INTEGER_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_UNSIGNED_INTEGER_CLASS_SPECIALIZATION_HPP

#include "integer.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t Bits>
class integer<Bits, sign::is_unsigned> {
    umag<Bits> m;

    template <std::size_t, sign> friend class integer;  

    static constexpr integer from_mag(umag<Bits> v) noexcept { integer r; r.m = v; return r; }

public:
    static constexpr std::size_t bits = Bits;
    static constexpr bool        is_signed_type = false;
    using mag_t = umag<Bits>;
    using half_type = typename umag<Bits>::half_type;
    using bit_index = long long;

    static constexpr bit_index bit_count = static_cast<bit_index>(Bits);

    constexpr integer() noexcept : m() {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
    constexpr integer(T v) noexcept : m(static_cast<std::uint64_t>(v)) {}

    template <std::size_t B = Bits, typename = typename std::enable_if<(B >= 128)>::type>
    constexpr integer(half_type low, half_type high) noexcept : m(low, high) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type, typename = void>
    OPTIONAL_CPP14_CONSTEXPR integer(T v) noexcept : m() {
        const std::uint64_t u = static_cast<std::uint64_t>(v);
        m = (v < 0) ? ((umag<Bits>::max() << 64) | umag<Bits>(u)) : umag<Bits>(u);
    }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void, typename = void>
    integer(T value) noexcept : m() {
        if (!(value == value) || value <= T(0)) return;             
        long double v = std::floor(static_cast<long double>(value));
        if (v >= std::ldexp(1.0L, static_cast<int>(Bits))) { m = umag<Bits>::max(); return; }
        const long double two64 = 18446744073709551616.0L;

        for (bit_index shift = 0; v >= 1.0L && shift < bit_count; shift += 64) {
            const std::uint64_t chunk = static_cast<std::uint64_t>(std::fmod(v, two64));
            m = m | (umag<Bits>(chunk) << static_cast<std::size_t>(shift));
            v = std::floor(v / two64);
        }
    }

    template <std::size_t F, sign SF, typename = typename std::enable_if<is_lossless<F, SF, Bits, sign::is_unsigned>::value>::type>
    constexpr integer(const integer<F, SF>& src) noexcept : m(resize_mag<Bits>(src.magnitude())) {}

    template <std::size_t F, sign SF, typename = typename std::enable_if<!is_lossless<F, SF, Bits, sign::is_unsigned>::value>::type, typename = void>
    explicit OPTIONAL_CPP14_CONSTEXPR integer(const integer<F, SF>& src) noexcept : m() {
        const umag<Bits> mm = resize_mag<Bits>(src.magnitude());
        m = src.is_negative() ? ((~mm) + umag<Bits>(std::uint64_t(1))) : mm;
    }

    explicit integer(const std::string& str, long long base = 10) noexcept : m() { parse_string(str, base); }

    explicit constexpr integer(const umag<Bits>& mag) noexcept : m(mag) {}

    constexpr const umag<Bits>& magnitude() const noexcept { return m; }
    constexpr bool is_negative()  const noexcept { return false; }
    constexpr bool is_undefined() const noexcept { return false; }
    constexpr bool is_zero()      const noexcept { return m.is_zero(); }

    static constexpr integer max() noexcept { return from_mag(umag<Bits>::max()); }
    static constexpr integer min() noexcept { return integer(); }

    template <std::size_t T>
    OPTIONAL_CPP14_CONSTEXPR integer<T, sign::is_unsigned> truncated() const noexcept {
        return integer<T, sign::is_unsigned>::from_mag(resize_mag<T>(m));
    }

    constexpr bool get_bit(std::size_t i) const noexcept { return m.get_bit(i); }
    OPTIONAL_CPP14_CONSTEXPR void set_bit(std::size_t i, bool b = true) noexcept { m.set_bit(i, b); }
    OPTIONAL_CPP14_CONSTEXPR bool toggle_bit(std::size_t i) noexcept { return m.toggle_bit(i); }
    constexpr std::uint64_t get_lowest_bits() const noexcept { return m.get_lowest_bits(); }
    constexpr half_type     get_low_bits()    const noexcept { return m.get_low_bits();  }
    constexpr half_type     get_high_bits()   const noexcept { return m.get_high_bits(); }
    OPTIONAL_CPP14_CONSTEXPR bit_index highest_bit() const noexcept { return m.highest_bit(); }
    OPTIONAL_CPP14_CONSTEXPR bool any_bit_below(std::size_t n) const noexcept { return m.any_bit_below(n); }

    static OPTIONAL_CPP14_CONSTEXPR long long max_digits(long long base = 10) noexcept {
        if (base < 2 || base > 36) return 0;
        return static_cast<long long>(umag<Bits>::max_digits(static_cast<unsigned>(base)));
    }

    static constexpr long long max_digits_base2()  noexcept { return static_cast<long long>(umag<Bits>::max_digits_base2()); }
    static constexpr long long max_digits_base10() noexcept { return static_cast<long long>(umag<Bits>::max_digits_base10()); }
    static constexpr long long max_digits_base16() noexcept { return static_cast<long long>(umag<Bits>::max_digits_base16()); }

    OPTIONAL_CPP14_CONSTEXPR integer operator+(const integer& o) const noexcept { return from_mag(m + o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer operator-(const integer& o) const noexcept { return from_mag(m - o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer operator*(const integer& o) const noexcept { return from_mag(m * o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer operator/(const integer& o) const noexcept { return from_mag(m / o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer operator%(const integer& o) const noexcept { return from_mag(m % o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer operator-() const noexcept { return from_mag((~m) + umag<Bits>(std::uint64_t(1))); }
    constexpr integer operator~() const noexcept { return from_mag(~m); }
    constexpr integer operator&(const integer& o) const noexcept { return from_mag(m & o.m); }
    constexpr integer operator|(const integer& o) const noexcept { return from_mag(m | o.m); }
    constexpr integer operator^(const integer& o) const noexcept { return from_mag(m ^ o.m); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator<<(T s) const noexcept {
        const bit_index n = static_cast<bit_index>(s);
        if (n == 0) return *this;
        if (n < 0 || n >= bit_count) return integer();
        return from_mag(m << static_cast<std::size_t>(n));
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator>>(T s) const noexcept {
        const bit_index n = static_cast<bit_index>(s);
        if (n == 0) return *this;
        if (n < 0 || n >= bit_count) return integer();
        return from_mag(m >> static_cast<std::size_t>(n));
    }

    OPTIONAL_CPP14_CONSTEXPR void divmod(const integer& d, integer& q, integer& r) const noexcept { m.divmod(d.m, q.m, r.m); }
    OPTIONAL_CPP14_CONSTEXPR bool is_exact_division(const integer& o) const noexcept { umag<Bits> q, r; m.divmod(o.m, q, r); return r.is_zero(); }

    OPTIONAL_CPP14_CONSTEXPR integer& operator++() noexcept { m = m + umag<Bits>(std::uint64_t(1)); return *this; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator--() noexcept { m = m - umag<Bits>(std::uint64_t(1)); return *this; }
    OPTIONAL_CPP14_CONSTEXPR integer operator++(int) noexcept { integer t(*this); ++*this; return t; }
    OPTIONAL_CPP14_CONSTEXPR integer operator--(int) noexcept { integer t(*this); --*this; return t; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator+=(const integer& o) noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator-=(const integer& o) noexcept { return *this = *this - o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator*=(const integer& o) noexcept { return *this = *this * o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator/=(const integer& o) noexcept { return *this = *this / o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator%=(const integer& o) noexcept { return *this = *this % o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator&=(const integer& o) noexcept { return *this = *this & o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator|=(const integer& o) noexcept { return *this = *this | o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator^=(const integer& o) noexcept { return *this = *this ^ o; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer& operator<<=(T s) noexcept { return *this = *this << s; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer& operator>>=(T s) noexcept { return *this = *this >> s; }

    OPTIONAL_CPP14_CONSTEXPR integer pow(const integer& e) const noexcept {
        umag<Bits> base = m, exp = e.m, result(std::uint64_t(1));

        while (!exp.is_zero()) {
            if (exp.get_bit(0)) result = result * base;
            base = base * base;
            exp  = exp >> 1;
        }

        return from_mag(result);
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer pow(T e) const noexcept { return pow(integer(e)); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type, typename = void>
    OPTIONAL_CPP14_CONSTEXPR integer pow(T e) const noexcept {
        if (e < T(0)) return (m == umag<Bits>(std::uint64_t(1))) ? integer(std::uint64_t(1)) : integer();
        return pow(integer(static_cast<std::uint64_t>(e)));
    }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void, typename = void>
    integer pow(T e) const noexcept {
        if (!(e == e)) return integer();                     
        const long double t = std::trunc(static_cast<long double>(e));
        if (t < 0.0L) return (m == umag<Bits>(std::uint64_t(1))) ? integer(std::uint64_t(1)) : integer();
        return pow(integer(t));
    }

    OPTIONAL_CPP14_CONSTEXPR integer& pow_mutable(const integer& e) noexcept { return *this = pow(e); }

    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    integer& pow_mutable(T e) noexcept { return *this = pow(e); }

    constexpr bool operator==(const integer& o) const noexcept { return m == o.m; }
    constexpr bool operator!=(const integer& o) const noexcept { return m != o.m; }
    constexpr bool operator< (const integer& o) const noexcept { return m <  o.m; }
    constexpr bool operator<=(const integer& o) const noexcept { return m <= o.m; }
    constexpr bool operator> (const integer& o) const noexcept { return m >  o.m; }
    constexpr bool operator>=(const integer& o) const noexcept { return m >= o.m; }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator+(T v) const noexcept { return *this + integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator-(T v) const noexcept { return *this - integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator*(T v) const noexcept { return *this * integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator/(T v) const noexcept { return *this / integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer operator%(T v) const noexcept { return *this % integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr bool operator==(T v) const noexcept { return *this == integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr bool operator!=(T v) const noexcept { return *this != integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr bool operator< (T v) const noexcept { return *this <  integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr bool operator> (T v) const noexcept { return *this >  integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator+(T v) const noexcept { return to_double() + static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator-(T v) const noexcept { return to_double() - static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator*(T v) const noexcept { return to_double() * static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator/(T v) const noexcept { return to_double() / static_cast<double>(v); }

    double ratio_to(const integer& o) const noexcept {
        if (o.is_zero()) return std::numeric_limits<double>::infinity();
        return to_double() / o.to_double();
    }

    double      to_double() const noexcept { return m.to_double(); }
    long double to_long_double() const noexcept { return static_cast<long double>(to_double()); }

    std::string to_string(long long base = 10) const {
        if (base < 2 || base > 36) return "";
        return m.to_string(static_cast<unsigned>(base));
    }

    std::string to_string_scientific(long long precision = 15) const;

    static integer from_hex(const std::string& s)     noexcept { return integer(s, 16); }
    static integer from_decimal(const std::string& s) noexcept { return integer(s, 10); }
    static integer from_binary(const std::string& s)  noexcept { return integer(s, 2);  }
    static constexpr integer from_magnitude(const umag<Bits>& v) noexcept { return from_mag(v); }
    static bool is_valid_string(const std::string& s, long long base) noexcept { integer t; return t.parse_string(s, base, true); }

    bool parse_string(const std::string& str, long long base, bool validate_only = false) noexcept {
        if (str.empty() || base < 2 || base > 36) return false;
        std::size_t start = 0;
        if (str[0] == '+') start = 1;
        else if (str[0] == '-') return false;           
        if (start >= str.size()) return false;
        if (base == 16 && str.size() > start + 2 && str[start] == '0' && (str[start+1]=='x'||str[start+1]=='X')) start += 2;
        umag<Bits> acc, b(static_cast<std::uint64_t>(base));
        bool found = false;

        for (std::size_t i = start; i < str.size(); ++i) {
            const char c = str[i]; long long d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else if (c == '_' || c == ',' || c == '\'') continue;
            else return false;
            if (d < 0 || d >= base) return false;
            found = true;
            if (!validate_only) acc = acc * b + umag<Bits>(static_cast<std::uint64_t>(d));
        }
        
        if (found && !validate_only) m = acc;
        return found;
    }

    friend std::ostream& operator<<(std::ostream& os, const integer& x) { return os << x.to_string(); }

public:
    OPTIONAL_CPP14_CONSTEXPR integer div_small(std::uint32_t d) const noexcept {
        if (d == 0) return integer();                     
        umag<Bits> q;
        m.divmod_small(d, 0u, q);
        return from_mag(q);
    }

    OPTIONAL_CPP14_CONSTEXPR std::uint32_t mod_small(std::uint32_t d) const noexcept {
        if (d == 0) return 0;
        umag<Bits> q;
        return m.divmod_small(d, 0u, q);
    }

    OPTIONAL_CPP14_CONSTEXPR std::uint32_t divmod_small(std::uint32_t d, integer& q) const noexcept {
        if (d == 0) { q = integer(); return 0; }
        umag<Bits> qm;
        const std::uint32_t r = m.divmod_small(d, 0u, qm);
        q = from_mag(qm);
        return r;
    }
};

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_UNSIGNED_INTEGER_CLASS_SPECIALIZATION_HPP