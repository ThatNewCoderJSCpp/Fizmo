#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "iocp_service.hpp"

namespace fizmo {
namespace networking {
namespace core {

#if defined(OS_WINDOWS)
IOCPOverlapped::IOCPOverlapped(IOCPOperationType op, SocketBase* sock, std::size_t buf_size) : operation(op), socket(sock), buffer(buf_size) {
    std::memset(static_cast<OVERLAPPED*>(this), 0, sizeof(OVERLAPPED));
    wsa_buf.buf = buffer.data();
    wsa_buf.len = static_cast<ULONG>(buffer.size());
}
#endif

#if defined(OS_WINDOWS)
IOCPSocketContext::IOCPSocketContext(SocketBase* s, SOCKET raw) : socket(s), raw_socket(raw) {}
#endif

#if defined(OS_WINDOWS)
IOCPService::IOCPService() : m_iocp(INVALID_HANDLE_VALUE), m_running(false) {}
#endif

#if defined(OS_WINDOWS)
IOCPService::~IOCPService() { stop(); }
#endif

#if defined(OS_WINDOWS)
bool IOCPService::start(std::size_t thread_count) {
    if (m_running.exchange(true)) return true;

    if (thread_count == 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        thread_count = si.dwNumberOfProcessors * 2;
    }

    m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, static_cast<DWORD>(thread_count));

    if (!m_iocp) {
        m_running = false;
        return false;
    }

    for (std::size_t i = 0; i < thread_count; ++i) { m_workers.emplace_back(&IOCPService::worker_thread, this); }
    return true;
}
#endif

#if defined(OS_WINDOWS)
void IOCPService::stop() {
    if (!m_running.exchange(false)) return;

    for (std::size_t i = 0; i < m_workers.size(); ++i) {
        PostQueuedCompletionStatus(m_iocp, 0, static_cast<ULONG_PTR>(IOCPOperationType::Shutdown), nullptr);
    }

    for (auto& t : m_workers) { if (t.joinable()) t.join(); }
    m_workers.clear();

    if (m_iocp != INVALID_HANDLE_VALUE) {
        CloseHandle(m_iocp);
        m_iocp = INVALID_HANDLE_VALUE;
    }

    std::lock_guard<std::mutex> lock(m_context_mutex);
    m_contexts.clear();
}
#endif

#if defined(OS_WINDOWS)
bool IOCPService::is_running() const noexcept { return m_running; }
#endif

#if defined(OS_WINDOWS)
bool IOCPService::associate(SocketBase* socket) {
    if (!socket || !m_running || m_iocp == INVALID_HANDLE_VALUE) return false;
    SOCKET raw = socket->get_raw_socket();
    if (raw == INVALID_SOCKET) return false;
    auto ctx = std::make_unique<IOCPSocketContext>(socket, raw);
    IOCPSocketContext* ctx_ptr = ctx.get();

    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(raw),
        m_iocp,
        reinterpret_cast<ULONG_PTR>(ctx_ptr),
        0
    );

    if (!result) return false;
    std::lock_guard<std::mutex> lock(m_context_mutex);
    m_contexts.push_back(std::move(ctx));
    return true;
}
#endif

#if defined(OS_WINDOWS)
bool IOCPService::async_receive(SocketBase* socket, std::size_t buffer_size, std::function<void(const char* data, std::size_t len, DWORD error)> callback) {
    if (!socket || !m_running) return false;
    SOCKET raw = socket->get_raw_socket();
    if (raw == INVALID_SOCKET) return false;
    auto* ov = new IOCPOverlapped(IOCPOperationType::Read, socket, buffer_size);

    ov->callback = [callback, ov](DWORD bytes, DWORD err) {
        if (callback) {
            callback(ov->buffer.data(), static_cast<std::size_t>(bytes), err);
        }
    };

    DWORD flags = 0;
    DWORD bytes_received = 0;
    int result = WSARecv(raw, &ov->wsa_buf, 1, &bytes_received, &flags, ov, nullptr);

    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();

        if (err != WSA_IO_PENDING) {
            delete ov;
            return false;
        }
    }

    return true;
}
#endif

#if defined(OS_WINDOWS)
bool IOCPService::async_send(SocketBase* socket, const void* data, std::size_t length, std::function<void(std::size_t bytes_sent, DWORD error)> callback) {
    if (!socket || !m_running || !data || length == 0) return false;
    SOCKET raw = socket->get_raw_socket();
    if (raw == INVALID_SOCKET) return false;
    auto* ov = new IOCPOverlapped(IOCPOperationType::Write, socket, length);
    std::memcpy(ov->buffer.data(), data, length);
    ov->wsa_buf.buf = ov->buffer.data();
    ov->wsa_buf.len = static_cast<ULONG>(length);

    ov->callback = [callback](DWORD bytes, DWORD err) {
        if (callback) { callback(static_cast<std::size_t>(bytes), err); }
    };

    DWORD bytes_sent = 0;
    int result = WSASend(raw, &ov->wsa_buf, 1, &bytes_sent, 0, ov, nullptr);

    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();

        if (err != WSA_IO_PENDING) {
            delete ov;
            return false;
        }
    }

    return true;
}
#endif

#if defined(OS_WINDOWS)
bool IOCPService::async_scatter_receive(
    SocketBase* socket,
    std::vector<detail::IOBuffer>& buffers,
    std::function<void(std::size_t total_bytes, DWORD error)> callback
) {
    if (!socket || !m_running || buffers.empty()) return false;
    SOCKET raw = socket->get_raw_socket();
    if (raw == INVALID_SOCKET) return false;
    auto* ov = new IOCPOverlapped(IOCPOperationType::Read, socket, 0);

    ov->callback = [callback](DWORD bytes, DWORD err) {
        if (callback) { callback(static_cast<std::size_t>(bytes), err); }
    };

    std::vector<WSABUF> wsa_bufs(buffers.size());

    for (std::size_t i = 0; i < buffers.size(); ++i) {
        wsa_bufs[i].buf = static_cast<char*>(buffers[i].data);
        wsa_bufs[i].len = static_cast<ULONG>(buffers[i].length);
    }

    DWORD flags = 0;
    DWORD bytes_received = 0;

    int result = WSARecv(raw, wsa_bufs.data(), static_cast<DWORD>(wsa_bufs.size()), &bytes_received, &flags, ov, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        delete ov;
        return false;
    }

    return true;
}
#endif

#if defined(OS_WINDOWS)
bool IOCPService::post(Task task) {
    if (!m_running || m_iocp == INVALID_HANDLE_VALUE) return false;
    auto* ov = new IOCPOverlapped(IOCPOperationType::Task, nullptr, 0);
    ov->callback = [task = std::move(task)](DWORD, DWORD) { task(); };

    return PostQueuedCompletionStatus(
        m_iocp, 0,
        static_cast<ULONG_PTR>(IOCPOperationType::Task),
        ov
    ) != 0;
}
#endif

#if defined(OS_WINDOWS)
void IOCPService::worker_thread() {
    while (m_running) {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL success = GetQueuedCompletionStatus(
            m_iocp,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            1000  
        );

        if (!overlapped) {
            if (!success) {
                DWORD err = GetLastError();
                if (err == WAIT_TIMEOUT) continue;
                if (!m_running) break;
                continue;
            }
                
            if (static_cast<IOCPOperationType>(completion_key) == IOCPOperationType::Shutdown) { break; }
            continue;
        }

        auto* ov = static_cast<IOCPOverlapped*>(overlapped);

        if (ov->operation == IOCPOperationType::Shutdown) {
            delete ov;
            break;
        }

        DWORD error = 0;
        if (!success) { error = GetLastError(); }

        if (ov->callback) {
            try {
                ov->callback(bytes_transferred, error);
            } catch (...) {
                // Swallow exceptions from callbacks
            }
        }

        delete ov;
    }
}
#endif

} // namespace core
} // namespace networking
} // namespace fizmo
