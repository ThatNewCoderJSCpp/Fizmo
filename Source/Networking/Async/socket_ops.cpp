#include "fizmo_library.hpp"
#include "socket_ops.hpp"
#include "endpoint.hpp"

#include <mutex>

#if !defined(OS_WINDOWS)
    #include <net/if.h>
#endif

namespace fizmo {
namespace networking {
namespace async {
namespace detail {

namespace {

#if defined(OS_WINDOWS)
inline int last_native() noexcept { return ::WSAGetLastError(); }
inline bool is_eintr(int) noexcept { return false; }
#else
inline int last_native() noexcept { return errno; }
inline bool is_eintr(int e) noexcept { return e == EINTR; }
#endif

#if defined(MSG_NOSIGNAL)
constexpr int kNoSignal = MSG_NOSIGNAL;
#else
constexpr int kNoSignal = 0;
#endif

} // namespace

void ensure_socket_runtime() noexcept {
#if defined(OS_WINDOWS)
    static std::once_flag once;

    std::call_once(once, [] {
        WSADATA data;
        ::WSAStartup(MAKEWORD(2, 2), &data);
    });
#endif
}

unsigned interface_index(const std::string& name) noexcept {
#if defined(OS_WINDOWS)
    (void)name;
    return 0;
#else
    return ::if_nametoindex(name.c_str());
#endif
}

native_socket_t open_socket(int family, int type, int protocol, std::error_code& ec) noexcept {
    ensure_socket_runtime();
    ec.clear();
#if defined(OS_WINDOWS)
    native_socket_t s = ::WSASocketW(family, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED | 0x80);
    if (s == INVALID_SOCKET) s = ::WSASocketW(family, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (s == INVALID_SOCKET) { ec = last_socket_error(); return invalid_socket; }
    u_long mode = 1;
    if (::ioctlsocket(s, FIONBIO, &mode) != 0) { ec = last_socket_error(); ::closesocket(s); return invalid_socket; }
    if (type == SOCK_DGRAM) disable_udp_connreset(s);
    return s;
#elif defined(OS_LINUX) || defined(OS_ANDROID)
    native_socket_t s = ::socket(family, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
    if (s < 0) { ec = last_socket_error(); return invalid_socket; }
    return s;
#else
    native_socket_t s = ::socket(family, type, protocol);
    if (s < 0) { ec = last_socket_error(); return invalid_socket; }
    ::fcntl(s, F_SETFD, FD_CLOEXEC);
    if (!set_nonblocking(s, true, ec)) { ::close(s); return invalid_socket; }
    #if defined(SO_NOSIGPIPE)
    int one = 1;
    ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    #endif
    return s;
#endif
}

void close_socket(native_socket_t s) noexcept {
    if (s == invalid_socket) return;
#if defined(OS_WINDOWS)
    ::closesocket(s);
#else
    ::close(s);
#endif
}

bool set_nonblocking(native_socket_t s, bool enable, std::error_code& ec) noexcept {
    ec.clear();
#if defined(OS_WINDOWS)
    u_long mode = enable ? 1 : 0;
    if (::ioctlsocket(s, FIONBIO, &mode) != 0) { ec = last_socket_error(); return false; }
#else
    const int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) { ec = last_socket_error(); return false; }
    const int next = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(s, F_SETFL, next) < 0) { ec = last_socket_error(); return false; }
#endif
    return true;
}

bool set_option(native_socket_t s, int level, int name, const void* value, std::size_t len, std::error_code& ec) noexcept {
    ec.clear();
    if (::setsockopt(s, level, name, static_cast<const char*>(value), static_cast<socklen_type>(len)) != 0) {
        ec = last_socket_error();
        return false;
    }

    return true;
}

bool get_option(native_socket_t s, int level, int name, void* value, std::size_t& len, std::error_code& ec) noexcept {
    ec.clear();
    socklen_type n = static_cast<socklen_type>(len);

    if (::getsockopt(s, level, name, static_cast<char*>(value), &n) != 0) {
        ec = last_socket_error();
        return false;
    }

    len = static_cast<std::size_t>(n);
    return true;
}

bool set_int_option(native_socket_t s, int level, int name, int value, std::error_code& ec) noexcept {
    return set_option(s, level, name, &value, sizeof(value), ec);
}

int get_int_option(native_socket_t s, int level, int name, std::error_code& ec) noexcept {
    int value = 0;
    std::size_t len = sizeof(value);
    get_option(s, level, name, &value, len, ec);
    return value;
}

std::error_code pending_error(native_socket_t s) noexcept {
    std::error_code ec;
    const int err = get_int_option(s, SOL_SOCKET, SO_ERROR, ec);
    if (ec) return ec;
    return socket_error_code(err);
}

bool bind(native_socket_t s, const Endpoint& ep, std::error_code& ec) noexcept {
    ec.clear();
    if (::bind(s, ep.data(), ep.size()) != 0) { ec = last_socket_error(); return false; }
    return true;
}

bool listen(native_socket_t s, int backlog, std::error_code& ec) noexcept {
    ec.clear();
    if (::listen(s, backlog) != 0) { ec = last_socket_error(); return false; }
    return true;
}

bool start_connect(native_socket_t s, const Endpoint& ep, std::error_code& ec) noexcept {
    ec.clear();

    for (;;) {
        if (::connect(s, ep.data(), ep.size()) == 0) return true;
        const int e = last_native();
        if (is_eintr(e)) continue;
        ec = socket_error_code(e);
        return false;
    }
}

native_socket_t accept(native_socket_t s, Endpoint* peer, std::error_code& ec) noexcept {
    ec.clear();
    Endpoint tmp;
    Endpoint& target = peer ? *peer : tmp;
    socklen_type len = target.capacity();

    for (;;) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
        native_socket_t c = ::accept4(s, target.data(), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
        native_socket_t c = ::accept(s, target.data(), &len);
#endif

        if (c == invalid_socket) {
            const int e = last_native();
            if (is_eintr(e)) continue;
            ec = socket_error_code(e);
            return invalid_socket;
        }

        target.resize(len);
#if !defined(OS_LINUX) && !defined(OS_ANDROID)
        std::error_code ignored;
        set_nonblocking(c, true, ignored);
    #if !defined(OS_WINDOWS)
        ::fcntl(c, F_SETFD, FD_CLOEXEC);
        #if defined(SO_NOSIGPIPE)
        int one = 1;
        ::setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
        #endif
    #endif
#endif
        return c;
    }
}

long recv(native_socket_t s, void* data, std::size_t len, int flags, std::error_code& ec) noexcept {
    ec.clear();

    for (;;) {
#if defined(OS_WINDOWS)
        const int chunk = len > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(len);
        const long n = ::recv(s, static_cast<char*>(data), chunk, flags);
#else
        const long n = static_cast<long>(::recv(s, data, len, flags));
#endif
        if (n >= 0) return n;
        const int e = last_native();
        if (is_eintr(e)) continue;
        ec = socket_error_code(e);
        return -1;
    }
}

long send(native_socket_t s, const void* data, std::size_t len, int flags, std::error_code& ec) noexcept {
    ec.clear();

    for (;;) {
#if defined(OS_WINDOWS)
        const int chunk = len > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(len);
        const long n = ::send(s, static_cast<const char*>(data), chunk, flags);
#else
        const long n = static_cast<long>(::send(s, data, len, flags | kNoSignal));
#endif
        if (n >= 0) return n;
        const int e = last_native();
        if (is_eintr(e)) continue;
        ec = socket_error_code(e);
        return -1;
    }
}

long recv_from(native_socket_t s, void* data, std::size_t len, int flags, Endpoint& from, std::error_code& ec) noexcept {
    ec.clear();

    for (;;) {
        socklen_type alen = from.capacity();
#if defined(OS_WINDOWS)
        const int chunk = len > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(len);
        const long n = ::recvfrom(s, static_cast<char*>(data), chunk, flags, from.data(), &alen);
#else
        const long n = static_cast<long>(::recvfrom(s, data, len, flags, from.data(), &alen));
#endif
        if (n >= 0) { from.resize(alen); return n; }
        const int e = last_native();
        if (is_eintr(e)) continue;
#if defined(OS_WINDOWS)
        if (e == WSAEMSGSIZE) { from.resize(alen); ec = error::message_size; return static_cast<long>(len); }
#endif
        ec = socket_error_code(e);
        return -1;
    }
}

long send_to(native_socket_t s, const void* data, std::size_t len, int flags, const Endpoint& to, std::error_code& ec) noexcept {
    ec.clear();

    for (;;) {
#if defined(OS_WINDOWS)
        const long n = ::sendto(s, static_cast<const char*>(data), static_cast<int>(len), flags, to.data(), to.size());
#else
        const long n = static_cast<long>(::sendto(s, data, len, flags | kNoSignal, to.data(), to.size()));
#endif
        if (n >= 0) return n;
        const int e = last_native();
        if (is_eintr(e)) continue;
        ec = socket_error_code(e);
        return -1;
    }
}

bool shutdown(native_socket_t s, ShutdownType how, std::error_code& ec) noexcept {
    ec.clear();
#if defined(OS_WINDOWS)
    const int h = how == ShutdownType::Receive ? SD_RECEIVE : (how == ShutdownType::Send ? SD_SEND : SD_BOTH);
#else
    const int h = how == ShutdownType::Receive ? SHUT_RD : (how == ShutdownType::Send ? SHUT_WR : SHUT_RDWR);
#endif
    if (::shutdown(s, h) != 0) { ec = last_socket_error(); return false; }
    return true;
}

bool local_endpoint(native_socket_t s, Endpoint& out, std::error_code& ec) noexcept {
    ec.clear();
    socklen_type len = out.capacity();
    if (::getsockname(s, out.data(), &len) != 0) { ec = last_socket_error(); return false; }
    out.resize(len);
    return true;
}

bool remote_endpoint(native_socket_t s, Endpoint& out, std::error_code& ec) noexcept {
    ec.clear();
    socklen_type len = out.capacity();
    if (::getpeername(s, out.data(), &len) != 0) { ec = last_socket_error(); return false; }
    out.resize(len);
    return true;
}

std::size_t available(native_socket_t s, std::error_code& ec) noexcept {
    ec.clear();
#if defined(OS_WINDOWS)
    u_long n = 0;
    if (::ioctlsocket(s, FIONREAD, &n) != 0) { ec = last_socket_error(); return 0; }
#else
    int n = 0;
    if (::ioctl(s, FIONREAD, &n) != 0) { ec = last_socket_error(); return 0; }
#endif
    return static_cast<std::size_t>(n);
}

int wait(native_socket_t s, WaitType type, int timeout_ms, std::error_code& ec) noexcept {
    ec.clear();
#if defined(OS_WINDOWS)
    WSAPOLLFD pfd;
    pfd.fd = s;
    pfd.events = type == WaitType::Read ? POLLRDNORM : POLLWRNORM;
    pfd.revents = 0;
    const int r = ::WSAPoll(&pfd, 1, timeout_ms);
    if (r < 0) { ec = last_socket_error(); return -1; }
    return r;
#else
    pollfd pfd;
    pfd.fd = s;
    pfd.events = type == WaitType::Read ? POLLIN : POLLOUT;
    pfd.revents = 0;

    for (;;) {
        const int r = ::poll(&pfd, 1, timeout_ms);
        if (r >= 0) return r;
        if (errno == EINTR) continue;
        ec = last_socket_error();
        return -1;
    }
#endif
}

void disable_udp_connreset(native_socket_t s) noexcept {
#if defined(OS_WINDOWS)
    #ifndef SIO_UDP_CONNRESET
        #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
    #endif
    BOOL flag = FALSE;
    DWORD bytes = 0;
    ::WSAIoctl(s, SIO_UDP_CONNRESET, &flag, sizeof(flag), nullptr, 0, &bytes, nullptr, nullptr);
#else
    (void)s;
#endif
}

} // namespace detail
} // namespace async
} // namespace networking
} // namespace fizmo
