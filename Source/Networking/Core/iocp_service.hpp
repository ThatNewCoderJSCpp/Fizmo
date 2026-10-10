#ifndef FIZMO_IOCP_SERVICE_HPP
#define FIZMO_IOCP_SERVICE_HPP

#include "../../Basic/fizmo_defines.hpp"

#ifdef OS_WINDOWS

#include "socket_base.hpp"
#include "buffered_socket.hpp"
#include "Error Conversion/winsock_errors.hpp"
#include <thread>
#include <atomic>
#include <functional>
#include <queue>
#include <condition_variable>

namespace fizmo {
namespace networking {
namespace core {

enum class IOCPOperationType : ULONG_PTR {
    Read = 1,
    Write,
    Accept,
    Connect,
    Disconnect,
    Task,        
    Shutdown     
};

struct IOCPOverlapped : OVERLAPPED {
    IOCPOperationType operation;
    WSABUF wsa_buf;
    std::vector<char> buffer;
    std::function<void(DWORD bytes_transferred, DWORD error)> callback;
    SocketBase* socket;

    IOCPOverlapped(IOCPOperationType op, SocketBase* sock, std::size_t buf_size = 8192);

    IOCPOverlapped(const IOCPOverlapped&) = delete;
    IOCPOverlapped& operator=(const IOCPOverlapped&) = delete;
    IOCPOverlapped(IOCPOverlapped&&) = delete;
    IOCPOverlapped& operator=(IOCPOverlapped&&) = delete;
};

struct IOCPSocketContext {
    SocketBase* socket;
    SOCKET raw_socket;
    IOCPSocketContext(SocketBase* s, SOCKET raw);
};

class IOCPService {
public:
    using Task = std::function<void()>;

private:
    HANDLE m_iocp;
    std::atomic<bool> m_running;
    std::vector<std::thread> m_workers;
    std::vector<std::unique_ptr<IOCPSocketContext>> m_contexts;
    mutable std::mutex m_context_mutex;

public:
    IOCPService();
    ~IOCPService();
    IOCPService(const IOCPService&) = delete;
    IOCPService& operator=(const IOCPService&) = delete;

    bool start(std::size_t thread_count = 0);

    void stop();

    bool is_running() const noexcept;

    bool associate(SocketBase* socket);

    bool async_receive(SocketBase* socket, std::size_t buffer_size, std::function<void(const char* data, std::size_t len, DWORD error)> callback);

    bool async_send(SocketBase* socket, const void* data, std::size_t length, std::function<void(std::size_t bytes_sent, DWORD error)> callback);

    bool async_scatter_receive(
        SocketBase* socket,
        std::vector<detail::IOBuffer>& buffers,
        std::function<void(std::size_t total_bytes, DWORD error)> callback
    );

    bool post(Task task);

private:
    void worker_thread();
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // OS_WINDOWS
#endif // FIZMO_IOCP_SERVICE_HPP