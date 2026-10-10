#ifndef FIZMO_ARRAYS_CONFIG_HPP
#define FIZMO_ARRAYS_CONFIG_HPP

#include <cstddef>
#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
    #define FIZMO_ARRAYS_WINDOWS 1
#elif defined(__linux__)
    #define FIZMO_ARRAYS_LINUX 1
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define FIZMO_ARRAY_INLINE inline __attribute__((always_inline))
    #define FIZMO_ARRAY_NOINLINE __attribute__((noinline))
    #define FIZMO_ARRAY_LIKELY(x) __builtin_expect(!!(x), 1)
    #define FIZMO_ARRAY_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define FIZMO_ARRAY_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define FIZMO_ARRAY_INLINE __forceinline
    #define FIZMO_ARRAY_NOINLINE __declspec(noinline)
    #define FIZMO_ARRAY_LIKELY(x) (x)
    #define FIZMO_ARRAY_UNLIKELY(x) (x)
    #define FIZMO_ARRAY_RESTRICT __restrict
#else
    #define FIZMO_ARRAY_INLINE inline
    #define FIZMO_ARRAY_NOINLINE
    #define FIZMO_ARRAY_LIKELY(x) (x)
    #define FIZMO_ARRAY_UNLIKELY(x) (x)
    #define FIZMO_ARRAY_RESTRICT
#endif

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #define FIZMO_ARRAYS_SSE2 1
#endif

#if defined(__AVX2__)
    #define FIZMO_ARRAYS_AVX2 1
#endif

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    #define FIZMO_ARRAYS_EXCEPTIONS 1
#endif

namespace fizmo {
namespace arrays {

static constexpr std::size_t kCacheLine = 64;
static constexpr std::size_t kMinAllocationBytes = 64;
#if defined(FIZMO_ARRAYS_WINDOWS)
static constexpr std::size_t kLargeAllocationBytes = std::size_t(1) * 1024 * 1024;
#else
static constexpr std::size_t kLargeAllocationBytes = std::size_t(32) * 1024 * 1024;
#endif
static constexpr std::size_t kHugePageBytes = std::size_t(2) * 1024 * 1024;
static constexpr std::size_t kParallelSortThreshold = std::size_t(1) << 16;
static constexpr std::size_t kRadixSortThreshold = 256;
static constexpr std::size_t kHashSetThreshold = 32;

} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_CONFIG_HPP
