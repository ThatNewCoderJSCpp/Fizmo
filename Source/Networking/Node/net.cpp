#include "fizmo_library.hpp"
#include "net.hpp"
#include "../Async/server.hpp"
#include "../Async/tcp.hpp"
#include "../Async/tls_stream.hpp"
#include "../Async/timer.hpp"
#include "../Security/tls_session_cache.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace fizmo {
namespace networking {
namespace net {

namespace {

std::atomic<std::uint64_t> g_socket_ids{0};

std::error_code err(async::error e) { return async::make_error_code(e); }

std::string family_name(const async::Endpoint& ep) {
    if (!ep.valid()) return {};
    return ep.is_v6() ? "IPv6" : "IPv4";
}

async::TcpStream* tcp_of(async::AsyncStream& s) {
    if (auto* t = dynamic_cast<async::TcpStream*>(&s)) return t;
    if (auto* t = dynamic_cast<async::TlsStream*>(&s)) return &t->next_layer();
    return nullptr;
}

} // namespace

int is_ip(const std::string& input) noexcept {
    if (input.empty()) return 0;
    async::Endpoint ep;
    if (!async::Endpoint::parse(input, 0, ep)) return 0;
    return ep.is_v6() ? 6 : 4;
}

bool is_ipv4(const std::string& input) noexcept { return is_ip(input) == 4; }

bool is_ipv6(const std::string& input) noexcept { return is_ip(input) == 6; }

std::string AddressInfo::to_string() const {
    if (family == "IPv6") return "[" + address + "]:" + std::to_string(port);
    return address + ":" + std::to_string(port);
}

struct Socket::State : std::enable_shared_from_this<Socket::State> {
    enum class Phase { Connecting, Open, Closed };

    struct PendingWrite {
        Bytes                                        data;
        std::function<void(const std::error_code&)> done;
    };

    mutable std::mutex                    m;
    Events                                ev;
    SocketOptions                         opt;
    std::uint64_t                         id = ++g_socket_ids;
    Phase                                 phase = Phase::Connecting;
    async::Connection::Ptr                conn;
    std::vector<PendingWrite>             pending;
    std::size_t                           pending_bytes = 0;
    bool                                  end_requested = false;
    bool                                  we_ended = false;
    bool                                  peer_ended = false;
    bool                                  paused = false;
    bool                                  unrefed = false;
    bool                                  need_drain = false;
    bool                                  had_error = false;
    bool                                  close_emitted = false;
    std::error_code                       last_error;
    std::chrono::milliseconds             timeout{0};
    std::size_t                           line_limit = 0;
    std::size_t                           message_limit = 0;
    std::string                           line_buf;
    Bytes                                 msg_buf;
    std::function<void()>                 cancel_connect;
    std::shared_ptr<security::TLSContext> tls;
    bool                                  verify_enabled = false;
    std::string                           servername;
    std::string                           auth_error;
    async::Endpoint                       remote;
    async::Endpoint                       local;
    std::any                              user;
    std::function<void(const std::shared_ptr<State>&)> on_closed_hook;

    void attach(const async::Connection::Ptr& c, bool start) {
        std::vector<PendingWrite> flush;
        bool do_end = false;
        bool do_pause = false;
        bool do_unref = false;
        std::chrono::milliseconds idle{0};

        {
            std::lock_guard<std::mutex> lk(m);
            conn = c;
            phase = Phase::Open;
            remote = c->remote_endpoint();
            local = c->local_endpoint();
            flush.swap(pending);
            pending_bytes = 0;
            do_end = end_requested;
            do_pause = paused;
            do_unref = unrefed;
            idle = timeout;
        }

        auto self = shared_from_this();
        c->set_high_watermark(opt.high_water_mark);
        if (opt.read_buffer_size != 16384 && opt.read_buffer_size > 0) c->set_read_buffer_size(opt.read_buffer_size);

        c->on_data([self](const async::Connection::Ptr&, const std::uint8_t* d, std::size_t n) { self->handle_data(d, n); });
        c->on_end([self](const async::Connection::Ptr&) {
            { std::lock_guard<std::mutex> lk(self->m); self->peer_ended = true; }
            self->ev.end.emit();
        });
        c->on_error([self](const async::Connection::Ptr&, std::error_code ec) { self->handle_error(ec); });
        c->on_close([self](const async::Connection::Ptr&, std::error_code ec) { self->handle_close(ec); });
        c->on_drain([self](const async::Connection::Ptr&) {
            bool fire = false;
            { std::lock_guard<std::mutex> lk(self->m); fire = self->need_drain; self->need_drain = false; }
            if (fire) self->ev.drain.emit();
        });
        c->on_timeout([self](const async::Connection::Ptr&) { self->ev.timeout.emit(); });

        if (idle.count() > 0) c->set_idle_timeout(idle);
        if (do_pause) c->pause();
        if (do_unref) c->set_ref(false);

        for (auto& w : flush) {
            auto done = std::move(w.done);
            if (done) c->write(std::move(w.data), [done](std::error_code ec) { done(ec); });
            else c->write(std::move(w.data));
        }

        if (do_end) c->end();
        if (start) c->start();
    }

    void handle_data(const std::uint8_t* d, std::size_t n) {
        ev.data.emit(DataView(d, n));
        std::size_t line_max = 0;
        std::size_t msg_max = 0;

        {
            std::lock_guard<std::mutex> lk(m);
            if (phase == Phase::Closed) return;
            line_max = line_limit;
            msg_max = message_limit;
        }

        if (line_max > 0) {
            std::vector<std::string> lines;
            bool overflow = false;

            {
                std::lock_guard<std::mutex> lk(m);
                line_buf.append(reinterpret_cast<const char*>(d), n);
                std::size_t start = 0;

                for (std::size_t i = 0; i < line_buf.size(); ++i) {
                    if (line_buf[i] != '\n') continue;
                    std::size_t end = i;
                    if (end > start && line_buf[end - 1] == '\r') --end;
                    lines.emplace_back(line_buf, start, end - start);
                    start = i + 1;
                }

                line_buf.erase(0, start);
                overflow = line_buf.size() > line_max;
            }

            for (auto& l : lines) ev.line.emit(l);
            if (overflow) { fail_and_destroy(err(async::error::limit_exceeded)); return; }
        }

        if (msg_max > 0) {
            std::vector<Bytes> msgs;
            bool overflow = false;

            {
                std::lock_guard<std::mutex> lk(m);
                msg_buf.insert(msg_buf.end(), d, d + n);
                std::size_t pos = 0;

                while (msg_buf.size() - pos >= 4) {
                    const std::uint8_t* p = msg_buf.data() + pos;
                    const std::size_t len = (std::size_t(p[0]) << 24) | (std::size_t(p[1]) << 16) | (std::size_t(p[2]) << 8) | std::size_t(p[3]);
                    if (len > msg_max) { overflow = true; break; }
                    if (msg_buf.size() - pos - 4 < len) break;
                    msgs.emplace_back(p + 4, p + 4 + len);
                    pos += 4 + len;
                }

                msg_buf.erase(msg_buf.begin(), msg_buf.begin() + static_cast<std::ptrdiff_t>(pos));
            }

            for (auto& b : msgs) ev.message.emit(DataView(b));
            if (overflow) fail_and_destroy(err(async::error::limit_exceeded));
        }
    }

    void handle_error(std::error_code ec) {
        {
            std::lock_guard<std::mutex> lk(m);
            had_error = true;
            last_error = ec;
        }

        emit_error(ec);
    }

    static bool routine_disconnect(const std::error_code& ec) {
        return ec == async::error::connection_reset || ec == async::error::connection_aborted || ec == async::error::eof
            || ec == async::error::operation_aborted || ec == async::error::shut_down
            || ec == std::errc::connection_reset || ec == std::errc::connection_aborted || ec == std::errc::broken_pipe;
    }

    void emit_error(const std::error_code& ec) {
        if (ev.error.emit(ec) == 0 && !routine_disconnect(ec)) report_unhandled_error("net::Socket", ec);
    }

    void handle_close(std::error_code ec) {
        bool had;

        {
            std::lock_guard<std::mutex> lk(m);
            if (close_emitted) return;
            close_emitted = true;
            phase = Phase::Closed;
            if (ec && ec != async::error::operation_aborted && ec != async::error::eof && !last_error) last_error = ec;
            had = had_error;
        }

        ev.close.emit(had);
        auto self = shared_from_this();
        std::function<void(const std::shared_ptr<State>&)> hook;

        {
            std::lock_guard<std::mutex> lk(m);
            hook = std::move(on_closed_hook);
            on_closed_hook = nullptr;
            cancel_connect = nullptr;
        }

        if (hook) hook(self);
        clear_events();
    }

    void clear_events() {
        ev.connect.clear();
        ev.ready.clear();
        ev.secure_connect.clear();
        ev.data.clear();
        ev.line.clear();
        ev.message.clear();
        ev.end.clear();
        ev.close.clear();
        ev.error.clear();
        ev.drain.clear();
        ev.timeout.clear();
        ev.lookup.clear();
    }

    void fail_and_destroy(const std::error_code& ec) {
        async::Connection::Ptr c;

        {
            std::lock_guard<std::mutex> lk(m);
            if (phase == Phase::Closed) return;
            had_error = true;
            last_error = ec;
            c = conn;
        }

        emit_error(ec);

        if (c) {
            c->destroy(ec);
            return;
        }

        fail_connect(ec, false);
    }

    void fail_connect(const std::error_code& ec, bool emit) {
        std::vector<PendingWrite> dropped;
        std::function<void()> cancel;

        {
            std::lock_guard<std::mutex> lk(m);
            if (phase != Phase::Connecting) return;
            had_error = had_error || emit;
            if (emit) last_error = ec;
            dropped.swap(pending);
            pending_bytes = 0;
            cancel = std::move(cancel_connect);
            cancel_connect = nullptr;
        }

        if (cancel) cancel();
        for (auto& w : dropped) if (w.done) w.done(ec);
        if (emit) emit_error(ec);
        handle_close(ec);
    }
};

Socket& Socket::on_connect(std::function<void()> fn) { m_state->ev.connect.add(std::move(fn)); return *this; }
Socket& Socket::on_ready(std::function<void()> fn) { m_state->ev.ready.add(std::move(fn)); return *this; }
Socket& Socket::on_secure_connect(std::function<void()> fn) { m_state->ev.secure_connect.add(std::move(fn)); return *this; }
Socket& Socket::on_data(std::function<void(const DataView&)> fn) { m_state->ev.data.add(std::move(fn)); return *this; }

Socket& Socket::on_line(std::function<void(const std::string&)> fn, std::size_t max_line) {
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->line_limit = max_line == 0 ? 1 : max_line; }
    m_state->ev.line.add(std::move(fn));
    return *this;
}

Socket& Socket::on_message(std::function<void(const DataView&)> fn, std::size_t max_message) {
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->message_limit = max_message == 0 ? 1 : max_message; }
    m_state->ev.message.add(std::move(fn));
    return *this;
}

Socket& Socket::on_end(std::function<void()> fn) { m_state->ev.end.add(std::move(fn)); return *this; }
Socket& Socket::on_close(std::function<void(bool)> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
Socket& Socket::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
Socket& Socket::on_drain(std::function<void()> fn) { m_state->ev.drain.add(std::move(fn)); return *this; }
Socket& Socket::on_timeout(std::function<void()> fn) { m_state->ev.timeout.add(std::move(fn)); return *this; }
Socket& Socket::on_lookup(std::function<void(const std::error_code&, const std::string&, Family)> fn) { m_state->ev.lookup.add(std::move(fn)); return *this; }
Socket::Events& Socket::events() { return m_state->ev; }

bool Socket::write(const DataView& data, std::function<void(const std::error_code&)> done) {
    auto st = m_state;
    async::Connection::Ptr c;

    {
        std::lock_guard<std::mutex> lk(st->m);

        if (st->phase == State::Phase::Closed || st->we_ended || st->end_requested) {
            const auto ec = err(st->phase == State::Phase::Closed ? async::error::closed : async::error::shut_down);
            if (done) loop().post([done, ec]() { done(ec); });
            return false;
        }

        if (!st->conn) {
            st->pending.push_back(State::PendingWrite{ data.bytes(), std::move(done) });
            st->pending_bytes += data.size();
            const bool ok = st->pending_bytes < st->opt.high_water_mark;
            if (!ok) st->need_drain = true;
            return ok;
        }

        c = st->conn;
    }

    bool ok;
    if (done) ok = c->write(data.bytes(), [done](std::error_code ec) { done(ec); });
    else ok = c->write(data.bytes());

    if (!ok) {
        std::lock_guard<std::mutex> lk(st->m);
        st->need_drain = true;
    }

    return ok;
}

bool Socket::write_line(const std::string& line) { return write(DataView(line + "\n")); }

bool Socket::write_message(const DataView& data) {
    Bytes framed(4 + data.size());
    const std::size_t len = data.size();
    framed[0] = static_cast<std::uint8_t>(len >> 24);
    framed[1] = static_cast<std::uint8_t>(len >> 16);
    framed[2] = static_cast<std::uint8_t>(len >> 8);
    framed[3] = static_cast<std::uint8_t>(len);
    if (len) std::memcpy(framed.data() + 4, data.data(), len);
    return write(DataView(framed));
}

Socket& Socket::end(std::function<void()> done) {
    auto st = m_state;
    async::Connection::Ptr c;

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->phase == State::Phase::Closed || st->we_ended || st->end_requested) return *this;

        if (!st->conn) {
            st->end_requested = true;
            if (done) st->pending.push_back(State::PendingWrite{ Bytes(), [done](const std::error_code&) { done(); } });
            return *this;
        }

        st->we_ended = true;
        c = st->conn;
    }

    if (done) c->write(Bytes(), [done](std::error_code) { done(); });
    c->end();
    return *this;
}

Socket& Socket::end(const DataView& data, std::function<void()> done) {
    if (!data.empty()) write(data);
    return end(std::move(done));
}

Socket& Socket::destroy() {
    auto st = m_state;
    async::Connection::Ptr c;
    bool connecting = false;

    {
        std::lock_guard<std::mutex> lk(st->m);
        if (st->phase == State::Phase::Closed) return *this;
        c = st->conn;
        connecting = !c;
    }

    if (c) c->close();
    else if (connecting) loop().post([st]() { st->fail_connect(err(async::error::operation_aborted), false); });
    return *this;
}

Socket& Socket::destroy(const std::error_code& reason) {
    if (!reason) return destroy();
    auto st = m_state;
    loop().dispatch([st, reason]() { st->fail_and_destroy(reason); });
    return *this;
}

Socket& Socket::destroy_soon() { return end(); }

Socket& Socket::pause() {
    async::Connection::Ptr c;
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->paused = true; c = m_state->conn; }
    if (c) c->pause();
    return *this;
}

Socket& Socket::resume() {
    async::Connection::Ptr c;
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->paused = false; c = m_state->conn; }
    if (c) c->resume();
    return *this;
}

Socket& Socket::set_timeout(std::chrono::milliseconds timeout, std::function<void()> fn) {
    if (fn) m_state->ev.timeout.once(std::move(fn));
    async::Connection::Ptr c;
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->timeout = timeout; c = m_state->conn; }
    if (c) c->set_idle_timeout(timeout);
    return *this;
}

Socket& Socket::set_no_delay(bool enable) {
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->opt.no_delay = enable; }
    auto c = connection();
    if (c) if (auto* t = tcp_of(c->stream())) t->set_no_delay(enable);
    return *this;
}

Socket& Socket::set_keep_alive(bool enable) {
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->opt.keep_alive = enable; }
    auto c = connection();
    if (c) if (auto* t = tcp_of(c->stream())) t->set_keep_alive(enable);
    return *this;
}

Socket& Socket::ref() {
    async::Connection::Ptr c;
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->unrefed = false; c = m_state->conn; }
    if (c) c->set_ref(true);
    return *this;
}

Socket& Socket::unref() {
    async::Connection::Ptr c;
    { std::lock_guard<std::mutex> lk(m_state->m); m_state->unrefed = true; c = m_state->conn; }
    if (c) c->set_ref(false);
    return *this;
}

Socket Socket::pipe(Socket destination, bool end_destination) {
    Socket src = *this;
    std::weak_ptr<State> weak_src = m_state;
    std::weak_ptr<State> weak_dst = destination.m_state;

    on_data([weak_src, weak_dst](const DataView& d) {
        auto s = weak_src.lock();
        auto t = weak_dst.lock();
        if (!s || !t) return;
        if (!Socket(t).write(d)) Socket(s).pause();
    });

    destination.on_drain([weak_src]() { if (auto s = weak_src.lock()) Socket(s).resume(); });

    if (end_destination) {
        on_end([weak_dst]() { if (auto t = weak_dst.lock()) Socket(t).end(); });
    }

    return destination;
}

std::uint64_t Socket::id() const noexcept { return m_state ? m_state->id : 0; }

async::Endpoint Socket::remote_endpoint() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->remote;
}

async::Endpoint Socket::local_endpoint() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->local;
}

std::string Socket::remote_address() const { auto ep = remote_endpoint(); return ep.valid() ? ep.to_v4_unmapped().address() : std::string(); }
std::uint16_t Socket::remote_port() const { auto ep = remote_endpoint(); return ep.valid() ? ep.port() : 0; }
std::string Socket::remote_family() const { auto ep = remote_endpoint(); return ep.valid() ? family_name(ep.to_v4_unmapped()) : std::string(); }
std::string Socket::local_address() const { auto ep = local_endpoint(); return ep.valid() ? ep.to_v4_unmapped().address() : std::string(); }
std::uint16_t Socket::local_port() const { auto ep = local_endpoint(); return ep.valid() ? ep.port() : 0; }

AddressInfo Socket::address() const {
    auto ep = local_endpoint();
    AddressInfo a;
    if (!ep.valid()) return a;
    ep = ep.to_v4_unmapped();
    a.address = ep.address();
    a.family = family_name(ep);
    a.port = ep.port();
    return a;
}

bool Socket::connecting() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->phase == State::Phase::Connecting;
}

bool Socket::pending() const noexcept { return connecting(); }

bool Socket::destroyed() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->phase == State::Phase::Closed;
}

bool Socket::is_paused() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->paused;
}

bool Socket::writable() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->phase != State::Phase::Closed && !m_state->we_ended && !m_state->end_requested;
}

std::string Socket::ready_state() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (m_state->phase == State::Phase::Connecting) return "opening";
    if (m_state->phase == State::Phase::Closed) return "closed";
    if (m_state->we_ended && m_state->peer_ended) return "closed";
    if (m_state->we_ended) return "readOnly";
    if (m_state->peer_ended) return "writeOnly";
    return "open";
}

std::size_t Socket::bytes_read() const noexcept { auto c = connection(); return c ? static_cast<std::size_t>(c->bytes_read()) : 0; }
std::size_t Socket::bytes_written() const noexcept { auto c = connection(); return c ? static_cast<std::size_t>(c->bytes_written()) : 0; }

std::size_t Socket::buffered_amount() const noexcept {
    async::Connection::Ptr c;

    {
        std::lock_guard<std::mutex> lk(m_state->m);
        if (!m_state->conn) return m_state->pending_bytes;
        c = m_state->conn;
    }

    return c->buffered_amount();
}

std::error_code Socket::last_error() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->last_error;
}

namespace {

async::TlsStream* tls_of(const async::Connection::Ptr& c) {
    if (!c) return nullptr;
    return dynamic_cast<async::TlsStream*>(&c->stream());
}

} // namespace

bool Socket::encrypted() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->tls != nullptr || (m_state->conn && m_state->conn->is_secure());
}

bool Socket::authorized() const noexcept {
    auto c = connection();
    std::lock_guard<std::mutex> lk(m_state->m);
    return c && c->is_secure() && m_state->verify_enabled && m_state->auth_error.empty();
}

std::string Socket::authorization_error() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->auth_error.empty()) return m_state->auth_error;
    if (!m_state->verify_enabled && (m_state->tls || (m_state->conn && m_state->conn->is_secure()))) return "CERTIFICATE_VERIFICATION_DISABLED";
    return {};
}

std::string Socket::alpn_protocol() const { auto* t = tls_of(connection()); return t ? t->negotiated_alpn() : std::string(); }

std::string Socket::protocol() const {
    auto* t = tls_of(connection());
    if (!t) return {};
    switch (t->negotiated_protocol_version()) {
        case security::TLSVersion::TLS_1_0: return "TLSv1";
        case security::TLSVersion::TLS_1_1: return "TLSv1.1";
        case security::TLSVersion::TLS_1_2: return "TLSv1.2";
        case security::TLSVersion::TLS_1_3: return "TLSv1.3";
        default: return "unknown";
    }
}

std::string Socket::cipher() const { auto* t = tls_of(connection()); return t ? t->negotiated_cipher() : std::string(); }

std::string Socket::servername() const {
    auto* t = tls_of(connection());
    if (t) { auto s = t->requested_server_name(); if (!s.empty()) return s; }
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->servername;
}

bool Socket::session_reused() const { auto* t = tls_of(connection()); return t && t->session_resumed(); }

bool Socket::peer_certificate(security::Certificate& out) const { auto* t = tls_of(connection()); return t && t->peer_certificate(out); }

std::any& Socket::data() { return m_state->user; }

async::Connection::Ptr Socket::connection() const {
    if (!m_state) return nullptr;
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->conn;
}

struct Server::State : std::enable_shared_from_this<Server::State> {
    mutable std::mutex                                       m;
    Events                                                   ev;
    ServerOptions                                            opt;
    std::shared_ptr<security::TLSContext>                    tls;
    std::unique_ptr<async::TcpServer>                        server;
    std::unordered_map<std::uint64_t, Socket>                sockets;
    std::vector<std::function<void(const std::error_code&)>> close_callbacks;
    std::shared_ptr<State>                                   keep_alive;
    bool                                                     listening = false;
    bool                                                     closing = false;

    void on_connection(const async::Connection::Ptr& c) {
        std::size_t limit;
        std::size_t count;

        {
            std::lock_guard<std::mutex> lk(m);
            limit = opt.max_connections;
            count = sockets.size();
        }

        if (limit != 0 && count >= limit) {
            const auto ep = c->remote_endpoint();
            c->close();
            ev.drop.emit(ep);
            return;
        }

        auto st = std::make_shared<Socket::State>();
        st->opt = opt;
        st->tls = tls;
        st->paused = opt.pause_on_connect;
        st->timeout = opt.timeout;
        st->verify_enabled = tls && tls->verify_mode() != security::VerifyMode::None;
        std::weak_ptr<State> weak = shared_from_this();

        st->on_closed_hook = [weak](const std::shared_ptr<Socket::State>& s) {
            auto self = weak.lock();
            if (!self) return;
            self->remove_socket(s->id);
        };

        Socket sock(st);

        {
            std::lock_guard<std::mutex> lk(m);
            sockets[st->id] = sock;
        }

        st->attach(c, false);
        if (auto* t = dynamic_cast<async::TcpStream*>(&c->stream())) { if (opt.keep_alive) t->set_keep_alive(true); }
        ev.connection.emit(sock);
    }

    void remove_socket(std::uint64_t id) {
        bool finish = false;

        {
            std::lock_guard<std::mutex> lk(m);
            sockets.erase(id);
            finish = closing && sockets.empty();
        }

        if (finish) finish_close();
    }

    void finish_close() {
        std::vector<std::function<void(const std::error_code&)>> cbs;
        std::shared_ptr<State> keep;

        {
            std::lock_guard<std::mutex> lk(m);
            if (!closing) return;
            closing = false;
            cbs.swap(close_callbacks);
            keep = std::move(keep_alive);
            keep_alive.reset();
        }

        ev.close.emit();
        for (auto& cb : cbs) cb({});
    }

    std::unique_ptr<async::TcpServer> make_tcp_server(const ListenOptions& lo) {
        async::ServerOptions so;
        so.backlog = lo.backlog;
        so.v6_only = lo.ipv6_only;
        so.reuse_port = lo.reuse_port;
        so.no_delay = opt.no_delay;
        so.keep_alive = opt.keep_alive;
        so.handshake_timeout = opt.handshake_timeout;
        so.read_buffer_size = opt.read_buffer_size;
        so.high_watermark = opt.high_water_mark;
        std::unique_ptr<async::TcpServer> s;
        if (tls) s.reset(new async::TcpServer(loop(), *tls, so));
        else s.reset(new async::TcpServer(loop(), so));
        std::weak_ptr<State> weak = shared_from_this();

        s->on_connection([weak](const async::Connection::Ptr& c) {
            if (auto self = weak.lock()) self->on_connection(c);
            else c->close();
        });

        s->on_error([weak](std::error_code ec) {
            auto self = weak.lock();
            if (self && self->ev.error.emit(ec) == 0) report_unhandled_error("net::Server", ec);
        });

        s->on_handshake_error([weak](const async::Endpoint& peer, const security::SSLError& e) {
            auto self = weak.lock();
            if (self) self->ev.tls_client_error.emit(err(async::error::handshake_failed), e.to_string(), peer);
        });

        return s;
    }
};

Server& Server::listen(std::uint16_t port, std::function<void()> cb) {
    ListenOptions o;
    o.port = port;
    return listen(o, std::move(cb));
}

Server& Server::listen(std::uint16_t port, const std::string& host, std::function<void()> cb) {
    ListenOptions o;
    o.port = port;
    o.host = host;
    return listen(o, std::move(cb));
}

Server& Server::listen(const ListenOptions& options, std::function<void()> cb) {
    auto st = m_state;
    if (cb) st->ev.listening.once(std::move(cb));

    {
        std::lock_guard<std::mutex> lk(st->m);

        if (st->listening) {
            loop().post([st]() { if (st->ev.error.emit(err(async::error::already_open)) == 0) report_unhandled_error("net::Server", err(async::error::already_open)); });
            return *this;
        }
    }

    if (st->tls && !st->tls->is_initialized() && !st->tls->initialize()) {
        loop().post([st]() { if (st->ev.error.emit(err(async::error::tls_error)) == 0) report_unhandled_error("net::Server: " + st->tls->last_error(), err(async::error::tls_error)); });
        return *this;
    }

    std::vector<async::Endpoint> candidates;

    if (options.host.empty()) {
        candidates.push_back(async::Endpoint::any_v6(options.port));
        candidates.push_back(async::Endpoint::any_v4(options.port));
    } else {
        async::Endpoint ep;

        if (async::Endpoint::parse(options.host, options.port, ep)) {
            candidates.push_back(ep);
        } else {
            std::error_code rec;
            async::ResolveFlags flags;
            flags.passive = true;
            auto results = async::Resolver::resolve(options.host, std::to_string(options.port), rec, async::ResolveFamily::Any, flags);
            for (auto& r : results) candidates.push_back(r.endpoint);
        }
    }

    std::error_code ec = err(async::error::host_not_found);

    for (auto& ep : candidates) {
        auto s = st->make_tcp_server(options);
        ec.clear();
        if (!s->listen(ep, ec)) continue;

        {
            std::lock_guard<std::mutex> lk(st->m);
            st->server = std::move(s);
            st->listening = true;
            st->closing = false;
            st->keep_alive = st;
        }

        loop().post([st]() { st->ev.listening.emit(); });
        return *this;
    }

    loop().post([st, ec]() { if (st->ev.error.emit(ec) == 0) report_unhandled_error("net::Server::listen", ec); });
    return *this;
}

Server& Server::close(std::function<void(const std::error_code&)> cb) {
    auto st = m_state;
    bool finish = false;

    {
        std::lock_guard<std::mutex> lk(st->m);

        if (!st->listening) {
            if (cb) loop().post([cb]() { cb(err(async::error::not_open)); });
            return *this;
        }

        st->listening = false;
        st->closing = true;
        if (cb) st->close_callbacks.push_back(std::move(cb));
        if (st->server) st->server->stop();
        finish = st->sockets.empty();
    }

    if (finish) loop().post([st]() { st->finish_close(); });
    return *this;
}

Server& Server::close_all_connections() {
    for (auto& s : connections()) s.destroy();
    return *this;
}

Server& Server::end_all_connections() {
    for (auto& s : connections()) s.end();
    return *this;
}

Server& Server::on_connection(std::function<void(Socket)> fn) { m_state->ev.connection.add(std::move(fn)); return *this; }
Server& Server::on_listening(std::function<void()> fn) { m_state->ev.listening.add(std::move(fn)); return *this; }
Server& Server::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
Server& Server::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
Server& Server::on_tls_client_error(std::function<void(const std::error_code&, const std::string&, const async::Endpoint&)> fn) { m_state->ev.tls_client_error.add(std::move(fn)); return *this; }
Server& Server::on_drop(std::function<void(const async::Endpoint&)> fn) { m_state->ev.drop.add(std::move(fn)); return *this; }
Server::Events& Server::events() { return m_state->ev; }

Server& Server::set_max_connections(std::size_t max) {
    std::lock_guard<std::mutex> lk(m_state->m);
    m_state->opt.max_connections = max;
    return *this;
}

std::size_t Server::max_connections() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->opt.max_connections;
}

bool Server::listening() const noexcept {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->listening;
}

bool Server::secure() const noexcept { return m_state->tls != nullptr; }

AddressInfo Server::address() const {
    AddressInfo a;
    std::lock_guard<std::mutex> lk(m_state->m);
    if (!m_state->server) return a;
    auto ep = m_state->server->local_endpoint();
    if (!ep.valid()) return a;
    a.address = ep.address();
    a.family = family_name(ep);
    a.port = ep.port();
    return a;
}

std::uint16_t Server::port() const { return address().port; }

std::size_t Server::connection_count() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return m_state->sockets.size();
}

std::vector<Socket> Server::connections() const {
    std::vector<Socket> out;
    std::lock_guard<std::mutex> lk(m_state->m);
    out.reserve(m_state->sockets.size());
    for (auto& kv : m_state->sockets) out.push_back(kv.second);
    return out;
}

std::size_t Server::broadcast(const DataView& data, const Socket& except) {
    std::size_t n = 0;

    for (auto& s : connections()) {
        if (s == except || !s.writable()) continue;
        s.write(data);
        ++n;
    }

    return n;
}

std::shared_ptr<security::TLSContext> Server::tls_context() const { return m_state->tls; }

namespace detail {

Server make_server(const ServerOptions& options, std::shared_ptr<security::TLSContext> tls) {
    auto st = std::make_shared<Server::State>();
    st->opt = options;
    st->tls = std::move(tls);
    return Server(st);
}

namespace {

struct ConnectAttempt : std::enable_shared_from_this<ConnectAttempt> {
    std::shared_ptr<Socket::State>        st;
    ConnectOptions                        opt;
    std::string                           servername;
    std::vector<async::Endpoint>          endpoints;
    std::size_t                           index = 0;
    std::error_code                       last;
    std::mutex                            m;
    std::shared_ptr<async::TcpStream>     tcp;
    std::shared_ptr<async::SteadyTimer>   timer;
    std::shared_ptr<std::unique_ptr<async::TlsStream>> tls_holder;
    bool                                  aborted = false;

    void cancel() {
        std::lock_guard<std::mutex> lk(m);
        aborted = true;
        if (timer) timer->cancel();
        if (tcp) tcp->close();
        if (tls_holder && *tls_holder) (*tls_holder)->close();
    }

    bool is_aborted() {
        std::lock_guard<std::mutex> lk(m);
        return aborted;
    }

    void start() {
        auto self = shared_from_this();
        async::Endpoint numeric;

        if (async::Endpoint::parse(opt.host, opt.port, numeric)) {
            endpoints.push_back(numeric);
            next();
            return;
        }

        async::ResolveFamily fam = async::ResolveFamily::Any;
        if (opt.family == Family::IPv4) fam = async::ResolveFamily::V4;
        if (opt.family == Family::IPv6) fam = async::ResolveFamily::V6;

        networking::detail::default_resolver().async_resolve(opt.host, opt.port, [self](std::error_code ec, async::Resolver::Results results) {
            if (self->is_aborted()) return;
            std::string first;
            Family fam_out = Family::Any;

            if (!ec && !results.empty()) {
                first = results.front().endpoint.address();
                fam_out = results.front().endpoint.is_v6() ? Family::IPv6 : Family::IPv4;
            }

            self->st->ev.lookup.emit(ec, first, fam_out);

            if (ec || results.empty()) {
                self->st->fail_connect(ec ? ec : err(async::error::host_not_found), true);
                return;
            }

            self->endpoints = async::Resolver::endpoints(results);
            self->next();
        }, fam);
    }

    void next() {
        if (is_aborted()) return;

        if (index >= endpoints.size()) {
            st->fail_connect(last ? last : err(async::error::host_not_found), true);
            return;
        }

        const async::Endpoint target = endpoints[index++];
        auto self = shared_from_this();
        auto stream = std::make_shared<async::TcpStream>(loop());
        auto t = std::make_shared<async::SteadyTimer>(loop());
        auto timed_out = std::make_shared<std::atomic<bool>>(false);

        {
            std::lock_guard<std::mutex> lk(m);
            tcp = stream;
            timer = t;
        }

        if (opt.connect_timeout.count() > 0) {
            t->expires_after(opt.connect_timeout);
            t->async_wait([stream, timed_out](std::error_code ec) {
                if (ec) return;
                *timed_out = true;
                stream->close();
            });
        }

        stream->async_connect(target, [self, stream, t, timed_out, target](std::error_code ec) {
            t->cancel();
            if (self->is_aborted()) return;

            if (ec) {
                self->last = *timed_out ? err(async::error::timed_out) : ec;
                self->next();
                return;
            }

            std::error_code oec;
            if (self->opt.no_delay) stream->set_no_delay(true, oec);
            if (self->opt.keep_alive) stream->set_keep_alive(true, oec);

            if (!self->st->tls) {
                auto conn = async::Connection::create(std::unique_ptr<async::AsyncStream>(new async::TcpStream(std::move(*stream))));
                self->finish(conn);
                return;
            }

            auto holder = std::make_shared<std::unique_ptr<async::TlsStream>>(new async::TlsStream(std::move(*stream), *self->st->tls));

            {
                std::lock_guard<std::mutex> lk(self->m);
                self->tls_holder = holder;
                self->tcp.reset();
            }

            const std::string name = self->servername.empty() ? (is_ip(self->opt.host) ? target.address() : self->opt.host) : self->servername;
            (*holder)->set_hostname(name);
            (*holder)->set_session_key(security::TLSSessionCache::make_key(name, target.port()));
            if (self->opt.connect_timeout.count() > 0) (*holder)->set_handshake_timeout(self->opt.connect_timeout);
            self->st->ev.connect.emit();

            (*holder)->async_handshake(async::HandshakeType::Client, [self, holder](std::error_code hec) {
                if (self->is_aborted()) return;

                if (hec) {
                    security::SSLError e = (*holder)->last_error();
                    std::string detail = e ? e.to_string() : hec.message();

                    {
                        std::lock_guard<std::mutex> lk(self->st->m);
                        self->st->auth_error = detail;
                    }

                    holder->reset();
                    const auto code = (e.code == security::SSLErrorCode::CertificateUntrusted || e.code == security::SSLErrorCode::HostnameMismatch || e.code == security::SSLErrorCode::CertificateExpired || e.code == security::SSLErrorCode::CertificateRevoked || e.code == security::SSLErrorCode::CertificateInvalid || e.code == security::SSLErrorCode::PinViolation) ? err(async::error::certificate_error) : hec;
                    self->st->fail_connect(code, true);
                    return;
                }

                std::unique_ptr<async::AsyncStream> s(holder->release());
                self->finish(async::Connection::create(std::move(s)));
            });
        });
    }

    void finish(const async::Connection::Ptr& conn) {
        {
            std::lock_guard<std::mutex> lk(st->m);

            if (st->phase != Socket::State::Phase::Connecting) {
                conn->close();
                return;
            }

            st->cancel_connect = nullptr;
        }

        const bool secure = st->tls != nullptr;
        st->attach(conn, false);
        if (!secure) st->ev.connect.emit();
        if (secure) st->ev.secure_connect.emit();
        st->ev.ready.emit();
        conn->start();
    }
};

} // namespace

Socket make_client(const ConnectOptions& options, std::shared_ptr<security::TLSContext> tls, const std::string& servername) {
    auto st = std::make_shared<Socket::State>();
    st->opt = options;
    st->timeout = options.timeout;
    st->tls = tls;
    st->servername = servername.empty() ? options.host : servername;
    st->verify_enabled = tls && tls->verify_mode() != security::VerifyMode::None;
    auto attempt = std::make_shared<ConnectAttempt>();
    attempt->st = st;
    attempt->opt = options;
    attempt->servername = servername;
    std::weak_ptr<ConnectAttempt> weak = attempt;
    st->cancel_connect = [weak]() { if (auto a = weak.lock()) a->cancel(); };

    if (tls && !tls->is_initialized() && !tls->initialize()) {
        std::string why = tls->last_error();
        loop().post([st, why]() {
            { std::lock_guard<std::mutex> lk(st->m); st->auth_error = why; }
            st->fail_connect(err(async::error::tls_error), true);
        });
        return Socket(st);
    }

    loop().post([attempt]() { attempt->start(); });
    return Socket(st);
}

Socket make_failed(const std::error_code& ec, const std::string& detail, bool secure) {
    auto st = std::make_shared<Socket::State>();
    if (secure) st->tls = std::make_shared<security::TLSContext>(security::ContextUse::Client);
    st->auth_error = detail;
    loop().post([st, ec]() { st->fail_connect(ec, true); });
    return Socket(st);
}

Socket adopt(async::Connection::Ptr connection) {
    auto st = std::make_shared<Socket::State>();
    st->verify_enabled = false;
    st->attach(connection, true);
    return Socket(st);
}

} // namespace detail

Server create_server(std::function<void(Socket)> on_connection, const ServerOptions& options) {
    Server s = detail::make_server(options, nullptr);
    if (on_connection) s.on_connection(std::move(on_connection));
    return s;
}

Server create_server(const ServerOptions& options, std::function<void(Socket)> on_connection) {
    return create_server(std::move(on_connection), options);
}

Socket connect(std::uint16_t port, const std::string& host, std::function<void()> on_connect) {
    ConnectOptions o;
    o.port = port;
    o.host = host.empty() ? "localhost" : host;
    return connect(o, std::move(on_connect));
}

Socket connect(const ConnectOptions& options, std::function<void()> on_connect) {
    Socket s = detail::make_client(options, nullptr, {});
    if (on_connect) s.events().connect.once(std::move(on_connect));
    return s;
}

} // namespace net
} // namespace networking
} // namespace fizmo
