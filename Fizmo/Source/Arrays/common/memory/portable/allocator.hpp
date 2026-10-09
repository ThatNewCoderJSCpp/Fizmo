#ifndef FIZMO_ARRAYS_MEMORY_PORTABLE_ALLOCATOR_HPP
#define FIZMO_ARRAYS_MEMORY_PORTABLE_ALLOCATOR_HPP

#include "../../config.hpp"
#include <cstring>
#include <new>

namespace fizmo {
namespace arrays {
namespace memory {
namespace platform {

inline std::size_t page_size() noexcept { return 4096; }

inline void* allocate(std::size_t bytes, std::size_t align, std::size_t& usable) {
    if (bytes == 0) bytes = 1;
    usable = bytes;
    if (align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) return ::operator new(bytes);
    return ::operator new(bytes, std::align_val_t(align));
}

inline void deallocate(void* p, std::size_t, std::size_t align) noexcept {
    if (!p) return;
    if (align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) ::operator delete(p);
    else ::operator delete(p, std::align_val_t(align));
}

inline void* reallocate(void* p, std::size_t old_bytes, std::size_t used_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) {
    void* q = allocate(new_bytes, align, usable);
    if (p) {
        std::memcpy(q, p, used_bytes < usable ? used_bytes : usable);
        deallocate(p, old_bytes, align);
    }
    return q;
}

inline bool try_expand(void*, std::size_t, std::size_t, std::size_t, std::size_t&) noexcept { return false; }
inline void trim_cache() noexcept {}
inline std::size_t cached_bytes() noexcept { return 0; }
inline void prefetch_write(void*, std::size_t) noexcept {}
inline const char* name() noexcept { return "portable"; }

} // namespace platform
} // namespace memory
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_PORTABLE_ALLOCATOR_HPP
