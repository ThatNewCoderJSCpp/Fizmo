#ifndef FIZMO_NODE_WEBSOCKET_HPP
#define FIZMO_NODE_WEBSOCKET_HPP

#include "http.hpp"

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
namespace ws {

enum class ReadyState {
    Connecting = 0,
    Open = 1,
    Closing = 2,
    Closed = 3
};

namespace close_code {

constexpr std::uint16_t Normal             = 1000;
constexpr std::uint16_t GoingAway          = 1001;
constexpr std::uint16_t ProtocolError      = 1002;
constexpr std::uint16_t UnsupportedData    = 1003;
constexpr std::uint16_t NoStatus           = 1005;
constexpr std::uint16_t Abnormal           = 1006;
constexpr std::uint16_t InvalidPayload     = 1007;
constexpr std::uint16_t PolicyViolation    = 1008;
constexpr std::uint16_t MessageTooBig      = 1009;
constexpr std::uint16_t MandatoryExtension = 1010;
constexpr std::uint16_t InternalError      = 1011;

} // namespace close_code

bool is_valid_utf8(const std::uint8_t* data, std::size_t len) noexcept;
std::string accept_key(const std::string& client_key);

class WebSocket {
public:
    struct State;

    struct Events {
        Event<>                                      open;
        Event<const DataView&, bool>                 message;
        Event<std::uint16_t, const std::string&>     close;
        Event<const std::error_code&>                error;
        Event<const DataView&>                       ping;
        Event<const DataView&>                       pong;
    };

    WebSocket() noexcept = default;
    explicit WebSocket(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }
    bool operator==(const WebSocket& o) const noexcept { return m_state == o.m_state; }
    bool operator!=(const WebSocket& o) const noexcept { return m_state != o.m_state; }

    WebSocket& on_open(std::function<void()> fn);
    WebSocket& on_message(std::function<void(const DataView& data, bool is_binary)> fn);
    WebSocket& on_text(std::function<void(const std::string&)> fn);
    WebSocket& on_binary(std::function<void(const DataView&)> fn);
    WebSocket& on_close(std::function<void(std::uint16_t code, const std::string& reason)> fn);
    WebSocket& on_error(std::function<void(const std::error_code&)> fn);
    WebSocket& on_ping(std::function<void(const DataView&)> fn);
    WebSocket& on_pong(std::function<void(const DataView&)> fn);
    Events& events();

    bool send(const std::string& text, std::function<void(const std::error_code&)> done = {});
    bool send(const char* text) { return send(std::string(text)); }
    bool send(const DataView& data, bool binary, std::function<void(const std::error_code&)> done = {});
    bool send_binary(const DataView& data, std::function<void(const std::error_code&)> done = {}) { return send(data, true, std::move(done)); }
    bool ping(const DataView& data = DataView());
    bool pong(const DataView& data = DataView());
    WebSocket& close(std::uint16_t code = close_code::Normal, const std::string& reason = {});
    WebSocket& terminate();
    WebSocket& pause();
    WebSocket& resume();

    ReadyState ready_state() const noexcept;
    bool is_open() const noexcept { return ready_state() == ReadyState::Open; }
    std::string protocol() const;
    std::string url() const;
    bool is_server() const noexcept;
    std::size_t buffered_amount() const noexcept;
    std::string remote_address() const;
    std::uint16_t remote_port() const;
    net::Socket socket() const;
    std::uint64_t id() const noexcept;

    std::any& data();
    template <typename T> T* data_as() { return std::any_cast<T>(&data()); }
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

struct ClientOptions {
    std::vector<std::string>   protocols;
    http::Headers              headers;
    std::string                origin;
    std::size_t                max_payload = 64u << 20;
    std::chrono::milliseconds  handshake_timeout{30000};
    std::chrono::milliseconds  close_timeout{30000};
    bool                       auto_pong = true;
    tls::SecureContextOptions  secure;
};

WebSocket connect(const std::string& url, const ClientOptions& options = ClientOptions(), std::function<void()> on_open = {});
WebSocket connect(const std::string& url, std::function<void()> on_open);

struct ServerOptions {
    std::string                path;
    std::size_t                max_payload = 64u << 20;
    std::chrono::milliseconds  close_timeout{30000};
    bool                       auto_pong = true;
    bool                       client_tracking = true;
    std::function<std::string(const std::vector<std::string>& offered, const http::IncomingMessage& request)> handle_protocols;
    std::function<bool(const http::IncomingMessage& request)> verify_client;
    http::ServerOptions        http;
};

class Server {
public:
    struct State;

    struct Events {
        Event<WebSocket, http::IncomingMessage> connection;
        Event<>                                 listening;
        Event<>                                 close;
        Event<const std::error_code&>           error;
    };

    Server() noexcept = default;
    explicit Server(std::shared_ptr<State> s) noexcept : m_state(std::move(s)) {}
    explicit operator bool() const noexcept { return m_state != nullptr; }

    Server& listen(std::uint16_t port, std::function<void()> on_listening = {});
    Server& listen(std::uint16_t port, const std::string& host, std::function<void()> on_listening = {});
    Server& attach(http::Server server);
    Server& close(std::function<void()> on_closed = {});

    bool should_handle(const http::IncomingMessage& request) const;
    void handle_upgrade(http::IncomingMessage request, net::Socket socket, const DataView& head, std::function<void(WebSocket)> on_ready);

    Server& on_connection(std::function<void(WebSocket, http::IncomingMessage)> fn);
    Server& on_listening(std::function<void()> fn);
    Server& on_close(std::function<void()> fn);
    Server& on_error(std::function<void(const std::error_code&)> fn);
    Events& events();

    std::vector<WebSocket> clients() const;
    std::size_t client_count() const;
    std::size_t broadcast(const std::string& text, const WebSocket& except = WebSocket());
    std::size_t broadcast(const DataView& data, bool binary, const WebSocket& except = WebSocket());

    std::uint16_t port() const;
    net::AddressInfo address() const;
    http::Server http_server() const;
    const std::shared_ptr<State>& state() const noexcept { return m_state; }

private:
    std::shared_ptr<State> m_state;
};

Server create_server(const ServerOptions& options = ServerOptions(), std::function<void(WebSocket, http::IncomingMessage)> on_connection = {});

} // namespace ws
} // namespace networking
} // namespace fizmo

#endif // FIZMO_NODE_WEBSOCKET_HPP
