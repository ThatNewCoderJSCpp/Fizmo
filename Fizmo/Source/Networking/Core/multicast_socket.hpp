#ifndef FIZMO_MULTICAST_SOCKET_HPP
#define FIZMO_MULTICAST_SOCKET_HPP

#include "udp_socket.hpp"
#include "socket_options.hpp"
#include <set>

namespace fizmo {
namespace networking {
namespace core {

class MulticastSocket {
private:
    mutable std::mutex m_mutex;
    UDPSocket m_socket;
    std::set<std::string> m_joined_groups;
    std::string m_local_interface;
    int m_ttl;
    bool m_loopback;

public:
    explicit MulticastSocket(AddressFamily family = AddressFamily::IPv4) : m_socket(family), m_local_interface("0.0.0.0"), m_ttl(1), m_loopback(true) {
        options(m_socket).set(OptionCode::ReuseAddress, true);
    }

    MulticastSocket(const MulticastSocket&) = delete;
    MulticastSocket& operator=(const MulticastSocket&) = delete;

    MulticastSocket(MulticastSocket&& other) noexcept : m_socket(AddressFamily::IPv4), m_ttl(1), m_loopback(true) {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_socket          = std::move(other.m_socket);
        m_joined_groups   = std::move(other.m_joined_groups);
        m_local_interface = std::move(other.m_local_interface);
        m_ttl             = other.m_ttl;
        m_loopback        = other.m_loopback;
    }

    ~MulticastSocket() {
        std::lock_guard<std::mutex> lock(m_mutex);
        leave_all_unlocked();
    }

    bool bind(std::uint16_t port) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        NetworkAddress addr("0.0.0.0", port);
        return m_socket.bind(addr);
    }

    bool bind(const NetworkAddress& addr) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.bind(addr);
    }

    bool join(const std::string& group_address) noexcept {
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

    bool leave(const std::string& group_address) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_joined_groups.find(group_address);
        if (it == m_joined_groups.end()) return false;
        bool ok = m_socket.leave_multicast_group(group_address, m_local_interface);
        if (ok) { m_joined_groups.erase(it); }
        return ok;
    }

    void leave_all() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        leave_all_unlocked();
    }

    int send(const void* data, std::size_t length, const std::string& group, std::uint16_t port) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        NetworkAddress dest(group, port);
        return m_socket.send(data, length, dest);
    }

    int send_string(const std::string& str, const std::string& group, std::uint16_t port) noexcept {
        return send(str.data(), str.size(), group, port);
    }

    int receive(void* buffer, std::size_t length, NetworkAddress& source) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_socket.receive(buffer, length, source);
    }

    void set_ttl(int ttl) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ttl = ttl;
        m_socket.set_multicast_ttl(ttl);
    }

    void set_loopback(bool enable) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loopback = enable;
        m_socket.set_multicast_loopback(enable);
    }

    void set_interface(const std::string& local_interface) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_local_interface = local_interface;
        m_socket.set_multicast_interface(local_interface);
    }

    void set_blocking(bool blocking) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_blocking(blocking);
    }

    void set_receive_timeout(unsigned int ms) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_socket.set_receive_timeout(ms);
    }

    void close() noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        leave_all_unlocked();
        m_socket.close();
    }

    const std::set<std::string>& joined_groups() const noexcept { return m_joined_groups; }
    bool is_member(const std::string& group) const noexcept { return m_joined_groups.count(group) > 0; }
    UDPSocket& socket() noexcept { return m_socket; }
    const UDPSocket& socket() const noexcept { return m_socket; }

    bool has_critical_errors() const noexcept { return m_socket.has_critical_errors(); }
    SocketError last_error() const noexcept { return m_socket.last_error(); }

private:
    void leave_all_unlocked() noexcept {
        for (const auto& group : m_joined_groups) { m_socket.leave_multicast_group(group, m_local_interface); }
        m_joined_groups.clear();
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_MULTICAST_SOCKET_HPP