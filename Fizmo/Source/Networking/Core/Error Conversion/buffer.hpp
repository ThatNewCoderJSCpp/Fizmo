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
;

    Buffer(const Buffer& other);

    Buffer(Buffer&& other) noexcept
;

    Buffer& operator=(const Buffer& other);

    Buffer& operator=(Buffer&& other) noexcept;

    ~Buffer() = default;

public:
    std::size_t size() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_size; }
    std::size_t capacity() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity; }
    std::size_t available_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity - m_size; }
    std::size_t readable_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_write_pos - m_read_pos; }
    std::size_t writable_bytes() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_capacity - m_write_pos; }
    bool empty() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_size == 0; }

    void reserve(const std::size_t cap) { std::lock_guard<std::mutex> l(m_mutex); reserve_internal(cap); }

    void resize(const std::size_t new_size);

    void clear() noexcept { std::lock_guard<std::mutex> l(m_mutex); m_size = 0; m_read_pos = 0; m_write_pos = 0; }

    void compact();

    std::uint8_t* data_unsafe() noexcept { return m_data.get(); }
    const std::uint8_t* data_unsafe() const noexcept { return m_data.get(); }
    std::uint8_t* read_ptr() noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_read_pos; }
    const std::uint8_t* read_ptr() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_read_pos; }
    std::uint8_t* write_ptr() noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_write_pos; }
    const std::uint8_t* write_ptr() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_data.get() + m_write_pos; }
    std::size_t read_position() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_read_pos; }
    std::size_t write_position() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_write_pos; }

    void set_read_position(const std::size_t pos) noexcept { std::lock_guard<std::mutex> l(m_mutex); m_read_pos = std::min(pos, m_size); }

    void set_write_position(const std::size_t pos) noexcept;

    void advance_read_position(const std::size_t bytes) noexcept {
        std::lock_guard<std::mutex> l(m_mutex);
        m_read_pos = std::min(m_read_pos + bytes, m_size);
    }

    void advance_write_position(const std::size_t bytes) noexcept;

    void write(const void* data, const std::size_t length);

    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        write(&value, sizeof(T));
    }

    void write_string(const std::string& str) { write(str.c_str(), str.size()); }

    bool read(void* dest, const std::size_t length);

    template<typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "Type must be trivially copyable");
        return read(&value, sizeof(T));
    }

    bool read_string(std::string& str, const std::size_t length);

    int receive_from_socket(TCPSocket& socket, std::size_t max_length = 0);

    int send_to_socket(TCPSocket& socket, std::size_t max_length = 0);

    int receive_from_socket(UDPSocket& socket, std::size_t max_length = 0);

    int send_to_socket(UDPSocket& socket, std::size_t max_length = 0);

    int sendto_socket(UDPSocket& socket, const NetworkAddress& dest, std::size_t max_length = 0);

    int recvfrom_socket(UDPSocket& socket, NetworkAddress& source, std::size_t max_length = 0);

private:
    void reserve_internal(const std::size_t new_capacity);
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