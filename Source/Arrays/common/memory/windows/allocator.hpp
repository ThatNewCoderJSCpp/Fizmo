#ifndef FIZMO_ARRAYS_MEMORY_WINDOWS_ALLOCATOR_HPP
#define FIZMO_ARRAYS_MEMORY_WINDOWS_ALLOCATOR_HPP

#include "../../config.hpp"
#include "../../../../Basic/fizmo_defines.hpp"
#include <cstddef>

namespace fizmo {
namespace arrays {
namespace memory {
namespace platform {

std::size_t page_size() noexcept;
void* allocate(std::size_t bytes, std::size_t align, std::size_t& usable);
void deallocate(void* p, std::size_t bytes, std::size_t align) noexcept;
void* reallocate(void* p, std::size_t old_bytes, std::size_t used_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable);
bool try_expand(void* p, std::size_t old_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) noexcept;
void trim_cache() noexcept;
std::size_t cached_bytes() noexcept;

inline void prefetch_write(void*, std::size_t) noexcept {}

inline const char* name() noexcept { return "windows"; }

} // namespace platform
} // namespace memory
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_WINDOWS_ALLOCATOR_HPP
