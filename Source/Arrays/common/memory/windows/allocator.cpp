#include "fizmo_library.hpp"

#if defined(OS_WINDOWS)

#include <cstring>
#include <malloc.h>
#include <new>

namespace fizmo {
namespace arrays {
namespace memory {
namespace platform {

std::size_t page_size() noexcept {
    static const std::size_t size = [] {
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : std::size_t(4096);
    }();
    return size;
}

inline std::size_t granularity() noexcept {
    static const std::size_t size = [] {
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwAllocationGranularity ? static_cast<std::size_t>(info.dwAllocationGranularity) : std::size_t(65536);
    }();
    return size;
}

inline std::size_t round_to_pages(std::size_t bytes) noexcept {
    const std::size_t p = page_size();
    return (bytes + p - 1) & ~(p - 1);
}

inline std::size_t reservation_for(std::size_t bytes) noexcept {
    const std::size_t g = granularity();
#if defined(_WIN64)
    std::size_t want = bytes < (std::size_t(1) << 40) ? bytes * 4 : bytes;
    if (want < std::size_t(16) * 1024 * 1024) want = std::size_t(16) * 1024 * 1024;
#else
    std::size_t want = bytes * 2;
#endif
    if (want < bytes) want = bytes;
    return (want + g - 1) & ~(g - 1);
}

inline HANDLE heap() noexcept {
    static const HANDLE h = ::GetProcessHeap();
    return h;
}

class RegionCache {
public:
    struct Entry {
        void*       ptr;
        std::size_t committed;
    };

    static constexpr std::size_t kSlots = 8;
    static constexpr std::size_t kMaxEntryBytes = std::size_t(64) * 1024 * 1024;
    static constexpr std::size_t kMaxTotalBytes = std::size_t(96) * 1024 * 1024;

    RegionCache() noexcept = default;
    RegionCache(const RegionCache&) = delete;
    RegionCache& operator=(const RegionCache&) = delete;
    ~RegionCache() { trim(); dead() = true; }

    static bool& dead() noexcept {
        static thread_local bool flag = false;
        return flag;
    }

    void* take(std::size_t bytes, std::size_t& got) noexcept {
        std::size_t best = kSlots;
        for (std::size_t i = 0; i < m_count; ++i) {
            const std::size_t b = m_entries[i].committed;
            if (b >= bytes && b <= bytes * 2 && (best == kSlots || b < m_entries[best].committed)) best = i;
        }
        if (best == kSlots) return nullptr;
        void* p = m_entries[best].ptr;
        got = m_entries[best].committed;
        m_total -= got;
        m_entries[best] = m_entries[--m_count];
        return p;
    }

    bool give(void* p, std::size_t committed) noexcept {
        if (committed > kMaxEntryBytes) return false;
        while (m_count > 0 && (m_count == kSlots || m_total + committed > kMaxTotalBytes)) {
            std::size_t largest = 0;
            for (std::size_t i = 1; i < m_count; ++i) if (m_entries[i].committed > m_entries[largest].committed) largest = i;
            ::VirtualFree(m_entries[largest].ptr, 0, MEM_RELEASE);
            m_total -= m_entries[largest].committed;
            m_entries[largest] = m_entries[--m_count];
        }
        if (m_total + committed > kMaxTotalBytes) return false;
        m_entries[m_count++] = Entry{ p, committed };
        m_total += committed;
        return true;
    }

    void trim() noexcept {
        for (std::size_t i = 0; i < m_count; ++i) ::VirtualFree(m_entries[i].ptr, 0, MEM_RELEASE);
        m_count = 0;
        m_total = 0;
    }

    std::size_t cached_bytes() const noexcept { return m_total; }

private:
    Entry       m_entries[kSlots] = {};
    std::size_t m_count = 0;
    std::size_t m_total = 0;
};

inline RegionCache* region_cache() noexcept {
    if (RegionCache::dead()) return nullptr;
    static thread_local RegionCache cache;
    return &cache;
}

inline bool is_region_kind(std::size_t bytes, std::size_t align) noexcept {
    return bytes >= kLargeAllocationBytes && align <= page_size();
}

inline std::size_t heap_alignment() noexcept {
#if defined(MEMORY_ALLOCATION_ALIGNMENT)
    return MEMORY_ALLOCATION_ALIGNMENT;
#else
    return 2 * sizeof(void*);
#endif
}

inline std::size_t reserved_after(void* base, std::size_t committed) noexcept {
    MEMORY_BASIC_INFORMATION mbi;
    char* next = static_cast<char*>(base) + committed;
    if (::VirtualQuery(next, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.AllocationBase != base || mbi.State != MEM_RESERVE) return 0;
    return static_cast<std::size_t>(mbi.RegionSize);
}

inline void* reserve_region(std::size_t bytes, std::size_t& got) {
    bytes = round_to_pages(bytes);
    if (RegionCache* c = region_cache()) {
        if (void* p = c->take(bytes, got)) return p;
    }
    void* base = ::VirtualAlloc(nullptr, reservation_for(bytes), MEM_RESERVE, PAGE_READWRITE);
    if (!base) base = ::VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE);
    if (!base) throw std::bad_alloc();
    if (!::VirtualAlloc(base, bytes, MEM_COMMIT, PAGE_READWRITE)) {
        ::VirtualFree(base, 0, MEM_RELEASE);
        throw std::bad_alloc();
    }
    got = bytes;
    return base;
}

inline bool grow_region(void* base, std::size_t committed, std::size_t want) noexcept {
    if (want <= committed) return true;
    const std::size_t more = want - committed;
    if (reserved_after(base, committed) < more) return false;
    return ::VirtualAlloc(static_cast<char*>(base) + committed, more, MEM_COMMIT, PAGE_READWRITE) != nullptr;
}

inline void release_region(void* base, std::size_t committed) noexcept {
    if (RegionCache* c = region_cache()) {
        if (c->give(base, committed)) return;
    }
    ::VirtualFree(base, 0, MEM_RELEASE);
}

inline std::size_t clamp_small_usable(std::size_t usable, std::size_t align) noexcept {
    if (align <= page_size() && usable >= kLargeAllocationBytes) return kLargeAllocationBytes - 1;
    return usable;
}

void* allocate(std::size_t bytes, std::size_t align, std::size_t& usable) {
    if (bytes == 0) bytes = 1;
    if (is_region_kind(bytes, align)) return reserve_region(bytes, usable);
    if (align <= heap_alignment()) {
        void* p = ::HeapAlloc(heap(), 0, bytes);
        if (!p) throw std::bad_alloc();
        const SIZE_T s = ::HeapSize(heap(), 0, p);
        usable = clamp_small_usable(s == static_cast<SIZE_T>(-1) ? bytes : static_cast<std::size_t>(s), align);
        return p;
    }
    void* p = ::_aligned_malloc(bytes, align);
    if (!p) throw std::bad_alloc();
    usable = clamp_small_usable(bytes, align);
    return p;
}

void deallocate(void* p, std::size_t bytes, std::size_t align) noexcept {
    if (!p) return;
    if (is_region_kind(bytes, align)) release_region(p, round_to_pages(bytes));
    else if (align <= heap_alignment()) ::HeapFree(heap(), 0, p);
    else ::_aligned_free(p);
}

void* reallocate(void* p, std::size_t old_bytes, std::size_t used_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) {
    if (!p) return allocate(new_bytes, align, usable);
    if (new_bytes == 0) new_bytes = 1;
    const bool old_region = is_region_kind(old_bytes, align);
    const bool new_region = is_region_kind(new_bytes, align);
    if (old_region) old_bytes = round_to_pages(old_bytes);
    if (old_region && new_region) {
        const std::size_t want = round_to_pages(new_bytes);
        if (want <= old_bytes) {
            if (want < old_bytes) ::VirtualFree(static_cast<char*>(p) + want, old_bytes - want, MEM_DECOMMIT);
            usable = want;
            return p;
        }
        if (RegionCache* c = region_cache()) {
            std::size_t got = 0;
            if (void* q = c->take(want, got)) {
                std::memcpy(q, p, used_bytes < got ? used_bytes : got);
                release_region(p, old_bytes);
                usable = got;
                return q;
            }
        }
        if (grow_region(p, old_bytes, want)) { usable = want; return p; }
    } else if (!old_region && !new_region && align <= heap_alignment()) {
        void* q = ::HeapReAlloc(heap(), 0, p, new_bytes);
        if (!q) throw std::bad_alloc();
        const SIZE_T s = ::HeapSize(heap(), 0, q);
        usable = clamp_small_usable(s == static_cast<SIZE_T>(-1) ? new_bytes : static_cast<std::size_t>(s), align);
        return q;
    }
    void* q = allocate(new_bytes, align, usable);
    std::memcpy(q, p, used_bytes < usable ? used_bytes : usable);
    deallocate(p, old_bytes, align);
    return q;
}

bool try_expand(void* p, std::size_t old_bytes, std::size_t new_bytes, std::size_t align, std::size_t& usable) noexcept {
    if (!p) return false;
    if (is_region_kind(old_bytes, align)) {
        old_bytes = round_to_pages(old_bytes);
        const std::size_t want = round_to_pages(new_bytes);
        if (want <= old_bytes) { usable = old_bytes; return true; }
        if (!grow_region(p, old_bytes, want)) return false;
        usable = want;
        return true;
    }
    if (align > heap_alignment() || is_region_kind(new_bytes, align)) return false;
    void* q = ::HeapReAlloc(heap(), HEAP_REALLOC_IN_PLACE_ONLY, p, new_bytes);
    if (q != p) return false;
    const SIZE_T s = ::HeapSize(heap(), 0, p);
    usable = clamp_small_usable(s == static_cast<SIZE_T>(-1) ? new_bytes : static_cast<std::size_t>(s), align);
    return true;
}

void trim_cache() noexcept {
    if (RegionCache* c = region_cache()) c->trim();
}

std::size_t cached_bytes() noexcept {
    RegionCache* c = region_cache();
    return c ? c->cached_bytes() : 0;
}

} // namespace platform
} // namespace memory
} // namespace arrays
} // namespace fizmo

#endif
