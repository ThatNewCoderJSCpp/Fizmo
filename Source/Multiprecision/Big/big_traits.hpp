#ifndef FIZMO_MULTIPRECISION_BIG_TYPE_TRAITS_HPP
#define FIZMO_MULTIPRECISION_BIG_TYPE_TRAITS_HPP

#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>

#include "big_float.hpp"   

namespace fizmo {

template <class T>
struct is_fizmo_arithmetic : std::integral_constant<bool, is_fizmo_int<T>::value || is_fizmo_float<T>::value> {};

template <class T> constexpr bool is_fizmo_arithmetic_v = is_fizmo_arithmetic<T>::value;

template <class>  struct is_fizmo_big_int : std::false_type {};
template <> struct is_fizmo_big_int<multiprecision::BigUInt> : std::true_type {};
template <> struct is_fizmo_big_int<multiprecision::BigInt>  : std::true_type {};
template <class T> constexpr bool is_fizmo_big_int_v = is_fizmo_big_int<T>::value;

template <class>  struct is_fizmo_big_float : std::false_type {};
template <> struct is_fizmo_big_float<multiprecision::BigFloat> : std::true_type {};
template <class T> constexpr bool is_fizmo_big_float_v = is_fizmo_big_float<T>::value;

template <class T>
struct is_fizmo_big : std::integral_constant<bool, is_fizmo_big_int<T>::value || is_fizmo_big_float<T>::value> {};
template <class T> constexpr bool is_fizmo_big_v = is_fizmo_big<T>::value;

template <class T> struct fizmo_is_dynamic_width : is_fizmo_big<T> {};
template <class T> constexpr bool fizmo_is_dynamic_width_v = fizmo_is_dynamic_width<T>::value;

template <> struct is_fizmo_int<multiprecision::BigUInt>    : std::true_type {};
template <> struct is_fizmo_int<multiprecision::BigInt>     : std::true_type {};
template <> struct is_fizmo_float<multiprecision::BigFloat> : std::true_type {};

template <> struct is_fizmo_unsigned<multiprecision::BigUInt> : std::true_type {};
template <> struct is_fizmo_signed<multiprecision::BigInt>    : std::true_type {};
template <> struct is_fizmo_signed<multiprecision::BigFloat>  : std::true_type {};

template <> struct fizmo_make_signed<multiprecision::BigUInt>  { using type = multiprecision::BigInt;  };
template <> struct fizmo_make_unsigned<multiprecision::BigInt> { using type = multiprecision::BigUInt; };

constexpr std::size_t fizmo_big_rank_base = std::numeric_limits<std::size_t>::max() - 3;

template <> struct integer_rank<multiprecision::BigUInt> : std::integral_constant<std::size_t, fizmo_big_rank_base + 1> {};

template <> struct integer_rank<multiprecision::BigInt> : std::integral_constant<std::size_t, fizmo_big_rank_base + 2> {};

template <> struct fizmo_float_rank<multiprecision::BigFloat, void> : std::integral_constant<std::size_t, fizmo_big_rank_base + 3> {};


template <> struct fizmo_type_bits<multiprecision::BigUInt, void>  : std::integral_constant<std::size_t, multiprecision::BigUInt::max_bits> {};
template <> struct fizmo_type_bits<multiprecision::BigInt, void>   : std::integral_constant<std::size_t, multiprecision::BigInt::max_bits> {};
template <> struct fizmo_type_bits<multiprecision::BigFloat, void> : std::integral_constant<std::size_t, multiprecision::BigFloatContext::max_prec> {};

template <> struct fizmo_numeric_bit_width<multiprecision::BigUInt , void> { static constexpr std::size_t value = multiprecision::BigUInt::max_bits; };
template <> struct fizmo_numeric_bit_width<multiprecision::BigInt  , void> { static constexpr std::size_t value = multiprecision::BigInt::max_bits; };
template <> struct fizmo_numeric_bit_width<multiprecision::BigFloat, void> { static constexpr std::size_t value = multiprecision::BigFloatContext::max_prec; };

template <> struct fizmo_float_from_int<multiprecision::BigUInt , void> { using type = multiprecision::BigFloat; };
template <> struct fizmo_float_from_int<multiprecision::BigInt  , void> { using type = multiprecision::BigFloat; };
template <> struct fizmo_int_from_float<multiprecision::BigFloat, void> { using type = multiprecision::BigInt;  };

template <class T> struct fizmo_promote<T, typename std::enable_if<is_fizmo_big_v<T>>::type> { using type = T; };
template <class T> struct fizmo_demote <T, typename std::enable_if<is_fizmo_big_v<T>>::type> { using type = T; };

namespace bigdetail {

template <class T>
struct is_float_like : std::integral_constant<bool, is_fizmo_float_v<T> || std::is_floating_point<T>::value> {};

template <class T>
struct is_signed_like : std::integral_constant<bool,
    is_fizmo_signed_v<T> || std::is_floating_point<T>::value ||
    (std::is_integral<T>::value && std::is_signed<T>::value)> {};

} // namespace bigdetail

template <class A, class B>
struct fizmo_common_type<A, B, typename std::enable_if<is_fizmo_big_v<A> || is_fizmo_big_v<B>>::type> {
private:
    static constexpr bool any_float  = bigdetail::is_float_like<A>::value  || bigdetail::is_float_like<B>::value;
    static constexpr bool any_signed = bigdetail::is_signed_like<A>::value || bigdetail::is_signed_like<B>::value;

public:
    using type = typename std::conditional<
        any_float,
        multiprecision::BigFloat,
        typename std::conditional<any_signed, multiprecision::BigInt, multiprecision::BigUInt>::type
    >::type;
};

} // namespace fizmo

namespace std {
    inline string to_string(const fizmo::multiprecision::BigUInt&  x) { return fizmo::multiprecision::BigInt(x).to_string(); }
    inline string to_string(const fizmo::multiprecision::BigInt&   x) { return x.to_string(); }
    inline string to_string(const fizmo::multiprecision::BigFloat& x) { return x.to_string(); }
}

#endif // FIZMO_MULTIPRECISION_BIG_TYPE_TRAITS_HPP