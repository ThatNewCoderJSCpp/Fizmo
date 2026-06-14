#ifndef FIZMO_NETWORK_BUFFER_HPP
#define FIZMO_NETWORK_BUFFER_HPP

#include "tcp_socket.hpp"
#include "udp_socket.hpp"
#include <mutex>

namespace fizmo {
namespace networking {
namespace core {

class Buffer {
private:
    mutable std::mutex m_mutex;
    std::unique_ptr<std::uint8_t[]> m_data;
    std::size_t m_size;
    std::size_t m_capacity;
    std::size_t m_read_pos;
    std::size_t m_write_pos;

public:
    Buffer(const std::size_t initial_capacity = 1024)
        : m_size(0), m_capacity(initial_capacity), m_read_pos(0), m_write_pos(0) {
        m_data = std::make_unique<std::uint8_t[]>(m_capacity);
    }

    Buffer(const Buffer& other) {
        std::lock_guard<std::mutex> lock_other(other.m_mutex);
        m_size      = other.m_size;
        m_capacity  = other.m_capacity;
        m_read_pos  = other.m_read_pos;
        m_write_pos = other.m_write_pos;
        m_data = std::make_unique<std::uint8_t[]>(m_capacity);
        std::memcpy(m_data.get(), other.m_data.get(), m_size);
    }

    Buffer(Buffer&& other) noexcept
        : m_size(0), m_capacity(0), m_read_pos(0), m_write_pos(0) {
        std::lock_guard<std::mutex> lock_other(other.m_mutex);
        m_data      = std::move(other.m_data);
        m_size      = other.m_size;
        m_capacity  = other.m_capacity;
        m_read_pos  = other.m_read_pos;
        m_write_pos = other.m_write_pos;
        other.m_size = 0;
        other.m_capacity = 0;
        other.m_read_pos = 0;
        other.m_write_pos = 0;
    }

    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            std::unique_lock<std::mutex> lk1(m_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(other.m_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            if (m_capacity < other.m_size) {
                m_data = std::make_unique<std::uint8_t[]>(other.m_capacity);
                m_capacity = other.m_capacity;
            }
            m_size      = other.m_size;
            m_read_pos  = other.m_read_pos;
            m_write_pos = other.m_write_pos;
            std::memcpy(m_data.get(), other.m_data.get(), m_size);
        }
        return *this;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            std::unique_lock<std::mutex> lk1(m_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(other.m_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            m_data      = std::move(other.m_data);
            m_size      = other.m_size;
            m_capacity  = other.m_capacity;
            m_read_pos  = other.m_read_pos;
            m_write_pos = other.m_write_pos;
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_read_pos = 0;
            other.m_write_pos = 0;
        }
        return *this;
    }

    ~Buffer() = default;

public:
    std::size_t size() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_size; }
    std::size_t capacity() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity; }
    std::size_t available_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity - m_size; }
    std::size_t readable_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_write_pos - m_read_pos; }
    std::size_t writable_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity - m_write_pos; }
    bool empty() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_size == 0; }

    void reserve(const std::size_t cap) { std::lock_guard<std::mutex> l(m_mutex); reserve_internal(cap); }

    void resize(const std::size_t new_size) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (new_size > m_capacity) { reserve_internal(new_size + 1); }
        m_size = new_size;
        m_write_pos = std::max(m_write_pos, m_size);
    }

    void clear() noexcept { std::lock_guard<std::mutex> l(m_mutex); m_size = 0; m_read_pos = 0; m_write_pos = 0; }

    void compact() {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos > 0) {
            if (m_read_pos < m_write_pos) { std::memmove(m_data.get(), m_data.get() + m_read_pos, m_write_pos - m_read_pos); }
            m_write_pos -= m_read_pos;
            m_size      -= m_read_pos;
            m_read_pos   = 0;
        }
    }

    std::uint8_t* data_unsafe() noexcept { return m_data.get(); }
    const std::uint8_t* data_unsafe() const noexcept { return m_data.get(); }
    std::uint8_t* read_ptr() noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_read_pos; }
    const std::uint8_t* read_ptr() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_read_pos; }
    std::uint8_t* write_ptr() noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_write_pos; }
    const std::uint8_t* write_ptr() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_write_pos; }
    std::size_t read_position() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_read_pos; }
    std::size_t write_position() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_write_pos; }

    void set_read_position(const std::size_t pos) noexcept { std::lock_guard<std::mutex> l(m_mutex); m_read_pos = std::min(pos, m_size); }

    void set_write_position(const std::size_t pos) noexcept {
        std::lock_guard<std::mutex> l(m_mutex);
        m_write_pos = std::min(pos, m_capacity);
        m_size = std::max(m_size, m_write_pos);
    }

    void advance_read_position(const std::size_t bytes) noexcept {
        std::lock_guard<std::mutex> l(m_mutex);
        m_read_pos = std::min(m_read_pos + bytes, m_size);
    }

    void advance_write_position(const std::size_t bytes) noexcept {
        std::lock_guard<std::mutex> l(m_mutex);
        std::size_t nw = m_write_pos + bytes;
        if (nw > m_capacity) { reserve_internal(std::max(nw + 1, m_capacity * 2)); }
        m_write_pos = nw;
        m_size = std::max(m_size, m_write_pos);
    }

    void write(const void* data, const std::size_t length) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (length == 0) return;
        if (m_write_pos + length > m_capacity) { reserve_internal(std::max(m_write_pos + length + 1, m_capacity * 2)); }
        std::memcpy(m_data.get() + m_write_pos, data, length);
        m_write_pos += length;
        m_size = std::max(m_size, m_write_pos);
    }

    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        write(&value, sizeof(T));
    }

    void write_string(const std::string& str) { write(str.c_str(), str.size()); }

    bool read(void* dest, const std::size_t length) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos + length > m_write_pos) return false;
        std::memcpy(dest, m_data.get() + m_read_pos, length);
        m_read_pos += length;
        return true;
    }

    template<typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        return read(&value, sizeof(T));
    }

    bool read_string(std::string& str, const std::size_t length) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos + length > m_write_pos) return false;
        str.assign(reinterpret_cast<const char*>(m_data.get() + m_read_pos), length);
        m_read_pos += length;
        return true;
    }

    int receive_from_socket(TCPSocket& socket, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive(m_data.get() + m_write_pos, max_length);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

    int send_to_socket(TCPSocket& socket, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        if (max_length == 0 || max_length > m_write_pos - m_read_pos) { max_length = m_write_pos - m_read_pos; }
        int n = socket.send(m_data.get() + m_read_pos, max_length);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

    int receive_from_socket(UDPSocket& socket, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive_connected(m_data.get() + m_write_pos, max_length);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

    int send_to_socket(UDPSocket& socket, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        if (max_length == 0 || max_length > m_write_pos - m_read_pos) { max_length = m_write_pos - m_read_pos; }
        int n = socket.send_connected(m_data.get() + m_read_pos, max_length);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

    int sendto_socket(UDPSocket& socket, const NetworkAddress& dest, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        std::size_t avail = m_write_pos - m_read_pos;
        if (max_length == 0 || max_length > avail) { max_length = avail; }
        int n = socket.send(m_data.get() + m_read_pos, max_length, dest);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

    int recvfrom_socket(UDPSocket& socket, NetworkAddress& source, std::size_t max_length = 0) {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive(m_data.get() + m_write_pos, max_length, source);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

private:
    void reserve_internal(const std::size_t new_capacity) {
        if (new_capacity <= m_capacity) return;
        auto nd = std::make_unique<std::uint8_t[]>(new_capacity);
        std::memcpy(nd.get(), m_data.get(), m_size);
        m_data = std::move(nd);
        m_capacity = new_capacity;
    }
};

inline int TCPSocket::send_buffer(Buffer& buffer, const std::size_t max_length) noexcept { return buffer.send_to_socket(*this, max_length); }
inline int TCPSocket::receive_to_buffer(Buffer& buffer, const std::size_t max_length) noexcept { return buffer.receive_from_socket(*this, max_length); }

inline int UDPSocket::send_buffer_connected(Buffer& buffer, const std::size_t max_length) noexcept { return buffer.send_to_socket(*this, max_length); }
inline int UDPSocket::receive_to_buffer_connected(Buffer& buffer, const std::size_t max_length) noexcept { return buffer.receive_from_socket(*this, max_length); }
inline int UDPSocket::send_buffer(Buffer& buffer, const NetworkAddress& dest, const std::size_t max_length) noexcept { return buffer.sendto_socket(*this, dest, max_length); }
inline int UDPSocket::receive_to_buffer(Buffer& buffer, NetworkAddress& source, const std::size_t max_length) noexcept { return buffer.recvfrom_socket(*this, source, max_length); }

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NETWORK_BUFFER_HPP