#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "buffer.hpp"

namespace fizmo {
namespace networking {
namespace core {

Buffer::Buffer(const std::size_t initial_capacity) : m_size(0), m_capacity(initial_capacity), m_read_pos(0), m_write_pos(0) {
        m_data = std::make_unique<std::uint8_t[]>(m_capacity);
    }

Buffer::Buffer(const Buffer& other) {
        std::lock_guard<std::mutex> lock_other(other.m_mutex);
        m_size      = other.m_size;
        m_capacity  = other.m_capacity;
        m_read_pos  = other.m_read_pos;
        m_write_pos = other.m_write_pos;
        m_data = std::make_unique<std::uint8_t[]>(m_capacity);
        std::memcpy(m_data.get(), other.m_data.get(), m_size);
    }

Buffer::Buffer(Buffer&& other) noexcept : m_size(0), m_capacity(0), m_read_pos(0), m_write_pos(0) {
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

auto Buffer::operator=(const Buffer& other) -> Buffer& {
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

auto Buffer::operator=(Buffer&& other) noexcept -> Buffer& {
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

auto Buffer::resize(const std::size_t new_size) -> void {
        std::lock_guard<std::mutex> l(m_mutex);
        if (new_size > m_capacity) { reserve_internal(new_size + 1); }
        m_size = new_size;
        m_write_pos = std::max(m_write_pos, m_size);
    }

auto Buffer::compact() -> void {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos > 0) {
            if (m_read_pos < m_write_pos) { std::memmove(m_data.get(), m_data.get() + m_read_pos, m_write_pos - m_read_pos); }
            m_write_pos -= m_read_pos;
            m_size      -= m_read_pos;
            m_read_pos   = 0;
        }
    }

auto Buffer::set_write_position(const std::size_t pos) noexcept -> void {
        std::lock_guard<std::mutex> l(m_mutex);
        m_write_pos = std::min(pos, m_capacity);
        m_size = std::max(m_size, m_write_pos);
    }

auto Buffer::advance_write_position(const std::size_t bytes) noexcept -> void {
        std::lock_guard<std::mutex> l(m_mutex);
        std::size_t nw = m_write_pos + bytes;
        if (nw > m_capacity) { reserve_internal(std::max(nw + 1, m_capacity * 2)); }
        m_write_pos = nw;
        m_size = std::max(m_size, m_write_pos);
    }

auto Buffer::write(const void* data, const std::size_t length) -> void {
        std::lock_guard<std::mutex> l(m_mutex);
        if (length == 0) return;
        if (m_write_pos + length > m_capacity) { reserve_internal(std::max(m_write_pos + length + 1, m_capacity * 2)); }
        std::memcpy(m_data.get() + m_write_pos, data, length);
        m_write_pos += length;
        m_size = std::max(m_size, m_write_pos);
    }

auto Buffer::read(void* dest, const std::size_t length) -> bool {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos + length > m_write_pos) return false;
        std::memcpy(dest, m_data.get() + m_read_pos, length);
        m_read_pos += length;
        return true;
    }

auto Buffer::read_string(std::string& str, const std::size_t length) -> bool {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos + length > m_write_pos) return false;
        str.assign(reinterpret_cast<const char*>(m_data.get() + m_read_pos), length);
        m_read_pos += length;
        return true;
    }

auto Buffer::receive_from_socket(TCPSocket& socket, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive(m_data.get() + m_write_pos, max_length);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

auto Buffer::send_to_socket(TCPSocket& socket, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        if (max_length == 0 || max_length > m_write_pos - m_read_pos) { max_length = m_write_pos - m_read_pos; }
        int n = socket.send(m_data.get() + m_read_pos, max_length);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

auto Buffer::receive_from_socket(UDPSocket& socket, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive_connected(m_data.get() + m_write_pos, max_length);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

auto Buffer::send_to_socket(UDPSocket& socket, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        if (max_length == 0 || max_length > m_write_pos - m_read_pos) { max_length = m_write_pos - m_read_pos; }
        int n = socket.send_connected(m_data.get() + m_read_pos, max_length);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

auto Buffer::sendto_socket(UDPSocket& socket, const NetworkAddress& dest, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (m_read_pos >= m_write_pos) return 0;
        std::size_t avail = m_write_pos - m_read_pos;
        if (max_length == 0 || max_length > avail) { max_length = avail; }
        int n = socket.send(m_data.get() + m_read_pos, max_length, dest);
        if (n > 0) { m_read_pos += n; }
        return n;
    }

auto Buffer::recvfrom_socket(UDPSocket& socket, NetworkAddress& source, std::size_t max_length) -> int {
        std::lock_guard<std::mutex> l(m_mutex);
        if (max_length == 0) { max_length = m_capacity - m_write_pos; }
        if (m_write_pos + max_length > m_capacity) { reserve_internal(std::max(m_write_pos + max_length + 1, m_capacity * 2)); }
        int n = socket.receive(m_data.get() + m_write_pos, max_length, source);
        if (n > 0) { m_write_pos += n; m_size = std::max(m_size, m_write_pos); }
        return n;
    }

auto Buffer::reserve_internal(const std::size_t new_capacity) -> void {
        if (new_capacity <= m_capacity) return;
        auto nd = std::make_unique<std::uint8_t[]>(new_capacity);
        std::memcpy(nd.get(), m_data.get(), m_size);
        m_data = std::move(nd);
        m_capacity = new_capacity;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
