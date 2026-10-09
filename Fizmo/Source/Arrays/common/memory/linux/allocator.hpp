#ifndef FIZMO_ARRAYS_MEMORY_LINUX_ALLOCATOR_HPP
#define FIZMO_ARRAYS_MEMORY_LINUX_ALLOCATOR_HPP

#include "../../config.hpp"
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

namespace fizmo {
namespace arrays {
namespace memory {
namespace platform {

inline std::size_t page_size() noexcept {
    static const std::size_t size = [] {
        const long p = ::sysconf(_SC_PAGESIZE);
        return p > 0 ? static_cast<std::size_t>(p) : std::size_t(4096);
    }();
    return size;
}

inline std::size_t round_to_pages(std::size_t bytes) noexcept {
    const std::size_t p = page_size();
    return (bytes + p - 1) & ~(p - 1);
}

class MappingCache {
public:
    struct Entry {
        void*       ptr;
        std::size_t bytes;
    };

    static constexpr std::size_t kSlots = 6;
    static constexpr std::size_t kMaxEntryBytes = std::size_t(512) * 1024 * 1024;
    static constexpr std::size_t kMaxTotalBytes = std::size_t(1024) * 1024 * 1024;

    MappingCache() noexcept = default;
    MappingCache(const MappingCache&) = delete;
    MappingCache& operator=(const MappingCache&) = delete;

    ~MappingCache() {
        trim();
        dead() = true;
    }

    static bool& dead() noexcept {
        static thread_local bool flag = false;
        return flag;
    }

    void* take(std::size_t bytes, std::size_t& got) noexcept {
        std::size_t best = kSlots;
        for (std::size_t i = 0; i < m_count; ++i) {
            const std::size_t b = m_entries[i].bytes;
            if (b >= bytes && b <= bytes * 2 && (best == kSlots || b < m_entries[best].bytes)) best = i;
        }
        if (best == kSlots) return nullptr;
        void* p = m_entries[best].ptr;
        got = m_entries[best].bytes;
        m_total -= got;
        m_entries[best] = m_entries[--m_count];
        return p;
    }

    bool give(void* p, std::size_t bytes) noexcept {
        if (bytes > kMaxEntryBytes) return false;
        while (m_count > 0 && (m_count == kSlots || m_total + bytes > kMaxTotalBytes)) {
            std::size_t largest = 0;
            for (std::size_t i = 1; i < m_count; ++i) if (m_entries[i].bytes > m_entries[largest].bytes) largest = i;
            ::munmap(m_entries[largest].ptr, m_entries[largest].bytes);
            m_total -= m_entries[largest].bytes;
            m_entries[largest] = m_entries[--m_count];
        }
        if (m_total + bytes > kMaxTotalBytes) return false;
#if defined(MADV_FREE)
        ::madvise(p, bytes, MADV_FREE);
#endif
        m_entries[m_count++] = Entry{ p, bytes };
        m_total += bytes;
        return true;
    }

    void trim() noexcept {
        for (std::size_t i = 0; i < m_count; ++i) ::munmap(m_entries[i].ptr, m_entries[i].bytes);
        m_count = 0;
        m_total = 0;
    }

    std::size_t cached_bytes() const noexcept { return m_total; }

private:
    Entry       m_entries[kSlots] = {};
    std::size_t m_count = 0;
    std::size_t m_total = 0;
};

inline MappingCache* mapping_cache() noexcept {
    if (MappingCache::dead()) return nullptr;
    static thread_local MappingCache cache;
    return &cache;
}

inline bool is_mapped_kind(std::size_t bytes, std::size_t align) noexcept {
    return bytes >= kLargeAllocationBytes && align <= page_size();
}

inline void advise_huge(void* p, std::size_t bytes) noexcept {
#if defined(MADV_HUGEPAGE)
    if (bytes >= kHugePageBytes) ::madvise(p, bytes, MADV_HUGEPAGE);
#else
    (void)p; (void)bytes;
#endif
}

inline void* map_pages(std::size_t bytes, std::size_t& got) {
    bytes = round_to_pages(bytes);
    if (MappingCache* c = mapping_cache()) {
        if (void* p = c->take(bytes, got)) return p;
    }
    void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) throw std::bad_alloc();
    advise_huge(p, bytes);
    got = bytes;
    return p;
}

inline void unmap_pages(void* p, std::size_t bytes) noexcept {
    if (MappingCache* c = mapping_cache()) {
        if (c->give(p, bytes)) return;
    }
    ::munmap(p, bytes);
}

inline std::size_t clamp_small_usable(std::size_t usable, std::size_t align) noexcept {
    if (align <= page_size() && usable >= kLargeAllocationBytes) return kLargeAllocationBytes - 1;
    return usable;
}

inline void* allocate(std::size_t bytes, std::size_t align, std::size_t& usable) {
    if (bytes == 0) bytes = 1;
    if (is_mapped_kind(bytes, align)) return map_pages(bytes, usable);
    void* p = nullptr;
    if (align <= alignof(std::max_align_t)) {
        p = std::malloc(bytes);
        if (!p) throw std::bad_alloc();
    } else {
        if (::posix_memalign(&p, align < sizeof(void*) ? sizeof(void*) : align, bytes) != 0 || !p) throw std::bad_alloc();
    }
    usable = clamp_small_usable(::malloc_usable_size(p), align);
    return p;
}

inline void deallocate(void* p, std::size_t bytes, std::size_t align) noexcept {
    if (!p) return;
    if (is_mapped_kind(bytes, align)) unmap_pages(p, round_to_pages(bytes));
    else std::free(p);
}

inline void* reallocate(void* p, std::size_t old_bytes, std::size_t used_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) {
    if (!p) return allocate(new_bytes, align, usable);
    if (new_bytes == 0) new_bytes = 1;
    const bool old_mapped = is_mapped_kind(old_bytes, align);
    const bool new_mapped = is_mapped_kind(new_bytes, align);
    if (old_mapped) old_bytes = round_to_pages(old_bytes);
    if (old_mapped && new_mapped) {
        const std::size_t want = round_to_pages(new_bytes);
        if (want == old_bytes) { usable = old_bytes; return p; }
        if (want > old_bytes) {
            if (MappingCache* c = mapping_cache()) {
                std::size_t got = 0;
                if (void* q = c->take(want, got)) {
                    std::memcpy(q, p, used_bytes < got ? used_bytes : got);
                    unmap_pages(p, old_bytes);
                    usable = got;
                    return q;
                }
            }
        }
        void* q = ::mremap(p, old_bytes, want, MREMAP_MAYMOVE);
        if (q == MAP_FAILED) throw std::bad_alloc();
        advise_huge(q, want);
        usable = want;
        return q;
    }
    if (!old_mapped && !new_mapped && align <= alignof(std::max_align_t)) {
        void* q = std::realloc(p, new_bytes);
        if (!q) throw std::bad_alloc();
        usable = clamp_small_usable(::malloc_usable_size(q), align);
        return q;
    }
    void* q = allocate(new_bytes, align, usable);
    std::memcpy(q, p, used_bytes < usable ? used_bytes : usable);
    deallocate(p, old_bytes, align);
    return q;
}

inline bool try_expand(void* p, std::size_t old_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) noexcept {
    if (!p || !is_mapped_kind(old_bytes, align)) return false;
    old_bytes = round_to_pages(old_bytes);
    const std::size_t want = round_to_pages(new_bytes);
    if (want <= old_bytes) { usable = old_bytes; return true; }
    void* q = ::mremap(p, old_bytes, want, 0);
    if (q == MAP_FAILED || q != p) return false;
    advise_huge(p, want);
    usable = want;
    return true;
}

inline void trim_cache() noexcept {
    if (MappingCache* c = mapping_cache()) c->trim();
}

inline std::size_t cached_bytes() noexcept {
    MappingCache* c = mapping_cache();
    return c ? c->cached_bytes() : 0;
}

inline void prefetch_write(void* p, std::size_t bytes) noexcept {
#if defined(MADV_WILLNEED)
    if (bytes >= kLargeAllocationBytes) {
        const std::size_t page = page_size();
        const std::uintptr_t a = reinterpret_cast<std::uintptr_t>(p) & ~static_cast<std::uintptr_t>(page - 1);
        ::madvise(reinterpret_cast<void*>(a), bytes + (reinterpret_cast<std::uintptr_t>(p) - a), MADV_WILLNEED);
    }
#else
    (void)p; (void)bytes;
#endif
}

inline const char* name() noexcept { return "linux"; }

} // namespace platform
} // namespace memory
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_LINUX_ALLOCATOR_HPP
