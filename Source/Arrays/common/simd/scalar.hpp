#ifndef FIZMO_ARRAYS_SIMD_SCALAR_HPP
#define FIZMO_ARRAYS_SIMD_SCALAR_HPP

#include "../config.hpp"

namespace fizmo {
namespace arrays {
namespace simd {
namespace scalar {

template <typename T>
inline std::size_t find(const T* p, std::size_t n, T v) noexcept {
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        if (p[i] == v) return i;
        if (p[i + 1] == v) return i + 1;
        if (p[i + 2] == v) return i + 2;
        if (p[i + 3] == v) return i + 3;
    }
    for (; i < n; ++i) if (p[i] == v) return i;
    return n;
}

template <typename T>
inline std::size_t rfind(const T* p, std::size_t n, T v) noexcept {
    for (std::size_t i = n; i > 0; --i) if (p[i - 1] == v) return i - 1;
    return n;
}

template <typename T>
inline std::size_t count(const T* p, std::size_t n, T v) noexcept {
    std::size_t c = 0;
    for (std::size_t i = 0; i < n; ++i) c += (p[i] == v) ? 1u : 0u;
    return c;
}

} // namespace scalar
} // namespace simd
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_SIMD_SCALAR_HPP
