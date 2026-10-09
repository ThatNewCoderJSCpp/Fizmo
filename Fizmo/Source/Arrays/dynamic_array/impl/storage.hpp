#ifndef FIZMO_DYNAMIC_ARRAY_STORAGE_HPP
#define FIZMO_DYNAMIC_ARRAY_STORAGE_HPP

#include "../../common/memory/allocator.hpp"
#include "../../common/memory/element_ops.hpp"

namespace fizmo {
namespace arrays {
namespace detail {

template <typename T>
class HeapStorage {
protected:
    T*          m_data = nullptr;
    std::size_t m_size = 0;
    std::size_t m_capacity = 0;

    using ET = ElementTraits<T>;

    HeapStorage() noexcept = default;
    HeapStorage(const HeapStorage&) = delete;
    HeapStorage& operator=(const HeapStorage&) = delete;
    ~HeapStorage() { fz_free(); }

    bool fz_owns_heap() const noexcept { return m_data != nullptr; }
    bool fz_is_inline() const noexcept { return false; }

    void fz_clear() noexcept {
        destroy_n(m_data, m_size);
        m_size = 0;
    }

    void fz_free() noexcept {
        destroy_n(m_data, m_size);
        if (m_data) memory::deallocate(m_data, m_capacity);
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }

    void fz_realloc(std::size_t want) {
        if (want < m_size) want = m_size;
        if (want == 0) {
            if (m_data) memory::deallocate(m_data, m_capacity);
            m_data = nullptr;
            m_capacity = 0;
            return;
        }
        if (ET::relocatable) {
            const memory::Block<T> b = memory::reallocate(m_data, m_capacity, m_size, want);
            m_data = b.data;
            m_capacity = b.capacity;
            return;
        }
        if (m_data && want > m_capacity) {
            std::size_t got = 0;
            if (memory::try_expand(m_data, m_capacity, want, got)) { m_capacity = got; return; }
        }
        const memory::Block<T> b = memory::allocate<T>(want);
#if defined(FIZMO_ARRAYS_EXCEPTIONS)
        try { relocate_or_copy_n(b.data, m_data, m_size); }
        catch (...) { memory::deallocate(b.data, b.capacity); throw; }
#else
        relocate_or_copy_n(b.data, m_data, m_size);
#endif
        if (m_data) memory::deallocate(m_data, m_capacity);
        m_data = b.data;
        m_capacity = b.capacity;
    }

    void fz_reserve_total(std::size_t need) { if (need > m_capacity) fz_realloc(memory::grow_capacity<T>(m_capacity, need)); }
    void fz_reserve_exact(std::size_t need) { if (need > m_capacity) fz_realloc(need); }

    bool fz_shrink() {
        if (m_capacity == m_size) return false;
        fz_realloc(m_size);
        return true;
    }

    void fz_steal(HeapStorage& o) noexcept {
        m_data = o.m_data;
        m_size = o.m_size;
        m_capacity = o.m_capacity;
        o.m_data = nullptr;
        o.m_size = 0;
        o.m_capacity = 0;
    }

    void fz_swap(HeapStorage& o) noexcept {
        T* d = m_data; m_data = o.m_data; o.m_data = d;
        std::size_t s = m_size; m_size = o.m_size; o.m_size = s;
        std::size_t c = m_capacity; m_capacity = o.m_capacity; o.m_capacity = c;
    }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_DYNAMIC_ARRAY_STORAGE_HPP
