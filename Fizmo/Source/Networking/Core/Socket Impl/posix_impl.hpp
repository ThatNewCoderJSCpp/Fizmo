#ifndef FIZMO_POSIX_IMPL_FILE_HPP
#define FIZMO_POSIX_IMPL_FILE_HPP

#include "socket_impl_base.hpp"

#ifdef OS_LINUX

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

class PosixImpl : public SocketImplBase {
public:
    static constexpr int kInvalidSocket = -1;

private:
    int m_socket;

    static int send_flags() noexcept {
    #ifdef MSG_NOSIGNAL
        return MSG_NOSIGNAL;
    #else
        return 0;
    #endif
    }

    static void suppress_sigpipe_once() noexcept;

    void apply_socket_defaults() noexcept;

    static int clamp_ssize(ssize_t n) noexcept;

public:
    int native_af() const noexcept { return static_cast<int>(m_family); }
    int native_socktype() const noexcept { return (m_type == SocketType::TCP) ? SOCK_STREAM : SOCK_DGRAM; }
    int native_protocol() const noexcept { return (m_type == SocketType::TCP) ? IPPROTO_TCP : IPPROTO_UDP; }

    int create_socket() const noexcept {
        return ::socket(native_af(), native_socktype(), native_protocol());
    }

    static socklen_t fill_sockaddr(const NetworkAddress& address, struct sockaddr_storage& storage) noexcept;

    static NetworkAddress parse_sockaddr(const struct sockaddr_storage& storage) noexcept;

    static AddressFamily family_from_storage(const struct sockaddr_storage& ss) noexcept {
        return (ss.ss_family == AF_INET6) ? AddressFamily::IPv6 : AddressFamily::IPv4;
    }

public:
    explicit PosixImpl(SocketType type, AddressFamily family = AddressFamily::IPv4);

    PosixImpl(int existing_socket, SocketType type, AddressFamily family);

    ~PosixImpl() override { close(); }

public:
    void set_send_timeout(unsigned int milliseconds) noexcept override;

    void set_receive_timeout(unsigned int milliseconds) noexcept override;

public: 
    bool connect(const NetworkAddress& address) noexcept override;

    bool bind(const NetworkAddress& address) noexcept override;

    bool listen(int backlog) noexcept override;

    std::unique_ptr<SocketImplBase> accept() noexcept override;

    int send(const void* data, std::size_t length) noexcept override;

    int receive(void* buffer, std::size_t length) noexcept override;

public: 
    int sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept override;

    int recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept override;

public:
    int scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept override;

    int gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept override;

    int scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept override;

    int gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept override;

public: 
    void close() noexcept override;

    bool shutdown(ShutdownMode mode) noexcept override;

    int set_blocking(bool blocking) noexcept override;

    int set_option(int level, int option, const void* value, int length) noexcept override;

    int get_option(int level, int option, void* value, int* length) noexcept override;

    int set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept override;

public:
    int get_raw_socket() const noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        return m_socket;
    }

    static bool would_block() noexcept {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    static bool is_readable(int sock) noexcept;

    static bool is_writable(int sock) noexcept;

    bool check_connect_completed() noexcept;

    native_handle_t native_handle() const noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        return m_socket;
    }
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_LINUX
#endif // FIZMO_POSIX_IMPL_FILE_HPP