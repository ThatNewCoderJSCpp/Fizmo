#ifndef FIZMO_ARRAYS_ALGORITHMS_SEARCH_HPP
#define FIZMO_ARRAYS_ALGORITHMS_SEARCH_HPP

#include "../traits.hpp"
#include "../simd/simd.hpp"
#include "../memory/element_ops.hpp"

namespace fizmo {
namespace arrays {
namespace algo {

template <typename T, typename U, typename Cmp>
inline std::size_t lower_bound(const T* a, std::size_t n, const U& v, Cmp comp) {
    if (n == 0) return 0;
    const T* base = a;
    while (n > 1) {
        const std::size_t half = n / 2;
        base = comp(base[half], v) ? base + half : base;
        n -= half;
    }
    return static_cast<std::size_t>(base - a) + (comp(*base, v) ? 1u : 0u);
}

template <typename T, typename U, typename Cmp>
inline std::size_t upper_bound(const T* a, std::size_t n, const U& v, Cmp comp) {
    if (n == 0) return 0;
    const T* base = a;
    while (n > 1) {
        const std::size_t half = n / 2;
        base = !comp(v, base[half]) ? base + half : base;
        n -= half;
    }
    return static_cast<std::size_t>(base - a) + (!comp(v, *base) ? 1u : 0u);
}

template <typename T, typename U, typename Cmp>
inline bool binary_search(const T* a, std::size_t n, const U& v, Cmp comp) {
    const std::size_t i = lower_bound(a, n, v, comp);
    return i < n && !comp(v, a[i]);
}

template <typename T, typename Cmp>
inline std::size_t fibonacci_search(const T* a, std::size_t n, const T& v, Cmp comp) {
    if (n == 0) return npos;
    std::size_t f2 = 0, f1 = 1, f = 1;
    while (f < n) { f2 = f1; f1 = f; f = f1 + f2; }
    long long offset = -1;
    while (f > 1) {
        const long long probe = offset + static_cast<long long>(f2);
        const std::size_t i = probe < static_cast<long long>(n - 1) ? static_cast<std::size_t>(probe) : n - 1;
        if (comp(a[i], v)) { f = f1; f1 = f2; f2 = f - f1; offset = static_cast<long long>(i); }
        else if (comp(v, a[i])) { f = f2; f1 = f1 - f2; f2 = f - f1; }
        else return i;
    }
    const std::size_t last = static_cast<std::size_t>(offset + 1);
    if (f1 && last < n && !comp(a[last], v) && !comp(v, a[last])) return last;
    return npos;
}

template <typename T>
inline std::size_t interpolation_search(const T* a, std::size_t n, const T& v) {
    if (n == 0) return npos;
    std::size_t lo = 0, hi = n - 1;
    while (lo <= hi && !(v < a[lo]) && !(a[hi] < v)) {
        if (!(a[lo] < a[hi])) return (!(a[lo] < v) && !(v < a[lo])) ? lo : npos;
        const long double den = static_cast<long double>(a[hi]) - static_cast<long double>(a[lo]);
        const long double num = static_cast<long double>(v) - static_cast<long double>(a[lo]);
        long double f = num / den;
        if (!(f >= 0.0L)) f = 0.0L;
        if (f > 1.0L) f = 1.0L;
        std::size_t pos = lo + static_cast<std::size_t>(f * static_cast<long double>(hi - lo));
        if (pos > hi) pos = hi;
        if (a[pos] < v) lo = pos + 1;
        else if (v < a[pos]) { if (pos == 0) return npos; hi = pos - 1; }
        else return pos;
    }
    return npos;
}

template <typename T, typename Cmp>
inline std::size_t exponential_search(const T* a, std::size_t n, const T& v, Cmp comp) {
    if (n == 0) return npos;
    if (!comp(a[0], v) && !comp(v, a[0])) return 0;
    std::size_t bound = 1;
    while (bound < n && comp(a[bound], v)) bound *= 2;
    const std::size_t lo = bound / 2;
    const std::size_t hi = bound + 1 < n ? bound + 1 : n;
    const std::size_t i = lo + lower_bound(a + lo, hi - lo, v, comp);
    if (i < hi && !comp(v, a[i])) return i;
    return npos;
}

template <typename T, typename Eq = EqualTo>
inline std::size_t find(const T* a, std::size_t n, const T& v) {
    return simd::find(a, n, v);
}

template <typename T, typename U>
inline typename std::enable_if<!std::is_same<T, U>::value, std::size_t>::type
find_any(const T* a, std::size_t n, const U& v) {
    for (std::size_t i = 0; i < n; ++i) if (a[i] == v) return i;
    return n;
}

template <typename T>
inline std::size_t find_subsequence(const T* hay, std::size_t n, const T* needle, std::size_t m) {
    if (m == 0) return 0;
    if (m > n) return npos;
    const std::size_t last = n - m;
    std::size_t i = 0;
    while (i <= last) {
        const std::size_t f = simd::find(hay + i, last - i + 1, needle[0]);
        if (f > last - i) return npos;
        i += f;
        if (detail::equal_n(hay + i + 1, needle + 1, m - 1)) return i;
        ++i;
    }
    return npos;
}

template <typename T>
inline std::size_t rfind_subsequence(const T* hay, std::size_t n, const T* needle, std::size_t m) {
    if (m == 0) return n;
    if (m > n) return npos;
    for (std::size_t i = n - m + 1; i > 0; --i) if (detail::equal_n(hay + i - 1, needle, m)) return i - 1;
    return npos;
}

} // namespace algo
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_SEARCH_HPP
