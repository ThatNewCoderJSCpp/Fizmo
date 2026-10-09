#ifndef FIZMO_BUFFERED_UDP_SOCKET_CLASS_HPP
#define FIZMO_BUFFERED_UDP_SOCKET_CLASS_HPP

#include "udp_socket.hpp"
#include "datagram.hpp"
#include <deque>
#include <mutex>

namespace fizmo {
namespace networking {
namespace core {

class BufferedUDPSocket {
private:
    mutable std::mutex m_mutex;
    UDPSocket m_socket;
    std::deque<Datagram> m_send_queue;
    std::deque<Datagram> m_receive_queue;
    std::size_t m_max_datagram_size;
    std::size_t m_max_queue_size;

public:
    explicit BufferedUDPSocket(
        AddressFamily family = AddressFamily::IPv4,
        std::size_t max_datagram_size = 65507,
        std::size_t max_queue_size = 1024
    );

    BufferedUDPSocket(const BufferedUDPSocket&) = delete;
    BufferedUDPSocket& operator=(const BufferedUDPSocket&) = delete;

    BufferedUDPSocket(BufferedUDPSocket&& other) noexcept;

    BufferedUDPSocket& operator=(BufferedUDPSocket&& other) noexcept;

    ~BufferedUDPSocket() = default;

    bool bind(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.bind(addr);
    }

    bool connect(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.connect(addr);
    }

    void close() noexcept;

    bool queue_send(const void* data, std::size_t len, const NetworkAddress& dest);

    bool queue_send(const Datagram& dg);

    bool queue_send(Datagram&& dg);

    bool queue_send_string(const std::string& str, const NetworkAddress& dest) {
        return queue_send(str.data(), str.size(), dest);
    }

    bool queue_send_connected(const void* data, std::size_t len);

    bool queue_send_connected(const Datagram& dg) {
        return queue_send_connected(dg.raw(), dg.size());
    }
    
    int flush_one() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return flush_one_unlocked();
    }

    int flush() noexcept;

    int receive_one() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return receive_one_unlocked();
    }

    int receive_all() noexcept;

    bool pop_received(Datagram& dg);

    bool peek_received(Datagram& dg) const;

    bool has_pending_data() const noexcept     { std::lock_guard<std::mutex> l(m_mutex); return !m_receive_queue.empty(); }
    std::size_t pending_count() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_receive_queue.size(); }
    bool has_data_to_send() const noexcept     { std::lock_guard<std::mutex> l(m_mutex); return !m_send_queue.empty(); }
    std::size_t send_queue_size() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_send_queue.size(); }

    std::size_t max_datagram_size() const noexcept { return m_max_datagram_size; }
    std::size_t max_queue_size() const noexcept    { return m_max_queue_size; }

    void set_max_datagram_size(std::size_t s) noexcept { m_max_datagram_size = s; }
    void set_max_queue_size(std::size_t s) noexcept    { m_max_queue_size = s; }

    bool has_critical_errors() const noexcept        { std::lock_guard<std::mutex> l(m_mutex); return m_socket.has_critical_errors(); }
    bool has_error_of_type(ErrorCode c) const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_socket.has_error_of_type(c); }
    std::vector<SocketError> errors() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_socket.errors(); }
    SocketError last_error() const noexcept          { std::lock_guard<std::mutex> l(m_mutex); return m_socket.last_error(); }
    void add_error(const SocketError& e) noexcept    { std::lock_guard<std::mutex> l(m_mutex); m_socket.add_error(e); }
    void clear_errors() noexcept                     { std::lock_guard<std::mutex> l(m_mutex); m_socket.clear_errors(); }

    int set_blocking(bool b) noexcept               { std::lock_guard<std::mutex> l(m_mutex); return m_socket.set_blocking(b); }

    template<typename T>
    int set_option(int level, int option, const T& value) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.set_option(level, option, value);
    }

    void set_send_timeout(unsigned int ms) noexcept    { std::lock_guard<std::mutex> l(m_mutex); m_socket.set_send_timeout(ms); }
    void set_receive_timeout(unsigned int ms) noexcept { std::lock_guard<std::mutex> l(m_mutex); m_socket.set_receive_timeout(ms); }

    unsigned int send_timeout() const noexcept    { std::lock_guard<std::mutex> l(m_mutex); return m_socket.send_timeout(); }
    unsigned int receive_timeout() const noexcept { std::lock_guard<std::mutex> l(m_mutex); return m_socket.receive_timeout(); }

    UDPSocket&       socket() noexcept       { return m_socket; }
    const UDPSocket& socket() const noexcept { return m_socket; }

private:
    int flush_one_unlocked() noexcept;

    int receive_one_unlocked() noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_BUFFERED_UDP_SOCKET_CLASS_HPP