#ifndef FIZMO_ARRAYS_ALGORITHMS_SORT_HPP
#define FIZMO_ARRAYS_ALGORITHMS_SORT_HPP

#include "../memory/temp_buffer.hpp"
#include "../simd/bits.hpp"

namespace fizmo {
namespace arrays {
namespace algo {

using detail::swap_values;

template <typename T, typename Cmp>
inline void insertion_sort(T* a, std::size_t n, Cmp& comp) {
    for (std::size_t i = 1; i < n; ++i) {
        if (!comp(a[i], a[i - 1])) continue;
        T tmp(static_cast<T&&>(a[i]));
        std::size_t j = i;
        do { a[j] = static_cast<T&&>(a[j - 1]); --j; } while (j > 0 && comp(tmp, a[j - 1]));
        a[j] = static_cast<T&&>(tmp);
    }
}

template <typename T, typename Cmp>
inline void unguarded_insertion_sort(T* begin, T* end, Cmp& comp) {
    if (begin == end) return;
    for (T* cur = begin + 1; cur != end; ++cur) {
        T* sift = cur;
        T* sift_1 = cur - 1;
        if (comp(*sift, *sift_1)) {
            T tmp(static_cast<T&&>(*sift));
            do { *sift-- = static_cast<T&&>(*sift_1); } while (comp(tmp, *--sift_1));
            *sift = static_cast<T&&>(tmp);
        }
    }
}

template <typename T, typename Cmp>
inline bool partial_insertion_sort(T* begin, T* end, Cmp& comp) {
    if (begin == end) return true;
    std::size_t moved = 0;
    for (T* cur = begin + 1; cur != end; ++cur) {
        if (moved > 8) return false;
        T* sift = cur;
        T* sift_1 = cur - 1;
        if (comp(*sift, *sift_1)) {
            T tmp(static_cast<T&&>(*sift));
            do { *sift-- = static_cast<T&&>(*sift_1); } while (sift != begin && comp(tmp, *--sift_1));
            *sift = static_cast<T&&>(tmp);
            moved += static_cast<std::size_t>(cur - sift);
        }
    }
    return true;
}

template <typename T, typename Cmp>
inline void sift_down(T* a, std::size_t root, std::size_t n, Cmp& comp) {
    T value(static_cast<T&&>(a[root]));
    std::size_t hole = root;
    for (;;) {
        std::size_t child = 2 * hole + 1;
        if (child >= n) break;
        if (child + 1 < n && comp(a[child], a[child + 1])) ++child;
        if (!comp(value, a[child])) break;
        a[hole] = static_cast<T&&>(a[child]);
        hole = child;
    }
    a[hole] = static_cast<T&&>(value);
}

template <typename T, typename Cmp>
inline void make_heap(T* a, std::size_t n, Cmp& comp) {
    for (std::size_t i = n / 2; i > 0; --i) sift_down(a, i - 1, n, comp);
}

template <typename T, typename Cmp>
inline void heap_sort(T* a, std::size_t n, Cmp& comp) {
    if (n < 2) return;
    make_heap(a, n, comp);
    for (std::size_t end = n - 1; end > 0; --end) {
        swap_values(a[0], a[end]);
        sift_down(a, 0, end, comp);
    }
}

template <typename T, typename Cmp>
FIZMO_ARRAY_INLINE void sort2(T* a, T* b, Cmp& comp) { if (comp(*b, *a)) swap_values(*a, *b); }

template <typename T, typename Cmp>
FIZMO_ARRAY_INLINE void sort3(T* a, T* b, T* c, Cmp& comp) { sort2(a, b, comp); sort2(b, c, comp); sort2(a, b, comp); }

template <typename T, typename Cmp>
inline T* partition_right(T* begin, T* end, Cmp& comp, bool& already_partitioned) {
    T pivot(static_cast<T&&>(*begin));
    T* first = begin;
    T* last = end;
    while (comp(*++first, pivot)) {}
    if (first - 1 == begin) { while (first < last && !comp(*--last, pivot)) {} }
    else { while (!comp(*--last, pivot)) {} }
    already_partitioned = first >= last;
    while (first < last) {
        swap_values(*first, *last);
        while (comp(*++first, pivot)) {}
        while (!comp(*--last, pivot)) {}
    }
    T* pivot_pos = first - 1;
    *begin = static_cast<T&&>(*pivot_pos);
    *pivot_pos = static_cast<T&&>(pivot);
    return pivot_pos;
}

template <typename T, typename Cmp>
inline T* partition_left(T* begin, T* end, Cmp& comp) {
    T pivot(static_cast<T&&>(*begin));
    T* first = begin;
    T* last = end;
    while (comp(pivot, *--last)) {}
    if (last + 1 == end) { while (first < last && !comp(pivot, *++first)) {} }
    else { while (!comp(pivot, *++first)) {} }
    while (first < last) {
        swap_values(*first, *last);
        while (comp(pivot, *--last)) {}
        while (!comp(pivot, *++first)) {}
    }
    T* pivot_pos = last;
    *begin = static_cast<T&&>(*pivot_pos);
    *pivot_pos = static_cast<T&&>(pivot);
    return pivot_pos;
}

static constexpr std::size_t kInsertionThreshold = 24;
static constexpr std::size_t kNintherThreshold = 128;

template <typename T, typename Cmp>
inline void pdq_loop(T* begin, T* end, Cmp& comp, int bad_allowed, bool leftmost) {
    for (;;) {
        const std::size_t size = static_cast<std::size_t>(end - begin);
        if (size < kInsertionThreshold) {
            if (leftmost) insertion_sort(begin, size, comp);
            else unguarded_insertion_sort(begin, end, comp);
            return;
        }
        const std::size_t s2 = size / 2;
        if (size > kNintherThreshold) {
            sort3(begin, begin + s2, end - 1, comp);
            sort3(begin + 1, begin + (s2 - 1), end - 2, comp);
            sort3(begin + 2, begin + (s2 + 1), end - 3, comp);
            sort3(begin + (s2 - 1), begin + s2, begin + (s2 + 1), comp);
            swap_values(*begin, *(begin + s2));
        } else {
            sort3(begin + s2, begin, end - 1, comp);
        }
        if (!leftmost && !comp(*(begin - 1), *begin)) {
            begin = partition_left(begin, end, comp) + 1;
            continue;
        }
        bool already = false;
        T* pivot_pos = partition_right(begin, end, comp, already);
        const std::size_t l_size = static_cast<std::size_t>(pivot_pos - begin);
        const std::size_t r_size = static_cast<std::size_t>(end - (pivot_pos + 1));
        const bool unbalanced = l_size < size / 8 || r_size < size / 8;
        if (unbalanced) {
            if (--bad_allowed == 0) {
                heap_sort(begin, size, comp);
                return;
            }
            if (l_size >= kInsertionThreshold) {
                swap_values(*begin, *(begin + l_size / 4));
                swap_values(*(pivot_pos - 1), *(pivot_pos - l_size / 4));
                if (l_size > kNintherThreshold) {
                    swap_values(*(begin + 1), *(begin + (l_size / 4 + 1)));
                    swap_values(*(begin + 2), *(begin + (l_size / 4 + 2)));
                    swap_values(*(pivot_pos - 2), *(pivot_pos - (l_size / 4 + 1)));
                    swap_values(*(pivot_pos - 3), *(pivot_pos - (l_size / 4 + 2)));
                }
            }
            if (r_size >= kInsertionThreshold) {
                swap_values(*(pivot_pos + 1), *(pivot_pos + (1 + r_size / 4)));
                swap_values(*(end - 1), *(end - r_size / 4));
                if (r_size > kNintherThreshold) {
                    swap_values(*(pivot_pos + 2), *(pivot_pos + (2 + r_size / 4)));
                    swap_values(*(pivot_pos + 3), *(pivot_pos + (3 + r_size / 4)));
                    swap_values(*(end - 2), *(end - (1 + r_size / 4)));
                    swap_values(*(end - 3), *(end - (2 + r_size / 4)));
                }
            }
        } else if (already && partial_insertion_sort(begin, pivot_pos, comp) && partial_insertion_sort(pivot_pos + 1, end, comp)) {
            return;
        }
        pdq_loop(begin, pivot_pos, comp, bad_allowed, leftmost);
        begin = pivot_pos + 1;
        leftmost = false;
    }
}

template <typename T, typename Cmp>
inline void pdq_sort(T* a, std::size_t n, Cmp comp) {
    if (n < 2) return;
    pdq_loop(a, a + n, comp, static_cast<int>(simd::log2_floor(n)) + 1, true);
}

template <typename T, typename Cmp>
inline void merge_with_buffer(T* a, std::size_t mid, std::size_t n, T* buf, Cmp& comp) {
    detail::construct_move_n(buf, a, mid);
    std::size_t i = 0, j = mid, k = 0;
    while (i < mid && j < n) {
        if (comp(a[j], buf[i])) a[k++] = static_cast<T&&>(a[j++]);
        else a[k++] = static_cast<T&&>(buf[i++]);
    }
    while (i < mid) a[k++] = static_cast<T&&>(buf[i++]);
    detail::destroy_n(buf, mid);
}

template <typename T, typename Cmp>
inline void merge_sort_rec(T* a, std::size_t n, T* buf, Cmp& comp) {
    if (n <= 32) { insertion_sort(a, n, comp); return; }
    const std::size_t mid = n / 2;
    merge_sort_rec(a, mid, buf, comp);
    merge_sort_rec(a + mid, n - mid, buf, comp);
    if (!comp(a[mid], a[mid - 1])) return;
    merge_with_buffer(a, mid, n, buf, comp);
}

template <typename T, typename Cmp>
inline void merge_in_place(T* a, std::size_t mid, std::size_t n, Cmp& comp) {
    if (mid == 0 || mid == n) return;
    if (n == 2) { if (comp(a[1], a[0])) swap_values(a[0], a[1]); return; }
    std::size_t cut1, cut2;
    if (mid > n - mid) {
        cut1 = mid / 2;
        std::size_t lo = mid, len = n - mid;
        while (len > 0) { const std::size_t h = len / 2; if (comp(a[lo + h], a[cut1])) { lo += h + 1; len -= h + 1; } else len = h; }
        cut2 = lo;
    } else {
        cut2 = mid + (n - mid) / 2;
        std::size_t lo = 0, len = mid;
        while (len > 0) { const std::size_t h = len / 2; if (!comp(a[cut2], a[lo + h])) { lo += h + 1; len -= h + 1; } else len = h; }
        cut1 = lo;
    }
    detail::rotate_left_n(a + cut1, cut2 - cut1, mid - cut1);
    const std::size_t new_mid = cut1 + (cut2 - mid);
    merge_in_place(a, cut1, new_mid, comp);
    merge_in_place(a + new_mid, cut2 - new_mid, n - new_mid, comp);
}

template <typename T, typename Cmp>
inline void stable_sort(T* a, std::size_t n, Cmp comp) {
    if (n < 2) return;
    if (n <= 32) { insertion_sort(a, n, comp); return; }
    detail::TempBuffer<T> buf(n / 2 + 1);
    merge_sort_rec(a, n, buf.data(), comp);
}

template <typename T, typename Cmp>
inline void nth_element(T* a, std::size_t n, std::size_t k, Cmp comp) {
    if (n < 2 || k >= n) return;
    std::size_t lo = 0, hi = n;
    int depth = 2 * static_cast<int>(simd::log2_floor(n)) + 2;
    while (hi - lo > kInsertionThreshold) {
        if (depth-- == 0) {
            T* base = a + lo;
            const std::size_t len = hi - lo;
            make_heap(base, len, comp);
            for (std::size_t end = len - 1; end > 0; --end) { swap_values(base[0], base[end]); sift_down(base, 0, end, comp); }
            return;
        }
        const std::size_t mid = lo + (hi - lo) / 2;
        sort3(a + lo, a + mid, a + hi - 1, comp);
        swap_values(a[lo], a[mid]);
        T* p;
        if (lo > 0 && !comp(a[lo - 1], a[lo])) p = partition_left(a + lo, a + hi, comp);
        else { bool already = false; p = partition_right(a + lo, a + hi, comp, already); }
        const std::size_t pi = static_cast<std::size_t>(p - a);
        if (pi == k) return;
        if (k < pi) hi = pi; else lo = pi + 1;
    }
    insertion_sort(a + lo, hi - lo, comp);
}

template <typename T, typename Cmp>
inline void partial_sort(T* a, std::size_t n, std::size_t k, Cmp comp) {
    if (k > n) k = n;
    if (k == 0 || n < 2) return;
    if (k >= n / 2) { pdq_sort(a, n, comp); return; }
    make_heap(a, k, comp);
    for (std::size_t i = k; i < n; ++i) {
        if (comp(a[i], a[0])) {
            swap_values(a[0], a[i]);
            sift_down(a, 0, k, comp);
        }
    }
    for (std::size_t end = k - 1; end > 0; --end) { swap_values(a[0], a[end]); sift_down(a, 0, end, comp); }
}

template <typename T, typename Cmp>
inline bool is_sorted(const T* a, std::size_t n, Cmp comp) {
    for (std::size_t i = 1; i < n; ++i) if (comp(a[i], a[i - 1])) return false;
    return true;
}

template <typename T, typename Cmp>
inline std::size_t is_sorted_until(const T* a, std::size_t n, Cmp comp) {
    for (std::size_t i = 1; i < n; ++i) if (comp(a[i], a[i - 1])) return i;
    return n;
}

template <typename T, typename Pred>
inline std::size_t partition(T* a, std::size_t n, Pred& pred) {
    std::size_t write = 0;
    for (std::size_t read = 0; read < n; ++read) {
        if (pred(a[read])) {
            if (write != read) swap_values(a[write], a[read]);
            ++write;
        }
    }
    return write;
}

template <typename T, typename Pred>
inline std::size_t stable_partition(T* a, std::size_t n, Pred& pred) {
    if (n == 0) return 0;
    detail::TempBuffer<T> buf(n);
    T* b = buf.data();
    std::size_t keep = 0, moved = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (pred(a[i])) { if (keep != i) a[keep] = static_cast<T&&>(a[i]); ++keep; }
        else { detail::construct_element(b + moved, static_cast<T&&>(a[i])); ++moved; }
    }
    for (std::size_t i = 0; i < moved; ++i) a[keep + i] = static_cast<T&&>(b[i]);
    detail::destroy_n(b, moved);
    return keep;
}

} // namespace algo
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_SORT_HPP
