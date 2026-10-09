#ifndef FIZMO_ARRAYS_SIMD_HPP
#define FIZMO_ARRAYS_SIMD_HPP

#include "scalar.hpp"
#include "x86.hpp"
#include <type_traits>

namespace fizmo {
namespace arrays {
namespace simd {

template <typename T>
struct lane_of {
    static constexpr int value =
        std::is_same<T, float>::value ? 4 :
        std::is_same<T, double>::value ? 5 :
        (std::is_integral<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value) ?
            (sizeof(T) == 1 ? 0 : sizeof(T) == 2 ? 1 : sizeof(T) == 4 ? 2 : sizeof(T) == 8 ? 3 : -1) : -1;
};

template <typename T>
struct accelerated : std::integral_constant<bool, lane_of<T>::value >= 0> {};

template <typename T>
inline std::size_t find(const T* p, std::size_t n, const T& v) noexcept {
#if defined(FIZMO_ARRAYS_SSE2)
    if (accelerated<T>::value && n >= 16) {
        std::size_t r = n;
        switch (lane_of<T>::value) {
            case 0: r = x86::find<x86::Lane::I8>(p, n, &v); break;
            case 1: r = x86::find<x86::Lane::I16>(p, n, &v); break;
            case 2: r = x86::find<x86::Lane::I32>(p, n, &v); break;
            case 3: r = x86::find<x86::Lane::I64>(p, n, &v); break;
            case 4: r = x86::find<x86::Lane::F32>(p, n, &v); break;
            case 5: r = x86::find<x86::Lane::F64>(p, n, &v); break;
            default: break;
        }
        if (r < n && p[r] == v) return r;
        for (std::size_t i = r; i < n; ++i) if (p[i] == v) return i;
        return n;
    }
#endif
    return scalar::find(p, n, v);
}

template <typename T>
inline std::size_t rfind(const T* p, std::size_t n, const T& v) noexcept {
#if defined(FIZMO_ARRAYS_SSE2)
    if (accelerated<T>::value && n >= 16) {
        std::size_t tail = n;
        std::size_t r = n;
        switch (lane_of<T>::value) {
            case 0: r = x86::rfind<x86::Lane::I8>(p, n, &v, tail); break;
            case 1: r = x86::rfind<x86::Lane::I16>(p, n, &v, tail); break;
            case 2: r = x86::rfind<x86::Lane::I32>(p, n, &v, tail); break;
            case 3: r = x86::rfind<x86::Lane::I64>(p, n, &v, tail); break;
            case 4: r = x86::rfind<x86::Lane::F32>(p, n, &v, tail); break;
            case 5: r = x86::rfind<x86::Lane::F64>(p, n, &v, tail); break;
            default: break;
        }
        if (r < n) return r;
        const std::size_t t = scalar::rfind(p, tail, v);
        return t < tail ? t : n;
    }
#endif
    return scalar::rfind(p, n, v);
}

template <typename T>
inline std::size_t count(const T* p, std::size_t n, const T& v) noexcept {
#if defined(FIZMO_ARRAYS_SSE2)
    if (accelerated<T>::value && n >= 16) {
        std::size_t done = 0;
        std::size_t c = 0;
        switch (lane_of<T>::value) {
            case 0: c = x86::count<x86::Lane::I8>(p, n, &v, done); break;
            case 1: c = x86::count<x86::Lane::I16>(p, n, &v, done); break;
            case 2: c = x86::count<x86::Lane::I32>(p, n, &v, done); break;
            case 3: c = x86::count<x86::Lane::I64>(p, n, &v, done); break;
            case 4: c = x86::count<x86::Lane::F32>(p, n, &v, done); break;
            case 5: c = x86::count<x86::Lane::F64>(p, n, &v, done); break;
            default: break;
        }
        return c + scalar::count(p + done, n - done, v);
    }
#endif
    return scalar::count(p, n, v);
}

} // namespace simd
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_SIMD_HPP
