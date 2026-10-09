#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "posix_impl.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

#if defined(OS_LINUX)
auto PosixImpl::suppress_sigpipe_once() noexcept -> void {
        static const bool done = [] {
            struct sigaction sa;
            std::memset(&sa, 0, sizeof(sa));
            sa.sa_handler = SIG_IGN;
            ::sigaction(SIGPIPE, &sa, nullptr);
            return true;
        }();

        (void)done;
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::apply_socket_defaults() noexcept -> void {
    #ifdef SO_NOSIGPIPE
        if (m_socket != kInvalidSocket) {
            int on = 1;
            ::setsockopt(m_socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
        }
    #endif
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::clamp_ssize(ssize_t n) noexcept -> int {
        if (n < 0) return -1;
        if (n > static_cast<ssize_t>(INT_MAX)) return INT_MAX;
        return static_cast<int>(n);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::fill_sockaddr(const NetworkAddress& address, struct sockaddr_storage& storage) noexcept -> socklen_t {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::parse_sockaddr(const struct sockaddr_storage& storage) noexcept -> NetworkAddress {
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
#endif

#if defined(OS_LINUX)
PosixImpl::PosixImpl(SocketType type, AddressFamily family) : SocketImplBase(type, family), m_socket(kInvalidSocket) {
        suppress_sigpipe_once();
        m_socket = create_socket();
        apply_socket_defaults();
    }
#endif

#if defined(OS_LINUX)
PosixImpl::PosixImpl(int existing_socket, SocketType type, AddressFamily family) : SocketImplBase(type, family, true), m_socket(existing_socket) {
        suppress_sigpipe_once();
        apply_socket_defaults();
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::set_send_timeout(unsigned int milliseconds) noexcept -> void {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_send_timeout_ms = milliseconds;

        if (m_socket != kInvalidSocket) {
            struct timeval tv;
            tv.tv_sec  = static_cast<time_t>(milliseconds / 1000);
            tv.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
            ::setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::set_receive_timeout(unsigned int milliseconds) noexcept -> void {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_receive_timeout_ms = milliseconds;

        if (m_socket != kInvalidSocket) {
            struct timeval tv;
            tv.tv_sec  = static_cast<time_t>(milliseconds / 1000);
            tv.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
            ::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        }
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::connect(const NetworkAddress& address) noexcept -> bool {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::bind(const NetworkAddress& address) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(address, storage);
        return (::bind(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len) == 0);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::listen(int backlog) noexcept -> bool {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return false;
        return (::listen(m_socket, backlog) == 0);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::accept() noexcept -> std::unique_ptr<SocketImplBase> {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::send(const void* data, std::size_t length) noexcept -> int {
        if (m_socket == kInvalidSocket || !m_connected.load(std::memory_order_acquire)) return -1;
        ssize_t n;
        do { n = ::send(m_socket, data, length, send_flags()); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::receive(void* buffer, std::size_t length) noexcept -> int {
        if (m_socket == kInvalidSocket || !m_connected.load(std::memory_order_acquire)) return -1;
        ssize_t n;
        do { n = ::recv(m_socket, buffer, length, 0); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        struct sockaddr_storage storage;
        socklen_t addr_len = fill_sockaddr(dest, storage);
        ssize_t n;
        do { n = ::sendto(m_socket, data, length, send_flags(), reinterpret_cast<struct sockaddr*>(&storage), addr_len); }
        while (n < 0 && errno == EINTR);
        return clamp_ssize(n);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::close() noexcept -> void {
        std::lock_guard<std::mutex> lock(m_impl_mutex);

        if (m_socket != kInvalidSocket) {
            ::close(m_socket);
            m_socket = kInvalidSocket;
        }

        m_connected.store(false, std::memory_order_release);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::shutdown(ShutdownMode mode) noexcept -> bool {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::set_blocking(bool blocking) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        int flags = ::fcntl(m_socket, F_GETFL, 0);
        if (flags == -1) return -1;
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (::fcntl(m_socket, F_SETFL, flags) == -1) ? -1 : 0;
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::set_option(int level, int option, const void* value, int length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket) return -1;
        return ::setsockopt(m_socket, level, option, value, static_cast<socklen_t>(length));
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::get_option(int level, int option, void* value, int* length) noexcept -> int {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        if (m_socket == kInvalidSocket || !length) return -1;
        socklen_t len = static_cast<socklen_t>(*length);
        int result = ::getsockopt(m_socket, level, option, value, &len);
        *length = static_cast<int>(len);
        return result;
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept -> int {
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
#endif

#if defined(OS_LINUX)
auto PosixImpl::is_readable(int sock) noexcept -> bool {
        if (sock == kInvalidSocket) return false;
        struct pollfd pfd; pfd.fd = sock; pfd.events = POLLIN; pfd.revents = 0;
        return (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) != 0);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::is_writable(int sock) noexcept -> bool {
        if (sock == kInvalidSocket) return false;
        struct pollfd pfd; pfd.fd = sock; pfd.events = POLLOUT; pfd.revents = 0;
        return (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLOUT) != 0);
    }
#endif

#if defined(OS_LINUX)
auto PosixImpl::check_connect_completed() noexcept -> bool {
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
#endif

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
