#include "fizmo_library.hpp"
#include "udp.hpp"

#include <cstring>

namespace fizmo {
namespace networking {
namespace async {

namespace {

[[noreturn]] void throw_error(const std::error_code& ec, const char* what) {
    throw std::system_error(ec, what);
}

} // namespace

UdpSocket::UdpSocket(IOContext& ctx) noexcept : m_ctx(&ctx), m_fd(invalid_socket), m_family(AF_UNSPEC) {}

UdpSocket::UdpSocket(IOContext& ctx, Protocol protocol) : UdpSocket(ctx) {
    open(protocol);
}

UdpSocket::UdpSocket(IOContext& ctx, const Endpoint& bind_endpoint, bool reuse_address) : UdpSocket(ctx) {
    open(bind_endpoint.protocol());
    std::error_code ec;
    if (reuse_address && !set_reuse_address(true, ec)) throw_error(ec, "UdpSocket::set_reuse_address");
    bind(bind_endpoint);
}

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : m_ctx(other.m_ctx), m_fd(other.m_fd), m_desc(std::move(other.m_desc)), m_family(other.m_family) {
    other.m_fd = invalid_socket;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        m_ctx = other.m_ctx;
        m_fd = other.m_fd;
        m_desc = std::move(other.m_desc);
        m_family = other.m_family;
        other.m_fd = invalid_socket;
    }

    return *this;
}

void UdpSocket::open(Protocol protocol) {
    std::error_code ec;
    if (!open(protocol, ec)) throw_error(ec, "UdpSocket::open");
}

bool UdpSocket::open(Protocol protocol, std::error_code& ec) {
    if (is_open()) { ec = error::already_open; return false; }
    const int family = protocol == Protocol::V6 ? AF_INET6 : AF_INET;
    native_socket_t fd = detail::open_socket(family, SOCK_DGRAM, IPPROTO_UDP, ec);
    if (fd == invalid_socket) return false;
    auto d = m_ctx->reactor().register_descriptor(fd, ec);

    if (!d) {
        detail::close_socket(fd);
        return false;
    }

    m_fd = fd;
    m_desc = std::move(d);
    m_family = family;
    return true;
}

bool UdpSocket::bind(const Endpoint& endpoint, std::error_code& ec) {
    if (!is_open() && !open(endpoint.protocol(), ec)) return false;
    return detail::bind(m_fd, endpoint, ec);
}

void UdpSocket::bind(const Endpoint& endpoint) {
    std::error_code ec;
    if (!bind(endpoint, ec)) throw_error(ec, "UdpSocket::bind");
}

bool UdpSocket::connect(const Endpoint& endpoint, std::error_code& ec) {
    if (!is_open() && !open(endpoint.protocol(), ec)) return false;
    return detail::start_connect(m_fd, endpoint, ec);
}

void UdpSocket::close() {
    if (!is_open()) return;
    m_ctx->reactor().deregister_descriptor(m_desc);
    detail::close_socket(m_fd);
    m_fd = invalid_socket;
}

void UdpSocket::cancel() {
    if (m_desc) m_ctx->reactor().cancel_ops(m_desc);
}

void UdpSocket::start(detail::Reactor::OpType type, UniqueFunction<bool(std::error_code&, std::size_t&)> perform, IoHandler handler) {
    m_ctx->work_started();
    detail::ReactorOpPtr op(new detail::ReactorOp());
    op->perform = std::move(perform);
    op->handler = std::move(handler);
    m_ctx->reactor().start_op(m_desc, type, std::move(op), true);
}

void UdpSocket::async_send_to(ConstBuffer buffer, const Endpoint& to, IoHandler handler) {
    if (!is_open()) {
        std::error_code ec;
        if (!open(to.protocol(), ec)) { m_ctx->post([h = std::move(handler), ec]() mutable { h(ec, 0); }); return; }
    }

    const native_socket_t fd = m_fd;

    start(detail::Reactor::Write,
        [fd, buffer, to](std::error_code& e, std::size_t& n) {
            const long r = detail::send_to(fd, buffer.data(), buffer.size(), 0, to, e);
            if (r >= 0) { n = static_cast<std::size_t>(r); return true; }
            if (e == error::would_block) { e.clear(); return false; }
            return true;
        },
        std::move(handler));
}

void UdpSocket::async_receive_from(MutableBuffer buffer, ReceiveFromHandler handler) {
    if (!is_open()) {
        m_ctx->post([h = std::move(handler)]() mutable { h(make_error_code(error::bad_descriptor), 0, Endpoint()); });
        return;
    }

    const native_socket_t fd = m_fd;
    auto from = std::make_shared<Endpoint>();

    start(detail::Reactor::Read,
        [fd, buffer, from](std::error_code& e, std::size_t& n) {
            const long r = detail::recv_from(fd, buffer.data(), buffer.size(), 0, *from, e);
            if (r >= 0) { n = static_cast<std::size_t>(r); return true; }
            if (e == error::would_block) { e.clear(); return false; }
            if (e == error::connection_reset) { e.clear(); return false; }
            return true;
        },
        [h = std::move(handler), from](std::error_code e, std::size_t n) mutable { h(e, n, *from); });
}

void UdpSocket::async_send(ConstBuffer buffer, IoHandler handler) {
    if (!is_open()) { m_ctx->post([h = std::move(handler)]() mutable { h(make_error_code(error::bad_descriptor), 0); }); return; }
    const native_socket_t fd = m_fd;

    start(detail::Reactor::Write,
        [fd, buffer](std::error_code& e, std::size_t& n) {
            const long r = detail::send(fd, buffer.data(), buffer.size(), 0, e);
            if (r >= 0) { n = static_cast<std::size_t>(r); return true; }
            if (e == error::would_block) { e.clear(); return false; }
            return true;
        },
        std::move(handler));
}

void UdpSocket::async_receive(MutableBuffer buffer, IoHandler handler) {
    if (!is_open()) { m_ctx->post([h = std::move(handler)]() mutable { h(make_error_code(error::bad_descriptor), 0); }); return; }
    const native_socket_t fd = m_fd;

    start(detail::Reactor::Read,
        [fd, buffer](std::error_code& e, std::size_t& n) {
            const long r = detail::recv(fd, buffer.data(), buffer.size(), 0, e);
            if (r >= 0) { n = static_cast<std::size_t>(r); return true; }
            if (e == error::would_block) { e.clear(); return false; }
            return true;
        },
        std::move(handler));
}

void UdpSocket::async_wait(WaitType type, ErrorHandler handler) {
    if (!is_open()) { m_ctx->post([h = std::move(handler)]() mutable { h(make_error_code(error::bad_descriptor)); }); return; }
    m_ctx->work_started();
    detail::ReactorOpPtr op(new detail::ReactorOp());
    const native_socket_t fd = m_fd;
    op->perform = [fd, type](std::error_code& e, std::size_t&) {
        const int r = detail::wait(fd, type, 0, e);
        if (r < 0) return true;
        e.clear();
        return r > 0;
    };
    op->handler = [h = std::move(handler)](std::error_code e, std::size_t) mutable { h(e); };
    m_ctx->reactor().start_op(m_desc, type == WaitType::Read ? detail::Reactor::Read : detail::Reactor::Write, std::move(op), true);
}

std::size_t UdpSocket::send_to(ConstBuffer buffer, const Endpoint& to, std::error_code& ec, int timeout_ms) {
    if (!is_open() && !open(to.protocol(), ec)) return 0;

    for (;;) {
        const long r = detail::send_to(m_fd, buffer.data(), buffer.size(), 0, to, ec);
        if (r >= 0) return static_cast<std::size_t>(r);
        if (ec != error::would_block) return 0;
        const int w = detail::wait(m_fd, WaitType::Write, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

std::size_t UdpSocket::receive_from(MutableBuffer buffer, Endpoint& from, std::error_code& ec, int timeout_ms) {
    if (!is_open()) { ec = error::bad_descriptor; return 0; }

    for (;;) {
        const long r = detail::recv_from(m_fd, buffer.data(), buffer.size(), 0, from, ec);
        if (r >= 0) return static_cast<std::size_t>(r);
        if (ec != error::would_block && ec != error::connection_reset) return 0;
        const int w = detail::wait(m_fd, WaitType::Read, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

std::size_t UdpSocket::send(ConstBuffer buffer, std::error_code& ec, int timeout_ms) {
    if (!is_open()) { ec = error::bad_descriptor; return 0; }

    for (;;) {
        const long r = detail::send(m_fd, buffer.data(), buffer.size(), 0, ec);
        if (r >= 0) return static_cast<std::size_t>(r);
        if (ec != error::would_block) return 0;
        const int w = detail::wait(m_fd, WaitType::Write, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

std::size_t UdpSocket::receive(MutableBuffer buffer, std::error_code& ec, int timeout_ms) {
    if (!is_open()) { ec = error::bad_descriptor; return 0; }

    for (;;) {
        const long r = detail::recv(m_fd, buffer.data(), buffer.size(), 0, ec);
        if (r >= 0) return static_cast<std::size_t>(r);
        if (ec != error::would_block) return 0;
        const int w = detail::wait(m_fd, WaitType::Read, timeout_ms, ec);
        if (w < 0) return 0;
        if (w == 0) { ec = error::timed_out; return 0; }
    }
}

Endpoint UdpSocket::local_endpoint(std::error_code& ec) const {
    Endpoint e;
    if (!is_open()) { ec = error::bad_descriptor; return e; }
    detail::local_endpoint(m_fd, e, ec);
    return e;
}

Endpoint UdpSocket::local_endpoint() const {
    std::error_code ec;
    return local_endpoint(ec);
}

Endpoint UdpSocket::remote_endpoint(std::error_code& ec) const {
    Endpoint e;
    if (!is_open()) { ec = error::bad_descriptor; return e; }
    detail::remote_endpoint(m_fd, e, ec);
    return e;
}

bool UdpSocket::set_reuse_address(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_REUSEADDR, enable ? 1 : 0, ec);
}

bool UdpSocket::set_reuse_port(bool enable, std::error_code& ec) {
#if defined(SO_REUSEPORT)
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_REUSEPORT, enable ? 1 : 0, ec);
#else
    (void)enable;
    ec = error::unsupported;
    return false;
#endif
}

bool UdpSocket::set_broadcast(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_BROADCAST, enable ? 1 : 0, ec);
}

bool UdpSocket::set_v6_only(bool enable, std::error_code& ec) {
    return detail::set_int_option(m_fd, IPPROTO_IPV6, IPV6_V6ONLY, enable ? 1 : 0, ec);
}

bool UdpSocket::set_receive_buffer_size(int bytes, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_RCVBUF, bytes, ec);
}

bool UdpSocket::set_send_buffer_size(int bytes, std::error_code& ec) {
    return detail::set_int_option(m_fd, SOL_SOCKET, SO_SNDBUF, bytes, ec);
}

bool UdpSocket::set_ttl(int ttl, std::error_code& ec) {
    if (m_family == AF_INET6) return detail::set_int_option(m_fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, ttl, ec);
    return detail::set_int_option(m_fd, IPPROTO_IP, IP_TTL, ttl, ec);
}

bool UdpSocket::set_dont_fragment(bool enable, std::error_code& ec) {
#if defined(OS_WINDOWS)
    return detail::set_int_option(m_fd, IPPROTO_IP, IP_DONTFRAGMENT, enable ? 1 : 0, ec);
#elif defined(IP_MTU_DISCOVER)
    return detail::set_int_option(m_fd, IPPROTO_IP, IP_MTU_DISCOVER, enable ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT, ec);
#else
    (void)enable;
    ec = error::unsupported;
    return false;
#endif
}

bool UdpSocket::membership(const std::string& group, const std::string& iface, bool join, std::error_code& ec) {
    Endpoint g;
    if (!Endpoint::parse(group, 0, g)) { ec = error::invalid_argument; return false; }

    if (g.is_v4()) {
        ip_mreq mreq;
        std::memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr = reinterpret_cast<const sockaddr_in*>(g.data())->sin_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (!iface.empty()) {
            Endpoint i;
            if (!Endpoint::parse(iface, 0, i) || !i.is_v4()) { ec = error::invalid_argument; return false; }
            mreq.imr_interface = reinterpret_cast<const sockaddr_in*>(i.data())->sin_addr;
        }

        return detail::set_option(m_fd, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq), ec);
    }

    ipv6_mreq mreq;
    std::memset(&mreq, 0, sizeof(mreq));
    mreq.ipv6mr_multiaddr = reinterpret_cast<const sockaddr_in6*>(g.data())->sin6_addr;
    mreq.ipv6mr_interface = iface.empty() ? 0 : detail::interface_index(iface);
#if defined(IPV6_JOIN_GROUP)
    const int opt = join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP;
#else
    const int opt = join ? IPV6_ADD_MEMBERSHIP : IPV6_DROP_MEMBERSHIP;
#endif
    return detail::set_option(m_fd, IPPROTO_IPV6, opt, &mreq, sizeof(mreq), ec);
}

bool UdpSocket::join_group(const std::string& group, std::error_code& ec, const std::string& interface_address) {
    return membership(group, interface_address, true, ec);
}

bool UdpSocket::leave_group(const std::string& group, std::error_code& ec, const std::string& interface_address) {
    return membership(group, interface_address, false, ec);
}

bool UdpSocket::set_multicast_ttl(int ttl, std::error_code& ec) {
    if (m_family == AF_INET6) return detail::set_int_option(m_fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, ttl, ec);
    return detail::set_int_option(m_fd, IPPROTO_IP, IP_MULTICAST_TTL, ttl, ec);
}

bool UdpSocket::set_multicast_loopback(bool enable, std::error_code& ec) {
    if (m_family == AF_INET6) return detail::set_int_option(m_fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, enable ? 1 : 0, ec);
    return detail::set_int_option(m_fd, IPPROTO_IP, IP_MULTICAST_LOOP, enable ? 1 : 0, ec);
}

bool UdpSocket::set_multicast_interface(const std::string& interface_address, std::error_code& ec) {
    if (m_family == AF_INET6) {
        const unsigned idx = detail::interface_index(interface_address);
        return detail::set_option(m_fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx), ec);
    }

    Endpoint i;
    if (!Endpoint::parse(interface_address, 0, i) || !i.is_v4()) { ec = error::invalid_argument; return false; }
    const in_addr addr = reinterpret_cast<const sockaddr_in*>(i.data())->sin_addr;
    return detail::set_option(m_fd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr), ec);
}

bool UdpSocket::set_option(int level, int name, const void* value, std::size_t len, std::error_code& ec) {
    return detail::set_option(m_fd, level, name, value, len, ec);
}

bool UdpSocket::get_option(int level, int name, void* value, std::size_t& len, std::error_code& ec) const {
    return detail::get_option(m_fd, level, name, value, len, ec);
}

std::size_t UdpSocket::available(std::error_code& ec) const {
    return detail::available(m_fd, ec);
}

} // namespace async
} // namespace networking
} // namespace fizmo
