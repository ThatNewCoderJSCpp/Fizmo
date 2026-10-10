#include "fizmo_library.hpp"
#include "multicast_socket.hpp"

namespace fizmo {
namespace networking {
namespace core {

MulticastSocket::MulticastSocket(AddressFamily family) : m_socket(family), m_local_interface("0.0.0.0"), m_ttl(1), m_loopback(true) {
    options(m_socket).set(BooleanOption::ReuseAddress, true);
}

MulticastSocket::MulticastSocket(MulticastSocket&& other) noexcept : m_socket(AddressFamily::IPv4), m_ttl(1), m_loopback(true) {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_socket          = std::move(other.m_socket);
    m_joined_groups   = std::move(other.m_joined_groups);
    m_local_interface = std::move(other.m_local_interface);
    m_ttl             = other.m_ttl;
    m_loopback        = other.m_loopback;
}

bool MulticastSocket::bind(std::uint16_t port) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    NetworkAddress addr("0.0.0.0", port);
    return m_socket.bind(addr);
}

bool MulticastSocket::join(const std::string& group_address) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_joined_groups.count(group_address)) return true; 
    bool ok = m_socket.join_multicast_group(group_address, m_local_interface);

    if (ok) {
        m_joined_groups.insert(group_address);
        m_socket.set_multicast_ttl(m_ttl);
        m_socket.set_multicast_loopback(m_loopback);
    }

    return ok;
}

bool MulticastSocket::leave(const std::string& group_address) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_joined_groups.find(group_address);
    if (it == m_joined_groups.end()) return false;
    bool ok = m_socket.leave_multicast_group(group_address, m_local_interface);
    if (ok) { m_joined_groups.erase(it); }
    return ok;
}

int MulticastSocket::send(const void* data, std::size_t length, const std::string& group, std::uint16_t port) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    NetworkAddress dest(group, port);
    return m_socket.send(data, length, dest);
}

void MulticastSocket::set_loopback(bool enable) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loopback = enable;
    m_socket.set_multicast_loopback(enable);
}

void MulticastSocket::set_interface(const std::string& local_interface) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_local_interface = local_interface;
    m_socket.set_multicast_interface(local_interface);
}

void MulticastSocket::leave_all_unlocked() noexcept {
    for (const auto& group : m_joined_groups) { m_socket.leave_multicast_group(group, m_local_interface); }
    m_joined_groups.clear();
}

} // namespace core
} // namespace networking
} // namespace fizmo
