#ifndef FIZMO_SOCKET_SELECTOR_HPP
#define FIZMO_SOCKET_SELECTOR_HPP

#include "socket_event.hpp"
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace fizmo {
namespace networking {
namespace core {

class SocketSelector {
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
    fd_set m_read_set;
    fd_set m_write_set;
    fd_set m_error_set;
#endif

public:
    SocketSelector() {
    #ifdef OS_WINDOWS
        FD_ZERO(&m_read_set);
        FD_ZERO(&m_write_set);
        FD_ZERO(&m_error_set);
    #endif
    }

    ~SocketSelector() = default;
    SocketSelector(const SocketSelector&) = delete;
    SocketSelector& operator=(const SocketSelector&) = delete;

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

    int wait(const Duration& timeout = Milliseconds(0)) {
        std::lock_guard<std::mutex> lock(m_selector_mutex);
        m_raw_results.clear();
        m_buffered_results.clear();
        m_buffered_udp_results.clear();
        prepare_sets();
        struct timeval tv;
        struct timeval* tv_ptr = nullptr;

        if (timeout != Duration::max()) {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
            tv.tv_sec  = static_cast<long>(us / 1000000);
            tv.tv_usec = static_cast<long>(us % 1000000);
            tv_ptr = &tv;
        }

        int result = 0;

    #ifdef OS_WINDOWS
        result = select(0, &m_read_set, &m_write_set, &m_error_set, tv_ptr);
    #endif
        if (result > 0) { update_results(); }
        return result;
    }

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

private:
    void prepare_sets() { prepare_states_impl(); }
    void update_results() { update_results_impl(); }

private:
    void prepare_states_impl() {
    #ifdef OS_WINDOWS
        FD_ZERO(&m_read_set);
        FD_ZERO(&m_write_set);
        FD_ZERO(&m_error_set);

        for (const auto& entry : m_raw_sockets) {
            SocketBase* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            BufferedSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();
            
            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if (((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && socket->has_data_to_send()) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            BufferedUDPSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                if ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) { FD_SET(raw, &m_read_set); }
                if (((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && socket->has_data_to_send()) { FD_SET(raw, &m_write_set); }
                FD_SET(raw, &m_error_set);
            }
        }
    #endif
    }

    void update_results_impl() {
    #ifdef OS_WINDOWS
        for (const auto& entry : m_raw_sockets) {
            SocketBase* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->get_raw_socket();

            if (raw != INVALID_SOCKET) {
                SocketState st;
                st.readable  = ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) && FD_ISSET(raw, &m_read_set);
                st.writable  = ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && FD_ISSET(raw, &m_write_set);
                st.has_error = FD_ISSET(raw, &m_error_set);
                if (st.readable || st.writable || st.has_error) { m_raw_results[socket] = st; }
            }
        }

        for (const auto& entry : m_buffered_sockets) {
            BufferedSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                SocketState st;
                st.readable  = ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) && FD_ISSET(raw, &m_read_set);
                st.writable  = ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && FD_ISSET(raw, &m_write_set);
                st.has_error = FD_ISSET(raw, &m_error_set);
                if (st.readable || st.writable || st.has_error) { m_buffered_results[socket] = st; }
            }
        }

        for (const auto& entry : m_buffered_udp_sockets) {
            BufferedUDPSocket* socket = entry.first;
            SocketEvent::Type events = entry.second;
            SOCKET raw = socket->socket().get_raw_socket();

            if (raw != INVALID_SOCKET) {
                SocketState st;
                st.readable  = ((events & SocketEvent::Type::Read)  != SocketEvent::Type::None) && FD_ISSET(raw, &m_read_set);
                st.writable  = ((events & SocketEvent::Type::Write) != SocketEvent::Type::None) && FD_ISSET(raw, &m_write_set);
                st.has_error = FD_ISSET(raw, &m_error_set);
                if (st.readable || st.writable || st.has_error) { m_buffered_udp_results[socket] = st; }
            }
        }
    #endif
    }
};

} // namespace core
} // namespace networking
} // namespace fizmo

#endif // FIZMO_SOCKET_SELECTOR_HPP