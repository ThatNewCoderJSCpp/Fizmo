#ifndef FIZMO_ARRAYS_ALGORITHMS_SORT_INTEGER_HPP
#define FIZMO_ARRAYS_ALGORITHMS_SORT_INTEGER_HPP

#include "sort.hpp"

namespace fizmo {
namespace arrays {
namespace algo {

template <typename T>
struct RadixKey {
    static constexpr bool supported = (std::is_integral<T>::value && !std::is_same<T, bool>::value) || std::is_same<T, float>::value || std::is_same<T, double>::value;
    using U = typename std::conditional<sizeof(T) == 1, std::uint8_t,
              typename std::conditional<sizeof(T) == 2, std::uint16_t,
              typename std::conditional<sizeof(T) == 4, std::uint32_t, std::uint64_t>::type>::type>::type;

    static FIZMO_ARRAY_INLINE U key(const T& v) noexcept {
        U u;
        std::memcpy(&u, &v, sizeof(T));
        if (std::is_floating_point<T>::value) {
            const U sign = static_cast<U>(U(1) << (sizeof(T) * 8 - 1));
            return (u & sign) ? static_cast<U>(~u) : static_cast<U>(u | sign);
        }
        if (std::is_signed<T>::value) return static_cast<U>(u ^ static_cast<U>(U(1) << (sizeof(T) * 8 - 1)));
        return u;
    }
};

template <typename T>
inline void radix_sort_lsd(T* a, std::size_t n) {
    static_assert(RadixKey<T>::supported, "radix sort needs an integer or floating point type");
    using K = RadixKey<T>;
    using U = typename K::U;
    if (n < 2) return;
    constexpr int passes = static_cast<int>(sizeof(T));
    std::size_t counts[passes][256];
    std::memset(counts, 0, sizeof(counts));
    for (std::size_t i = 0; i < n; ++i) {
        const U k = K::key(a[i]);
        for (int p = 0; p < passes; ++p) ++counts[p][(k >> (8 * p)) & 0xFF];
    }
    detail::TempBuffer<T> buf(n);
    T* src = a;
    T* dst = buf.data();
    bool any = false;
    for (int p = 0; p < passes; ++p) {
        std::size_t* c = counts[p];
        bool skip = false;
        for (int b = 0; b < 256; ++b) { if (c[b] == n) { skip = true; break; } if (c[b]) break; }
        if (skip) continue;
        std::size_t sum = 0;
        for (int b = 0; b < 256; ++b) { const std::size_t t = c[b]; c[b] = sum; sum += t; }
        const int shift = 8 * p;
        for (std::size_t i = 0; i < n; ++i) {
            const U k = K::key(src[i]);
            std::memcpy(static_cast<void*>(dst + c[(k >> shift) & 0xFF]++), static_cast<const void*>(src + i), sizeof(T));
        }
        T* t = src; src = dst; dst = t;
        any = true;
    }
    if (any && src != a) std::memcpy(static_cast<void*>(a), static_cast<const void*>(src), n * sizeof(T));
}

template <typename T>
inline void radix_sort_msd_rec(T* a, T* buf, std::size_t n, int shift) {
    using K = RadixKey<T>;
    using U = typename K::U;
    if (n <= 32 || shift < 0) {
        Less less;
        insertion_sort(a, n, less);
        return;
    }
    std::size_t counts[256] = {};
    for (std::size_t i = 0; i < n; ++i) ++counts[(K::key(a[i]) >> shift) & 0xFF];
    std::size_t offsets[257];
    offsets[0] = 0;
    for (int b = 0; b < 256; ++b) offsets[b + 1] = offsets[b] + counts[b];
    if (counts[(K::key(a[0]) >> shift) & 0xFF] == n) { radix_sort_msd_rec(a, buf, n, shift - 8); return; }
    std::size_t pos[256];
    for (int b = 0; b < 256; ++b) pos[b] = offsets[b];
    for (std::size_t i = 0; i < n; ++i) {
        const U k = K::key(a[i]);
        std::memcpy(static_cast<void*>(buf + pos[(k >> shift) & 0xFF]++), static_cast<const void*>(a + i), sizeof(T));
    }
    std::memcpy(static_cast<void*>(a), static_cast<const void*>(buf), n * sizeof(T));
    for (int b = 0; b < 256; ++b) {
        const std::size_t len = offsets[b + 1] - offsets[b];
        if (len > 1) radix_sort_msd_rec(a + offsets[b], buf, len, shift - 8);
    }
}

template <typename T>
inline void radix_sort_msd(T* a, std::size_t n) {
    static_assert(RadixKey<T>::supported, "radix sort needs an integer or floating point type");
    if (n < 2) return;
    detail::TempBuffer<T> buf(n);
    radix_sort_msd_rec(a, buf.data(), n, static_cast<int>(sizeof(T) * 8 - 8));
}

template <typename T>
inline bool counting_sort(T* a, std::size_t n, bool descending) {
    if (n < 2) return true;
    T lo = a[0], hi = a[0];
    for (std::size_t i = 1; i < n; ++i) { if (a[i] < lo) lo = a[i]; if (hi < a[i]) hi = a[i]; }
    using UT = typename std::make_unsigned<T>::type;
    const UT span = static_cast<UT>(static_cast<UT>(hi) - static_cast<UT>(lo));
    const std::size_t limit = (n < (std::size_t(1) << 20) ? std::size_t(1) << 20 : n) * 4;
    if (static_cast<unsigned long long>(span) >= limit) {
        radix_sort_lsd(a, n);
        if (descending) detail::reverse_n(a, n);
        return false;
    }
    const std::size_t range = static_cast<std::size_t>(span) + 1;
    detail::TempBuffer<std::size_t> counts(range);
    std::size_t* c = counts.data();
    std::memset(c, 0, range * sizeof(std::size_t));
    for (std::size_t i = 0; i < n; ++i) ++c[static_cast<std::size_t>(static_cast<UT>(static_cast<UT>(a[i]) - static_cast<UT>(lo)))];
    std::size_t idx = 0;
    for (std::size_t k = 0; k < range; ++k) {
        const std::size_t r = descending ? range - 1 - k : k;
        const T v = static_cast<T>(static_cast<UT>(static_cast<UT>(lo) + static_cast<UT>(r)));
        for (std::size_t m = c[r]; m > 0; --m) a[idx++] = v;
    }
    return true;
}

template <typename T, typename Cmp>
struct radix_eligible : std::integral_constant<bool, RadixKey<T>::supported && (std::is_same<Cmp, Less>::value || std::is_same<Cmp, Greater>::value)> {};

template <typename T, typename Cmp>
inline typename std::enable_if<radix_eligible<T, Cmp>::value>::type
fast_sort(T* a, std::size_t n, Cmp comp) {
    if (n >= kRadixSortThreshold) {
        radix_sort_lsd(a, n);
        if (std::is_same<Cmp, Greater>::value) detail::reverse_n(a, n);
        return;
    }
    pdq_sort(a, n, comp);
}

template <typename T, typename Cmp>
inline typename std::enable_if<!radix_eligible<T, Cmp>::value>::type
fast_sort(T* a, std::size_t n, Cmp comp) { pdq_sort(a, n, comp); }

template <typename T, typename Cmp>
inline typename std::enable_if<radix_eligible<T, Cmp>::value>::type
fast_stable_sort(T* a, std::size_t n, Cmp comp) {
    if (n >= kRadixSortThreshold && !std::is_floating_point<T>::value && std::is_same<Cmp, Less>::value) { radix_sort_lsd(a, n); return; }
    stable_sort(a, n, comp);
}

template <typename T, typename Cmp>
inline typename std::enable_if<!radix_eligible<T, Cmp>::value>::type
fast_stable_sort(T* a, std::size_t n, Cmp comp) { stable_sort(a, n, comp); }

} // namespace algo
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_ALGORITHMS_SORT_INTEGER_HPP
