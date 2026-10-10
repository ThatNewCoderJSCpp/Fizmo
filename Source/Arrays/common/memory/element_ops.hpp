#ifndef FIZMO_ARRAYS_MEMORY_ELEMENT_OPS_HPP
#define FIZMO_ARRAYS_MEMORY_ELEMENT_OPS_HPP

#include "../traits.hpp"
#include <cstring>
#include <new>

namespace fizmo {
namespace arrays {
namespace detail {

template <typename T>
struct ElementTraits {
    static constexpr bool trivial_copy = std::is_trivially_copyable<T>::value;
    static constexpr bool trivial_dtor = std::is_trivially_destructible<T>::value;
    static constexpr bool relocatable = is_trivially_relocatable<T>::value;
    static constexpr bool zero_init = std::is_trivially_default_constructible<T>::value && std::is_trivially_copyable<T>::value;
    static constexpr bool bitwise_equal = std::is_integral<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value;
    static constexpr bool nothrow_move = std::is_nothrow_move_constructible<T>::value || !std::is_copy_constructible<T>::value;
};

template <typename T>
FIZMO_ARRAY_INLINE bool points_into(const T* p, const T* base, std::size_t n) noexcept {
    const std::uintptr_t a = reinterpret_cast<std::uintptr_t>(p);
    const std::uintptr_t b = reinterpret_cast<std::uintptr_t>(base);
    return a >= b && a < b + n * sizeof(T);
}

template <typename T>
FIZMO_ARRAY_INLINE void destroy_n(T* p, std::size_t n) noexcept {
    if (!ElementTraits<T>::trivial_dtor) for (std::size_t i = 0; i < n; ++i) p[i].~T();
}

template <typename T>
FIZMO_ARRAY_INLINE void destroy_element(T* p) noexcept {
    if (!ElementTraits<T>::trivial_dtor) p->~T();
}

template <typename T, typename... Args>
FIZMO_ARRAY_INLINE T* construct_element(T* p, Args&&... args) {
    return ::new (static_cast<void*>(p)) T(static_cast<Args&&>(args)...);
}

template <typename T>
struct ConstructGuard {
    T*          base;
    std::size_t done = 0;
    bool        active = true;
    explicit ConstructGuard(T* b) noexcept : base(b) {}
    ~ConstructGuard() { if (active) destroy_n(base, done); }
    void release() noexcept { active = false; }
};

template <typename T>
inline void construct_default_n(T* p, std::size_t n) {
    if (n == 0) return;
    if (ElementTraits<T>::zero_init) { std::memset(static_cast<void*>(p), 0, n * sizeof(T)); return; }
    ConstructGuard<T> g(p);
    for (; g.done < n; ++g.done) ::new (static_cast<void*>(p + g.done)) T();
    g.release();
}

template <typename T>
inline void construct_fill_n(T* p, std::size_t n, const T& value) {
    if (n == 0) return;
    if (ElementTraits<T>::trivial_copy && sizeof(T) == 1) {
        unsigned char b;
        std::memcpy(&b, &value, 1);
        std::memset(static_cast<void*>(p), b, n);
        return;
    }
    if (ElementTraits<T>::trivial_copy) {
        for (std::size_t i = 0; i < n; ++i) std::memcpy(static_cast<void*>(p + i), &value, sizeof(T));
        return;
    }
    ConstructGuard<T> g(p);
    for (; g.done < n; ++g.done) ::new (static_cast<void*>(p + g.done)) T(value);
    g.release();
}

template <typename T>
inline void construct_copy_n(T* FIZMO_ARRAY_RESTRICT dst, const T* FIZMO_ARRAY_RESTRICT src, std::size_t n) {
    if (n == 0) return;
    if (ElementTraits<T>::trivial_copy) { std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T)); return; }
    ConstructGuard<T> g(dst);
    for (; g.done < n; ++g.done) ::new (static_cast<void*>(dst + g.done)) T(src[g.done]);
    g.release();
}

template <typename T>
inline void construct_move_n(T* FIZMO_ARRAY_RESTRICT dst, T* FIZMO_ARRAY_RESTRICT src, std::size_t n) {
    if (n == 0) return;
    if (ElementTraits<T>::trivial_copy) { std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T)); return; }
    ConstructGuard<T> g(dst);
    for (; g.done < n; ++g.done) ::new (static_cast<void*>(dst + g.done)) T(static_cast<T&&>(src[g.done]));
    g.release();
}

template <typename T, typename It>
inline std::size_t construct_copy_range(T* dst, It first, It last) {
    ConstructGuard<T> g(dst);
    for (; !(first == last); ++first, ++g.done) ::new (static_cast<void*>(dst + g.done)) T(*first);
    g.release();
    return g.done;
}

template <typename T, typename It>
inline void construct_convert_n(T* dst, It first, std::size_t n) {
    ConstructGuard<T> g(dst);
    for (; g.done < n; ++g.done, ++first) ::new (static_cast<void*>(dst + g.done)) T(static_cast<T>(*first));
    g.release();
}

template <typename T>
inline void relocate_n(T* FIZMO_ARRAY_RESTRICT dst, T* FIZMO_ARRAY_RESTRICT src, std::size_t n) noexcept(ElementTraits<T>::relocatable || std::is_nothrow_move_constructible<T>::value) {
    if (n == 0) return;
    if (ElementTraits<T>::relocatable) { std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T)); return; }
    for (std::size_t i = 0; i < n; ++i) {
        ::new (static_cast<void*>(dst + i)) T(static_cast<T&&>(src[i]));
        src[i].~T();
    }
}

template <typename T>
inline void relocate_or_copy_n(T* dst, T* src, std::size_t n) {
    if (ElementTraits<T>::relocatable || std::is_nothrow_move_constructible<T>::value || !std::is_copy_constructible<T>::value) {
        relocate_n(dst, src, n);
        return;
    }
    construct_copy_n(dst, static_cast<const T*>(src), n);
    destroy_n(src, n);
}

template <typename T>
inline void open_gap(T* data, std::size_t size, std::size_t pos, std::size_t count) noexcept(ElementTraits<T>::relocatable || std::is_nothrow_move_constructible<T>::value) {
    if (count == 0 || pos >= size) return;
    if (ElementTraits<T>::relocatable) {
        std::memmove(static_cast<void*>(data + pos + count), static_cast<const void*>(data + pos), (size - pos) * sizeof(T));
        return;
    }
    for (std::size_t i = size; i > pos; --i) {
        ::new (static_cast<void*>(data + i - 1 + count)) T(static_cast<T&&>(data[i - 1]));
        data[i - 1].~T();
    }
}

template <typename T>
inline void close_gap(T* data, std::size_t size, std::size_t pos, std::size_t count) noexcept(ElementTraits<T>::relocatable || std::is_nothrow_move_constructible<T>::value) {
    if (count == 0 || pos + count >= size) return;
    if (ElementTraits<T>::relocatable) {
        std::memmove(static_cast<void*>(data + pos), static_cast<const void*>(data + pos + count), (size - pos - count) * sizeof(T));
        return;
    }
    for (std::size_t i = pos + count; i < size; ++i) {
        ::new (static_cast<void*>(data + i - count)) T(static_cast<T&&>(data[i]));
        data[i].~T();
    }
}

template <typename T>
inline void copy_assign_n(T* dst, const T* src, std::size_t n) {
    if (n == 0 || dst == src) return;
    if (ElementTraits<T>::trivial_copy) { std::memmove(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T)); return; }
    if (dst < src || dst >= src + n) { for (std::size_t i = 0; i < n; ++i) dst[i] = src[i]; }
    else { for (std::size_t i = n; i > 0; --i) dst[i - 1] = src[i - 1]; }
}

template <typename T>
inline void move_assign_n(T* dst, T* src, std::size_t n) {
    if (n == 0 || dst == src) return;
    if (ElementTraits<T>::trivial_copy) { std::memmove(static_cast<void*>(dst), static_cast<const void*>(src), n * sizeof(T)); return; }
    if (dst < src || dst >= src + n) { for (std::size_t i = 0; i < n; ++i) dst[i] = static_cast<T&&>(src[i]); }
    else { for (std::size_t i = n; i > 0; --i) dst[i - 1] = static_cast<T&&>(src[i - 1]); }
}

template <typename T, typename U>
inline void fill_assign_n(T* dst, std::size_t n, const U& value) {
    if (n == 0) return;
    if (ElementTraits<T>::trivial_copy && sizeof(T) == 1 && std::is_same<T, U>::value) {
        unsigned char b;
        std::memcpy(&b, &value, 1);
        std::memset(static_cast<void*>(dst), b, n);
        return;
    }
    for (std::size_t i = 0; i < n; ++i) dst[i] = value;
}

template <typename T>
inline bool equal_n(const T* a, const T* b, std::size_t n) {
    if (a == b || n == 0) return true;
    if (ElementTraits<T>::bitwise_equal) return std::memcmp(a, b, n * sizeof(T)) == 0;
    for (std::size_t i = 0; i < n; ++i) if (!(a[i] == b[i])) return false;
    return true;
}

template <typename T>
inline void swap_ranges_n(T* a, T* b, std::size_t n) {
    if (n == 0 || a == b) return;
    if (ElementTraits<T>::trivial_copy && (a + n <= b || b + n <= a)) {
        unsigned char tmp[256];
        const std::size_t bytes = n * sizeof(T);
        unsigned char* pa = reinterpret_cast<unsigned char*>(a);
        unsigned char* pb = reinterpret_cast<unsigned char*>(b);
        for (std::size_t off = 0; off < bytes; off += sizeof(tmp)) {
            const std::size_t c = bytes - off < sizeof(tmp) ? bytes - off : sizeof(tmp);
            std::memcpy(tmp, pa + off, c);
            std::memcpy(pa + off, pb + off, c);
            std::memcpy(pb + off, tmp, c);
        }
        return;
    }
    for (std::size_t i = 0; i < n; ++i) swap_values(a[i], b[i]);
}

template <typename T>
inline void reverse_n(T* p, std::size_t n) {
    if (n < 2) return;
    T* lo = p;
    T* hi = p + n - 1;
    while (lo < hi) { swap_values(*lo, *hi); ++lo; --hi; }
}

template <typename T>
inline void rotate_left_n(T* p, std::size_t n, std::size_t k) {
    if (n < 2) return;
    k %= n;
    if (k == 0) return;
    reverse_n(p, k);
    reverse_n(p + k, n - k);
    reverse_n(p, n);
}

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_ELEMENT_OPS_HPP
