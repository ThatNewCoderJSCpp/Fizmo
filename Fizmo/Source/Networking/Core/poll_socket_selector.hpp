#ifndef FIZMO_POLL_SOCKET_SELECTOR_HPP
#define FIZMO_POLL_SOCKET_SELECTOR_HPP

#include "socket_event.hpp"
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace fizmo {
namespace networking {
namespace core {

class PollSocketSelector {
public:
    using Duration     = std::chrono::steady_clock::duration;
    using Milliseconds = std::chrono::milliseconds;

private:
    struct SocketState {
        bool readable  = false;
        bool writable  = false;
        bool has_error = false;
    };

    std::unordered_map<SocketBase*       , SocketEvent::Type> m_raw_sockets;
    std::unordered_map<BufferedSocket*   , SocketEvent::Type> m_buffered_sockets;
    std::unordered_map<BufferedUDPSocket*, SocketEvent::Type> m_buffered_udp_sockets;

    std::unordered_map<SocketBase*       , SocketState> m_raw_results;
    std::unordered_map<BufferedSocket*   , SocketState> m_buffered_results;
    std::unordered_map<BufferedUDPSocket*, SocketState> m_buffered_udp_results;

    mutable std::mutex m_selector_mutex;

#ifdef OS_WINDOWS
    std::vector<WSAPOLLFD>          m_pollfds;
    std::vector<SocketBase*>        m_raw_index;        
    std::vector<BufferedSocket*>    m_buffered_index;
    std::vector<BufferedUDPSocket*> m_buffered_udp_index;
#endif

public:
    PollSocketSelector()  = default;
    ~PollSocketSelector() = default;
    PollSocketSelector(const PollSocketSelector&) = delete;
    PollSocketSelector& operator=(const PollSocketSelector&) = delete;

public:
    void add(SocketBase* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets[socket] = events;
    }

    void add(BufferedSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets[socket] = events;
    }

    void add(BufferedUDPSocket* socket, SocketEvent::Type events = SocketEvent::Type::Read | SocketEvent::Type::Error) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets[socket] = events;
    }

    void remove(SocketBase* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets.erase(socket);
        m_raw_results.erase(socket);
    }

    void remove(BufferedSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_sockets.erase(socket);
        m_buffered_results.erase(socket);
    }

    void remove(BufferedUDPSocket* socket) {
        if (!socket) return;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_buffered_udp_sockets.erase(socket);
        m_buffered_udp_results.erase(socket);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_sockets.clear();
        m_buffered_sockets.clear();
        m_buffered_udp_sockets.clear();
        m_raw_results.clear();
        m_buffered_results.clear();
        m_buffered_udp_results.clear();
    }

public:
    int wait(const Duration& timeout = Milliseconds(0)) {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_results.clear();
        m_buffered_results.clear();
        m_buffered_udp_results.clear();

    #ifdef OS_WINDOWS
        build_pollfd_array();
        if (m_pollfds.empty()) return 0;
        INT timeout_ms;

        if (timeout == Duration::max()) {
            timeout_ms = -1;   // block indefinitely
        } else {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
            timeout_ms = static_cast<INT>(ms);
        }

        int result = WSAPoll(m_pollfds.data(), static_cast<ULONG>(m_pollfds.size()), timeout_ms);

        if (result > 0) { harvest_results(); }
        return result;
    #else
        return 0;
    #endif
    }

public:
    bool is_readable(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.readable);
    }

    bool is_writable(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.writable);
    }

    bool has_error(SocketBase* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_raw_results.find(socket);
        return (it != m_raw_results.end() && it->second.has_error);
    }

    bool is_readable(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.readable);
    }

    bool is_writable(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.writable);
    }

    bool has_error(BufferedSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_results.find(socket);
        return (it != m_buffered_results.end() && it->second.has_error);
    }

    bool is_readable(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        if (socket->has_pending_data()) return true;
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.readable);
    }

    bool is_writable(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.writable);
    }

    bool has_error(BufferedUDPSocket* socket) const {
        if (!socket) return false;
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        auto it = m_buffered_udp_results.find(socket);
        return (it != m_buffered_udp_results.end() && it->second.has_error);
    }

public:
    std::unordered_map<SocketBase*, SocketState> get_raw_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_raw_results;
    }

    std::unordered_map<BufferedSocket*, SocketState> get_buffered_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_buffered_results;
    }

    std::unordered_map<BufferedUDPSocket*, SocketState> get_buffered_udp_results() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_buffered_udp_results;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        return m_raw_sockets.size() + m_buffered_sockets.size() + m_buffered_udp_sockets.size();
    }

private:
#ifdef OS_WINDOWS
    static SHORT to_poll_events(SocketEvent::Type events) noexcept {
        SHORT pe = 0;
        if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) pe |= POLLRDNORM;
        if ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) pe |= POLLWRNORM;
        return pe;
    }

    void build_pollfd_array() {
        std::size_t total = m_raw_sockets.size() + m_buffered_sockets.size() + m_buffered_udp_sockets.size();
        m_pollfds.clear();
        m_pollfds.reserve(total);
        m_raw_index.clear();
        m_raw_index.reserve(m_raw_sockets.size());
        m_buffered_index.clear();
        m_buffered_index.reserve(m_buffered_sockets.size());
        m_buffered_udp_index.clear();
        m_buffered_udp_index.reserve(m_buffered_udp_sockets.size());

        for (const auto& entry : m_raw_sockets) {
            SOCKET raw = entry.first->get_raw_socket();
            if (raw == INVALID_SOCKET) continue;
            WSAPOLLFD pfd;
            pfd.fd      = raw;
            pfd.events  = to_poll_events(entry.second);
            pfd.revents = 0;
            m_pollfds.push_back(pfd);
            m_raw_index.push_back(entry.first);
            m_buffered_index.push_back(nullptr);
            m_buffered_udp_index.push_back(nullptr);
        }

        for (const auto& entry : m_buffered_sockets) {
            SOCKET raw = entry.first->socket().get_raw_socket();
            if (raw == INVALID_SOCKET) continue;
            SHORT pe = 0;
            if ((entry.second & SocketEvent::Type::Read) != SocketEvent::Type::None) pe |= POLLRDNORM;
            if (((entry.second & SocketEvent::Type::Write) != SocketEvent::Type::None) && entry.first->has_data_to_send()) pe |= POLLWRNORM;
            WSAPOLLFD pfd;
            pfd.fd      = raw;
            pfd.events  = pe;
            pfd.revents = 0;
            m_pollfds.push_back(pfd);
            m_raw_index.push_back(nullptr);
            m_buffered_index.push_back(entry.first);
            m_buffered_udp_index.push_back(nullptr);
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            SOCKET raw = entry.first->socket().get_raw_socket();
            if (raw == INVALID_SOCKET) continue;
            SHORT pe = 0;
            if ((entry.second & SocketEvent::Type::Read) != SocketEvent::Type::None) pe |= POLLRDNORM;
            if (((entry.second & SocketEvent::Type::Write) != SocketEvent::Type::None) && entry.first->has_data_to_send()) pe |= POLLWRNORM;
            WSAPOLLFD pfd;
            pfd.fd      = raw;
            pfd.events  = pe;
            pfd.revents = 0;
            m_pollfds.push_back(pfd);
            m_raw_index.push_back(nullptr);
            m_buffered_index.push_back(nullptr);
            m_buffered_udp_index.push_back(entry.first);
        }
    }

    void harvest_results() {
        for (std::size_t i = 0; i < m_pollfds.size(); ++i) {
            SHORT rev = m_pollfds[i].revents;
            if (rev == 0) continue;
            SocketState st;
            st.readable  = (rev & (POLLRDNORM | POLLHUP)) != 0;
            st.writable  = (rev & POLLWRNORM) != 0;
            st.has_error = (rev & (POLLERR | POLLNVAL)) != 0;

            if (m_raw_index[i]) {
                m_raw_results[m_raw_index[i]] = st;
            } else if (m_buffered_index[i]) {
                m_buffered_results[m_buffered_index[i]] = st;
            } else if (m_buffered_udp_index[i]) {
                m_buffered_udp_results[m_buffered_udp_index[i]] = st;
            }
        }
    }

#endif // OS_WINDOWS
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_POLL_SOCKET_SELECTOR_HPP