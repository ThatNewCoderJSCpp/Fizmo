#ifndef FIZMO_MULTIPRECISION_INTEGER_HPP
#define FIZMO_MULTIPRECISION_INTEGER_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <ostream>
#include "../../Basic/fizmo_defines.hpp"

namespace fizmo { 
namespace multiprecision {

enum class sign { is_unsigned = 0, is_signed };

template <std::size_t Bits, typename = void> class umag;
template <std::size_t W>                     struct wide_mul;   

template <std::size_t Bits>
class umag<Bits, typename std::enable_if<(Bits <= 64)>::type> {
    static_assert(Bits == 8 || Bits == 16 || Bits == 32 || Bits == 64, "small umag width must be 8, 16, 32, or 64");

    std::uint64_t v;

    static constexpr std::uint64_t mask() noexcept {
        return Bits >= 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << (Bits & 63)) - std::uint64_t(1));
    }

public:
    static constexpr std::size_t bits = Bits;
    using half_type = std::uint64_t;

    constexpr umag() noexcept : v(0) {}
    constexpr umag(std::uint64_t x) noexcept : v(x & mask()) {}

    template <std::size_t F, typename = typename std::enable_if<(F < Bits)>::type>
    constexpr umag(const umag<F>& src) noexcept : v(src.to_u64() & mask()) {}

    constexpr std::uint64_t to_u64()          const noexcept { return v; }
    constexpr std::uint64_t get_lowest_bits() const noexcept { return v; }
    constexpr half_type     get_low_bits()    const noexcept { return v; }
    constexpr half_type     get_high_bits()   const noexcept { return v; }
    constexpr bool          is_zero()         const noexcept { return v == 0; }
    static constexpr umag   max()             noexcept { return umag(mask()); }

    constexpr bool get_bit(std::size_t i) const noexcept { return i < Bits && ((v >> i) & 1u) != 0; }

    OPTIONAL_CPP14_CONSTEXPR void set_bit(std::size_t i, bool b) noexcept {
        if (i >= Bits) return;
        if (b) v |= (std::uint64_t(1) << i); else v &= ~(std::uint64_t(1) << i);
        v &= mask();
    }

    OPTIONAL_CPP14_CONSTEXPR bool toggle_bit(std::size_t i) noexcept {
        if (i >= Bits) return false;
        v ^= (std::uint64_t(1) << i);
        v &= mask();
        return ((v >> i) & 1u) != 0;
    }

    static constexpr unsigned max_digits_base2()  noexcept { return static_cast<unsigned>(Bits); }
    static constexpr unsigned max_digits_base16() noexcept { return static_cast<unsigned>(Bits / 4); }
    static constexpr unsigned max_digits_base10() noexcept { return static_cast<unsigned>(static_cast<double>(Bits) * 0.301029995663981195) + 1; }

    static OPTIONAL_CPP14_CONSTEXPR unsigned max_digits(unsigned base = 10) noexcept {
        if (base < 2)   return 0;
        if (base == 2)  return max_digits_base2();
        if (base == 10) return max_digits_base10();
        if (base == 16) return max_digits_base16();
        umag mx = max(); umag b(static_cast<std::uint64_t>(base)); unsigned d = 0;
        while (mx >= b) { mx = mx / b; ++d; }
        return d + 1;
    }

    constexpr umag operator~()              const noexcept { return umag(~v); }
    constexpr umag operator&(const umag& o) const noexcept { return umag(v & o.v); }
    constexpr umag operator|(const umag& o) const noexcept { return umag(v | o.v); }
    constexpr umag operator^(const umag& o) const noexcept { return umag(v ^ o.v); }
    constexpr umag operator+(const umag& o) const noexcept { return umag(v + o.v); }
    constexpr umag operator-(const umag& o) const noexcept { return umag(v - o.v); }
    constexpr umag operator*(const umag& o) const noexcept { return umag(v * o.v); }

    constexpr umag operator<<(std::size_t s) const noexcept { return s >= Bits ? umag() : umag(v << s); }
    constexpr umag operator>>(std::size_t s) const noexcept { return s >= Bits ? umag() : umag(v >> s); }

    OPTIONAL_CPP14_CONSTEXPR void divmod(const umag& d, umag& q, umag& r) const noexcept {
        if (d.v == 0) { q = umag(); r = umag(); return; }  
        q = umag(v / d.v);
        r = umag(v % d.v);
    }

    OPTIONAL_CPP14_CONSTEXPR umag operator/(const umag& o) const noexcept { umag q, r; divmod(o, q, r); return q; }
    OPTIONAL_CPP14_CONSTEXPR umag operator%(const umag& o) const noexcept { umag q, r; divmod(o, q, r); return r; }

    constexpr bool operator==(const umag& o) const noexcept { return v == o.v; }
    constexpr bool operator!=(const umag& o) const noexcept { return v != o.v; }
    constexpr bool operator< (const umag& o) const noexcept { return v <  o.v; }
    constexpr bool operator<=(const umag& o) const noexcept { return v <= o.v; }
    constexpr bool operator> (const umag& o) const noexcept { return v >  o.v; }
    constexpr bool operator>=(const umag& o) const noexcept { return v >= o.v; }

    int compare(const umag& o) const noexcept { return (v < o.v) ? -1 : (v > o.v ? 1 : 0); }

    template <std::size_t T>
    constexpr typename std::enable_if<(T < Bits), umag<T>>::type narrow() const noexcept { return umag<T>(v); }

    double to_double() const noexcept { return static_cast<double>(v); }

    std::string to_string(unsigned base = 10) const {
        if (base < 2 || base > 36) return "";
        if (v == 0) return "0";
        std::uint64_t tmp = v; std::string s;

        while (tmp != 0) {
            unsigned d = static_cast<unsigned>(tmp % base);
            s += (d < 10) ? char('0' + d) : char('a' + (d - 10));
            tmp /= base;
        }

        for (std::size_t i = 0, j = s.size(); i < j; ++i, --j) { char t = s[i]; s[i] = s[j - 1]; s[j - 1] = t; }
        return s;
    }
};

template <std::size_t HalfBits, typename = void>
struct native_half { using type = umag<HalfBits>; };

template <std::size_t HalfBits>
struct native_half<HalfBits, typename std::enable_if<(HalfBits <= 64)>::type> {
    using type = std::uint64_t;
};

template <std::size_t HalfBits>
constexpr typename std::enable_if<(HalfBits <= 64), std::uint64_t>::type
to_native_half(const umag<HalfBits>& h) noexcept { return h.to_u64(); }

template <std::size_t HalfBits>
constexpr typename std::enable_if<(HalfBits > 64), umag<HalfBits>>::type
to_native_half(const umag<HalfBits>& h) noexcept { return h; }

template <std::size_t Bits>
class umag<Bits, typename std::enable_if<(Bits >= 128)>::type> {
    static_assert(Bits >= 128 && (Bits & (Bits - 1)) == 0, "Bits must be a power of two >= 128");

public:
    using half = umag<Bits / 2>;
    static constexpr std::size_t bits = Bits;

private:
    half m_low;
    half m_high;

public:
    using half_type = typename native_half<Bits / 2>::type; 

    constexpr umag() noexcept : m_low(), m_high() {}
    constexpr umag(half low, half high) noexcept : m_low(low), m_high(high) {}
    constexpr umag(std::uint64_t x) noexcept : m_low(half(x)), m_high() {}

    template <std::size_t F, typename = typename std::enable_if<(F < Bits)>::type>
    constexpr umag(const umag<F>& src) noexcept : m_low(half(src)), m_high() {}

    static constexpr umag from_bits(half low, half high) noexcept { return umag(low, high); }
    constexpr std::uint64_t to_u64()          const noexcept { return m_low.to_u64(); }
    constexpr std::uint64_t get_lowest_bits() const noexcept { return m_low.to_u64(); }
    constexpr half_type     get_low_bits()    const noexcept { return to_native_half<Bits / 2>(m_low);  }
    constexpr half_type     get_high_bits()   const noexcept { return to_native_half<Bits / 2>(m_high); }

    constexpr bool is_zero() const noexcept { return m_low.is_zero() && m_high.is_zero(); }
    static constexpr umag max() noexcept { return umag(half::max(), half::max()); }

    constexpr bool get_bit(std::size_t i) const noexcept {
        return i < Bits / 2 ? m_low.get_bit(i) : m_high.get_bit(i - Bits / 2);
    }

    OPTIONAL_CPP14_CONSTEXPR void set_bit(std::size_t i, bool b) noexcept {
        if (i < Bits / 2)  m_low.set_bit(i, b);
        else if (i < Bits) m_high.set_bit(i - Bits / 2, b);
    }

    OPTIONAL_CPP14_CONSTEXPR bool toggle_bit(std::size_t i) noexcept {
        if (i < Bits / 2)  return m_low.toggle_bit(i);
        if (i < Bits)      return m_high.toggle_bit(i - Bits / 2);
        return false;
    }

    static constexpr unsigned max_digits_base2()  noexcept { return static_cast<unsigned>(Bits); }
    static constexpr unsigned max_digits_base16() noexcept { return static_cast<unsigned>(Bits / 4); }
    static constexpr unsigned max_digits_base10() noexcept { return static_cast<unsigned>(static_cast<double>(Bits) * 0.301029995663981195) + 1; }
    
    static OPTIONAL_CPP14_CONSTEXPR unsigned max_digits(unsigned base = 10) noexcept {
        if (base < 2)   return 0;
        if (base == 2)  return max_digits_base2();
        if (base == 10) return max_digits_base10();
        if (base == 16) return max_digits_base16();
        umag mx = max(); umag b(static_cast<std::uint64_t>(base)); unsigned d = 0;
        while (mx >= b) { mx = mx / b; ++d; }
        return d + 1;
    }

    constexpr umag operator~() const noexcept { return umag(~m_low, ~m_high); }
    constexpr umag operator&(const umag& o) const noexcept { return umag(m_low & o.m_low, m_high & o.m_high); }
    constexpr umag operator|(const umag& o) const noexcept { return umag(m_low | o.m_low, m_high | o.m_high); }
    constexpr umag operator^(const umag& o) const noexcept { return umag(m_low ^ o.m_low, m_high ^ o.m_high); }

    constexpr bool operator==(const umag& o) const noexcept { return m_low == o.m_low && m_high == o.m_high; }
    constexpr bool operator!=(const umag& o) const noexcept { return !(*this == o); }
    constexpr bool operator< (const umag& o) const noexcept { return m_high < o.m_high || (m_high == o.m_high && m_low < o.m_low); }
    constexpr bool operator> (const umag& o) const noexcept { return o < *this; }
    constexpr bool operator<=(const umag& o) const noexcept { return !(o < *this); }
    constexpr bool operator>=(const umag& o) const noexcept { return !(*this < o); }

    OPTIONAL_CPP14_CONSTEXPR umag operator+(const umag& o) const noexcept {
        half low   = m_low + o.m_low;
        half carry = (low < m_low) ? half(std::uint64_t(1)) : half();
        half high  = m_high + o.m_high + carry;
        return umag(low, high);
    }

    OPTIONAL_CPP14_CONSTEXPR umag operator-(const umag& o) const noexcept {
        half low    = m_low - o.m_low;
        half borrow = (m_low < o.m_low) ? half(std::uint64_t(1)) : half();
        half high   = m_high - o.m_high - borrow;
        return umag(low, high);
    }
    
    OPTIONAL_CPP14_CONSTEXPR umag operator<<(std::size_t n) const noexcept {
        if (n == 0)    return *this;
        if (n >= Bits) return umag();
        const std::size_t H = Bits / 2;
        if (n >= H)    return umag(half(), m_low << (n - H));
        return umag(m_low << n, (m_high << n) | (m_low >> (H - n)));
    }

    OPTIONAL_CPP14_CONSTEXPR umag operator>>(std::size_t n) const noexcept {
        if (n == 0)    return *this;
        if (n >= Bits) return umag();
        const std::size_t H = Bits / 2;
        if (n >= H)    return umag(m_high >> (n - H), half());
        return umag((m_low >> n) | (m_high << (H - n)), m_high >> n);
    }

    OPTIONAL_CPP14_CONSTEXPR umag operator*(const umag& o) const noexcept {
        if (is_zero() || o.is_zero()) return umag();
        half ll_h, ll_l; wide_mul<Bits / 2>::mul(m_low,  o.m_low,  ll_h, ll_l);
        half hl_h, hl_l; wide_mul<Bits / 2>::mul(m_high, o.m_low,  hl_h, hl_l);
        half lh_h, lh_l; wide_mul<Bits / 2>::mul(m_low,  o.m_high, lh_h, lh_l);
        (void)hl_h; 
        (void)lh_h;
        return from_bits(ll_l, ll_h + hl_l + lh_l);
    }

    OPTIONAL_CPP14_CONSTEXPR void divmod(const umag& divisor, umag& quotient, umag& remainder) const noexcept {
        quotient = umag(); remainder = umag();
        if (divisor.is_zero()) return;                  
        if (*this <  divisor) { remainder = *this; return; }
        if (*this == divisor) { quotient = umag(std::uint64_t(1)); return; }
        long highest = -1;

        for (long i = static_cast<long>(Bits) - 1; i >= 0; --i) if (get_bit(static_cast<std::size_t>(i))) { highest = i; break; }
        
        for (long i = highest; i >= 0; --i) {
            remainder = remainder << 1;
            if (get_bit(static_cast<std::size_t>(i))) remainder.set_bit(0, true);
            if (!(remainder < divisor)) { remainder = remainder - divisor; quotient.set_bit(static_cast<std::size_t>(i), true); }
        }
    }

    OPTIONAL_CPP14_CONSTEXPR umag operator/(const umag& o) const noexcept { umag q, r; divmod(o, q, r); return q; }
    OPTIONAL_CPP14_CONSTEXPR umag operator%(const umag& o) const noexcept { umag q, r; divmod(o, q, r); return r; }

    template <std::size_t T>
    constexpr typename std::enable_if<(T == Bits / 2), umag<T>>::type narrow() const noexcept { return m_low; }
    
    template <std::size_t T>
    constexpr typename std::enable_if<(T < Bits / 2), umag<T>>::type narrow() const noexcept { return m_low.template narrow<T>(); }

    int compare(const umag& o) const noexcept { return (*this < o) ? -1 : (*this > o ? 1 : 0); }

    double to_double() const noexcept {
        return m_high.to_double() * std::ldexp(1.0, static_cast<int>(Bits / 2)) + m_low.to_double();
    }

    std::string to_string(unsigned base = 10) const {
        if (base < 2 || base > 36) return "";
        if (is_zero()) return "0";
        umag tmp(*this), b(static_cast<std::uint64_t>(base));
        std::string s;

        while (!tmp.is_zero()) {
            umag q, r;
            tmp.divmod(b, q, r);
            unsigned d = static_cast<unsigned>(r.to_u64());
            s += (d < 10) ? char('0' + d) : char('a' + (d - 10));
            tmp = q;
        }

        for (std::size_t i = 0, j = s.size(); i < j; ++i, --j) { char t = s[i]; s[i] = s[j - 1]; s[j - 1] = t; }
        return s;
    }
};

template <std::size_t W>
struct wide_mul {
    using H = umag<W / 2>;

    static OPTIONAL_CPP14_CONSTEXPR void mul(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const H a_lo = a.get_low_bits(),  a_hi = a.get_high_bits();
        const H b_lo = b.get_low_bits(),  b_hi = b.get_high_bits();
        H ll_h, ll_l; wide_mul<W / 2>::mul(a_lo, b_lo, ll_h, ll_l);
        H lh_h, lh_l; wide_mul<W / 2>::mul(a_lo, b_hi, lh_h, lh_l);
        H hl_h, hl_l; wide_mul<W / 2>::mul(a_hi, b_lo, hl_h, hl_l);
        H hh_h, hh_l; wide_mul<W / 2>::mul(a_hi, b_hi, hh_h, hh_l);
        const H mid_low      = lh_l + hl_l;
        const H carry1       = (mid_low < lh_l) ? H(std::uint64_t(1)) : H();
        const H new_low_high = ll_h + mid_low;
        const H carry2       = (new_low_high < ll_h) ? H(std::uint64_t(1)) : H();
        low = umag<W>::from_bits(ll_l, new_low_high);
        const H mid_high        = lh_h + hl_h + carry1 + carry2;
        const H carry3          = (mid_high < lh_h) ? H(std::uint64_t(1)) : H();
        const H result_high_low = hh_l + mid_high;
        const H carry4          = (result_high_low < hh_l) ? H(std::uint64_t(1)) : H();
        high = umag<W>::from_bits(result_high_low, hh_h + carry3 + carry4);
    }
};

template <>
struct wide_mul<64> {
    static OPTIONAL_CPP14_CONSTEXPR void mul(const umag<64>& A, const umag<64>& B, umag<64>& high, umag<64>& low) noexcept {
        const std::uint64_t a = A.to_u64(), b = B.to_u64();
        const std::uint64_t a_low = a & 0xFFFFFFFFULL, a_high = a >> 32;
        const std::uint64_t b_low = b & 0xFFFFFFFFULL, b_high = b >> 32;
        const std::uint64_t low_prod  = a_low * b_low;
        const std::uint64_t mid1      = a_low * b_high;
        const std::uint64_t mid2      = a_high * b_low;
        const std::uint64_t high_prod = a_high * b_high;
        const std::uint64_t mid   = mid1 + mid2;
        const std::uint64_t carry = (mid < mid1) ? (1ULL << 32) : 0ULL;
        const std::uint64_t low_u = low_prod + (mid << 32);
        const std::uint64_t high_u = high_prod + (mid >> 32) + carry + ((low_u < low_prod) ? 1ULL : 0ULL);
        low = umag<64>(low_u); high = umag<64>(high_u);
    }
};

template <std::size_t T, std::size_t F>
constexpr typename std::enable_if<(T == F), umag<T>>::type resize_mag(const umag<F>& x) noexcept { return x; }

template <std::size_t T, std::size_t F>
constexpr typename std::enable_if<(T > F), umag<T>>::type resize_mag(const umag<F>& x) noexcept { return umag<T>(x); }

template <std::size_t T, std::size_t F>
constexpr typename std::enable_if<(T < F), umag<T>>::type resize_mag(const umag<F>& x) noexcept { return x.template narrow<T>(); }

template <std::size_t F, sign SF, std::size_t T, sign ST>
struct is_lossless : std::integral_constant<bool, (SF == sign::is_signed && ST == sign::is_unsigned) ? false : (T >= F)> {};

template <std::size_t Bits, sign S> class integer;

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_INTEGER_HPP