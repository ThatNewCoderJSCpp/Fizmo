#ifndef FIZMO_ASYNC_SERVER_HPP
#define FIZMO_ASYNC_SERVER_HPP

#include "connection.hpp"
#include "tcp.hpp"
#include "tls_stream.hpp"
#include "resolver.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace async {

struct ServerOptions {
    std::size_t               max_connections        = 0;
    std::size_t               max_pending_handshakes = 256;
    std::chrono::milliseconds idle_timeout{0};
    std::chrono::milliseconds handshake_timeout{10000};
    bool                      no_delay               = true;
    bool                      keep_alive             = false;
    bool                      reuse_address          = true;
    bool                      reuse_port             = false;
    bool                      v6_only                = false;
    int                       backlog                = 512;
    std::size_t               read_buffer_size       = 16384;
    std::size_t               high_watermark         = 1u << 20;
};

class TcpServer {
public:
    using ConnectionHandler     = std::function<void(const Connection::Ptr&)>;
    using ErrorHandler          = std::function<void(std::error_code)>;
    using HandshakeErrorHandler = std::function<void(const Endpoint&, const security::SSLError&)>;

    explicit TcpServer(IOContext& ctx, ServerOptions options = ServerOptions());
    TcpServer(IOContext& ctx, security::TLSContext& tls, ServerOptions options = ServerOptions());
    virtual ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer& on_connection(ConnectionHandler h);
    TcpServer& on_error(ErrorHandler h);
    TcpServer& on_handshake_error(HandshakeErrorHandler h);

    bool listen(const Endpoint& endpoint, std::error_code& ec);
    bool listen(std::uint16_t port, const std::string& host, std::error_code& ec);
    void listen(std::uint16_t port, const std::string& host = "0.0.0.0");

    void stop();
    void close_all();
    void shutdown_all();

    bool is_listening() const noexcept;
    bool is_tls() const noexcept;
    Endpoint local_endpoint() const;
    std::uint16_t port() const;
    std::size_t connection_count() const;
    std::vector<Connection::Ptr> connections() const;
    std::size_t broadcast(const std::string& data, const Connection::Ptr& except = nullptr);
    IOContext& context() const noexcept;

    struct Impl;

private:
    std::shared_ptr<Impl> m_impl;
};

class TlsServer : public TcpServer {
public:
    TlsServer(IOContext& ctx, security::TLSContext& tls, ServerOptions options = ServerOptions()) : TcpServer(ctx, tls, options) {}
};

struct ClientOptions {
    std::chrono::milliseconds connect_timeout{10000};
    std::chrono::milliseconds idle_timeout{0};
    ResolveFamily             family     = ResolveFamily::Any;
    bool                      no_delay   = true;
    bool                      keep_alive = false;
    std::size_t               read_buffer_size = 16384;
    std::size_t               high_watermark   = 1u << 20;
    security::BackoffPolicy   reconnect;
};

class TcpClient {
public:
    using ConnectHandler = std::function<void(std::error_code, const Connection::Ptr&)>;

    explicit TcpClient(IOContext& ctx, ClientOptions options = ClientOptions());
    TcpClient(IOContext& ctx, security::TLSContext& tls, ClientOptions options = ClientOptions());
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    void connect(const std::string& host, std::uint16_t port, ConnectHandler handler);
    void connect(const Endpoint& endpoint, ConnectHandler handler, const std::string& server_name = {});
    void connect_with_retry(const std::string& host, std::uint16_t port, ConnectHandler handler, std::uint32_t max_attempts = 0);
    void cancel();

    bool is_tls() const noexcept;
    security::HostFailureTracker& failure_tracker() noexcept;
    IOContext& context() const noexcept;

    struct Impl;

private:
    std::shared_ptr<Impl> m_impl;
};

class TlsClient : public TcpClient {
public:
    TlsClient(IOContext& ctx, security::TLSContext& tls, ClientOptions options = ClientOptions()) : TcpClient(ctx, tls, options) {}
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_SERVER_HPP
