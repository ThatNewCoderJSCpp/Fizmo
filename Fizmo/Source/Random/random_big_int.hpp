#ifndef RANDOM_BIG_INT_HPP
#define RANDOM_BIG_INT_HPP

#include "random_std_int.hpp"
#include "../Multiprecision/Big/big_uint.hpp"
#include "../Multiprecision/Big/big_int.hpp"

namespace fizmo {
namespace detail {

inline multiprecision::BigUint random_big_uint_bits_secure(std::size_t bit_len) {
    if (bit_len == 0) return multiprecision::BigUint::zero();
    const std::size_t num_limbs = (bit_len + 63) / 64;
    const std::size_t top_bits  = bit_len % 64; 
    std::vector<std::uint64_t> limbs(num_limbs);
    os_random_bytes(limbs.data(), num_limbs * sizeof(std::uint64_t));
    if (top_bits != 0) limbs[num_limbs - 1] &= (std::uint64_t(1) << top_bits) - 1;
    multiprecision::BigUint result(0);
    for (std::size_t i = num_limbs; i-- > 0; ) result = (result << 64) | multiprecision::BigUint(limbs[i]);
    return result;
}

inline multiprecision::BigUint random_big_uint_bits_unsecure(std::size_t bit_len, RNG& rng) {
    if (bit_len == 0) return multiprecision::BigUint::zero();
    const std::size_t num_limbs = (bit_len + 63) / 64;
    const std::size_t top_bits  = bit_len % 64;
    multiprecision::BigUint result(0);

    for (std::size_t i = num_limbs; i-- > 0; ) {
        std::uint64_t chunk = rng.random_int<std::uint64_t>(
            std::numeric_limits<std::uint64_t>::min(),
            std::numeric_limits<std::uint64_t>::max()
        );

        if (i == num_limbs - 1 && top_bits != 0) chunk &= (std::uint64_t(1) << top_bits) - 1;
        result = (result << 64) | multiprecision::BigUint(chunk);
    }

    return result;
}

inline multiprecision::BigUint big_uint_in_range_secure(const multiprecision::BigUint& range) {
    if (range.is_zero()) return multiprecision::BigUint::zero();
    const std::size_t bit_len = range.bit_length();

    while (true) {
        multiprecision::BigUint candidate = random_big_uint_bits_secure(bit_len);
        if (candidate <= range) return candidate;
    }
}

inline multiprecision::BigUint big_uint_in_range_unsecure(const multiprecision::BigUint& range, RNG& rng) {
    if (range.is_zero()) return multiprecision::BigUint::zero();
    const std::size_t bit_len = range.bit_length();

    while (true) {
        multiprecision::BigUint candidate = random_big_uint_bits_unsecure(bit_len, rng);
        if (candidate <= range) return candidate;
    }
}

} // namespace detail

inline multiprecision::BigUint random_int(const multiprecision::BigUint& a, const multiprecision::BigUint& b) {
    if (b <= a) return a;
    return a + detail::big_uint_in_range_secure(b - a);
}

inline multiprecision::BigUint random_int_nothrow(const multiprecision::BigUint& a, const multiprecision::BigUint& b) noexcept {
    try   { return random_int(a, b); }
    catch (...) { return b; }
}

inline multiprecision::BigUint unsecure_random_int(const multiprecision::BigUint& a, const multiprecision::BigUint& b) {
    if (b <= a) return a;
    detail::RNG rng;
    return a + detail::big_uint_in_range_unsecure(b - a, rng);
}

inline multiprecision::BigUint unsecure_random_int_nothrow(const multiprecision::BigUint& a, const multiprecision::BigUint& b) noexcept {
    try   { return unsecure_random_int(a, b); }
    catch (...) { return b; }
}

inline multiprecision::BigInt random_int(const multiprecision::BigInt& a, const multiprecision::BigInt& b) {
    if (b <= a) return a;
    const multiprecision::BigUint range  = (b - a).magnitude();
    const multiprecision::BigUint mapped = detail::big_uint_in_range_secure(range);
    return a + multiprecision::BigInt(mapped);
}

inline multiprecision::BigInt random_int_nothrow(const multiprecision::BigInt& a, const multiprecision::BigInt& b) noexcept {
    try   { return random_int(a, b); }
    catch (...) { return multiprecision::BigInt::error(); }
}

inline multiprecision::BigInt unsecure_random_int(const multiprecision::BigInt& a, const multiprecision::BigInt& b) {
    if (b <= a) return a;
    detail::RNG rng;
    const multiprecision::BigUint range  = (b - a).magnitude();
    const multiprecision::BigUint mapped = detail::big_uint_in_range_unsecure(range, rng);
    return a + multiprecision::BigInt(mapped);
}

inline multiprecision::BigInt unsecure_random_int_nothrow(const multiprecision::BigInt& a, const multiprecision::BigInt& b) noexcept {
    try   { return unsecure_random_int(a, b); }
    catch (...) { return multiprecision::BigInt::error(); }
}

} // namespace fizmo

#endif // RANDOM_BIG_INT_HPP