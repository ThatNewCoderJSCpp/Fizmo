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

    static void suppress_sigpipe_once() noexcept {
        static const bool done = [] {
            struct sigaction sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_IGN;
            ::sigaction(SIGPIPE, &sa, nullptr);
            return true;
        }();

        (void)done;
    }

    void apply_socket_defaults() noexcept {
    #ifdef SO_NOSIGPIPE
        if (m_socket != kInvalidSocket) {
            int on = 1;
            ::setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
        }
    #endif
    }

    static int clamp_ssize(ssize_t n) noexcept {
        if (n < 0) return -1;
        if (n > static_cast<ssize_t>(INT_MAX)) return INT_MAX;
        return static_cast<int>(n);
    }

public:
    int native_af() const noexcept { return static_cast<int>(m_family); }
    int native_socktype() const noexcept { return (m_type == SocketType::TCP) ? SOCK_STREAM : SOCK_DGRAM; }
    int native_protocol() const noexcept { return (m_type == SocketType::TCP) ? IPPROTO_TCP : IPPROTO_UDP; }

    int create_socket() const noexcept {
        return ::socket(native_af(), native_socktype(), native_protocol());
    }

    static socklen_t fill_sockaddr(const NetworkAddress& address, struct sockaddr_storage& storage) noexcept {
        std::memset(&storage, 0, sizeof(storage));

        if (address.family() == AddressFamily::IPv6) {
            auto* a6 = reinterpret_cast<struct sockaddr_in6*>(&storage);
            a6->sin6_family = AF_INET6;
            a6->sin6_port   = htons(address.port());
            ::inet_pton(AF_INET6, address.host().c_str(), &a6->sin6_addr);
            return static_cast<socklen_t>(sizeof(struct sockaddr_in6));
        }

        auto* a4 = reinterpret_cast<struct sockaddr_in*>(&storage);
        a4->sin_family = AF_INET;
        a4->sin_port   = htons(address.port());
        ::inet_pton(AF_INET, address.host().c_str(), &a4->sin_addr);
        return static_cast<socklen_t>(sizeof(struct sockaddr_in));
    }

    static NetworkAddress parse_sockaddr(const struct sockaddr_storage& storage) noexcept {
        if (storage.ss_family == AF_INET6) {
            auto* a6 = reinterpret_cast<const struct sockaddr_in6*>(&storage);
            char buf[INET6_ADDRSTRLEN];
            ::inet_ntop(AF_INET6, &a6->sin6_addr, buf, INET6_ADDRSTRLEN);
            return NetworkAddress(buf, ntohs(a6->sin6_port), AddressFamily::IPv6);
        }

        auto* a4 = reinterpret_cast<const struct sockaddr_in*>(&storage);
        char buf[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &a4->sin_addr, buf, INET_ADDRSTRLEN);
        return NetworkAddress(buf, ntohs(a4->sin_port), AddressFamily::IPv4);
    }

    static AddressFamily family_from_storage(const struct sockaddr_storage& ss) noexcept {
        return (ss.ss_family == AF_INET6) ? AddressFamily::IPv6 : AddressFamily::IPv4;
    }

public:
    explicit PosixImpl(SocketType type, AddressFamily family = AddressFamily::IPv4) : SocketImplBase(type, family), m_socket(kInvalidSocket) {
        suppress_sigpipe_once();
        m_socket = create_socket();
        apply_socket_defaults();
    }

    PosixImpl(int existing_socket, SocketType type, AddressFamily family) : SocketImplBase(type, family, true), m_socket(existing_socket) {
        suppress_sigpipe_once();
        apply_socket_defaults();
    }

    ~PosixImpl() override { close(); }

public:
    void set_send_timeout(unsigned int milliseconds) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_send_timeout_ms = milliseconds;

        if (m_socket != kInvalidSocket) {
            struct timeval tv;
            tv.tv_sec  = static_cast<time_t>(milliseconds / 1000);
            tv.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
            ::setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
    }

    void set_receive_timeout(unsigned int milliseconds) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_receive_timeout_ms = milliseconds;

        if (m_socket != kInvalidSocket) {
            struct timeval tv;
            tv.tv_sec  = static_cast<time_t>(milliseconds / 1000);
            tv.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
            ::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        }
    }

public: 
    bool connect(const NetworkAddress& address) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(address, storage);

        if (m_connect_timeout_ms == 0) {
            int result;
            do { result = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len); }
            while (result != 0 && errno == EINTR);
            m_connected.store(result == 0, std::memory_order_release);
            return m_connected.load(std::memory_order_acquire);
        }

        const int saved_flags = ::fcntl(m_socket, F_GETFL, 0);
        if (saved_flags == -1) return false;
        ::fcntl(m_socket, F_SETFL, saved_flags | O_NONBLOCK);
        int result = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len);

        if (result == 0) {
            ::fcntl(m_socket, F_SETFL, saved_flags);
            m_connected.store(true, std::memory_order_release);
            return true;
        }

        if (errno != EINPROGRESS) {
            ::fcntl(m_socket, F_SETFL, saved_flags);
            m_connected.store(false, std::memory_order_release);
            return false;
        }

        struct pollfd pfd;
        pfd.fd      = m_socket;
        pfd.events  = POLLOUT;
        pfd.revents = 0;
        int poll_result;
        do { poll_result = ::poll(&pfd, 1, static_cast<int>(m_connect_timeout_ms)); }
        while (poll_result < 0 && errno == EINTR);
        ::fcntl(m_socket, F_SETFL, saved_flags);

        if (poll_result == 0) {
            ::close(m_socket);
            m_socket = create_socket();
            apply_socket_defaults();
            errno = ETIMEDOUT;
            m_connected.store(false, std::memory_order_release);
            return false;
        }

        if (poll_result < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            m_connected.store(false, std::memory_order_release);
            return false;
        }

        int so_error = 0;
        socklen_t len = sizeof(so_error);

        if (::getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0) {
            if (so_error != 0) errno = so_error;
            m_connected.store(so_error == 0, std::memory_order_release);
            return m_connected.load(std::memory_order_acquire);
        }

        m_connected.store(false, std::memory_order_release);
        return false;
    }

    bool bind(const NetworkAddress& address) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(address, storage);
        return (::bind(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len) == 0);
    }

    bool listen(int backlog) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        return (::listen(m_socket, backlog) == 0);
    }

    std::unique_ptr<SocketImplBase> accept() noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return nullptr;
        struct sockaddr_storage client_storage;
        socklen_t addr_len = sizeof(client_storage);
        std::memset(&client_storage, 0, sizeof(client_storage));
        int client;
        
    #ifdef SOCK_CLOEXEC
        do { client = ::accept4(m_socket, reinterpret_cast<struct sockaddr*>(&client_storage), &addr_len, SOCK_CLOEXEC); }
        while (client < 0 && errno == EINTR);
    #else
        do { client = ::accept(m_socket, reinterpret_cast<struct sockaddr*>(&client_storage), &addr_len); }
        while (client < 0 && errno == EINTR);
    #endif

        if (client == kInvalidSocket) return nullptr;
        AddressFamily client_family = family_from_storage(client_storage);
        return std::unique_ptr<SocketImplBase>(new PosixImpl(client, m_type, client_family));
    }

    int send(const void* data, std::size_t length) noexcept override {
        if (m_socket == kInvalidSocket || !m_connected.load(std::memory_order_acquire)) return -1;
        ssize_t n;
        do { n = ::send(m_socket, data, length, send_flags()); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

    int receive(void* buffer, std::size_t length) noexcept override {
        if (m_socket == kInvalidSocket || !m_connected.load(std::memory_order_acquire)) return -1;
        ssize_t n;
        do { n = ::recv(m_socket, buffer, length, 0); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

public: 
    int sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(dest, storage);
        ssize_t n;
        do { n = ::sendto(m_socket, data, length, send_flags(), reinterpret_cast<struct sockaddr*>(&storage), addr_len); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

    int recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        struct sockaddr_storage storage;
        socklen_t addr_len = sizeof(storage);
        std::memset(&storage, 0, sizeof(storage));
        ssize_t n;
        do { n = ::recvfrom(m_socket, buffer, length, 0, reinterpret_cast<struct sockaddr*>(&storage), &addr_len); }
        while (n < 0 && errno == EINTR);
        if (n >= 0) { source = parse_sockaddr(storage); }
        return clamp_ssize(n);
    }

public:
    int scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        if (buffer_count == 0 || !buffers) return 0;
        constexpr std::size_t STACK_LIMIT = 16;
        struct iovec stack_iov[STACK_LIMIT];
        std::unique_ptr<struct iovec[]> heap_iov;
        struct iovec* iov = stack_iov;

        if (buffer_count > STACK_LIMIT) {
            heap_iov = std::unique_ptr<struct iovec[]>(new struct iovec[buffer_count]);
            iov = heap_iov.get();
        }

        for (std::size_t i = 0; i < buffer_count; ++i) {
            iov[i].iov_base = buffers[i].data;
            iov[i].iov_len  = buffers[i].length;
        }

        ssize_t n;
        do { n = ::readv(m_socket, iov, static_cast<int>(buffer_count)); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

    int gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        if (buffer_count == 0 || !buffers) return 0;
        constexpr std::size_t STACK_LIMIT = 16;
        struct iovec stack_iov[STACK_LIMIT];
        std::unique_ptr<struct iovec[]> heap_iov;
        struct iovec* iov = stack_iov;

        if (buffer_count > STACK_LIMIT) {
            heap_iov = std::unique_ptr<struct iovec[]>(new struct iovec[buffer_count]);
            iov = heap_iov.get();
        }

        for (std::size_t i = 0; i < buffer_count; ++i) {
            iov[i].iov_base = const_cast<void*>(buffers[i].data);
            iov[i].iov_len  = buffers[i].length;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.msg_iov    = iov;
        msg.msg_iovlen = buffer_count;
        ssize_t n;
        do { n = ::sendmsg(m_socket, &msg, send_flags()); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

    int scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        if (buffer_count == 0 || !buffers) return 0;
        constexpr std::size_t STACK_LIMIT = 16;
        struct iovec stack_iov[STACK_LIMIT];
        std::unique_ptr<struct iovec[]> heap_iov;
        struct iovec* iov = stack_iov;

        if (buffer_count > STACK_LIMIT) {
            heap_iov = std::unique_ptr<struct iovec[]>(new struct iovec[buffer_count]);
            iov = heap_iov.get();
        }

        for (std::size_t i = 0; i < buffer_count; ++i) {
            iov[i].iov_base = buffers[i].data;
            iov[i].iov_len  = buffers[i].length;
        }

        struct sockaddr_storage storage;
        std::memset(&storage, 0, sizeof(storage));
        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &storage;
        msg.msg_namelen = sizeof(storage);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = buffer_count;
        ssize_t n;
        do { n = ::recvmsg(m_socket, &msg, 0); }
        while (n < 0 && errno == EINTR);
        if (n >= 0) { source = parse_sockaddr(storage); }
        return clamp_ssize(n);
    }

    int gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        if (buffer_count == 0 || !buffers) return 0;
        constexpr std::size_t STACK_LIMIT = 16;
        struct iovec stack_iov[STACK_LIMIT];
        std::unique_ptr<struct iovec[]> heap_iov;
        struct iovec* iov = stack_iov;

        if (buffer_count > STACK_LIMIT) {
            heap_iov = std::unique_ptr<struct iovec[]>(new struct iovec[buffer_count]);
            iov = heap_iov.get();
        }

        for (std::size_t i = 0; i < buffer_count; ++i) {
            iov[i].iov_base = const_cast<void*>(buffers[i].data);
            iov[i].iov_len  = buffers[i].length;
        }

        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(dest, storage);
        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &storage;
        msg.msg_namelen = addr_len;
        msg.msg_iov     = iov;
        msg.msg_iovlen  = buffer_count;
        ssize_t n;
        do { n = ::sendmsg(m_socket, &msg, send_flags()); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }

public: 
    void close() noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);

        if (m_socket != kInvalidSocket) {
            ::close(m_socket);
            m_socket = kInvalidSocket;
        }

        m_connected.store(false, std::memory_order_release);
    }

    bool shutdown(ShutdownMode mode) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        int how;

        switch (mode) {
            case ShutdownMode::Read:  how = SHUT_RD;   break;
            case ShutdownMode::Write: how = SHUT_WR;   break;
            default:                  how = SHUT_RDWR; break;
        }

        return (::shutdown(m_socket, how) == 0);
    }

    int set_blocking(bool blocking) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        int flags = ::fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) return -1;
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (::fcntl(m_socket, F_SETFL, flags) == -1) ? -1 : 0;
    }

    int set_option(int level, int option, const void* value, int length) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        return ::setsockopt(m_socket, level, option, value, static_cast<socklen_t>(length));
    }

    int get_option(int level, int option, void* value, int* length) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket || !length) return -1;
        socklen_t len = static_cast<socklen_t>(*length);
        int result = ::getsockopt(m_socket, level, option, value, &len);
        *length = static_cast<int>(len);
        return result;
    }

    int set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept override {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        int on = 1;
        if (::setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) != 0) return -1;

    #if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL)
        int idle     = static_cast<int>(idle_ms / 1000);
        int interval = static_cast<int>(interval_ms / 1000);
        if (idle     < 1) idle     = 1;
        if (interval < 1) interval = 1;
        if (::setsockopt(m_socket, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,     sizeof(idle))     != 0) return -1;
        if (::setsockopt(m_socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0) return -1;
        return 0;
    #else
        (void)idle_ms; (void)interval_ms;
        return -1;
    #endif
    }

public:
    int get_raw_socket() const noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        return m_socket;
    }

    static bool would_block() noexcept {
        return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    static bool is_readable(int sock) noexcept {
        if (sock == kInvalidSocket) return false;
        struct pollfd pfd; pfd.fd = sock; pfd.events = POLLIN; pfd.revents = 0;
        return (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) != 0);
    }

    static bool is_writable(int sock) noexcept {
        if (sock == kInvalidSocket) return false;
        struct pollfd pfd; pfd.fd = sock; pfd.events = POLLOUT; pfd.revents = 0;
        return (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLOUT) != 0);
    }

    bool check_connect_completed() noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        struct pollfd pfd; pfd.fd = m_socket; pfd.events = POLLOUT; pfd.revents = 0;
        if (::poll(&pfd, 1, 0) <= 0) return false;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;
        if (!(pfd.revents & POLLOUT)) return false;
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (::getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0) return false;
        m_connected.store(so_error == 0, std::memory_order_release);
        return (so_error == 0);
    }

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