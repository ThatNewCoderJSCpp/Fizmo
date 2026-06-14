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
    ) : m_socket(family), m_max_datagram_size(max_datagram_size), m_max_queue_size(max_queue_size) {}

    BufferedUDPSocket(const BufferedUDPSocket&) = delete;
    BufferedUDPSocket& operator=(const BufferedUDPSocket&) = delete;

    BufferedUDPSocket(BufferedUDPSocket&& other) noexcept : m_socket(AddressFamily::IPv4), m_max_datagram_size(65507), m_max_queue_size(1024) {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_socket            = std::move(other.m_socket);
        m_send_queue        = std::move(other.m_send_queue);
        m_receive_queue     = std::move(other.m_receive_queue);
        m_max_datagram_size = other.m_max_datagram_size;
        m_max_queue_size    = other.m_max_queue_size;
    }

    BufferedUDPSocket& operator=(BufferedUDPSocket&& other) noexcept {
        if (this != &other) {
            std::unique_lock<std::mutex> lk1(m_mutex, std::defer_lock);
            std::unique_lock<std::mutex> lk2(other.m_mutex, std::defer_lock);
            std::lock(lk1, lk2);
            m_socket.close();
            m_socket            = std::move(other.m_socket);
            m_send_queue        = std::move(other.m_send_queue);
            m_receive_queue     = std::move(other.m_receive_queue);
            m_max_datagram_size = other.m_max_datagram_size;
            m_max_queue_size    = other.m_max_queue_size;
        }

        return *this;
    }

    ~BufferedUDPSocket() = default;

    bool bind(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.bind(addr);
    }

    bool connect(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.connect(addr);
    }

    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.close();
        m_send_queue.clear();
        m_receive_queue.clear();
    }

    bool queue_send(const void* data, std::size_t len, const NetworkAddress& dest) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_send_queue.size() >= m_max_queue_size) return false;
        if (len > m_max_datagram_size) return false;
        m_send_queue.emplace_back(data, len, dest);
        return true;
    }

    bool queue_send(const Datagram& dg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_send_queue.size() >= m_max_queue_size) return false;
        if (dg.size() > m_max_datagram_size) return false;
        m_send_queue.push_back(dg);
        return true;
    }

    bool queue_send(Datagram&& dg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_send_queue.size() >= m_max_queue_size) return false;
        if (dg.size() > m_max_datagram_size) return false;
        m_send_queue.push_back(std::move(dg));
        return true;
    }

    bool queue_send_string(const std::string& str, const NetworkAddress& dest) {
        return queue_send(str.data(), str.size(), dest);
    }

    bool queue_send_connected(const void* data, std::size_t len) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_send_queue.size() >= m_max_queue_size) return false;
        if (len > m_max_datagram_size) return false;
        m_send_queue.emplace_back(data, len, NetworkAddress{});
        return true;
    }

    bool queue_send_connected(const Datagram& dg) {
        return queue_send_connected(dg.raw(), dg.size());
    }
    
    int flush_one() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return flush_one_unlocked();
    }

    int flush() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        int total = 0;

        while (!m_send_queue.empty()) {
            int n = flush_one_unlocked();
            if (n <= 0) break;
            total += n;
        }

        return total;
    }

    int receive_one() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return receive_one_unlocked();
    }

    int receive_all() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        int total = 0;

        while (true) {
            int n = receive_one_unlocked();
            if (n <= 0) break;
            total += n;
        }

        return total;
    }

    bool pop_received(Datagram& dg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_receive_queue.empty()) return false;
        dg = std::move(m_receive_queue.front());
        m_receive_queue.pop_front();
        return true;
    }

    bool peek_received(Datagram& dg) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_receive_queue.empty()) return false;
        dg = m_receive_queue.front();
        return true;
    }

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
    int flush_one_unlocked() noexcept {
        if (m_send_queue.empty()) return 0;
        Datagram& dg = m_send_queue.front();
        int n;

        if (dg.address.host().empty() || dg.address.host() == "0.0.0.0" || dg.address.host() == "::") {
            n = m_socket.send_connected(dg.raw(), dg.size());
        } else {
            n = m_socket.send(dg.raw(), dg.size(), dg.address);
        }

        if (n > 0) { m_send_queue.pop_front(); }
        return n;
    }

    int receive_one_unlocked() noexcept {
        if (m_receive_queue.size() >= m_max_queue_size) return 0;
        std::vector<std::uint8_t> buf(m_max_datagram_size);
        NetworkAddress source;
        int n = m_socket.receive(buf.data(), buf.size(), source);

        if (n > 0) {
            buf.resize(static_cast<std::size_t>(n));
            m_receive_queue.emplace_back(std::move(buf), source);
        }

        return n;
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_BUFFERED_UDP_SOCKET_CLASS_HPP