#ifndef FIZMO_UDP_SOCKET_HPP
#define FIZMO_UDP_SOCKET_HPP

#include "socket_base.hpp"
#include "socket_options.hpp"
#include "Socket Impl/native_socket_ops.hpp"
#include "Socket Impl/native_ip_options.hpp"
#include <string>
#include <utility>

namespace fizmo {
namespace networking {
namespace core {

class UDPSocket : public SocketBase {
public:
    explicit UDPSocket(AddressFamily family = AddressFamily::IPv4) noexcept : SocketBase(SocketType::UDP, family) {}
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&& other) noexcept : SocketBase(std::move(other)) {}

    UDPSocket& operator=(UDPSocket&& other) noexcept {
        SocketBase::operator=(std::move(other));
        return *this;
    }

    ~UDPSocket() override = default;

public:
    bool connect(const NetworkAddress& address) noexcept;

    bool disconnect() noexcept;

    bool is_connected() const noexcept { return m_state.state() == SocketState::State::Connected; }

public:
    int send_connected(const void* data, const std::size_t length) noexcept;

    int receive_connected(void* buffer, const std::size_t length) noexcept;

public:
    int send(const void* data, const std::size_t length, const NetworkAddress& dest) noexcept;

    int receive(void* buffer, const std::size_t length, NetworkAddress& source) noexcept;

public:
    int send_string(const std::string& msg, const NetworkAddress& dest) noexcept { return send(msg.data(), msg.size(), dest); }

    std::pair<std::string, NetworkAddress> receive_string(std::size_t max_length = 65507) noexcept;

    int send_string_connected(const std::string& msg) noexcept { return send_connected(msg.data(), msg.size()); }

    std::string receive_string_connected(std::size_t max_length = 65507) noexcept;

public:
    int broadcast(const void* data, std::size_t length, std::uint16_t port) noexcept;

    int broadcast_string(const std::string& msg, std::uint16_t port) noexcept { return broadcast(msg.data(), msg.size(), port); }

public:
    int peek(void* buffer, std::size_t length, NetworkAddress& source) noexcept;

    int peek_connected(void* buffer, std::size_t length) noexcept;

public:
    bool set_max_hops(int hops) noexcept;

    int get_max_hops() const noexcept;

public:
    int scatter_receive(detail::IOBuffer* buffers, std::size_t count, NetworkAddress& source) noexcept;

    int gather_send(const detail::IOBuffer* buffers, std::size_t count, const NetworkAddress& dest) noexcept;

public:
    bool join_multicast_group(const std::string& group_address, const std::string& local_interface = "0.0.0.0") noexcept;

    bool leave_multicast_group(const std::string& group_address, const std::string& local_interface = "0.0.0.0") noexcept;

    bool join_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface = "0.0.0.0"
    ) noexcept;

    bool leave_source_group(
        const std::string& group_address,
        const std::string& source_address,
        const std::string& local_interface = "0.0.0.0"
    ) noexcept;

public:
    bool set_multicast_ttl(int ttl) noexcept;

    bool set_multicast_loopback(bool enable) noexcept;

    bool set_multicast_interface(const std::string& local_interface) noexcept;

public:
    static unsigned int resolve_interface_index(const std::string& local_interface) noexcept {
        return detail::IPOptions::interface_index(local_interface);
    }

public:
    int send_buffer_connected(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer_connected(Buffer& buffer, const std::size_t max_length = 0) noexcept;
    int send_buffer(Buffer& buffer, const NetworkAddress& dest, const std::size_t max_length = 0) noexcept;
    int receive_to_buffer(Buffer& buffer, NetworkAddress& source, const std::size_t max_length = 0) noexcept;
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_UDP_SOCKET_HPP