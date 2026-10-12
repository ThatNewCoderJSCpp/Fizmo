#include "fizmo_library.hpp"
#include "tcp.hpp"

namespace fizmo {
namespace networking {
namespace async {

namespace {

[[noreturn]] void throw_error(const std::error_code& ec, const char* what) {
    throw std::system_error(ec, what);
}

int family_of(Protocol p) noexcept { return p == Protocol::V6 ? AF_INET6 : AF_INET; }

} // namespace

TcpStream::TcpStream(IOContext& ctx) noexcept : m_ctx(&ctx), m_fd(invalid_socket) {}

TcpStream::TcpStream(IOContext& ctx, Protocol protocol) : TcpStream(ctx) {
    open(protocol);
}

TcpStream::~TcpStream() {
    close();
}

TcpStream::TcpStream(TcpStream&& other) noexcept
    : m_ctx(other.m_ctx), m_fd(other.m_fd), m_desc(std::move(other.m_desc)), m_deadline(std::move(other.m_deadline)) {
    other.m_fd = invalid_socket;
}

TcpStream& TcpStream::operator=(TcpStream&& other) noexcept {
    if (this != &other) {
        close();
        m_ctx = other.m_ctx;
        m_fd = other.m_fd;
        m_desc = std::move(other.m_desc);
        m_deadline = std::move(other.m_deadline);
        other.m_fd = invalid_socket;
    }

    return *this;
}

void TcpStream::open(Protocol protocol) {
    std::error_code ec;
    if (!open(protocol, ec)) throw_error(ec, "TcpStream::open");
}

bool TcpStream::open(Protocol protocol, std::error_code& ec) {
    if (is_open()) { ec = error::already_open; return false; }
    native_socket_t fd = detail::open_socket(family_of(protocol), SOCK_STREAM, IPPROTO_TCP, ec);
    if (fd == invalid_socket) return false;

    if (!assign(fd, ec)) {
        detail::close_socket(fd);
        return false;
    }

    return true;
}

bool TcpStream::assign(native_socket_t fd, std::error_code& ec) {
    ec.clear();
    if (is_open()) { ec = error::already_open; return false; }
    if (!detail::set_nonblocking(fd, true, ec)) return false;
    auto d = m_ctx->reactor().register_descriptor(fd, ec);
    if (!d) return false;
    m_fd = fd;
    m_desc = std::move(d);
    return true;
}

void TcpStream::close() {
    std::error_code ec;
    close(ec);
}

void TcpStream::close(std::error_code& ec) {
    ec.clear();
    if (m_deadline) m_deadline->cancel();
    if (!is_open()) return;
    m_ctx->reactor().deregister_descriptor(m_desc);
    detail::close_socket(m_fd);
    m_fd = invalid_socket;
}

native_socket_t TcpStream::release() {
    if (!is_open()) return invalid_socket;
    if (m_deadline) m_deadline->cancel();
    m_ctx->reactor().deregister_descriptor(m_desc);
    native_socket_t fd = m_fd;
    m_fd = invalid_socket;
    return fd;
}

void TcpStream::cancel() {
    if (m_desc) m_ctx->reactor().cancel_ops(m_desc);
}

void TcpStream::post_result(IoHandler handler, std::error_code ec, std::size_t n) {
    m_ctx->post([h = std::move(handler), ec, n]() mutable { h(ec, n); });
}

void TcpStream::start(detail::Reactor::OpType type, UniqueFunction<bool(std::error_code&, std::size_t&)> perform, IoHandler handler, bool speculative) {
    m_ctx->work_started();
    detail::ReactorOpPtr op(new detail::ReactorOp());
    op->perform = std::move(perform);
    op->handler = std::move(handler);
    m_ctx->reactor().start_op(m_desc, type, std::move(op), speculative);
}

void TcpStream::async_connect(const Endpoint& endpoint, ErrorHandler handler) {
    std::error_code ec;

    if (!is_open() && !open(endpoint.protocol(), ec)) {
        post_result([h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); }, ec, 0);
        return;
    }

    if (detail::start_connect(m_fd, endpoint, ec) || !is_would_block(ec)) {
        post_result([h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); }, ec, 0);
        return;
    }

    const native_socket_t fd = m_fd;

    start(detail::Reactor::Write,
        [fd](std::error_code& e, std::size_t&) {
            e = detail::pending_error(fd);
            if (e) return true;
            Endpoint peer;
            std::error_code pe;
            if (detail::remote_endpoint(fd, peer, pe)) return true;
            if (pe == error::not_connected) return false;
            e = pe;
            return true;
        },
        [h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); },
        true);
}

void TcpStream::connect(const Endpoint& endpoint) {
    std::error_code ec;
    if (!connect(endpoint, ec)) throw_error(ec, "TcpStream::connect");
}

bool TcpStream::connect(const Endpoint& endpoint, std::error_code& ec, int timeout_ms) {
    if (!is_open() && !open(endpoint.protocol(), ec)) return false;
    if (detail::start_connect(m_fd, endpoint, ec)) return true;
    if (!is_would_block(ec)) return false;
    const int r = detail::wait(m_fd, WaitType::Write, timeout_ms, ec);
    if (r < 0) return false;
    if (r == 0) { ec = error::timed_out; return false; }
    ec = detail::pending_error(m_fd);
    return !ec;
}

void TcpStream::async_read_some(MutableBuffer buffer, IoHandler handler) {
    if (!is_open()) { post_result(std::move(handler), error::bad_descriptor, 0); return; }
    if (buffer.size() == 0) { post_result(std::move(handler), {}, 0); return; }
    const native_socket_t fd = m_fd;

    start(detail::Reactor::Read,
        [fd, buffer](std::error_code& e, std::size_t& n) {
            const long r = detail::recv(fd, buffer.data(), buffer.size(), 0, e);

            if (r > 0) { n = static_cast<std::size_t>(r); return true; }
            if (r == 0) { e = error::eof; n = 0; return true; }
            if (e == error::would_block) { e.clear(); return false; }
            n = 0;
            return true;
        },
        std::move(handler), true);
}

void TcpStream::async_write_some(ConstBuffer buffer, IoHandler handler) {
    if (!is_open()) { post_result(std::move(handler), error::bad_descriptor, 0); return; }
    if (buffer.size() == 0) { post_result(std::move(handler), {}, 0); return; }
    const native_socket_t fd = m_fd;

    start(detail::Reactor::Write,
        [fd, buffer](std::error_code& e, std::size_t& n) {
            const long r = detail::send(fd, buffer.data(), buffer.size(), 0, e);

            if (r >= 0) { n = static_cast<std::size_t>(r); return true; }
            if (e == error::would_block) { e.clear(); return false; }
            n = 0;
            return true;
        },
        std::move(handler), true);
}

void TcpStream::async_wait(WaitType type, ErrorHandler handler) {
    if (!is_open()) {
        post_result([h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); }, error::bad_descriptor, 0);
        return;
    }

    const native_socket_t fd = m_fd;

    start(type == WaitType::Read ? detail::Reactor::Read : detail::Reactor::Write,
        [fd, type](std::error_code& e, std::size_t&) {
            const int r = detail::wait(fd, type, 0, e);
            if (r < 0) return true;
            e.clear();
            return r > 0;
        },
        [h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); },
        true);
}

void TcpStream::async_shutdown(ErrorHandler handler) {
    std::error_code ec;
    if (is_open()) detail::shutdown(m_fd, ShutdownType::Send, ec);
    else ec = error::bad_descriptor;
    post_result([h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); }, ec, 0);
}

std::size_t TcpStream::read_some(MutableBuffer buffer, std::error_code& ec, int timeout_ms) {
    ec.clear();
    if (!is_open()) { ec = error::bad_descriptor; return 0; }
    if (buffer.size() == 0) return 0;

    for (;;) {
        const long r = detail::recv(m_fd, buffer.data(), buffer.size(), 0, ec);
        if (r > 0) return static_cast<std::size_t>(r);
        if (r == 0) { ec = error::eof; return 0; }
        if (ec != error::would_block) return 0;
        const int w = detail::wait(m_fd, WaitType::Read, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

std::size_t TcpStream::write_some(ConstBuffer buffer, std::error_code& ec, int timeout_ms) {
    ec.clear();
    if (!is_open()) { ec = error::bad_descriptor; return 0; }
    if (buffer.size() == 0) return 0;

    for (;;) {
        const long r = detail::send(m_fd, buffer.data(), buffer.size(), 0, ec);
        if (r >= 0) return static_cast<std::size_t>(r);
        if (ec != error::would_block) return 0;
        const int w = detail::wait(m_fd, WaitType::Write, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

std::size_t TcpStream::read_some(MutableBuffer buffer) {
    std::error_code ec;
    const std::size_t n = read_some(buffer, ec);
    if (ec) throw_error(ec, "TcpStream::read_some");
    return n;
}

std::size_t TcpStream::write_some(ConstBuffer buffer) {
    std::error_code ec;
    const std::size_t n = write_some(buffer, ec);
    if (ec) throw_error(ec, "TcpStream::write_some");
    return n;
}

bool TcpStream::shutdown(ShutdownType type, std::error_code& ec) {
    if (!is_open()) { ec = error::bad_descriptor; return false; }
    return detail::shutdown(m_fd, type, ec);
}

void TcpStream::shutdown(ShutdownType type) {
    std::error_code ec;
    if (!shutdown(type, ec)) throw_error(ec, "TcpStream::shutdown");
}

Endpoint TcpStream::local_endpoint(std::error_code& ec) const {
    Endpoint e;
    if (!is_open()) { ec = error::bad_descriptor; return e; }
    detail::local_endpoint(m_fd, e, ec);
    return e;
}

Endpoint TcpStream::remote_endpoint(std::error_code& ec) const {
    Endpoint e;
    if (!is_open()) { ec = error::bad_descriptor; return e; }
    detail::remote_endpoint(m_fd, e, ec);
    return e;
}

Endpoint TcpStream::local_endpoint() const {
    std::error_code ec;
    return local_endpoint(ec);
}

Endpoint TcpStream::remote_endpoint() const {
    std::error_code ec;
    return remote_endpoint(ec);
}

void TcpStream::set_timeout(std::chrono::milliseconds timeout) {
    if (!m_deadline) m_deadline.reset(new SteadyTimer(*m_ctx));
    m_deadline->expires_after(timeout);
    std::weak_ptr<detail::Descriptor> weak = m_desc;
    IOContext* ctx = m_ctx;

    m_deadline->async_wait([weak, ctx](std::error_code ec) {
        if (ec) return;
        if (auto d = weak.lock()) ctx->reactor().cancel_ops(d, make_error_code(error::timed_out));
    });
}

void TcpStream::clear_timeout() {
    if (m_deadline) m_deadline->cancel();
}

bool TcpStream::set_no_delay(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, IPPROTO_TCP, TCP_NODELAY, enable ? 1 : 0, ec);
}

bool TcpStream::set_keep_alive(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_KEEPALIVE, enable ? 1 : 0, ec);
}

bool TcpStream::set_keep_alive_params(int idle_seconds, int interval_seconds, int probes, std::error_code& ec) {
    if (!set_keep_alive(true, ec)) return false;
#if defined(TCP_KEEPIDLE)
    if (!detail::set_int_option(m_fd, IPPROTO_TCP, TCP_KEEPIDLE, idle_seconds, ec)) return false;
#elif defined(TCP_KEEPALIVE)
    if (!detail::set_int_option(m_fd, IPPROTO_TCP, TCP_KEEPALIVE, idle_seconds, ec)) return false;
#endif
#if defined(TCP_KEEPINTVL)
    if (!detail::set_int_option(m_fd, IPPROTO_TCP, TCP_KEEPINTVL, interval_seconds, ec)) return false;
#endif
#if defined(TCP_KEEPCNT)
    if (!detail::set_int_option(m_fd, IPPROTO_TCP, TCP_KEEPCNT, probes, ec)) return false;
#endif
    (void)interval_seconds;
    (void)probes;
    return true;
}

bool TcpStream::set_linger(bool enable, int seconds, std::error_code& ec) {
    linger l;
    l.l_onoff = enable ? 1 : 0;
    l.l_linger = static_cast<decltype(l.l_linger)>(seconds);
    return detail::set_option(m_fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l), ec);
}

bool TcpStream::set_receive_buffer_size(int bytes, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_RCVBUF, bytes, ec);
}

bool TcpStream::set_send_buffer_size(int bytes, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_SNDBUF, bytes, ec);
}

int TcpStream::receive_buffer_size(std::error_code& ec) const {
    return detail::get_int_option(m_fd, SOL_SOCKET, SO_RCVBUF, ec);
}

int TcpStream::send_buffer_size(std::error_code& ec) const {
    return detail::get_int_option(m_fd, SOL_SOCKET, SO_SNDBUF, ec);
}

bool TcpStream::set_option(int level, int name, const void* value, std::size_t len, std::error_code& ec) {
    return detail::set_option(m_fd, level, name, value, len, ec);
}

bool TcpStream::get_option(int level, int name, void* value, std::size_t& len, std::error_code& ec) const {
    return detail::get_option(m_fd, level, name, value, len, ec);
}

std::size_t TcpStream::available(std::error_code& ec) const {
    return detail::available(m_fd, ec);
}

TcpAcceptor::TcpAcceptor(IOContext& ctx) noexcept : m_ctx(&ctx), m_fd(invalid_socket) {}

TcpAcceptor::TcpAcceptor(IOContext& ctx, const Endpoint& endpoint, bool reuse_address, int backlog) : TcpAcceptor(ctx) {
    std::error_code ec;
    if (!listen(endpoint, ec, reuse_address, backlog)) throw_error(ec, "TcpAcceptor");
}

TcpAcceptor::~TcpAcceptor() {
    close();
}

TcpAcceptor::TcpAcceptor(TcpAcceptor&& other) noexcept : m_ctx(other.m_ctx), m_fd(other.m_fd), m_desc(std::move(other.m_desc)) {
    other.m_fd = invalid_socket;
}

TcpAcceptor& TcpAcceptor::operator=(TcpAcceptor&& other) noexcept {
    if (this != &other) {
        close();
        m_ctx = other.m_ctx;
        m_fd = other.m_fd;
        m_desc = std::move(other.m_desc);
        other.m_fd = invalid_socket;
    }

    return *this;
}

bool TcpAcceptor::open(Protocol protocol, std::error_code& ec) {
    if (is_open()) { ec = error::already_open; return false; }
    native_socket_t fd = detail::open_socket(family_of(protocol), SOCK_STREAM, IPPROTO_TCP, ec);
    if (fd == invalid_socket) return false;
    auto d = m_ctx->reactor().register_descriptor(fd, ec);

    if (!d) {
        detail::close_socket(fd);
        return false;
    }

    m_fd = fd;
    m_desc = std::move(d);
    return true;
}

bool TcpAcceptor::bind(const Endpoint& endpoint, std::error_code& ec) {
    if (!is_open()) { ec = error::bad_descriptor; return false; }
    return detail::bind(m_fd, endpoint, ec);
}

bool TcpAcceptor::listen(int backlog, std::error_code& ec) {
    if (!is_open()) { ec = error::bad_descriptor; return false; }
    return detail::listen(m_fd, backlog, ec);
}

bool TcpAcceptor::listen(const Endpoint& endpoint, std::error_code& ec, bool reuse_address, int backlog) {
    if (!is_open() && !open(endpoint.protocol(), ec)) return false;
    if (reuse_address && !set_reuse_address(true, ec)) return false;
    if (!bind(endpoint, ec)) return false;
    return listen(backlog, ec);
}

bool TcpAcceptor::set_reuse_address(bool enable, std::error_code& ec) {
#if defined(OS_WINDOWS)
    (void)enable;
    ec.clear();
    return true;
#else
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_REUSEADDR, enable ? 1 : 0, ec);
#endif
}

bool TcpAcceptor::set_reuse_port(bool enable, std::error_code& ec) {
#if defined(SO_REUSEPORT)
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_REUSEPORT, enable ? 1 : 0, ec);
#else
    (void)enable;
    ec = error::unsupported;
    return false;
#endif
}

bool TcpAcceptor::set_v6_only(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, enable ? 1 : 0, ec);
}

void TcpAcceptor::close() {
    if (!is_open()) return;
    m_ctx->reactor().deregister_descriptor(m_desc);
    detail::close_socket(m_fd);
    m_fd = invalid_socket;
}

void TcpAcceptor::cancel() {
    if (m_desc) m_ctx->reactor().cancel_ops(m_desc);
}

Endpoint TcpAcceptor::local_endpoint(std::error_code& ec) const {
    Endpoint e;
    if (!is_open()) { ec = error::bad_descriptor; return e; }
    detail::local_endpoint(m_fd, e, ec);
    return e;
}

Endpoint TcpAcceptor::local_endpoint() const {
    std::error_code ec;
    return local_endpoint(ec);
}

void TcpAcceptor::async_accept(AcceptHandler handler) {
    IOContext* ctx = m_ctx;

    if (!is_open()) {
        ctx->post([h = std::move(handler), ctx]() mutable { h(make_error_code(error::bad_descriptor), TcpStream(*ctx)); });
        return;
    }

    const native_socket_t fd = m_fd;
    auto accepted = std::make_shared<native_socket_t>(invalid_socket);
    ctx->work_started();
    detail::ReactorOpPtr op(new detail::ReactorOp());

    op->perform = [fd, accepted](std::error_code& e, std::size_t&) {
        for (;;) {
            native_socket_t c = detail::accept(fd, nullptr, e);

            if (c != invalid_socket) {
                *accepted = c;
                return true;
            }

            if (e == error::would_block) { e.clear(); return false; }
            if (e == error::connection_aborted) continue;
            return true;
        }
    };

    op->handler = [h = std::move(handler), accepted, ctx](std::error_code e, std::size_t) mutable {
        TcpStream stream(*ctx);

        if (!e && *accepted != invalid_socket) {
            std::error_code ae;

            if (!stream.assign(*accepted, ae)) {
                detail::close_socket(*accepted);
                e = ae;
            }

            *accepted = invalid_socket;
        } else if (*accepted != invalid_socket) {
            detail::close_socket(*accepted);
            *accepted = invalid_socket;
        }

        h(e, std::move(stream));
    };

    ctx->reactor().start_op(m_desc, detail::Reactor::Read, std::move(op), true);
}

TcpStream TcpAcceptor::accept(std::error_code& ec, int timeout_ms) {
    TcpStream stream(*m_ctx);
    if (!is_open()) { ec = error::bad_descriptor; return stream; }

    for (;;) {
        native_socket_t c = detail::accept(m_fd, nullptr, ec);

        if (c != invalid_socket) {
            if (!stream.assign(c, ec)) detail::close_socket(c);
            return stream;
        }

        if (ec == error::connection_aborted) continue;
        if (ec != error::would_block) return stream;
        const int w = detail::wait(m_fd, WaitType::Read, timeout_ms, ec);
        if (w < 0) return stream;
        if (w == 0) { ec = error::timed_out; return stream; }
    }
}

TcpStream TcpAcceptor::accept() {
    std::error_code ec;
    TcpStream s = accept(ec);
    if (ec) throw_error(ec, "TcpAcceptor::accept");
    return s;
}

} // namespace async
} // namespace networking
} // namespace fizmo
