#ifndef FIZMO_ARRAYS_ALGORITHMS_PARALLEL_HPP
#define FIZMO_ARRAYS_ALGORITHMS_PARALLEL_HPP

#include "sort_integer.hpp"
#include "../threads/threads.hpp"

namespace fizmo {
namespace arrays {
namespace algo {

template <typename T, typename Cmp>
inline std::size_t merge_path(const T* a, std::size_t na, const T* b, std::size_t nb, std::size_t d, Cmp& comp) {
    std::size_t lo = d > nb ? d - nb : 0;
    std::size_t hi = d < na ? d : na;
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        const std::size_t j = d - mid;
        if (mid < na && j > 0 && !comp(b[j - 1], a[mid])) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

template <typename T, typename Cmp>
inline void merge_segment(const T* a, std::size_t na, const T* b, std::size_t nb, std::size_t d0, std::size_t d1, T* out, Cmp& comp) {
    std::size_t i = merge_path(a, na, b, nb, d0, comp);
    std::size_t j = d0 - i;
    for (std::size_t k = d0; k < d1; ++k) {
        if (j < nb && (i >= na || comp(b[j], a[i]))) std::memcpy(static_cast<void*>(out + k), static_cast<const void*>(b + j++), sizeof(T));
        else std::memcpy(static_cast<void*>(out + k), static_cast<const void*>(a + i++), sizeof(T));
    }
}

inline unsigned choose_threads(std::size_t n, unsigned requested) noexcept {
    unsigned t = requested ? requested : threads::hardware_threads();
    if (t > 64) t = 64;
    const std::size_t by_size = n / (kParallelSortThreshold / 4);
    if (by_size < t) t = static_cast<unsigned>(by_size);
    return t < 1 ? 1 : t;
}

template <typename T, typename Cmp>
inline void parallel_sort(T* a, std::size_t n, Cmp comp, unsigned requested = 0, bool stable = false) {
    const unsigned t = choose_threads(n, requested);
    if (t < 2 || n < kParallelSortThreshold) {
        if (stable) fast_stable_sort(a, n, comp); else fast_sort(a, n, comp);
        return;
    }
    std::size_t bounds[65];
    for (unsigned i = 0; i <= t; ++i) bounds[i] = n * i / t;
    threads::run(t, [&](std::size_t i) {
        if (stable) fast_stable_sort(a + bounds[i], bounds[i + 1] - bounds[i], comp);
        else fast_sort(a + bounds[i], bounds[i + 1] - bounds[i], comp);
    });
    if (std::is_trivially_copyable<T>::value) {
        detail::TempBuffer<T> buf(n);
        T* src = a;
        T* dst = buf.data();
        for (std::size_t width = 1; width < t; width *= 2) {
            threads::run(t, [&](std::size_t task) {
                const std::size_t g0 = n * task / t, g1 = n * (task + 1) / t;
                for (std::size_t p = 0; p < t; p += 2 * width) {
                    const std::size_t lo = bounds[p];
                    const std::size_t mid = bounds[p + width < t ? p + width : t];
                    const std::size_t hi = bounds[p + 2 * width < t ? p + 2 * width : t];
                    const std::size_t s = g0 > lo ? g0 : lo;
                    const std::size_t e = g1 < hi ? g1 : hi;
                    if (s >= e) continue;
                    merge_segment(src + lo, mid - lo, src + mid, hi - mid, s - lo, e - lo, dst + lo, comp);
                }
            });
            T* tmp = src; src = dst; dst = tmp;
        }
        if (src != a) {
            threads::run(t, [&](std::size_t task) {
                const std::size_t g0 = n * task / t, g1 = n * (task + 1) / t;
                std::memcpy(static_cast<void*>(a + g0), static_cast<const void*>(src + g0), (g1 - g0) * sizeof(T));
            });
        }
        return;
    }
    for (std::size_t width = 1; width < t; width *= 2) {
        std::size_t pairs = 0;
        for (std::size_t p = 0; p + width < t; p += 2 * width) ++pairs;
        threads::run(pairs, [&](std::size_t q) {
            const std::size_t p = q * 2 * width;
            const std::size_t lo = bounds[p];
            const std::size_t mid = bounds[p + width];
            const std::size_t hi = bounds[p + 2 * width < t ? p + 2 * width : t];
            if (mid == lo || hi == mid || !comp(a[mid], a[mid - 1])) return;
            detail::TempBuffer<T> buf(mid - lo);
            merge_with_buffer(a + lo, mid - lo, hi - lo, buf.data(), comp);
        });
    }
}

template <typename T, typename Fn>
inline void parallel_for(T* a, std::size_t n, Fn fn, unsigned requested = 0, bool indexed = false) {
    unsigned t = requested ? requested : threads::hardware_threads();
    if (t > 64) t = 64;
    if (static_cast<std::size_t>(t) > n) t = static_cast<unsigned>(n ? n : 1);
    if (t < 2 || n < 4096) {
        for (std::size_t i = 0; i < n; ++i) fn(i, a[i]);
        return;
    }
    (void)indexed;
    threads::run(t, [&](std::size_t task) {
        const std::size_t g0 = n * task / t, g1 = n * (task + 1) / t;
        for (std::size_t i = g0; i < g1; ++i) fn(i, a[i]);
    });
}

} // namespace algo
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_PARALLEL_HPP
