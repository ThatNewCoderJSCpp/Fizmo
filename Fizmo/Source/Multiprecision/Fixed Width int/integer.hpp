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

constexpr int clz64_recursive(std::uint64_t x, int n = 0) noexcept {
    return x == 0 ? 64
         : !(x & 0xFFFFFFFF00000000ull) ? clz64_recursive(x << 32, n + 32)
         : !(x & 0xFFFF000000000000ull) ? clz64_recursive(x << 16, n + 16)
         : !(x & 0xFF00000000000000ull) ? clz64_recursive(x <<  8, n +  8)
         : !(x & 0xF000000000000000ull) ? clz64_recursive(x <<  4, n +  4)
         : !(x & 0xC000000000000000ull) ? clz64_recursive(x <<  2, n +  2)
         : !(x & 0x8000000000000000ull) ? n + 1
         : n;
}

constexpr int ctz64_recursive(std::uint64_t x) noexcept {          
    return 63 - clz64_recursive(x & (~x + std::uint64_t(1)));
}

constexpr inline int clz64(std::uint64_t x) noexcept {
    if (x == 0) return 64;
    int n = 0;
    if (!(x & 0xFFFFFFFF00000000ull)) { x <<= 32; n += 32; }
    if (!(x & 0xFFFF000000000000ull)) { x <<= 16; n += 16; }
    if (!(x & 0xFF00000000000000ull)) { x <<=  8; n +=  8; }
    if (!(x & 0xF000000000000000ull)) { x <<=  4; n +=  4; }
    if (!(x & 0xC000000000000000ull)) { x <<=  2; n +=  2; }
    if (!(x & 0x8000000000000000ull)) {           n +=  1; }
    return n;
}

constexpr int ctz64(std::uint64_t x) noexcept {          
    return 63 - clz64(x & (~x + std::uint64_t(1)));
}

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

    OPTIONAL_CPP14_CONSTEXPR long long highest_bit() const noexcept {
        return v ? (63 - static_cast<long long>(clz64_recursive(v))) : -1;
    }

    OPTIONAL_CPP14_CONSTEXPR long long count_leading_zeros() const noexcept {
        const long long h = highest_bit();
        return (h < 0) ? static_cast<long long>(Bits) : static_cast<long long>(Bits) - 1 - h;
    }

    constexpr bool any_bit_below(std::size_t n) const noexcept {
        return n == 0   ? false
            : n >= Bits ? (v != 0)
            : (v & ((std::uint64_t(1) << n) - std::uint64_t(1))) != 0;
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

    OPTIONAL_CPP14_CONSTEXPR std::uint32_t
    divmod_small(std::uint32_t d, std::uint32_t rem, umag& q) const noexcept {
        std::uint64_t cur = (std::uint64_t(rem) << 32) | (v >> 32);
        const std::uint64_t qh = cur / d;
        rem = static_cast<std::uint32_t>(cur % d);
        cur = (std::uint64_t(rem) << 32) | (v & 0xFFFFFFFFull);
        const std::uint64_t ql = cur / d;
        rem = static_cast<std::uint32_t>(cur % d);
        q = umag((qh << 32) | ql);
        return rem;
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

    OPTIONAL_CPP14_CONSTEXPR long long highest_bit() const noexcept {
        const long long h = m_high.highest_bit();
        if (h >= 0) return h + static_cast<long long>(Bits / 2);
        return m_low.highest_bit();
    }
    
    OPTIONAL_CPP14_CONSTEXPR long long count_leading_zeros() const noexcept {
        const long long h = highest_bit();
        return (h < 0) ? static_cast<long long>(Bits) : static_cast<long long>(Bits) - 1 - h;
    }

    OPTIONAL_CPP14_CONSTEXPR bool any_bit_below(std::size_t n) const noexcept {
        if (n == 0)    return false;
        if (n >= Bits) return !is_zero();
        if (n <= Bits / 2) return m_low.any_bit_below(n);
        return !m_low.is_zero() || m_high.any_bit_below(n - Bits / 2);
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
        const long long ha = highest_bit();
        const long long hb = divisor.highest_bit();
        const long long shift = ha - hb;                 
        remainder = *this >> static_cast<std::size_t>(shift);

        for (long long i = shift; i >= 0; --i) {
            if (!(remainder < divisor)) {
                remainder = remainder - divisor;
                quotient.set_bit(static_cast<std::size_t>(i), true);
            }

            if (i > 0) {
                remainder = remainder << 1;
                if (get_bit(static_cast<std::size_t>(i - 1))) remainder.set_bit(0, true);
            }
        }
    }

    OPTIONAL_CPP14_CONSTEXPR std::uint32_t
    divmod_small(std::uint32_t d, std::uint32_t rem, umag& q) const noexcept {
        half qh, ql;
        rem = m_high.divmod_small(d, rem, qh);
        rem = m_low .divmod_small(d, rem, ql);
        q = umag(ql, qh);
        return rem;
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
        umag tmp(*this);
        std::string s;

        while (!tmp.is_zero()) {
            umag q;
            const unsigned d = static_cast<unsigned>(tmp.divmod_small(static_cast<std::uint32_t>(base), 0u, q));
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
        if (a.is_zero() || b.is_zero()) { high = umag<W>(); low = umag<W>(); return; }
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

template <>
struct wide_mul<32> {
    static OPTIONAL_CPP14_CONSTEXPR void mul(const umag<32>& a, const umag<32>& b, umag<32>& high, umag<32>& low) noexcept {
        const std::uint64_t p = a.to_u64() * b.to_u64();
        low  = umag<32>(p);
        high = umag<32>(p >> 32);
    }
};

template <>
struct wide_mul<16> {
    static OPTIONAL_CPP14_CONSTEXPR void mul(const umag<16>& a, const umag<16>& b, umag<16>& high, umag<16>& low) noexcept {
        const std::uint64_t p = a.to_u64() * b.to_u64();
        low  = umag<16>(p);
        high = umag<16>(p >> 16);
    }
};

template <>
struct wide_mul<8> {
    static OPTIONAL_CPP14_CONSTEXPR void mul(const umag<8>& a, const umag<8>& b, umag<8>& high, umag<8>& low) noexcept {
        const std::uint64_t p = a.to_u64() * b.to_u64();
        low  = umag<8>(p);
        high = umag<8>(p >> 8);
    }
};

template <std::size_t W>
constexpr typename std::enable_if<(W >= 64), umag<W * 2>>::type
combine_product(const umag<W>& low, const umag<W>& high) noexcept {
    return umag<W * 2>::from_bits(low, high);
}

template <std::size_t W>
constexpr typename std::enable_if<(W < 64), umag<W * 2>>::type
combine_product(const umag<W>& low, const umag<W>& high) noexcept {
    return umag<W * 2>((high.to_u64() << W) | low.to_u64());
}


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