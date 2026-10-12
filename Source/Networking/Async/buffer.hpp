#ifndef FIZMO_ASYNC_BUFFER_HPP
#define FIZMO_ASYNC_BUFFER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace networking {
namespace async {

class MutableBuffer {
public:
    MutableBuffer() noexcept = default;
    MutableBuffer(void* data, std::size_t size) noexcept : m_data(data), m_size(size) {}

    void*       data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }

    MutableBuffer operator+(std::size_t n) const noexcept {
        const std::size_t off = n < m_size ? n : m_size;
        return MutableBuffer(static_cast<char*>(m_data) + off, m_size - off);
    }

    MutableBuffer& operator+=(std::size_t n) noexcept { return *this = *this + n; }

private:
    void*       m_data = nullptr;
    std::size_t m_size = 0;
};

class ConstBuffer {
public:
    ConstBuffer() noexcept = default;
    ConstBuffer(const void* data, std::size_t size) noexcept : m_data(data), m_size(size) {}
    ConstBuffer(const MutableBuffer& b) noexcept : m_data(b.data()), m_size(b.size()) {}

    const void* data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }

    ConstBuffer operator+(std::size_t n) const noexcept {
        const std::size_t off = n < m_size ? n : m_size;
        return ConstBuffer(static_cast<const char*>(m_data) + off, m_size - off);
    }

    ConstBuffer& operator+=(std::size_t n) noexcept { return *this = *this + n; }

private:
    const void* m_data = nullptr;
    std::size_t m_size = 0;
};

inline MutableBuffer buffer(void* data, std::size_t size) noexcept { return MutableBuffer(data, size); }
inline ConstBuffer   buffer(const void* data, std::size_t size) noexcept { return ConstBuffer(data, size); }
inline MutableBuffer buffer(std::string& s) noexcept { return MutableBuffer(&s[0], s.size()); }
inline ConstBuffer   buffer(const std::string& s) noexcept { return ConstBuffer(s.data(), s.size()); }

template <typename T>
MutableBuffer buffer(std::vector<T>& v) noexcept { return MutableBuffer(v.data(), v.size() * sizeof(T)); }

template <typename T>
ConstBuffer buffer(const std::vector<T>& v) noexcept { return ConstBuffer(v.data(), v.size() * sizeof(T)); }

template <typename T, std::size_t N>
MutableBuffer buffer(std::array<T, N>& a) noexcept { return MutableBuffer(a.data(), N * sizeof(T)); }

template <typename T, std::size_t N>
ConstBuffer buffer(const std::array<T, N>& a) noexcept { return ConstBuffer(a.data(), N * sizeof(T)); }

template <typename T, std::size_t N>
MutableBuffer buffer(T (&a)[N]) noexcept { return MutableBuffer(a, N * sizeof(T)); }

template <typename T, std::size_t N>
ConstBuffer buffer(const T (&a)[N]) noexcept { return ConstBuffer(a, N * sizeof(T)); }

inline MutableBuffer buffer(const MutableBuffer& b, std::size_t max_size) noexcept {
    return MutableBuffer(b.data(), b.size() < max_size ? b.size() : max_size);
}

inline ConstBuffer buffer(const ConstBuffer& b, std::size_t max_size) noexcept {
    return ConstBuffer(b.data(), b.size() < max_size ? b.size() : max_size);
}

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_BUFFER_HPP
