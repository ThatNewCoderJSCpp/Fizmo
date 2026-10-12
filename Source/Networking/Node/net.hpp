#ifndef FIZMO_NODE_NET_HPP
#define FIZMO_NODE_NET_HPP

#include "data_view.hpp"
#include "events.hpp"
#include "loop.hpp"
#include "../Async/connection.hpp"
#include "../Async/endpoint.hpp"
#include "../Security/tls_context.hpp"
#include "../Security/certificate.hpp"

#include <any>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace fizmo {
namespace networking {

namespace tls {
struct SecureContextOptions;
} // namespace tls

namespace net {

enum class Family {
    Any = 0,
    IPv4 = 4,
    IPv6 = 6
};

int is_ip(const std::string& input) noexcept;
bool is_ipv4(const std::string& input) noexcept;
bool is_ipv6(const std::string& input) noexcept;

struct AddressInfo {
    std::string   address;
    std::string   family;
    std::uint16_t port = 0;

    std::string to_string() const;
};

struct SocketOptions {
    bool                      no_delay        = true;
    bool                      keep_alive      = false;
    std::chrono::milliseconds timeout{0};
    std::size_t               high_water_mark = 16384;
    std::size_t               read_buffer_size = 16384;
};

struct ConnectOptions : SocketOptions {
    std::string               host = "localhost";
    std::uint16_t             port = 0;
    Family                    family = Family::Any;
    std::chrono::milliseconds connect_timeout{30000};
};

struct ListenOptions {
    std::uint16_t port      = 0;
    std::string   host;
    int           backlog   = 511;
    bool          ipv6_only = false;
    bool          reuse_port = false;
};

struct ServerOptions : SocketOptions {
    std::size_t               max_connections = 0;
    std::chrono::milliseconds handshake_timeout{10000};
    bool                      pause_on_connect = false;
};

class Server;

class Socket {
public:
    struct State;

    struct Events {
        Event<>                                                       connect;
        Event<>                                                       ready;
        Event<>                                                       secure_connect;
        Event<const DataView&>                                        data;
        Event<const std::string&>                                     line;
        Event<const DataView&>                                        message;
        Event<>                                                       end;
        Event<bool>                                                   close;
        Event<const std::error_code&>                                 error;
        Event<>                                                       drain;
        Event<>                                                       timeout;
        Event<const std::error_code&, const std::string&, Family>     lookup;
    };

    Socket() noexcept = default;
    explicit Socket(std::shared_ptr<State> state) noexcept : m_state(std::move(state)) {}

    explicit operator bool() const noexcept { return m_state != nullptr; }
    bool operator==(const Socket& o) const noexcept { return m_state == o.m_state; }
    bool operator!=(const Socket& o) const noexcept { return m_state != o.m_state; }

    Socket& on_connect(std::function<void()> fn);
    Socket& on_ready(std::function<void()> fn);
    Socket& on_secure_connect(std::function<void()> fn);
    Socket& on_data(std::function<void(const DataView&)> fn);
    Socket& on_line(std::function<void(const std::string&)> fn, std::size_t max_line = 1u << 16);
    Socket& on_message(std::function<void(const DataView&)> fn, std::size_t max_message = 16u << 20);
    Socket& on_end(std::function<void()> fn);
    Socket& on_close(std::function<void(bool had_error)> fn);
    Socket& on_error(std::function<void(const std::error_code&)> fn);
    Socket& on_drain(std::function<void()> fn);
    Socket& on_timeout(std::function<void()> fn);
    Socket& on_lookup(std::function<void(const std::error_code&, const std::string&, Family)> fn);
    Events& events();

    bool write(const DataView& data, std::function<void(const std::error_code&)> done = {});
    bool write_line(const std::string& line);
    bool write_message(const DataView& data);
    Socket& end(std::function<void()> done = {});
    Socket& end(const DataView& data, std::function<void()> done = {});
    Socket& destroy();
    Socket& destroy(const std::error_code& reason);
    Socket& destroy_soon();

    Socket& pause();
    Socket& resume();
    Socket& set_timeout(std::chrono::milliseconds timeout, std::function<void()> on_timeout = {});
    Socket& set_timeout(std::int64_t timeout_ms, std::function<void()> fn = {}) { return set_timeout(std::chrono::milliseconds(timeout_ms), std::move(fn)); }
    Socket& set_no_delay(bool enable = true);
    Socket& ref();
    Socket& unref();
    Socket& set_keep_alive(bool enable = true);
    Socket pipe(Socket destination, bool end_destination = true);

    std::uint64_t id() const noexcept;
    std::string remote_address() const;
    std::uint16_t remote_port() const;
    std::string remote_family() const;
    std::string local_address() const;
    std::uint16_t local_port() const;
    AddressInfo address() const;
    async::Endpoint remote_endpoint() const;
    async::Endpoint local_endpoint() const;

    bool connecting() const noexcept;
    bool pending() const noexcept;
    bool destroyed() const noexcept;
    bool is_paused() const noexcept;
    bool writable() const noexcept;
    std::string ready_state() const;
    std::size_t bytes_read() const noexcept;
    std::size_t bytes_written() const noexcept;
    std::size_t buffered_amount() const noexcept;
    std::size_t writable_length() const noexcept { return buffered_amount(); }
    std::error_code last_error() const;

    bool encrypted() const noexcept;
    bool authorized() const noexcept;
    std::string authorization_error() const;
    std::string alpn_protocol() const;
    std::string protocol() const;
    std::string cipher() const;
    std::string servername() const;
    bool session_reused() const;
    bool peer_certificate(security::Certificate& out) const;

    std::any& data();
    template <typename T> T* data_as() { return std::any_cast<T>(&data()); }

    async::Connection::Ptr connection() const;
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;

    friend class Server;
};

class Server {
public:
    struct State;

    struct Events {
        Event<Socket>                                                 connection;
        Event<>                                                       listening;
        Event<>                                                       close;
        Event<const std::error_code&>                                 error;
        Event<const std::error_code&, const std::string&, const async::Endpoint&> tls_client_error;
        Event<const async::Endpoint&>                                 drop;
    };

    Server() noexcept = default;
    explicit Server(std::shared_ptr<State> state) noexcept : m_state(std::move(state)) {}

    explicit operator bool() const noexcept { return m_state != nullptr; }

    Server& listen(std::uint16_t port, std::function<void()> on_listening = {});
    Server& listen(std::uint16_t port, const std::string& host, std::function<void()> on_listening = {});
    Server& listen(const ListenOptions& options, std::function<void()> on_listening = {});
    Server& close(std::function<void(const std::error_code&)> on_closed = {});
    Server& close_all_connections();
    Server& end_all_connections();

    Server& on_connection(std::function<void(Socket)> fn);
    Server& on_listening(std::function<void()> fn);
    Server& on_close(std::function<void()> fn);
    Server& on_error(std::function<void(const std::error_code&)> fn);
    Server& on_tls_client_error(std::function<void(const std::error_code&, const std::string&, const async::Endpoint&)> fn);
    Server& on_drop(std::function<void(const async::Endpoint&)> fn);
    Events& events();

    Server& set_max_connections(std::size_t max);
    std::size_t max_connections() const noexcept;

    bool listening() const noexcept;
    bool secure() const noexcept;
    AddressInfo address() const;
    std::uint16_t port() const;
    std::size_t connection_count() const;
    std::vector<Socket> connections() const;
    std::size_t broadcast(const DataView& data, const Socket& except = Socket());

    std::shared_ptr<security::TLSContext> tls_context() const;
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

Server create_server(std::function<void(Socket)> on_connection = {}, const ServerOptions& options = ServerOptions());
Server create_server(const ServerOptions& options, std::function<void(Socket)> on_connection = {});

Socket connect(std::uint16_t port, const std::string& host = "localhost", std::function<void()> on_connect = {});
Socket connect(const ConnectOptions& options, std::function<void()> on_connect = {});
inline Socket create_connection(std::uint16_t port, const std::string& host = "localhost", std::function<void()> on_connect = {}) { return connect(port, host, std::move(on_connect)); }
inline Socket create_connection(const ConnectOptions& options, std::function<void()> on_connect = {}) { return connect(options, std::move(on_connect)); }

namespace detail {

Server make_server(const ServerOptions& options, std::shared_ptr<security::TLSContext> tls);
Socket make_client(const ConnectOptions& options, std::shared_ptr<security::TLSContext> tls, const std::string& servername);
Socket adopt(async::Connection::Ptr connection);
Socket make_failed(const std::error_code& ec, const std::string& detail, bool secure);

} // namespace detail

} // namespace net
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_NET_HPP
