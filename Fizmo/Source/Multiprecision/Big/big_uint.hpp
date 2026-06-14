#ifndef MULTIPRECISION_BIG_UINT_HPP
#define MULTIPRECISION_BIG_UINT_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <initializer_list>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include "../Fixed Width Float/traits.hpp"

namespace fizmo {
namespace multiprecision {

namespace detail {

template <typename T>
OPTIONAL_CPP14_CONSTEXPR T compute_ln2() noexcept {
    T one(1); 
    T three(3); 
    T two(2); 
    T y = one / three; 
    T y2 = y * y; 
    T y_power = y; 
    T sum = T::zero(); 
    T prev = T::nan(); 
    constexpr std::size_t max_iter = static_cast<std::size_t>(T::mantissa_bits / 3) + 16; 
    
    for (std::size_t k = 0; k < max_iter; ++k) { 
        T term = y_power / T(2 * k + 1); 
        T next = sum + term; 
        if (next == sum || next == prev) break; 
        prev = sum; 
        sum = next; 
        y_power = y_power * y2; 
    } 
    
    return sum * two; 
}

template <typename T>
T ln2_const() noexcept {
    static const T value = compute_ln2<T>();
    return value;
}

template <typename T, typename = typename std::enable_if<is_fizmo_fixed_float_v<T>>::type> 
OPTIONAL_CPP14_CONSTEXPR T compute_ln10() noexcept { 
    T one(1); 
    T two(2); 
    T three(3); 
    T five(5); 
    T y = (five - one) / (five + one); 
    T y2 = y * y; 
    T y_power = y; 
    T sum = T::zero(); 
    T prev = T::nan(); 
    constexpr std::size_t max_iter = static_cast<std::size_t>(T::mantissa_bits / 2.5) + 16; 
    
    for (std::size_t k = 0; k < max_iter; ++k) { 
        T term = y_power / T(2 * k + 1); 
        T next = sum + term; 
        if (next == sum || next == prev) break; 
        prev = sum; 
        sum = next; 
        y_power = y_power * y2; 
    } 
    
    T ln5 = sum * two; 
    T ln2 = ln2_const<T>(); 
    return ln2 + ln5; 
}

template <typename T>
T ln10_const() noexcept {
    static const T value = compute_ln10<T>();
    return value;
}

} // namespace detail

struct DivmodResult;
class BigInt;

class BigUint {
private:
    std::vector<std::uint64_t> m_data;

    void normalize() noexcept {
        while (!m_data.empty() && m_data.back() == 0) m_data.pop_back();
    }
    
    static constexpr std::uint64_t MAX_LIMBS = 1000000000ULL;

private:
    BigUint div_small(std::uint64_t d) const {
        BigUint result;
        if (is_zero()) { return result; }
        result.m_data.resize(m_data.size());
        std::uint64_t rem = 0;

        for (std::size_t i = m_data.size(); i-- > 0; ) {
            uint128 cur = (uint128(rem) << 64) + uint128(m_data[i]);
            uint128 q   = cur / d;
            uint128 r   = cur % d;
            result.m_data[i] = q.get_lowest_bits();
            rem = r.get_low_bits();
        }

        result.normalize();
        return result;
    }

    std::uint64_t mod_small(std::uint64_t d) const {
        if (is_zero() || d == 0) return 0;
        uint128 rem(0);

        for (std::size_t i = m_data.size(); i-- > 0; ) {
            rem = (rem << 64) + uint128(m_data[i]);
            rem = rem % uint128(d);
        }

        return rem.get_low_bits();
    }

    static void add_limbs_at(
        std::vector<std::uint64_t>& dst,
        const std::vector<std::uint64_t>& src,
        std::size_t offset
    ) {
        if (src.empty()) return;
        if (dst.size() < offset + src.size() + 1) dst.resize(offset + src.size() + 1, 0);
        uint128 carry(0);

        for (std::size_t i = 0; i < src.size(); ++i) {
            uint128 sum = uint128(dst[offset + i]) + uint128(src[i]) + carry;
            dst[offset + i] = sum.get_low_bits();
            carry = sum.get_high_bits();
        }

        for (std::size_t i = offset + src.size(); !carry.is_zero() && i < dst.size(); ++i) {
            uint128 sum = uint128(dst[i]) + carry;
            dst[i] = sum.get_low_bits();
            carry = sum.get_high_bits();
        }

        if (!carry.is_zero()) dst.push_back(carry.get_low_bits());
    }

    BigUint mul_small(std::uint64_t d) const {
        if (d == 0 || is_zero()) return BigUint::zero();
        if (d == 1) return *this;
        BigUint result;
        result.m_data.resize(m_data.size() + 1);
        uint128 carry(0);

        for (std::size_t i = 0; i < m_data.size(); ++i) {
            uint128 prod = uint128(m_data[i]) * uint128(d) + carry;
            result.m_data[i] = prod.get_low_bits();
            carry = prod.get_high_bits();
        }

        result.m_data[m_data.size()] = carry.get_low_bits();
        result.normalize();
        return result;
    }

    BigUint add_small(std::uint64_t d) const {
        if (d == 0) return *this;
        if (is_zero()) return BigUint(d);
        BigUint r = *this;
        uint128 carry(d);

        for (std::size_t i = 0; i < r.m_data.size() && !carry.is_zero(); ++i) {
            uint128 s = uint128(r.m_data[i]) + carry;
            r.m_data[i] = s.get_low_bits();
            carry = s.get_high_bits();
        }

        if (!carry.is_zero()) r.m_data.push_back(carry.get_low_bits());
        return r;
    }

    BigUint sub_small(std::uint64_t d) const {
        if (d == 0) return *this;
        if (is_zero() || *this < BigUint(d)) return BigUint::zero();
        BigUint r = *this;
        std::uint64_t borrow = d;

        for (std::size_t i = 0; i < r.m_data.size() && borrow; ++i) {
            std::uint64_t v = r.m_data[i];
            r.m_data[i] = v - borrow;
            borrow = (v < borrow) ? 1u : 0u;
        }

        r.normalize();
        return r;
    }

public:
    BigUint() noexcept = default;
    
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint(T value) {
        using U = typename std::make_unsigned<T>::type;
        U v = static_cast<U>(value);
        if (v != 0) { m_data.push_back(static_cast<std::uint64_t>(v)); }
    }

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    BigUint(const T& value) {
        const std::size_t bits = fizmo::fizmo_numeric_bit_width_v<T>;
        const std::size_t limbs = (bits + 63) / 64;
        m_data.assign(limbs, 0);

        for (std::size_t i = 0; i < bits; ++i) {
            if (value.get_bit(i)) {
                std::size_t limb = i / 64;
                std::size_t offset = i % 64;
                m_data[limb] |= (1ULL << offset);
            }
        }

        normalize();
    }

    BigUint(std::initializer_list<std::uint64_t> limbs) : m_data(limbs) { normalize(); }
    BigUint(const BigUint&) = default;
    BigUint(BigUint&& other) noexcept = default;
    BigUint& operator=(const BigUint&) = default;
    BigUint& operator=(BigUint&&) noexcept = default;
    BigUint(const BigInt& i);
    BigUint& operator=(const BigInt& i);
    explicit operator BigInt();

    template <typename T, typename = typename std::enable_if<fizmo::is_fizmo_static_int_v<T>>::type>
    BigUint& operator=(const T& value) {
        *this = BigUint(value);
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint& operator=(const T value) {
        *this = BigUint(value);
        return *this;
    }

    BigUint(const std::string& s);
    
    static BigUint filled(std::size_t limbs, std::uint64_t value) {
        BigUint r;
        if (limbs == 0) return r;
        r.m_data.assign(limbs, value);
        r.normalize();
        return r;
    }

public:
    bool is_zero() const noexcept { return m_data.empty(); }
    bool is_even() const noexcept { return is_zero() || ((m_data[0] & 1ULL) == 0); }
    bool is_one() const noexcept { return *this == BigUint::one(); }
    std::size_t limb_count() const noexcept { return m_data.size(); }
    std::uint64_t limb(std::size_t i) const noexcept { return (i < m_data.size()) ? m_data[i] : 0; }
    const std::vector<std::uint64_t>& data() const noexcept { return m_data; }

public:
    std::size_t get_exponent_base2() const noexcept {
        if (is_zero()) return 0;
        return bit_length() - 1;
    }

    std::size_t get_exponent_base10() const {
        if (is_zero()) return 0;
        std::string s = to_string(false, '\0'); 
        return s.size() - 1;
    }

    std::size_t count_trailing_zeros() const noexcept {
        std::size_t tz = 0;

        for (std::size_t i = 0; i < limb_count(); ++i) {
            std::uint64_t limb = m_data[i];

            if (limb == 0) {
                tz += 64;
                continue;
            }

            std::uint64_t x = limb;
            std::size_t c = 0;
            
            while ((x & 1) == 0) {
                x >>= 1;
                ++c;
            }

            tz += c;
            break;
        }

        return tz;
    }

public:
    static BigUint zero() noexcept { return BigUint(); }
    static BigUint one() noexcept { return BigUint(1); }

    template <typename T, typename = typename std::enable_if<is_fizmo_fixed_float_v<T>>::type>
    static T log2_max() noexcept { return T(64) * T(MAX_LIMBS); }

    template <typename T, typename = typename std::enable_if<is_fizmo_fixed_float_v<T>>::type>
    static T ln_max() noexcept { return log2_max<T>() * detail::ln2_const<T>(); }

    template <typename T, typename = typename std::enable_if<is_fizmo_fixed_float_v<T>>::type>
    static T log10_max() noexcept { return ln_max<T>() / detail::ln10_const<T>(); }

public:
    friend bool operator==(const BigUint& a, const BigUint& b) noexcept { return a.m_data == b.m_data; }
    friend bool operator!=(const BigUint& a, const BigUint& b) noexcept { return !(a == b); }

    friend bool operator<(const BigUint& a, const BigUint& b) noexcept {
        if (a.m_data.size() != b.m_data.size()) return a.m_data.size() < b.m_data.size();
        for (std::size_t i = a.m_data.size(); i-- > 0; ) { if (a.m_data[i] != b.m_data[i]) { return a.m_data[i] < b.m_data[i]; }}
        return false;
    }

    friend bool operator>(const BigUint& a, const BigUint& b) noexcept { return b < a; }
    friend bool operator<=(const BigUint& a, const BigUint& b) noexcept { return !(b < a); }
    friend bool operator>=(const BigUint& a, const BigUint& b) noexcept { return !(a < b); }

// All range-based methods are semi-open on [lo, hi)
public:
    static unsigned int clz64(std::uint64_t x) noexcept {
        if (x == 0) return 64;
        unsigned int n = 0;
        if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
        if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
        if ((x & 0xFF00000000000000ULL) == 0) { n += 8;  x <<= 8;  }
        if ((x & 0xF000000000000000ULL) == 0) { n += 4;  x <<= 4;  }
        if ((x & 0xC000000000000000ULL) == 0) { n += 2;  x <<= 2;  }
        if ((x & 0x8000000000000000ULL) == 0) { n += 1;             }
        return n;
    }

    std::size_t bit_length() const noexcept {
        if (is_zero()) return 0;
        std::size_t top = m_data.size() - 1;
        std::uint64_t v = m_data[top];
        std::size_t bits = top * 64;
        while (v) { ++bits; v >>= 1; }
        return bits;
    }

    bool get_bit(std::size_t i) const noexcept {
        std::size_t limb = i / 64;
        std::size_t off  = i % 64;
        if (limb >= m_data.size()) return false;
        return (m_data[limb] >> off) & 1ULL;
    }

    std::vector<bool> get_bits(std::size_t lo, std::size_t hi) const {
        std::vector<bool> result;
        if (lo >= hi) return result;
        result.reserve(hi - lo);
        for (std::size_t i = lo; i < hi; ++i) { result.push_back(get_bit(i)); }
        return result;
    }

    void set_bit(std::size_t i) {
        std::size_t limb = i / 64;
        std::size_t off  = i % 64;
        if (limb >= m_data.size()) m_data.resize(limb + 1, 0);
        m_data[limb] |= (1ULL << off);
    }

    void set_bits(std::size_t lo, std::size_t hi) {
        if (lo >= hi) return;
        std::size_t last_limb = (hi - 1) / 64;
        if (last_limb >= m_data.size()) m_data.resize(last_limb + 1, 0);

        for (std::size_t i = lo; i < hi; ) {
            std::size_t limb = i / 64;
            std::size_t off  = i % 64;
            std::size_t bits_in_limb = std::min(hi - i, 64 - off);
            std::uint64_t mask = (bits_in_limb == 64) ? ~0ULL : ((1ULL << bits_in_limb) - 1) << off;
            m_data[limb] |= mask;
            i += bits_in_limb;
        }
    }

    void set_bit(std::size_t i, bool value) {
        std::size_t limb = i / 64;
        std::size_t off  = i % 64;

        if (value) {
            if (limb >= m_data.size()) m_data.resize(limb + 1, 0);
            m_data[limb] |= (1ULL << off);
        } else {
            if (limb >= m_data.size()) return; 
            m_data[limb] &= ~(1ULL << off);
            normalize();
        }
    }

    void set_bits(std::size_t lo, std::size_t hi, bool value) {
        if (value) {
            set_bits(lo, hi);
        } else {
            clear_bits(lo, hi);
        }
    }

    void clear_bit(std::size_t i) {
        std::size_t limb = i / 64;
        std::size_t off  = i % 64;
        if (limb >= m_data.size()) return;
        m_data[limb] &= ~(1ULL << off);
        normalize();
    }

    void clear_bits(std::size_t lo, std::size_t hi) {
        if (lo >= hi) return;

        for (std::size_t i = lo; i < hi; ) {
            std::size_t limb = i / 64;
            std::size_t off  = i % 64;
            if (limb >= m_data.size()) break; 
            std::size_t bits_in_limb = std::min(hi - i, 64 - off);
            std::uint64_t mask = (bits_in_limb == 64) ? ~0ULL : ((1ULL << bits_in_limb) - 1) << off;
            m_data[limb] &= ~mask;
            i += bits_in_limb;
        }

        normalize();
    }

    void flip_bit(std::size_t i) {
        std::size_t limb = i / 64;
        std::size_t off  = i % 64;
        if (limb >= m_data.size()) m_data.resize(limb + 1, 0);
        m_data[limb] ^= (1ULL << off);
        normalize();
    }

    void flip_bits(std::size_t lo, std::size_t hi) {
        if (lo >= hi) return;
        std::size_t last_limb = (hi - 1) / 64;
        if (last_limb >= m_data.size()) m_data.resize(last_limb + 1, 0);

        for (std::size_t i = lo; i < hi; ) {
            std::size_t limb = i / 64;
            std::size_t off  = i % 64;
            std::size_t bits_in_limb = std::min(hi - i, 64 - off);
            std::uint64_t mask = (bits_in_limb == 64) ? ~0ULL : ((1ULL << bits_in_limb) - 1) << off;
            m_data[limb] ^= mask;
            i += bits_in_limb;
        }

        normalize();
    }

public:
    BigUint slice(std::size_t lo, std::size_t hi) const {
        BigUint r;
        if (lo >= hi) return r;
        std::size_t end = std::min(hi, m_data.size());
        if (lo >= end) return r;
        r.m_data.assign(m_data.begin() + lo, m_data.begin() + end);
        r.normalize();
        return r;
    }

    // this * 2^(shift_limbs*64) + low
    BigUint concatenate(const BigUint& low, std::size_t shift_limbs) const {
        BigUint r;
        std::size_t total = std::max(low.m_data.size(), shift_limbs + m_data.size());
        r.m_data.assign(total, 0);
        for (std::size_t i = 0; i < low.m_data.size(); ++i) r.m_data[i] = low.m_data[i];
        for (std::size_t i = 0; i < m_data.size(); ++i) r.m_data[i + shift_limbs] = m_data[i];
        r.normalize();
        return r;
    }

    BigUint get_bits_as_int(std::size_t lo, std::size_t hi) const {
        BigUint r;
        if (lo >= hi) return r;
        const std::size_t count = hi - lo;
        const std::size_t out_limbs = (count + 63) / 64;
        r.m_data.assign(out_limbs, 0);

        for (std::size_t i = 0; i < count; ++i) {
            if (get_bit(lo + i)) { r.m_data[i / 64] |= (1ULL << (i % 64)); }
        }

        r.normalize();
        return r;
    }

    BigUint shift_limbs(std::size_t n) const {
        if (is_zero() || is_one() || n == 0) return *this;
        BigUint r;
        r.m_data.resize(m_data.size() + n, 0);
        for (std::size_t i = 0; i < m_data.size(); ++i) r.m_data[i + n] = m_data[i];
        return r;
    }

public:
    friend BigUint operator+(const BigUint& a, const BigUint& b) {
        if (a.is_zero()) { return b; }
        if (b.is_zero()) { return a; }
        BigUint result;
        const std::size_t n = std::max(a.m_data.size(), b.m_data.size());
        result.m_data.resize(n);
        uint128 carry(0);

        for (std::size_t i = 0; i < n; ++i) {
            uint128 av = (i < a.m_data.size() ? uint128(a.m_data[i]) : uint128(0));
            uint128 bv = (i < b.m_data.size() ? uint128(b.m_data[i]) : uint128(0));
            uint128 sum = av + bv + carry;
            result.m_data[i] = sum.get_low_bits();  
            carry = sum.get_high_bits();
        }

        if (!carry.is_zero()) { result.m_data.push_back(carry.get_low_bits()); }
        return result;
    }

    BigUint& operator+=(const BigUint& other) {
        if (is_zero()) { *this = other; return *this; }
        if (other.is_zero()) { return *this; }
        const std::size_t n = std::max(m_data.size(), other.m_data.size());
        if (m_data.size() < n) m_data.resize(n, 0);
        uint128 carry(0);

        for (std::size_t i = 0; i < n; ++i) {
            uint128 av(m_data[i]);
            uint128 bv = (i < other.m_data.size() ? uint128(other.m_data[i]) : uint128(0));
            uint128 sum = av + bv + carry;
            m_data[i] = sum.get_low_bits();
            carry = sum.get_high_bits();
        }

        if (!carry.is_zero()) { m_data.push_back(carry.get_low_bits()); }
        return *this;
    }

    BigUint  operator+(std::uint64_t d) const { return add_small(d); }

    BigUint& operator+=(std::uint64_t d) {
        if (d == 0) return *this;
        if (is_zero()) { m_data.push_back(d); return *this; }
        uint128 carry(d);

        for (std::size_t i = 0; i < m_data.size() && !carry.is_zero(); ++i) {
            uint128 s = uint128(m_data[i]) + carry;
            m_data[i] = s.get_low_bits();
            carry = s.get_high_bits();
        }

        if (!carry.is_zero()) m_data.push_back(carry.get_low_bits());
        return *this;
    }

    friend BigUint operator-(const BigUint& a, const BigUint& b) {
        if (a <= b) { return BigUint::zero(); }
        if (b.is_zero()) { return a; }
        BigUint result;
        result.m_data.resize(a.m_data.size());
        std::uint64_t borrow = 0;

        for (std::size_t i = 0; i < a.m_data.size(); ++i) {
            uint128 av(a.m_data[i]);
            uint128 bv = (i < b.m_data.size() ? uint128(b.m_data[i]) : uint128(0));
            uint128 sub = bv + uint128(borrow);

            if (av >= sub) {
                result.m_data[i] = (av - sub).get_low_bits();
                borrow = 0;
            } else {
                uint128 base = uint128(1) << 64;
                result.m_data[i] = (base + av - sub).get_low_bits();
                borrow = 1;
            }
        }

        result.normalize();
        return result;
    }

    BigUint& operator-=(const BigUint& other) {
        if (*this <= other) {
            *this = BigUint::zero();
            return *this;
        }

        if (other.is_zero()) { return *this; }
        std::uint64_t borrow = 0;

        for (std::size_t i = 0; i < m_data.size(); ++i) {
            uint128 av(m_data[i]);
            uint128 bv = (i < other.m_data.size() ? uint128(other.m_data[i]) : uint128(0));
            uint128 sub = bv + uint128(borrow);

            if (av >= sub) {
                m_data[i] = (av - sub).get_low_bits();
                borrow = 0;
            } else {
                uint128 base = uint128(1) << 64;
                m_data[i] = (base + av - sub).get_low_bits();
                borrow = 1;
            }
        }

        normalize();
        return *this;
    }

    BigUint  operator-(std::uint64_t d) const { return sub_small(d); }

    BigUint& operator-=(std::uint64_t d) {
        if (d == 0) return *this;
        if (is_zero() || *this < BigUint(d)) { m_data.clear(); return *this; }
        std::uint64_t borrow = d;

        for (std::size_t i = 0; i < m_data.size() && borrow; ++i) {
            std::uint64_t v = m_data[i];
            m_data[i] = v - borrow;
            borrow = (v < borrow) ? 1u : 0u;
        }

        normalize();
        return *this;
    }

public:
    friend BigUint operator+(BigUint&& a, const BigUint& b) { a += b; return a; }
    friend BigUint operator+(const BigUint& a, BigUint&& b) { b += a; return b; }
    friend BigUint operator+(BigUint&& a, BigUint&& b)      { a += b; return a; }

    friend BigUint operator-(BigUint&& a, const BigUint& b) { a -= b; return a; }
    friend BigUint operator-(BigUint&& a, BigUint&& b)      { a -= b; return a; }

    friend BigUint operator&(BigUint&& a, const BigUint& b) { a &= b; return a; }
    friend BigUint operator&(const BigUint& a, BigUint&& b) { b &= a; return b; }
    friend BigUint operator&(BigUint&& a, BigUint&& b)      { a &= b; return a; }

    friend BigUint operator|(BigUint&& a, const BigUint& b) { a |= b; return a; }
    friend BigUint operator|(const BigUint& a, BigUint&& b) { b |= a; return b; }
    friend BigUint operator|(BigUint&& a, BigUint&& b)      { a |= b; return a; }

    friend BigUint operator^(BigUint&& a, const BigUint& b) { a ^= b; return a; }
    friend BigUint operator^(const BigUint& a, BigUint&& b) { b ^= a; return b; }
    friend BigUint operator^(BigUint&& a, BigUint&& b)      { a ^= b; return a; }

public:
    friend BigUint operator*(const BigUint& a, const BigUint& b) { 
        if (a.is_zero() || b.is_zero()) return BigUint::zero();
        if (a.is_one()) return b;
        if (b.is_one()) return a;
        return a.mul_textbook(b);
    }

    BigUint& operator*=(const BigUint& b) { 
        if (is_zero()) return *this;
        if (b.is_zero()) { m_data.clear(); return *this; }
        if (b.is_one()) return *this;
        if (is_one()) { m_data = b.m_data; return *this; }
        BigUint tmp = *this * b;
        m_data = std::move(tmp.m_data);
        return *this;    
    }

    BigUint operator*(std::uint64_t d) const { return mul_small(d); } 
    BigUint& operator*=(std::uint64_t d) { *this = mul_small(d); return *this; }

public:
    BigUint mul_textbook(const BigUint& other) const {
        if (is_zero() || other.is_zero()) return BigUint::zero();
        if (is_one()) { return other; }
        if (other.is_one()) { return *this; }
        BigUint result;
        result.m_data.assign(m_data.size() + other.m_data.size(), 0);

        for (std::size_t i = 0; i < m_data.size(); ++i) {
            uint128 carry(0);
            uint128 a(m_data[i]);

            for (std::size_t j = 0; j < other.m_data.size(); ++j) {
                uint128 b(other.m_data[j]);
                uint128 prod = a * b + uint128(result.m_data[i + j]) + carry;
                result.m_data[i + j] = prod.get_low_bits();
                carry = prod.get_high_bits();
            }

            if (!carry.is_zero()) { 
                std::size_t k = i + other.m_data.size();
                uint128 tmp = uint128(result.m_data[k]) + carry;
                result.m_data[k] = tmp.get_low_bits();
                uint128 c2 = tmp.get_high_bits();

                while (!c2.is_zero()) {
                    ++k;
                    if (k >= result.m_data.size()) result.m_data.push_back(0);
                    uint128 tmp2 = uint128(result.m_data[k]) + c2;
                    result.m_data[k] = tmp2.get_low_bits();
                    c2 = tmp2.get_high_bits();
                }    
            }
        }

        result.normalize();
        return result;
    }

    BigUint mul_karatsuba(const BigUint& b) const {
        if (is_zero() || b.is_zero()) return BigUint::zero();
        if (is_one()) { return b; }
        if (b.is_one()) { return *this; }
        const std::size_t n = std::max(m_data.size(), b.m_data.size());
        const std::size_t half = n / 2;
        BigUint a_lo = slice(0, half);
        BigUint a_hi = slice(half, m_data.size());
        BigUint b_lo = b.slice(0, half);
        BigUint b_hi = b.slice(half, b.m_data.size());
        BigUint z0 = a_lo.mul_karatsuba(b_lo);
        BigUint z2 = a_hi.mul_karatsuba(b_hi);
        a_lo += a_hi;
        b_lo += b_hi;
        a_hi.m_data.clear();
        b_hi.m_data.clear();
        BigUint z1 = a_lo.mul_karatsuba(b_lo);
        z1 -= z0;
        z1 -= z2;
        BigUint result;
        const std::size_t result_size = m_data.size() + b.m_data.size() + 1;
        result.m_data.assign(result_size, 0);
        add_limbs_at(result.m_data, z0.m_data, 0);
        add_limbs_at(result.m_data, z1.m_data, half);
        add_limbs_at(result.m_data, z2.m_data, 2 * half);
        result.normalize();
        return result;
    }

    BigUint mul_toom3(const BigUint& other) const {
        if (is_zero() || other.is_zero()) return BigUint::zero();
        if (is_one()) return other;
        if (other.is_one()) return *this;
        const std::size_t n = std::max(m_data.size(), other.m_data.size());
        const std::size_t k = (n + 2) / 3;
        BigUint a0 = slice(0, k);
        BigUint a1 = slice(k, 2 * k);
        BigUint a2 = slice(2 * k, m_data.size());
        BigUint b0 = other.slice(0, k);
        BigUint b1 = other.slice(k, 2 * k);
        BigUint b2 = other.slice(2 * k, other.m_data.size());
        BigUint p1 = a0 + a1;  p1 += a2;
        BigUint q1 = b0 + b1;  q1 += b2;
        BigUint a02 = a0 + a2;
        BigUint b02 = b0 + b2;
        bool neg_p = a02 < a1;
        BigUint pm1 = neg_p ? (a1 - a02) : (a02 - a1);
        bool neg_q = b02 < b1;
        BigUint qm1 = neg_q ? (b1 - b02) : (b02 - b1);
        bool neg_vm = neg_p != neg_q;
        a02.m_data.clear();
        b02.m_data.clear();
        BigUint p2 = (a2 << 1);  p2 += a1;  p2 <<= 1;  p2 += a0;
        BigUint q2 = (b2 << 1);  q2 += b1;  q2 <<= 1;  q2 += b0;
        BigUint v0   = a0.mul_toom3(b0);
        a0.m_data.clear();  b0.m_data.clear();
        BigUint vinf = a2.mul_toom3(b2);
        a2.m_data.clear();  b2.m_data.clear();
        a1.m_data.clear();  b1.m_data.clear();
        BigUint v1 = p1.mul_toom3(q1);
        p1.m_data.clear();  q1.m_data.clear();
        BigUint vm = pm1.mul_toom3(qm1);   
        pm1.m_data.clear();  qm1.m_data.clear();
        BigUint v2 = p2.mul_toom3(q2);
        p2.m_data.clear();  q2.m_data.clear();
        BigUint c2 = neg_vm ? (v1 - vm) : (v1 + vm);
        c2 >>= 1;
        c2 -= v0;
        c2 -= vinf;
        BigUint c3_pos = v2 + v0.mul_small(3);
        v2.m_data.clear();
        BigUint c3_neg = v1.mul_small(3) + vinf.mul_small(12);
        if (neg_vm) c3_pos += vm;
        else        c3_neg += vm;
        BigUint c3 = (c3_pos - c3_neg).div_small(6);
        c3_pos.m_data.clear();
        c3_neg.m_data.clear();
        BigUint c1 = neg_vm ? (v1 + vm) : (v1 - vm);
        v1.m_data.clear();
        vm.m_data.clear();
        c1 >>= 1;
        c1 -= c3;
        BigUint result;
        result.m_data.assign(m_data.size() + other.m_data.size() + 1, 0);
        add_limbs_at(result.m_data, v0.m_data,   0);
        add_limbs_at(result.m_data, c1.m_data,   k);
        add_limbs_at(result.m_data, c2.m_data,   2 * k);
        add_limbs_at(result.m_data, c3.m_data,   3 * k);
        add_limbs_at(result.m_data, vinf.m_data, 4 * k);
        result.normalize();
        return result;
    }

    BigUint mul_fft(const BigUint& other) const {
        if (is_zero() || other.is_zero()) return BigUint::zero();
        if (is_one())  return other;
        if (other.is_one()) return *this;
        std::vector<std::uint64_t> ca = to_chunks(m_data);
        std::vector<std::uint64_t> cb = to_chunks(other.m_data);
        const std::size_t conv_len = ca.size() + cb.size();
        std::size_t n = 1;
        int log_n = 0;
        while (n < conv_len) { n <<= 1; ++log_n; }

        if (log_n > MAX_NTT_LOG) {
            // Too large for chosen primes
            return mul_toom3(other);
        }
    
        ca.resize(n, 0);
        cb.resize(n, 0);
        std::vector<std::uint64_t> r1(ca), t1(cb);
        convolve_mod(r1, t1, NTT_P1, NTT_G1);       
        std::vector<std::uint64_t> r2(ca), t2(cb);
        convolve_mod(r2, t2, NTT_P2, NTT_G2);          
        convolve_mod(ca, cb, NTT_P3, NTT_G3);           
        std::vector<std::uint64_t>& r3 = ca;            
        const std::uint64_t P1P2       = NTT_P1 * NTT_P2;        
        const std::uint64_t p1_inv_p2  = mod_pow(NTT_P1, NTT_P2 - 2, NTT_P2);
        const std::uint64_t p1p2_mod3  = P1P2 % NTT_P3;
        const std::uint64_t p1p2_inv3  = mod_pow(p1p2_mod3, NTT_P3 - 2, NTT_P3);
        std::vector<std::uint64_t> out;
        out.reserve(n + 8);
        uint128 carry(0);

        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t v1 = r1[i], v2 = r2[i], v3 = r3[i];
            std::uint64_t t = mod_mul(mod_sub(v2, v1 % NTT_P2, NTT_P2), p1_inv_p2, NTT_P2);
            std::uint64_t x12 = v1 + NTT_P1 * t;
            std::uint64_t t2 = mod_mul(mod_sub(v3, x12 % NTT_P3, NTT_P3), p1p2_inv3, NTT_P3);
            uint128 val = uint128(x12) + uint128(P1P2) * uint128(t2) + carry;
            out.push_back(val.get_low_bits() & CHUNK_MASK);
            carry = val >> CHUNK_BITS;
        }

        while (!carry.is_zero()) {
            out.push_back(carry.get_low_bits() & CHUNK_MASK);
            carry = carry >> CHUNK_BITS;
        }

        r1.clear();  r2.clear();  r3.clear();
        BigUint result;
        const std::size_t num_limbs = (out.size() + CHUNKS_PER_LIMB - 1) / CHUNKS_PER_LIMB;
        result.m_data.assign(num_limbs, 0);

        for (std::size_t i = 0; i < out.size(); ++i) {
            const std::size_t limb_idx = i / CHUNKS_PER_LIMB;
            const std::size_t shift    = (i % CHUNKS_PER_LIMB) * CHUNK_BITS;
            result.m_data[limb_idx] |= out[i] << shift;
        }

        result.normalize();
        return result;
    }

public: 
    static DivmodResult divmod(const BigUint& a, const BigUint& b);
    BigUint& operator/=(const BigUint& b);
    BigUint& operator%=(const BigUint& b);
    BigUint operator/(std::uint64_t d) const { return div_small(d); } 
    BigUint& operator/=(std::uint64_t d) { *this = div_small(d); return *this; }
    BigUint operator%(std::uint64_t d) const { return mod_small(d); } 
    BigUint& operator%=(std::uint64_t d) { *this = mod_small(d); return *this; }

public:
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint operator<<(T shift) const {
        if (shift <= 0 || is_zero()) return *this;
        using U = typename std::make_unsigned<T>::type;
        U s = static_cast<U>(shift);
        const std::size_t limb_shift = s / 64;
        const std::size_t bit_shift  = s % 64;
        BigUint result;
        result.m_data.assign(m_data.size() + limb_shift + 1, 0);

        if (bit_shift == 0) {
            for (std::size_t i = 0; i < m_data.size(); ++i) {
                result.m_data[i + limb_shift] = m_data[i];
            }
        } else {
            const std::size_t rshift = 64 - bit_shift;
            std::uint64_t carry = 0;

            for (std::size_t i = 0; i < m_data.size(); ++i) {
                std::uint64_t v = m_data[i];
                result.m_data[i + limb_shift] |= (v << bit_shift);
                result.m_data[i + limb_shift] |= carry;
                carry = (v >> rshift);
            }

            result.m_data[m_data.size() + limb_shift] = carry;
        }

        result.normalize();
        return result;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint operator>>(T shift) const {
        if (shift <= 0 || is_zero()) return *this;
        using U = typename std::make_unsigned<T>::type;
        U s = static_cast<U>(shift);
        const std::size_t limb_shift = s / 64;
        const std::size_t bit_shift  = s % 64;
        if (limb_shift >= m_data.size()) { return BigUint::zero(); }
        BigUint result;
        result.m_data.assign(m_data.size() - limb_shift, 0);

        if (bit_shift == 0) {
            for (std::size_t i = limb_shift; i < m_data.size(); ++i) {
                result.m_data[i - limb_shift] = m_data[i];
            }
        } else {
            const std::size_t lshift = 64 - bit_shift;
            std::uint64_t carry = 0;

            for (std::size_t i = m_data.size(); i-- > limb_shift; ) {
                std::uint64_t v = m_data[i];
                result.m_data[i - limb_shift] = (v >> bit_shift) | carry;
                carry = (v << lshift);
            }
        }

        result.normalize();
        return result;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint& operator<<=(T shift) {
        if (shift <= 0 || is_zero()) return *this;
        using U = typename std::make_unsigned<T>::type;
        U s = static_cast<U>(shift);
        const std::size_t limb_shift = s / 64;
        const std::size_t bit_shift  = s % 64;
        const std::size_t old_size   = m_data.size();
        m_data.resize(old_size + limb_shift + 1, 0);

        if (bit_shift == 0) {
            for (std::size_t i = old_size; i-- > 0;) m_data[i + limb_shift] = m_data[i];
        } else {
            const std::size_t rshift = 64 - bit_shift;
            m_data[old_size + limb_shift] = m_data[old_size - 1] >> rshift;
            for (std::size_t i = old_size - 1; i > 0; --i) m_data[i + limb_shift] = (m_data[i] << bit_shift) | (m_data[i - 1] >> rshift);
            m_data[limb_shift] = m_data[0] << bit_shift;
        }

        for (std::size_t i = 0; i < limb_shift; ++i) m_data[i] = 0;
        normalize();
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigUint& operator>>=(T shift) {
        if (shift <= 0 || is_zero()) return *this;
        using U = typename std::make_unsigned<T>::type;
        U s = static_cast<U>(shift);
        const std::size_t limb_shift = s / 64;
        const std::size_t bit_shift  = s % 64;
        if (limb_shift >= m_data.size()) { m_data.clear(); return *this; }

        if (bit_shift == 0) {
            for (std::size_t i = 0; i + limb_shift < m_data.size(); ++i) m_data[i] = m_data[i + limb_shift];
        } else {
            const std::size_t lshift = 64 - bit_shift;

            for (std::size_t i = limb_shift; i < m_data.size(); ++i) {
                m_data[i - limb_shift] = m_data[i] >> bit_shift;
                if (i + 1 < m_data.size()) m_data[i - limb_shift] |= m_data[i + 1] << lshift;
            }
        }

        m_data.resize(m_data.size() - limb_shift);
        normalize();
        return *this;
    }

public:
    friend BigUint operator&(const BigUint& a, const BigUint& b) {
        const std::size_t n = std::min(a.m_data.size(), b.m_data.size());
        BigUint result;
        result.m_data.resize(n);
        for (std::size_t i = 0; i < n; ++i) { result.m_data[i] = a.m_data[i] & b.m_data[i]; }
        result.normalize();
        return result;
    }

    friend BigUint operator|(const BigUint& a, const BigUint& b) {
        const std::size_t n = std::max(a.m_data.size(), b.m_data.size());
        BigUint result;
        result.m_data.resize(n);
        for (std::size_t i = 0; i < n; ++i) { result.m_data[i] = a.limb(i) | b.limb(i); }
        result.normalize();
        return result;
    }

    friend BigUint operator^(const BigUint& a, const BigUint& b) {
        const std::size_t n = std::max(a.m_data.size(), b.m_data.size());
        BigUint result;
        result.m_data.resize(n);
        for (std::size_t i = 0; i < n; ++i) { result.m_data[i] = a.limb(i) ^ b.limb(i); }
        result.normalize();
        return result;
    }

    BigUint& operator&=(const BigUint& other) {
        m_data.resize(std::min(m_data.size(), other.m_data.size()));
        for (std::size_t i = 0; i < m_data.size(); ++i) { m_data[i] &= other.m_data[i]; }
        normalize();
        return *this;
    }

    BigUint& operator|=(const BigUint& other) {
        if (other.m_data.size() > m_data.size()) { m_data.resize(other.m_data.size(), 0); }
        for (std::size_t i = 0; i < other.m_data.size(); ++i) { m_data[i] |= other.m_data[i]; }
        return *this;
    }

    BigUint& operator^=(const BigUint& other) {
        if (other.m_data.size() > m_data.size()) { m_data.resize(other.m_data.size(), 0); }
        for (std::size_t i = 0; i < other.m_data.size(); ++i) { m_data[i] ^= other.m_data[i]; }
        normalize();
        return *this;
    }

public:
    std::string to_binary_string() const {
        if (is_zero()) return "0";
        std::string result;
        result.reserve(bit_length());
        bool leading = true;

        for (std::size_t i = m_data.size(); i-- > 0; ) {
            for (int b = 63; b >= 0; --b) {
                bool bit = (m_data[i] >> b) & 1ULL;
                if (leading && !bit) continue;
                leading = false;
                result += (bit ? '1' : '0');
            }
        }

        return result;
    }

    std::string to_hex_string() const {
        if (is_zero()) return "0";
        static constexpr char digits[] = "0123456789abcdef";
        std::string result;
        result.reserve(m_data.size() * 16);
        bool leading = true;

        for (std::size_t i = m_data.size(); i-- > 0; ) {
            for (int n = 60; n >= 0; n -= 4) {
                std::uint8_t nibble = (m_data[i] >> n) & 0xF;
                if (leading && nibble == 0) continue;
                leading = false;
                result += digits[nibble];
            }
        }

        return result;
    }

    std::string to_hex_string(std::size_t n) const {
        if (is_zero()) { return "0"; }
        std::string hex = to_hex_string();
        std::size_t len = hex.size();
        std::stringstream ss;

        if (len <= n) {
            ss << "0x" << hex;
        } else {
            ss << "0x" << hex.substr(0, n) << "...";
        }

        ss << " (" << bit_length() << " bits)";
        return ss.str();
    }

    friend std::ostream& operator<<(std::ostream& os, const BigUint& x) {
        os << x.to_scientific_string(20);
        return os;
    }

    std::string to_string(bool include_seperators = true, char sep = ',', std::size_t group = 3) const {
        if (is_zero()) return "0";
        BigUint tmp = *this;
        std::vector<std::string> chunks;
        static constexpr std::uint64_t BASE = 1000000000000000000ULL; // 10^18

        while (!tmp.is_zero()) {
            std::uint64_t rem = tmp.mod_small(BASE);
            tmp = tmp.div_small(BASE);

            if (!tmp.is_zero())
                chunks.push_back(fmt_chunk(rem, 18));
            else
                chunks.push_back(std::to_string(rem));
        }

        std::string result;
        for (std::size_t i = chunks.size(); i-- > 0; ) { result += chunks[i]; }

        if (sep != '\0' && group > 0 && include_seperators) {
            std::string with_sep;
            with_sep.reserve(result.size() + result.size() / group);
            std::size_t count = 0;

            for (std::size_t i = result.size(); i-- > 0; ) {
                with_sep.push_back(result[i]);
                if (++count == group && i != 0) {
                    with_sep.push_back(sep);
                    count = 0;
                }
            }

            std::reverse(with_sep.begin(), with_sep.end());
            return with_sep;
        }

        return result;
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        if (is_zero()) return "0";
        std::string full = to_string(false, '\0');
        std::size_t exponent = full.size() - 1;
        std::string sig;
        sig.reserve(sig_figs);

        for (std::size_t i = 0; i < sig_figs; ++i) {
            if (i < full.size())
                sig.push_back(full[i]);
            else
                sig.push_back('0'); 
        }

        if (sig_figs > 1) { sig.insert(sig.begin() + 1, '.'); }
        return sig + "e" + std::to_string(exponent);
    }

private:
    static std::string fmt_chunk(std::uint64_t v, int width) {
        std::string s = std::to_string(v);
        if (s.size() < static_cast<std::size_t>(width)) return std::string(width - s.size(), '0') + s;
        return s;
    }


private:
    static constexpr std::uint64_t NTT_P1 = 998244353;
    static constexpr std::uint64_t NTT_G1 = 3;
    static constexpr int           NTT_K1 = 23;

    static constexpr std::uint64_t NTT_P2 = 2013265921;
    static constexpr std::uint64_t NTT_G2 = 31;
    static constexpr int           NTT_K2 = 27;

    static constexpr std::uint64_t NTT_P3 = 754974721;
    static constexpr std::uint64_t NTT_G3 = 11;
    static constexpr int           NTT_K3 = 24;

    static constexpr int    CHUNK_BITS     = 16;
    static constexpr std::uint64_t CHUNK_MASK = (1ULL << CHUNK_BITS) - 1;
    static constexpr int    CHUNKS_PER_LIMB = 64 / CHUNK_BITS;  
    static constexpr int    MAX_NTT_LOG     = NTT_K1;            

    static std::uint64_t mod_mul(std::uint64_t a, std::uint64_t b, std::uint64_t p) {
        return a * b % p;           
    }

    static std::uint64_t mod_add(std::uint64_t a, std::uint64_t b, std::uint64_t p) {
        std::uint64_t s = a + b;
        return s >= p ? s - p : s;
    }

    static std::uint64_t mod_sub(std::uint64_t a, std::uint64_t b, std::uint64_t p) {
        return a >= b ? a - b : a + p - b;
    }

    static std::uint64_t mod_pow(std::uint64_t base, std::uint64_t exp, std::uint64_t p) {
        std::uint64_t r = 1;
        base %= p;

        while (exp) {
            if (exp & 1) r = mod_mul(r, base, p);
            base = mod_mul(base, base, p);
            exp >>= 1;
        }

        return r;
    }

    static void ntt(std::vector<std::uint64_t>& a, bool inverse, std::uint64_t p, std::uint64_t g) {
        const std::size_t n = a.size();  

        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }

        for (std::size_t len = 2; len <= n; len <<= 1) {
            std::uint64_t w = inverse ? mod_pow(g, p - 1 - (p - 1) / len, p) : mod_pow(g, (p - 1) / len, p);           

            for (std::size_t i = 0; i < n; i += len) {
                std::uint64_t wn = 1;
                const std::size_t half = len >> 1;

                for (std::size_t j = 0; j < half; ++j) {
                    std::uint64_t u = a[i + j];
                    std::uint64_t v = mod_mul(a[i + j + half], wn, p);
                    a[i + j]        = mod_add(u, v, p);
                    a[i + j + half] = mod_sub(u, v, p);
                    wn = mod_mul(wn, w, p);
                }
            }
        }

        if (inverse) {
            std::uint64_t n_inv = mod_pow(n, p - 2, p);
            for (auto& x : a) x = mod_mul(x, n_inv, p);
        }
    }

    static void pointwise_mul(std::vector<std::uint64_t>& a, std::vector<std::uint64_t>& b, std::uint64_t p) {
        for (std::size_t i = 0; i < a.size(); ++i) a[i] = mod_mul(a[i], b[i], p);
        b.clear();
        b.shrink_to_fit();
    }

    static void convolve_mod(std::vector<std::uint64_t>& fa, std::vector<std::uint64_t>& fb, std::uint64_t p, std::uint64_t g) {
        for (auto& x : fa) x %= p;
        for (auto& x : fb) x %= p;
        ntt(fa, false, p, g);
        ntt(fb, false, p, g);
        pointwise_mul(fa, fb, p);      
        ntt(fa, true,  p, g);
    }

    static std::vector<std::uint64_t> to_chunks(const std::vector<std::uint64_t>& limbs) {
        std::vector<std::uint64_t> c;
        c.reserve(limbs.size() * CHUNKS_PER_LIMB);

        for (std::uint64_t w : limbs) {
            for (int j = 0; j < CHUNKS_PER_LIMB; ++j) {
                c.push_back(w & CHUNK_MASK);
                w >>= CHUNK_BITS;
            }
        }

        while (!c.empty() && c.back() == 0) c.pop_back();
        return c;
    }
};

struct DivmodResult {
    BigUint quotient;
    BigUint remainder;
    bool is_valid = true;
};

DivmodResult BigUint::divmod(const BigUint& a, const BigUint& b) {
    if (b.is_zero()) { return {BigUint::zero(), a, false}; }
    if (a.is_zero()) { return {BigUint::zero(), BigUint::zero()}; }
    if (a < b)       { return {BigUint::zero(), a}; }
    if (a == b)      { return {BigUint::one(),  BigUint::zero()}; }

    if (b.limb_count() == 1) {
        BigUint q = a.div_small(b.m_data[0]);
        std::uint64_t r = a.mod_small(b.m_data[0]);
        return {q, r == 0 ? BigUint::zero() : BigUint(r)};
    }

    const std::size_t n = b.limb_count();   
    const std::size_t m = a.limb_count() - n; 
    const unsigned int shift = clz64(b.m_data[n - 1]);
    BigUint u = a << shift;
    BigUint v = b << shift;
    u.m_data.resize(std::max(u.m_data.size(), m + n + 1), 0);
    BigUint q;
    q.m_data.resize(m + 1, 0);

    for (std::size_t j = m + 1; j-- > 0; ) {
        uint128 hi_two = (uint128(u.m_data[j + n]) << 64) +  uint128(u.m_data[j + n - 1]);
        uint128 q_hat = hi_two / uint128(v.m_data[n - 1]);
        uint128 r_hat = hi_two % uint128(v.m_data[n - 1]);

        for (;;) {
            if (q_hat.get_high_bits() != 0) {
                q_hat = q_hat - uint128(1);
                r_hat = r_hat + uint128(v.m_data[n - 1]);
                if (r_hat.get_high_bits() != 0) break;
                continue;
            }
            
            uint128 lhs = q_hat * uint128(v.m_data[n - 2]);
            uint128 rhs = (r_hat << 64) + uint128(u.m_data[j + n - 2]);

            if (lhs > rhs) {
                q_hat = q_hat - uint128(1);
                r_hat = r_hat + uint128(v.m_data[n - 1]);
                if (r_hat.get_high_bits() != 0) break;
            } else {
                break;
            }
        }

        const std::uint64_t qh = q_hat.get_low_bits();
        std::vector<std::uint64_t> prod(n + 1, 0);

        {
            uint128 carry(0);

            for (std::size_t i = 0; i < n; ++i) {
                uint128 p = uint128(qh) * uint128(v.m_data[i]) + carry;
                prod[i] = p.get_low_bits();
                carry = p.get_high_bits();
            }

            prod[n] = carry.get_low_bits();
        }

        std::uint64_t borrow = 0;

        for (std::size_t i = 0; i <= n; ++i) {
            uint128 sub = uint128(prod[i]) + uint128(borrow);

            if (uint128(u.m_data[j + i]) >= sub) {
                u.m_data[j + i] = (uint128(u.m_data[j + i]) - sub).get_low_bits();
                borrow = 0;
            } else {
                uint128 base = uint128(1) << 64;
                u.m_data[j + i] = (base + uint128(u.m_data[j + i]) - sub).get_low_bits();
                borrow = 1;
            }
        }

        q.m_data[j] = qh;

        if (borrow) {
            q.m_data[j]--;
            uint128 c(0);

            for (std::size_t i = 0; i < n; ++i) {
                uint128 sum = uint128(u.m_data[j + i]) + uint128(v.m_data[i]) + c;
                u.m_data[j + i] = sum.get_low_bits();
                c = sum.get_high_bits();
            }

            u.m_data[j + n] += c.get_low_bits();
        }
    }

    q.normalize();
    BigUint rem;
    rem.m_data.assign(u.m_data.begin(), u.m_data.begin() + static_cast<std::ptrdiff_t>(n));
    if (shift > 0) rem = rem >> shift;
    rem.normalize();
    return {q, rem};
}

BigUint operator/(const BigUint& a, const BigUint& b) { return BigUint::divmod(a, b).quotient; }
BigUint operator%(const BigUint& a, const BigUint& b) { return BigUint::divmod(a, b).remainder; }

BigUint& BigUint::operator/=(const BigUint& b) {
    auto dr = divmod(*this, b);
    m_data = std::move(dr.quotient.m_data);
    return *this;
}

BigUint& BigUint::operator%=(const BigUint& b) {
    auto dr = divmod(*this, b);
    m_data = std::move(dr.remainder.m_data);
    return *this;
}

BigUint::BigUint(const std::string& s) {
    *this = BigUint::zero();
    if (s.empty()) return;
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && std::isspace(s[start])) ++start;
    while (end > start && std::isspace(s[end - 1])) --end;
    if (start >= end) return;
    std::string_view v(s.c_str() + start, end - start);
    std::size_t epos = v.find_first_of("eE");
    std::string_view mant = (epos == std::string_view::npos) ? v : v.substr(0, epos);
    std::string_view expo = (epos == std::string_view::npos) ? std::string_view() : v.substr(epos + 1);
    long long exponent = 0;

    if (!expo.empty()) {
        bool neg = false;
        std::size_t i = 0;

        if (expo[0] == '+' || expo[0] == '-') {
            neg = (expo[0] == '-');
            i = 1;
        }

        for (; i < expo.size(); ++i) {
            if (!std::isdigit(expo[i])) return; 
            exponent = exponent * 10 + (expo[i] - '0');
        }

        if (neg) exponent = -exponent;
    }

    std::string digits;
    digits.reserve(mant.size());
    long long digits_after_decimal = 0;
    bool seen_decimal = false;

    for (char c : mant) {
        if (c == '.') {
            if (seen_decimal) return;
            seen_decimal = true;
            continue;
        }

        if (!std::isdigit(c)) return; 
        digits.push_back(c);
        if (seen_decimal) digits_after_decimal++;
    }

    if (digits.empty()) return;
    BigUint value(0);
    
    for (char c : digits) { 
        value *= 10ULL;
        value += c - '0'; 
    }
    
    long long effective_exp = exponent - digits_after_decimal;

    if (effective_exp > 0) {
        BigUint pow10(1);
        BigUint ten(10);
        for (long long i = 0; i < effective_exp; ++i) { pow10 *= ten; }
        value *= pow10;
    } else if (effective_exp < 0) {
        BigUint pow10(1);
        BigUint ten(10);
        long long k = -effective_exp;
        for (long long i = 0; i < k; ++i) { pow10 *= ten; }
        auto dr = BigUint::divmod(value, pow10);

        if (!dr.remainder.is_zero()) {
            // truncate. No throw
        }
        
        value = dr.quotient;
    }

    *this = value;
}

} // namespace multiprecision
} // namespace fizmo

#endif // MULTIPRECISION_BIG_UINT_HPP