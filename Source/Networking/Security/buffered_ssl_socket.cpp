#include "fizmo_library.hpp"
#include "buffered_ssl_socket.hpp"

namespace fizmo {
namespace networking {
namespace security {

void BufferedSSLSocket::close(TLSShutdownMode mode) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (mode != TLSShutdownMode::Immediate) flush_unlocked();
    m_socket.close(mode);
    m_send_buffer.clear();
    m_receive_buffer.clear();
}

void BufferedSSLSocket::graceful_close(unsigned int linger_ms) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    flush_unlocked();
    m_socket.close(TLSShutdownMode::Graceful, std::chrono::milliseconds(linger_ms));
    m_send_buffer.clear();
    m_receive_buffer.clear();
}

void BufferedSSLSocket::write_line(const std::string& line, const std::string& terminator) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_send_buffer.write_string(line);
    m_send_buffer.write_string(terminator);
}

int BufferedSSLSocket::flush_unlocked() noexcept {
    std::size_t total = 0;

    while (m_send_buffer.readable_bytes() > 0) {
        const std::size_t n = m_send_buffer.readable_bytes();
        const int sent = m_socket.send(m_send_buffer.read_ptr(), n);
        if (sent <= 0) return total > 0 ? static_cast<int>(total) : sent;
        m_send_buffer.advance_read_position(static_cast<std::size_t>(sent));
        total += static_cast<std::size_t>(sent);
    }

    m_send_buffer.compact();
    return static_cast<int>(total);
}

bool BufferedSSLSocket::write_and_flush(const void* data, std::size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_send_buffer.write(data, len);
    return flush_unlocked() > 0;
}

bool BufferedSSLSocket::write_string_and_flush(const std::string& str) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_send_buffer.write_string(str);
    return flush_unlocked() > 0;
}

int BufferedSSLSocket::receive_unlocked(std::size_t max_length) noexcept {
    std::uint8_t chunk[16384];
    const std::size_t want = max_length == 0 || max_length > sizeof(chunk) ? sizeof(chunk) : max_length;
    const int n = m_socket.receive(chunk, want);
    if (n <= 0) return n;
    m_receive_buffer.compact();
    m_receive_buffer.write(chunk, static_cast<std::size_t>(n));
    return n;
}

bool BufferedSSLSocket::ensure_readable(std::size_t needed) noexcept {
    while (m_receive_buffer.readable_bytes() < needed) {
        if (receive_unlocked(needed - m_receive_buffer.readable_bytes()) <= 0) return false;
    }

    return true;
}

bool BufferedSSLSocket::read_line(std::string& line, char delimiter, bool strip_cr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint8_t* data = m_receive_buffer.read_ptr();
    const std::size_t avail = m_receive_buffer.readable_bytes();

    for (std::size_t i = 0; i < avail; ++i) {
        if (data[i] == static_cast<std::uint8_t>(delimiter)) {
            line.assign(reinterpret_cast<const char*>(data), i);
            m_receive_buffer.advance_read_position(i + 1);
            if (strip_cr && !line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }
    }

    return false;
}

bool BufferedSSLSocket::read_line_with_receive(std::string& line, char delimiter, std::size_t max_length, bool strip_cr) {
    for (;;) {
        if (read_line(line, delimiter, strip_cr)) return true;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_receive_buffer.readable_bytes() >= max_length) return false;
        if (receive_unlocked(0) <= 0) return false;
    }
}

bool BufferedSSLSocket::read_with_receive(void* dest, std::size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ensure_readable(len) && m_receive_buffer.read(dest, len);
}

bool BufferedSSLSocket::read_string_with_receive(std::string& str, std::size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ensure_readable(len) && m_receive_buffer.read_string(str, len);
}

std::string BufferedSSLSocket::read_available() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string out;
    const std::size_t n = m_receive_buffer.readable_bytes();
    if (n > 0) m_receive_buffer.read_string(out, n);
    return out;
}

void BufferedSSLSocket::compact_buffers() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_send_buffer.compact();
    m_receive_buffer.compact();
}

std::string BufferedSSLSocket::to_string() const {
    std::string s = "BufferedSSLSocket[" + m_socket.to_string();
    s += " rx=" + std::to_string(readable_bytes()) + " tx=" + std::to_string(pending_write_bytes()) + "]";
    return s;
}

} // namespace security
} // namespace networking
} // namespace fizmo
