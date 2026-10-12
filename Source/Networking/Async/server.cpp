#include "fizmo_library.hpp"
#include "server.hpp"

namespace fizmo {
namespace networking {
namespace async {

namespace {

void apply_stream_options(TcpStream& s, bool no_delay, bool keep_alive) {
    std::error_code ec;
    if (no_delay) s.set_no_delay(true, ec);
    if (keep_alive) s.set_keep_alive(true, ec);
}

} // namespace

struct TcpServer::Impl : std::enable_shared_from_this<TcpServer::Impl> {
    IOContext&                                         ctx;
    security::TLSContext*                              tls;
    ServerOptions                                      opt;
    TcpAcceptor                                        acceptor;
    mutable std::mutex                                 m;
    std::unordered_map<std::uint64_t, Connection::Ptr> conns;
    std::size_t                                        pending_handshakes = 0;
    bool                                               listening = false;
    ConnectionHandler                                  on_connection;
    ErrorHandler                                       on_error;
    HandshakeErrorHandler                              on_handshake_error;

    Impl(IOContext& c, security::TLSContext* t, ServerOptions o) : ctx(c), tls(t), opt(o), acceptor(c) {}

    void accept_next() {
        auto self = shared_from_this();
        acceptor.async_accept([self](std::error_code ec, TcpStream s) { self->on_accept(ec, std::move(s)); });
    }

    void on_accept(std::error_code ec, TcpStream s) {
        bool still_listening;

        {
            std::lock_guard<std::mutex> lk(m);
            still_listening = listening;
        }

        if (ec) {
            if (ec == error::operation_aborted || !still_listening) return;
            ErrorHandler cb;
            { std::lock_guard<std::mutex> lk(m); cb = on_error; }
            if (cb) cb(ec);
            accept_next();
            return;
        }

        if (!still_listening) return;
        apply_stream_options(s, opt.no_delay, opt.keep_alive);

        {
            std::lock_guard<std::mutex> lk(m);

            if (opt.max_connections != 0 && conns.size() + pending_handshakes >= opt.max_connections) {
                s.close();
                accept_next();
                return;
            }

            if (tls && opt.max_pending_handshakes != 0 && pending_handshakes >= opt.max_pending_handshakes) {
                s.close();
                accept_next();
                return;
            }

            if (tls) ++pending_handshakes;
        }

        if (tls) {
            auto holder = std::make_shared<std::unique_ptr<TlsStream>>(new TlsStream(std::move(s), *tls));
            auto self = shared_from_this();
            const Endpoint peer = (*holder)->remote_endpoint();
            if (opt.handshake_timeout.count() > 0) (*holder)->set_handshake_timeout(opt.handshake_timeout);

            (*holder)->async_handshake(HandshakeType::Server, [self, holder, peer](std::error_code hec) {
                HandshakeErrorHandler cb;

                {
                    std::lock_guard<std::mutex> lk(self->m);
                    if (self->pending_handshakes > 0) --self->pending_handshakes;
                    cb = self->on_handshake_error;
                }

                if (hec) {
                    security::SSLError err = (*holder)->last_error();
                    if (!err) { err.code = security::SSLErrorCode::HandshakeFailed; err.detail = hec.message(); }
                    holder->reset();
                    if (cb) cb(peer, err);
                    return;
                }

                std::unique_ptr<AsyncStream> stream(holder->release());
                self->add_connection(std::move(stream));
            });

        } else {
            add_connection(std::unique_ptr<AsyncStream>(new TcpStream(std::move(s))));
        }

        accept_next();
    }

    void add_connection(std::unique_ptr<AsyncStream> stream) {
        auto conn = Connection::create(std::move(stream));
        conn->set_high_watermark(opt.high_watermark);
        if (opt.read_buffer_size != 16384) conn->set_read_buffer_size(opt.read_buffer_size);
        if (opt.idle_timeout.count() > 0) conn->set_idle_timeout(opt.idle_timeout);
        std::weak_ptr<Impl> weak = shared_from_this();

        conn->set_close_hook([weak](const Connection::Ptr& c) {
            if (auto self = weak.lock()) {
                std::lock_guard<std::mutex> lk(self->m);
                self->conns.erase(c->id());
            }
        });

        ConnectionHandler cb;

        {
            std::lock_guard<std::mutex> lk(m);
            conns[conn->id()] = conn;
            cb = on_connection;
        }

        if (cb) cb(conn);
        conn->start();
    }
};

TcpServer::TcpServer(IOContext& ctx, ServerOptions options) : m_impl(std::make_shared<Impl>(ctx, nullptr, options)) {}

TcpServer::TcpServer(IOContext& ctx, security::TLSContext& tls, ServerOptions options) : m_impl(std::make_shared<Impl>(ctx, &tls, options)) {}

TcpServer::~TcpServer() {
    stop();
    close_all();
}

TcpServer& TcpServer::on_connection(ConnectionHandler h) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->on_connection = std::move(h);
    return *this;
}

TcpServer& TcpServer::on_error(ErrorHandler h) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->on_error = std::move(h);
    return *this;
}

TcpServer& TcpServer::on_handshake_error(HandshakeErrorHandler h) {
    std::lock_guard<std::mutex> lk(m_impl->m);
    m_impl->on_handshake_error = std::move(h);
    return *this;
}

bool TcpServer::listen(const Endpoint& endpoint, std::error_code& ec) {
    if (m_impl->tls && !m_impl->tls->is_initialized() && !m_impl->tls->initialize()) {
        ec = error::tls_error;
        return false;
    }

    auto& acc = m_impl->acceptor;
    if (!acc.is_open() && !acc.open(endpoint.protocol(), ec)) return false;
    if (m_impl->opt.reuse_address && !acc.set_reuse_address(true, ec)) return false;
    if (m_impl->opt.reuse_port) acc.set_reuse_port(true, ec);
    if (endpoint.is_v6()) acc.set_v6_only(m_impl->opt.v6_only, ec);
    ec.clear();
    if (!acc.bind(endpoint, ec) || !acc.listen(m_impl->opt.backlog, ec)) return false;

    {
        std::lock_guard<std::mutex> lk(m_impl->m);
        m_impl->listening = true;
    }

    m_impl->accept_next();
    return true;
}

bool TcpServer::listen(std::uint16_t port, const std::string& host, std::error_code& ec) {
    Endpoint ep;

    if (!Endpoint::parse(host, port, ep)) {
        auto results = Resolver::resolve(host, std::to_string(port), ec, ResolveFamily::Any, ResolveFlags{ true, false, true, true, false });
        if (ec || results.empty()) { if (!ec) ec = error::host_not_found; return false; }
        ep = results.front().endpoint;
    }

    return listen(ep, ec);
}

void TcpServer::listen(std::uint16_t port, const std::string& host) {
    std::error_code ec;
    if (!listen(port, host, ec)) throw std::system_error(ec, "TcpServer::listen");
}

void TcpServer::stop() {
    {
        std::lock_guard<std::mutex> lk(m_impl->m);
        m_impl->listening = false;
    }

    m_impl->acceptor.close();
}

void TcpServer::close_all() {
    for (auto& c : connections()) c->close();
}

void TcpServer::shutdown_all() {
    for (auto& c : connections()) c->end();
}

bool TcpServer::is_listening() const noexcept {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->listening;
}

bool TcpServer::is_tls() const noexcept { return m_impl->tls != nullptr; }

Endpoint TcpServer::local_endpoint() const { return m_impl->acceptor.local_endpoint(); }

std::uint16_t TcpServer::port() const { return local_endpoint().port(); }

std::size_t TcpServer::connection_count() const {
    std::lock_guard<std::mutex> lk(m_impl->m);
    return m_impl->conns.size();
}

std::vector<Connection::Ptr> TcpServer::connections() const {
    std::vector<Connection::Ptr> out;
    std::lock_guard<std::mutex> lk(m_impl->m);
    out.reserve(m_impl->conns.size());
    for (auto& kv : m_impl->conns) out.push_back(kv.second);
    return out;
}

std::size_t TcpServer::broadcast(const std::string& data, const Connection::Ptr& except) {
    std::size_t n = 0;

    for (auto& c : connections()) {
        if (c == except || !c->is_open()) continue;
        c->write(data);
        ++n;
    }

    return n;
}

IOContext& TcpServer::context() const noexcept { return m_impl->ctx; }

struct TcpClient::Impl : std::enable_shared_from_this<TcpClient::Impl> {
    IOContext&                   ctx;
    security::TLSContext*        tls;
    ClientOptions                opt;
    Resolver                     resolver;
    security::HostFailureTracker tracker;
    std::atomic<bool>            cancelled{false};

    Impl(IOContext& c, security::TLSContext* t, ClientOptions o) : ctx(c), tls(t), opt(o), resolver(c) {
        if (tls) {
            try {
                const auto p = tls->policy();
                tracker.configure(p.max_handshake_failures, p.failure_blacklist_time);
            } catch (...) {}
        }
    }

    Connection::Ptr make_connection(std::unique_ptr<AsyncStream> stream) {
        auto conn = Connection::create(std::move(stream));
        conn->set_high_watermark(opt.high_watermark);
        if (opt.read_buffer_size != 16384) conn->set_read_buffer_size(opt.read_buffer_size);
        if (opt.idle_timeout.count() > 0) conn->set_idle_timeout(opt.idle_timeout);
        return conn;
    }

    void deliver(const ConnectHandler& h, std::error_code ec, const Connection::Ptr& conn) {
        if (h) h(ec, conn);
        if (!ec && conn) conn->start();
    }

    void try_endpoints(std::shared_ptr<std::vector<Endpoint>> eps, std::size_t i, std::string server_name, ConnectHandler h, std::error_code last) {
        if (cancelled) { deliver(h, make_error_code(error::operation_aborted), nullptr); return; }

        if (i >= eps->size()) {
            deliver(h, last ? last : make_error_code(error::host_not_found), nullptr);
            return;
        }

        auto self = shared_from_this();
        auto tcp = std::make_shared<TcpStream>(ctx);
        auto timer = std::make_shared<SteadyTimer>(ctx);
        auto timed_out = std::make_shared<bool>(false);

        if (opt.connect_timeout.count() > 0) {
            timer->expires_after(opt.connect_timeout);

            timer->async_wait([tcp, timed_out](std::error_code ec) {
                if (ec) return;
                *timed_out = true;
                tcp->close();
            });
        }

        const Endpoint target = (*eps)[i];

        tcp->async_connect(target, [self, tcp, timer, timed_out, eps, i, server_name, h, target](std::error_code ec) {
            timer->cancel();

            if (ec) {
                self->try_endpoints(eps, i + 1, server_name, h, *timed_out ? make_error_code(error::timed_out) : ec);
                return;
            }

            apply_stream_options(*tcp, self->opt.no_delay, self->opt.keep_alive);

            if (!self->tls) {
                self->deliver(h, {}, self->make_connection(std::unique_ptr<AsyncStream>(new TcpStream(std::move(*tcp)))));
                return;
            }

            auto holder = std::make_shared<std::unique_ptr<TlsStream>>(new TlsStream(std::move(*tcp), *self->tls));
            const std::string name = server_name.empty() ? target.address() : server_name;
            (*holder)->set_hostname(name);
            (*holder)->set_session_key(security::TLSSessionCache::make_key(name, target.port()));

            (*holder)->async_handshake(HandshakeType::Client, [self, holder, name, h](std::error_code hec) {
                if (hec) {
                    self->tracker.record_failure(name);
                    holder->reset();
                    self->deliver(h, hec, nullptr);
                    return;
                }

                self->tracker.record_success(name);
                std::unique_ptr<AsyncStream> stream(holder->release());
                self->deliver(h, {}, self->make_connection(std::move(stream)));
            });
        });
    }

    void connect(const std::string& host, std::uint16_t port, ConnectHandler h) {
        if (tls && tracker.is_blacklisted(host)) {
            ctx.post([h]() { if (h) h(make_error_code(error::limit_exceeded), nullptr); });
            return;
        }

        if (tls && !tls->is_initialized() && !tls->initialize()) {
            ctx.post([h]() { if (h) h(make_error_code(error::tls_error), nullptr); });
            return;
        }

        Endpoint numeric;

        if (Endpoint::parse(host, port, numeric)) {
            auto eps = std::make_shared<std::vector<Endpoint>>(1, numeric);
            auto self = shared_from_this();
            ctx.post([self, eps, host, h]() { self->try_endpoints(eps, 0, host, h, {}); });
            return;
        }

        auto self = shared_from_this();

        resolver.async_resolve(host, port, [self, host, h](std::error_code ec, Resolver::Results results) {
            if (ec) { self->deliver(h, ec, nullptr); return; }
            auto eps = std::make_shared<std::vector<Endpoint>>(Resolver::endpoints(results));
            self->try_endpoints(eps, 0, host, h, {});
        }, opt.family);
    }
};

TcpClient::TcpClient(IOContext& ctx, ClientOptions options) : m_impl(std::make_shared<Impl>(ctx, nullptr, options)) {}

TcpClient::TcpClient(IOContext& ctx, security::TLSContext& tls, ClientOptions options) : m_impl(std::make_shared<Impl>(ctx, &tls, options)) {}

TcpClient::~TcpClient() {
    cancel();
}

void TcpClient::connect(const std::string& host, std::uint16_t port, ConnectHandler handler) {
    m_impl->cancelled = false;
    m_impl->connect(host, port, std::move(handler));
}

void TcpClient::connect(const Endpoint& endpoint, ConnectHandler handler, const std::string& server_name) {
    m_impl->cancelled = false;

    if (m_impl->tls && !m_impl->tls->is_initialized() && !m_impl->tls->initialize()) {
        m_impl->ctx.post([handler]() { if (handler) handler(make_error_code(error::tls_error), nullptr); });
        return;
    }

    auto eps = std::make_shared<std::vector<Endpoint>>(1, endpoint);
    auto impl = m_impl;
    impl->ctx.post([impl, eps, server_name, handler]() { impl->try_endpoints(eps, 0, server_name, handler, {}); });
}

void TcpClient::connect_with_retry(const std::string& host, std::uint16_t port, ConnectHandler handler, std::uint32_t max_attempts) {
    auto impl = m_impl;
    impl->cancelled = false;
    auto backoff = std::make_shared<security::Backoff>(impl->opt.reconnect);
    auto attempt = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weak_attempt = attempt;

    *attempt = [impl, backoff, weak_attempt, host, port, handler, max_attempts]() {
        auto strong = weak_attempt.lock();

        impl->connect(host, port, [impl, backoff, strong, handler, max_attempts](std::error_code ec, const Connection::Ptr& conn) {
            auto self_attempt = strong;

            if (!ec || impl->cancelled || !self_attempt || (max_attempts != 0 && backoff->attempts() + 1 >= max_attempts)) {
                if (handler) handler(ec, conn);
                return;
            }

            auto timer = std::make_shared<SteadyTimer>(impl->ctx);
            timer->expires_after(backoff->next_delay());

            timer->async_wait([timer, self_attempt, impl, handler](std::error_code tec) {
                if (tec || impl->cancelled) {
                    if (handler) handler(make_error_code(error::operation_aborted), nullptr);
                    return;
                }

                (*self_attempt)();
            });
        });
    };

    auto keep = attempt;
    impl->ctx.post([keep]() { (*keep)(); });
}

void TcpClient::cancel() {
    m_impl->cancelled = true;
    m_impl->resolver.cancel();
}

bool TcpClient::is_tls() const noexcept { return m_impl->tls != nullptr; }

security::HostFailureTracker& TcpClient::failure_tracker() noexcept { return m_impl->tracker; }

IOContext& TcpClient::context() const noexcept { return m_impl->ctx; }

} // namespace async
} // namespace networking
} // namespace fizmo
