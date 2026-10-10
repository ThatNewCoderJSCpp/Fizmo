#ifndef FIZMO_SOCKET_BASE_HPP
#define FIZMO_SOCKET_BASE_HPP

#include "../addresses.hpp"
#include <memory>
#include <mutex>
#include <atomic>

namespace fizmo {
namespace networking {
namespace core {

enum class SocketType {
    TCP = 0,
    UDP
};

enum class ShutdownMode {
    Read  = 0, 
    Write = 1,  
    Both  = 2   
};

namespace detail {

#ifdef OS_WINDOWS
    using native_handle_t = SOCKET;
    using socklen_type    = int;
    inline const native_handle_t kInvalidHandle = INVALID_SOCKET;
#else
    using native_handle_t = int;
    using socklen_type    = socklen_t;
    inline constexpr native_handle_t kInvalidHandle = -1;
#endif

struct IOBuffer {
    void*       data;
    std::size_t length;
    IOBuffer() : data(nullptr), length(0) {}
    IOBuffer(void* d, std::size_t l) : data(d), length(l) {}
};

class SocketImplBase {
protected:
    std::atomic<bool> m_connected;
    SocketType m_type;
    AddressFamily m_family;
    mutable std::mutex m_impl_mutex;
    unsigned int m_connect_timeout_ms;
    unsigned int m_send_timeout_ms;
    unsigned int m_receive_timeout_ms;

    explicit SocketImplBase(SocketType type, AddressFamily family = AddressFamily::IPv4) noexcept
;

    explicit SocketImplBase(SocketType type, AddressFamily family, bool connected) noexcept
;

public:
    virtual ~SocketImplBase() = default;
    SocketImplBase(const SocketImplBase&) = delete;
    SocketImplBase& operator=(const SocketImplBase&) = delete;

    bool is_connected() const noexcept { return m_connected.load(std::memory_order_acquire); }
    SocketType type() const noexcept { return m_type; }
    AddressFamily family() const noexcept { return m_family; }

public: // TCP
    virtual bool connect(const NetworkAddress& address) noexcept = 0;
    virtual bool bind(const NetworkAddress& address) noexcept = 0;
    virtual bool listen(int backlog = 5) noexcept = 0;
    virtual std::unique_ptr<SocketImplBase> accept() noexcept = 0;

    virtual int send(const void* data, std::size_t length) noexcept = 0;
    virtual int receive(void* buffer, std::size_t length) noexcept = 0;

public: // UDP
    virtual int sendto(const void* data, std::size_t length, const NetworkAddress& dest) noexcept = 0;
    virtual int recvfrom(void* buffer, std::size_t length, NetworkAddress& source) noexcept = 0;

public: // Scatter/gather I/O 
    virtual int scatter_receive(IOBuffer* buffers, std::size_t buffer_count) noexcept = 0;
    virtual int gather_send(const IOBuffer* buffers, std::size_t buffer_count) noexcept = 0;
    virtual int scatter_recvfrom(IOBuffer* buffers, std::size_t buffer_count, NetworkAddress& source) noexcept = 0;
    virtual int gather_sendto(const IOBuffer* buffers, std::size_t buffer_count, const NetworkAddress& dest) noexcept = 0;

public: // Common
    virtual void close() noexcept = 0;
    virtual bool shutdown(ShutdownMode mode) noexcept = 0;
    virtual int  set_blocking(bool blocking) noexcept = 0;
    virtual int set_option(int level, int option, const void* value, int  length) noexcept = 0;
    virtual int get_option(int level, int option,       void* value, int* length) noexcept = 0;

    virtual void set_connect_timeout(unsigned int milliseconds) noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_connect_timeout_ms = milliseconds;
    }

    virtual void set_send_timeout(unsigned int milliseconds) noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_send_timeout_ms = milliseconds;
    }

    virtual void set_receive_timeout(unsigned int milliseconds) noexcept {
        std::lock_guard<std::mutex> lock(m_impl_mutex);
        m_receive_timeout_ms = milliseconds;
    }

    virtual int set_keepalive_params(unsigned long idle_ms, unsigned long interval_ms) noexcept {
        (void)idle_ms; 
        (void)interval_ms;
        return -1; 
    }

    virtual native_handle_t native_handle() const noexcept = 0;
};

} // namespace detail
} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_BASE_HPP