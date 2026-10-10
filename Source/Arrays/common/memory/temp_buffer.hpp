#ifndef FIZMO_ARRAYS_MEMORY_TEMP_BUFFER_HPP
#define FIZMO_ARRAYS_MEMORY_TEMP_BUFFER_HPP

#include "allocator.hpp"
#include "element_ops.hpp"

namespace fizmo {
namespace arrays {
namespace detail {

template <typename T, std::size_t LocalBytes = 1024>
class TempBuffer {
    static constexpr std::size_t kLocal = LocalBytes / sizeof(T) > 0 ? LocalBytes / sizeof(T) : 1;

    alignas(alignof(T) > alignof(std::max_align_t) ? alignof(T) : alignof(std::max_align_t)) unsigned char m_local[kLocal * sizeof(T)];
    T*          m_ptr;
    std::size_t m_capacity;
    bool        m_heap;

public:
    explicit TempBuffer(std::size_t n) : m_ptr(reinterpret_cast<T*>(m_local)), m_capacity(kLocal), m_heap(false) {
        if (n > kLocal) {
            memory::Block<T> b = memory::allocate<T>(n);
            m_ptr = b.data;
            m_capacity = b.capacity;
            m_heap = true;
        }
    }

    TempBuffer(const TempBuffer&) = delete;
    TempBuffer& operator=(const TempBuffer&) = delete;

    ~TempBuffer() { if (m_heap) memory::deallocate(m_ptr, m_capacity); }

    T* data() noexcept { return m_ptr; }
    std::size_t capacity() const noexcept { return m_capacity; }
};

template <typename T>
class ConstructedBuffer {
    TempBuffer<T> m_raw;
    std::size_t   m_count = 0;

public:
    explicit ConstructedBuffer(std::size_t n) : m_raw(n) {}
    ~ConstructedBuffer() { destroy_n(m_raw.data(), m_count); }

    ConstructedBuffer(const ConstructedBuffer&) = delete;
    ConstructedBuffer& operator=(const ConstructedBuffer&) = delete;

    T* data() noexcept { return m_raw.data(); }
    std::size_t size() const noexcept { return m_count; }

    template <typename... Args>
    void emplace(Args&&... args) { construct_element(m_raw.data() + m_count, static_cast<Args&&>(args)...); ++m_count; }

    void adopt_count(std::size_t n) noexcept { m_count = n; }
};

} // namespace detail
} // namespace arrays
} // namespace fizmo

#endif // FIZMO_ARRAYS_MEMORY_TEMP_BUFFER_HPP
