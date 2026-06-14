#ifndef FIZMO_MULTIPRECISION_INTEGER_TYPE_TRAITS_HPP
#define FIZMO_MULTIPRECISION_INTEGER_TYPE_TRAITS_HPP

#include "signed_integer.hpp"

namespace fizmo {

namespace multiprecision {
namespace detail {

template <std::size_t Bits>
inline std::string scientific(const umag<Bits>& m, bool neg, unsigned precision) {
    int highest = -1;
    for (long i = static_cast<long>(Bits) - 1; i >= 0; --i) if (m.get_bit(static_cast<std::size_t>(i))) { highest = static_cast<int>(i); break; }
    if (highest < 0) return "0.0e0";
    const std::size_t ndig = static_cast<std::size_t>(static_cast<double>(Bits) * 0.30103) + 3;
    std::vector<unsigned char> bcd(ndig, 0);

    for (int bit = highest; bit >= 0; --bit) {
        for (std::size_t i = 0; i < ndig; ++i) if (bcd[i] >= 5) bcd[i] += 3;
        unsigned char carry = m.get_bit(static_cast<std::size_t>(bit)) ? 1 : 0;
        for (int i = static_cast<int>(ndig) - 1; i >= 0; --i) { bcd[i] = (bcd[i] << 1) | carry; carry = (bcd[i] >> 4) & 1; bcd[i] &= 0x0F; }
    }

    std::size_t first = 0; while (first < ndig && bcd[first] == 0) ++first;
    if (first >= ndig) return "0.0e0";
    const unsigned exp = static_cast<unsigned>(ndig - first - 1);
    std::string r; if (neg) r += '-';
    r += char('0' + bcd[first]); r += '.';
    for (unsigned k = 0; k < precision; ++k) { std::size_t idx = first + 1 + k; r += (idx < ndig) ? char('0' + bcd[idx]) : '0'; }
    r += 'e'; r += std::to_string(exp);
    return r;
}

} // namespace detail

template <std::size_t B> std::string integer<B, sign::is_unsigned>::to_string_scientific(unsigned p) const { return detail::scientific<B>(m, false, p); }

template <std::size_t B> std::string integer<B, sign::is_signed>::to_string_scientific(unsigned p) const {
    if (is_undefined()) return "undefined";
    return detail::scientific<B>(m, neg, p);
}

template <typename T, std::size_t B, sign S, typename = typename std::enable_if<std::is_integral<T>::value>::type>
OPTIONAL_CPP14_CONSTEXPR integer<B, S> operator+(T v, const integer<B, S>& x) noexcept { return integer<B, S>(v) + x; }

template <typename T, std::size_t B, sign S, typename = typename std::enable_if<std::is_integral<T>::value>::type>
OPTIONAL_CPP14_CONSTEXPR integer<B, S> operator-(T v, const integer<B, S>& x) noexcept { return integer<B, S>(v) - x; }

template <typename T, std::size_t B, sign S, typename = typename std::enable_if<std::is_integral<T>::value>::type>
OPTIONAL_CPP14_CONSTEXPR integer<B, S> operator*(T v, const integer<B, S>& x) noexcept { return integer<B, S>(v) * x; }

using uint8    = integer<8,    sign::is_unsigned>; using int8    = integer<8,    sign::is_signed>;
using uint16   = integer<16,   sign::is_unsigned>; using int16   = integer<16,   sign::is_signed>;
using uint32   = integer<32,   sign::is_unsigned>; using int32   = integer<32,   sign::is_signed>;
using uint64   = integer<64,   sign::is_unsigned>; using int64   = integer<64,   sign::is_signed>;
using uint128  = integer<128,  sign::is_unsigned>; using int128  = integer<128,  sign::is_signed>;
using uint256  = integer<256,  sign::is_unsigned>; using int256  = integer<256,  sign::is_signed>;
using uint512  = integer<512,  sign::is_unsigned>; using int512  = integer<512,  sign::is_signed>;
using uint1024 = integer<1024, sign::is_unsigned>; using int1024 = integer<1024, sign::is_signed>;
using uint2048 = integer<2048, sign::is_unsigned>; using int2048 = integer<2048, sign::is_signed>;
using uint4096 = integer<4096, sign::is_unsigned>; using int4096 = integer<4096, sign::is_signed>;

} // namespace multiprecision 

template <class>                                 struct is_fizmo_static_int                                : std::false_type {};
template <std::size_t B, multiprecision::sign S> struct is_fizmo_static_int<multiprecision::integer<B, S>> : std::true_type  {};
template <class T> constexpr bool is_fizmo_static_int_v = is_fizmo_static_int<T>::value;

template <class>                                 struct is_fizmo_int                                : std::false_type {};
template <std::size_t B, multiprecision::sign S> struct is_fizmo_int<multiprecision::integer<B, S>> : std::true_type  {};
template <class T> constexpr bool is_fizmo_int_v = is_fizmo_int<T>::value;

template <class>         struct is_fizmo_signed                                                              : std::false_type {};
template <std::size_t B> struct is_fizmo_signed<multiprecision::integer<B, multiprecision::sign::is_signed>> : std::true_type  {};
template <class T> constexpr bool is_fizmo_signed_v = is_fizmo_signed<T>::value;

template <class>         struct is_fizmo_unsigned                                                                : std::false_type {};
template <std::size_t B> struct is_fizmo_unsigned<multiprecision::integer<B, multiprecision::sign::is_unsigned>> : std::true_type  {};
template <class T> constexpr bool is_fizmo_unsigned_v = is_fizmo_unsigned<T>::value;

template <class T> struct fizmo_make_signed { using type = T; };
template <std::size_t B, multiprecision::sign S> struct fizmo_make_signed<multiprecision::integer<B, S>> { using type = multiprecision::integer<B, multiprecision::sign::is_signed>; };
template <class T> using fizmo_make_signed_t = typename fizmo_make_signed<T>::type;

template <class T> struct fizmo_make_unsigned { using type = T; };
template <std::size_t B, multiprecision::sign S> struct fizmo_make_unsigned<multiprecision::integer<B, S>> { using type = multiprecision::integer<B, multiprecision::sign::is_unsigned>; };
template <class T> using fizmo_make_unsigned_t = typename fizmo_make_unsigned<T>::type;

template <class T> struct integer_rank;

template <std::size_t B, multiprecision::sign S>
struct integer_rank<multiprecision::integer<B, S>> : std::integral_constant<std::size_t, B * 2 + (S == multiprecision::sign::is_signed ? 1 : 0)> {};
    
template <typename T>
constexpr std::size_t integer_rank_v = integer_rank<T>::value; 

} // namespace fizmo

namespace std {
    template <std::size_t B, fizmo::multiprecision::sign S>
    inline string to_string(const fizmo::multiprecision::integer<B, S>& x) { return x.to_string(); }
}

#endif // FIZMO_MULTIPRECISION_INTEGER_TYPE_TRAITS_HPP