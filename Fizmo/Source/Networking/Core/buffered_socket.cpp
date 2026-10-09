#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "buffered_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

BufferedSocket::BufferedSocket(BufferedSocket&& other) noexcept : m_socket(AddressFamily::IPv4), m_send_buffer(1), m_receive_buffer(1) {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_socket         = std::move(other.m_socket);
        m_send_buffer    = std::move(other.m_send_buffer);
        m_receive_buffer = std::move(other.m_receive_buffer);
    }

auto BufferedSocket::operator=(BufferedSocket&& other) noexcept -> BufferedSocket& {
        if (this != &other) {
            std::unique_lock<std::mutex> lk1(m_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(other.m_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            m_socket.close();
            m_socket         = std::move(other.m_socket);
            m_send_buffer    = std::move(other.m_send_buffer);
            m_receive_buffer = std::move(other.m_receive_buffer);
        }

        return *this;
    }

auto BufferedSocket::graceful_close(unsigned int linger_ms) noexcept -> void {
        std::lock_guard<std::mutex> lock(m_mutex);
        flush_unlocked();
        m_socket.graceful_close(linger_ms);
        m_send_buffer.clear();
        m_receive_buffer.clear();
    }

auto BufferedSocket::close() noexcept -> void {
        std::lock_guard<std::mutex> lock(m_mutex);
        flush_unlocked();  
        m_socket.close();
        m_send_buffer.clear();
        m_receive_buffer.clear();
    }

auto BufferedSocket::accept() noexcept -> std::unique_ptr<BufferedSocket> {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto raw = m_socket.accept();
        if (!raw) return nullptr;
        auto bs = std::make_unique<BufferedSocket>();
        bs->m_socket = std::move(*raw);
        return bs;
    }

auto BufferedSocket::write_line(const std::string& line) -> void {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write_string(line);
        const char nl = '\n';
        m_send_buffer.write(&nl, 1);
    }

auto BufferedSocket::write_and_flush(const void* data, std::size_t len) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(data, len);
        return flush_unlocked() > 0;
    }

auto BufferedSocket::write_string_and_flush(const std::string& str) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write_string(str);
        return flush_unlocked() > 0;
    }

auto BufferedSocket::receive(std::size_t max_length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_receive_buffer.compact();
        return m_receive_buffer.receive_from_socket(m_socket, max_length);
    }

auto BufferedSocket::read_line(std::string& line, char delimiter) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        line.clear();
        const uint8_t* data = m_receive_buffer.read_ptr();
        std::size_t avail = m_receive_buffer.readable_bytes();
        
        for (std::size_t i = 0; i < avail; ++i) {
            if (data[i] == static_cast<uint8_t>(delimiter)) {
                line.assign(reinterpret_cast<const char*>(data), i);
                m_receive_buffer.advance_read_position(i + 1);
                return true;
            }
        }
        
        return false;
    }

auto BufferedSocket::read_with_receive(void* dest, std::size_t len) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        return ensure_readable(len) && m_receive_buffer.read(dest, len);
    }

auto BufferedSocket::read_string_with_receive(std::string& str, std::size_t len) -> bool {
        std::lock_guard<std::mutex> lock(m_mutex);
        return ensure_readable(len) && m_receive_buffer.read_string(str, len);
    }

auto BufferedSocket::compact_buffers() noexcept -> void {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.compact();
        m_receive_buffer.compact();
    }

auto BufferedSocket::ensure_readable(std::size_t needed) noexcept -> bool {
        if (m_receive_buffer.readable_bytes() >= needed) return true;
        m_receive_buffer.compact();
        int got = m_receive_buffer.receive_from_socket(m_socket, needed - m_receive_buffer.readable_bytes());
        return got > 0 && m_receive_buffer.readable_bytes() >= needed;
    }

} // namespace core
} // namespace networking
} // namespace fizmo
