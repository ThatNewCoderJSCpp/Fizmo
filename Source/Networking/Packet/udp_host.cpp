#include "fizmo_library.hpp"
#include "udp_host.hpp"
#include "../../Crypto/hmac.hpp"
#include "../../Random/random_std_int.hpp"

#include <array>
#include <mutex>

namespace fizmo {
namespace networking {
namespace packet {

const char* disconnect_reason_to_string(DisconnectReason r) noexcept {
    switch (r) {
        case DisconnectReason::None:             return "None";
        case DisconnectReason::Requested:        return "Requested";
        case DisconnectReason::Timeout:          return "Timeout";
        case DisconnectReason::ServerFull:       return "ServerFull";
        case DisconnectReason::Denied:           return "Denied";
        case DisconnectReason::ProtocolMismatch: return "ProtocolMismatch";
        case DisconnectReason::Shutdown:         return "Shutdown";
        case DisconnectReason::LocalClose:       return "LocalClose";
        case DisconnectReason::ConnectTimeout:   return "ConnectTimeout";
        case DisconnectReason::SocketError:      return "SocketError";
    }

    return "Unknown";
}

namespace detail {

namespace {

enum PacketType : std::uint8_t {
    ConnectRequest    = 1,
    Challenge         = 2,
    ChallengeResponse = 3,
    Accepted          = 4,
    Denied            = 5,
    Payload           = 6,
    Disconnect        = 7
};

constexpr std::size_t kHandshakePad = 96;

} // namespace

struct UdpHostCore : std::enable_shared_from_this<UdpHostCore> {
    using clock = std::chrono::steady_clock;

    enum class ClientState { Idle, Requesting, Responding, Connected };

    async::IOContext&                                   ctx;
    UdpHostOptions                                      opt;
    bool                                                is_server;
    async::Strand                                       strand;
    async::UdpSocket                                    socket;
    async::SteadyTimer                                  timer;
    std::vector<std::uint8_t>                           rbuf;
    bool                                                running = false;
    bool                                                flush_scheduled = false;
    mutable std::mutex                                  peers_mutex;
    std::recursive_mutex                                core_mutex;
    std::unordered_map<async::Endpoint, UdpPeer::Ptr>   peers;
    std::array<std::uint8_t, 32>                        secret{};
    UdpServer::ConnectHandler                           on_connect;
    UdpPeer::MessageHandler                             on_message;
    UdpPeer::DisconnectHandler                          on_disconnect;
    ClientState                                         cstate = ClientState::Idle;
    async::Endpoint                                     server_ep;
    std::uint64_t                                       client_salt = 0;
    std::uint64_t                                       cookie = 0;
    clock::time_point                                   connect_deadline;
    clock::time_point                                   last_handshake;
    UdpClient::ConnectHandler                           connect_handler;
    UdpPeer::Ptr                                        client_peer;

    UdpHostCore(async::IOContext& c, UdpHostOptions o, bool server)
        : ctx(c), opt(o), is_server(server), strand(c), socket(c), timer(c), rbuf(65536) {
        random_bytes(secret.data(), secret.size());
    }

    void start() {
        if (running) return;
        running = true;
        std::error_code ec;
        if (opt.socket_buffer_size) {
            socket.set_receive_buffer_size(static_cast<int>(opt.socket_buffer_size), ec);
            socket.set_send_buffer_size(static_cast<int>(opt.socket_buffer_size), ec);
        }
        receive();
        schedule_tick();
    }

    void receive() {
        if (!running) return;
        auto self = shared_from_this();

        socket.async_receive_from(async::MutableBuffer(rbuf.data(), rbuf.size()), [self](std::error_code ec, std::size_t n, async::Endpoint from) {
            self->strand.post([self, ec, n, from]() {
                std::lock_guard<std::recursive_mutex> g(self->core_mutex);
                self->on_datagram(ec, n, from);
            });
        });
    }

    void schedule_tick() {
        if (!running) return;
        timer.expires_after(opt.tick_interval);
        std::weak_ptr<UdpHostCore> weak = shared_from_this();

        timer.async_wait([weak](std::error_code ec) {
            if (ec) return;
            auto self = weak.lock();
            if (!self) return;
            self->strand.post([self]() {
                std::lock_guard<std::recursive_mutex> g(self->core_mutex);
                if (self->running) { self->tick(); self->schedule_tick(); }
            });
        });
    }

    void send_raw(const async::Endpoint& to, PacketWriter& w) {
        const std::uint32_t crc = packet_checksum(opt.protocol_id, w.data().data(), w.size());
        w.write_u32(crc);
        std::error_code ec;
        socket.send_to(async::ConstBuffer(w.data().data(), w.size()), to, ec, 0);
    }

    std::uint64_t make_cookie(const async::Endpoint& from, std::uint64_t salt, std::int64_t bucket) const {
        crypto::HMAC_SHA256 mac(secret.data(), secret.size());
        const std::string addr = from.to_string();
        mac.update(addr);
        std::uint8_t tmp[16];
        for (int i = 0; i < 8; ++i) tmp[i] = static_cast<std::uint8_t>(salt >> (8 * i));
        for (int i = 0; i < 8; ++i) tmp[8 + i] = static_cast<std::uint8_t>(static_cast<std::uint64_t>(bucket) >> (8 * i));
        mac.update(tmp, sizeof(tmp));
        const auto d = mac.finish();
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | d[i];
        return v | 1;
    }

    static std::int64_t bucket_now() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 10;
    }

    void send_simple(const async::Endpoint& to, std::uint8_t type, std::uint64_t a, std::uint64_t b, std::size_t pad, std::uint64_t c = 0, bool with_c = false) {
        PacketWriter w(ByteOrder::Big, 128);
        w.write_u8(type).write_u64(a).write_u64(b);
        if (with_c) w.write_u64(c);
        if (w.size() < pad) w.write_zeros(pad - w.size());
        send_raw(to, w);
    }

    void send_disconnect(const async::Endpoint& to, std::uint32_t token, DisconnectReason reason) {
        for (int i = 0; i < 3; ++i) {
            PacketWriter w(ByteOrder::Big, 16);
            w.write_u8(Disconnect).write_u32(token).write_u8(static_cast<std::uint8_t>(reason));
            send_raw(to, w);
        }
    }

    UdpPeer::Ptr make_peer(const async::Endpoint& remote, std::uint32_t token) {
        auto peer = std::make_shared<UdpPeer>(shared_from_this(), remote, token, opt.endpoint);
        peer->m_last_receive = clock::now();
        UdpHostCore* raw = this;
        std::weak_ptr<UdpPeer> weak = peer;

        peer->m_endpoint.on_send_packet = [raw, remote, token](const std::uint8_t* data, std::size_t len) {
            PacketWriter w(ByteOrder::Big, len + 16);
            w.write_u8(Payload).write_u32(token).write_raw(data, len);
            raw->send_raw(remote, w);
        };

        peer->m_endpoint.on_message = [raw, weak](std::uint8_t channel, std::vector<std::uint8_t> msg) {
            auto p = weak.lock();
            if (!p) return;
            if (p->m_on_message) p->m_on_message(p, channel, std::move(msg));
            else if (raw->on_message) raw->on_message(p, channel, std::move(msg));
        };

        return peer;
    }

    UdpPeer::Ptr find_peer(const async::Endpoint& ep) {
        std::lock_guard<std::mutex> lk(peers_mutex);
        auto it = peers.find(ep);
        return it == peers.end() ? nullptr : it->second;
    }

    void remove_peer(UdpPeer::Ptr peer, DisconnectReason reason, bool notify_remote) {
        if (!peer || !peer->m_connected) return;
        peer->m_connected = false;
        if (notify_remote) send_disconnect(peer->m_remote, peer->m_token, reason);

        {
            std::lock_guard<std::mutex> lk(peers_mutex);
            peers.erase(peer->m_remote);
        }

        if (!is_server && client_peer == peer) {
            client_peer.reset();
            cstate = ClientState::Idle;
        }

        if (peer->m_on_disconnect) peer->m_on_disconnect(peer, reason);
        else if (on_disconnect) on_disconnect(peer, reason);
        peer->m_on_message = nullptr;
        peer->m_on_disconnect = nullptr;
        peer->m_endpoint.on_message = nullptr;
        peer->m_endpoint.on_send_packet = nullptr;
    }

    void fail_connect(std::error_code ec, DisconnectReason reason) {
        cstate = ClientState::Idle;
        auto h = std::move(connect_handler);
        connect_handler = nullptr;
        if (h) h(ec, reason, nullptr);
    }

    void on_datagram(std::error_code ec, std::size_t n, const async::Endpoint& from) {
        if (!running) return;

        if (ec) {
            if (ec == async::error::operation_aborted || ec == async::error::bad_descriptor) return;
            receive();
            return;
        }

        handle(rbuf.data(), n, from);
        receive();
    }

    void handle(const std::uint8_t* data, std::size_t n, const async::Endpoint& from) {
        if (n < 5) return;
        const std::size_t body = n - 4;
        const std::uint32_t crc = (std::uint32_t(data[body]) << 24) | (std::uint32_t(data[body + 1]) << 16) | (std::uint32_t(data[body + 2]) << 8) | data[body + 3];
        if (packet_checksum(opt.protocol_id, data, body) != crc) return;
        PacketReader r(data, body);
        const std::uint8_t type = r.read_u8();
        const auto now = clock::now();

        if (type == Payload) {
            const std::uint32_t token = r.read_u32();
            if (!r.ok()) return;
            UdpPeer::Ptr peer = find_peer(from);
            if (!peer || peer->m_token != token) return;
            peer->m_last_receive = now;
            peer->m_endpoint.receive_packet(r.current(), r.remaining(), now);
            return;
        }

        if (type == Disconnect) {
            const std::uint32_t token = r.read_u32();
            if (!r.ok()) return;
            UdpPeer::Ptr peer = find_peer(from);
            if (peer && peer->m_token == token) remove_peer(peer, DisconnectReason::Requested, false);
            return;
        }

        if (is_server) handle_server(type, r, from);
        else handle_client(type, r, from, now);
    }

    void handle_server(std::uint8_t type, PacketReader& r, const async::Endpoint& from) {
        if (type == ConnectRequest) {
            const std::uint64_t protocol = r.read_u64();
            const std::uint64_t salt = r.read_u64();
            if (!r.ok() || r.size() < kHandshakePad) return;
            if (protocol != opt.protocol_id) return;
            UdpPeer::Ptr existing = find_peer(from);

            if (existing) {
                send_simple(from, Accepted, salt, existing->m_token, 0);
                return;
            }

            send_simple(from, Challenge, salt, make_cookie(from, salt, bucket_now()), 0);
            return;
        }

        if (type == ChallengeResponse) {
            const std::uint64_t protocol = r.read_u64();
            const std::uint64_t salt = r.read_u64();
            const std::uint64_t ck = r.read_u64();
            if (!r.ok() || r.size() < kHandshakePad || protocol != opt.protocol_id) return;
            const std::int64_t b = bucket_now();
            if (ck != make_cookie(from, salt, b) && ck != make_cookie(from, salt, b - 1)) return;
            UdpPeer::Ptr existing = find_peer(from);

            if (existing) {
                send_simple(from, Accepted, salt, existing->m_token, 0);
                return;
            }

            {
                std::lock_guard<std::mutex> lk(peers_mutex);

                if (peers.size() >= opt.max_peers) {
                    send_simple(from, Denied, salt, static_cast<std::uint64_t>(DisconnectReason::ServerFull), 0);
                    return;
                }
            }

            const std::uint32_t token = static_cast<std::uint32_t>(salt ^ ck ^ (ck >> 32));
            auto peer = make_peer(from, token);

            {
                std::lock_guard<std::mutex> lk(peers_mutex);
                peers[from] = peer;
            }

            send_simple(from, Accepted, salt, token, 0);
            if (on_connect) on_connect(peer);
        }
    }

    void send_request() {
        last_handshake = clock::now();
        if (cstate == ClientState::Requesting) send_simple(server_ep, ConnectRequest, opt.protocol_id, client_salt, kHandshakePad);
        else if (cstate == ClientState::Responding) send_simple(server_ep, ChallengeResponse, opt.protocol_id, client_salt, kHandshakePad, cookie, true);
    }

    void handle_client(std::uint8_t type, PacketReader& r, const async::Endpoint& from, clock::time_point) {
        if (!(from == server_ep)) return;
        const std::uint64_t salt = r.read_u64();
        const std::uint64_t value = r.read_u64();
        if (!r.ok() || salt != client_salt) return;

        if (type == Challenge && cstate == ClientState::Requesting) {
            cookie = value;
            cstate = ClientState::Responding;
            send_request();
            return;
        }

        if (type == Accepted && (cstate == ClientState::Requesting || cstate == ClientState::Responding)) {
            cstate = ClientState::Connected;
            client_peer = make_peer(server_ep, static_cast<std::uint32_t>(value));

            {
                std::lock_guard<std::mutex> lk(peers_mutex);
                peers[server_ep] = client_peer;
            }

            auto h = std::move(connect_handler);
            connect_handler = nullptr;
            if (h) h(std::error_code(), DisconnectReason::None, client_peer);
            return;
        }

        if (type == Denied && cstate != ClientState::Connected && cstate != ClientState::Idle) {
            const auto reason = static_cast<DisconnectReason>(value);
            fail_connect(make_error_code(async::error::connection_refused), reason == DisconnectReason::None ? DisconnectReason::Denied : reason);
        }
    }

    void tick() {
        const auto now = clock::now();
        std::vector<UdpPeer::Ptr> list, expired;

        {
            std::lock_guard<std::mutex> lk(peers_mutex);
            for (auto& kv : peers) list.push_back(kv.second);
        }

        for (auto& p : list) {
            if (now - p->m_last_receive > opt.timeout) expired.push_back(p);
            else p->m_endpoint.update(now);
        }

        for (auto& p : expired) remove_peer(p, DisconnectReason::Timeout, true);

        if (!is_server && (cstate == ClientState::Requesting || cstate == ClientState::Responding)) {
            if (now >= connect_deadline) fail_connect(make_error_code(async::error::timed_out), DisconnectReason::ConnectTimeout);
            else if (now - last_handshake >= opt.handshake_resend) send_request();
        }
    }

    void request_flush() {
        if (flush_scheduled) return;
        flush_scheduled = true;
        auto self = shared_from_this();

        strand.post([self]() {
            std::lock_guard<std::recursive_mutex> g(self->core_mutex);
            self->flush_scheduled = false;
            if (!self->running) return;
            const auto now = clock::now();
            std::vector<UdpPeer::Ptr> list;

            {
                std::lock_guard<std::mutex> lk(self->peers_mutex);
                for (auto& kv : self->peers) list.push_back(kv.second);
            }

            for (auto& p : list) if (p->m_endpoint.has_pending_data()) p->m_endpoint.update(now);
        });
    }

    void stop() {
        if (!running) return;
        std::vector<UdpPeer::Ptr> list;

        {
            std::lock_guard<std::mutex> lk(peers_mutex);
            for (auto& kv : peers) list.push_back(kv.second);
        }

        for (auto& p : list) remove_peer(p, DisconnectReason::Shutdown, true);
        if (!is_server && connect_handler) fail_connect(make_error_code(async::error::operation_aborted), DisconnectReason::LocalClose);
        running = false;
        timer.cancel();
        socket.close();
    }
};

} // namespace detail

UdpPeer::UdpPeer(std::weak_ptr<detail::UdpHostCore> core, const async::Endpoint& remote, std::uint32_t token, const EndpointConfig& cfg)
    : m_core(std::move(core)), m_remote(remote), m_token(token), m_endpoint(cfg) {}

bool UdpPeer::send(std::uint8_t channel, const void* data, std::size_t len) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    return send(channel, std::vector<std::uint8_t>(p, p + len));
}

bool UdpPeer::send(std::uint8_t channel, std::vector<std::uint8_t> data) {
    auto core = m_core.lock();
    if (!core || !m_connected) return false;

    if (core->strand.running_in_this_thread()) {
        const bool ok = m_endpoint.send(channel, std::move(data));
        core->request_flush();
        return ok;
    }

    auto self = shared_from_this();

    core->strand.post([self, core, channel, d = std::move(data)]() mutable {
        std::lock_guard<std::recursive_mutex> g(core->core_mutex);
        if (!self->m_connected) return;
        self->m_endpoint.send(channel, std::move(d));
        core->request_flush();
    });

    return true;
}

void UdpPeer::disconnect() {
    auto core = m_core.lock();
    if (!core) return;
    auto self = shared_from_this();
    core->strand.dispatch([self, core]() {
        std::lock_guard<std::recursive_mutex> g(core->core_mutex);
        core->remove_peer(self, DisconnectReason::Requested, true);
    });
}

UdpServer::UdpServer(async::IOContext& ctx, UdpHostOptions options) : m_core(std::make_shared<detail::UdpHostCore>(ctx, options, true)) {}

UdpServer::~UdpServer() {
    stop();
}

bool UdpServer::listen(const async::Endpoint& endpoint, std::error_code& ec) {
    if (!m_core->socket.bind(endpoint, ec)) return false;
    auto core = m_core;
    core->strand.dispatch([core]() {
        std::lock_guard<std::recursive_mutex> g(core->core_mutex);
        core->start();
    });
    return true;
}

void UdpServer::listen(std::uint16_t port, const std::string& host) {
    std::error_code ec;
    if (!listen(async::Endpoint(host, port), ec)) throw std::system_error(ec, "UdpServer::listen");
}

void UdpServer::stop() {
    std::lock_guard<std::recursive_mutex> g(m_core->core_mutex);
    m_core->stop();
}

UdpServer& UdpServer::on_connect(ConnectHandler h) { m_core->on_connect = std::move(h); return *this; }
UdpServer& UdpServer::on_message(UdpPeer::MessageHandler h) { m_core->on_message = std::move(h); return *this; }
UdpServer& UdpServer::on_disconnect(UdpPeer::DisconnectHandler h) { m_core->on_disconnect = std::move(h); return *this; }

void UdpServer::broadcast(std::uint8_t channel, const std::vector<std::uint8_t>& data, const UdpPeer::Ptr& except) {
    for (auto& p : peers()) {
        if (p != except) p->send(channel, data);
    }
}

std::size_t UdpServer::peer_count() const {
    std::lock_guard<std::mutex> lk(m_core->peers_mutex);
    return m_core->peers.size();
}

std::vector<UdpPeer::Ptr> UdpServer::peers() const {
    std::vector<UdpPeer::Ptr> out;
    std::lock_guard<std::mutex> lk(m_core->peers_mutex);
    for (auto& kv : m_core->peers) out.push_back(kv.second);
    return out;
}

async::Endpoint UdpServer::local_endpoint() const {
    return m_core->socket.local_endpoint();
}

UdpClient::UdpClient(async::IOContext& ctx, UdpHostOptions options) : m_core(std::make_shared<detail::UdpHostCore>(ctx, options, false)) {}

UdpClient::~UdpClient() {
    if (m_resolver) m_resolver->cancel();
    std::lock_guard<std::recursive_mutex> g(m_core->core_mutex);
    m_core->stop();
}

void UdpClient::connect(const async::Endpoint& server, ConnectHandler handler) {
    auto core = m_core;

    core->strand.dispatch([core, server, handler]() {
        std::lock_guard<std::recursive_mutex> g(core->core_mutex);
        if (core->cstate != detail::UdpHostCore::ClientState::Idle) {
            if (handler) handler(make_error_code(async::error::already_open), DisconnectReason::None, nullptr);
            return;
        }

        if (!core->socket.is_open()) {
            std::error_code ec;
            const async::Endpoint local = server.is_v6() ? async::Endpoint::any_v6(0) : async::Endpoint::any_v4(0);

            if (!core->socket.bind(local, ec)) {
                if (handler) handler(ec, DisconnectReason::SocketError, nullptr);
                return;
            }
        }

        core->start();
        core->server_ep = server;
        core->client_salt = random_int<std::uint64_t>();
        core->cstate = detail::UdpHostCore::ClientState::Requesting;
        core->connect_deadline = detail::UdpHostCore::clock::now() + core->opt.connect_timeout;
        core->connect_handler = handler;
        core->send_request();
    });
}

void UdpClient::connect(const std::string& host, std::uint16_t port, ConnectHandler handler) {
    async::Endpoint ep;

    if (async::Endpoint::parse(host, port, ep)) {
        connect(ep, std::move(handler));
        return;
    }

    if (!m_resolver) m_resolver.reset(new async::Resolver(m_core->ctx));
    auto core = m_core;
    std::weak_ptr<detail::UdpHostCore> weak = core;

    m_resolver->async_resolve(host, port, [this, weak, handler](std::error_code ec, async::Resolver::Results results) {
        if (!weak.lock()) return;

        if (ec || results.empty()) {
            if (handler) handler(ec ? ec : make_error_code(async::error::host_not_found), DisconnectReason::None, nullptr);
            return;
        }

        connect(results.front().endpoint, handler);
    }, async::ResolveFamily::Any);
}

void UdpClient::disconnect() {
    auto core = m_core;

    core->strand.dispatch([core]() {
        std::lock_guard<std::recursive_mutex> g(core->core_mutex);
        if (core->client_peer) core->remove_peer(core->client_peer, DisconnectReason::LocalClose, true);
        else if (core->connect_handler) core->fail_connect(make_error_code(async::error::operation_aborted), DisconnectReason::LocalClose);
    });
}

UdpClient& UdpClient::on_message(UdpPeer::MessageHandler h) { m_core->on_message = std::move(h); return *this; }
UdpClient& UdpClient::on_disconnect(UdpPeer::DisconnectHandler h) { m_core->on_disconnect = std::move(h); return *this; }

bool UdpClient::send(std::uint8_t channel, const void* data, std::size_t len) {
    auto p = peer();
    return p && p->send(channel, data, len);
}

bool UdpClient::send(std::uint8_t channel, std::vector<std::uint8_t> data) {
    auto p = peer();
    return p && p->send(channel, std::move(data));
}

bool UdpClient::connected() const {
    auto p = peer();
    return p && p->connected();
}

UdpPeer::Ptr UdpClient::peer() const {
    std::lock_guard<std::mutex> lk(m_core->peers_mutex);
    auto it = m_core->peers.find(m_core->server_ep);
    return it == m_core->peers.end() ? nullptr : it->second;
}

} // namespace packet
} // namespace networking
} // namespace fizmo
