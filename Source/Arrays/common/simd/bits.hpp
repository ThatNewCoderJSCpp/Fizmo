#ifndef FIZMO_ARRAYS_SIMD_BITS_HPP
#define FIZMO_ARRAYS_SIMD_BITS_HPP

#include "../config.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
    #include <intrin.h>
#endif

namespace fizmo {
namespace arrays {
namespace simd {

FIZMO_ARRAY_INLINE unsigned ctz32(std::uint32_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_ctz(v));
#elif defined(_MSC_VER)
    unsigned long i;
    _BitScanForward(&i, v);
    return static_cast<unsigned>(i);
#else
    unsigned n = 0;
    while (!(v & 1u)) { v >>= 1; ++n; }
    return n;
#endif
}

FIZMO_ARRAY_INLINE unsigned clz32(std::uint32_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_clz(v));
#elif defined(_MSC_VER)
    unsigned long i;
    _BitScanReverse(&i, v);
    return 31u - static_cast<unsigned>(i);
#else
    unsigned n = 0;
    while (!(v & 0x80000000u)) { v <<= 1; ++n; }
    return n;
#endif
}

FIZMO_ARRAY_INLINE unsigned popcount32(std::uint32_t v) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_popcount(v));
#else
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    return static_cast<unsigned>((((v + (v >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
#endif
}

FIZMO_ARRAY_INLINE unsigned log2_floor(std::size_t v) noexcept {
    unsigned n = 0;
    while (v > 1) { v >>= 1; ++n; }
    return n;
}

} // namespace simd
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_SIMD_BITS_HPP
