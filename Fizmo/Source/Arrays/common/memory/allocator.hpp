#ifndef FIZMO_ARRAYS_MEMORY_ALLOCATOR_HPP
#define FIZMO_ARRAYS_MEMORY_ALLOCATOR_HPP

#include "../config.hpp"

#if defined(FIZMO_ARRAYS_FORCE_PORTABLE)
    #include "portable/allocator.hpp"
#elif defined(FIZMO_ARRAYS_LINUX)
    #include "linux/allocator.hpp"
#elif defined(FIZMO_ARRAYS_WINDOWS)
    #include "windows/allocator.hpp"
#else
    #include "portable/allocator.hpp"
#endif

#include <limits>
#include <new>

namespace fizmo {
namespace arrays {
namespace memory {

inline const char* backend_name() noexcept { return platform::name(); }
inline std::size_t page_size() noexcept { return platform::page_size(); }
inline void trim_cache() noexcept { platform::trim_cache(); }
inline std::size_t cached_bytes() noexcept { return platform::cached_bytes(); }

template <typename T>
struct Block {
    T*          data = nullptr;
    std::size_t capacity = 0;
};

template <typename T>
inline std::size_t max_elements() noexcept { return std::numeric_limits<std::size_t>::max() / (sizeof(T) * 2); }

template <typename T>
inline std::size_t bytes_for(std::size_t n) {
    if (n > max_elements<T>()) throw std::bad_alloc();
    return n * sizeof(T);
}

template <typename T>
inline Block<T> allocate(std::size_t n) {
    std::size_t usable = 0;
    void* p = platform::allocate(bytes_for<T>(n), alignof(T), usable);
    return Block<T>{ static_cast<T*>(p), usable / sizeof(T) };
}

template <typename T>
inline void deallocate(T* p, std::size_t capacity) noexcept {
    platform::deallocate(p, capacity * sizeof(T), alignof(T));
}

template <typename T>
inline Block<T> reallocate(T* p, std::size_t old_capacity, std::size_t used, std::size_t new_capacity) {
    std::size_t usable = 0;
    void* q = platform::reallocate(p, old_capacity * sizeof(T), used * sizeof(T), bytes_for<T>(new_capacity), alignof(T), usable);
    return Block<T>{ static_cast<T*>(q), usable / sizeof(T) };
}

template <typename T>
inline bool try_expand(T* p, std::size_t old_capacity, std::size_t new_capacity, std::size_t& got) noexcept {
    if (new_capacity > max_elements<T>()) return false;
    std::size_t usable = 0;
    if (!platform::try_expand(p, old_capacity * sizeof(T), new_capacity * sizeof(T), alignof(T), usable)) return false;
    got = usable / sizeof(T);
    return got >= new_capacity;
}

template <typename T>
inline void prefetch_write(T* p, std::size_t n) noexcept { platform::prefetch_write(p, n * sizeof(T)); }

template <typename T>
inline std::size_t grow_capacity(std::size_t current, std::size_t needed) noexcept {
    const std::size_t min_elems = sizeof(T) >= kMinAllocationBytes ? 1 : kMinAllocationBytes / sizeof(T);
    std::size_t grown = current + current / 2;
    if (grown < current) grown = needed;
    if (grown < needed) grown = needed;
    if (grown < min_elems) grown = min_elems;
    return grown;
}

} // namespace memory
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_ALLOCATOR_HPP
