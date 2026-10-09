#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "winsock_impl.hpp"

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

#if defined(OS_WINDOWS)
bool WinsockImpl::WinsockLifetime::acquire() {
    std::lock_guard<std::mutex> lock(init_mutex);

    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
        initialized = true;
    }

    ++ref_count;
    return true;
}
#endif

#if defined(OS_WINDOWS)
void WinsockImpl::WinsockLifetime::release() {
    std::lock_guard<std::mutex> lock(init_mutex);

    if (--ref_count <= 0 && initialized) {
        WSACleanup();
        initialized = false;
        ref_count = 0;
    }
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::wsa_lifetime() -> WinsockLifetime& {
    static WinsockLifetime instance;
    return instance;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::native_af() const noexcept { return static_cast<int>(m_family); }
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::native_socktype() const noexcept { return (m_type == SocketType::TCP) ? SOCK_STREAM : SOCK_DGRAM; }
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::native_protocol() const noexcept { return (m_type == SocketType::TCP) ? IPPROTO_TCP : IPPROTO_UDP; }
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::create_socket() const noexcept -> SOCKET { return ::socket(native_af(), native_socktype(), native_protocol()); }
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::fill_sockaddr(const NetworkAddress& address, struct sockaddr_storage& storage) noexcept {
    std::memset(&storage, 0, sizeof(storage));

    if (address.family() == AddressFamily::IPv6) {
        auto* a6 = reinterpret_cast<struct sockaddr_in6*>(&storage);
        a6->sin6_family = AF_INET6;
        a6->sin6_port   = htons(address.port());
        inet_pton(AF_INET6, address.host().c_str(), &a6->sin6_addr);
        return static_cast<int>(sizeof(struct sockaddr_in6));
    }

    auto* a4 = reinterpret_cast<struct sockaddr_in*>(&storage);
    a4->sin_family = AF_INET;
    a4->sin_port   = htons(address.port());
    inet_pton(AF_INET, address.host().c_str(), &a4->sin_addr);
    return static_cast<int>(sizeof(struct sockaddr_in));
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::parse_sockaddr(const struct sockaddr_storage& storage) noexcept -> NetworkAddress {
    if (storage.ss_family == AF_INET6) {
        auto* a6 = reinterpret_cast<const struct sockaddr_in6*>(&storage);
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &a6->sin6_addr, buf, INET6_ADDRSTRLEN);
        return NetworkAddress(buf, ntohs(a6->sin6_port), AddressFamily::IPv6);
    }

    auto* a4 = reinterpret_cast<const struct sockaddr_in*>(&storage);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &a4->sin_addr, buf, INET_ADDRSTRLEN);
    return NetworkAddress(buf, ntohs(a4->sin_port), AddressFamily::IPv4);
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::family_from_storage(const struct sockaddr_storage& ss) noexcept -> AddressFamily {
    return (ss.ss_family == AF_INET6) ? AddressFamily::IPv6 : AddressFamily::IPv4;
}
#endif

#if defined(OS_WINDOWS)
WinsockImpl::WinsockImpl(SocketType type, AddressFamily family) : SocketImplBase(type, family) {
    wsa_lifetime().acquire();
    m_socket = create_socket();
}
#endif

#if defined(OS_WINDOWS)
WinsockImpl::WinsockImpl(SOCKET existingSocket, SocketType type, AddressFamily family) : SocketImplBase(type, family, true), m_socket(existingSocket) {
    wsa_lifetime().acquire();
}
#endif

#if defined(OS_WINDOWS)
WinsockImpl::~WinsockImpl() {
    close();
    wsa_lifetime().release();
}
#endif

#if defined(OS_WINDOWS)
void WinsockImpl::set_send_timeout(unsigned int milliseconds) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    m_send_timeout_ms = milliseconds;

    if (m_socket != INVALID_SOCKET) {
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&m_send_timeout_ms), sizeof(m_send_timeout_ms));
    }
}
#endif

#if defined(OS_WINDOWS)
void WinsockImpl::set_receive_timeout(unsigned int milliseconds) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    m_receive_timeout_ms = milliseconds;

    if (m_socket != INVALID_SOCKET) {
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&m_receive_timeout_ms), sizeof(m_receive_timeout_ms));
    }
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::shutdown(ShutdownMode mode) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    int how;

    switch (mode) {
        case ShutdownMode::Read:  how = SD_RECEIVE; break;
        case ShutdownMode::Write: how = SD_SEND;    break;
        case ShutdownMode::Both:  how = SD_BOTH;    break;
        default: how = SD_BOTH; break;
    }

    int result = ::shutdown(m_socket, how);
    if (result == 0 && mode != ShutdownMode::Read) { m_connected.store(false, std::memory_order_release); }
    return result == 0;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET || !m_connected.load(std::memory_order_acquire)) return -1;
    if (buffer_count == 0 || !buffers) return 0;
    constexpr std::size_t STACK_LIMIT = 16;
    WSABUF stack_bufs[STACK_LIMIT];
    std::unique_ptr<WSABUF[]> heap_bufs;
    WSABUF* wsa_bufs = stack_bufs;

    if (buffer_count > STACK_LIMIT) {
        heap_bufs = std::make_unique<WSABUF[]>(buffer_count);
        wsa_bufs = heap_bufs.get();
    }

    for (std::size_t i = 0; i < buffer_count; ++i) {
        wsa_bufs[i].buf = static_cast<char*>(buffers[i].data);
        wsa_bufs[i].len = static_cast<ULONG>(buffers[i].length);
    }

    DWORD bytes_received = 0;
    DWORD flags = 0;
    int result = WSARecv(m_socket, wsa_bufs, static_cast<DWORD>(buffer_count), &bytes_received, &flags, nullptr, nullptr);
    return (result == 0) ? static_cast<int>(bytes_received) : -1;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET || !m_connected.load(std::memory_order_acquire)) return -1;
    if (buffer_count == 0 || !buffers) return 0;
    constexpr std::size_t STACK_LIMIT = 16;
    WSABUF stack_bufs[STACK_LIMIT];
    std::unique_ptr<WSABUF[]> heap_bufs;
    WSABUF* wsa_bufs = stack_bufs;

    if (buffer_count > STACK_LIMIT) {
        heap_bufs = std::make_unique<WSABUF[]>(buffer_count);
        wsa_bufs = heap_bufs.get();
    }

    for (std::size_t i = 0; i < buffer_count; ++i) {
        wsa_bufs[i].buf = static_cast<char*>(const_cast<void*>(buffers[i].data));
        wsa_bufs[i].len = static_cast<ULONG>(buffers[i].length);
    }

    DWORD bytes_sent = 0;
    int result = WSASend(m_socket, wsa_bufs, static_cast<DWORD>(buffer_count), &bytes_sent, 0, nullptr, nullptr);
    return (result == 0) ? static_cast<int>(bytes_sent) : -1;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    if (buffer_count == 0 || !buffers) return 0;
    constexpr std::size_t STACK_LIMIT = 16;
    WSABUF stack_bufs[STACK_LIMIT];
    std::unique_ptr<WSABUF[]> heap_bufs;
    WSABUF* wsa_bufs = stack_bufs;

    if (buffer_count > STACK_LIMIT) {
        heap_bufs = std::make_unique<WSABUF[]>(buffer_count);
        wsa_bufs = heap_bufs.get();
    }

    for (std::size_t i = 0; i < buffer_count; ++i) {
        wsa_bufs[i].buf = static_cast<char*>(buffers[i].data);
        wsa_bufs[i].len = static_cast<ULONG>(buffers[i].length);
    }

    struct sockaddr_storage storage;
    int addr_len = sizeof(storage);
    std::memset(&storage, 0, sizeof(storage));
    DWORD bytes_received = 0;
    DWORD flags = 0;

    int result = WSARecvFrom(
        m_socket, 
        wsa_bufs,
        static_cast<DWORD>(buffer_count),
        &bytes_received, 
        &flags,
        reinterpret_cast<struct sockaddr*>(&storage), 
        &addr_len,
        nullptr, 
        nullptr
    );
        
    if (result == 0) {
        source = parse_sockaddr(storage);
        return static_cast<int>(bytes_received);
    }

    return -1;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    if (buffer_count == 0 || !buffers) return 0;
    constexpr std::size_t STACK_LIMIT = 16;
    WSABUF stack_bufs[STACK_LIMIT];
    std::unique_ptr<WSABUF[]> heap_bufs;
    WSABUF* wsa_bufs = stack_bufs;

    if (buffer_count > STACK_LIMIT) {
        heap_bufs = std::make_unique<WSABUF[]>(buffer_count);
        wsa_bufs = heap_bufs.get();
    }

    for (std::size_t i = 0; i < buffer_count; ++i) {
        wsa_bufs[i].buf = static_cast<char*>(const_cast<void*>(buffers[i].data));
        wsa_bufs[i].len = static_cast<ULONG>(buffers[i].length);
    }

    struct sockaddr_storage storage;
    int addr_len = fill_sockaddr(dest, storage);
    DWORD bytes_sent = 0;

    int result = WSASendTo(
        m_socket, 
        wsa_bufs, 
        static_cast<DWORD>(buffer_count),
        &bytes_sent, 
        0,
        reinterpret_cast<struct sockaddr*>(&storage), 
        addr_len,
        nullptr, 
        nullptr
    );

    return (result == 0) ? static_cast<int>(bytes_sent) : -1;
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::connect(const NetworkAddress& address) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    struct sockaddr_storage storage;
    int addr_len = fill_sockaddr(address, storage);

    if (m_connect_timeout_ms > 0) {
        u_long mode = 1;
        ioctlsocket(m_socket, FIONBIO, &mode);
        int result = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len);

        if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
            mode = 0; ioctlsocket(m_socket, FIONBIO, &mode);
            return false;
        }

        fd_set write_set, error_set;
        FD_ZERO(&write_set); FD_ZERO(&error_set);
        FD_SET(m_socket, &write_set); FD_SET(m_socket, &error_set);
        struct timeval tv;
        tv.tv_sec  = m_connect_timeout_ms / 1000;
        tv.tv_usec = (m_connect_timeout_ms % 1000) * 1000;
        result = select(0, NULL, &write_set, &error_set, &tv);
        mode = 0; ioctlsocket(m_socket, FIONBIO, &mode);

        if (result == 0) {
            closesocket(m_socket);
            m_socket = create_socket();
            m_connected.store(false, std::memory_order_release);
            return false;
        }

        if (result == SOCKET_ERROR || FD_ISSET(m_socket, &error_set)) {
            m_connected.store(false, std::memory_order_release);
            return false;
        }

        int error = 0; int len = sizeof(error);

        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == 0) {
            m_connected.store(error == 0, std::memory_order_release);
            return m_connected.load(std::memory_order_acquire);
        }

        m_connected.store(false, std::memory_order_release);
        return false;
    } else {
        int result = ::connect(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len);
        m_connected.store(result == 0, std::memory_order_release);
        return m_connected.load(std::memory_order_acquire);
    }
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::bind(const NetworkAddress& address) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    struct sockaddr_storage storage;
    int addr_len = fill_sockaddr(address, storage);
    return (::bind(m_socket, reinterpret_cast<struct sockaddr*>(&storage), addr_len) == 0);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::listen(int backlog) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    return (::listen(m_socket, backlog) == 0);
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::accept() noexcept -> std::unique_ptr<SocketImplBase> {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return nullptr;
    struct sockaddr_storage client_storage;
    int addr_len = sizeof(client_storage);
    SOCKET clientSocket = ::accept(m_socket, reinterpret_cast<struct sockaddr*>(&client_storage), &addr_len);
    if (clientSocket == INVALID_SOCKET) return nullptr;
    AddressFamily client_family = family_from_storage(client_storage);
    return std::make_unique<WinsockImpl>(clientSocket, m_type, client_family);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::send(const void* data, size_t length) noexcept {
    if (m_socket == INVALID_SOCKET || !m_connected.load(std::memory_order_acquire)) return -1;
    return ::send(m_socket, static_cast<const char*>(data), static_cast<int>(length), 0);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::receive(void* buffer, size_t length) noexcept {
    if (m_socket == INVALID_SOCKET || !m_connected.load(std::memory_order_acquire)) return -1;
    return ::recv(m_socket, static_cast<char*>(buffer), static_cast<int>(length), 0);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    struct sockaddr_storage storage;
    int addr_len = fill_sockaddr(dest, storage);
    return ::sendto(m_socket, static_cast<const char*>(data), static_cast<int>(length), 0, reinterpret_cast<struct sockaddr*>(&storage), addr_len);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    struct sockaddr_storage storage;
    int addr_len = sizeof(storage);
    std::memset(&storage, 0, sizeof(storage));
    int result = ::recvfrom(m_socket, static_cast<char*>(buffer), static_cast<int>(length), 0, reinterpret_cast<struct sockaddr*>(&storage), &addr_len);
    if (result >= 0) { source = parse_sockaddr(storage); }
    return result;
}
#endif

#if defined(OS_WINDOWS)
void WinsockImpl::close() noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    m_connected.store(false, std::memory_order_release);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::set_blocking(bool blocking) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;   
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(m_socket, FIONBIO, &mode);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::set_option(int level, int option, const void* value, int length) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1; 
    return setsockopt(m_socket, level, option, static_cast<const char*>(value), length);
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::get_option(int level, int option, void* value, int* length) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    return getsockopt(m_socket, level, option, static_cast<char*>(value), length);
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::get_raw_socket() const noexcept -> SOCKET {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    return m_socket;
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::associate_iocp(HANDLE iocp_handle, ULONG_PTR key) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    HANDLE result = CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_socket), iocp_handle, key, 0);
    return result != nullptr;
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::would_block() noexcept {
    int error = WSAGetLastError();
    return (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS);
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::is_readable(SOCKET sock) noexcept {
    if (sock == INVALID_SOCKET) return false;
    fd_set read_set; FD_ZERO(&read_set); FD_SET(sock, &read_set);
    struct timeval tv = {0, 0};
    return (select(0, &read_set, NULL, NULL, &tv) > 0 && FD_ISSET(sock, &read_set));
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::is_writable(SOCKET sock) noexcept {
    if (sock == INVALID_SOCKET) return false;
    fd_set write_set; FD_ZERO(&write_set); FD_SET(sock, &write_set);
    struct timeval tv = {0, 0};
    return (select(0, NULL, &write_set, NULL, &tv) > 0 && FD_ISSET(sock, &write_set));
}
#endif

#if defined(OS_WINDOWS)
bool WinsockImpl::check_connect_completed() noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return false;
    fd_set write_set, error_set;
    FD_ZERO(&write_set); FD_ZERO(&error_set);
    FD_SET(m_socket, &write_set); FD_SET(m_socket, &error_set);
    struct timeval tv = {0, 0};
    int result = select(0, NULL, &write_set, &error_set, &tv);

    if (result > 0) {
        if (FD_ISSET(m_socket, &error_set)) return false;

        if (FD_ISSET(m_socket, &write_set)) {
            int error = 0; int len = sizeof(error);
            if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) == 0) {
                bool success = error == 0;
                m_connected.store(success, std::memory_order_release);
                return m_connected.load(std::memory_order_acquire);
            }
        }
    }

    return false;
}
#endif

#if defined(OS_WINDOWS)
int WinsockImpl::set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    if (m_socket == INVALID_SOCKET) return -1;
    struct tcp_keepalive ka;
    ka.onoff             = 1;
    ka.keepalivetime     = idle_ms;
    ka.keepaliveinterval = interval_ms;
    DWORD bytes_returned = 0;

    int result = WSAIoctl(
        m_socket,
        SIO_KEEPALIVE_VALS,
        &ka, 
        sizeof(ka),
        nullptr, 
        0,
        &bytes_returned,
        nullptr, 
        nullptr
    );

    return (result == 0) ? 0 : -1;
}
#endif

#if defined(OS_WINDOWS)
auto WinsockImpl::native_handle() const noexcept -> native_handle_t {
    std::lock_guard<std::mutex> lock(m_impl_mutex);
    return m_socket;
}
#endif

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo
