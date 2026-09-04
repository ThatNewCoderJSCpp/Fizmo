#ifndef FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_CLASS_HPP
#define FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_CLASS_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <limits>

#include "../../Arrays/small_vector.hpp"
#include "../Fixed Width int/type_traits.hpp"
#include "../limb_kernels.hpp"

namespace fizmo {

template <std::size_t N, std::size_t B>
struct integer_log : std::integral_constant<std::size_t, (N < B ? 0 : 1 + integer_log<N / B, B>::value)> {};

template <std::size_t N>
struct integer_log2 : std::integral_constant<std::size_t, (N < 2 ? 0 : 1 + integer_log2<N / 2>::value)> {};

template <std::size_t N>
struct integer_log10 : std::integral_constant<std::size_t, (N < 2 ? 0 : 1 + integer_log10<N / 10>::value)> {};

namespace multiprecision {

namespace budetail {

using limb_vec = SmallVector<std::uint64_t, 4>;
constexpr std::uint64_t mask32 = 0xFFFFFFFFull;

template <class T>
typename std::enable_if<std::is_signed<T>::value, bool>::type
fits_limb(T v) noexcept { return v >= T(0); }

template <class T>
typename std::enable_if<!std::is_signed<T>::value, bool>::type
fits_limb(T) noexcept { return true; }

template <class T>
std::uint64_t to_limb(T v) noexcept {
    return static_cast<std::uint64_t>(v);
}

template <std::size_t Bits>
struct limb_count : std::integral_constant<std::size_t, (Bits + 63) / 64> {};

template <std::size_t Bits>
std::uint64_t limb_at(const umag<Bits>& m, std::size_t i) noexcept {
    return (i == 0) ? m.to_u64() : (m >> (i * 64)).to_u64();
}

inline void append_limbs(limb_vec& out, std::uint64_t v);

template <std::size_t Bits>
typename std::enable_if<(Bits <= 64)>::type append_limbs(limb_vec& out, const umag<Bits>& m);

template <std::size_t Bits>
typename std::enable_if<(Bits >= 128)>::type append_limbs(limb_vec& out, const umag<Bits>& m);

inline void append_limbs(limb_vec& out, std::uint64_t v) { out.push_back(v); }

template <std::size_t Bits>
typename std::enable_if<(Bits <= 64)>::type
append_limbs(limb_vec& out, const umag<Bits>& m) { out.push_back(m.to_u64()); }

template <std::size_t Bits>
typename std::enable_if<(Bits >= 128)>::type
append_limbs(limb_vec& out, const umag<Bits>& m) {
    append_limbs(out, m.get_low_bits());
    append_limbs(out, m.get_high_bits());
}

inline void mul_wide(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) noexcept {
    mdetail::mul_wide64(a, b, hi, lo);
}

inline std::uint64_t div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& rem) noexcept {
    if (d <= mask32) {
        std::uint64_t cur = (hi << 32) | (lo >> 32);
        const std::uint64_t qh = cur / d; cur %= d;
        cur = (cur << 32) | (lo & mask32);
        const std::uint64_t ql = cur / d; rem = cur % d;
        return (qh << 32) | ql;
    }

    const unsigned s = static_cast<unsigned>(clz64(d));
    d <<= s;
    if (s != 0) { hi = (hi << s) | (lo >> (64 - s)); lo <<= s; }
    const std::uint64_t dh = d >> 32, dl = d & mask32;
    const std::uint64_t un1 = lo >> 32, un0 = lo & mask32;
    std::uint64_t q1 = hi / dh;
    std::uint64_t rh = hi - q1 * dh;

    while (q1 > mask32 || q1 * dl > ((rh << 32) | un1)) {
        --q1; rh += dh;
        if (rh > mask32) break;
    }

    const std::uint64_t u21 = ((hi << 32) | un1) - q1 * d;
    std::uint64_t q0 = u21 / dh;
    rh = u21 - q0 * dh;

    while (q0 > mask32 || q0 * dl > ((rh << 32) | un0)) {
        --q0; rh += dh;
        if (rh > mask32) break;
    }

    rem = (((u21 << 32) | un0) - q0 * d) >> s;
    return (q1 << 32) | q0;
}

template <class T>
typename std::enable_if<std::is_signed<T>::value, std::uint64_t>::type
limb_abs(T v, bool& neg) noexcept {
    neg = v < T(0);
    return neg ? (static_cast<std::uint64_t>(-(v + T(1))) + std::uint64_t(1)) : static_cast<std::uint64_t>(v);
}

template <class T>
typename std::enable_if<!std::is_signed<T>::value, std::uint64_t>::type
limb_abs(T v, bool& neg) noexcept { neg = false; return static_cast<std::uint64_t>(v); }

struct small_arg { std::uint64_t mag; bool neg; };

template <class T>
small_arg as_small(T v) noexcept { small_arg s; s.mag = limb_abs(v, s.neg); return s; }


} // namespace budetail

class BigUInt {
private:
    using limb_store = budetail::limb_vec;
    limb_store m_data;

private:
    template <std::size_t Bits>
    struct fits_inline : std::integral_constant<bool, (budetail::limb_count<Bits>::value <= limb_store::inline_capacity)> {};

    void trim() noexcept {
        std::size_t n = m_data.size();
        while (n > 1 && m_data[n - 1] == 0) --n;
        m_data.set_size_unchecked(n);
    }

    void set_undefined() noexcept { m_data.clear(); }
    void set_zero() { m_data.clear(); m_data.push_back(std::uint64_t(0)); }

    template <std::size_t Bits>
    void assign_mag(const umag<Bits>& mag) {
        m_data.clear();
        m_data.reserve(budetail::limb_count<Bits>::value);
        budetail::append_limbs(m_data, mag);
        trim();
    }

    template <std::size_t Bits, sign S>
    void assign_int(const integer<Bits, S>& src) noexcept(fits_inline<Bits>::value) {
        if (src.is_negative() || src.is_undefined()) { m_data.clear(); return; }
        assign_mag(src.magnitude());
    }

    void ensure_limbs(std::size_t n) { if (m_data.size() < n) m_data.resize(n); }
    void assign_limb(std::uint64_t v) { m_data.clear(); m_data.push_back(v); }

    void set_limb_unchecked(std::uint64_t v) noexcept {
        m_data.set_size_unchecked(1);
        m_data[0] = v;
    }

    // False: *this is undefined
    bool grow_to(std::size_t n) {
        if (n > max_limbs) { set_undefined(); return false; }
        if (m_data.size() < n) m_data.resize(n);
        return true;
    }

    // False: *this is undefined
    bool push_limb(std::uint64_t v) {
        if (m_data.size() >= max_limbs) { set_undefined(); return false; }
        m_data.push_back(v);
        return true;
    }

public:
    static constexpr long long no_bit = -1;
    static constexpr std::uint64_t no_remainder = ~std::uint64_t(0);
    static constexpr std::size_t max_limbs = std::size_t(1) << 24;
    static constexpr std::size_t max_bits  = max_limbs * 64;
    static constexpr std::size_t log2_max = max_bits - 1;
    static constexpr long double log10_max = max_bits * 0.30102999566398119521373889472449L;
    static constexpr std::size_t integer_log10_max = static_cast<std::size_t>(log10_max);
    static constexpr std::size_t max_digits = integer_log10_max + 1;
    static constexpr std::uint32_t max_factorial = 3318996;

public:
    BigUInt() : m_data() { m_data.push_back(std::uint64_t(0)); }
    BigUInt(const BigUInt& o) : m_data(o.m_data) {}
    BigUInt(BigUInt&& o) noexcept(std::is_nothrow_move_constructible<limb_store>::value) : m_data(std::move(o.m_data)) {}

    BigUInt(const limb_store& v) {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return; }
        m_data = v;
        trim();
    }

    BigUInt(limb_store&& v) noexcept {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return; }
        m_data = std::move(v);
        trim();
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value>::type>
    BigUInt(T v) noexcept : m_data() {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        assign_limb(budetail::to_limb(v));
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type, typename = void>
    explicit BigUInt(T v) noexcept : m_data() {
        set_undefined();
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (v <= T(0)) return;
        assign_limb(budetail::to_limb(v));
    }

    template <std::size_t Bits, sign S, typename = typename std::enable_if<S == sign::is_unsigned>::type>
    BigUInt(const integer<Bits, S>& src) : m_data() { assign_mag(src.magnitude()); }

    template <std::size_t Bits, sign S, typename = typename std::enable_if<S == sign::is_signed>::type, typename = void>
    explicit BigUInt(const integer<Bits, S>& src) : m_data() {
        set_undefined();
        if (src.is_undefined() || src.is_negative()) return;
        assign_mag(src.magnitude());
    }

    BigUInt& operator=(const BigUInt& o) {
        if (this != &o) m_data = o.m_data;
        return *this;
    }

    BigUInt& operator=(BigUInt&& o) noexcept(std::is_nothrow_move_assignable<limb_store>::value) {
        if (this != &o) m_data = std::move(o.m_data);
        return *this;
    }

    BigUInt& operator=(const limb_store& v) {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return *this; }
        m_data = v;
        trim();
        return *this;
    }

    BigUInt& operator=(limb_store&& v) noexcept {
        if (v.empty() || v.size() > max_limbs) { set_undefined(); return *this; }
        m_data = std::move(v);
        trim();
        return *this;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && std::is_unsigned_v<T>>::type>
    BigUInt& operator=(T v) noexcept {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        assign_limb(budetail::to_limb(v));
        return *this;
    }

    template <std::size_t Bits, sign S, typename = typename std::enable_if<S == sign::is_unsigned>::type>
    BigUInt& operator=(const integer<Bits, S>& src) noexcept(fits_inline<Bits>::value) {
        assign_mag(src.magnitude());
        return *this;
    }

public:
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    assign(T v) noexcept {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (!budetail::fits_limb(v)) { set_undefined(); return *this; }
        assign_limb(budetail::to_limb(v));
        return *this;
    }

    template <std::size_t Bits, sign S>
    BigUInt& assign(const integer<Bits, S>& src) noexcept(fits_inline<Bits>::value) {
        assign_int(src);
        return *this;
    }

    static BigUInt from_limbs(const std::uint64_t* p, std::size_t n) {
        BigUInt r;
        if (n == 0 || n > max_limbs) { r.set_undefined(); return r; }
        if (!r.m_data.resize_uninit(n)) { r.set_undefined(); return r; }
        for (std::size_t i = 0; i < n; ++i) r.m_data[i] = p[i];
        r.trim();
        return r;
    }

public:
    bool        is_zero()      const noexcept { return m_data.size() == 1 && m_data[0] == 0; }
    bool        is_undefined() const noexcept { return m_data.empty(); }
    std::size_t limb_count()   const noexcept { return m_data.size(); }

    const limb_store& data() const noexcept { return m_data; }
    const std::uint64_t* limbs() const noexcept { return m_data.data(); }
    std::uint64_t limb(std::size_t i) const noexcept { return i < m_data.size() ? m_data[i] : std::uint64_t(0); }

public:
    static BigUInt zero() { return BigUInt(); }
    static BigUInt one() { return BigUInt(std::uint64_t(1)); }

    static BigUInt undefined() {
        BigUInt r;
        r.set_undefined();
        return r;
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static BigUInt integer_type_max() {
        return BigUInt(std::numeric_limits<T>::max());
    }

    template <typename T, typename = typename std::enable_if<is_fizmo_static_int_v<T>>::type, typename = void>
    static BigUInt integer_type_max() {
        return BigUInt(T::max());
    }

    static BigUInt max() {
        BigUInt r;

        if (!r.m_data.resize_uninit(max_limbs)) {
            r.set_undefined();
            return r;
        }

        for (std::size_t i = 0; i < max_limbs; ++i) r.m_data[i] = ~std::uint64_t(0); // fill with 0xFFFFFFFFFFFFFFFF
        return r;
    }

public:
    bool is_one()  const noexcept { return m_data.size() == 1 && m_data[0] == std::uint64_t(1); }
    bool is_even() const noexcept { return !m_data.empty() && (m_data[0] & std::uint64_t(1)) == 0; }
    bool is_odd()  const noexcept { return !m_data.empty() && (m_data[0] & std::uint64_t(1)) != 0; }

public:
    int compare(const BigUInt& o) const noexcept {
        const std::size_t a = m_data.size();
        const std::size_t b = o.m_data.size();
        if (a != b) return (a < b) ? -1 : 1;

        for (std::size_t i = a; i > 0; --i) {
            const std::uint64_t x = m_data[i - 1];
            const std::uint64_t y = o.m_data[i - 1];
            if (x != y) return (x < y) ? -1 : 1;
        }

        return 0;
    }

    bool equals(const BigUInt& o) const noexcept {
        if (m_data.size() != o.m_data.size()) return false;
        for (std::size_t i = 0; i < m_data.size(); ++i) { if (m_data[i] != o.m_data[i]) return false; }
        return true;
    }

public:
    std::size_t bit_length() const noexcept {
        if (m_data.empty()) return 0;
        const std::size_t top = m_data.size() - 1;
        return top * 64 + static_cast<std::size_t>(64 - clz64(m_data[top]));
    }

    bool get_bit(std::size_t i) const noexcept {
        const std::size_t w = i / 64;
        return w < m_data.size() && ((m_data[w] >> (i % 64)) & std::uint64_t(1)) != 0;
    }

    void set_bit(std::size_t i, bool b = true) {
        const std::size_t w = i / 64;

        if (!b) {
            if (w >= m_data.size()) return;
            m_data[w] &= ~(std::uint64_t(1) << (i % 64));
            if (w + 1 == m_data.size()) trim();
            return;
        }

        if (!grow_to(w + 1)) return;
        m_data[w] |= (std::uint64_t(1) << (i % 64));
    }

    void clear_bit(std::size_t i) noexcept {
        const std::size_t w = i / 64;
        if (w >= m_data.size()) return;
        m_data[w] &= ~(std::uint64_t(1) << (i % 64));
        if (w + 1 == m_data.size()) trim();
    }

    bool flip_bit(std::size_t i) {
        const std::size_t w = i / 64;
        const std::uint64_t mask = std::uint64_t(1) << (i % 64);
        if (w >= m_data.size()) { if (!grow_to(w + 1)) return false; m_data[w] = mask; return true; }
        m_data[w] ^= mask;
        const bool now = (m_data[w] & mask) != 0;
        if (!now && w + 1 == m_data.size()) trim();
        return now;
    }

public:
    BigUInt& shifted_limbs_mutable(std::size_t n) {
        if (n == 0 || m_data.empty() || is_zero()) return *this;
        const std::size_t old = m_data.size();
        if (!grow_to(old + n)) return *this;
        for (std::size_t i = old; i > 0; --i) m_data[i - 1 + n] = m_data[i - 1];
        for (std::size_t i = 0; i < n; ++i) m_data[i] = 0;
        return *this;
    }

    BigUInt shifted_limbs(std::size_t n) const & {
        if (n == 0 || m_data.empty() || is_zero()) return *this;
        if (m_data.size() + n > max_limbs) return undefined();
        BigUInt r;
        r.m_data.resize(m_data.size() + n);
        for (std::size_t i = 0; i < m_data.size(); ++i) r.m_data[i + n] = m_data[i];
        return r;
    }

    BigUInt shifted_limbs(std::size_t n) && {
        shifted_limbs_mutable(n);
        return std::move(*this);
    }

    BigUInt& add_shifted_mutable(const BigUInt& src, std::size_t offset) {
        if (m_data.empty() || src.m_data.empty()) { set_undefined(); return *this; }
        if (&src == this) { const BigUInt tmp(src); return add_shifted_mutable(tmp, offset); }
        if (src.is_zero()) return *this;
        const std::size_t ns = src.m_data.size();
        if (!grow_to(offset + ns)) return *this;
        std::uint64_t carry = 0;

        for (std::size_t i = 0; i < ns; ++i) {
            const std::size_t w = i + offset;
            const std::uint64_t a  = m_data[w];
            const std::uint64_t s  = a + src.m_data[i];
            const std::uint64_t c1 = (s < a) ? 1u : 0u;
            const std::uint64_t t  = s + carry;
            const std::uint64_t c2 = (t < s) ? 1u : 0u;
            m_data[w] = t;
            carry = c1 | c2;
        }

        for (std::size_t w = offset + ns; carry != 0; ++w) {
            if (w == m_data.size()) { push_limb(carry); break; }
            const std::uint64_t s = m_data[w] + carry;
            carry = (s < carry) ? 1u : 0u;
            m_data[w] = s;
        }

        return *this;
    }

    BigUInt concatenate(const BigUInt& low, std::size_t offset) const {
        BigUInt r(low);
        r.add_shifted_mutable(*this, offset);
        return r;
    }

    BigUInt concatenate(BigUInt&& low, std::size_t offset) const {
        low.add_shifted_mutable(*this, offset);
        return std::move(low);
    }

public:
    long long get_exponent_base2() const noexcept {
        if (m_data.empty() || is_zero()) return no_bit;
        return static_cast<long long>(bit_length()) - 1;
    }

    long long get_exponent_base10() const noexcept {
        if (m_data.empty() || is_zero()) return no_bit;

        if (m_data.size() == 1) {
            std::uint64_t v = m_data[0];
            long long e = 0;
            while (v >= 10) { v /= 10; ++e; }
            return e;
        }

        BigUInt t(*this);
        long long e = 0;
        constexpr std::uint64_t p19 = 10000000000000000000ull;

        while (t.limb_count() > 1) {
            t.div_small_mutable(p19);
            if (t.is_zero()) break;
            e += 19;
        }

        std::uint64_t v = t.limb(0);
        while (v >= 10) { v /= 10; ++e; }
        return e;
    }

    long long count_leading_zeros() const noexcept {
        if (m_data.empty()) return no_bit;
        return static_cast<long long>(m_data.size() * 64 - bit_length());
    }

    long long count_leading_zeros(std::size_t width) const noexcept {
        if (m_data.empty()) return no_bit;
        const std::size_t len = bit_length();
        if (len > width) return no_bit;
        return static_cast<long long>(width - len);
    }

    long long count_trailing_zeros() const noexcept {
        if (m_data.empty()) return no_bit;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t w = m_data[i];
            if (w != 0) return static_cast<long long>(i * 64) + ctz64(w);
        }

        return no_bit;
    }

public:
    BigUInt& add_small_mutable(std::uint64_t v) {
        if (m_data.empty() || v == 0) return *this;
        std::uint64_t carry = v;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n && carry != 0; ++i) {
            const std::uint64_t s = m_data[i] + carry;
            carry = (s < carry) ? 1u : 0u;
            m_data[i] = s;
        }

        if (carry != 0) push_limb(carry);
        return *this;
    }

    BigUInt& sub_small_mutable(std::uint64_t v) noexcept {
        if (m_data.empty() || v == 0) return *this;
        std::uint64_t borrow = v;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n && borrow != 0; ++i) {
            const std::uint64_t a = m_data[i];
            m_data[i] = a - borrow;
            borrow = (a < borrow) ? 1u : 0u;
        }

        if (borrow != 0) { set_undefined(); return *this; }
        trim();
        return *this;
    }

    BigUInt& mul_small_mutable(std::uint64_t v) {
        if (m_data.empty() || v == 1) return *this;
        if (v == 0) { set_zero(); return *this; }
        std::uint64_t carry = 0;
        const std::size_t n = m_data.size();

        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t hi, lo;
            budetail::mul_wide(m_data[i], v, hi, lo);
            const std::uint64_t s = lo + carry;
            carry = hi + ((s < carry) ? 1u : 0u);
            m_data[i] = s;
        }

        if (carry != 0) push_limb(carry);
        return *this;
    }

    std::uint64_t divmod_small_mutable(std::uint64_t d) noexcept {
        if (m_data.empty()) return no_remainder;
        if (d == 0) { set_undefined(); return no_remainder; }
        if (d == 1) return 0;
        std::uint64_t rem = 0;
        for (std::size_t i = m_data.size(); i > 0; --i) { m_data[i - 1] = budetail::div_wide(rem, m_data[i - 1], d, rem); }
        trim();
        return rem;
    }

    BigUInt& div_small_mutable(std::uint64_t d) noexcept {
        divmod_small_mutable(d);
        return *this;
    }

    BigUInt& mod_small_mutable(std::uint64_t d) noexcept {
        const std::uint64_t r = divmod_small_mutable(d);
        if (m_data.empty()) return *this;
        set_limb_unchecked(r);
        return *this;
    }

public:
    BigUInt add_small(std::uint64_t v) const &  { BigUInt r(*this); r.add_small_mutable(v); return r; }
    BigUInt add_small(std::uint64_t v)       && { add_small_mutable(v); return std::move(*this); }

    BigUInt sub_small(std::uint64_t v) const &  { BigUInt r(*this); r.sub_small_mutable(v); return r; }
    BigUInt sub_small(std::uint64_t v)       && { sub_small_mutable(v); return std::move(*this); }

    BigUInt mul_small(std::uint64_t v) const &  { BigUInt r(*this); r.mul_small_mutable(v); return r; }
    BigUInt mul_small(std::uint64_t v)       && { mul_small_mutable(v); return std::move(*this); }

    BigUInt div_small(std::uint64_t d) const &  { BigUInt r(*this); r.div_small_mutable(d); return r; }
    BigUInt div_small(std::uint64_t d)       && { div_small_mutable(d); return std::move(*this); }

    BigUInt divmod_small(std::uint64_t d, std::uint64_t& rem) const & {
        BigUInt q(*this);
        rem = q.divmod_small_mutable(d);
        return q;
    }

    BigUInt divmod_small(std::uint64_t d, std::uint64_t& rem) && {
        rem = divmod_small_mutable(d);
        return std::move(*this);
    }

    std::uint64_t mod_small(std::uint64_t d) const noexcept {
        if (m_data.empty() || d == 0) return no_remainder;
        if (d == 1) return 0;
        std::uint64_t rem = 0;
        for (std::size_t i = m_data.size(); i > 0; --i) budetail::div_wide(rem, m_data[i - 1], d, rem);
        return rem;
    }

public:
    BigUInt& add_mutable(const BigUInt& o) {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return shift_left_mutable(1);
        const std::size_t bn   = o.m_data.size();
        const std::size_t maxn = (m_data.size() > bn) ? m_data.size() : bn;
        if (!grow_to(maxn)) return *this;
        std::uint64_t carry = mdetail::add_n(m_data.data(), m_data.data(), o.m_data.data(), bn);
        if (bn < maxn) carry = mdetail::add_1(m_data.data() + bn, m_data.data() + bn, maxn - bn, carry);
        if (carry != 0) push_limb(carry);
        return *this;
    }

    BigUInt& sub_mutable(const BigUInt& o) noexcept {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_zero(); return *this; }
        if (compare(o) < 0) { set_undefined(); return *this; }
        const std::size_t an = m_data.size();
        const std::size_t bn = o.m_data.size();
        std::uint64_t borrow = mdetail::sub_n(m_data.data(), m_data.data(), o.m_data.data(), bn);
        if (bn < an) borrow = mdetail::sub_1(m_data.data() + bn, m_data.data() + bn, an - bn, borrow);
        (void)borrow;
        trim();
        return *this;
    }

    BigUInt add(const BigUInt& o) const &  { BigUInt r(*this); r.add_mutable(o); return r; }
    BigUInt add(const BigUInt& o)       && { add_mutable(o); return std::move(*this); }

    BigUInt subtract(const BigUInt& o) const &  { BigUInt r(*this); r.sub_mutable(o); return r; }
    BigUInt subtract(const BigUInt& o)       && { sub_mutable(o); return std::move(*this); }

public:
    template <class T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    operator+=(T v) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (m_data.empty()) return *this;
        const budetail::small_arg s = budetail::as_small(v);
        return s.neg ? sub_small_mutable(s.mag) : add_small_mutable(s.mag);
    }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    operator-=(T v) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (m_data.empty()) return *this;
        const budetail::small_arg s = budetail::as_small(v);
        return s.neg ? add_small_mutable(s.mag) : sub_small_mutable(s.mag);
    }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    operator*=(T v) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (m_data.empty()) return *this;
        const budetail::small_arg s = budetail::as_small(v);
        if (s.mag == 0) { set_zero(); return *this; }
        if (s.neg && !is_zero()) { set_undefined(); return *this; }
        return mul_small_mutable(s.mag);
    }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    operator/=(T v) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (m_data.empty()) return *this;
        const budetail::small_arg s = budetail::as_small(v);
        if (s.mag == 0) { set_undefined(); return *this; }
        if (s.neg && !is_zero()) { set_undefined(); return *this; }
        return div_small_mutable(s.mag);
    }

    template <class T>
    typename std::enable_if<std::is_integral<T>::value, BigUInt&>::type
    operator%=(T v) {
        static_assert(sizeof(T) <= sizeof(std::uint64_t), "BigUInt: native integers wider than 64 bits are not supported");
        if (m_data.empty()) return *this;
        const budetail::small_arg s = budetail::as_small(v);
        if (s.mag == 0) { set_undefined(); return *this; }
        return mod_small_mutable(s.mag);
    }

public:
    BigUInt& shift_left_mutable(std::size_t n) {
        if (m_data.empty()) return *this;
        if (n == 0 || is_zero()) return *this;
        const std::size_t limbs = n / 64;
        if (limbs >= max_limbs) { set_undefined(); return *this; }
        const unsigned      bits = static_cast<unsigned>(n % 64);
        const std::size_t   old  = m_data.size();
        const std::uint64_t top  = m_data[old - 1];
        const std::size_t   extra = (bits != 0 && (top >> (64 - bits)) != 0) ? 1u : 0u;
        const std::size_t   nsz  = old + limbs + extra;
        if (!grow_to(nsz)) return *this;

        if (bits == 0) {
            for (std::size_t j = nsz; j > limbs; --j) m_data[j - 1] = m_data[j - 1 - limbs];
        } else {
            const unsigned rs = 64 - bits;

            for (std::size_t j = nsz; j > limbs; --j) {
                const std::size_t   k  = j - 1 - limbs;
                const std::uint64_t hi = (k < old) ? (m_data[k] << bits) : std::uint64_t(0);
                const std::uint64_t lo = (k >= 1)  ? (m_data[k - 1] >> rs) : std::uint64_t(0);
                m_data[j - 1] = hi | lo;
            }
        }

        for (std::size_t j = 0; j < limbs; ++j) m_data[j] = 0;
        trim();
        return *this;
    }

    BigUInt& shift_right_mutable(std::size_t n) noexcept {
        if (m_data.empty()) return *this;
        if (n == 0 || is_zero()) return *this;
        const std::size_t limbs = n / 64;
        const std::size_t old   = m_data.size();
        if (limbs >= old) { set_limb_unchecked(0); return *this; }
        const unsigned    bits = static_cast<unsigned>(n % 64);
        const std::size_t nsz  = old - limbs;

        if (bits == 0) {
            for (std::size_t j = 0; j < nsz; ++j) m_data[j] = m_data[j + limbs];
        } else {
            const unsigned ls = 64 - bits;

            for (std::size_t j = 0; j < nsz; ++j) {
                const std::size_t   k  = j + limbs;
                const std::uint64_t lo = m_data[k] >> bits;
                const std::uint64_t hi = (k + 1 < old) ? (m_data[k + 1] << ls) : std::uint64_t(0);
                m_data[j] = lo | hi;
            }
        }

        m_data.set_size_unchecked(nsz);
        trim();
        return *this;
    }

    BigUInt& and_mutable(const BigUInt& o) noexcept {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return *this;
        const std::size_t n = (m_data.size() < o.m_data.size()) ? m_data.size() : o.m_data.size();
        for (std::size_t i = 0; i < n; ++i) m_data[i] &= o.m_data[i];
        m_data.set_size_unchecked(n);
        trim();
        return *this;
    }

    BigUInt& or_mutable(const BigUInt& o) {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) return *this;
        const std::size_t on = o.m_data.size();
        if (!grow_to(on)) return *this;
        for (std::size_t i = 0; i < on; ++i) m_data[i] |= o.m_data[i];
        return *this;
    }

    BigUInt& xor_mutable(const BigUInt& o) {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_limb_unchecked(0); return *this; }
        const std::size_t on = o.m_data.size();
        if (!grow_to(on)) return *this;
        for (std::size_t i = 0; i < on; ++i) m_data[i] ^= o.m_data[i];
        trim();
        return *this;
    }

    BigUInt& andn_mutable(const BigUInt& o) noexcept {
        if (m_data.empty() || o.m_data.empty()) { set_undefined(); return *this; }
        if (&o == this) { set_limb_unchecked(0); return *this; }
        const std::size_t n  = m_data.size();
        const std::size_t on = o.m_data.size();
        for (std::size_t i = 0; i < n && i < on; ++i) m_data[i] &= ~o.m_data[i];
        trim();
        return *this;
    }

    BigUInt& and_small_mutable(std::uint64_t v) noexcept {
        if (m_data.empty()) return *this;
        set_limb_unchecked(m_data[0] & v);
        return *this;
    }

    BigUInt& or_small_mutable(std::uint64_t v) noexcept {
        if (m_data.empty()) return *this;
        m_data[0] |= v;
        return *this;
    }

    BigUInt& xor_small_mutable(std::uint64_t v) noexcept {
        if (m_data.empty()) return *this;
        m_data[0] ^= v;
        return *this;
    }

    // Inverts exactly the limbs currently held, then renormalises
    // NOT an involution: see complement_bits for a width-stable version.
    BigUInt& complement_mutable() noexcept {
        if (m_data.empty()) return *this;
        for (std::size_t i = 0; i < m_data.size(); ++i) m_data[i] = ~m_data[i];
        trim();
        return *this;
    }

    BigUInt& complement_bits_mutable(std::size_t width) {
        if (m_data.empty()) return *this;
        if (width == 0)         { set_limb_unchecked(0); return *this; }
        if (width > max_bits)   { set_undefined();       return *this; }
        const std::size_t nsz = (width + 63) / 64;
        if (!grow_to(nsz)) return *this;
        for (std::size_t i = 0; i < nsz; ++i) m_data[i] = ~m_data[i];
        const unsigned tail = static_cast<unsigned>(width % 64);
        if (tail != 0) m_data[nsz - 1] &= (std::uint64_t(1) << tail) - 1;
        m_data.set_size_unchecked(nsz);
        trim();
        return *this;
    }

public:
    BigUInt shifted_left(std::size_t n) const &  { BigUInt r(*this); r.shift_left_mutable(n); return r; }
    BigUInt shifted_left(std::size_t n)       && { shift_left_mutable(n); return std::move(*this); }

    BigUInt shifted_right(std::size_t n) const & { BigUInt r(*this); r.shift_right_mutable(n); return r; }
    BigUInt shifted_right(std::size_t n)      && { shift_right_mutable(n); return std::move(*this); }

    BigUInt bit_and(const BigUInt& o) const &    { BigUInt r(*this); r.and_mutable(o); return r; }
    BigUInt bit_and(const BigUInt& o)      &&    { and_mutable(o); return std::move(*this); }

    BigUInt bit_or(const BigUInt& o) const &     { BigUInt r(*this); r.or_mutable(o); return r; }
    BigUInt bit_or(const BigUInt& o)      &&     { or_mutable(o); return std::move(*this); }

    BigUInt bit_xor(const BigUInt& o) const &    { BigUInt r(*this); r.xor_mutable(o); return r; }
    BigUInt bit_xor(const BigUInt& o)      &&    { xor_mutable(o); return std::move(*this); }

    BigUInt bit_andn(const BigUInt& o) const &   { BigUInt r(*this); r.andn_mutable(o); return r; }
    BigUInt bit_andn(const BigUInt& o)      &&   { andn_mutable(o); return std::move(*this); }

    BigUInt and_small(std::uint64_t v) const &   { BigUInt r(*this); r.and_small_mutable(v); return r; }
    BigUInt and_small(std::uint64_t v)      &&   { and_small_mutable(v); return std::move(*this); }

    BigUInt or_small(std::uint64_t v) const &    { BigUInt r(*this); r.or_small_mutable(v); return r; }
    BigUInt or_small(std::uint64_t v)      &&    { or_small_mutable(v); return std::move(*this); }

    BigUInt xor_small(std::uint64_t v) const &   { BigUInt r(*this); r.xor_small_mutable(v); return r; }
    BigUInt xor_small(std::uint64_t v)      &&   { xor_small_mutable(v); return std::move(*this); }

    BigUInt complement() const &                 { BigUInt r(*this); r.complement_mutable(); return r; }
    BigUInt complement()      &&                 { complement_mutable(); return std::move(*this); }

    BigUInt complement_bits(std::size_t w) const & { BigUInt r(*this); r.complement_bits_mutable(w); return r; }
    BigUInt complement_bits(std::size_t w)      && { complement_bits_mutable(w); return std::move(*this); }
};

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_CLASS_HPP