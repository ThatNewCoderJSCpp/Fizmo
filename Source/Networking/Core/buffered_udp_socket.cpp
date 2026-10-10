#include "fizmo_library.hpp"
#include "buffered_udp_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

BufferedUDPSocket::BufferedUDPSocket(
        AddressFamily family,
        std::size_t max_datagram_size,
        std::size_t max_queue_size 
) : m_socket(family), m_max_datagram_size(max_datagram_size), m_max_queue_size(max_queue_size) {}

BufferedUDPSocket::BufferedUDPSocket(BufferedUDPSocket&& other) noexcept : m_socket(AddressFamily::IPv4), m_max_datagram_size(65507), m_max_queue_size(1024) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_socket            = std::move(other.m_socket);
    m_send_queue        = std::move(other.m_send_queue);
    m_receive_queue     = std::move(other.m_receive_queue);
    m_max_datagram_size = other.m_max_datagram_size;
    m_max_queue_size    = other.m_max_queue_size;
}

auto BufferedUDPSocket::operator=(BufferedUDPSocket&& other) noexcept -> BufferedUDPSocket& {
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

void BufferedUDPSocket::close() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_socket.close();
    m_send_queue.clear();
    m_receive_queue.clear();
}

bool BufferedUDPSocket::queue_send(const void* data, std::size_t len, const NetworkAddress& dest) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_send_queue.size() >= m_max_queue_size) return false;
    if (len > m_max_datagram_size) return false;
    m_send_queue.emplace_back(data, len, dest);
    return true;
}

bool BufferedUDPSocket::queue_send(const Datagram& dg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_send_queue.size() >= m_max_queue_size) return false;
    if (dg.size() > m_max_datagram_size) return false;
    m_send_queue.push_back(dg);
    return true;
}

bool BufferedUDPSocket::queue_send(Datagram&& dg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_send_queue.size() >= m_max_queue_size) return false;
    if (dg.size() > m_max_datagram_size) return false;
    m_send_queue.push_back(std::move(dg));
    return true;
}

bool BufferedUDPSocket::queue_send_connected(const void* data, std::size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_send_queue.size() >= m_max_queue_size) return false;
    if (len > m_max_datagram_size) return false;
    m_send_queue.emplace_back(data, len, NetworkAddress{});
    return true;
}

int BufferedUDPSocket::flush() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    int total = 0;

    while (!m_send_queue.empty()) {
        int n = flush_one_unlocked();
        if (n <= 0) break;
        total += n;
    }

    return total;
}

int BufferedUDPSocket::receive_all() noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    int total = 0;

    while (true) {
        int n = receive_one_unlocked();
        if (n <= 0) break;
        total += n;
    }

    return total;
}

bool BufferedUDPSocket::pop_received(Datagram& dg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_receive_queue.empty()) return false;
    dg = std::move(m_receive_queue.front());
    m_receive_queue.pop_front();
    return true;
}

bool BufferedUDPSocket::peek_received(Datagram& dg) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_receive_queue.empty()) return false;
    dg = m_receive_queue.front();
    return true;
}

int BufferedUDPSocket::flush_one_unlocked() noexcept {
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

int BufferedUDPSocket::receive_one_unlocked() noexcept {
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

} // namespace core
} // namespace networking
} // namespace fizmo
