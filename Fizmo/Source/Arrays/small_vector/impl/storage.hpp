#ifndef FIZMO_SMALL_VECTOR_STORAGE_HPP
#define FIZMO_SMALL_VECTOR_STORAGE_HPP

#include "../../common/memory/allocator.hpp"
#include "../../common/memory/element_ops.hpp"

namespace fizmo {
namespace arrays {
namespace detail {

template <typename T, std::size_t N>
class InlineStorage {
protected:
    T*          m_data;
    std::size_t m_size = 0;
    std::size_t m_capacity = N;
    alignas(T) unsigned char m_inline[sizeof(T) * N];

    using ET = ElementTraits<T>;

    InlineStorage() noexcept : m_data(inline_ptr()) {}
    InlineStorage(const InlineStorage&) = delete;
    InlineStorage& operator=(const InlineStorage&) = delete;
    ~InlineStorage() { fz_free(); }

    T* inline_ptr() noexcept { return reinterpret_cast<T*>(m_inline); }
    const T* inline_ptr() const noexcept { return reinterpret_cast<const T*>(m_inline); }
    bool fz_is_inline() const noexcept { return m_data == inline_ptr(); }
    bool fz_owns_heap() const noexcept { return !fz_is_inline(); }

    void fz_clear() noexcept {
        destroy_n(m_data, m_size);
        m_size = 0;
    }

    void fz_free() noexcept {
        destroy_n(m_data, m_size);
        if (!fz_is_inline()) memory::deallocate(m_data, m_capacity);
        m_data = inline_ptr();
        m_size = 0;
        m_capacity = N;
    }

    void fz_realloc(std::size_t want) {
        if (want < m_size) want = m_size;
        if (want <= N) {
            if (fz_is_inline()) return;
            T* heap = m_data;
            const std::size_t cap = m_capacity;
            relocate_or_copy_n(inline_ptr(), heap, m_size);
            memory::deallocate(heap, cap);
            m_data = inline_ptr();
            m_capacity = N;
            return;
        }
        if (!fz_is_inline()) {
            if (ET::relocatable) {
                const memory::Block<T> b = memory::reallocate(m_data, m_capacity, m_size, want);
                m_data = b.data;
                m_capacity = b.capacity;
                return;
            }
            if (want > m_capacity) {
                std::size_t got = 0;
                if (memory::try_expand(m_data, m_capacity, want, got)) { m_capacity = got; return; }
            }
        }
        const memory::Block<T> b = memory::allocate<T>(want);
#if defined(FIZMO_ARRAYS_EXCEPTIONS)
        try { relocate_or_copy_n(b.data, m_data, m_size); }
        catch (...) { memory::deallocate(b.data, b.capacity); throw; }
#else
        relocate_or_copy_n(b.data, m_data, m_size);
#endif
        if (!fz_is_inline()) memory::deallocate(m_data, m_capacity);
        m_data = b.data;
        m_capacity = b.capacity;
    }

    void fz_reserve_total(std::size_t need) { if (need > m_capacity) fz_realloc(memory::grow_capacity<T>(m_capacity, need)); }
    void fz_reserve_exact(std::size_t need) { if (need > m_capacity) fz_realloc(need); }

    bool fz_shrink() {
        if (fz_is_inline()) return false;
        if (m_size > N && m_capacity == m_size) return false;
        fz_realloc(m_size);
        return true;
    }

    void fz_steal(InlineStorage& o) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) {
        if (o.fz_is_inline()) {
            relocate_or_copy_n(inline_ptr(), o.m_data, o.m_size);
            m_data = inline_ptr();
            m_capacity = N;
            m_size = o.m_size;
            o.m_size = 0;
            return;
        }
        m_data = o.m_data;
        m_size = o.m_size;
        m_capacity = o.m_capacity;
        o.m_data = o.inline_ptr();
        o.m_size = 0;
        o.m_capacity = N;
    }

    void fz_swap(InlineStorage& o) noexcept(ET::relocatable || std::is_nothrow_move_constructible<T>::value) {
        if (this == &o) return;
        if (!fz_is_inline() && !o.fz_is_inline()) {
            T* d = m_data; m_data = o.m_data; o.m_data = d;
            std::size_t s = m_size; m_size = o.m_size; o.m_size = s;
            std::size_t c = m_capacity; m_capacity = o.m_capacity; o.m_capacity = c;
            return;
        }
        InlineStorage tmp;
        tmp.fz_steal(*this);
        fz_steal(o);
        o.fz_steal(tmp);
    }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_SMALL_VECTOR_STORAGE_HPP
