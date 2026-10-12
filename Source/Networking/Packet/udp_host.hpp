#ifndef FIZMO_UDP_HOST_HPP
#define FIZMO_UDP_HOST_HPP

#include "reliable_endpoint.hpp"
#include "../Async/udp.hpp"
#include "../Async/strand.hpp"
#include "../Async/timer.hpp"
#include "../Async/resolver.hpp"

#include <any>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace networking {
namespace packet {

enum class DisconnectReason : std::uint8_t {
    None = 0,
    Requested,
    Timeout,
    ServerFull,
    Denied,
    ProtocolMismatch,
    Shutdown,
    LocalClose,
    ConnectTimeout,
    SocketError
};

const char* disconnect_reason_to_string(DisconnectReason r) noexcept;

namespace detail {
struct UdpHostCore;
} // namespace detail

class UdpPeer : public std::enable_shared_from_this<UdpPeer> {
public:
    using Ptr = std::shared_ptr<UdpPeer>;
    using MessageHandler    = std::function<void(const Ptr&, std::uint8_t channel, std::vector<std::uint8_t> message)>;
    using DisconnectHandler = std::function<void(const Ptr&, DisconnectReason reason)>;

    bool send(std::uint8_t channel, const void* data, std::size_t len);
    bool send(std::uint8_t channel, std::vector<std::uint8_t> data);
    bool send(std::uint8_t channel, const std::string& data) { return send(channel, data.data(), data.size()); }
    void disconnect();

    void on_message(MessageHandler h) { m_on_message = std::move(h); }
    void on_disconnect(DisconnectHandler h) { m_on_disconnect = std::move(h); }

    std::uint32_t id() const noexcept { return m_token; }
    async::Endpoint remote_endpoint() const noexcept { return m_remote; }
    bool connected() const noexcept { return m_connected; }
    double rtt_ms() const noexcept { return m_endpoint.tracker().stats().rtt_ms; }
    double packet_loss() const noexcept { return m_endpoint.tracker().stats().packet_loss; }
    PacketTrackerStats tracker_stats() const { return m_endpoint.tracker().stats(); }
    EndpointStats endpoint_stats() const { return m_endpoint.stats(); }

    ReliableEndpoint& endpoint() noexcept { return m_endpoint; }
    std::any& user_data() noexcept { return m_user_data; }

    UdpPeer(std::weak_ptr<detail::UdpHostCore> core, const async::Endpoint& remote, std::uint32_t token, const EndpointConfig& cfg);

private:
    friend struct detail::UdpHostCore;

    std::weak_ptr<detail::UdpHostCore>      m_core;
    async::Endpoint                         m_remote;
    std::uint32_t                           m_token;
    ReliableEndpoint                        m_endpoint;
    bool                                    m_connected = true;
    std::chrono::steady_clock::time_point   m_last_receive;
    MessageHandler                          m_on_message;
    DisconnectHandler                       m_on_disconnect;
    std::any                                m_user_data;
};

struct UdpHostOptions {
    std::uint64_t             protocol_id      = 0x46495A4D4F4E4554ull;
    std::size_t               max_peers        = 64;
    std::chrono::milliseconds timeout{10000};
    std::chrono::milliseconds tick_interval{10};
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds handshake_resend{250};
    std::size_t               socket_buffer_size = 1u << 20;
    EndpointConfig            endpoint;
};

class UdpServer {
public:
    using ConnectHandler    = std::function<void(const UdpPeer::Ptr&)>;

    explicit UdpServer(async::IOContext& ctx, UdpHostOptions options = UdpHostOptions());
    ~UdpServer();

    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    bool listen(const async::Endpoint& endpoint, std::error_code& ec);
    void listen(std::uint16_t port, const std::string& host = "0.0.0.0");
    void stop();

    UdpServer& on_connect(ConnectHandler h);
    UdpServer& on_message(UdpPeer::MessageHandler h);
    UdpServer& on_disconnect(UdpPeer::DisconnectHandler h);

    void broadcast(std::uint8_t channel, const std::vector<std::uint8_t>& data, const UdpPeer::Ptr& except = nullptr);
    std::size_t peer_count() const;
    std::vector<UdpPeer::Ptr> peers() const;
    async::Endpoint local_endpoint() const;

private:
    std::shared_ptr<detail::UdpHostCore> m_core;
};

class UdpClient {
public:
    using ConnectHandler = std::function<void(std::error_code, DisconnectReason, const UdpPeer::Ptr&)>;

    explicit UdpClient(async::IOContext& ctx, UdpHostOptions options = UdpHostOptions());
    ~UdpClient();

    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;

    void connect(const async::Endpoint& server, ConnectHandler handler);
    void connect(const std::string& host, std::uint16_t port, ConnectHandler handler);
    void disconnect();

    UdpClient& on_message(UdpPeer::MessageHandler h);
    UdpClient& on_disconnect(UdpPeer::DisconnectHandler h);

    bool send(std::uint8_t channel, const void* data, std::size_t len);
    bool send(std::uint8_t channel, std::vector<std::uint8_t> data);
    bool send(std::uint8_t channel, const std::string& data) { return send(channel, data.data(), data.size()); }

    bool connected() const;
    UdpPeer::Ptr peer() const;

private:
    std::shared_ptr<detail::UdpHostCore> m_core;
    std::unique_ptr<async::Resolver>     m_resolver;
};

} // namespace packet
} // namespace networking
} // namespace fizmo

#endif // FIZMO_UDP_HOST_HPP
