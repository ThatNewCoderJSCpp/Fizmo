#ifndef FIZMO_ARRAYS_ALGORITHMS_SORT_NAMED_HPP
#define FIZMO_ARRAYS_ALGORITHMS_SORT_NAMED_HPP

#include "sort.hpp"

namespace fizmo {
namespace arrays {
namespace algo {

template <typename T, typename Cmp>
inline void bubble_sort(T* a, std::size_t n, Cmp comp) {
    std::size_t end = n;
    while (end > 1) {
        std::size_t last_swap = 0;
        for (std::size_t i = 1; i < end; ++i) {
            if (comp(a[i], a[i - 1])) { swap_values(a[i], a[i - 1]); last_swap = i; }
        }
        end = last_swap;
    }
}

template <typename T, typename Cmp>
inline void selection_sort(T* a, std::size_t n, Cmp comp) {
    for (std::size_t i = 0; i + 1 < n; ++i) {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < n; ++j) if (comp(a[j], a[best])) best = j;
        if (best != i) swap_values(a[i], a[best]);
    }
}

template <typename T, typename Cmp>
inline void cocktail_sort(T* a, std::size_t n, Cmp comp) {
    if (n < 2) return;
    std::size_t lo = 0, hi = n - 1;
    while (lo < hi) {
        std::size_t last = lo;
        for (std::size_t i = lo; i < hi; ++i) if (comp(a[i + 1], a[i])) { swap_values(a[i], a[i + 1]); last = i; }
        hi = last;
        if (lo >= hi) break;
        last = hi;
        for (std::size_t i = hi; i > lo; --i) if (comp(a[i], a[i - 1])) { swap_values(a[i], a[i - 1]); last = i; }
        lo = last;
    }
}

template <typename T, typename Cmp>
inline void gnome_sort(T* a, std::size_t n, Cmp comp) {
    std::size_t i = 1;
    while (i < n) {
        if (i == 0 || !comp(a[i], a[i - 1])) ++i;
        else { swap_values(a[i], a[i - 1]); --i; }
    }
}

template <typename T, typename Cmp>
inline void shell_sort(T* a, std::size_t n, Cmp comp) {
    if (n < 2) return;
    static const std::size_t ciura[] = { 1, 4, 10, 23, 57, 132, 301, 701, 1750 };
    std::size_t gaps[64];
    std::size_t count = 0;
    for (std::size_t g : ciura) { if (g >= n) break; gaps[count++] = g; }
    while (count > 0 && count < 64) {
        const std::size_t next = gaps[count - 1] * 9 / 4;
        if (next >= n || next <= gaps[count - 1]) break;
        gaps[count++] = next;
    }
    if (count == 0) gaps[count++] = 1;
    for (std::size_t gi = count; gi > 0; --gi) {
        const std::size_t gap = gaps[gi - 1];
        for (std::size_t i = gap; i < n; ++i) {
            if (!comp(a[i], a[i - gap])) continue;
            T key(static_cast<T&&>(a[i]));
            std::size_t j = i;
            do { a[j] = static_cast<T&&>(a[j - gap]); j -= gap; } while (j >= gap && comp(key, a[j - gap]));
            a[j] = static_cast<T&&>(key);
        }
    }
}

template <typename T, typename Cmp>
inline void bitonic_merge(T* a, std::size_t cnt, bool ascending, Cmp& comp) {
    while (cnt > 1) {
        const std::size_t half = cnt / 2;
        for (std::size_t i = 0; i < half; ++i) {
            const bool swap = ascending ? comp(a[i + half], a[i]) : comp(a[i], a[i + half]);
            if (swap) swap_values(a[i], a[i + half]);
        }
        bitonic_merge(a, half, ascending, comp);
        a += half;
        cnt = half;
    }
}

template <typename T, typename Cmp>
inline void bitonic_rec(T* a, std::size_t cnt, bool ascending, Cmp& comp) {
    if (cnt <= 1) return;
    const std::size_t half = cnt / 2;
    bitonic_rec(a, half, true, comp);
    bitonic_rec(a + half, half, false, comp);
    bitonic_merge(a, cnt, ascending, comp);
}

template <typename T, typename Cmp>
inline void bitonic_sort(T* a, std::size_t n, Cmp comp) {
    if (n < 2) return;
    std::size_t padded = 1;
    while (padded < n) padded <<= 1;
    if (padded == n) { bitonic_rec(a, n, true, comp); return; }
    std::size_t max_i = 0;
    for (std::size_t i = 1; i < n; ++i) if (comp(a[max_i], a[i])) max_i = i;
    detail::ConstructedBuffer<T> buf(padded);
    for (std::size_t i = 0; i < n; ++i) buf.emplace(static_cast<T&&>(a[i]));
    const T& maxv = buf.data()[max_i];
    for (std::size_t i = n; i < padded; ++i) buf.emplace(maxv);
    bitonic_rec(buf.data(), padded, true, comp);
    for (std::size_t i = 0; i < n; ++i) a[i] = static_cast<T&&>(buf.data()[i]);
}

template <typename T, typename Cmp>
inline void quick_sort_rec(T* a, std::size_t n, Cmp& comp, int depth) {
    while (n > 16) {
        if (depth-- <= 0) { heap_sort(a, n, comp); return; }
        const std::size_t mid = n / 2;
        sort3(a, a + mid, a + n - 1, comp);
        T pivot(a[mid]);
        std::size_t lt = 0, i = 0, gt = n;
        while (i < gt) {
            if (comp(a[i], pivot)) { if (i != lt) swap_values(a[lt], a[i]); ++lt; ++i; }
            else if (comp(pivot, a[i])) { --gt; swap_values(a[i], a[gt]); }
            else ++i;
        }
        if (lt < n - gt) { quick_sort_rec(a, lt, comp, depth); a += gt; n -= gt; }
        else { quick_sort_rec(a + gt, n - gt, comp, depth); n = lt; }
    }
    insertion_sort(a, n, comp);
}

template <typename T, typename Cmp>
inline void quick_sort(T* a, std::size_t n, Cmp comp) {
    quick_sort_rec(a, n, comp, 2 * static_cast<int>(simd::log2_floor(n ? n : 1)) + 4);
}

} // namespace algo
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_SORT_NAMED_HPP
