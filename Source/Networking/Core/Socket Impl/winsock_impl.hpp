#ifndef FIZMO_WINSOCK_IMPL_FILE_HPP
#define FIZMO_WINSOCK_IMPL_FILE_HPP

#include "socket_impl_base.hpp"

#ifdef OS_WINDOWS

namespace fizmo {
namespace networking {
namespace core {
namespace detail {

class WinsockImpl : public SocketImplBase {
private:
    SOCKET m_socket;

    struct WinsockLifetime {
        std::atomic<int> ref_count{0};
        std::mutex init_mutex;
        bool initialized = false;

        bool acquire();

        void release();
    };

    static WinsockLifetime& wsa_lifetime();

public:
    int native_af() const noexcept;
    int native_socktype() const noexcept;
    int native_protocol() const noexcept;
    SOCKET create_socket() const noexcept;

    static int fill_sockaddr(const NetworkAddress& address, struct sockaddr_storage& storage) noexcept;

    static NetworkAddress parse_sockaddr(const struct sockaddr_storage& storage) noexcept;

    static AddressFamily family_from_storage(const struct sockaddr_storage& ss) noexcept;

public:
    explicit WinsockImpl(SocketType type, AddressFamily family = AddressFamily::IPv4);

    WinsockImpl(SOCKET existingSocket, SocketType type, AddressFamily family);

    ~WinsockImpl();

public:
    void set_send_timeout(unsigned int milliseconds) noexcept override;

    void set_receive_timeout(unsigned int milliseconds) noexcept override;

public: 
    bool shutdown(ShutdownMode mode) noexcept override;

public: // SCATTER/GATHER I/O 
    int scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept override;

    int gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept override;

    int scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept override;

    int gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept override;

public: // TCP 
    bool connect(const NetworkAddress& address) noexcept override;

    bool bind(const NetworkAddress& address) noexcept override;

    bool listen(int backlog) noexcept override;

    std::unique_ptr<SocketImplBase> accept() noexcept override;

    int send(const void* data, size_t length) noexcept override;

    int receive(void* buffer, size_t length) noexcept override;

public: // UDP
    int sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept override;

    int recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept override;

public: // Common
    void close() noexcept override;

    int set_blocking(bool blocking) noexcept override;

    int set_option(int level, int option, const void* value, int length) noexcept override;

    int get_option(int level, int option, void* value, int* length) noexcept override;

public:
    SOCKET get_raw_socket() const noexcept;

    bool associate_iocp(HANDLE iocp_handle, ULONG_PTR key) noexcept;

    static bool would_block() noexcept;

    static bool is_readable(SOCKET sock) noexcept;

    static bool is_writable(SOCKET sock) noexcept;

    bool check_connect_completed() noexcept;

    int set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept override;

    native_handle_t native_handle() const noexcept override;
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_WINSOCK_IMPL_FILE_HPP