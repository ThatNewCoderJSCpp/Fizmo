#ifndef FIZMO_BIG_UNSIGNED_INTEGER_OPERATOR_OVERLOADS_HPP
#define FIZMO_BIG_UNSIGNED_INTEGER_OPERATOR_OVERLOADS_HPP

#include "big_uint_mult_detail.hpp"
#include "big_uint_div_detail.hpp"

namespace fizmo {
namespace multiprecision {

inline bool operator==(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return a.equals(b);
}

inline bool operator!=(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return !a.equals(b);
}

inline bool operator<(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return a.compare(b) < 0;
}

inline bool operator>(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return a.compare(b) > 0;
}

inline bool operator<=(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return a.compare(b) <= 0;
}

inline bool operator>=(const BigUInt& a, const BigUInt& b) noexcept {
    if (a.is_undefined() || b.is_undefined()) { return false; }
    return a.compare(b) >= 0;
}

#define FIZMO_BIGUINT_BINOP(OP)                                                       \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, BigUInt>::type                \
    operator OP(const BigUInt& a, T v) { BigUInt r(a); r OP##= v; return r; }         \
                                                                                      \
    template <class T>                                                                \
    typename std::enable_if<std::is_integral<T>::value, BigUInt>::type                \
    operator OP(BigUInt&& a, T v) { a OP##= v; return std::move(a); }

FIZMO_BIGUINT_BINOP(+)
FIZMO_BIGUINT_BINOP(-)
FIZMO_BIGUINT_BINOP(*)
FIZMO_BIGUINT_BINOP(/)
FIZMO_BIGUINT_BINOP(%)

#undef FIZMO_BIGUINT_BINOP

template <class T>
typename std::enable_if<std::is_integral<T>::value, BigUInt>::type
operator+(T v, const BigUInt& a) { BigUInt r(a); r += v; return r; }

template <class T>
typename std::enable_if<std::is_integral<T>::value, BigUInt>::type
operator+(T v, BigUInt&& a) { a += v; return std::move(a); }

template <class T>
typename std::enable_if<std::is_integral<T>::value, BigUInt>::type
operator*(T v, const BigUInt& a) { BigUInt r(a); r *= v; return r; }

template <class T>
typename std::enable_if<std::is_integral<T>::value, BigUInt>::type
operator*(T v, BigUInt&& a) { a *= v; return std::move(a); }

inline BigUInt& operator<<=(BigUInt& a, std::size_t n) { a.shift_left_mutable(n);  return a; }
inline BigUInt& operator>>=(BigUInt& a, std::size_t n) { a.shift_right_mutable(n); return a; }

inline BigUInt operator<<(const BigUInt& a, std::size_t n) { return a.shifted_left(n); }
inline BigUInt operator<<(BigUInt&& a,      std::size_t n) { return std::move(a).shifted_left(n); }
inline BigUInt operator>>(const BigUInt& a, std::size_t n) { return a.shifted_right(n); }
inline BigUInt operator>>(BigUInt&& a,      std::size_t n) { return std::move(a).shifted_right(n); }

inline BigUInt& operator&=(BigUInt& a, const BigUInt& b) { a.and_mutable(b); return a; }
inline BigUInt& operator|=(BigUInt& a, const BigUInt& b) { a.or_mutable(b);  return a; }
inline BigUInt& operator^=(BigUInt& a, const BigUInt& b) { a.xor_mutable(b); return a; }

inline BigUInt& operator&=(BigUInt& a, std::uint64_t v) { a.and_small_mutable(v); return a; }
inline BigUInt& operator|=(BigUInt& a, std::uint64_t v) { a.or_small_mutable(v);  return a; }
inline BigUInt& operator^=(BigUInt& a, std::uint64_t v) { a.xor_small_mutable(v); return a; }

inline BigUInt operator&(const BigUInt& a, const BigUInt& b) { return a.bit_and(b); }
inline BigUInt operator&(BigUInt&& a,      const BigUInt& b) { return std::move(a).bit_and(b); }
inline BigUInt operator&(const BigUInt& a, BigUInt&& b)      { return std::move(b).bit_and(a); }
inline BigUInt operator&(BigUInt&& a,      BigUInt&& b)      { return std::move(a).bit_and(b); }

inline BigUInt operator|(const BigUInt& a, const BigUInt& b) { return a.bit_or(b); }
inline BigUInt operator|(BigUInt&& a,      const BigUInt& b) { return std::move(a).bit_or(b); }
inline BigUInt operator|(const BigUInt& a, BigUInt&& b)      { return std::move(b).bit_or(a); }
inline BigUInt operator|(BigUInt&& a,      BigUInt&& b)      { return std::move(a).bit_or(b); }

inline BigUInt operator^(const BigUInt& a, const BigUInt& b) { return a.bit_xor(b); }
inline BigUInt operator^(BigUInt&& a,      const BigUInt& b) { return std::move(a).bit_xor(b); }
inline BigUInt operator^(const BigUInt& a, BigUInt&& b)      { return std::move(b).bit_xor(a); }
inline BigUInt operator^(BigUInt&& a,      BigUInt&& b)      { return std::move(a).bit_xor(b); }

inline BigUInt operator&(const BigUInt& a, std::uint64_t v) { return a.and_small(v); }
inline BigUInt operator&(BigUInt&& a,      std::uint64_t v) { return std::move(a).and_small(v); }
inline BigUInt operator&(std::uint64_t v, const BigUInt& a) { return a.and_small(v); }
inline BigUInt operator&(std::uint64_t v, BigUInt&& a)      { return std::move(a).and_small(v); }

inline BigUInt operator|(const BigUInt& a, std::uint64_t v) { return a.or_small(v); }
inline BigUInt operator|(BigUInt&& a,      std::uint64_t v) { return std::move(a).or_small(v); }
inline BigUInt operator|(std::uint64_t v, const BigUInt& a) { return a.or_small(v); }
inline BigUInt operator|(std::uint64_t v, BigUInt&& a)      { return std::move(a).or_small(v); }

inline BigUInt operator^(const BigUInt& a, std::uint64_t v) { return a.xor_small(v); }
inline BigUInt operator^(BigUInt&& a,      std::uint64_t v) { return std::move(a).xor_small(v); }
inline BigUInt operator^(std::uint64_t v, const BigUInt& a) { return a.xor_small(v); }
inline BigUInt operator^(std::uint64_t v, BigUInt&& a)      { return std::move(a).xor_small(v); }

inline BigUInt operator~(const BigUInt& a) { return a.complement(); }
inline BigUInt operator~(BigUInt&& a)      { return std::move(a).complement(); }

inline BigUInt  operator* (const BigUInt& a, const BigUInt& b) { return mdetail::mul(a, b); }
inline BigUInt& operator*=(BigUInt& a, const BigUInt& b)       { a = mdetail::mul(a, b); return a; }

inline BigUInt  operator+ (const BigUInt& a, const BigUInt& b) { BigUInt r(a); r.add_mutable(b); return r; }
inline BigUInt  operator+ (BigUInt&& a,      const BigUInt& b) { a.add_mutable(b); return std::move(a); }
inline BigUInt  operator+ (const BigUInt& a, BigUInt&& b)      { b.add_mutable(a); return std::move(b); }
inline BigUInt  operator+ (BigUInt&& a,      BigUInt&& b)      { a.add_mutable(b); return std::move(a); }
inline BigUInt& operator+=(BigUInt& a, const BigUInt& b)       { a.add_mutable(b); return a; }

inline BigUInt  operator- (const BigUInt& a, const BigUInt& b) { BigUInt r(a); r.sub_mutable(b); return r; }
inline BigUInt  operator- (BigUInt&& a,      const BigUInt& b) { a.sub_mutable(b); return std::move(a); }
inline BigUInt& operator-=(BigUInt& a, const BigUInt& b)       { a.sub_mutable(b); return a; }

inline BigUInt  operator/ (const BigUInt& a, const BigUInt& b) { return mdetail::div(a, b); }
inline BigUInt& operator/=(BigUInt& a, const BigUInt& b)       { a = mdetail::div(a, b); return a; }

inline BigUInt  operator% (const BigUInt& a, const BigUInt& b) { return mdetail::mod(a, b); }
inline BigUInt& operator%=(BigUInt& a, const BigUInt& b)       { a = mdetail::mod(a, b); return a; }

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_BIG_UNSIGNED_INTEGER_OPERATOR_OVERLOADS_HPP