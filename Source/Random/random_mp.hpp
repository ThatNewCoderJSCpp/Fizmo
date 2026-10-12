#ifndef FIZMO_RANDOM_MP_HPP
#define FIZMO_RANDOM_MP_HPP

#include "random_std_int.hpp"
#include "../Multiprecision/Big/big_float.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fizmo {

multiprecision::BigUInt random_biguint_bits(std::size_t bits);

multiprecision::BigUInt random_biguint_below(const multiprecision::BigUInt& bound);

multiprecision::BigUInt random_big(const multiprecision::BigUInt& a, const multiprecision::BigUInt& b);

multiprecision::BigInt random_big(const multiprecision::BigInt& a, const multiprecision::BigInt& b);

inline multiprecision::BigUInt random_big(const multiprecision::BigUInt& b) { return random_big(multiprecision::BigUInt::zero(), b); }

namespace detail {

template <typename T>
struct is_floatmp_type : std::false_type {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct is_floatmp_type<multiprecision::floatmp<TB, MB, S>> : std::true_type {};

struct ExactBinary {
    multiprecision::BigInt       mantissa;
    std::int64_t exponent = 0;
};

struct BinaryFormat {
    std::size_t  precision;
    std::int64_t min_quantum;
};

constexpr std::int64_t kUnboundedQuantum = std::numeric_limits<std::int64_t>::min() / 4;

ExactBinary exact_from(const multiprecision::BigFloat& v);

multiprecision::BigFloat exact_to_bigfloat(const ExactBinary& v);

ExactBinary uniform_floor(const ExactBinary& a, const ExactBinary& b, const BinaryFormat& fmt);

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, BinaryFormat>::type binary_format() noexcept {
    const std::int64_t p = std::numeric_limits<T>::digits;
    return { static_cast<std::size_t>(p), static_cast<std::int64_t>(std::numeric_limits<T>::min_exponent) - p };
}

template <typename T>
typename std::enable_if<is_floatmp_type<T>::value, BinaryFormat>::type binary_format() noexcept {
    const std::size_t mb = T::mantissa_bits;
    const std::size_t eb = T::exponent_bits;
    if (eb >= 60) return { mb + 1, kUnboundedQuantum };
    const std::int64_t bias = (std::int64_t(1) << (eb - 1)) - 1;
    return { mb + 1, 1 - bias - static_cast<std::int64_t>(mb) };
}

template <typename T>
T from_bigfloat(const multiprecision::BigFloat& v) { return static_cast<T>(v); }

template <std::size_t Bits>
multiprecision::integer<Bits, multiprecision::sign::is_unsigned> integer_from_big(const multiprecision::BigInt& v, std::integral_constant<multiprecision::sign, multiprecision::sign::is_unsigned>) {
    using U = multiprecision::integer<Bits, multiprecision::sign::is_unsigned>;
    const multiprecision::BigUInt& mag = v.magnitude();
    U r(std::uint64_t(0));

    for (std::size_t i = mag.limb_count(); i-- > 0;) {
        if (Bits > 64) r <<= 64;
        r |= U(mag.limb(i));
    }

    return r;
}

template <std::size_t Bits>
multiprecision::integer<Bits, multiprecision::sign::is_signed> integer_from_big(const multiprecision::BigInt& v, std::integral_constant<multiprecision::sign, multiprecision::sign::is_signed>) {
    using U = multiprecision::integer<Bits, multiprecision::sign::is_unsigned>;
    using I = multiprecision::integer<Bits, multiprecision::sign::is_signed>;
    const U u = integer_from_big<Bits>(v, std::integral_constant<multiprecision::sign, multiprecision::sign::is_unsigned>());
    return I::from_magnitude(u.magnitude(), v.is_negative());
}

} // namespace detail

template <std::size_t Bits, multiprecision::sign S>
multiprecision::integer<Bits, S> random_int(const multiprecision::integer<Bits, S>& a, const multiprecision::integer<Bits, S>& b) {
    const multiprecision::BigInt lo(a), hi(b);
    return detail::integer_from_big<Bits>(random_big(lo, hi), std::integral_constant<multiprecision::sign, S>());
}

template <std::size_t Bits, multiprecision::sign S>
multiprecision::integer<Bits, S> random_int(const multiprecision::integer<Bits, S>& b) {
    return random_int(multiprecision::integer<Bits, S>(std::uint64_t(0)), b);
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value || detail::is_floatmp_type<T>::value, T>::type
random_real(const T& a, const T& b) {
    const multiprecision::BigFloat fa(a), fb(b);
    if (!fa.is_finite() || !fb.is_finite()) throw std::domain_error("random_real: bounds must be finite");
    if (fa == fb) return a;
    const bool swap = fb < fa;
    const detail::ExactBinary r = detail::uniform_floor(detail::exact_from(swap ? fb : fa), detail::exact_from(swap ? fa : fb), detail::binary_format<T>());
    return detail::from_bigfloat<T>(detail::exact_to_bigfloat(r));
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value || detail::is_floatmp_type<T>::value, T>::type
random_real() {
    return random_real<T>(T(0), T(1));
}

multiprecision::BigFloat random_real(const multiprecision::BigFloat& a, const multiprecision::BigFloat& b, std::size_t precision = 256);

inline multiprecision::BigFloat random_bigfloat(std::size_t precision = 256) {
    return random_real(multiprecision::BigFloat(0), multiprecision::BigFloat(1), precision);
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value || detail::is_floatmp_type<T>::value, T>::type
random_unit() {
    return random_real<T>(T(0), T(1));
}

} // namespace fizmo

#endif // FIZMO_RANDOM_MP_HPP
