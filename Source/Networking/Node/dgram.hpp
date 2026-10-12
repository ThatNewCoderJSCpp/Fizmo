#ifndef FIZMO_NODE_DGRAM_HPP
#define FIZMO_NODE_DGRAM_HPP

#include "data_view.hpp"
#include "events.hpp"
#include "loop.hpp"
#include "net.hpp"
#include "../Async/endpoint.hpp"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace dgram {

enum class Type {
    UDP4,
    UDP6
};

struct RemoteInfo {
    std::string     address;
    std::string     family;
    std::uint16_t   port = 0;
    std::size_t     size = 0;
    async::Endpoint endpoint;
};

struct SocketOptions {
    Type        type = Type::UDP4;
    bool        reuse_address = false;
    bool        reuse_port = false;
    bool        ipv6_only = false;
    int         receive_buffer_size = 0;
    int         send_buffer_size = 0;
    std::size_t max_datagram_size = 65536;
};

class Socket {
public:
    struct State;

    struct Events {
        Event<const DataView&, const RemoteInfo&> message;
        Event<>                                   listening;
        Event<>                                   connect;
        Event<>                                   close;
        Event<const std::error_code&>             error;
    };

    Socket() noexcept = default;
    explicit Socket(std::shared_ptr<State> state) noexcept : m_state(std::move(state)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }
    bool operator==(const Socket& o) const noexcept { return m_state == o.m_state; }

    Socket& on_message(std::function<void(const DataView&, const RemoteInfo&)> fn);
    Socket& on_listening(std::function<void()> fn);
    Socket& on_connect(std::function<void()> fn);
    Socket& on_close(std::function<void()> fn);
    Socket& on_error(std::function<void(const std::error_code&)> fn);
    Events& events();

    Socket& bind(std::uint16_t port = 0, const std::string& address = {}, std::function<void()> on_listening = {});
    Socket& bind(std::uint16_t port, std::function<void()> on_listening) { return bind(port, std::string(), std::move(on_listening)); }
    Socket& connect(std::uint16_t port, const std::string& address = "localhost", std::function<void(const std::error_code&)> callback = {});
    Socket& disconnect();
    Socket& close(std::function<void()> callback = {});

    Socket& send(const DataView& message, std::uint16_t port, const std::string& address, std::function<void(const std::error_code&, std::size_t)> callback = {});
    Socket& send(const DataView& message, const async::Endpoint& to, std::function<void(const std::error_code&, std::size_t)> callback = {});
    Socket& send(const DataView& message, std::function<void(const std::error_code&, std::size_t)> callback = {});

    net::AddressInfo address() const;
    net::AddressInfo remote_address() const;
    bool bound() const noexcept;
    bool connected() const noexcept;
    bool closed() const noexcept;

    Socket& set_broadcast(bool enable);
    Socket& set_ttl(int ttl);
    Socket& set_multicast_ttl(int ttl);
    Socket& set_multicast_loopback(bool enable);
    Socket& set_multicast_interface(const std::string& interface_address);
    Socket& add_membership(const std::string& group, const std::string& interface_address = {});
    Socket& drop_membership(const std::string& group, const std::string& interface_address = {});
    Socket& set_receive_buffer_size(int bytes);
    Socket& set_send_buffer_size(int bytes);

    std::any& data();
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

Socket create_socket(Type type = Type::UDP4, std::function<void(const DataView&, const RemoteInfo&)> on_message = {});
Socket create_socket(const SocketOptions& options, std::function<void(const DataView&, const RemoteInfo&)> on_message = {});

} // namespace dgram
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_DGRAM_HPP
