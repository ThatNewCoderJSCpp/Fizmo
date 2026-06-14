#ifndef FIZMO_MULTIPRECISION_SIGNED_INTEGER_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_SIGNED_INTEGER_CLASS_SPECIALIZATION_HPP

#include "unsigned_integer.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t Bits>
class integer<Bits, sign::is_signed> {
    umag<Bits> m;
    bool       neg = false;

    template <std::size_t, sign> friend class integer;   

    OPTIONAL_CPP14_CONSTEXPR void normalize() noexcept { if (m.is_zero()) neg = false; }
    static OPTIONAL_CPP14_CONSTEXPR integer make(umag<Bits> mag, bool n) noexcept { integer r; r.m = mag; r.neg = n; r.normalize(); return r; }

public:
    static constexpr std::size_t bits = Bits;
    static constexpr bool        is_signed_type = true;
    using mag_t = umag<Bits>;
    using half_type = typename umag<Bits>::half_type;

    constexpr integer() noexcept : m(), neg(false) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
    constexpr integer(T v) noexcept : m(static_cast<std::uint64_t>(v)), neg(false) {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type, typename = void>
    OPTIONAL_CPP14_CONSTEXPR integer(T v) noexcept
        : m((v < 0) ? umag<Bits>(static_cast<std::uint64_t>(-(v + 1)) + std::uint64_t(1)) : umag<Bits>(static_cast<std::uint64_t>(v))),
          neg(v < 0) { normalize(); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void, typename = void>
    integer(T value) noexcept : m(), neg(false) {
        if (!(value == value)) { *this = undefined(); return; }
        neg = value < T(0);
        long double v = std::floor(std::fabs(static_cast<long double>(value)));
        if (v >= std::ldexp(1.0L, static_cast<int>(Bits))) { m = umag<Bits>::max(); normalize(); return; }
        const long double two64 = 18446744073709551616.0L;
        
        for (std::size_t shift = 0; v >= 1.0L && shift < Bits; shift += 64) {
            std::uint64_t chunk = static_cast<std::uint64_t>(std::fmod(v, two64));
            m = m | (umag<Bits>(chunk) << shift);
            v = std::floor(v / two64);
        }
        
        normalize();
    }

    template <std::size_t F, sign SF, typename = typename std::enable_if<is_lossless<F, SF, Bits, sign::is_signed>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR integer(const integer<F, SF>& src) noexcept : m(), neg(false) {
        if (src.is_undefined()) { m = umag<Bits>(); neg = true; return; }
        m   = resize_mag<Bits>(src.magnitude());
        neg = src.is_negative();
        normalize();
    }

    template <std::size_t F, sign SF, typename = typename std::enable_if<!is_lossless<F, SF, Bits, sign::is_signed>::value>::type, typename = void>
    explicit OPTIONAL_CPP14_CONSTEXPR integer(const integer<F, SF>& src) noexcept : m(), neg(false) {
        if (src.is_undefined()) { m = umag<Bits>(); neg = true; return; }
        neg = src.is_negative();
        const umag<F>    mm       = src.magnitude();
        const umag<Bits> narrowed = resize_mag<Bits>(mm);
        m = (resize_mag<F>(narrowed) != mm) ? umag<Bits>::max() : narrowed;  
        normalize();
    }

    template <std::size_t B = Bits, typename = typename std::enable_if<(B >= 128)>::type>
    constexpr integer(half_type low, half_type high) noexcept : m(low, high), neg(false) {}

    explicit integer(const std::string& str, int base = 10) noexcept : m(), neg(false) { if (!parse_string(str, base)) *this = undefined(); }

    static OPTIONAL_CPP14_CONSTEXPR integer undefined() noexcept { integer r; r.m = umag<Bits>(); r.neg = true; return r; }

    constexpr const umag<Bits>& magnitude() const noexcept { return m; }
    constexpr std::uint64_t get_lowest_bits() const noexcept { return m.get_lowest_bits(); }
    constexpr half_type     get_low_bits()    const noexcept { return m.get_low_bits();  }
    constexpr half_type     get_high_bits()   const noexcept { return m.get_high_bits(); }
    constexpr bool is_negative()  const noexcept { return neg; }
    constexpr bool is_undefined() const noexcept { return m.is_zero() && neg; }
    constexpr bool is_zero()      const noexcept { return m.is_zero() && !neg; }

    static constexpr integer max() noexcept { return make(umag<Bits>::max(), false); }
    static constexpr integer min() noexcept { return make(umag<Bits>::max(), true);  }

    template <std::size_t T>
    OPTIONAL_CPP14_CONSTEXPR integer<T, sign::is_signed> truncated() const noexcept {
        return is_undefined() ? integer<T, sign::is_signed>::undefined() : integer<T, sign::is_signed>::make(resize_mag<T>(m), neg);
    }

    OPTIONAL_CPP14_CONSTEXPR integer operator-() const noexcept { return is_undefined() ? *this : make(m, !neg); }
    OPTIONAL_CPP14_CONSTEXPR integer abs() const noexcept { return is_undefined() ? *this : make(m, false); }

    int compare_magnitude(const integer& o) const noexcept { return m.compare(o.m); }
    OPTIONAL_CPP14_CONSTEXPR integer add_magnitudes(const integer& o) const noexcept { return make(m + o.m, false); }
    OPTIONAL_CPP14_CONSTEXPR integer subtract_magnitudes(const integer& o) const noexcept { return make(m - o.m, false); }

    OPTIONAL_CPP14_CONSTEXPR integer operator+(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined()) return undefined();
        if (neg == o.neg) return make(m + o.m, neg);
        if (m >= o.m)     return make(m - o.m, neg);
        return make(o.m - m, o.neg);
    }

    OPTIONAL_CPP14_CONSTEXPR integer operator-(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined()) return undefined();
        return *this + (-o);
    }
    
    OPTIONAL_CPP14_CONSTEXPR integer operator*(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined()) return undefined();
        return make(m * o.m, neg != o.neg);
    }
    
    OPTIONAL_CPP14_CONSTEXPR integer operator/(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined() || o.is_zero()) return undefined();
        return make(m / o.m, neg != o.neg);
    }
    
    OPTIONAL_CPP14_CONSTEXPR integer operator%(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined() || o.is_zero()) return undefined();
        return make(m % o.m, neg);                       
    }

    OPTIONAL_CPP14_CONSTEXPR bool is_exact_division(const integer& o) const noexcept {
        if (o.is_zero()) return false;
        umag<Bits> q, r; m.divmod(o.m, q, r); return r.is_zero();
    }

    OPTIONAL_CPP14_CONSTEXPR integer floor_div(const integer& o) const noexcept {
        if (is_undefined() || o.is_undefined() || o.is_zero()) return undefined();
        integer q = *this / o;
        if ((neg != o.neg) && !is_exact_division(o)) q = q - integer(1);
        return q;
    }

    OPTIONAL_CPP14_CONSTEXPR integer& operator++() noexcept { return *this = *this + integer(1); }
    OPTIONAL_CPP14_CONSTEXPR integer& operator--() noexcept { return *this = *this - integer(1); }
    OPTIONAL_CPP14_CONSTEXPR integer operator++(int) noexcept { integer t(*this); ++*this; return t; }
    OPTIONAL_CPP14_CONSTEXPR integer operator--(int) noexcept { integer t(*this); --*this; return t; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator+=(const integer& o) noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator-=(const integer& o) noexcept { return *this = *this - o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator*=(const integer& o) noexcept { return *this = *this * o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator/=(const integer& o) noexcept { return *this = *this / o; }
    OPTIONAL_CPP14_CONSTEXPR integer& operator%=(const integer& o) noexcept { return *this = *this % o; }

    OPTIONAL_CPP14_CONSTEXPR bool operator==(const integer& o) const noexcept { return neg == o.neg && m == o.m; }
    OPTIONAL_CPP14_CONSTEXPR bool operator!=(const integer& o) const noexcept { return !(*this == o); }

    OPTIONAL_CPP14_CONSTEXPR bool operator< (const integer& o) const noexcept {
        if (neg != o.neg) return neg;
        return neg ? (m > o.m) : (m < o.m);
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator> (const integer& o) const noexcept { return o < *this; }
    OPTIONAL_CPP14_CONSTEXPR bool operator<=(const integer& o) const noexcept { return !(o < *this); }
    OPTIONAL_CPP14_CONSTEXPR bool operator>=(const integer& o) const noexcept { return !(*this < o); }

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
    OPTIONAL_CPP14_CONSTEXPR bool operator==(T v) const noexcept { return *this == integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR bool operator< (T v) const noexcept { return *this <  integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR bool operator> (T v) const noexcept { return *this >  integer(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator+(T v) const noexcept { return to_double() + static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator-(T v) const noexcept { return to_double() - static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator*(T v) const noexcept { return to_double() * static_cast<double>(v); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
    double operator/(T v) const noexcept { return to_double() / static_cast<double>(v); }

    constexpr bool get_bit(std::size_t i) const noexcept { return m.get_bit(i); }
    OPTIONAL_CPP14_CONSTEXPR void set_bit(std::size_t i, bool b = true) noexcept { m.set_bit(i, b); normalize(); }
    OPTIONAL_CPP14_CONSTEXPR bool toggle_bit(std::size_t i) noexcept { bool r = m.toggle_bit(i); normalize(); return r; }
    static OPTIONAL_CPP14_CONSTEXPR unsigned max_digits(unsigned base = 10) noexcept { return umag<Bits>::max_digits(base); }
    static constexpr unsigned max_digits_base2()  noexcept { return umag<Bits>::max_digits_base2(); }
    static constexpr unsigned max_digits_base10() noexcept { return umag<Bits>::max_digits_base10(); }
    static constexpr unsigned max_digits_base16() noexcept { return umag<Bits>::max_digits_base16(); }

    double to_double() const noexcept {
        if (is_undefined()) return std::numeric_limits<double>::quiet_NaN();
        return neg ? -m.to_double() : m.to_double();
    }

    long double to_long_double() const noexcept { return static_cast<long double>(to_double()); }

    std::string to_string(unsigned base = 10) const {
        if (is_undefined()) return "undefined";
        std::string s = m.to_string(base);
        return (neg && s != "0") ? "-" + s : s;
    }

    std::string to_string_scientific(unsigned precision = 15) const;

    static integer from_hex(const std::string& s)     noexcept { return integer(s, 16); }
    static integer from_decimal(const std::string& s) noexcept { return integer(s, 10); }
    static integer from_binary(const std::string& s)  noexcept { return integer(s, 2);  }

    bool parse_string(const std::string& str, int base, bool validate_only = false) noexcept {
        if (str.empty() || base < 2 || base > 36) return false;
        std::size_t start = 0; bool negative = false;
        if (str[0] == '-') { negative = true; start = 1; }
        else if (str[0] == '+') start = 1;
        if (start >= str.size()) return false;
        if (base == 16 && str.size() > start + 2 && str[start] == '0' && (str[start+1]=='x'||str[start+1]=='X')) start += 2;
        umag<Bits> acc, b(static_cast<std::uint64_t>(base));
        bool found = false;

        for (std::size_t i = start; i < str.size(); ++i) {
            char c = str[i]; int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else if (c == '_' || c == ',' || c == '\'') continue;
            else return false;
            if (d < 0 || d >= base) return false;
            found = true;
            if (!validate_only) acc = acc * b + umag<Bits>(static_cast<std::uint64_t>(d));
        }
        
        if (found && !validate_only) { m = acc; neg = negative && !m.is_zero(); }
        return found;
    }

    friend std::ostream& operator<<(std::ostream& os, const integer& x) { return os << x.to_string(); }
};

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_SIGNED_INTEGER_CLASS_SPECIALIZATION_HPP