#ifndef RANDOM_FIXED_INT_HPP
#define RANDOM_FIXED_INT_HPP

#include "random_std_int.hpp"
#include "../Multiprecision/Fixed Width int/type_traits.hpp"

namespace fizmo {
namespace detail {

template <typename U>
typename std::enable_if<is_fizmo_unsigned_v<U> && is_fizmo_int_v<U>, U>::type
random_fizmo_unsigned_secure() {
    constexpr std::size_t num_chunks = fizmo_numeric_bit_width_v<U> / 64;
    U result(0);
    for (std::size_t i = 0; i < num_chunks; ++i) {
        std::uint64_t chunk = 0;
        os_random_bytes(&chunk, sizeof(chunk));
        result = (result << 64) | U(chunk);
    }
    return result;
}

template <typename U>
typename std::enable_if<is_fizmo_unsigned_v<U> && is_fizmo_int_v<U>, U>::type
random_fizmo_unsigned_unsecure(RNG& rng) {
    constexpr std::size_t num_chunks = fizmo_numeric_bit_width_v<U> / 64;
    U result(0);
    for (std::size_t i = 0; i < num_chunks; ++i) {
        const std::uint64_t chunk = rng.random_int<std::uint64_t>(
            std::numeric_limits<std::uint64_t>::min(),
            std::numeric_limits<std::uint64_t>::max()
        );
        result = (result << 64) | U(chunk);
    }
    return result;
}

template <typename T, typename U = fizmo_make_unsigned_t<T>>
U fizmo_range_as_unsigned(T a, T b) {
    return static_cast<U>(b) - static_cast<U>(a);
}

template <typename U>
typename std::enable_if<is_fizmo_unsigned_v<U> && is_fizmo_int_v<U>, U>::type
fizmo_modulo(U rand_val, U range) {
    if (range == U::max()) return rand_val;
    return rand_val % (range + U(1));
}

} // namespace detail

template <typename T>
typename std::enable_if<is_fizmo_unsigned_v<T> && is_fizmo_int_v<T>, T>::type
random_int(T a, T b) {
    const T range    = b - a;
    const T rand_val = detail::random_fizmo_unsigned_secure<T>();
    return a + detail::fizmo_modulo(rand_val, range);
}

template <typename T>
typename std::enable_if<is_fizmo_signed_v<T> && is_fizmo_int_v<T>, T>::type
random_int(T a, T b) {
    using U = fizmo_make_unsigned_t<T>;
    const U range    = detail::fizmo_range_as_unsigned(a, b);
    const U rand_val = detail::random_fizmo_unsigned_secure<U>();
    const U mapped   = detail::fizmo_modulo(rand_val, range);
    return a + static_cast<T>(mapped);
}

template <typename T>
typename std::enable_if<is_fizmo_int_v<T>, T>::type
random_int_nothrow(T a, T b) noexcept {
    try {
        return random_int(a, b);
    } catch (...) {
        return b;
    }
}

template <typename T>
typename std::enable_if<is_fizmo_unsigned_v<T> && is_fizmo_int_v<T>, T>::type
unsecure_random_int(T a, T b) {
    detail::RNG rng;
    const T range    = b - a;
    const T rand_val = detail::random_fizmo_unsigned_unsecure<T>(rng);
    return a + detail::fizmo_modulo(rand_val, range);
}

template <typename T>
typename std::enable_if<is_fizmo_signed_v<T> && is_fizmo_int_v<T>, T>::type
unsecure_random_int(T a, T b) {
    using U = fizmo_make_unsigned_t<T>;
    detail::RNG rng;
    const U range    = detail::fizmo_range_as_unsigned(a, b);
    const U rand_val = detail::random_fizmo_unsigned_unsecure<U>(rng);
    const U mapped   = detail::fizmo_modulo(rand_val, range);
    return a + static_cast<T>(mapped);
}

template <typename T>
typename std::enable_if<is_fizmo_int_v<T>, T>::type
unsecure_random_int_nothrow(T a, T b) noexcept {
    try {
        return unsecure_random_int(a, b);
    } catch (...) {
        return b;
    }
}

} // namespace fizmo

#endif // RANDOM_FIXED_INT_HPP