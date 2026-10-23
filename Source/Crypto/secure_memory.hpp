#ifndef FIZMO_CRYPTO_SECURE_MEMORY_HPP
#define FIZMO_CRYPTO_SECURE_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace crypto {

void secure_zero(void* data, std::size_t len) noexcept;

bool secure_compare(const void* a, const void* b, std::size_t len) noexcept;

bool strequal(std::string_view a, std::string_view b) noexcept;

bool strequal(const char* a, const char* b) noexcept;

inline bool secure_equals(const std::string& a, const std::string& b) noexcept {
    return a.size() == b.size() && secure_compare(a.data(), b.data(), a.size());
}

inline bool secure_equals(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b) noexcept {
    return a.size() == b.size() && secure_compare(a.data(), b.data(), a.size());
}

bool lock_memory(void* data, std::size_t len) noexcept;

void unlock_memory(void* data, std::size_t len) noexcept;

template <typename T>
class SecureAllocator {
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    template <typename U>
    struct rebind { using other = SecureAllocator<U>; };

    SecureAllocator() noexcept = default;

    template <typename U>
    SecureAllocator(const SecureAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc();
        void* p = ::operator new(n * sizeof(T));
        lock_memory(p, n * sizeof(T));
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t n) noexcept {
        if (!p) return;
        secure_zero(p, n * sizeof(T));
        unlock_memory(p, n * sizeof(T));
        ::operator delete(p);
    }

    friend bool operator==(const SecureAllocator&, const SecureAllocator&) noexcept { return true; }
    friend bool operator!=(const SecureAllocator&, const SecureAllocator&) noexcept { return false; }
};

template <typename T = std::uint8_t>
using SecureVector = std::vector<T, SecureAllocator<T>>;

using SecureString = std::basic_string<char, std::char_traits<char>, SecureAllocator<char>>;

template <typename T = std::uint8_t>
class SecureBuffer {
public:
    SecureBuffer() = default;
    explicit SecureBuffer(std::size_t n) : m_data(n) {}
    SecureBuffer(const void* data, std::size_t n) : m_data(n) { if (n) std::memcpy(m_data.data(), data, n * sizeof(T)); }
    SecureBuffer(const SecureBuffer&) = default;
    SecureBuffer(SecureBuffer&&) noexcept = default;
    SecureBuffer& operator=(const SecureBuffer&) = default;
    SecureBuffer& operator=(SecureBuffer&&) noexcept = default;
    ~SecureBuffer() = default;

    T*          data()       noexcept { return m_data.data(); }
    const T*    data() const noexcept { return m_data.data(); }
    std::size_t size() const noexcept { return m_data.size(); }
    bool        empty() const noexcept { return m_data.empty(); }

    T&       operator[](std::size_t i)       noexcept { return m_data[i]; }
    const T& operator[](std::size_t i) const noexcept { return m_data[i]; }

    T*       begin()       noexcept { return m_data.data(); }
    T*       end()         noexcept { return m_data.data() + m_data.size(); }
    const T* begin() const noexcept { return m_data.data(); }
    const T* end()   const noexcept { return m_data.data() + m_data.size(); }

    void resize(std::size_t n) { m_data.resize(n); }
    void clear() noexcept { secure_zero(m_data.data(), m_data.size() * sizeof(T)); m_data.clear(); }
    void wipe() noexcept { secure_zero(m_data.data(), m_data.size() * sizeof(T)); }

    void append(const void* data, std::size_t n) {
        const auto* p = static_cast<const T*>(data);
        m_data.insert(m_data.end(), p, p + n);
    }

    bool equals(const SecureBuffer& o) const noexcept {
        return size() == o.size() && secure_compare(data(), o.data(), size() * sizeof(T));
    }

    std::vector<T> to_vector() const { return std::vector<T>(m_data.begin(), m_data.end()); }

private:
    SecureVector<T> m_data;
};

} // namespace crypto
} // namespace fizmo

#endif // FIZMO_CRYPTO_SECURE_MEMORY_HPP
