#ifndef FIZMO_ARRAYS_SIMD_X86_HPP
#define FIZMO_ARRAYS_SIMD_X86_HPP

#include "bits.hpp"
#include <cstring>

#if defined(FIZMO_ARRAYS_SSE2)
#include <emmintrin.h>
#if defined(FIZMO_ARRAYS_AVX2)
#include <immintrin.h>
#endif

namespace fizmo {
namespace arrays {
namespace simd {
namespace x86 {

enum class Lane : int { I8 = 0, I16, I32, I64, F32, F64 };

template <Lane L>
struct Sse;

template <>
struct Sse<Lane::I8> {
    static constexpr unsigned size = 1;
    static __m128i splat(const void* v) noexcept { std::uint8_t x; std::memcpy(&x, v, 1); return _mm_set1_epi8(static_cast<char>(x)); }
    static __m128i eq(__m128i a, __m128i b) noexcept { return _mm_cmpeq_epi8(a, b); }
};

template <>
struct Sse<Lane::I16> {
    static constexpr unsigned size = 2;
    static __m128i splat(const void* v) noexcept { std::uint16_t x; std::memcpy(&x, v, 2); return _mm_set1_epi16(static_cast<short>(x)); }
    static __m128i eq(__m128i a, __m128i b) noexcept { return _mm_cmpeq_epi16(a, b); }
};

template <>
struct Sse<Lane::I32> {
    static constexpr unsigned size = 4;
    static __m128i splat(const void* v) noexcept { std::uint32_t x; std::memcpy(&x, v, 4); return _mm_set1_epi32(static_cast<int>(x)); }
    static __m128i eq(__m128i a, __m128i b) noexcept { return _mm_cmpeq_epi32(a, b); }
};

template <>
struct Sse<Lane::I64> {
    static constexpr unsigned size = 8;
    static __m128i splat(const void* v) noexcept { long long x; std::memcpy(&x, v, 8); return _mm_set1_epi64x(x); }
    static __m128i eq(__m128i a, __m128i b) noexcept {
        const __m128i c = _mm_cmpeq_epi32(a, b);
        return _mm_and_si128(c, _mm_shuffle_epi32(c, 0xB1));
    }
};

template <>
struct Sse<Lane::F32> {
    static constexpr unsigned size = 4;
    static __m128i splat(const void* v) noexcept { float x; std::memcpy(&x, v, 4); return _mm_castps_si128(_mm_set1_ps(x)); }
    static __m128i eq(__m128i a, __m128i b) noexcept { return _mm_castps_si128(_mm_cmpeq_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
};

template <>
struct Sse<Lane::F64> {
    static constexpr unsigned size = 8;
    static __m128i splat(const void* v) noexcept { double x; std::memcpy(&x, v, 8); return _mm_castpd_si128(_mm_set1_pd(x)); }
    static __m128i eq(__m128i a, __m128i b) noexcept { return _mm_castpd_si128(_mm_cmpeq_pd(_mm_castsi128_pd(a), _mm_castsi128_pd(b))); }
};

FIZMO_ARRAY_INLINE std::uint32_t mask16(__m128i m) noexcept { return static_cast<std::uint32_t>(_mm_movemask_epi8(m)); }

#if defined(FIZMO_ARRAYS_AVX2)
template <Lane L>
struct Avx;

template <> struct Avx<Lane::I8>  { static __m256i splat(const void* v) noexcept { std::uint8_t x; std::memcpy(&x, v, 1); return _mm256_set1_epi8(static_cast<char>(x)); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_cmpeq_epi8(a, b); } };
template <> struct Avx<Lane::I16> { static __m256i splat(const void* v) noexcept { std::uint16_t x; std::memcpy(&x, v, 2); return _mm256_set1_epi16(static_cast<short>(x)); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_cmpeq_epi16(a, b); } };
template <> struct Avx<Lane::I32> { static __m256i splat(const void* v) noexcept { std::uint32_t x; std::memcpy(&x, v, 4); return _mm256_set1_epi32(static_cast<int>(x)); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_cmpeq_epi32(a, b); } };
template <> struct Avx<Lane::I64> { static __m256i splat(const void* v) noexcept { long long x; std::memcpy(&x, v, 8); return _mm256_set1_epi64x(x); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_cmpeq_epi64(a, b); } };
template <> struct Avx<Lane::F32> { static __m256i splat(const void* v) noexcept { float x; std::memcpy(&x, v, 4); return _mm256_castps_si256(_mm256_set1_ps(x)); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_castps_si256(_mm256_cmp_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), _CMP_EQ_OQ)); } };
template <> struct Avx<Lane::F64> { static __m256i splat(const void* v) noexcept { double x; std::memcpy(&x, v, 8); return _mm256_castpd_si256(_mm256_set1_pd(x)); } static __m256i eq(__m256i a, __m256i b) noexcept { return _mm256_castpd_si256(_mm256_cmp_pd(_mm256_castsi256_pd(a), _mm256_castsi256_pd(b), _CMP_EQ_OQ)); } };
#endif

template <Lane L>
inline std::size_t find(const void* data, std::size_t n, const void* value) noexcept {
    using S = Sse<L>;
    constexpr unsigned es = S::size;
    const char* b = static_cast<const char*>(data);
    const std::size_t bytes = n * es;
    std::size_t i = 0;
#if defined(FIZMO_ARRAYS_AVX2)
    {
        const __m256i k = Avx<L>::splat(value);
        for (; i + 128 <= bytes; i += 128) {
            const __m256i e0 = Avx<L>::eq(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i)), k);
            const __m256i e1 = Avx<L>::eq(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i + 32)), k);
            const __m256i e2 = Avx<L>::eq(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i + 64)), k);
            const __m256i e3 = Avx<L>::eq(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i + 96)), k);
            const __m256i any = _mm256_or_si256(_mm256_or_si256(e0, e1), _mm256_or_si256(e2, e3));
            if (_mm256_movemask_epi8(any)) {
                const __m256i es4[4] = { e0, e1, e2, e3 };
                for (int q = 0; q < 4; ++q) {
                    const std::uint32_t m = static_cast<std::uint32_t>(_mm256_movemask_epi8(es4[q]));
                    if (m) return (i + 32 * q + ctz32(m)) / es;
                }
            }
        }
        for (; i + 32 <= bytes; i += 32) {
            const std::uint32_t m = static_cast<std::uint32_t>(_mm256_movemask_epi8(Avx<L>::eq(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i)), k)));
            if (m) return (i + ctz32(m)) / es;
        }
    }
#endif
    const __m128i k = S::splat(value);
    for (; i + 64 <= bytes; i += 64) {
        const __m128i e0 = S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)), k);
        const __m128i e1 = S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 16)), k);
        const __m128i e2 = S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 32)), k);
        const __m128i e3 = S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 48)), k);
        if (mask16(_mm_or_si128(_mm_or_si128(e0, e1), _mm_or_si128(e2, e3)))) {
            std::uint32_t m = mask16(e0);
            if (m) return (i + ctz32(m)) / es;
            m = mask16(e1);
            if (m) return (i + 16 + ctz32(m)) / es;
            m = mask16(e2);
            if (m) return (i + 32 + ctz32(m)) / es;
            m = mask16(e3);
            return (i + 48 + ctz32(m)) / es;
        }
    }
    for (; i + 16 <= bytes; i += 16) {
        const std::uint32_t m = mask16(S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)), k));
        if (m) return (i + ctz32(m)) / es;
    }
    return i / es;
}

template <Lane L>
inline std::size_t rfind(const void* data, std::size_t n, const void* value, std::size_t& tail_start) noexcept {
    using S = Sse<L>;
    constexpr unsigned es = S::size;
    const char* b = static_cast<const char*>(data);
    const std::size_t bytes = n * es;
    const __m128i k = S::splat(value);
    std::size_t end = bytes;
    while (end >= 16) {
        const std::size_t at = end - 16;
        const std::uint32_t m = mask16(S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + at)), k));
        if (m) { tail_start = 0; return (at + 31 - clz32(m)) / es; }
        end = at;
    }
    tail_start = end / es;
    return n;
}

template <Lane L>
inline std::size_t count(const void* data, std::size_t n, const void* value, std::size_t& done) noexcept {
    using S = Sse<L>;
    constexpr unsigned es = S::size;
    const char* b = static_cast<const char*>(data);
    const std::size_t bytes = n * es;
    const __m128i k = S::splat(value);
    const __m128i zero = _mm_setzero_si128();
    __m128i total = zero;
    std::size_t i = 0;
    while (i + 64 <= bytes) {
        __m128i a0 = zero, a1 = zero, a2 = zero, a3 = zero;
        const std::size_t block_end = (bytes - i) / 64 > 255 ? i + 255 * 64 : i + ((bytes - i) / 64) * 64;
        for (; i < block_end; i += 64) {
            a0 = _mm_sub_epi8(a0, S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)), k));
            a1 = _mm_sub_epi8(a1, S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 16)), k));
            a2 = _mm_sub_epi8(a2, S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 32)), k));
            a3 = _mm_sub_epi8(a3, S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i + 48)), k));
        }
        total = _mm_add_epi64(total, _mm_add_epi64(_mm_add_epi64(_mm_sad_epu8(a0, zero), _mm_sad_epu8(a1, zero)), _mm_add_epi64(_mm_sad_epu8(a2, zero), _mm_sad_epu8(a3, zero))));
    }
    std::size_t bits = 0;
    for (; i + 16 <= bytes; i += 16) bits += popcount32(mask16(S::eq(_mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i)), k)));
    std::uint64_t lanes[2];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(lanes), total);
    done = i / es;
    return static_cast<std::size_t>((lanes[0] + lanes[1] + bits) / es);
}

} // namespace x86
} // namespace simd
} // namespace arrays
} // namespace fizmo

#endif

#endif // FIZMO_ARRAYS_SIMD_X86_HPP
