#include "fizmo_library.hpp"
#include "dgram.hpp"
#include "dns.hpp"
#include "../Async/udp.hpp"

#include <atomic>
#include <cstring>
#include <mutex>

namespace fizmo {
namespace networking {
namespace dgram {

namespace {

std::error_code err(async::error e) { return async::make_error_code(e); }

net::AddressInfo info_of(const async::Endpoint& ep) {
    net::AddressInfo a;
    if (!ep.valid()) return a;
    const async::Endpoint e = ep.to_v4_unmapped();
    a.address = e.address();
    a.family = e.is_v6() ? "IPv6" : "IPv4";
    a.port = e.port();
    return a;
}

} // namespace

struct Socket::State : std::enable_shared_from_this<Socket::State> {
    mutable std::mutex     m;
    Events                 ev;
    SocketOptions          opt;
    async::UdpSocket       sock;
    Bytes                  rbuf;
    std::atomic<bool>      bound{false};
    bool                   binding = false;
    std::atomic<bool>      connected{false};
    std::atomic<bool>      closed{false};
    async::Endpoint        remote;
    async::Endpoint        local;
    std::shared_ptr<State> keep;
    std::any               user;

    explicit State(const SocketOptions& o) : opt(o), sock(loop()), rbuf(o.max_datagram_size == 0 ? 65536 : o.max_datagram_size) {}

    async::Protocol protocol() const { return opt.type == Type::UDP6 ? async::Protocol::V6 : async::Protocol::V4; }
    net::Family family() const { return opt.type == Type::UDP6 ? net::Family::Any : net::Family::IPv4; }

    void emit_error(const std::error_code& ec) {
        if (ev.error.emit(ec) == 0) report_unhandled_error("dgram::Socket", ec);
    }

    bool ensure_open(std::error_code& ec) {
        if (sock.is_open()) return true;
        if (!sock.open(protocol(), ec)) return false;
        std::error_code ignore;
        if (opt.reuse_address) sock.set_reuse_address(true, ignore);
        if (opt.reuse_port) sock.set_reuse_port(true, ignore);
        if (opt.type == Type::UDP6) sock.set_v6_only(opt.ipv6_only, ignore);
        if (opt.receive_buffer_size > 0) sock.set_receive_buffer_size(opt.receive_buffer_size, ignore);
        if (opt.send_buffer_size > 0) sock.set_send_buffer_size(opt.send_buffer_size, ignore);
        return true;
    }

    bool do_bind(const async::Endpoint& ep, std::error_code& ec) {
        if (closed) { ec = err(async::error::closed); return false; }
        if (bound) { ec = err(async::error::already_open); return false; }
        if (!ensure_open(ec)) return false;
        if (!sock.bind(ep, ec)) return false;
        bound = true;
        { std::lock_guard<std::mutex> lk(m); local = sock.local_endpoint(); }
        keep = shared_from_this();
        receive_next();
        ev.listening.emit();
        return true;
    }

    bool implicit_bind(std::error_code& ec) {
        if (bound) return true;
        return do_bind(opt.type == Type::UDP6 ? async::Endpoint::any_v6(0) : async::Endpoint::any_v4(0), ec);
    }

    void receive_next() {
        if (closed) return;
        auto self = shared_from_this();

        sock.async_receive_from(async::MutableBuffer(rbuf.data(), rbuf.size()), [self](std::error_code ec, std::size_t n, async::Endpoint from) {
            if (self->closed) return;

            if (ec) {
                if (ec == async::error::operation_aborted) return;
                self->emit_error(ec);
                self->receive_next();
                return;
            }

            RemoteInfo ri;
            const async::Endpoint e = from.to_v4_unmapped();
            ri.address = e.address();
            ri.family = e.is_v6() ? "IPv6" : "IPv4";
            ri.port = e.port();
            ri.size = n;
            ri.endpoint = from;
            self->ev.message.emit(DataView(self->rbuf.data(), n), ri);
            self->receive_next();
        });
    }

    async::Endpoint map_target(const async::Endpoint& ep) const {
        if (opt.type == Type::UDP6 && ep.is_v4()) {
            async::Endpoint mapped;
            if (async::Endpoint::parse("::ffff:" + ep.address(), ep.port(), mapped)) return mapped;
        }

        return ep;
    }

    void send_to(std::shared_ptr<Bytes> data, const async::Endpoint& to, std::function<void(const std::error_code&, std::size_t)> cb) {
        std::error_code ec;

        if (closed) ec = err(async::error::closed);
        else implicit_bind(ec);

        if (ec) {
            if (cb) cb(ec, 0);
            else emit_error(ec);
            return;
        }

        auto self = shared_from_this();

        sock.async_send_to(async::ConstBuffer(data->data(), data->size()), map_target(to), [self, data, cb](std::error_code sec, std::size_t n) {
            if (cb) cb(sec, n);
            else if (sec && sec != async::error::operation_aborted) self->emit_error(sec);
        });
    }

    void send_connected(std::shared_ptr<Bytes> data, std::function<void(const std::error_code&, std::size_t)> cb) {
        if (closed || !connected) {
            const auto ec = err(closed ? async::error::closed : async::error::not_connected);
            if (cb) cb(ec, 0);
            else emit_error(ec);
            return;
        }

        auto self = shared_from_this();

        sock.async_send(async::ConstBuffer(data->data(), data->size()), [self, data, cb](std::error_code sec, std::size_t n) {
            if (cb) cb(sec, n);
            else if (sec && sec != async::error::operation_aborted) self->emit_error(sec);
        });
    }

    void resolve(const std::string& address, std::uint16_t port, std::function<void(const std::error_code&, const async::Endpoint&)> cb) {
        const std::string host = address.empty() ? (opt.type == Type::UDP6 ? std::string("::1") : std::string("127.0.0.1")) : address;
        async::Endpoint ep;

        if (async::Endpoint::parse(host, port, ep)) {
            cb({}, ep);
            return;
        }

        dns::LookupOptions lo;
        lo.family = family();
        lo.verbatim = opt.type == Type::UDP6;

        dns::lookup(host, lo, [cb, port](const std::error_code& ec, const std::string& addr, net::Family) {
            async::Endpoint out;
            if (!ec && !async::Endpoint::parse(addr, port, out)) { cb(err(async::error::host_not_found), out); return; }
            cb(ec, out);
        });
    }

    void finish_close() {
        ev.close.emit();
        ev.message.clear();
        ev.listening.clear();
        ev.connect.clear();
        ev.close.clear();
        ev.error.clear();
        keep.reset();
    }
};

Socket& Socket::on_message(std::function<void(const DataView&, const RemoteInfo&)> fn) { m_state->ev.message.add(std::move(fn)); return *this; }
Socket& Socket::on_listening(std::function<void()> fn) { m_state->ev.listening.add(std::move(fn)); return *this; }
Socket& Socket::on_connect(std::function<void()> fn) { m_state->ev.connect.add(std::move(fn)); return *this; }
Socket& Socket::on_close(std::function<void()> fn) { m_state->ev.close.add(std::move(fn)); return *this; }
Socket& Socket::on_error(std::function<void(const std::error_code&)> fn) { m_state->ev.error.add(std::move(fn)); return *this; }
Socket::Events& Socket::events() { return m_state->ev; }

Socket& Socket::bind(std::uint16_t port, const std::string& address, std::function<void()> on_listening) {
    auto st = m_state;
    if (on_listening) st->ev.listening.once(std::move(on_listening));

    loop().dispatch([st, port, address]() {
        if (st->bound || st->binding) { st->emit_error(err(async::error::already_open)); return; }
        st->binding = true;
        std::string host = address;
        if (host.empty()) host = st->opt.type == Type::UDP6 ? "::" : "0.0.0.0";

        st->resolve(host, port, [st](const std::error_code& ec, const async::Endpoint& ep) {
            st->binding = false;
            if (ec) { st->emit_error(ec); return; }
            std::error_code bec;
            if (!st->do_bind(ep, bec)) st->emit_error(bec);
        });
    });

    return *this;
}

Socket& Socket::connect(std::uint16_t port, const std::string& address, std::function<void(const std::error_code&)> callback) {
    auto st = m_state;

    loop().dispatch([st, port, address, callback]() {
        st->resolve(address, port, [st, callback](const std::error_code& ec, const async::Endpoint& ep) {
            std::error_code cec = ec;

            if (!cec) {
                st->implicit_bind(cec);
                if (!cec) st->sock.connect(st->map_target(ep), cec);
            }

            if (!cec) {
                st->connected = true;
                { std::lock_guard<std::mutex> lk(st->m); st->remote = ep; }
                st->ev.connect.emit();
            }

            if (callback) callback(cec);
            else if (cec) st->emit_error(cec);
        });
    });

    return *this;
}

Socket& Socket::disconnect() {
    auto st = m_state;

    loop().dispatch([st]() {
        if (!st->connected || !st->sock.is_open()) return;
        sockaddr_storage ss;
        std::memset(&ss, 0, sizeof(ss));
#if defined(OS_WINDOWS)
        ss.ss_family = static_cast<decltype(ss.ss_family)>(st->opt.type == Type::UDP6 ? AF_INET6 : AF_INET);
        const int len = st->opt.type == Type::UDP6 ? static_cast<int>(sizeof(sockaddr_in6)) : static_cast<int>(sizeof(sockaddr_in));
#else
        ss.ss_family = AF_UNSPEC;
        const socklen_t len = static_cast<socklen_t>(sizeof(sockaddr));
#endif
        ::connect(st->sock.native_handle(), reinterpret_cast<sockaddr*>(&ss), len);
        st->connected = false;
        std::lock_guard<std::mutex> lk(st->m);
        st->remote = async::Endpoint();
    });

    return *this;
}

Socket& Socket::close(std::function<void()> callback) {
    auto st = m_state;
    if (callback) st->ev.close.once(std::move(callback));

    loop().dispatch([st]() {
        if (st->closed) return;
        st->closed = true;
        st->sock.close();
        loop().post([st]() { st->finish_close(); });
    });

    return *this;
}

Socket& Socket::send(const DataView& message, std::uint16_t port, const std::string& address, std::function<void(const std::error_code&, std::size_t)> callback) {
    auto st = m_state;
    auto data = std::make_shared<Bytes>(message.bytes());

    loop().dispatch([st, data, port, address, callback]() {
        st->resolve(address, port, [st, data, callback](const std::error_code& ec, const async::Endpoint& ep) {
            if (ec) {
                if (callback) callback(ec, 0);
                else st->emit_error(ec);
                return;
            }

            st->send_to(data, ep, callback);
        });
    });

    return *this;
}

Socket& Socket::send(const DataView& message, const async::Endpoint& to, std::function<void(const std::error_code&, std::size_t)> callback) {
    auto st = m_state;
    auto data = std::make_shared<Bytes>(message.bytes());
    loop().dispatch([st, data, to, callback]() { st->send_to(data, to, callback); });
    return *this;
}

Socket& Socket::send(const DataView& message, std::function<void(const std::error_code&, std::size_t)> callback) {
    auto st = m_state;
    auto data = std::make_shared<Bytes>(message.bytes());
    loop().dispatch([st, data, callback]() { st->send_connected(data, callback); });
    return *this;
}

net::AddressInfo Socket::address() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return info_of(m_state->local);
}

net::AddressInfo Socket::remote_address() const {
    std::lock_guard<std::mutex> lk(m_state->m);
    return info_of(m_state->remote);
}

bool Socket::bound() const noexcept { return m_state->bound; }
bool Socket::connected() const noexcept { return m_state->connected; }
bool Socket::closed() const noexcept { return m_state->closed; }

namespace {

template <typename F>
void apply_option(const std::shared_ptr<Socket::State>& st, F f) {
    loop().dispatch([st, f]() {
        std::error_code ec;
        if (!st->ensure_open(ec) || !f(st->sock, ec)) st->emit_error(ec ? ec : err(async::error::invalid_argument));
    });
}

} // namespace

Socket& Socket::set_broadcast(bool enable) {
    apply_option(m_state, [enable](async::UdpSocket& s, std::error_code& ec) { return s.set_broadcast(enable, ec); });
    return *this;
}

Socket& Socket::set_ttl(int ttl) {
    apply_option(m_state, [ttl](async::UdpSocket& s, std::error_code& ec) { return s.set_ttl(ttl, ec); });
    return *this;
}

Socket& Socket::set_multicast_ttl(int ttl) {
    apply_option(m_state, [ttl](async::UdpSocket& s, std::error_code& ec) { return s.set_multicast_ttl(ttl, ec); });
    return *this;
}

Socket& Socket::set_multicast_loopback(bool enable) {
    apply_option(m_state, [enable](async::UdpSocket& s, std::error_code& ec) { return s.set_multicast_loopback(enable, ec); });
    return *this;
}

Socket& Socket::set_multicast_interface(const std::string& interface_address) {
    apply_option(m_state, [interface_address](async::UdpSocket& s, std::error_code& ec) { return s.set_multicast_interface(interface_address, ec); });
    return *this;
}

Socket& Socket::add_membership(const std::string& group, const std::string& interface_address) {
    apply_option(m_state, [group, interface_address](async::UdpSocket& s, std::error_code& ec) { return s.join_group(group, ec, interface_address); });
    return *this;
}

Socket& Socket::drop_membership(const std::string& group, const std::string& interface_address) {
    apply_option(m_state, [group, interface_address](async::UdpSocket& s, std::error_code& ec) { return s.leave_group(group, ec, interface_address); });
    return *this;
}

Socket& Socket::set_receive_buffer_size(int bytes) {
    apply_option(m_state, [bytes](async::UdpSocket& s, std::error_code& ec) { return s.set_receive_buffer_size(bytes, ec); });
    return *this;
}

Socket& Socket::set_send_buffer_size(int bytes) {
    apply_option(m_state, [bytes](async::UdpSocket& s, std::error_code& ec) { return s.set_send_buffer_size(bytes, ec); });
    return *this;
}

std::any& Socket::data() { return m_state->user; }

Socket create_socket(Type type, std::function<void(const DataView&, const RemoteInfo&)> on_message) {
    SocketOptions o;
    o.type = type;
    return create_socket(o, std::move(on_message));
}

Socket create_socket(const SocketOptions& options, std::function<void(const DataView&, const RemoteInfo&)> on_message) {
    auto st = std::make_shared<Socket::State>(options);
    Socket s(st);
    if (on_message) s.on_message(std::move(on_message));
    return s;
}

} // namespace dgram
} // namespace networking
} // namespace fizmo
