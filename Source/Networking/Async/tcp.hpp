#ifndef FIZMO_ASYNC_TCP_HPP
#define FIZMO_ASYNC_TCP_HPP

#include "stream.hpp"
#include "socket_ops.hpp"
#include "timer.hpp"
#include "reactor.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <system_error>

namespace fizmo {
namespace networking {
namespace async {

class TcpStream final : public AsyncStream {
public:
    explicit TcpStream(IOContext& ctx) noexcept;
    TcpStream(IOContext& ctx, Protocol protocol);
    ~TcpStream() override;

    TcpStream(const TcpStream&) = delete;
    TcpStream& operator=(const TcpStream&) = delete;
    TcpStream(TcpStream&& other) noexcept;
    TcpStream& operator=(TcpStream&& other) noexcept;

public:
    IOContext& context() const noexcept override { return *m_ctx; }

    void open(Protocol protocol);
    bool open(Protocol protocol, std::error_code& ec);
    bool assign(native_socket_t fd, std::error_code& ec);
    bool is_open() const noexcept override { return m_fd != invalid_socket; }
    void close() override;
    void close(std::error_code& ec);
    native_socket_t release();
    native_socket_t native_handle() const noexcept { return m_fd; }

    void cancel() override;

    void async_connect(const Endpoint& endpoint, ErrorHandler handler);
    void connect(const Endpoint& endpoint);
    bool connect(const Endpoint& endpoint, std::error_code& ec, int timeout_ms = -1);

    void async_read_some(MutableBuffer buffer, IoHandler handler) override;
    void async_write_some(ConstBuffer buffer, IoHandler handler) override;
    void async_wait(WaitType type, ErrorHandler handler);
    void async_shutdown(ErrorHandler handler) override;

    std::size_t read_some(MutableBuffer buffer, std::error_code& ec, int timeout_ms = -1);
    std::size_t write_some(ConstBuffer buffer, std::error_code& ec, int timeout_ms = -1);
    std::size_t read_some(MutableBuffer buffer);
    std::size_t write_some(ConstBuffer buffer);

    bool shutdown(ShutdownType type, std::error_code& ec);
    void shutdown(ShutdownType type);

    Endpoint local_endpoint() const override;
    Endpoint remote_endpoint() const override;
    Endpoint local_endpoint(std::error_code& ec) const;
    Endpoint remote_endpoint(std::error_code& ec) const;

    void set_timeout(std::chrono::milliseconds timeout) override;
    void clear_timeout() override;

public:
    bool set_no_delay(bool enable, std::error_code& ec);
    bool set_keep_alive(bool enable, std::error_code& ec);
    bool set_keep_alive_params(int idle_seconds, int interval_seconds, int probes, std::error_code& ec);
    bool set_linger(bool enable, int seconds, std::error_code& ec);
    bool set_receive_buffer_size(int bytes, std::error_code& ec);
    bool set_send_buffer_size(int bytes, std::error_code& ec);
    int  receive_buffer_size(std::error_code& ec) const;
    int  send_buffer_size(std::error_code& ec) const;
    bool set_option(int level, int name, const void* value, std::size_t len, std::error_code& ec);
    bool get_option(int level, int name, void* value, std::size_t& len, std::error_code& ec) const;
    std::size_t available(std::error_code& ec) const;

    void set_no_delay(bool enable) { std::error_code ec; set_no_delay(enable, ec); }
    void set_keep_alive(bool enable) { std::error_code ec; set_keep_alive(enable, ec); }

private:
    IOContext*                   m_ctx;
    native_socket_t              m_fd;
    detail::DescriptorPtr        m_desc;
    std::unique_ptr<SteadyTimer> m_deadline;

    void start(detail::Reactor::OpType type, UniqueFunction<bool(std::error_code&, std::size_t&)> perform, IoHandler handler, bool speculative);
    void post_result(IoHandler handler, std::error_code ec, std::size_t n);
};

class TcpAcceptor {
public:
    using AcceptHandler = UniqueFunction<void(std::error_code, TcpStream)>;

    explicit TcpAcceptor(IOContext& ctx) noexcept;
    TcpAcceptor(IOContext& ctx, const Endpoint& endpoint, bool reuse_address = true, int backlog = 512);
    ~TcpAcceptor();

    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;
    TcpAcceptor(TcpAcceptor&& other) noexcept;
    TcpAcceptor& operator=(TcpAcceptor&& other) noexcept;

public:
    IOContext& context() const noexcept { return *m_ctx; }

    bool open(Protocol protocol, std::error_code& ec);
    bool bind(const Endpoint& endpoint, std::error_code& ec);
    bool listen(int backlog, std::error_code& ec);
    bool listen(const Endpoint& endpoint, std::error_code& ec, bool reuse_address = true, int backlog = 512);

    bool set_reuse_address(bool enable, std::error_code& ec);
    bool set_reuse_port(bool enable, std::error_code& ec);
    bool set_v6_only(bool enable, std::error_code& ec);

    bool is_open() const noexcept { return m_fd != invalid_socket; }
    void close();
    void cancel();
    native_socket_t native_handle() const noexcept { return m_fd; }

    Endpoint local_endpoint() const;
    Endpoint local_endpoint(std::error_code& ec) const;

    void async_accept(AcceptHandler handler);
    TcpStream accept(std::error_code& ec, int timeout_ms = -1);
    TcpStream accept();

private:
    IOContext*            m_ctx;
    native_socket_t       m_fd;
    detail::DescriptorPtr m_desc;
};

} // namespace async
} // namespace networking
} // namespace fizmo

#endif // FIZMO_ASYNC_TCP_HPP
