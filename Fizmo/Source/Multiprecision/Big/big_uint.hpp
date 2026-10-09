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

 std::uint64_t div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& rem) noexcept;

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
    bool grow_to(std::size_t n);

    // False: *this is undefined
    bool push_limb(std::uint64_t v);

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

    BigUInt& operator=(const limb_store& v);

    BigUInt& operator=(limb_store&& v) noexcept;

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

    static BigUInt from_limbs(const std::uint64_t* p, std::size_t n);

public:
    bool        is_zero()      const noexcept { return m_data.size() == 1 && m_data[0] == 0; }
    bool        is_undefined() const noexcept { return m_data.empty(); }
    std::size_t limb_count()   const noexcept { return m_data.size(); }

    std::uint64_t get_lowest_bits() const noexcept { return m_data.front(); }
    std::uint64_t get_highest_bits() const noexcept { return m_data.back(); }

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

    static BigUInt max();

public:
    bool is_one()  const noexcept { return m_data.size() == 1 && m_data[0] == std::uint64_t(1); }
    bool is_even() const noexcept { return !m_data.empty() && (m_data[0] & std::uint64_t(1)) == 0; }
    bool is_odd()  const noexcept { return !m_data.empty() && (m_data[0] & std::uint64_t(1)) != 0; }

public:
    int compare(const BigUInt& o) const noexcept;

    bool equals(const BigUInt& o) const noexcept;

public:
    std::size_t bit_length() const noexcept;

    bool get_bit(std::size_t i) const noexcept;

    void set_bit(std::size_t i, bool b = true);

    void clear_bit(std::size_t i) noexcept;

    bool flip_bit(std::size_t i);

public:
    BigUInt& shifted_limbs_mutable(std::size_t n);

    BigUInt shifted_limbs(std::size_t n) const &;

    BigUInt shifted_limbs(std::size_t n) && {
        shifted_limbs_mutable(n);
        return std::move(*this);
    }

    BigUInt& add_shifted_mutable(const BigUInt& src, std::size_t offset);

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

    long long get_exponent_base10() const noexcept;

    long long count_leading_zeros() const noexcept;

    long long count_leading_zeros(std::size_t width) const noexcept;

    long long count_trailing_zeros() const noexcept;

public:
    BigUInt& add_small_mutable(std::uint64_t v);

    BigUInt& sub_small_mutable(std::uint64_t v) noexcept;

    BigUInt& mul_small_mutable(std::uint64_t v);

    std::uint64_t divmod_small_mutable(std::uint64_t d) noexcept;

    BigUInt& div_small_mutable(std::uint64_t d) noexcept {
        divmod_small_mutable(d);
        return *this;
    }

    BigUInt& mod_small_mutable(std::uint64_t d) noexcept;

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

    std::uint64_t mod_small(std::uint64_t d) const noexcept;

public:
    BigUInt& add_mutable(const BigUInt& o);

    BigUInt& sub_mutable(const BigUInt& o) noexcept;

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
    BigUInt& shift_left_mutable(std::size_t n);

    BigUInt& shift_right_mutable(std::size_t n) noexcept;

    BigUInt& and_mutable(const BigUInt& o) noexcept;

    BigUInt& or_mutable(const BigUInt& o);

    BigUInt& xor_mutable(const BigUInt& o);

    BigUInt& andn_mutable(const BigUInt& o) noexcept;

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
    BigUInt& complement_mutable() noexcept;

    BigUInt& complement_bits_mutable(std::size_t width);

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