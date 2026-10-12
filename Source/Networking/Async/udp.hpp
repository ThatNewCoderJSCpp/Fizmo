#ifndef FIZMO_ASYNC_UDP_HPP
#define FIZMO_ASYNC_UDP_HPP

#include "stream.hpp"
#include "socket_ops.hpp"
#include "reactor.hpp"

#include <memory>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace async {

class UdpSocket {
public:
    using ReceiveFromHandler = UniqueFunction<void(std::error_code, std::size_t, Endpoint)>;

    explicit UdpSocket(IOContext& ctx) noexcept;
    UdpSocket(IOContext& ctx, Protocol protocol);
    UdpSocket(IOContext& ctx, const Endpoint& bind_endpoint, bool reuse_address = false);
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

public:
    IOContext& context() const noexcept { return *m_ctx; }

    void open(Protocol protocol);
    bool open(Protocol protocol, std::error_code& ec);
    bool bind(const Endpoint& endpoint, std::error_code& ec);
    void bind(const Endpoint& endpoint);
    bool connect(const Endpoint& endpoint, std::error_code& ec);
    bool is_open() const noexcept { return m_fd != invalid_socket; }
    void close();
    void cancel();
    native_socket_t native_handle() const noexcept { return m_fd; }

    void async_send_to(ConstBuffer buffer, const Endpoint& to, IoHandler handler);
    void async_receive_from(MutableBuffer buffer, ReceiveFromHandler handler);
    void async_send(ConstBuffer buffer, IoHandler handler);
    void async_receive(MutableBuffer buffer, IoHandler handler);
    void async_wait(WaitType type, ErrorHandler handler);

    std::size_t send_to(ConstBuffer buffer, const Endpoint& to, std::error_code& ec, int timeout_ms = -1);
    std::size_t receive_from(MutableBuffer buffer, Endpoint& from, std::error_code& ec, int timeout_ms = -1);
    std::size_t send(ConstBuffer buffer, std::error_code& ec, int timeout_ms = -1);
    std::size_t receive(MutableBuffer buffer, std::error_code& ec, int timeout_ms = -1);

    Endpoint local_endpoint() const;
    Endpoint local_endpoint(std::error_code& ec) const;
    Endpoint remote_endpoint(std::error_code& ec) const;

public:
    bool set_reuse_address(bool enable, std::error_code& ec);
    bool set_reuse_port(bool enable, std::error_code& ec);
    bool set_broadcast(bool enable, std::error_code& ec);
    bool set_v6_only(bool enable, std::error_code& ec);
    bool set_receive_buffer_size(int bytes, std::error_code& ec);
    bool set_send_buffer_size(int bytes, std::error_code& ec);
    bool set_ttl(int ttl, std::error_code& ec);
    bool set_dont_fragment(bool enable, std::error_code& ec);

    bool join_group(const std::string& group, std::error_code& ec, const std::string& interface_address = {});
    bool leave_group(const std::string& group, std::error_code& ec, const std::string& interface_address = {});
    bool set_multicast_ttl(int ttl, std::error_code& ec);
    bool set_multicast_loopback(bool enable, std::error_code& ec);
    bool set_multicast_interface(const std::string& interface_address, std::error_code& ec);

    bool set_option(int level, int name, const void* value, std::size_t len, std::error_code& ec);
    bool get_option(int level, int name, void* value, std::size_t& len, std::error_code& ec) const;
    std::size_t available(std::error_code& ec) const;

private:
    IOContext*            m_ctx;
    native_socket_t       m_fd;
    detail::DescriptorPtr m_desc;
    int                   m_family;

    void start(detail::Reactor::OpType type, UniqueFunction<bool(std::error_code&, std::size_t&)> perform, IoHandler handler);
    bool membership(const std::string& group, const std::string& iface, bool join, std::error_code& ec);
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_UDP_HPP
