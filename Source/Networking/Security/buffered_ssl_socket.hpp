#ifndef FIZMO_BUFFERED_SSL_SOCKET_HPP
#define FIZMO_BUFFERED_SSL_SOCKET_HPP

#include "ssl_socket.hpp"
#include "../Core/buffer.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>

namespace fizmo {
namespace networking {
namespace security {

class BufferedSSLSocket {
public:
    explicit BufferedSSLSocket(TLSContext& ctx, core::AddressFamily family = core::AddressFamily::IPv4, std::size_t buffer_size = 16384)
        : m_socket(ctx, family), m_send_buffer(buffer_size), m_receive_buffer(buffer_size) {}

    BufferedSSLSocket(const BufferedSSLSocket&) = delete;
    BufferedSSLSocket& operator=(const BufferedSSLSocket&) = delete;

public:
    bool connect(const core::NetworkAddress& addr, const std::string& hostname = {}) noexcept { return m_socket.connect(addr, hostname); }
    bool accept(std::unique_ptr<core::TCPSocket> raw) noexcept { return m_socket.accept(std::move(raw)); }

    void close(TLSShutdownMode mode = TLSShutdownMode::Unilateral) noexcept;
    void graceful_close(unsigned int linger_ms = 2000) noexcept;

    void write(const void* data, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(data, len);
    }

    void write_string(const std::string& str) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write_string(str);
    }

    template <typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(value);
    }

    void write_line(const std::string& line, const std::string& terminator = "\r\n");

    int flush() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return flush_unlocked();
    }

    bool write_and_flush(const void* data, std::size_t len);
    bool write_string_and_flush(const std::string& str);

    int receive(std::size_t max_length = 0) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return receive_unlocked(max_length);
    }

    bool read(void* dest, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read(dest, len);
    }

    template <typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read(value);
    }

    bool read_string(std::string& str, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read_string(str, len);
    }

    bool read_line(std::string& line, char delimiter = '\n', bool strip_cr = true);
    bool read_line_with_receive(std::string& line, char delimiter = '\n', std::size_t max_length = 1u << 20, bool strip_cr = true);
    bool read_with_receive(void* dest, std::size_t len);

    template <typename T>
    bool read_with_receive(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        return ensure_readable(sizeof(T)) && m_receive_buffer.read(value);
    }

    bool read_string_with_receive(std::string& str, std::size_t len);
    std::string read_available();

    bool has_pending_data() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_receive_buffer.readable_bytes() > 0 || m_socket.pending() > 0; }
    std::size_t readable_bytes() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_receive_buffer.readable_bytes(); }
    std::size_t pending_write_bytes() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_send_buffer.readable_bytes(); }
    void compact_buffers() noexcept;

    SSLSocket&       ssl()       noexcept { return m_socket; }
    const SSLSocket& ssl() const noexcept { return m_socket; }

    std::string to_string() const;

private:
    SSLSocket          m_socket;
    core::Buffer       m_send_buffer;
    core::Buffer       m_receive_buffer;
    mutable std::mutex m_mutex;

    int flush_unlocked() noexcept;
    int receive_unlocked(std::size_t max_length) noexcept;
    bool ensure_readable(std::size_t needed) noexcept;
};

} // namespace security
} // namespace networking
} // namespace fizmo

#endif // FIZMO_BUFFERED_SSL_SOCKET_HPP
