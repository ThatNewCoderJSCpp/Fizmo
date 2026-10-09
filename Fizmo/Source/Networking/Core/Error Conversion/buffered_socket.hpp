#ifndef FIZMO_BUFFERED_SOCKET_CLASS_HPP
#define FIZMO_BUFFERED_SOCKET_CLASS_HPP

#include "buffer.hpp"

namespace fizmo {
namespace networking {
namespace core {

class BufferedSocket {
private:
    mutable std::mutex m_mutex;
    TCPSocket m_socket;
    Buffer m_send_buffer;
    Buffer m_receive_buffer;

public:
    explicit BufferedSocket(AddressFamily family = AddressFamily::IPv4) : m_socket(family), m_send_buffer(8192), m_receive_buffer(8192) {}
    BufferedSocket(const BufferedSocket&) = delete;
    BufferedSocket& operator=(const BufferedSocket&) = delete;

    BufferedSocket(BufferedSocket&& other) noexcept;

    BufferedSocket& operator=(BufferedSocket&& other) noexcept;

    ~BufferedSocket() = default;

    bool shutdown(ShutdownMode mode = ShutdownMode::Both) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.shutdown(mode);
    }

    void graceful_close(unsigned int linger_ms = 2000) noexcept;

    void close() noexcept;

    bool connect(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.connect(addr);
    }

    bool bind(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.bind(addr);
    }

    bool listen(int backlog = 5) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.listen(backlog);
    }

    std::unique_ptr<BufferedSocket> accept() noexcept;

    void write(const void* data, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(data, len);
    }

    void write_string(const std::string& str) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write_string(str);
    }

    template<typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(value);
    }

    void write_line(const std::string& line);

    int flush() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return flush_unlocked();
    }

    bool write_and_flush(const void* data, std::size_t len);

    bool write_string_and_flush(const std::string& str);

    template<typename T>
    bool write_and_flush(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_send_buffer.write(value);
        return flush_unlocked() > 0;
    }

    int receive(std::size_t max_length = 0) noexcept;

    bool read(void* dest, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read(dest, len);
    }

    template<typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read(value);
    }

    bool read_string(std::string& str, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_receive_buffer.read_string(str, len);
    }

    bool read_line(std::string& line, char delimiter = '\n');

    bool read_with_receive(void* dest, std::size_t len);

    template<typename T>
    bool read_with_receive(T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        std::lock_guard<std::mutex> lock(m_mutex);
        return ensure_readable(sizeof(T)) && m_receive_buffer.read(value);
    }

    bool read_string_with_receive(std::string& str, std::size_t len);

    bool has_pending_data() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_receive_buffer.readable_bytes() > 0; }
    std::size_t pending_data_size() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_receive_buffer.readable_bytes(); }
    bool has_data_to_send() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_send_buffer.readable_bytes() > 0; }
    std::size_t data_to_send_size() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_send_buffer.readable_bytes(); }

    bool has_critical_errors() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.has_critical_errors(); }
    bool has_error_of_type(ErrorCode code) const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.has_error_of_type(code); }
    std::vector<SocketError> errors() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.errors(); }
    SocketError last_error() const noexcept { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.last_error(); }
    void add_error(const SocketError& e) noexcept { std::lock_guard<std::mutex> lock(m_mutex); m_socket.add_error(e); }
    void clear_errors() noexcept { std::lock_guard<std::mutex> lock(m_mutex); m_socket.clear_errors(); }

    void set_blocking(bool blocking) noexcept { std::lock_guard<std::mutex> lock(m_mutex); m_socket.set_blocking(blocking); }

    template<typename T>
    void set_option(int level, int option, const T& value) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_option(level, option, value);
    }

    void set_connect_timeout(unsigned int ms) noexcept  { std::lock_guard<std::mutex> lock(m_mutex); m_socket.set_connect_timeout(ms); }
    void set_send_timeout(unsigned int ms) noexcept     { std::lock_guard<std::mutex> lock(m_mutex); m_socket.set_send_timeout(ms); }
    void set_receive_timeout(unsigned int ms) noexcept  { std::lock_guard<std::mutex> lock(m_mutex); m_socket.set_receive_timeout(ms); }

    unsigned int connect_timeout() const noexcept  { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.connect_timeout(); }
    unsigned int send_timeout() const noexcept     { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.send_timeout(); }
    unsigned int receive_timeout() const noexcept  { std::lock_guard<std::mutex> lock(m_mutex); return m_socket.receive_timeout(); }

    TCPSocket&       socket() noexcept       { return m_socket; }
    const TCPSocket& socket() const noexcept { return m_socket; }

    Buffer&       send_buffer() noexcept       { return m_send_buffer; }
    const Buffer& send_buffer() const noexcept { return m_send_buffer; }

    Buffer&       receive_buffer() noexcept       { return m_receive_buffer; }
    const Buffer& receive_buffer() const noexcept { return m_receive_buffer; }

    void compact_buffers() noexcept;

private:
    int flush_unlocked() noexcept { return m_send_buffer.send_to_socket(m_socket); }

    bool ensure_readable(std::size_t needed) noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_BUFFERED_SOCKET_CLASS_HPP